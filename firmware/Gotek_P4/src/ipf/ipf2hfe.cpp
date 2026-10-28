/*
 * ipf2hfe.cpp - IPF -> HFE v3 converter for GTi / FlashFloppy.
 * See ipf2hfe.h. Personal, non-commercial use (links SPS capsimg).
 *
 * HFE v3 stream rules followed here (as implemented by FlashFloppy
 * src/image/hfe.c):
 *  - track data is stored LSb-first; opcodes are bytes with low nibble
 *    0xF in file order: NOP 0x0F, INDEX 0x8F (ignored by FF),
 *    BITRATE 0x4F + rbit8(x), SKIPBITS 0xCF + rbit8(n), RAND 0x2F.
 *  - BITRATE x sets cell time to x/36 us (72 = 2us). NOP and BITRATE
 *    take no time. RAND = 8 random cells.
 *  - FF puts the index at stream byte 0 of each side, so tracks are
 *    rendered index-aligned (CAPS without DI_LOCK_INDEX does this).
 *  - FF can resume reading mid-track with a stale bitrate, so the
 *    current bitrate is re-asserted every REFRESH bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#ifndef __cdecl
#define __cdecl
#endif
#include "capsimg/CapsLibAll.h"
#include "ipf2hfe.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
static void *big_alloc(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
}
#else
static void *big_alloc(size_t n) { return malloc(n); }
#endif

/* ---- HFE v3 constants (file byte order) ---- */
#define OP_NOP      0x0F
#define OP_INDEX    0x8F
#define OP_BITRATE  0x4F
#define OP_SKIPBITS 0xCF
#define OP_RAND     0x2F

#define X_NOMINAL   72          /* 2us cell */
#define REFRESH     64          /* re-assert bitrate every N bytes */
#define DITHER_T    64.0        /* dither threshold, in x*cell units */
#define MAX_SIDE    32767       /* HFE track_len is uint16 for both sides */
#define NOISE_BYTES 12500       /* unformatted track: 100000 cells */
#define REV_US      200000.0    /* 300 rpm */

static uint8_t rbit8(uint8_t b)
{
    b = (uint8_t)((b & 0xF0) >> 4 | (b & 0x0F) << 4);
    b = (uint8_t)((b & 0xCC) >> 2 | (b & 0x33) << 2);
    b = (uint8_t)((b & 0xAA) >> 1 | (b & 0x55) << 1);
    return b;
}

/* ---- per-side stream builder ---- */
struct stream {
    uint8_t *buf;
    uint32_t cap, len;
    uint32_t next_refresh;
    int cur_x;
    int overflow;
};

static void s_put(struct stream *s, uint8_t b)
{
    if (s->len >= s->cap) { s->overflow = 1; return; }
    s->buf[s->len++] = b;
}

static void s_bitrate(struct stream *s, int x)
{
    s_put(s, OP_BITRATE);
    s_put(s, rbit8((uint8_t)x));
    s->cur_x = x;
}

/* Called before each atomic unit (data byte, RAND, SKIPBITS triple). */
static void s_unit(struct stream *s)
{
    if (s->len >= s->next_refresh) {
        s_bitrate(s, s->cur_x);
        s->next_refresh = (s->len / REFRESH + 1) * REFRESH;
    }
}

static void s_init(struct stream *s, uint8_t *buf, uint32_t cap, int x)
{
    s->buf = buf; s->cap = cap; s->len = 0; s->overflow = 0;
    s->cur_x = x;
    s->next_refresh = 0;   /* first unit emits BITRATE at byte 0 */
}

/* Emit the first n (1..7) cells of msb (MSb-first) as SKIPBITS + byte.
 * FF skips the (8-n) leading cells of the following byte, so the data
 * sits in the last n transmitted bits. Skipped bits are zero, so the
 * byte's low nibble can never look like an opcode. */
static void s_partial(struct stream *s, uint8_t msb, uint32_t n, int weak)
{
    uint8_t skip = (uint8_t)(8 - n);
    s_put(s, OP_SKIPBITS);
    s_put(s, rbit8(skip));
    if (weak) {
        s_put(s, OP_RAND);
    } else {
        uint8_t keep = (uint8_t)(msb & (0xFF << skip));
        s_put(s, (uint8_t)(rbit8(keep) << skip));
    }
}

/* Weak-bit mask helpers: one bit per track cell. */
static int mask_any(const uint8_t *m, uint32_t from, uint32_t n)
{
    for (uint32_t i = from; i < from + n; i++)
        if (m[i >> 3] & (0x80 >> (i & 7)))
            return 1;
    return 0;
}

static void mask_set(uint8_t *m, uint32_t nbits, uint32_t from, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t b = (from + i) % nbits;
        m[b >> 3] |= (uint8_t)(0x80 >> (b & 7));
    }
}

/* Build one side's HFE v3 stream from a CAPS track.
 * bits: MSb-first cell buffer, nbits cells, index at bit 0.
 * timebuf: optional per-byte density (1000 = nominal cell), timelen entries.
 * weak: optional cell mask. Returns 0 or -1 on overflow. */
static int build_side(struct stream *s, const uint8_t *bits, uint32_t nbits,
                      const UDWORD *timebuf, uint32_t timelen,
                      const uint8_t *weak, double dither_t, int *collisions)
{
    uint32_t nfull = nbits / 8, rem = nbits % 8, nbytes = nfull + (rem ? 1 : 0);

    /* Total time in "nominal cells", to normalise to one revolution. */
    double tot = 0;
    for (uint32_t k = 0; k < nbytes; k++) {
        double t = (timebuf && k < timelen && timebuf[k]) ? timebuf[k] : 1000.0;
        tot += t * ((k < nfull) ? 8 : rem) / 1000.0;
    }
    double base_x = 36.0 * (REV_US / tot);   /* x for a t=1000 cell */

    int first = 1;
    double err = 0;   /* accumulated (desired - emitted) time, x*cells */
    for (uint32_t k = 0; k < nbytes; k++) {
        double t = (timebuf && k < timelen && timebuf[k]) ? timebuf[k] : 1000.0;
        double xf = base_x * t / 1000.0;
        uint32_t cells = (k < nfull) ? 8 : rem;
        int x = s->cur_x;

        if (first || fabs(xf - x) >= 1.0) {
            x = (int)lround(xf);
            first = 0;
        } else if (err > dither_t) {
            x = (int)floor(xf) + 1;
        } else if (err < -dither_t) {
            x = (int)ceil(xf) - 1;
        }
        if (x < 8) x = 8;
        if (x > 255) x = 255;

        s_unit(s);
        if (x != s->cur_x)
            s_bitrate(s, x);
        err += (xf - x) * cells;

        int is_weak = weak && mask_any(weak, k * 8, cells);
        uint8_t raw = bits[k];

        if (cells == 8) {
            if (is_weak) {
                s_put(s, OP_RAND);
            } else {
                uint8_t b = rbit8(raw);
                if ((b & 0x0F) == 0x0F) {
                    /* First four cells are 1111: the byte would read as an
                     * opcode. Emit it exactly as two 4-cell SKIPBITS units
                     * instead (zero-time opcodes, cells unchanged). */
                    s_partial(s, raw, 4, 0);
                    s_partial(s, (uint8_t)(raw << 4), 4, 0);
                    (*collisions)++;
                } else {
                    s_put(s, b);
                }
            }
        } else {
            s_partial(s, raw, cells, is_weak);
        }
    }
    return s->overflow ? -1 : 0;
}

static void build_noise(struct stream *s)
{
    for (int i = 0; i < NOISE_BYTES; i++) {
        s_unit(s);
        s_put(s, OP_RAND);
    }
}

/* ---- HFE file layout ---- */
struct hfe_hdr {
    char sig[8];
    uint8_t formatrevision, nr_tracks, nr_sides, track_encoding;
    uint8_t bitrate_lo, bitrate_hi, rpm_lo, rpm_hi;
    uint8_t interface_mode, rsvd;
    uint8_t tlut_lo, tlut_hi;
    uint8_t write_allowed, single_step;
    uint8_t t0s0_alt, t0s0_enc, t0s1_alt, t0s1_enc;
};

static int put_le16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; return 0; }

extern "C" const char *ipf2hfe_strerror(int err)
{
    switch (err) {
    case IPF2HFE_OK: return "ok";
    case IPF2HFE_E_PARAM: return "bad parameter";
    case IPF2HFE_E_NOMEM: return "out of memory";
    case IPF2HFE_E_NOTIPF: return "not a valid IPF (CAPS rejected it)";
    case IPF2HFE_E_WRITE: return "write error";
    case IPF2HFE_E_TOOLONG: return "track too long for HFE";
    case IPF2HFE_E_OPEN: return "cannot open file";
    case IPF2HFE_E_READ: return "read error";
    }
    return "unknown error";
}

static int caps_inited;

/* Decode one track/side into stream s. Returns 0, or IPF2HFE_E_TOOLONG. */
static int convert_track(SDWORD img, int cyl, int head, int have,
                         struct stream *s, uint8_t *maskbuf, uint32_t maskcap,
                         ipf2hfe_stats *st)
{
    const UDWORD flags = DI_LOCK_DENVAR | DI_LOCK_UPDATEFD | DI_LOCK_TYPE |
                         DI_LOCK_TRKBIT | DI_LOCK_OVLBIT;
    struct CapsTrackInfoT2 ti;
    uint32_t cap = s->cap;
    uint8_t *buf = s->buf;

    memset(&ti, 0, sizeof(ti));
    ti.type = 2;
    if (!have || CAPSLockTrack(&ti, img, cyl, head, flags) != imgeOk
        || (ti.type & CTIT_MASK_TYPE) == ctitNoise || !ti.trackbuf
        || ti.tracklen < 64) {
        if (have) CAPSUnlockTrack(img, cyl, head);
        s_init(s, buf, cap, X_NOMINAL);
        build_noise(s);
        st->noise_tracks++;
        return 0;
    }

    uint32_t nbits = ti.tracklen;          /* bits, thanks to TRKBIT */
    const uint8_t *weak = NULL;
    if (ti.weakcnt && ((nbits + 7) / 8) <= maskcap) {
        memset(maskbuf, 0, (nbits + 7) / 8);
        for (UDWORD i = 0; i < ti.weakcnt; i++) {
            struct CapsDataInfo di;
            if (CAPSGetInfo(&di, img, cyl, head, cgiitWeak, i) == imgeOk
                && di.type == cditWeak && di.size)
                mask_set(maskbuf, nbits, di.start % nbits, di.size);
        }
        weak = maskbuf;
        st->weak_tracks++;
    }
    if (ti.timebuf && ti.timelen)
        st->var_tracks++;

    int res = -1;
    double t = DITHER_T;
    for (int attempt = 0; attempt < 4 && res; attempt++, t *= 4) {
        s_init(s, buf, cap, X_NOMINAL);
        int coll = 0;
        res = build_side(s, ti.trackbuf, nbits, ti.timebuf, ti.timelen,
                         weak, t, &coll);
        if (!res) st->collisions += coll;
    }
    CAPSUnlockTrack(img, cyl, head);
    return res ? IPF2HFE_E_TOOLONG : 0;
}

extern "C" int ipf2hfe_convert_mem(uint8_t *ipf, uint32_t ipf_len,
                                   ipf2hfe_write_fn wr, void *wr_ctx,
                                   ipf2hfe_progress_fn prog, void *prog_ctx,
                                   ipf2hfe_stats *stats)
{
    ipf2hfe_stats st;
    memset(&st, 0, sizeof(st));
    if (!ipf || ipf_len < 12 || !wr)
        return IPF2HFE_E_PARAM;
    if (memcmp(ipf, "CAPS", 4))
        return IPF2HFE_E_NOTIPF;

    if (!caps_inited) {
        CAPSInit();
        caps_inited = 1;
    }

    int rc = IPF2HFE_OK;
    SDWORD img = CAPSAddImage();
    if (img < 0)
        return IPF2HFE_E_NOMEM;
    if (CAPSLockImageMemory(img, ipf, ipf_len, DI_LOCK_MEMREF) != imgeOk) {
        CAPSRemImage(img);
        return IPF2HFE_E_NOTIPF;
    }

    struct CapsImageInfo ii;
    memset(&ii, 0, sizeof(ii));
    CAPSGetImageInfo(&ii, img);
    if (ii.type != ciitFDD || ii.maxcylinder > 99 || ii.maxhead > 1) {
        CAPSUnlockImage(img);
        CAPSRemImage(img);
        return IPF2HFE_E_NOTIPF;
    }

    int ncyl = (int)ii.maxcylinder + 1;
    int nsides = (int)ii.maxhead + 1;
    st.cyls = ncyl;
    st.sides = nsides;
    st.platform = (int)ii.platform[0];

    uint8_t *sb0 = (uint8_t *)big_alloc(MAX_SIDE);
    uint8_t *sb1 = (uint8_t *)big_alloc(MAX_SIDE);
    uint8_t *mask = (uint8_t *)big_alloc(MAX_SIDE);
    uint8_t *blk = (uint8_t *)big_alloc(((MAX_SIDE + 255) / 256) * 512);
    if (!sb0 || !sb1 || !mask || !blk) {
        rc = IPF2HFE_E_NOMEM;
        goto out;
    }

    {
        uint8_t lut[1024];
        memset(lut, 0xFF, sizeof(lut));
        uint32_t lut_blocks = (uint32_t)(ncyl * 4 + 511) / 512;
        uint32_t next_block = 1 + lut_blocks;

        /* Placeholder header + track table so every later write is an
         * append or an in-place overwrite (friendlier to FAT/SD writers). */
        memset(blk, 0, next_block * 512);
        if (wr(wr_ctx, 0, blk, next_block * 512)) {
            rc = IPF2HFE_E_WRITE;
            goto out;
        }

        for (int c = 0; c < ncyl && rc == IPF2HFE_OK; c++) {
            struct stream s0, s1;
            s0.buf = sb0; s0.cap = MAX_SIDE;
            s1.buf = sb1; s1.cap = MAX_SIDE;
            int have = c >= (int)ii.mincylinder;
            int r0 = convert_track(img, c, 0, have && 0 >= (int)ii.minhead,
                                   &s0, mask, MAX_SIDE, &st);
            int r1 = convert_track(img, c, 1, have && nsides > 1,
                                   &s1, mask, MAX_SIDE, &st);
            if (r0 || r1) { rc = IPF2HFE_E_TOOLONG; break; }

            /* Equalise side lengths with zero-time NOPs. */
            uint32_t L = s0.len > s1.len ? s0.len : s1.len;
            while (s0.len < L) s0.buf[s0.len++] = OP_NOP;
            while (s1.len < L) s1.buf[s1.len++] = OP_NOP;
            if ((int)L > st.max_side_bytes) st.max_side_bytes = (int)L;

            /* Interleave: 512-byte blocks, 256 side0 then 256 side1. */
            uint32_t nblk = (L + 255) / 256;
            memset(blk, 0, nblk * 512);
            for (uint32_t i = 0; i < L; i++) {
                blk[(i / 256) * 512 + (i % 256)] = s0.buf[i];
                blk[(i / 256) * 512 + 256 + (i % 256)] = s1.buf[i];
            }
            if (wr(wr_ctx, next_block * 512, blk, nblk * 512)) {
                rc = IPF2HFE_E_WRITE;
                break;
            }
            put_le16(&lut[c * 4], (uint16_t)next_block);
            put_le16(&lut[c * 4 + 2], (uint16_t)(L * 2));
            next_block += nblk;
            st.hfe_bytes = next_block * 512;

            if (prog) prog(prog_ctx, c + 1, ncyl);
        }

        if (rc == IPF2HFE_OK) {
            uint8_t hdr[512];
            memset(hdr, 0xFF, sizeof(hdr));
            struct hfe_hdr *h = (struct hfe_hdr *)hdr;
            memcpy(h->sig, "HXCHFEV3", 8);
            h->formatrevision = 0;
            h->nr_tracks = (uint8_t)ncyl;
            h->nr_sides = (uint8_t)nsides;
            int amiga = st.platform == ciipAmiga;
            h->track_encoding = amiga ? 1 : 0;          /* AMIGA/ISOIBM MFM */
            h->bitrate_lo = 250; h->bitrate_hi = 0;
            h->rpm_lo = 300 & 0xFF; h->rpm_hi = 300 >> 8;
            h->interface_mode = amiga ? 0x04
                              : st.platform == ciipAtariST ? 0x02 : 0x07;
            h->rsvd = 0x01;
            h->tlut_lo = 1; h->tlut_hi = 0;
            h->write_allowed = 0xFF;
            h->single_step = 0xFF;
            if (wr(wr_ctx, 512, lut, lut_blocks * 512) ||
                wr(wr_ctx, 0, hdr, 512))
                rc = IPF2HFE_E_WRITE;
        }
    }

out:
    free(sb0); free(sb1); free(mask); free(blk);
    CAPSUnlockAllTracks(img);
    CAPSUnlockImage(img);
    CAPSRemImage(img);
    if (stats) *stats = st;
    return rc;
}

/* ---- stdio wrapper ---- */
static int file_write(void *ctx, uint32_t off, const void *buf, uint32_t len)
{
    FILE *f = (FILE *)ctx;
    if (fseek(f, (long)off, SEEK_SET)) return -1;
    return fwrite(buf, 1, len, f) == len ? 0 : -1;
}

extern "C" int ipf2hfe_convert_file(const char *ipf_path, const char *hfe_path,
                                    ipf2hfe_progress_fn prog, void *prog_ctx,
                                    ipf2hfe_stats *stats)
{
    if (!ipf_path || !hfe_path) return IPF2HFE_E_PARAM;
    FILE *in = fopen(ipf_path, "rb");
    if (!in) return IPF2HFE_E_OPEN;
    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (sz <= 0 || sz > 16L * 1024 * 1024) { fclose(in); return IPF2HFE_E_READ; }
    uint8_t *ipf = (uint8_t *)big_alloc((size_t)sz);
    if (!ipf) { fclose(in); return IPF2HFE_E_NOMEM; }
    size_t got = fread(ipf, 1, (size_t)sz, in);
    fclose(in);
    if (got != (size_t)sz) { free(ipf); return IPF2HFE_E_READ; }

    size_t pl = strlen(hfe_path);
    char *tmp = (char *)malloc(pl + 5);
    if (!tmp) { free(ipf); return IPF2HFE_E_NOMEM; }
    memcpy(tmp, hfe_path, pl);
    memcpy(tmp + pl, ".tmp", 5);

    FILE *out = fopen(tmp, "wb");
    if (!out) { free(ipf); free(tmp); return IPF2HFE_E_OPEN; }
    int rc = ipf2hfe_convert_mem(ipf, (uint32_t)sz, file_write, out,
                                 prog, prog_ctx, stats);
    if (fclose(out)) rc = rc ? rc : IPF2HFE_E_WRITE;
    free(ipf);
    if (rc == IPF2HFE_OK) {
        remove(hfe_path);
        if (rename(tmp, hfe_path)) rc = IPF2HFE_E_WRITE;
    } else {
        remove(tmp);
    }
    free(tmp);
    return rc;
}
