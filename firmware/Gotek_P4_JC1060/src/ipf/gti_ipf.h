/*
 * gti_ipf.h - GTi glue: turn an .ipf into a cached HFE v3 that the
 * existing HFE insert path can mount.
 *
 * Header-only, Arduino (arduino-esp32 3.x) + fs::FS (SD_MMC, SD, FFat...).
 *
 *   #include "src/ipf/gti_ipf.h"
 *   String hfe;
 *   if (gtiIpfPrepare(SD_MMC, "/GAMES/Foo.ipf", hfe, onProgress))
 *       insertHfe(hfe);   // GTi's existing HFE insert routine
 *
 * The HFE is written to GTI_IPF_CACHE_DIR/<name>.<ipfsize hex>.hfe and
 * reused on later inserts. Delete the folder to force re-conversion.
 * Needs PSRAM enabled (about 2.5 MB peak for a typical disk).
 */
#pragma once
#include <Arduino.h>
#include <FS.h>
#include "ipf2hfe.h"
#include "esp_heap_caps.h"

#include <sys/stat.h>

#ifndef GTI_IPF_CACHE_DIR
#define GTI_IPF_CACHE_DIR "/GTI/ipfcache"
#endif
/* VFS mount point of the card, for stat(): SD_MMC's File::size() reports 0
 * for files in sub-folders on this core, so sizes come from the VFS. */
#ifndef GTI_IPF_VFS_ROOT
#define GTI_IPF_VFS_ROOT "/sdcard"
#endif

static inline uint32_t gtiIpfFileSize(File &f, const char *path)
{
    uint32_t n = f ? (uint32_t)f.size() : 0;
    if (n) return n;
    struct stat st;
    String v = String(GTI_IPF_VFS_ROOT) + path;
    return stat(v.c_str(), &st) == 0 ? (uint32_t)st.st_size : 0;
}

static inline bool gtiIsIpf(const char *path)
{
    size_t n = path ? strlen(path) : 0;
    return n > 4 && !strcasecmp(path + n - 4, ".ipf");
}

static int gti_ipf_fs_write(void *ctx, uint32_t off, const void *buf, uint32_t len)
{
    File *f = (File *)ctx;
    if (f->position() != off && !f->seek(off))
        return -1;
    return f->write((const uint8_t *)buf, len) == len ? 0 : -1;
}

/* Cache path for an IPF: /GTI/ipfcache/<basename-without-ext>.<size>.hfe */
static inline String gtiIpfCachePath(const char *ipfPath, uint32_t ipfSize)
{
    const char *base = strrchr(ipfPath, '/');
    base = base ? base + 1 : ipfPath;
    String name(base);
    name.remove(name.length() - 4);        /* drop .ipf */
    char sz[12];
    snprintf(sz, sizeof(sz), ".%lx", (unsigned long)ipfSize);
    return String(GTI_IPF_CACHE_DIR) + "/" + name + sz + ".hfe";
}

/* Convert (or reuse cached) IPF -> HFE. On success hfeOut holds the HFE
 * path on the same filesystem. Blocking: ~1-3 s per disk on the P4 the
 * first time; call the UI's tick from the progress callback if needed. */
static inline bool gtiIpfPrepare(fs::FS &fs, const char *ipfPath, String &hfeOut,
                                 ipf2hfe_progress_fn prog = nullptr,
                                 void *progCtx = nullptr,
                                 ipf2hfe_stats *statsOut = nullptr)
{
    File in = fs.open(ipfPath, FILE_READ);
    if (!in) {
        Serial.printf("[IPF] cannot open %s\n", ipfPath);
        return false;
    }
    uint32_t size = gtiIpfFileSize(in, ipfPath);
    String hfe = gtiIpfCachePath(ipfPath, size);

    if (fs.exists(hfe)) {
        File h = fs.open(hfe, FILE_READ);
        bool ok = h && gtiIpfFileSize(h, hfe.c_str()) > 1024;
        if (h) h.close();
        if (ok) {
            in.close();
            hfeOut = hfe;
            Serial.printf("[IPF] cached %s\n", hfe.c_str());
            return true;
        }
        fs.remove(hfe);
    }

    if (size < 12 || size > 16u * 1024 * 1024) {
        in.close();
        return false;
    }
    uint8_t *buf = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        in.close();
        Serial.printf("[IPF] no PSRAM for %u bytes\n", (unsigned)size);
        return false;
    }
    size_t got = 0;
    while (got < size) {
        int r = in.read(buf + got, size - got);
        if (r <= 0) break;
        got += r;
    }
    in.close();
    if (got != size) {
        free(buf);
        return false;
    }

    if (!fs.exists(GTI_IPF_CACHE_DIR)) {
        fs.mkdir("/GTI");
        fs.mkdir(GTI_IPF_CACHE_DIR);
    }
    String tmp = hfe + ".tmp";
    fs.remove(tmp);
    File out = fs.open(tmp, FILE_WRITE);
    if (!out) {
        free(buf);
        Serial.printf("[IPF] cannot create %s\n", tmp.c_str());
        return false;
    }

    ipf2hfe_stats st;
    uint32_t t0 = millis();
    int rc = ipf2hfe_convert_mem(buf, size, gti_ipf_fs_write, &out,
                                 prog, progCtx, &st);
    out.close();
    free(buf);

    if (rc != IPF2HFE_OK) {
        fs.remove(tmp);
        Serial.printf("[IPF] %s: %s\n", ipfPath, ipf2hfe_strerror(rc));
        return false;
    }
    fs.remove(hfe);
    if (!fs.rename(tmp, hfe)) {
        fs.remove(tmp);
        return false;
    }
    Serial.printf("[IPF] %s -> %s in %lu ms: %d cyl x %d, var %d, weak %d, noise %d\n",
                  ipfPath, hfe.c_str(), (unsigned long)(millis() - t0), st.cyls,
                  st.sides, st.var_tracks, st.weak_tracks, st.noise_tracks);
    if (statsOut) *statsOut = st;
    hfeOut = hfe;
    return true;
}
