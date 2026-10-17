# GTi — XIAO Webby dongle (Seeed XIAO ESP32-S3)

The Webby build of the XIAO wireless dongle: it takes disks from a GTi panel (or its own web page)
and presents them to a Gotek over USB. Current build: **`Webby-1.6.4-xiao-hfe4`** (kodak80 fork).

## What the hfe builds add
- **Big disks.** The RAM disk lives in PSRAM and is sized at boot: 6 MB if it fits, otherwise 4.5 MB,
  otherwise 1.75 MB. The largest disk it takes is shown on the web page (`/status` → `max`).
  HFE images up to about 6 MB load wirelessly from a 7″ P4 or 3.5″ JC3248 panel.
- **HFE recognised by content.** An image starting `HXCPICFE` / `HXCHFEV3` always mounts as `.hfe`,
  whatever name it was sent under, so FlashFloppy picks the right format.
- **Real file names.** The disk is mounted under the name the panel sent (`Prophecy (Disk 1).hfe`,
  `Lotus II.st`, `Turrican.adf`), as a VFAT long name, instead of `DISK.ADF`. Older panels that send
  no name still get `DISK.ADF`.
- The web upload page accepts `.hfe`.

## Board settings (Arduino IDE)
Board **XIAO_ESP32S3**, PSRAM **OPI PSRAM**, Flash size **8MB**, Partition scheme **Default 8MB with spiffs**,
USB Mode **USB-OTG (TinyUSB)**, USB CDC on Boot **Disabled**, CPU **240 MHz**.

## Which file to flash
- **`Gotek_XIAO_Webby.ino.merged.bin`** — the full image, flashed at address **0x0** with the web
  flasher or `esptool`. Use this on a new board.
- **`Gotek_XIAO_Webby.ino.bin`** — the app only, for an update through the dongle's own web page.
  Flashing this one at 0x0 leaves the board in a reset loop.

## Limits
- The SuperMini dongle stays at 1.76 MB; a panel shows **TOO BIG** if you send it something larger.
- Saves to a wireless HFE are pulled back by the panel on eject, as for ADFs.
