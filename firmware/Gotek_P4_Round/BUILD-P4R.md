# Gotek_P4_Round — Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C (3.4" round, 800×800)

**5.9.41-lab15n-P4R — bring-up build (30 Sep 2026).** Source = the 4.3" P4 `5.9.41-lab15n-P4` + the 3.4C board layer.
The normal square GTi UI is drawn **inside the circle** (a 624×464 landscape canvas in the middle of the glass).
The round layout (reel home screen, A–Z around the rim) is a later build, after a mock-up is approved (R9).

## Board layer (vs the 4.3" P4)
| Area | 4.3" P4 (JC4880) | 3.4C round (this sketch) |
|---|---|---|
| Panel | ST7701 480×800 portrait | **JD9365 800×800** (only the inscribed circle is glass) |
| DSI | 2 lanes | 2 lanes @ **1500 Mbps**, DPI **80 MHz**, h 20/20/40 v 4/12/24 |
| Reset / backlight | RST 5 / BL 23 | **RST 27 / BL 26 (active-low)** |
| Compose | 480×800 = whole panel | **464×624** (same portrait layout) → turned upright into the middle, rest black |
| Touch | GT911 SDA7/SCL8 | **GT9271** (GT911 protocol) SDA7/SCL8, 0x5D/0x14; ring touches ignored |
| microSD | 1-bit CLK12/CMD11/D0 13 | **4-bit** CLK43/CMD44/D0–D3 39–42, 1-bit fallback |
| SD update tag | `P4` / `OMEGAWARE.GTi.P4.fw` | **`P4R` / `OMEGAWARE.GTi.P4R.fw`** (never installs the other's .bin) |

Panel data: Waveshare BSP `waveshare/esp32_p4_wifi6_touch_lcd_xc` 3.0.1 (3.4" option) and their Arduino example —
same 197-command JD9365 init list in both. `esp_lcd_jd9365.c/.h` = Espressif's component v2.0.2 (Apache-2.0),
vendored; only the version log line is a literal.

## Tools ▸ menu
| Setting | Value |
|---|---|
| Board | ESP32P4 Dev Module |
| **Chip Variant** | **must match the chip**: esptool prints "Chip is ESP32-P4 (revision v1.x / v3.x)" when it connects. v1.x → "Before v3.00"; v3.x → "v3.00 or newer". Wrong one = won't boot. |
| Flash Size | 16MB (the board has 32 MB; 16 is fine) |
| Partition Scheme | Custom (sketch-local partitions.csv) |
| PSRAM | Enabled |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Disabled |
| Debug level | None |

## P4R-2 — 5.9.41-lab15o-P4R: the ROUND REEL (30 Sep 2026)
The reel now uses the whole glass (800×800) and is the home screen. The list and Settings are still the square canvas.
- `REELSTYLE=MOON` (default): covers ride an arc across the top, the one at 12 o'clock is selected. `REELSTYLE=FLAT`: one big cover, neighbours at the sides. Settings ▸ REEL STYLE.
- Status round the top rim; title, one line of the description, disk circles (up to 7 shown, `<` `>` when there are more), INSERT/EJECT.
- Bottom rim buttons: ROLL · LIST · CONFIG · ALL/FAV/MOST. Tap the cover or the title = the whole .nfo.
- `ROUNDUI=OFF` = the old square reel (Settings ▸ ROUND UI). `ROUNDHOME=LIST` = boot into the list instead of the reel.
- Settings ▸ TEST TOOLS ▸ **RIM TOUCH TEST**: tap 21 dots (12 on the rim, 8 inside, centre); gti.log gets how far each touch landed.
- How: round screens draw into their own 800×800 buffer (`roundBegin()`), `gfx_flush()` shows it with `p4disp_present_full()`;
  touch follows the last screen shown (round = 800×800 coordinates, ring included; square = the 624×464 canvas).

## P4R-2b/2c/3 — 5.9.41-lab15p/q/r-P4R (30 Sep 2026)
- 15p: a square screen after the round reel clears the ring (no reel leftovers round the edge).
- 15q: toasts (saves, dongle linked, SAVING GAME), load/eject endings, web-UI loads and the take-over question return to / show on the round screen.
- 15r **ROUND LIST**: the games as a wheel (middle row selected with a small cover, rows follow the edge and fade), A–Z round the
  right rim (tap a letter or slide a thumb along it - a big letter shows where you are), rim buttons INSERT/EJECT · REEL · CONFIG ·
  LIB (ADF→DSK→GEN). Tap a row = bring it to the middle; tap the middle row = the reel on that game. The reel's LIST button opens it.
  `drawFullUI()` now lands on the round reel/list whenever the round UI is on (leaving Settings, rescan, library switch, screensaver) -
  the square list is not used with ROUND UI on. `ROUNDHOME=LIST` boots into the round list.
- Still square: Settings, the .nfo reader, keyboard, dialogs, load progress, SD ACCESS, screensaver, boot screens (next builds).

## Hidden CONFIG.TXT keys for bring-up
- `PANELTURN=0|90|180|270` — turns the whole picture **and** touch, if the panel's "up" isn't the board's "up".
- `ROTATE=` works as on the 4.3" P4 (landscape / portrait / flipped).

## What to check (report back, with gti.log)
1. Picture appears, upright, centred, nothing cut off by the round edge; backlight on.
2. Touch: taps land where you touch. gti.log has `[touch] raw x,y` for the first 16 touches.
3. `[sd] clock 20000 kHz, 4-bit` (or 1-bit) in gti.log; library lists; covers.
4. Cable load → the PC sees DISK.ADF. SD ACCESS works.
5. Soak test (Settings ▸ TEST TOOLS) once.
