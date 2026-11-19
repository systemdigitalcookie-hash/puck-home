// ================= Wi-Fi networks: saved list (NVS, max 5), background scan, connect with fallback =================
// Runs on the net task. The UI only posts requests (wn_req_*) and reads results when the state says "done".
// WiFiManager's portal is only the fallback when no saved network can be reached at boot.
#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include "esp_wifi.h"

#define WN_MAX 5
#define WN_SCAN_MAX 20
struct WnNet { char ssid[33]; char pass[65]; };
struct WnScan { char ssid[33]; int8_t rssi; bool secure; };
static WnNet wn_saved[WN_MAX];
static int wn_nsaved = 0;
static WnScan wn_scan[WN_SCAN_MAX];
static int wn_nscan = 0;
static SemaphoreHandle_t wn_mx = NULL;
enum { WN_IDLE = 0, WN_BUSY, WN_DONE, WN_FAIL };
static volatile int wn_scan_state = WN_IDLE, wn_conn_state = WN_IDLE;
static volatile uint32_t wn_scan_gen = 0;          // bumps every finished scan
static volatile bool wn_req_scan = false, wn_req_conn = false, wn_req_auto = false;
static char wn_req_ssid[33], wn_req_pass[65];
static char wn_req_forget[33];
static volatile bool wn_req_forget_on = false;
static char wn_conn_msg[64] = "";
static volatile int wn_disc_reason = 0;
static bool wn_time_cfg = false;

static void wn_lock() { if (!wn_mx) wn_mx = xSemaphoreCreateMutex(); xSemaphoreTake(wn_mx, portMAX_DELAY); }
static void wn_unlock() { xSemaphoreGive(wn_mx); }

static void wn_load() {
  wn_lock(); Preferences p; wn_nsaved = 0;
  if (p.begin("wifi", true)) {
    int n = min((int)p.getInt("n", 0), (int)WN_MAX);
    for (int i = 0; i < n; i++) {
      char k[4]; snprintf(k, 4, "s%d", i); String s = p.getString(k, "");
      snprintf(k, 4, "p%d", i); String pw = p.getString(k, "");
      if (!s.length()) continue;
      snprintf(wn_saved[wn_nsaved].ssid, 33, "%s", s.c_str()); snprintf(wn_saved[wn_nsaved].pass, 65, "%s", pw.c_str()); wn_nsaved++;
    }
    p.end();
  }
  wn_unlock();
}
static void wn_save_locked() {
  Preferences p; if (!p.begin("wifi", false)) return;
  p.putInt("n", wn_nsaved);
  for (int i = 0; i < WN_MAX; i++) {
    char k[4]; snprintf(k, 4, "s%d", i);
    if (i < wn_nsaved) { p.putString(k, wn_saved[i].ssid); snprintf(k, 4, "p%d", i); p.putString(k, wn_saved[i].pass); }
    else { p.remove(k); snprintf(k, 4, "p%d", i); p.remove(k); }
  }
  p.end();
}
// most recently used first; the 6th network pushes out the oldest
static void wn_remember(const char *ssid, const char *pass) {
  if (!ssid || !ssid[0]) return;
  wn_lock();
  WnNet n; snprintf(n.ssid, 33, "%s", ssid); snprintf(n.pass, 65, "%s", pass ? pass : "");
  int at = -1; for (int i = 0; i < wn_nsaved; i++) if (!strcmp(wn_saved[i].ssid, ssid)) at = i;
  if (at < 0) at = wn_nsaved < WN_MAX ? wn_nsaved++ : WN_MAX - 1;
  for (int i = at; i > 0; i--) wn_saved[i] = wn_saved[i - 1];
  wn_saved[0] = n; wn_save_locked(); wn_unlock();
  Serial.printf("[wifi] saved \"%s\" (%d known)\n", ssid, wn_nsaved);
}
static bool wn_known(const char *ssid, char *pass_out) {
  bool f = false; wn_lock();
  for (int i = 0; i < wn_nsaved; i++) if (!strcmp(wn_saved[i].ssid, ssid)) { f = true; if (pass_out) strcpy(pass_out, wn_saved[i].pass); }
  wn_unlock(); return f;
}
static void wn_forget_now(const char *ssid) {
  wn_lock();
  int w = 0; for (int i = 0; i < wn_nsaved; i++) if (strcmp(wn_saved[i].ssid, ssid)) wn_saved[w++] = wn_saved[i];
  wn_nsaved = w; wn_save_locked(); wn_unlock();
  Serial.printf("[wifi] forgot \"%s\"\n", ssid);
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {   // drop it, also from the Wi-Fi driver's own memory
    WiFi.disconnect(false, true); delay(200);
    wn_req_auto = wn_nsaved > 0;
  }
}

static void wn_on_event(WiFiEvent_t ev, WiFiEventInfo_t info) {
  if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) wn_disc_reason = info.wifi_sta_disconnected.reason;
}
static const char *wn_reason_txt(int r) {
  switch (r) {
    case 15: case 202: case 204: case 2: return "Wrong password";
    case 201: return "Network not found";
    default: return "Couldn't connect";
  }
}
// blocking (net task only): connect to one network, true on success
static bool wn_try(const char *ssid, const char *pass, uint32_t ms) {
  WiFi.mode(WIFI_STA);
  if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(false, false); delay(150); }
  wn_disc_reason = 0;
  WiFi.begin(ssid, pass && pass[0] ? pass : NULL);
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (WiFi.status() == WL_CONNECTED) { Serial.printf("[wifi] connected \"%s\" %s %d dBm\n", ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI()); return true; }
    int r = wn_disc_reason;
    if ((r == 15 || r == 202 || r == 204 || r == 2) && millis() - t0 > 4000) break;   // auth failed: no point waiting
    if (r == 201 && millis() - t0 > 7000) break;
    delay(100);
  }
  Serial.printf("[wifi] \"%s\" failed, reason %d\n", ssid, (int)wn_disc_reason);
  WiFi.disconnect(false, false);
  return false;
}
static int wn_do_scan() {   // blocking, on the net task; dedupe by SSID (strongest), sort by RSSI
  int n = WiFi.scanNetworks(false, false);
  int k = 0;
  for (int i = 0; i < n && k < WN_SCAN_MAX; i++) {
    String s = WiFi.SSID(i); if (!s.length()) continue;   // hidden
    int dup = -1; for (int j = 0; j < k; j++) if (s == wn_scan[j].ssid) dup = j;
    int8_t r = WiFi.RSSI(i);
    if (dup >= 0) { if (r > wn_scan[dup].rssi) wn_scan[dup].rssi = r; continue; }
    snprintf(wn_scan[k].ssid, 33, "%s", s.c_str()); wn_scan[k].rssi = r; wn_scan[k].secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN; k++;
  }
  WiFi.scanDelete();
  for (int i = 1; i < k; i++) for (int j = i; j > 0 && wn_scan[j].rssi > wn_scan[j - 1].rssi; j--) { WnScan t = wn_scan[j]; wn_scan[j] = wn_scan[j - 1]; wn_scan[j - 1] = t; }
  wn_nscan = k;
  Serial.printf("[wifi] scan: %d networks\n", k);
  return n < 0 ? -1 : k;
}
// strongest saved network that is in range
static bool wn_auto_connect() {
  if (!wn_nsaved) return false;
  WiFi.mode(WIFI_STA);
  if (wn_do_scan() <= 0) return false;
  wn_scan_gen++;
  for (int i = 0; i < wn_nscan; i++) {
    char pw[65];
    if (wn_known(wn_scan[i].ssid, pw) && wn_try(wn_scan[i].ssid, pw, 12000)) { wn_remember(wn_scan[i].ssid, pw); return true; }
  }
  return false;
}
// import the network the Wi-Fi driver already remembers (e.g. set via the old WiFiManager portal)
static void wn_import_driver_creds() {
  WiFi.mode(WIFI_STA);
  wifi_config_t c; memset(&c, 0, sizeof c);
  if (esp_wifi_get_config(WIFI_IF_STA, &c) == ESP_OK && c.sta.ssid[0]) {
    char s[33] = {0}, p[65] = {0}; memcpy(s, c.sta.ssid, 32); memcpy(p, c.sta.password, 64);
    if (!wn_known(s, NULL)) wn_remember(s, p);
  }
}
static void wn_time_once() {
  if (wn_time_cfg) return;
  wn_time_cfg = true;
  configTime(0, 0, "pool.ntp.org", "time.google.com");   // cert validation needs correct time
  for (int i = 0; i < 40 && time(nullptr) < 1700000000; i++) delay(250);
}
// boot: saved networks first (strongest), then the WiFiManager portal as the fallback. True if online.
static bool wn_boot(const char *ap, char *status, size_t n) {
  WiFi.onEvent(wn_on_event);
  wn_load(); wn_import_driver_creds();
  snprintf(status, n, "Connecting Wi-Fi...");
  if (wn_auto_connect()) return true;
  WiFiManager wm; wm.setConfigPortalTimeout(180);
  snprintf(status, n, "WiFi: join AP %s", ap);
  if (wm.autoConnect(ap)) { wn_remember(WiFi.SSID().c_str(), WiFi.psk().c_str()); return true; }
  WiFi.mode(WIFI_STA);
  snprintf(status, n, "Not connected - Settings > Wi-Fi");
  return false;
}
// net task loop: serve UI requests. Returns true when the connection changed to online.
static bool wn_service() {
  static uint32_t last_retry = 0;
  bool online_now = false;
  if (wn_req_forget_on) { char s[33]; strcpy(s, wn_req_forget); wn_req_forget_on = false; wn_forget_now(s); }
  if (wn_req_scan) {
    wn_req_scan = false; wn_scan_state = WN_BUSY;
    wn_scan_state = wn_do_scan() >= 0 ? WN_DONE : WN_FAIL; wn_scan_gen++;
  }
  if (wn_req_conn) {
    char s[33], p[65]; strcpy(s, wn_req_ssid); strcpy(p, wn_req_pass); wn_req_conn = false;
    String ps = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String(""), pp = WiFi.status() == WL_CONNECTED ? WiFi.psk() : String("");
    if (wn_try(s, p, 15000)) { wn_remember(s, p); snprintf(wn_conn_msg, sizeof wn_conn_msg, "Connected to %s", s); wn_conn_state = WN_DONE; online_now = true; }
    else {
      const char *why = wn_reason_txt(wn_disc_reason);
      if (ps.length() && wn_try(ps.c_str(), pp.c_str(), 12000)) snprintf(wn_conn_msg, sizeof wn_conn_msg, "%s. Back on %s", why, ps.c_str());
      else snprintf(wn_conn_msg, sizeof wn_conn_msg, "%s", why);
      wn_conn_state = WN_FAIL;
    }
  }
  if (wn_req_auto) { wn_req_auto = false; online_now = wn_auto_connect(); }
  // offline with saved networks: retry every 2 minutes
  if (WiFi.status() != WL_CONNECTED && wn_nsaved && millis() - last_retry > 120000) { last_retry = millis(); online_now = wn_auto_connect(); }
  if (online_now) wn_time_once();
  return online_now;
}
