#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

// ============ SETTINGS ============
// Switches: Active-low type (using internal pull-up)
constexpr size_t N = 5;
const uint8_t SWITCH_PINS[N] = {13, 12, 11, 10, 9};
const uint32_t DEBOUNCE_MS = 30;

// --- Key bindings for Street Fighter 6 (change as needed) ---
#define KEY_UP     HID_KEY_W
#define KEY_LEFT   HID_KEY_A
#define KEY_DOWN   HID_KEY_S
#define KEY_RIGHT  HID_KEY_D

#define LP   HID_KEY_J           // Light Punch
#define MP   HID_KEY_K           // Medium Punch
#define HP   HID_KEY_SEMICOLON   // Heavy Punch
#define LK   HID_KEY_N           // Light Kick
#define MK   HID_KEY_M           // Medium Kick
#define HK   HID_KEY_COMMA       // Heavy Kick

// Which button triggers which move (change to your preference)
enum : uint8_t { BTN_HADOU=0, BTN_SHORYU=1, BTN_TATSU=2, BTN_TOGGLE_FACE=3 };

// Timings (ms) — Increase slightly if inputs are dropped
const uint16_t STEP = 28;        // Duration to hold each direction step
const uint16_t TAP  = 22;        // Tap duration for attack buttons
const uint16_t GAP  = 16;        // Gap between steps

// ============ HID SETTINGS ============
Adafruit_USBD_HID usb_hid;
uint8_t const desc_hid_report[] = { TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)) };

// ============ INTERNAL STATE ============
bool lastStable[N];
bool debouncing[N];
uint32_t lastChangeMs[N];

// Facing right = opponent is on the right side (typical orientation).
// Toggle this if sides are switched during a match.
bool facingRight = true;

// ----------------- HID UTILITIES -----------------
static inline bool ready() { return usb_hid.ready(); }

static inline void sendKeys(const uint8_t* keys, uint8_t count) {
  if (!ready()) return;
  uint8_t kc[6] = {0};
  for (uint8_t i = 0; i < count && i < 6; i++) kc[i] = keys[i];
  usb_hid.keyboardReport(1, 0, kc); // mods=0
}

static inline void releaseAll() {
  if (!ready()) return;
  usb_hid.keyboardRelease(1);
}

static inline void hold(std::initializer_list<uint8_t> ks, uint16_t ms) {
  uint8_t tmp[6] = {0};
  uint8_t i = 0;
  for (auto k : ks) { if (i < 6) tmp[i++] = k; }
  sendKeys(tmp, i);
  delay(ms);
}

static inline void tap(uint8_t k, uint16_t ms) {
  hold({k}, ms);
  releaseAll();
  delay(GAP);
}

// ----------------- DIRECTION HELPERS (face-direction aware) -----------------
inline uint8_t FORWARD()  { return facingRight ? KEY_RIGHT : KEY_LEFT; }
inline uint8_t BACKWARD() { return facingRight ? KEY_LEFT  : KEY_RIGHT; }

// ↘ (down-forward) / ↙ (down-back)
inline void hold_df(uint16_t ms){ hold({KEY_DOWN, FORWARD()},  ms); }
inline void hold_db(uint16_t ms){ hold({KEY_DOWN, BACKWARD()}, ms); }

// ----------------- COMMANDS (Ryu) -----------------
// Hadouken: ↓ ↘ → + Punch
void cmd_hadouken(uint8_t punch = LP) {
  hold({KEY_DOWN}, STEP); releaseAll(); delay(GAP);
  hold_df(STEP);          releaseAll(); delay(GAP);
  hold({FORWARD()}, STEP/2);
  tap(punch, TAP);
  releaseAll();
}

// Shoryuken: → ↓ ↘ + Punch (initial → is short)
void cmd_shoryu(uint8_t punch = MP) {
  hold({FORWARD()}, STEP/2); releaseAll(); delay(GAP/2);
  hold({KEY_DOWN}, STEP/2);  releaseAll(); delay(GAP/2);
  hold_df(STEP);
  tap(punch, TAP);
  releaseAll();
}

// Tatsumaki Senpuu Kyaku: ↓ ↙ ← + Kick
void cmd_tatsu(uint8_t kick = LK) {
  hold({KEY_DOWN}, STEP); releaseAll(); delay(GAP);
  hold_db(STEP);          releaseAll(); delay(GAP);
  hold({BACKWARD()}, STEP/2);
  tap(kick, TAP);
  releaseAll();
}

// ============ SETUP / LOOP ============
void setup() {
  for (size_t i = 0; i < N; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    lastStable[i]   = digitalRead(SWITCH_PINS[i]); // HIGH = not pressed
    debouncing[i]   = false;
    lastChangeMs[i] = 0;
  }

  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setPollInterval(2);
  usb_hid.setBootProtocol(true);
  usb_hid.begin();
  while (!TinyUSBDevice.mounted()) { delay(10); }
}

void onPressed(size_t idx) {
  switch (idx) {
    case BTN_HADOU:  cmd_hadouken(LP); break; // choose LP/MP/HP if desired
    case BTN_SHORYU: cmd_shoryu(MP);   break;
    case BTN_TATSU:  cmd_tatsu(LK);    break;
    case BTN_TOGGLE_FACE:
      facingRight = !facingRight;      // Toggle facing direction
      break;
    default: break;
  }
}

void loop() {
  uint32_t now = millis();
  for (size_t i = 0; i < N; i++) {
    int raw = digitalRead(SWITCH_PINS[i]); // pressed = LOW
    if (raw != lastStable[i]) {
      if (!debouncing[i]) {
        debouncing[i] = true;
        lastChangeMs[i] = now;
      } else if (now - lastChangeMs[i] >= DEBOUNCE_MS) {
        debouncing[i] = false;
        lastStable[i] = raw;
        if (raw == LOW) onPressed(i);
      }
    } else {
      debouncing[i] = false;
    }
  }
}
