// Puck Home v-base: Wi-Fi setup portal + OTA from GitHub releases + a minimal clock screen.
// AI/beginners: read AGENTS.md before editing. Keep the OTA code (net_task) intact.
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

static char status_txt[64] = "Starting...";
static volatile bool want_check = false;
static lv_obj_t *lbl_clock, *lbl_date, *lbl_wifi, *lbl_ver, *lbl_stat;

// ================= OTA + Wi-Fi (KEEP INTACT) =================
static void btn_cb(lv_event_t *e) { want_check = true; }

static void ensure_tz() {
  const char *tz = getenv("TZ");
  if (!tz || strcmp(tz, TZ_MYT)) { setenv("TZ", TZ_MYT, 1); tzset(); }   // configTime(0,0) resets TZ; re-apply
}

static void check_update() {
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
  WiFiManager wm;                       // saved creds reused; else AP "Puck-Home-Setup" portal
  wm.setConfigPortalTimeout(180);
  snprintf(status_txt, sizeof status_txt, "Join Wi-Fi: " AP_NAME);
  if (!wm.autoConnect(AP_NAME)) { snprintf(status_txt, sizeof status_txt, "Wi-Fi setup timed out"); vTaskDelete(NULL); }
  configTime(0, 0, "pool.ntp.org", "time.google.com");   // TLS needs correct time
  for (int i = 0; i < 40 && time(nullptr) < 1700000000; i++) delay(250);
  snprintf(status_txt, sizeof status_txt, "Online");
  check_update();                                         // check on boot
  for (;;) {
    if (want_check) { want_check = false; check_update(); }   // button
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
// ================= end OTA =================

// ================= UI (edit / replace this part to build your own app) =================
static void build_ui() {
  ui_styles_init();
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0); lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  lbl_clock = ui_label(scr, &lv_font_montserrat_48, C_TEXT, 0, "--:--");
  lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 0, -80);
  lbl_date = ui_label(scr, &lv_font_montserrat_14, C_GREY, 0, "");
  lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, -38);
  ui_divider(scr, 200, 162);

  lbl_wifi = ui_label(scr, &lv_font_montserrat_14, C_TEXT, 220, "Wi-Fi: ...");
  lv_obj_align(lbl_wifi, LV_ALIGN_CENTER, 0, -4);
  lbl_stat = ui_label(scr, &lv_font_montserrat_14, C_GREY, 220, "");
  lv_obj_align(lbl_stat, LV_ALIGN_CENTER, 0, 20);

  lv_obj_t *l;
  lv_obj_t *b = ui_btn(scr, 150, 40, "Check update", &l);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 66);
  lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, NULL);

  lbl_ver = ui_label(scr, &lv_font_montserrat_14, C_GREY, 0, "v" FW_STR);
  lv_obj_align(lbl_ver, LV_ALIGN_CENTER, 0, 118);
}

void setup() {
  I2C_Init(); Backlight_Init(); LCD_Init(); Lvgl_Init();
  pinMode(9, OUTPUT); digitalWrite(9, LOW);   // speaker amp off
  ensure_tz();
  build_ui();
  xTaskCreate(net_task, "net", 12288, NULL, 1, NULL);
}

void loop() {
  static uint32_t last = 0; static char shown_stat[64];
  uint32_t now = millis();
  if (now - last >= 500) {
    last = now; ensure_tz();
    time_t t = time(nullptr);
    if (t > 1700000000) {
      struct tm tm; localtime_r(&t, &tm); char b[40];
      strftime(b, sizeof b, "%H:%M", &tm); lv_label_set_text(lbl_clock, b);
      strftime(b, sizeof b, "%a, %d %b %Y", &tm); lv_label_set_text(lbl_date, b);
    }
    char w[64];
    if (WiFi.status() == WL_CONNECTED) snprintf(w, sizeof w, "Wi-Fi: %s", WiFi.SSID().c_str());
    else snprintf(w, sizeof w, "Wi-Fi: not connected");
    lv_label_set_text(lbl_wifi, w);
    if (strcmp(shown_stat, status_txt)) { snprintf(shown_stat, sizeof shown_stat, "%s", status_txt); lv_label_set_text(lbl_stat, shown_stat); }
  }
  Lvgl_Loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}
