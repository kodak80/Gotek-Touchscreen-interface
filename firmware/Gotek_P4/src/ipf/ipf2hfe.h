/*
 * ipf2hfe.h - IPF (SPS/CAPS) -> HFE v3 converter for GTi / FlashFloppy.
 *
 * Decodes IPF tracks with the SPS CAPS decoder (capsimg, non-commercial
 * licence) and writes an HFE v3 file that stock FlashFloppy plays:
 *   - index-aligned tracks (data-over-index preserved)
 *   - variable cell density (Copylock, Speedlock, long/short tracks)
 *     via HFEv3 BITRATE opcodes with error-diffused dithering
 *   - weak/fuzzy bits via HFEv3 RAND opcodes
 *   - non-byte-multiple track lengths via SKIPBITS
 *   - unformatted (noise) tracks as RAND
 *
 * For personal, non-commercial use (capsimg licence).
 */
#ifndef IPF2HFE_H
#define IPF2HFE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Random-access writer: write len bytes at file offset off.
 * Return 0 on success. Offsets are written mostly sequentially; the
 * header and track table are written last (offset 0). */
typedef int (*ipf2hfe_write_fn)(void *ctx, uint32_t off, const void *buf,
                                uint32_t len);

/* Optional progress callback: done/total tracks. */
typedef void (*ipf2hfe_progress_fn)(void *ctx, int done, int total);

typedef struct {
    int cyls;              /* cylinders written */
    int sides;             /* sides written */
    int var_tracks;        /* tracks with a CAPS density map */
    int weak_tracks;       /* tracks with weak/fuzzy areas */
    int noise_tracks;      /* unformatted / missing tracks */
    int collisions;        /* bytes re-encoded via SKIPBITS (1111 lead) */
    int max_side_bytes;    /* longest per-side HFE stream */
    uint32_t hfe_bytes;    /* total output file size */
    int platform;          /* first CAPS platform id (1=Amiga, 2=Atari ST) */
} ipf2hfe_stats;

enum {
    IPF2HFE_OK = 0,
    IPF2HFE_E_PARAM = -1,
    IPF2HFE_E_NOMEM = -2,
    IPF2HFE_E_NOTIPF = -3,   /* CAPS rejected the image */
    IPF2HFE_E_WRITE = -4,
    IPF2HFE_E_TOOLONG = -5,  /* a track does not fit in HFE (>32KB/side) */
    IPF2HFE_E_OPEN = -6,
    IPF2HFE_E_READ = -7
};

/* Convert an IPF image held in memory. The buffer is referenced (not
 * copied) by the decoder during the call, so it must stay valid until
 * the function returns. On ESP32-P4 put it in PSRAM. */
int ipf2hfe_convert_mem(uint8_t *ipf, uint32_t ipf_len,
                        ipf2hfe_write_fn wr, void *wr_ctx,
                        ipf2hfe_progress_fn prog, void *prog_ctx,
                        ipf2hfe_stats *stats);

/* Convenience wrapper using stdio (works on Linux and on ESP32 VFS
 * paths such as "/sdcard/GAMES/foo.ipf"). Loads the whole IPF into RAM
 * (heap_caps PSRAM on ESP32). Writes to hfe_path.tmp then renames. */
int ipf2hfe_convert_file(const char *ipf_path, const char *hfe_path,
                         ipf2hfe_progress_fn prog, void *prog_ctx,
                         ipf2hfe_stats *stats);

const char *ipf2hfe_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* IPF2HFE_H */
