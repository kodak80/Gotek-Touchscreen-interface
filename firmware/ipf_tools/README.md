# GTi IPF support (ipf2hfe)

Converts `.ipf` disk images to HFE v3 on the ESP32-P4. The Gotek then plays
the result with stock FlashFloppy. Decoding uses the SPS CAPS library
(capsimg 5.1, non-commercial licence); everything else is new code.

## What gets preserved

| IPF feature | HFE v3 output |
|---|---|
| Track bits | Bit-exact copy of the CAPS decode, index-aligned (data-over-index kept) |
| Auto density / long and short tracks | Cells scaled so each revolution is 200 ms, using BITRATE with dithering (timing error under 2 µs) |
| Variable density (Copylock, Speedlock, ...) | BITRATE follows the CAPS density map byte by byte |
| Weak / fuzzy bits | RAND opcodes covering the weak areas CAPS reports |
| Track length not a multiple of 8 | SKIPBITS |
| Unformatted tracks | 100,000 RAND cells |
| Bytes starting with 1111 (would look like an opcode) | Re-encoded exactly as two SKIPBITS halves |

The current bitrate is re-sent every 64 bytes. This is needed because
FlashFloppy can resume reading mid-track after a seek and would otherwise
keep a stale rate.

## Where things are

```
firmware/Gotek_P4_JC1060/src/ipf/   built into the P4G7 firmware (Arduino compiles src/ recursively)
  ipf2hfe.h / ipf2hfe.cpp           converter (C API, callback writer, no Arduino deps)
  gti_ipf.h                         GTi glue: gtiIpfPrepare(SD_MMC, ipfPath, hfeOut, ...) -> Game.ipf.hfe beside the .ipf
  capsimg/                          SPS decoder library 5.1, flattened (LICENCE.txt inside)
firmware/ipf_tools/                 desktop only - never part of a firmware build
  tools/ipf2hfe_cli.cpp             desktop converter
  tests/verify.cpp                  FlashFloppy HFE v3 playback model + checker
  tests/mkipf.py                    synthetic IPF generator (long/odd tracks, Copylock, fuzzy, noise)
  tests/glue_test.cpp + tests/shim  runs gti_ipf.h on Linux against a folder standing in for the SD card
  Makefile                          make && make check IPF=game.ipf
```

## How the P4G7 firmware uses it

- `doLoadSelected()`: a disk whose file is an `.ipf` (and has no save yet) is converted to
  `<name>.ipf.hfe` beside it on its first load (ipf3; ipf1/ipf2 used `/GTI/ipfcache`). A progress bar is shown on the cover
  panel. Later loads reuse that file. From there on it is an ordinary HFE load (RAM disk or alias,
  cable or wireless).
- `presentName()` / wireless fling name: the Gotek sees `.hfe` (`DISK.HFE` in the ADF/DSK
  libraries), because FlashFloppy picks the format from the extension.
- `savPathFor()` / `dongleSavPathFor()`: saves of `Game.ipf` go to `Game.sav.hfe` (or
  `Game.sav.XXXX.hfe`) beside it, as patched copies of the converted HFE.
- The ADF and DSK libraries list `.ipf` files. GENERIC already listed every file, and now skips
  `.ipx` analyser sidecars and the converted `.ipf.hfe` files.
- gti.log: `[ipf] ... -> ... : N cyl x 2, ... variable-density tracks, weak, unformatted`, or `(cached)`.
- Delete the `.ipf.hfe` beside an `.ipf` to force re-conversion (e.g. after replacing the `.ipf`).

## Test results (desktop, FlashFloppy playback model)

Every track of every file plays back bit-identical to the CAPS decode (outside
weak areas), with each revolution at 200 ms ± 2 µs.

| Image | Result |
|---|---|
| Xenon 2 disk 1 (Amiga, Copylock on track 0.1) | 1749 AmigaDOS sectors OK; density zones 94.6 % / 99.6 % / 104.6 % reproduced |
| Xenon 2 disk 2 (Amiga) | 1760 sectors OK |
| Toki (Amiga, custom format with 105,168-cell long tracks) | bit-exact, 200 ms |
| Artura (Atari ST, fuzzy-sector protection on track 79) | the 8192 weak cells (one whole sector) become RAND exactly; the rest is bit-exact; 798 sectors OK, and the fuzzy sector reads bad, as it should |
| Time disks 1–3, Waterski, Fire and Brimstone 1–2 (Atari ST) | all ST sectors OK; the only "bad" sectors are the same in the CAPS decode itself |
| Synthetic test IPF | long and odd-length tracks, data-over-index, fuzzy sector and noise track all pass |

## Known limits

- No-flux areas: FlashFloppy emits a pulse roughly every 2^14 sample-clock
  ticks during long no-flux runs. This is a FlashFloppy limit, not a converter
  one, and may affect protections that depend on no-flux areas.
- Writes: FlashFloppy can write to HFE v3, so saving to the cached HFE works.
  The original `.ipf` is never modified.
- The ESP32-P4 build hasn't been compiled here (no toolchain in this
  workspace). capsimg was originally a 32-bit Windows DLL, builds without
  warnings that matter under gnu++2b, and has no exceptions or RTTI, so it
  should build as-is.
- Saves: a converted HFE is about 2.2 MB, which is bigger than the default BIGDISK save window
  (1.76 MB), so only the first ~64 cylinders' writes are saved unless BIGDISK=ON. gti.log says so
  when it happens.
- Wireless: the dongle receives the HFE, so its size limit applies (SuperMini 1.76 MB says TOO
  BIG; the XIAO hfe3 takes it).
- First-time conversion speed on the 7″ P4: about 6 seconds per disk (measured on hardware with
  Atari ST and Amiga IPFs). On a PC it takes 20-40 ms per disk. Later loads reuse `Game.ipf.hfe`
  and cost nothing extra.
