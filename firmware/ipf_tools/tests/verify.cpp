/* verify.cpp - check an HFE v3 produced by ipf2hfe against the CAPS decode.
 *
 * Plays each side's stream through a model of FlashFloppy's
 * hfe_rdata_flux() (opcode handling, LSb-first bits, bitrate, skip, rand)
 * and checks:
 *   1. every non-weak cell equals the CAPS track buffer cell
 *   2. one revolution takes ~200ms
 *   3. the running timing error vs. the CAPS density map stays small
 *   4. AmigaDOS sectors decode with valid checksums (when present)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#include "capsimg/CapsLibAll.h"

static uint8_t rbit8(uint8_t b)
{
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= 0x80 >> i;
    return r;
}

struct cell { uint8_t bit; uint8_t x; uint8_t rnd; };

/* FlashFloppy hfe_rdata_flux model, one revolution. */
static void ff_play(const uint8_t *s, uint32_t len, std::vector<cell> &out,
                    int *x_state)
{
    uint32_t bc = 0, total = len * 8;
    int x = *x_state;
    while (bc < total) {
        uint32_t bit_off = bc % 8;
        uint8_t b = s[bc / 8];
        bc += 8 - bit_off;
        int rnd = 0;
        if ((b & 0xF) == 0xF) {
            switch (b) {
            case 0x4F:  /* bitrate */
                x = rbit8(s[bc / 8]);
                bc += 8;
                continue;
            case 0xCF: { /* skip bits */
                int n = rbit8(s[bc / 8]) & 7;
                bc += 8 + n;
                continue;
            }
            case 0x2F:  /* rand */
                b = (uint8_t)rand();
                rnd = 1;
                break;
            default:    /* nop, index, unknown */
                continue;
            }
        }
        b >>= bit_off;
        for (uint32_t i = bit_off; i < 8; i++) {
            out.push_back({(uint8_t)(b & 1), (uint8_t)x, (uint8_t)rnd});
            b >>= 1;
        }
    }
    *x_state = x;
}

static int getbit(const std::vector<cell> &c, size_t i) { return c[i % c.size()].bit; }

static uint32_t rawlong(const std::vector<cell> &c, size_t p)
{
    uint32_t v = 0;
    for (int i = 0; i < 32; i++) v = (v << 1) | getbit(c, p + i);
    return v;
}

static int amiga_sectors(const std::vector<cell> &c, int *bad)
{
    int ok = 0;
    *bad = 0;
    size_t n = c.size();
    uint32_t w = 0;
    for (size_t i = 0; i < n + 31; i++) {
        w = (w << 1) | getbit(c, i);
        if (w != 0x44894489 || i >= n + 31) continue;
        size_t p = i + 1;
        uint32_t r[270];
        for (int k = 0; k < 270; k++) r[k] = rawlong(c, p + 32 * k);
        uint32_t hx = 0, dx = 0;
        for (int k = 0; k < 10; k++) hx ^= r[k];
        for (int k = 14; k < 270; k++) dx ^= r[k];
        hx &= 0x55555555; dx &= 0x55555555;
        uint32_t hck = ((r[10] & 0x55555555) << 1) | (r[11] & 0x55555555);
        uint32_t dck = ((r[12] & 0x55555555) << 1) | (r[13] & 0x55555555);
        uint32_t info = ((r[0] & 0x55555555) << 1) | (r[1] & 0x55555555);
        if ((info >> 24) != 0xFF) continue;
        if (hx == hck && dx == dck) ok++; else (*bad)++;
        i += 32 * 270;
        w = 0;
    }
    return ok;
}


static uint8_t mfm_byte(const std::vector<cell> &c, size_t p)
{
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) v = (uint8_t)(v << 1 | getbit(c, p + 2 * i + 1));
    return v;
}

static uint16_t crc16(uint16_t crc, const uint8_t *d, size_t n)
{
    while (n--) {
        crc ^= (uint16_t)(*d++ << 8);
        for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

/* IBM/Atari ST sectors: IDAM + DAM, both CRCs checked. */
static int ibm_sectors(const std::vector<cell> &c, int *bad)
{
    int ok = 0; *bad = 0;
    size_t n = c.size();
    uint64_t w = 0;
    for (size_t i = 0; i < n + 47; i++) {
        w = (w << 1 | getbit(c, i)) & 0xFFFFFFFFFFFFull;
        if (w != 0x448944894489ull || i >= n + 47) continue;
        size_t p = i + 1;
        uint8_t id[10] = {0xA1, 0xA1, 0xA1};
        for (int k = 0; k < 7; k++) id[3 + k] = mfm_byte(c, p + 16 * k);
        if (id[3] != 0xFE || crc16(0xFFFF, id, 10)) continue;
        int size = 128 << (id[7] & 3);
        /* find DAM sync within 60 bytes */
        size_t q = p + 16 * 7, end = q + 16 * 60;
        uint64_t w2 = 0; int found = 0;
        for (; q < end; q++) {
            w2 = (w2 << 1 | getbit(c, q)) & 0xFFFFFFFFFFFFull;
            if (w2 == 0x448944894489ull) { found = 1; q++; break; }
        }
        if (!found) { (*bad)++; continue; }
        std::vector<uint8_t> d(4 + size + 2);
        d[0] = d[1] = d[2] = 0xA1;
        for (int k = 0; k < size + 3; k++) d[3 + k] = mfm_byte(c, q + 16 * k);
        if ((d[3] == 0xFB || d[3] == 0xF8) && !crc16(0xFFFF, d.data(), d.size())) ok++;
        else (*bad)++;
    }
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: verify in.ipf out.hfe [-q]\n"); return 2; }
    int quiet = argc > 3 && !strcmp(argv[3], "-q");
    int px = argc > 5 && !strcmp(argv[3], "-x") ? atoi(argv[4]) * 2 + atoi(argv[5]) : -1;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long isz = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> ipf(isz); fread(ipf.data(), 1, isz, f); fclose(f);
    f = fopen(argv[2], "rb");
    fseek(f, 0, SEEK_END); long hsz = ftell(f); fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> hfe(hsz); fread(hfe.data(), 1, hsz, f); fclose(f);

    if (memcmp(hfe.data(), "HXCHFEV3", 8)) { printf("FAIL: bad signature\n"); return 1; }
    int ncyl = hfe[9], nsides = hfe[10];
    int tlut = hfe[0x12] | hfe[0x13] << 8;

    CAPSInit();
    SDWORD img = CAPSAddImage();
    if (CAPSLockImageMemory(img, ipf.data(), isz, 0) != imgeOk) { printf("FAIL: caps\n"); return 1; }
    const UDWORD flags = DI_LOCK_DENVAR | DI_LOCK_UPDATEFD | DI_LOCK_TYPE |
                         DI_LOCK_TRKBIT | DI_LOCK_OVLBIT;

    int failures = 0, x_state = 72;
    long tot_sec_ok = 0, tot_sec_bad = 0, tot_ibm_ok = 0, tot_ibm_bad = 0;
    double worst_rev = 0, worst_drift = 0;
    for (int c = 0; c < ncyl; c++) {
        const uint8_t *e = &hfe[tlut * 512 + c * 4];
        uint32_t off = (e[0] | e[1] << 8) * 512, len = (e[2] | e[3] << 8) / 2;
        for (int h = 0; h < nsides; h++) {
            std::vector<uint8_t> side(len);
            for (uint32_t i = 0; i < len; i++)
                side[i] = hfe[off + (i / 256) * 512 + h * 256 + (i % 256)];
            std::vector<cell> cells;
            ff_play(side.data(), len, cells, &x_state);
            double rev = 0;
            for (auto &k : cells) rev += k.x / 36.0;

            struct CapsTrackInfoT2 ti;
            memset(&ti, 0, sizeof(ti)); ti.type = 2;
            int locked = CAPSLockTrack(&ti, img, c, h, flags) == imgeOk;
            int noise = !locked || !ti.trackbuf || (ti.type & CTIT_MASK_TYPE) == ctitNoise;
            char msg[256] = "";
            int bad = 0;
            if (noise) {
                size_t nr = 0;
                for (auto &k : cells) nr += k.rnd;
                if (nr != cells.size()) { bad = 1; snprintf(msg, sizeof msg, "noise track not all RAND"); }
                else snprintf(msg, sizeof msg, "noise, %zu RAND cells", nr);
            } else {
                uint32_t nbits = ti.tracklen;
                std::vector<uint8_t> weak(nbits, 0);
                for (UDWORD i = 0; i < ti.weakcnt; i++) {
                    struct CapsDataInfo di;
                    if (CAPSGetInfo(&di, img, c, h, cgiitWeak, i) == imgeOk)
                        for (UDWORD j = 0; j < di.size; j++) weak[(di.start + j) % nbits] = 1;
                }
                if (cells.size() != nbits) {
                    bad = 1;
                    snprintf(msg, sizeof msg, "cell count %zu != %u", cells.size(), nbits);
                } else {
                    uint32_t mism = 0, rnd = 0, weakc = 0;
                    for (uint32_t i = 0; i < nbits; i++) {
                        int cb = (ti.trackbuf[i >> 3] >> (7 - (i & 7))) & 1;
                        weakc += weak[i];
                        if (cells[i].rnd) { rnd++; if (!weak[i] && !weak[(i & ~7u)] && !weak[(i | 7) < nbits ? (i | 7) : i]) {
                                /* RAND byte must contain a weak cell */
                                int any = 0; for (uint32_t j = i & ~7u; j < (i & ~7u) + 8 && j < nbits; j++) any |= weak[j];
                                if (!any) mism++; }
                            continue; }
                        if (cells[i].bit != cb) mism++;
                    }
                    /* timing vs density map */
                    double tot = 0;
                    for (uint32_t i = 0; i < nbits; i++) {
                        uint32_t k = i / 8;
                        tot += (ti.timebuf && k < ti.timelen) ? ti.timebuf[k] / 1000.0 : 1.0;
                    }
                    double scale = 200000.0 / tot, want = 0, got = 0, drift = 0;
                    for (uint32_t i = 0; i < nbits; i++) {
                        uint32_t k = i / 8;
                        want += scale * ((ti.timebuf && k < ti.timelen) ? ti.timebuf[k] / 1000.0 : 1.0);
                        got += cells[i].x / 36.0;
                        if (fabs(got - want) > drift) drift = fabs(got - want);
                    }
                    if (drift > worst_drift) worst_drift = drift;
                    if (px == c * 2 + h) {
                        /* density profile: runs of equal CAPS timing vs mean emitted cell time */
                        uint32_t i = 0;
                        while (i < nbits) {
                            uint32_t k = i / 8;
                            uint32_t tv = (ti.timebuf && k < ti.timelen) ? ti.timebuf[k] : 1000;
                            uint32_t j = i; double sx = 0;
                            while (j < nbits && ((ti.timebuf && j / 8 < ti.timelen) ? ti.timebuf[j / 8] : 1000) == tv) { sx += cells[j].x / 36.0; j++; }
                            printf("   cells %6u-%6u  CAPS density %4u  -> HFE mean cell %.3fus (%.1f%% of 2us)\n",
                                   i, j - 1, tv, sx / (j - i), 100.0 * sx / (j - i) / 2.0);
                            i = j;
                        }
                    }
                    int sb = 0, so = amiga_sectors(cells, &sb);
                    int ib = 0, io = ibm_sectors(cells, &ib);
                    tot_ibm_ok += io; tot_ibm_bad += ib;
                    tot_sec_ok += so; tot_sec_bad += sb;
                    if (mism || drift > 4.0) bad = 1;
                    snprintf(msg, sizeof msg,
                             "%u cells, %s%s weak=%u rand=%u mism=%u drift=%.2fus amiga=%d/%d ibm=%d/%d (ok/bad)",
                             nbits, ti.timebuf ? "vardens " : "",
                             ti.weakcnt ? "fuzzy" : "", weakc, rnd, mism, drift, so, sb, io, ib);
                }
            }
            if (fabs(rev - 200000) > worst_rev) worst_rev = fabs(rev - 200000);
            if (fabs(rev - 200000) > 600) bad = 1;
            if (locked) CAPSUnlockTrack(img, c, h);
            if (bad) failures++;
            if (!quiet || bad)
                printf("%s c%02d h%d len=%5u rev=%.1fms  %s\n", bad ? "FAIL" : " ok ",
                       c, h, len, rev / 1000.0, msg);
        }
    }
    printf("summary: %d cyl x %d sides, failures=%d, worst rev err=%.0fus, "
           "worst drift=%.2fus, amiga sectors ok=%ld bad=%ld, ibm sectors ok=%ld bad=%ld\n",
           ncyl, nsides, failures, worst_rev, worst_drift, tot_sec_ok, tot_sec_bad, tot_ibm_ok, tot_ibm_bad);
    return failures ? 1 : 0;
}
