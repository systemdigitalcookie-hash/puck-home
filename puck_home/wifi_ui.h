// ================= Settings > Wi-Fi: status, background scan list, password keyboard, saved networks =================
// UI side only (LVGL task). All Wi-Fi work happens on the net task via wn_req_* in wifi_net.h.
#pragma once
static lv_obj_t *wf_ssid, *wf_info, *wf_list, *wf_scan_btn;
static lv_obj_t *wk = NULL, *wk_ta, *wk_kb, *wk_eye_lbl, *wk_form, *wk_busy, *wk_res_icon, *wk_res, *wk_ok, *wk_spin;
static uint32_t wf_seen_gen = 0, wf_rebuild_at = 0;
static bool wf_open = false;
static char wk_ssid[33];
static WnScan wf_rows[WN_SCAN_MAX];
static int wf_nrows = 0;

static void wf_bars(lv_obj_t *p, int rssi, int x) {
  int lvl = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
  for (int i = 0; i < 4; i++) {
    lv_obj_t *b = lv_obj_create(p); lv_obj_remove_style_all(b);
    int h = 4 + i * 3; lv_obj_set_size(b, 3, h); lv_obj_align(b, LV_ALIGN_LEFT_MID, x + i * 5, 6 - h / 2);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(b, lv_color_hex(i < lvl ? C_TEXT : C_DIV), 0);
    lv_obj_set_style_radius(b, 1, 0); lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
  }
}
static void wf_lock_icon(lv_obj_t *p, int xr) {   // tiny padlock drawn from two shapes (no lock glyph in the font)
  lv_obj_t *sh = lv_obj_create(p); lv_obj_remove_style_all(sh);
  lv_obj_set_size(sh, 8, 9); lv_obj_set_style_radius(sh, 4, 0); lv_obj_set_style_border_width(sh, 2, 0);
  lv_obj_set_style_border_color(sh, lv_color_hex(C_GREY), 0); lv_obj_align(sh, LV_ALIGN_RIGHT_MID, xr - 1, -4);
  lv_obj_t *bd = lv_obj_create(p); lv_obj_remove_style_all(bd);
  lv_obj_set_size(bd, 10, 8); lv_obj_set_style_radius(bd, 2, 0); lv_obj_set_style_bg_opa(bd, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bd, lv_color_hex(C_GREY), 0); lv_obj_align(bd, LV_ALIGN_RIGHT_MID, xr, 2);
  lv_obj_clear_flag(sh, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(bd, LV_OBJ_FLAG_CLICKABLE);
}
static lv_obj_t *wf_row(int h) {
  lv_obj_t *r = lv_obj_create(wf_list); lv_obj_remove_style_all(r);
  lv_obj_set_size(r, 250, h); lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(r, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_color(r, lv_color_hex(C_PRESS), LV_STATE_PRESSED);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0); lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, lv_color_hex(C_DIV), 0);
  return r;
}
static void wf_section(const char *t) {
  lv_obj_t *r = wf_row(26); lv_obj_set_style_border_width(r, 0, 0);
  lv_obj_t *l = ui_label(r, &lv_font_montserrat_12, C_GREY, 0, t); lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 12, -4);
}

// ---------- join flow (overlay) ----------
static void wk_close() { if (wk) { lv_obj_del_async(wk); wk = NULL; } wn_req_scan = true; }
static void wk_show_busy() {
  lv_obj_add_flag(wk_form, LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(wk_busy, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wk_spin, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(wk_ok, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(wk_res_icon, LV_OBJ_FLAG_HIDDEN);
  char b[64]; snprintf(b, sizeof b, "Connecting to\n%s...", wk_ssid); lv_label_set_text(wk_res, b);
  lv_obj_set_style_text_color(wk_res, lv_color_hex(C_TEXT), 0);
}
static void wk_connect(const char *pass) {
  if (wn_conn_state == WN_BUSY) return;
  snprintf(wn_req_ssid, sizeof wn_req_ssid, "%s", wk_ssid); snprintf(wn_req_pass, sizeof wn_req_pass, "%s", pass ? pass : "");
  wn_conn_state = WN_BUSY; wn_req_conn = true;
  wk_show_busy();
}
static void wk_cancel_cb(lv_event_t *e) { if (wn_conn_state != WN_BUSY) wk_close(); }
static void wk_go_cb(lv_event_t *e) { wk_connect(lv_textarea_get_text(wk_ta)); }
static void wk_kb_cb(lv_event_t *e) {
  lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_READY) wk_connect(lv_textarea_get_text(wk_ta));
  else if (c == LV_EVENT_CANCEL) wk_close();
}
static void wk_eye_cb(lv_event_t *e) {
  bool hidden = lv_textarea_get_password_mode(wk_ta);
  lv_textarea_set_password_mode(wk_ta, !hidden);
  lv_label_set_text(wk_eye_lbl, hidden ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}
static void wk_ok_cb(lv_event_t *e) { wk_close(); }
static void wk_open(const char *ssid, bool need_pass, const char *saved_pass) {
  if (wk) return;
  snprintf(wk_ssid, sizeof wk_ssid, "%s", ssid);
  wk = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(wk);
  lv_obj_set_size(wk, 360, 360); lv_obj_set_style_bg_opa(wk, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(wk, lv_color_hex(C_BG), 0);
  lv_obj_clear_flag(wk, LV_OBJ_FLAG_SCROLLABLE);
  // form: SSID, password field + show/hide, compact keyboard sized for the round screen
  wk_form = lv_obj_create(wk); lv_obj_remove_style_all(wk_form); lv_obj_set_size(wk_form, 360, 360); lv_obj_clear_flag(wk_form, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *t = ui_label(wk_form, &lv_font_montserrat_16, C_TEXT, 220, ssid);
  lv_label_set_long_mode(t, LV_LABEL_LONG_DOT); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);
  wk_ta = lv_textarea_create(wk_form);
  lv_obj_set_size(wk_ta, 190, 40); lv_obj_align(wk_ta, LV_ALIGN_TOP_MID, -22, 54);
  lv_textarea_set_one_line(wk_ta, true); lv_textarea_set_password_mode(wk_ta, true); lv_textarea_set_max_length(wk_ta, 63);
  lv_textarea_set_placeholder_text(wk_ta, "Password");
  lv_obj_set_style_text_font(wk_ta, &lv_font_montserrat_16, 0);
  lv_obj_set_style_border_color(wk_ta, lv_color_hex(C_LINE), 0); lv_obj_set_style_border_width(wk_ta, 1, 0);
  lv_obj_set_style_radius(wk_ta, 8, 0); lv_obj_set_style_bg_color(wk_ta, lv_color_hex(C_BG), 0);
  lv_obj_add_state(wk_ta, LV_STATE_FOCUSED);
  lv_obj_t *eye = ui_btn(wk_form, 40, 40, LV_SYMBOL_EYE_OPEN, &wk_eye_lbl);
  lv_obj_align(eye, LV_ALIGN_TOP_MID, 100, 54); lv_obj_add_event_cb(eye, wk_eye_cb, LV_EVENT_CLICKED, NULL);
  wk_kb = lv_keyboard_create(wk_form);
  lv_obj_set_size(wk_kb, 284, 150); lv_obj_align(wk_kb, LV_ALIGN_TOP_MID, 0, 102);
  lv_keyboard_set_textarea(wk_kb, wk_ta);
  lv_obj_set_style_bg_color(wk_kb, lv_color_hex(C_SURF), 0); lv_obj_set_style_border_width(wk_kb, 0, 0);
  lv_obj_set_style_pad_all(wk_kb, 3, 0); lv_obj_set_style_pad_gap(wk_kb, 3, 0); lv_obj_set_style_radius(wk_kb, 10, 0);
  lv_obj_set_style_text_font(wk_kb, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(wk_kb, lv_color_hex(C_BG), LV_PART_ITEMS); lv_obj_set_style_text_color(wk_kb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
  lv_obj_set_style_border_color(wk_kb, lv_color_hex(C_DIV), LV_PART_ITEMS); lv_obj_set_style_border_width(wk_kb, 1, LV_PART_ITEMS);
  lv_obj_set_style_radius(wk_kb, 5, LV_PART_ITEMS); lv_obj_set_style_shadow_width(wk_kb, 0, LV_PART_ITEMS);
  lv_obj_add_event_cb(wk_kb, wk_kb_cb, LV_EVENT_READY, NULL); lv_obj_add_event_cb(wk_kb, wk_kb_cb, LV_EVENT_CANCEL, NULL);
  lv_obj_t *cb = ui_btn(wk_form, 96, 36, "Cancel", NULL); lv_obj_align(cb, LV_ALIGN_TOP_MID, -54, 262);
  lv_obj_add_event_cb(cb, wk_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *gl; lv_obj_t *gb = ui_btn(wk_form, 96, 36, "Connect", &gl); lv_obj_align(gb, LV_ALIGN_TOP_MID, 54, 262);
  lv_obj_set_style_bg_color(gb, lv_color_hex(C_TEXT), 0); lv_obj_set_style_text_color(gl, lv_color_hex(C_BG), 0);
  lv_obj_add_event_cb(gb, wk_go_cb, LV_EVENT_CLICKED, NULL);
  // busy / result view
  wk_busy = lv_obj_create(wk); lv_obj_remove_style_all(wk_busy); lv_obj_set_size(wk_busy, 360, 360);
  lv_obj_clear_flag(wk_busy, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(wk_busy, LV_OBJ_FLAG_HIDDEN);
  wk_spin = lv_spinner_create(wk_busy, 1000, 60); lv_obj_set_size(wk_spin, 50, 50); lv_obj_align(wk_spin, LV_ALIGN_TOP_MID, 0, 104);
  lv_obj_set_style_arc_color(wk_spin, lv_color_hex(C_DIV), LV_PART_MAIN); lv_obj_set_style_arc_width(wk_spin, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(wk_spin, lv_color_hex(C_TEXT), LV_PART_INDICATOR); lv_obj_set_style_arc_width(wk_spin, 4, LV_PART_INDICATOR);
  wk_res_icon = ui_label(wk_busy, &lv_font_montserrat_28, C_TEXT, 0, LV_SYMBOL_OK); lv_obj_align(wk_res_icon, LV_ALIGN_TOP_MID, 0, 112);
  wk_res = ui_label(wk_busy, &lv_font_montserrat_14, C_TEXT, 240, ""); lv_obj_align(wk_res, LV_ALIGN_TOP_MID, 0, 172);
  wk_ok = ui_btn(wk_busy, 96, 36, "OK", NULL); lv_obj_align(wk_ok, LV_ALIGN_TOP_MID, 0, 240);
  lv_obj_add_event_cb(wk_ok, wk_ok_cb, LV_EVENT_CLICKED, NULL);
  if (!need_pass) wk_connect(saved_pass);   // open network (or saved password): straight to "Connecting..."
}

// ---------- list ----------
static void wf_net_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= wf_nrows || wk || wn_conn_state == WN_BUSY) return;
  char pw[65] = "";
  bool saved = wn_known(wf_rows[i].ssid, pw);
  wk_open(wf_rows[i].ssid, wf_rows[i].secure && !saved, pw);
}
static char wf_forget_ssid[33];
static void wf_forget_ok(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(m) == 0) { strcpy(wn_req_forget, wf_forget_ssid); wn_req_forget_on = true; wf_rebuild_at = millis() + 600; }
  lv_msgbox_close_async(m);
}
static void wf_forget_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  wn_lock(); if (i < wn_nsaved) strcpy(wf_forget_ssid, wn_saved[i].ssid); else wf_forget_ssid[0] = 0; wn_unlock();
  if (!wf_forget_ssid[0]) return;
  static const char *btns[] = {"Forget", "Cancel", ""};
  char b[64]; snprintf(b, sizeof b, "Forget %s?", wf_forget_ssid);
  lv_obj_t *m = lv_msgbox_create(NULL, b, "The puck won't join it automatically any more.", btns, false);
  ui_style_msgbox(m); lv_obj_set_width(m, 230); lv_obj_center(m);
  lv_obj_add_event_cb(m, wf_forget_ok, LV_EVENT_VALUE_CHANGED, NULL);
}
static void wf_rebuild() {
  if (!wf_list) return;
  lv_obj_clean(wf_list);
  String cur = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("");
  if (wn_scan_state == WN_BUSY) { wf_section("Scanning..."); }
  else {
    memcpy(wf_rows, wn_scan, sizeof wf_rows); wf_nrows = wn_nscan;
    wf_section(wf_nrows ? "Networks" : (wn_scan_state == WN_FAIL ? "Scan failed - tap Scan" : "No networks found"));
    for (int i = 0; i < wf_nrows; i++) {
      lv_obj_t *r = wf_row(40); lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(r, wf_net_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
      wf_bars(r, wf_rows[i].rssi, 12);
      bool here = cur.length() && cur == wf_rows[i].ssid;
      lv_obj_t *l = ui_label(r, &lv_font_montserrat_14, C_TEXT, 150, wf_rows[i].ssid);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 40, here || wn_known(wf_rows[i].ssid, NULL) ? -7 : 0);
      if (here || wn_known(wf_rows[i].ssid, NULL)) {
        lv_obj_t *s = ui_label(r, &lv_font_montserrat_12, C_GREY, 0, here ? "Connected" : "Saved");
        lv_obj_align(s, LV_ALIGN_LEFT_MID, 40, 10);
      }
      if (wf_rows[i].secure) wf_lock_icon(r, -14);
    }
  }
  wn_lock(); int n = wn_nsaved; char names[WN_MAX][33]; for (int i = 0; i < n; i++) strcpy(names[i], wn_saved[i].ssid); wn_unlock();
  if (n) {
    wf_section("Saved networks");
    for (int i = 0; i < n; i++) {
      lv_obj_t *r = wf_row(40);
      lv_obj_t *l = ui_label(r, &lv_font_montserrat_14, C_TEXT, 150, names[i]);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, 0);
      lv_obj_t *fl; lv_obj_t *fb = ui_btn(r, 70, 28, "Forget", &fl);
      lv_obj_set_style_text_color(fl, lv_color_hex(C_RED), 0); lv_obj_set_style_text_font(fl, &lv_font_montserrat_12, 0);
      lv_obj_align(fb, LV_ALIGN_RIGHT_MID, -8, 0); lv_obj_add_event_cb(fb, wf_forget_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
  }
}
static void wf_scan_cb(lv_event_t *e) { if (wn_scan_state != WN_BUSY) { wn_req_scan = true; wn_scan_state = WN_BUSY; wf_rebuild(); } }

static void wf_status() {
  if (!wf_open) return;
  bool on = WiFi.status() == WL_CONNECTED;
  lv_label_set_text(wf_ssid, on ? WiFi.SSID().c_str() : "Not connected");
  char b[64];
  if (on) snprintf(b, sizeof b, "%s  |  %d dBm", WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
  else snprintf(b, sizeof b, "Pick a network below");
  lv_label_set_text(wf_info, b);
}
static void wf_build(lv_obj_t *p) {
  wf_open = true;
  wf_ssid = ui_label(p, &lv_font_montserrat_16, C_TEXT, 220, ""); lv_label_set_long_mode(wf_ssid, LV_LABEL_LONG_DOT);
  lv_obj_align(wf_ssid, LV_ALIGN_TOP_MID, 0, 70);
  wf_info = ui_label(p, &lv_font_montserrat_12, C_GREY, 240, ""); lv_obj_align(wf_info, LV_ALIGN_TOP_MID, 0, 92);
  wf_scan_btn = ui_btn(p, 76, 26, LV_SYMBOL_REFRESH " Scan", NULL);
  lv_obj_set_style_text_font(wf_scan_btn, &lv_font_montserrat_12, 0);
  lv_obj_align(wf_scan_btn, LV_ALIGN_TOP_MID, 0, 112); lv_obj_add_event_cb(wf_scan_btn, wf_scan_cb, LV_EVENT_CLICKED, NULL);
  wf_list = lv_obj_create(p); lv_obj_remove_style_all(wf_list);
  lv_obj_set_size(wf_list, 250, 196); lv_obj_align(wf_list, LV_ALIGN_TOP_MID, 0, 142);
  lv_obj_set_flex_flow(wf_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(wf_list, LV_DIR_VER); lv_obj_set_scrollbar_mode(wf_list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_bottom(wf_list, 60, 0);
  wf_status();
  if (wn_scan_state != WN_BUSY) { wn_req_scan = true; wn_scan_state = WN_BUSY; }   // scan on open (net task, no freeze)
  wf_seen_gen = wn_scan_gen; wf_rebuild();
}
static void wf_close() { wf_open = false; wf_list = NULL; if (wk) { lv_obj_del(wk); wk = NULL; } }
// ~30 Hz
static void wf_tick() {
  if (!wf_open) return;
  if (wn_scan_gen != wf_seen_gen && !wk) { wf_seen_gen = wn_scan_gen; wf_rebuild(); wf_status(); }
  if (wf_rebuild_at && millis() > wf_rebuild_at) { wf_rebuild_at = 0; wf_rebuild(); }
  if (wk && wn_conn_state != WN_BUSY && !lv_obj_has_flag(wk_busy, LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(wk_ok, LV_OBJ_FLAG_HIDDEN)) {
    bool ok = wn_conn_state == WN_DONE;
    lv_obj_add_flag(wk_spin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wk_res_icon, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(wk_res_icon, ok ? LV_SYMBOL_OK : LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(wk_res_icon, lv_color_hex(ok ? C_TEXT : C_RED), 0);
    lv_label_set_text(wk_res, wn_conn_msg);
    lv_obj_set_style_text_color(wk_res, lv_color_hex(ok ? C_TEXT : C_RED), 0);
    lv_obj_clear_flag(wk_ok, LV_OBJ_FLAG_HIDDEN);
    wf_status();
  }
}
