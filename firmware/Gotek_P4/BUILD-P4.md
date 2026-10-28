# Gotek_P4 — Arduino build settings (Toby)

Full JC **5.8.6** interface ported to the **Guition JC4880P443C (ESP32-P4)**.
On-screen/firmware identity is now **`5.8.6-P4`** (marker `OMEGAWARE.GTi.P4.fw`).
Compiles clean under **arduino-esp32 3.3.11** (`esp32:esp32:esp32p4`): 29% flash, 46% RAM.
**Not yet run on hardware** — this build is the first bring-up.

## Board Manager
- Install/enable **arduino-esp32 core 3.3.11 or newer** (must list *ESP32P4 Dev Module*).

## Tools ▸ menu selections
| Setting | Value |
|---|---|
| Board | **ESP32P4 Dev Module** |
| Flash Size | **16MB** — use 16MB even on a 32MB-flash unit: a 16MB layout boots on a 32MB chip, but a 32MB layout does **not** boot on a 16MB chip. (The module's 32MB is PSRAM, not flash. Confirm the flash chip with `esptool --chip esp32p4 flash_id` if unsure.) |
| PSRAM | **Enabled** — 32 MB HP PSRAM (framebuffers + ramdisk live here) |
| CPU Frequency | **360 MHz** — leave default. ⛔ do **not** force 400 MHz: confirmed unstable on this v1.3 silicon. |
| USB Mode | **USB-OTG (TinyUSB)** — required for the USB-MSC floppy |
| USB CDC On Boot | **Disabled** (flash via BOOT-hold, same as the S3 boards) |
| Partition Scheme | dual-app OTA; see note below (a sketch-local `partitions.csv` overrides the menu) |
| Upload Speed | 460800 |

## Partition sizing — the short version
This firmware uses **no internal flash filesystem**: the game library, cover art and
CONFIG.TXT are all on the **SD card**, and the USB-MSC ramdisk + decode buffers are in
**PSRAM** (`ps_malloc`). Internal flash only needs **NVS + OTA-data + two app slots** (the
second slot is what makes the on-device SD-update / FW-UPDATE work). So partition size is
driven by the **app image + OTA**, *not* by how big the flash chip is — any surplus flash is
simply unused. 3 MB app slots give large headroom (app is ~0.9 MB today).

**Use the `partitions.csv` already in this folder** and pick **Tools > Partition Scheme > Custom**
(the option appears only when the sketch folder has a `partitions.csv`; close and reopen the sketch
if it is missing). It is the 16 MB layout: two 3 MB app slots, FAT, and the **c6fw** area that holds
the Wi-Fi co-processor (C6) firmware. With Custom selected the IDE reports "Maximum is 16777216
bytes" - the real limit is the 3 MB app slot (lab6-P4-ipf3 is ~2.08 MB).

> The two tables that used to be printed here (Aug layout) have **no c6fw area**. Do not copy them
> over `partitions.csv`.

## Flash (esptool, any OS, or a web flasher)
Which file goes where - **an app-only `.bin` at 0x0 wipes the bootloader and the board stays black**
(fix: flash a full merged image at 0x0):

| File | Offset |
|---|---|
| full merged image (`*-c6.merged.bin` from the GTi flasher, bootloader + partitions + app + C6) | `0x0` |
| `Gotek_P4.ino.bootloader.bin` | `0x2000` (the P4's bootloader is at 0x2000, not 0x1000) |
| `Gotek_P4.ino.partitions.bin` | `0x8000` |
| `Gotek_P4.ino.bin` (the app; Sketch > Export Compiled Binary) | `0x10000` |

```
esptool --chip esp32p4 -p <PORT> -b 460800 write_flash 0x0 <full-merged>.bin
esptool --chip esp32p4 -p <PORT> -b 460800 write_flash 0x10000 Gotek_P4.ino.bin   # app only
```
Without a cable: copy `Gotek_P4.ino.bin` to the SD card root as `GTi-P4-<anything>.bin` and use the
GTi's firmware update (it only takes `GTi-P4-*.bin`).
Won't connect? **Hold BOOT, tap RESET, release BOOT**; close the serial monitor first;
use the USB port that flashes (USB-Serial-JTAG), supply >=600 mA.

## First-run expectations (bring-up)
- **UI should render** in portrait 480×800 (reflowed by `relayout()`). A few 320×480 layout
  constants may need tidying once we see it — not a rewrite.
- **Touch pins (RST22/INT21, addr 0x5D) are community-sourced.** UI draws but touch dead/offset
  = wrong pins/addr; send the real ones.
- **USB-MSC** presents the PSRAM ramdisk (Arduino P4 MSC is documented flaky) — confirm the
  Gotek/PC mounts it; try both USB-C ports (HS OTG = GPIO49/50).
- **SD** (`SD_MMC`) TF pins unconfirmed on this board — browse may need pins set.
- Renders then reboots = suspect 400 MHz-on-v1.3; tell me and I'll pin 360 by build flag.
- **Wireless is stubbed** (ESP-NOW doesn't link on P4) — flinging is a no-op until the
  WiFi/TCP Webby transport is folded in (Stage 3).

## Report back for the next iteration
1) `flash_id` size, 2) touch working y/n + real pins, 3) which USB-C enumerates, 4) a photo of the UI.

## Board notes — confirmed on hardware (Sep 4)
- **USB-C ports:** the one **furthest from the BOOT/RESET buttons** is the **flashing** port
  (USB-Serial-JTAG). The near port is USB-OTG (the disk / USB-MSC side).
- **Reflashing a running GTi build** needs manual download mode: **hold BOOT, tap RESET, release
  BOOT**, then pick the serial port that appears (CDC is off, so a running build shows as a USB
  drive, not a port, and won't auto-reset).
- **Touch (GT911):** I2C **SDA=7 / SCL=8**; **RST and INT are NOT connected** to the MCU on this
  board (vendor BSP `esp32_p4_function_ev_board`: `BSP_LCD_TOUCH_RST/INT = GPIO_NUM_NC`). Do **not**
  drive GPIO21/22 — that was a wrong community guess. The chip powers up on the I2C bus; address is
  0x5D (0x14 fallback). Fixed in 5.8.6-P4.2.
- Backlight = GPIO23, panel = ST7701 480x800 DSI (matches the benchmark config).

## lab6-P4: the 7" P4G7 changes, ported (hfe1 → ipf3)
Same code as the 7" JC1060 build (`firmware/Gotek_P4_JC1060`, 5.9.42-lab7-P4G7-ipf3), applied on top of
5.9.42-lab5-P4. Only the version line differs; the board layer (ST7701 panel, GT911 touch) is unchanged.
- **hfe1:** wireless HFE of any size the dongle holds; TOO BIG / NOT LOADED shown at once; a disk the
  dongle did not take is not shown as IN DRIVE.
- **hfe2:** GENERIC library at power-on; CONFIG.TXT `LIBRARY=GEN|ADF|DSK`.
- **hfe3:** wireless EJECT retries the TCP connect and the save pull on a weak link.
- **ipf1 → ipf3:** `.ipf` disks are converted on the GTi to HFE v3 the first time they are loaded and
  stored beside the `.ipf` as `Game.ipf.hfe`; later loads reuse it and the library list hides
  `*.ipf.hfe`. The decoder is the SPS CAPS library in `src/ipf/capsimg` (non-commercial licence,
  `src/ipf/capsimg/LICENCE.txt`). Nothing extra to install: Arduino builds `src/` automatically.
  PSRAM must stay enabled (about 2.5 MB is used during a conversion). Details, tests and a desktop
  converter: `firmware/ipf_tools/README.md`.

Chip revision v1.x (JC4880 v1.3): keep **Chip Variant = Before v3.00**. The v3.x build on the web
flasher (P4V3) is the same source compiled for the newer chips.

First tests on a JC4880: an ADF and an HFE over the cable, an `.ipf` insert (the first one converts:
a progress bar on the cover panel, about 6 s on the 7"), a wireless send + EJECT, and `LIBRARY=` at boot.
