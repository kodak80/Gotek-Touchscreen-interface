# Docs — human-only capture list

_Maintained by the `gotek-docs` skill. These are the documentation assets the skill **cannot** make — screenshots of a running device, photos of hardware. Everything text-shaped (config keys, versions, feature blurbs) the skill writes itself; only genuinely visual items land here. Delete an entry once its asset is shot and placed._

## JC3248 / JC4827 — 5.9.7

- **OMEGA theme (`THEME=6`, the house default)** — a screenshot of the library running the OMEGA theme. Why: it's now the default reskin and the 7th theme; people pick themes by look, not name. Where: theme gallery in `docs/guide.html` / the download-page feature blurb.
- **`CRACKTRO=7` "OMEGAWARE" boot intro** — a screenshot (or short clip still) of the 1991 OMEGAWARE cracktro. Why: new cracktro style; the reader chooses cracktros by look. Where: alongside the other cracktro examples in the guide.
- **Web panel (`WEBUI=ON`)** — a screenshot of the shared web page served from the panel over home WiFi (the dashboard + upload/OTA view). Why: brand-new user-facing surface with no visual in the docs. Where: a "browse to your GTi" section in `docs/guide.html`.

## Flasher board cards (still SVG placeholders)

- **JC4827 (4.3″)**, **P4 (JC4880)**, **CYD** — a plain-background board photo each, dropped in `docs/img/` as `jc4827.jpg` / `p4.jpg` / `cyd.jpg`. Why: the other cards show real boards; these fall back to the schematic SVG. (JC3248, Super Mini, XIAO, Waveshare 7/7B already have photos.)

## kodak80 fork — wireless HFE + IPF (Oct 2026)

- **IPF conversion progress bar (7″ P4, `5.9.42-lab7-P4G7-ipf3`)** — a photo of the cover panel mid-conversion the first time an `.ipf` is loaded. Why: new screen that only appears once per disk; readers should recognise it as normal, not a hang. Where: `README.md` → "IPF disks (7″ P4 only)", and the IPF row in `docs/features.html`.
- **TOO BIG on a SuperMini send** — a photo of the panel after sending an HFE larger than 1.76 MB to a SuperMini. Why: new refusal message replacing the old silent 10 s wait. Where: `README.md` GEN notes.
- **FlashFloppy showing a long file name from the XIAO** — a photo of the Gotek's display with a real name (e.g. a `.hfe` game) instead of `DISK.ADF`. Why: shows the XIAO Webby hfe4 long-name mount. Where: footnote ⁴ under the dongle table in `README.md`.
