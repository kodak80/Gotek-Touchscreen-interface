# GTi backlog

One shared list for Mez, Dimitri and both their Claude sessions. Decisions are made between
Mez and Dimitri (their Claudes agree a proposal first, over the session relay). Add the date and
where it was decided. Move an item to Done with its commit or branch once it lands.

## In progress

- **CRACKTRO style pick page** (decided 6 Oct 2026). Settings gets a CRACKTRO STYLE row under the
  CRACKTRO toggle that opens a pick page (RANDOM or one of the 7 styles, current one marked > <);
  the choice is written to CONFIG.TXT CRACKTRO=. Branch `cracktro-style-pick` (Dimitri's side).
  When `cracktro-gti` lands, CUSTOM and the /cracktro/*.gti names become extra rows on that page.
- **cracktro-gti brought up to date with main** (6 Oct 2026). CUSTOM .gti cracktros on top of
  main's CRACKTRO ON/OFF toggle; the cracktro screensaver is a choice in the SAVER FX row.
- **Default mDNS name of a screen: GTi-XXXX** (decided 6 Oct 2026, option d). Every screen's
  default hostname is GTi-XXXX, XXXX = last 4 hex of its MAC in UPPERCASE (the same as
  screenName()). The fleet election leader also answers gotekomega.local as a delegated hostname
  (mdns_delegate_hostname_add, its address re-set when the IP changes, the http service added with
  mdns_service_add_for_host), so the link in the Webby portal keeps working with no Webby
  release. MDNS_NAME= in CONFIG.TXT still overrides. Branch `panel-fleet-a600`.

## To do

- **Per-dongle saves in the hivemind fan-out** (decided 6 Oct 2026). Rule: every WIRELESS save is
  Game.sav.XXXX.adf, XXXX = that dongle's MAC suffix, whether 1 dongle or 50. The cable save stays
  Game.sav.adf (it is local to that screen). Today the fan-out pins saves and skips the per-dongle
  file (doLoadSelected, lab15i: fanOut means no dongleSavTag and g_sv_wl_* cleared). Wanted: in a
  fan-out each dongle gets its own .sav.XXXX.adf if it exists (else the shared disk), and its saves
  come back into its own file. Next step: a branch and approach, agreed between the two sides.

## Parked

- **Multi-select SEND to several dongles, following the hivemind rules** (parked 6 Oct 2026, WIP).
  Several dongles selected = send the disk only: no 0x0A, no name. Nobody is asking for it yet.
- **Hivemind session with an exclusive lock** (Mez's idea, 6 Oct 2026; Mez designs it when it is
  picked up). A screen that starts a hivemind session takes over the dongles and locks out other
  screens for the duration: their CLAIM is refused like 0x03, naming the hivemind screen.

## Who can test what (6 Oct 2026)

| | Mez | Dimitri |
|---|---|---|
| S3 screens | JC3248 3.5" (main dev board), JC4827W543 4.3", Waveshare 7" RGB, Waveshare S3-Touch-LCD-7B (2.8" and CYD: legacy only) | Guition JC3248 3.5" (the same board as Mez's main dev board) |
| P4 screens | JC4880P443C 4.3", JC1060P470C 7", Waveshare P4-WIFI6-Touch-LCD-7B, P4-WIFI6-Touch-LCD-3.4C round (all chip rev v1.3) | none known yet |
| Dongles | ESP32-C3 SuperMini, Seeed XIAO, S3 Zero (all Webby) | Many ESP32-C3 SuperMinis; one Seeed XIAO with external antenna (Webby-1.6.10-xiao) |
| Amigas | A500s, A600s | A500 rev 3, rev 5 and rev 6A; A600; A600HD; A1200; A3000 |
| Accelerators | | PiStorm in one A600 and in the A500 rev 6A, both with FrameThrower |
| Gotek hookup | | Internal Gotek in the PiStorm A600 (every build is tested there; all have worked so far). External Gotek on the A500s. Internal Gotek in the A3000. |

- P4 builds: Mez's bench only, and only v1.3 silicon is verified.
- Multi-screen and two-screens-on-one-LAN tests (e.g. GTi-XXXX + gotekomega.local): Mez's bench.
- A3000, PiStorm/FrameThrower setups and (once fully tested) A1200: Dimitri's bench.
- Non-Amiga hosts (CPC, Atari ST, ...): outside testers Mez sends builds to.

## Agreed working rules (6 Oct 2026)

- One branch per topic, from current main; merge main in often.
- Before editing Gotek_JC3248.ino, announce the area on the relay and claim it. Mez often works
  at night in another timezone: don't wait for a reply when the areas don't overlap.
- Commit messages in English, version tag in the subject.
- The firmware/*/build/ .bins stay committed as the release library people pick from and
  downgrade to. (How to keep stale build output out of them is still open: relay #34.)
- Every setting must be reachable both in CONFIG.TXT and on the Settings screen.
