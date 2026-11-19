// Puck Home v2: Recorder (with recordings), Clock, Settings (incl. Wi-Fi picker and Storage). OTA from GitHub releases.
// AI/beginners: read AGENTS.md before editing. Keep the OTA code (check_update / net_task) intact.
#include "Display_ST77916.h"
#include "LVGL_Driver.h"
#include "I2C_Driver.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "wifi_net.h"
#include "recorder.h"
#include "ui_style.h"
#include "version.h"
#include "certs.h"

#define _S(x) #x
#define S(x) _S(x)
#define FW_STR S(FW_VERSION)
#define OTA_REPO "systemdigitalcookie-hash/puck-home"
#define OTA_BASE "https://github.com/" OTA_REPO "/releases/latest/download"
#define AP_NAME  "Puck-Home-Setup"
#define TZ_MYT   "MYT-8"

static char status_txt[96] = "Starting...";
static volatile bool want_check = false;

// ================= OTA + Wi-Fi (KEEP INTACT) =================
static void btn_cb(lv_event_t *e) { want_check = true; }

static void ensure_tz() {
  const char *tz = getenv("TZ");
  if (!tz || strcmp(tz, TZ_MYT)) { setenv("TZ", TZ_MYT, 1); tzset(); }   // configTime(0,0) resets TZ; re-apply
}

static void check_update() {
  if (rec.state != REC_IDLE) { snprintf(status_txt, sizeof status_txt, "Stop recording first"); return; }
  snprintf(status_txt, sizeof status_txt, "Checking...");
  WiFiClientSecure c; c.setCACert(GH_ROOT_CAS);   // TLS validated against pinned roots (certs.h)
  HTTPClient h; h.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); h.begin(c, String(OTA_BASE) + "/version.txt");
  int code = h.GET(); int remote = code == 200 ? h.getString().toInt() : -1; h.end();
  if (remote > FW_VERSION) {
    snprintf(status_txt, sizeof status_txt, "Updating to v%d...", remote);
    httpUpdate.rebootOnUpdate(true);
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.update(c, String(OTA_BASE) + "/firmware.bin");
    snprintf(status_txt, sizeof status_txt, "Update failed");
  } else if (code == 200) snprintf(status_txt, sizeof status_txt, "Up to date");
  else snprintf(status_txt, sizeof status_txt, "Check failed (%d)", code);
}

static void net_task(void *) {
  // saved networks first (strongest in range), WiFiManager portal "Puck-Home-Setup" only as the fallback
  bool online = wn_boot(AP_NAME, status_txt, sizeof status_txt);
  if (online) { wn_time_once(); snprintf(status_txt, sizeof status_txt, "Online"); check_update(); }   // check on boot
  for (;;) {
    if (wn_service()) { snprintf(status_txt, sizeof status_txt, "Online"); }            // Wi-Fi picker requests
    if (want_check) { want_check = false; check_update(); }                              // button
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
// ================= end OTA =================

// ================= RTC (PCF85063 @0x51) keeps time without Wi-Fi =================
#define RTC_ADDR 0x51
#define EPOCH_2026 1767225600UL
static uint8_t bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static int unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static bool rtc_read_epoch(time_t *out) {
  Wire.beginTransmission(RTC_ADDR); Wire.write(0x04);
  if (Wire.endTransmission(true)) return false;
  if (Wire.requestFrom((uint8_t)RTC_ADDR, (uint8_t)7) != 7) return false;
  uint8_t r[7]; for (int i = 0; i < 7; i++) r[i] = Wire.read();
  if (r[0] & 0x80) return false;                    // oscillator-stop flag: time not trustworthy
  struct tm t = {};
  t.tm_sec = unbcd(r[0] & 0x7F); t.tm_min = unbcd(r[1] & 0x7F); t.tm_hour = unbcd(r[2] & 0x3F);
  t.tm_mday = unbcd(r[3] & 0x3F); t.tm_mon = unbcd(r[5] & 0x1F) - 1; t.tm_year = 100 + unbcd(r[6]);
  ensure_tz(); t.tm_isdst = 0;
  time_t e = mktime(&t);
  if (e < (time_t)EPOCH_2026) return false;
  *out = e; return true;
}
static void rtc_write_epoch(time_t e) {
  ensure_tz();
  struct tm t; localtime_r(&e, &t);
  Wire.beginTransmission(RTC_ADDR); Wire.write(0x04);
  Wire.write(bcd(t.tm_sec)); Wire.write(bcd(t.tm_min)); Wire.write(bcd(t.tm_hour)); Wire.write(bcd(t.tm_mday));
  Wire.write(t.tm_wday & 7); Wire.write(bcd(t.tm_mon + 1)); Wire.write(bcd(t.tm_year % 100));
  Wire.endTransmission(true);
}

// ================= battery gauge (BQ27220 @0x55), used by Settings > Battery =================
static bool bq_word(uint8_t cmd, uint16_t *v) {
  Wire.beginTransmission(0x55); Wire.write(cmd);
  if (Wire.endTransmission(true)) return false;
  if (Wire.requestFrom((uint8_t)0x55, (uint8_t)2) != 2) return false;
  uint8_t lo = Wire.read(), hi = Wire.read(); *v = lo | (hi << 8); return true;
}
static void fmt_dur(char *out, size_t n, uint32_t s) {
  if (s >= 3600) snprintf(out, n, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
  else snprintf(out, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

// ================= UI (edit / extend this part to build your own app) =================
// Pages (swipe left/right): 0 Recorder, 1 Clock (start page), 2 Settings
static lv_obj_t *tv, *dots[3], *lbl_clock, *lbl_date, *lbl_wifi, *lbl_stat;
#define NPAGES 3
#define PG_CLOCK 1
#define PG_SETTINGS 2
static bool lock_rec = false;   // no page swipes while recording / saving
static void nav_apply() { if (lock_rec) lv_obj_clear_flag(tv, LV_OBJ_FLAG_SCROLLABLE); else lv_obj_add_flag(tv, LV_OBJ_FLAG_SCROLLABLE); }
#include "recorder_ui.h"
#include "settings_ui.h"

static void build_clock(lv_obj_t *t) {   // Li Li's clock page: big soft clock, bunny ears, hearts and a bow
  theme_bunny_ears(t, 22);
  lbl_clock = ui_label(t, T_FONT_XXL, C_TEXT, 0, "--:--");
  lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 0, -48);
  lbl_date = ui_label(t, T_FONT_M, C_GREY, 0, "");
  lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, -6);
  theme_heart(t, -100, 168, true); theme_heart(t, 100, 168, true);   // flank the date
  lbl_wifi = ui_label(t, T_FONT_M, C_TEXT, 220, "Wi-Fi: ...");
  lv_label_set_long_mode(lbl_wifi, LV_LABEL_LONG_DOT);
  lv_obj_align(lbl_wifi, LV_ALIGN_CENTER, 0, 26);
  lbl_stat = ui_label(t, T_FONT_M, C_GREY, 220, "");
  lv_label_set_long_mode(lbl_stat, LV_LABEL_LONG_DOT);
  lv_obj_align(lbl_stat, LV_ALIGN_CENTER, 0, 48);
  lv_obj_t *b = ui_btn(t, 150, 40, "Check update", NULL);
  lv_obj_set_style_radius(b, T_RADIUS_PILL, 0);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 92);
  lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, NULL);
  theme_flower(t, LV_ALIGN_CENTER, -98, 96, true); theme_flower(t, LV_ALIGN_CENTER, 98, 96, true);
  lv_obj_t *v = ui_label(t, T_FONT_S, C_GREY, 0, "v" FW_STR);
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 138);
}

static void build_ui() {
  ui_styles_init();
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0); lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, lv_color_hex(C_TEXT), 0);
  lv_obj_set_style_text_font(scr, &lv_font_montserrat_14, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  tv = lv_tileview_create(scr);
  lv_obj_set_size(tv, 360, 360); lv_obj_set_pos(tv, 0, 0);
  lv_obj_set_style_bg_color(tv, lv_color_hex(C_BG), 0); lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);
  lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *t0 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
  lv_obj_t *t1 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
  lv_obj_t *t2 = lv_tileview_add_tile(tv, 2, 0, LV_DIR_LEFT);

  for (int i = 0; i < NPAGES; i++) {   // page dots (created first: sub-screens hide them)
    dots[i] = lv_obj_create(lv_layer_top()); lv_obj_remove_style_all(dots[i]);
    lv_obj_set_size(dots[i], 6, 6); lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
    lv_obj_clear_flag(dots[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(dots[i], LV_ALIGN_TOP_MID, (i - 1) * 14, 332);
  }
  theme_ring();         // dotted pink border ring (theme.h)
  ru_build(t0);         // recorder + recent recordings (recorder_ui.h)
  build_clock(t1);
  settings_build(t2);   // Sound, Microphone, Display, Storage, Wi-Fi, Battery, About (settings_ui.h)
  lv_obj_set_tile_id(tv, PG_CLOCK, 0, LV_ANIM_OFF);
}

static void clock_tick() {   // 1 Hz
  ensure_tz();
  time_t t = time(nullptr);
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && t > (time_t)EPOCH_2026) rtc_write_epoch(t);
  if (t > (time_t)EPOCH_2026) {
    struct tm tm; localtime_r(&t, &tm); char b[40];
    strftime(b, sizeof b, "%H:%M", &tm); lv_label_set_text(lbl_clock, b);
    strftime(b, sizeof b, "%a, %d %b %Y", &tm); lv_label_set_text(lbl_date, b);
  }
  char w[64];
  if (WiFi.status() == WL_CONNECTED) snprintf(w, sizeof w, "Wi-Fi: %s", WiFi.SSID().c_str());
  else snprintf(w, sizeof w, "Wi-Fi: not connected");
  lv_label_set_text(lbl_wifi, w);
}

void setup() {
  Serial.begin(115200);
  I2C_Init(); Backlight_Init(); LCD_Init(); Lvgl_Init();
  pinMode(9, OUTPUT); digitalWrite(9, LOW);   // speaker amp off
  ensure_tz();
  time_t e; if (rtc_read_epoch(&e)) { struct timeval tvv = { e, 0 }; settimeofday(&tvv, NULL); }
  build_ui();
  settings_load();   // volume, mic gain, brightness from NVS (before rec_init so the gain applies)
  rec_init();
  xTaskCreate(net_task, "net", 12288, NULL, 1, NULL);
}

void loop() {
  static char shown_stat[96], shown_ota[96];
  static int last_page = -1;
  static uint32_t last_fast = 0, last_ui = 0, last_slow = 0;
  uint32_t now = millis();
  int sx = lv_obj_get_scroll_x(tv);
  if (now - last_fast >= 33) { last_fast = now; ru_tick(now); sub_tick(); }   // ~30 Hz
  if (now - last_ui >= 100) {
    last_ui = now;
    int page = (sx + 180) / 360; if (page < 0) page = 0; if (page > NPAGES - 1) page = NPAGES - 1;
    for (int i = 0; i < NPAGES; i++) lv_obj_set_style_bg_color(dots[i], lv_color_hex(i == page ? C_HOT : C_LINE), 0);
    if (page != last_page) { last_page = page; last_slow = 0; if (page == 0) list_dirty = true; if (page != 0 && rd) rd_close(); }
    if (strcmp(shown_stat, status_txt)) { snprintf(shown_stat, sizeof shown_stat, "%s", status_txt); lv_label_set_text(lbl_stat, shown_stat); }
    if (!ota_lbl) shown_ota[0] = 0;
    else if (strcmp(shown_ota, status_txt)) { snprintf(shown_ota, sizeof shown_ota, "%s", status_txt); lv_label_set_text(ota_lbl, shown_ota); }
  }
  if (now - last_slow >= 1000) {
    last_slow = now;
    clock_tick();
    if (last_page == PG_SETTINGS) settings_summaries();
    sub_refresh();
  }
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
