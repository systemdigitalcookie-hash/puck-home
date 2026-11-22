// Screen button (BOOT key, GPIO0, active low, 10k pull-up on the board).
//   short press  -> screen off / on (backlight off, panel display-off + sleep, LVGL and touch paused)
//   long press   -> (screen on) close whatever is open and go to the home page: btn_home(), defined by the sketch
//                   (screen off) just turns the screen back on
// GPIO0 is only ever read here (never driven), so holding BOOT while plugging in / resetting still enters USB download mode.
// Recording, playback, Wi-Fi and OTA keep running while the screen is off; they run outside LVGL.
// The sketch defines btn_home() and btn_before_off() (e.g. finish a hold-to-talk). Needs g_bright (settings_ui.h).
// setup(): sb_init(). loop(): sb_poll() first, and skip Lvgl_Loop() while sb_off.
#pragma once
#define SB_PIN       0
#define SB_DEBOUNCE  30     // ms the level must be stable
#define SB_LONG_MS   800    // hold this long for "home"
static bool sb_off = false;                 // screen is off: loop() skips Lvgl_Loop()
static bool sb_level = false, sb_raw = false, sb_armed = false, sb_long_done = false;
static uint32_t sb_raw_t = 0, sb_down_t = 0;
static void btn_home();
static void btn_before_off();

static void sb_screen(bool on) {
  if (on == !sb_off) return;
  if (!on) {
    btn_before_off();
    Set_Backlight(0);
    LCD_Power(false);
    sb_off = true;
    Serial.println("[button] screen off");
  } else {
    LCD_Power(true);
    sb_off = false;
    lv_obj_invalidate(lv_scr_act());        // repaint everything before the light comes back
    lv_refr_now(NULL);
    Set_Backlight(g_bright);
    Serial.println("[button] screen on");
  }
}
static void sb_init() { pinMode(SB_PIN, INPUT_PULLUP); }
static void sb_poll() {
  uint32_t now = millis();
  bool raw = digitalRead(SB_PIN) == LOW;
  if (raw != sb_raw) { sb_raw = raw; sb_raw_t = now; }
  if (now - sb_raw_t < SB_DEBOUNCE) return;
  if (!sb_armed) { if (!raw) { sb_armed = true; sb_level = false; } return; }   // held at boot: wait for a release first
  if (raw != sb_level) {
    sb_level = raw;
    if (raw) { sb_down_t = now; sb_long_done = false; }
    else if (!sb_long_done) sb_screen(sb_off);                                 // short press: toggle
  } else if (raw && !sb_long_done && now - sb_down_t >= SB_LONG_MS) {
    sb_long_done = true;
    if (sb_off) sb_screen(true); else { Serial.println("[button] home"); btn_home(); }
  }
}
