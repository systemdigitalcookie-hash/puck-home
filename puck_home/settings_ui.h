// ================= Settings page (v8): Android-style list + sub-screens, Notion-minimal palette =================
// Included from the .ino after the helpers it uses (bq_word, fmt_dur, want_check, status_txt, tv, dots).
#pragma once
#include <Preferences.h>
#include "wifi_ui.h"

static int g_bright = 70;

static void settings_load() {   // call before rec_init() so the mic gain applies from boot
  Preferences p;
  if (p.begin("puck", true)) {
    g_spk_volume = p.getInt("vol2", 90); g_mic_gain = p.getInt("gain_db", 42); g_bright = p.getInt("bright", 70);
    p.end();
  }
  g_spk_volume = constrain((int)g_spk_volume, 0, 100); g_mic_gain = constrain((int)g_mic_gain, 0, 60); g_bright = constrain(g_bright, 10, 100);
  Set_Backlight(g_bright);
}
static void settings_save_int(const char *k, int v) { Preferences p; if (p.begin("puck", false)) { p.putInt(k, v); p.end(); } }

// ---------- list ----------
enum { SET_SOUND = 0, SET_MIC, SET_DISPLAY, SET_STORAGE, SET_WIFI, SET_BATTERY, SET_ABOUT, SET_N };
static const char *SET_TITLE[SET_N] = {"Sound", "Microphone", "Display", "Storage", "Wi-Fi", "Battery", "About"};
static const char *SET_ICON[SET_N] = {LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_AUDIO, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_SD_CARD, LV_SYMBOL_WIFI, LV_SYMBOL_BATTERY_FULL, LV_SYMBOL_SETTINGS};
static lv_obj_t *set_sum[SET_N];
static lv_obj_t *sub = NULL;          // open sub-screen (NULL when closed)
static int sub_kind = -1;
static lv_obj_t *sub_val[6], *sub_lbl, *sub_slider, *sub_bar, *sub_hint;
static lv_obj_t *ota_lbl = NULL;      // About: OTA status line (NULL when About is closed)
static void sub_open(int kind);

static void set_row_cb(lv_event_t *e) { sub_open((int)(intptr_t)lv_event_get_user_data(e)); }

static void settings_build(lv_obj_t *t) {
  ui_title(t, "Settings");
  ui_divider(t, 220, 62);
  lv_obj_t *list = lv_obj_create(t); lv_obj_remove_style_all(list);
  lv_obj_set_size(list, 250, 252); lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 64);   // inset so the round corners don't clip rows
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER); lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_bottom(list, 40, 0);   // last rows can scroll up into the wide middle of the circle
  for (int i = 0; i < SET_N; i++) {
    lv_obj_t *r = lv_obj_create(list); lv_obj_remove_style_all(r);
    lv_obj_set_size(r, 250, 52);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(r, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(C_PRESS), LV_STATE_PRESSED);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0); lv_obj_set_style_border_width(r, i < SET_N - 1 ? 1 : 0, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(C_DIV), 0);
    lv_obj_add_event_cb(r, set_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *ic = ui_label(r, &lv_font_montserrat_16, C_GREY, 0, SET_ICON[i]);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t *ti = ui_label(r, &lv_font_montserrat_14, C_TEXT, 0, SET_TITLE[i]);
    lv_obj_set_style_text_align(ti, LV_TEXT_ALIGN_LEFT, 0); lv_obj_align(ti, LV_ALIGN_TOP_LEFT, 48, 8);
    set_sum[i] = ui_label(r, &lv_font_montserrat_12, C_GREY, 180, "");
    lv_label_set_long_mode(set_sum[i], LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(set_sum[i], LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(set_sum[i], LV_ALIGN_TOP_LEFT, 48, 28);
    lv_obj_t *ch = ui_label(r, &lv_font_montserrat_12, C_LINE, 0, LV_SYMBOL_RIGHT);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -10, 0);
  }
}

static void fmt_gb(char *o, size_t n, uint64_t b) {
  if (b >= 1000000000ULL) snprintf(o, n, "%.1f GB", b / 1e9); else snprintf(o, n, "%.0f MB", b / 1e6);
}

static void bat_info(int *soc, int *mv, int *ma, bool *present) {
  uint16_t st = 0, v = 0, s = 0, c = 0; *present = false; *soc = *mv = *ma = 0;
  if (!bq_word(0x0A, &st) || !bq_word(0x08, &v)) return;
  *mv = v; if (!(st & (1u << 3)) || v < 2500) return;
  *present = true; bq_word(0x2C, &s); bq_word(0x0C, &c);
  *soc = s; *ma = c < 0x8000 ? c : (int)c - 65536;
}

static void settings_summaries() {   // 1 Hz while the Settings page is visible
  char b[64];
  snprintf(b, sizeof b, "Volume %d", (int)g_spk_volume); ui_set_text(set_sum[SET_SOUND], b);
  snprintf(b, sizeof b, "Gain %d dB", (int)g_mic_gain); ui_set_text(set_sum[SET_MIC], b);
  snprintf(b, sizeof b, "Brightness %d%%", g_bright); ui_set_text(set_sum[SET_DISPLAY], b);
  char gb[16]; fmt_gb(gb, sizeof gb, rec_free_bytes());
  snprintf(b, sizeof b, "%s %s free", rec.storage[0] ? rec.storage : "-", gb); ui_set_text(set_sum[SET_STORAGE], b);
  if (WiFi.status() == WL_CONNECTED) snprintf(b, sizeof b, "%s", WiFi.SSID().c_str()); else snprintf(b, sizeof b, "Not connected");
  ui_set_text(set_sum[SET_WIFI], b);
  int soc, mv, ma; bool pr; bat_info(&soc, &mv, &ma, &pr);
  if (pr) snprintf(b, sizeof b, "%d%%%s", soc, ma > 20 ? ", charging" : ""); else snprintf(b, sizeof b, "No battery (USB power)");
  ui_set_text(set_sum[SET_BATTERY], b);
  snprintf(b, sizeof b, "Firmware v%d", FW_VERSION); ui_set_text(set_sum[SET_ABOUT], b);
}

// ---------- sub-screens ----------
static int16_t *beep_buf = NULL, *mt_buf = NULL;
#define MT_SAMPLES (16000 * 3)
static int mt_state = 0;   // mic test: 0 idle, 1 recording, 2 playing
static uint32_t mt_t0 = 0;

static void sub_close() {
  if (!sub) return;
  if (mt_state == 1) talk_cap_stop();
  if (mt_state == 2 || talk_playing) talk_play_stop();
  mt_state = 0;
  if (sub_kind == SET_WIFI) wf_close();
  lv_obj_del_async(sub); sub = NULL; sub_kind = -1; ota_lbl = NULL; sub_slider = sub_bar = sub_hint = sub_lbl = NULL;
  for (int i = 0; i < NPAGES; i++) lv_obj_clear_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
}
static void sub_back_cb(lv_event_t *e) { sub_close(); }
static void sub_gesture_cb(lv_event_t *e) {
  lv_obj_t *act = lv_indev_get_obj_act();
  if (act && lv_obj_check_type(act, &lv_slider_class)) return;   // dragging a slider is not "back"
  if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) sub_close();
}

static void kv_row(int i, const char *key) {
  int y = 76 + i * 34;
  lv_obj_t *k = ui_label(sub, &lv_font_montserrat_14, C_GREY, 0, key);
  lv_obj_align(k, LV_ALIGN_TOP_MID, -60, y);
  sub_val[i] = ui_label(sub, &lv_font_montserrat_14, C_TEXT, 140, "");
  lv_label_set_long_mode(sub_val[i], LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(sub_val[i], LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(sub_val[i], LV_ALIGN_TOP_MID, 48, y);
  ui_divider(sub, 236, y + 26);
}

static lv_obj_t *mk_slider(int y, int lo, int hi, int v) {
  lv_obj_t *s = lv_slider_create(sub);
  lv_obj_set_size(s, 210, 6); lv_obj_align(s, LV_ALIGN_TOP_MID, 0, y);
  lv_slider_set_range(s, lo, hi); lv_slider_set_value(s, v, LV_ANIM_OFF);
  theme_bar(s, 8);   // pink track + fill
  lv_obj_set_style_bg_color(s, lv_color_hex(C_CARD), LV_PART_KNOB); lv_obj_set_style_radius(s, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_border_color(s, lv_color_hex(C_HOT), LV_PART_KNOB); lv_obj_set_style_border_width(s, 3, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(s, 0, LV_PART_KNOB); lv_obj_set_style_pad_all(s, 8, LV_PART_KNOB);
  lv_obj_set_style_outline_width(s, 0, LV_STATE_FOCUSED); lv_obj_set_style_outline_width(s, 0, LV_STATE_FOCUS_KEY);
  lv_obj_set_ext_click_area(s, 16);
  return s;
}

static void vol_cb(lv_event_t *e) {
  lv_obj_t *s = lv_event_get_target(e);
  g_spk_volume = lv_slider_get_value(s);
  char b[24]; snprintf(b, sizeof b, "%d", (int)g_spk_volume); lv_label_set_text(sub_lbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) settings_save_int("vol2", g_spk_volume);
}
static void gain_cb(lv_event_t *e) {
  lv_obj_t *s = lv_event_get_target(e);
  g_mic_gain = lv_slider_get_value(s) * 3;
  char b[24]; snprintf(b, sizeof b, "%d dB", (int)g_mic_gain); lv_label_set_text(sub_lbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) settings_save_int("gain_db", g_mic_gain);
}
static void bright_cb(lv_event_t *e) {
  lv_obj_t *s = lv_event_get_target(e);
  g_bright = lv_slider_get_value(s); Set_Backlight(g_bright);
  char b[24]; snprintf(b, sizeof b, "%d%%", g_bright); lv_label_set_text(sub_lbl, b);
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) settings_save_int("bright", g_bright);
}
static bool audio_free() {
  bool f = rec.state == REC_IDLE && !rec_playing && !talk_playing;
  return f;
}

static void test_spk_cb(lv_event_t *e) {
  if (!beep_buf) beep_buf = (int16_t *)heap_caps_malloc(16000 * 2, MALLOC_CAP_SPIRAM);
  if (!beep_buf || !audio_free()) { lv_label_set_text(sub_hint, "Audio busy"); return; }
  // two short tones (660 Hz, 880 Hz) with soft edges
  for (int i = 0; i < 16000; i++) {
    float f = i < 8000 ? 660.f : 880.f; int k = i % 8000;
    float env = k < 400 ? k / 400.f : (k > 6800 ? max(0.f, (7600 - k) / 800.f) : 1.f);
    beep_buf[i] = (int16_t)(9000 * env * sinf(2 * PI * f * i / 16000.f));
  }
  if (talk_play_start(beep_buf, 16000)) lv_label_set_text(sub_hint, "Playing test tone");
  else lv_label_set_text(sub_hint, "Speaker failed");
}
static void mic_test_cb(lv_event_t *e) {
  if (mt_state) return;
  if (!mt_buf) mt_buf = (int16_t *)heap_caps_malloc(MT_SAMPLES * 2, MALLOC_CAP_SPIRAM);
  if (!mt_buf || !audio_free()) { lv_label_set_text(sub_hint, "Audio busy"); return; }
  if (!talk_cap_start(mt_buf, MT_SAMPLES)) { lv_label_set_text(sub_hint, "Mic start failed"); return; }
  mt_state = 1; mt_t0 = millis(); lv_label_set_text(sub_hint, "Speak now (3 s)...");
}
static void sub_refresh() {   // 1 Hz live values
  if (!sub) return;
  char b[64];
  if (sub_kind == SET_STORAGE) {
    char f[16], t[16]; fmt_gb(f, sizeof f, rec_free_bytes()); fmt_gb(t, sizeof t, rec_total_bytes());
    ui_set_text(sub_val[0], rec.sd[0] ? rec.sd : (!strcmp(rec.storage, "SD") ? "SD card" : "Internal flash"));
    snprintf(b, sizeof b, "%s of %s", f, t); ui_set_text(sub_val[1], b);
    fmt_dur(f, sizeof f, rec.free_secs); ui_set_text(sub_val[2], f);
    snprintf(b, sizeof b, "%d", rec.count); ui_set_text(sub_val[3], b);
  } else if (sub_kind == SET_WIFI) {
    wf_status();
  } else if (sub_kind == SET_BATTERY) {
    int soc, mv, ma; bool pr; bat_info(&soc, &mv, &ma, &pr);
    if (pr) {
      snprintf(b, sizeof b, "%d%%", soc); ui_set_text(sub_val[0], b);
      snprintf(b, sizeof b, "%.2f V", mv / 1000.f); ui_set_text(sub_val[1], b);
      snprintf(b, sizeof b, "%+d mA", ma); ui_set_text(sub_val[2], b);
      ui_set_text(sub_val[3], ma > 20 ? "Charging" : (ma < -20 ? "On battery" : "Full / idle"));
    } else { ui_set_text(sub_val[0], "None"); ui_set_text(sub_val[1], "-"); ui_set_text(sub_val[2], "-"); ui_set_text(sub_val[3], "USB power"); }
  }
}

// ~30 Hz while a sub-screen is open: mic test state machine + live meter
static void sub_tick() {
  if (!sub) return;
  if (sub_kind == SET_WIFI) wf_tick();
  if (sub_bar) lv_bar_set_value(sub_bar, talk_level, LV_ANIM_OFF);
  if (sub_kind == SET_MIC) {
    if (mt_state == 1 && (talk_cap_n >= MT_SAMPLES || millis() - mt_t0 > 3200)) {
      uint32_t n = talk_cap_stop();
      if (n > 1600 && talk_play_start(mt_buf, n)) { mt_state = 2; lv_label_set_text(sub_hint, "Playing back"); }
      else { mt_state = 0; lv_label_set_text(sub_hint, "Playback failed"); }
    } else if (mt_state == 2 && !talk_playing) { mt_state = 0; lv_label_set_text(sub_hint, ""); }
  }
  if (sub_kind == SET_SOUND && !talk_playing && sub_hint && !strcmp(lv_label_get_text(sub_hint), "Playing test tone")) lv_label_set_text(sub_hint, "");
}

static void sub_open(int kind) {
  if (sub) return;
  sub_kind = kind;
  sub = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(sub);
  lv_obj_set_size(sub, 360, 360); lv_obj_set_pos(sub, 0, 0);
  lv_obj_set_style_bg_opa(sub, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(sub, lv_color_hex(C_BG), 0);
  lv_obj_clear_flag(sub, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sub, sub_gesture_cb, LV_EVENT_GESTURE, NULL);
  for (int i = 0; i < NPAGES; i++) lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  // header: back arrow + title
  lv_obj_t *bk = ui_btn(sub, 36, 36, LV_SYMBOL_LEFT, NULL);
  lv_obj_set_style_radius(bk, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(bk, 0, 0);
  lv_obj_align(bk, LV_ALIGN_TOP_MID, -92, 22); lv_obj_set_ext_click_area(bk, 10);
  lv_obj_add_event_cb(bk, sub_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ti = ui_title(sub, SET_TITLE[kind]);
  lv_obj_align(ti, LV_ALIGN_TOP_MID, 0, 28);
  ui_divider(sub, 220, 62);
  char b[48];
  switch (kind) {
    case SET_SOUND:
      ui_label(sub, &lv_font_montserrat_14, C_GREY, 0, "Volume"); lv_obj_align(lv_obj_get_child(sub, -1), LV_ALIGN_TOP_MID, -70, 92);
      snprintf(b, sizeof b, "%d", (int)g_spk_volume);
      sub_lbl = ui_label(sub, &lv_font_montserrat_14, C_TEXT, 0, b); lv_obj_align(sub_lbl, LV_ALIGN_TOP_MID, 80, 92);
      sub_slider = mk_slider(128, 0, 100, g_spk_volume);
      lv_obj_add_event_cb(sub_slider, vol_cb, LV_EVENT_VALUE_CHANGED, NULL); lv_obj_add_event_cb(sub_slider, vol_cb, LV_EVENT_RELEASED, NULL);
      { lv_obj_t *t = ui_btn(sub, 150, 36, "Test speaker", NULL); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 176); lv_obj_add_event_cb(t, test_spk_cb, LV_EVENT_CLICKED, NULL); }
      sub_hint = ui_label(sub, &lv_font_montserrat_12, C_GREY, 220, ""); lv_obj_align(sub_hint, LV_ALIGN_TOP_MID, 0, 226);
      break;
    case SET_MIC:
      ui_label(sub, &lv_font_montserrat_14, C_GREY, 0, "Gain"); lv_obj_align(lv_obj_get_child(sub, -1), LV_ALIGN_TOP_MID, -76, 92);
      snprintf(b, sizeof b, "%d dB", (int)g_mic_gain);
      sub_lbl = ui_label(sub, &lv_font_montserrat_14, C_TEXT, 0, b); lv_obj_align(sub_lbl, LV_ALIGN_TOP_MID, 74, 92);
      sub_slider = mk_slider(128, 0, 20, g_mic_gain / 3);   // 0..60 dB in 3 dB steps
      lv_obj_add_event_cb(sub_slider, gain_cb, LV_EVENT_VALUE_CHANGED, NULL); lv_obj_add_event_cb(sub_slider, gain_cb, LV_EVENT_RELEASED, NULL);
      ui_label(sub, &lv_font_montserrat_12, C_GREY, 0, "Level"); lv_obj_align(lv_obj_get_child(sub, -1), LV_ALIGN_TOP_MID, 0, 158);
      sub_bar = lv_bar_create(sub); lv_obj_set_size(sub_bar, 210, 8); lv_obj_align(sub_bar, LV_ALIGN_TOP_MID, 0, 178);
      lv_bar_set_range(sub_bar, 0, 100);
      theme_bar(sub_bar, 8);
      { lv_obj_t *t = ui_btn(sub, 150, 36, "Mic test", NULL); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 200); lv_obj_add_event_cb(t, mic_test_cb, LV_EVENT_CLICKED, NULL); }
      sub_hint = ui_label(sub, &lv_font_montserrat_12, C_GREY, 220, "Records 3 s, then plays it back");
      lv_obj_align(sub_hint, LV_ALIGN_TOP_MID, 0, 248);
      break;
    case SET_DISPLAY:
      ui_label(sub, &lv_font_montserrat_14, C_GREY, 0, "Brightness"); lv_obj_align(lv_obj_get_child(sub, -1), LV_ALIGN_TOP_MID, -60, 92);
      snprintf(b, sizeof b, "%d%%", g_bright);
      sub_lbl = ui_label(sub, &lv_font_montserrat_14, C_TEXT, 0, b); lv_obj_align(sub_lbl, LV_ALIGN_TOP_MID, 80, 92);
      sub_slider = mk_slider(128, 10, 100, g_bright);
      lv_obj_add_event_cb(sub_slider, bright_cb, LV_EVENT_VALUE_CHANGED, NULL); lv_obj_add_event_cb(sub_slider, bright_cb, LV_EVENT_RELEASED, NULL);
      break;
    case SET_STORAGE:
      kv_row(0, "Using"); kv_row(1, "Free"); kv_row(2, "Rec. time"); kv_row(3, "Recordings");
      break;
    case SET_WIFI:
      wf_build(sub);
      break;
    case SET_BATTERY:
      kv_row(0, "Level"); kv_row(1, "Voltage"); kv_row(2, "Current"); kv_row(3, "State");
      break;
    case SET_ABOUT:
      kv_row(0, "Firmware"); kv_row(1, "Built"); kv_row(2, "Device");
      snprintf(b, sizeof b, "v%d", FW_VERSION); lv_label_set_text(sub_val[0], b);
      lv_label_set_text(sub_val[1], __DATE__);
      { String m = WiFi.macAddress(); m.replace(":", ""); lv_label_set_text(sub_val[2], m.c_str()); }
      { lv_obj_t *t = ui_btn(sub, 150, 36, "Check update", NULL); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 186); lv_obj_add_event_cb(t, btn_cb, LV_EVENT_CLICKED, NULL); }
      ota_lbl = ui_label(sub, &lv_font_montserrat_12, C_GREY, 210, status_txt);
      lv_label_set_long_mode(ota_lbl, LV_LABEL_LONG_DOT); lv_obj_align(ota_lbl, LV_ALIGN_TOP_MID, 0, 232);
      break;
  }
  sub_refresh();
}
