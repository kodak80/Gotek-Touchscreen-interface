// Gotek_S3Zero_LEDTest — status-LED blink for the Waveshare ESP32-S3-Zero
// ------------------------------------------------------------------------
// The Zero has ONE onboard LED: a single WS2812 (addressable RGB) on GPIO21.
// It is NOT a plain on/off LED — you clock the WS2812 data protocol out to it.
// The Arduino-ESP32 core (v3.x) does that for us with neopixelWrite(), so this
// sketch needs NO external library.
//
// Arduino IDE board settings for the Zero:
//   Board:            "ESP32S3 Dev Module"  (or "Waveshare ESP32-S3-Zero" if listed)
//   Flash Size:       4MB    PSRAM: "QSPI PSRAM"  (chip is ESP32-S3FH4R2)
//   USB CDC On Boot:  Enabled   (only needed if you want Serial over the USB-C port)
//
// This is a bench blink to prove the LED + pick status colours. The setStatus()
// helper is written so it lifts straight into Webby later (call it on state change).

#define LED_PIN        21    // WS2812 data — fixed on the S3-Zero, don't change
#define LED_BRIGHTNESS 24    // 0..255. Keep it low — a WS2812 at 255 is blinding.

// --- the states we'll want the dongle to show ---
enum LedStatus {
  ST_BOOT,      // just powered up
  ST_IDLE,      // up, nothing happening
  ST_AP,        // own AP up (blind / ESP-NOW pairing)
  ST_WIFI,      // joined home WiFi
  ST_ACTIVITY,  // a FLING / transfer in progress
  ST_ERROR      // something went wrong
};

// This Zero's onboard LED reads its data bytes in R,G,B order, but the ESP32 core's
// neopixelWrite() emits WS2812-standard G,R,B — so logical green came out PURPLE
// (red/green swapped). Passing (g, r, b) instead of (r, g, b) corrects it. If you
// ever meet a board that's true GRB, set LED_SWAP_RG to 0.
#define LED_SWAP_RG 1

static inline void led(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t R = (uint16_t)r * LED_BRIGHTNESS / 255;
  uint8_t G = (uint16_t)g * LED_BRIGHTNESS / 255;
  uint8_t B = (uint16_t)b * LED_BRIGHTNESS / 255;
#if LED_SWAP_RG
  neopixelWrite(LED_PIN, G, R, B);   // corrected order for this Zero
#else
  neopixelWrite(LED_PIN, R, G, B);   // standard WS2812
#endif
}
static inline void ledOff() { neopixelWrite(LED_PIN, 0, 0, 0); }

// map a status to a colour — tweak to taste
static void setStatus(LedStatus s) {
  switch (s) {
    case ST_BOOT:     led(255, 140,   0); break; // amber
    case ST_IDLE:     led(  0,  40,   0); break; // dim green
    case ST_AP:       led(  0,   0, 255); break; // blue
    case ST_WIFI:     led(  0, 200,  60); break; // green
    case ST_ACTIVITY: led(160,   0, 200); break; // purple
    case ST_ERROR:    led(255,   0,   0); break; // red
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("S3-Zero status LED test — WS2812 on GPIO21");
  setStatus(ST_BOOT);
  delay(600);
}

// simple heartbeat blink so you can see it's alive: green on/off, ~1 Hz.
// (Swap the body for setStatus(...) calls once it's wired into Webby.)
void loop() {
  led(0, 200, 60);   // green on
  delay(500);
  ledOff();          // off
  delay(500);
}
