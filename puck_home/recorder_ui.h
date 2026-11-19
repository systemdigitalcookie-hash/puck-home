// ================= Recorder page (v10): explicit state machine + recent recordings + detail screen =================
// States: READY (record button + recent list) -> RECORDING (pulsing dot, big timer, level, Pause/Stop) <-> PAUSED
//         -> SAVING (spinner) -> SAVED (check, 2 s) -> READY;  any failure -> ERROR (message + Retry / OK).
// Included from puck_home.ino after ui_style.h and nav_apply().
#pragma once

enum { RU_READY = 0, RU_REC, RU_PAUSED, RU_SAVING, RU_SAVED, RU_ERROR };
static int ru = -1;
static uint32_t ru_t0 = 0, ru_tap = 0;
static char ru_err[80];
static uint32_t ru_saved_secs = 0;
static lv_obj_t *rp_ready, *rp_rec, *rp_saving, *rp_saved, *rp_err;
static lv_obj_t *r_list, *r_dot, *r_state, *r_timer, *r_bar, *r_pause_btn, *r_pause_lbl, *r_stop_btn, *r_hs = NULL;
static lv_obj_t *r_stage, *r_saved_lbl, *r_err_lbl;
static RecFile files[40];
static int nfiles = 0;
static bool list_dirty = true;
static uint32_t list_t = 0;
static bool ru_sd_fb = false;   // v4: last save fell back from SD to internal flash   // v4: when the recordings list was last rebuilt
static int list_seen_count = -1;

static void fmt_mmss(char *o, size_t n, uint32_t s) {
  if (s >= 3600) snprintf(o, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
  else snprintf(o, n, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}
static uint32_t wav_secs(uint32_t bytes) { return bytes > 44 ? (bytes - 44) / 32000 : 0; }
static bool ru_debounce() { uint32_t n = millis(); if (n - ru_tap < 400) return false; ru_tap = n; return true; }
static lv_obj_t *ru_panel(lv_obj_t *t) {
  lv_obj_t *p = lv_obj_create(t); lv_obj_remove_style_all(p);
  lv_obj_set_size(p, 360, 360); lv_obj_set_pos(p, 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  return p;
}

static void ru_set(int s) {
  if (s == ru) return;
  ru = s; ru_t0 = millis();
  lv_obj_t *show = s == RU_READY ? rp_ready : (s == RU_REC || s == RU_PAUSED) ? rp_rec : s == RU_SAVING ? rp_saving : s == RU_SAVED ? rp_saved : rp_err;
  lv_obj_t *all[] = {rp_ready, rp_rec, rp_saving, rp_saved, rp_err};
  for (lv_obj_t *p : all) { if (p == show) lv_obj_clear_flag(p, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN); }
  if (s == RU_REC || s == RU_PAUSED) {
    bool rec_on = s == RU_REC;
    lv_label_set_text(r_state, rec_on ? "Recording" : "Paused");
    lv_obj_set_style_bg_color(r_dot, lv_color_hex(rec_on ? C_RED : C_GREY), 0);
    lv_obj_set_style_bg_opa(r_dot, LV_OPA_COVER, 0);
    lv_label_set_text(r_pause_lbl, rec_on ? LV_SYMBOL_PAUSE "  Pause" : LV_SYMBOL_PLAY "  Resume");
    lv_obj_set_style_opa(r_timer, LV_OPA_COVER, 0);
    if (rec_on) lv_obj_clear_flag(r_bar, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(r_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_tile_id(tv, 0, 0, LV_ANIM_OFF);
  }
  if (s == RU_SAVING) lv_obj_add_flag(r_stage, LV_OBJ_FLAG_HIDDEN);
  if (s == RU_SAVED) {
    char d[16], b[64]; fmt_mmss(d, sizeof d, ru_saved_secs);
    if (d[0] == '0' && d[2] != ':') memmove(d, d + 1, strlen(d));   // 00:42 -> 0:42
    if (ru_sd_fb) snprintf(b, sizeof b, "Saved %s to internal (SD error)", d);
    else snprintf(b, sizeof b, "Saved %s to %s", d, !strcmp(rec.storage, "SD") ? "SD" : "flash");
    lv_label_set_text(r_saved_lbl, b);
  }
  if (s == RU_ERROR) lv_label_set_text(r_err_lbl, ru_err);
  if (s == RU_READY) list_dirty = true;
  lock_rec = (s == RU_REC || s == RU_PAUSED || s == RU_SAVING);
  nav_apply();
}
static void ru_error(const char *m) { snprintf(ru_err, sizeof ru_err, "%s", m && m[0] ? m : "Something went wrong"); ru_set(RU_ERROR); }

static void ru_start() {
  if (rec_playing) rec_play_stop();
  rec_start();
  if (rec.state == REC_RECORDING) ru_set(RU_REC);
  else ru_error(rec.msg[0] ? rec.msg : "Could not start recording");
}
static void ru_rec_cb(lv_event_t *e) { if (ru_debounce() && ru == RU_READY) ru_start(); }
static void ru_pause_cb(lv_event_t *e) {
  if (!ru_debounce()) return;
  if (rec.state == REC_RECORDING) { rec_pause(); ru_set(RU_PAUSED); }
  else if (rec.state == REC_PAUSED) { rec_resume(); ru_set(RU_REC); }
}
static void ru_stop_cb(lv_event_t *e) {
  if (!ru_debounce()) return;
  if (rec.state == REC_RECORDING || rec.state == REC_PAUSED) { ru_saved_secs = rec.secs; rec_stop(); ru_set(RU_SAVING); }
}
static void ru_ok_cb(lv_event_t *e) { if (ru_debounce()) ru_set(RU_READY); }
static void ru_retry_cb(lv_event_t *e) { if (!ru_debounce()) return; ru_set(RU_READY); ru_start(); }

// ---------- recording detail overlay ----------
static lv_obj_t *rd = NULL, *rd_play_lbl, *rd_bar, *rd_time;
static RecFile rd_f;
static bool rd_del_pending = false;
static void rd_close() {
  if (!rd) return;
  if (rec_playing) rec_play_stop();
  lv_obj_del_async(rd); rd = NULL; rd_del_pending = false;
  for (int i = 0; i < NPAGES; i++) lv_obj_clear_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
  list_dirty = true;
}
static void rd_back_cb(lv_event_t *e) { rd_close(); }
static void rd_gesture_cb(lv_event_t *e) { if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) rd_close(); }
static void rd_play_cb(lv_event_t *e) {
  if (!ru_debounce()) return;
  if (rec_playing) { rec_play_toggle_pause(); return; }
  bool busy = rec.state != REC_IDLE;
  if (busy) { lv_label_set_text(rd_time, "Audio busy"); return; }
  rec_play(rd_f.name);
  if (!rec_playing) lv_label_set_text(rd_time, rec.msg);
}
static void rd_del_confirm_cb(lv_event_t *e) {
  lv_obj_t *m = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(m) == 0) { if (rec_playing) rec_play_stop(); rd_del_pending = true; }
  lv_msgbox_close_async(m);
}
static void rd_del_cb(lv_event_t *e) {
  static const char *btns[] = {"Delete", "Cancel", ""};
  lv_obj_t *m = lv_msgbox_create(NULL, "Delete recording?", "This can't be undone.", btns, false);
  ui_style_msgbox(m); lv_obj_set_width(m, 220); lv_obj_center(m);
  lv_obj_add_event_cb(m, rd_del_confirm_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
static void rec_title(const RecFile &f, char *o, size_t n) { int num = 0; sscanf(f.name, "rec_%d", &num); snprintf(o, n, "Recording %d", num); }
static void rec_date(const RecFile &f, char *o, size_t n) {
  if (f.t > (time_t)1767225600) { struct tm tmv; localtime_r(&f.t, &tmv); strftime(o, n, "%d %b, %H:%M", &tmv); } else snprintf(o, n, "No date");
}
static void rd_open(int i) {
  if (rd || i < 0 || i >= nfiles) return;
  rd_f = files[i];
  rd = lv_obj_create(lv_scr_act()); lv_obj_remove_style_all(rd);
  lv_obj_set_size(rd, 360, 360); lv_obj_set_style_bg_opa(rd, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(rd, lv_color_hex(C_BG), 0);
  lv_obj_clear_flag(rd, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(rd, rd_gesture_cb, LV_EVENT_GESTURE, NULL);
  for (int k = 0; k < NPAGES; k++) lv_obj_add_flag(dots[k], LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *bk = ui_btn(rd, 36, 36, LV_SYMBOL_LEFT, NULL);
  lv_obj_set_style_radius(bk, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_border_width(bk, 0, 0);
  lv_obj_align(bk, LV_ALIGN_TOP_MID, -92, 22); lv_obj_set_ext_click_area(bk, 10);
  lv_obj_add_event_cb(bk, rd_back_cb, LV_EVENT_CLICKED, NULL);
  char b[48]; rec_title(rd_f, b, sizeof b); ui_title(rd, b);
  rec_date(rd_f, b, sizeof b);
  lv_obj_t *dl = ui_label(rd, &lv_font_montserrat_12, C_GREY, 0, b); lv_obj_align(dl, LV_ALIGN_TOP_MID, 0, 56);
  lv_obj_t *pb = ui_btn(rd, 64, 64, LV_SYMBOL_PLAY, &rd_play_lbl);
  lv_obj_set_style_radius(pb, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_text_font(rd_play_lbl, &lv_font_montserrat_20, 0);
  lv_obj_align(pb, LV_ALIGN_TOP_MID, 0, 84); lv_obj_add_event_cb(pb, rd_play_cb, LV_EVENT_CLICKED, NULL);
  rd_bar = lv_bar_create(rd); lv_obj_set_size(rd_bar, 200, 8); lv_obj_align(rd_bar, LV_ALIGN_TOP_MID, 0, 164);
  lv_bar_set_range(rd_bar, 0, 1000);
  theme_bar(rd_bar, 8);
  rd_time = ui_label(rd, &lv_font_montserrat_12, C_GREY, 220, ""); lv_obj_align(rd_time, LV_ALIGN_TOP_MID, 0, 174);
  // info rows
  const char *keys[2] = {"Stored", "Size"}; char vals[2][24];
  snprintf(vals[0], 24, "%s", !strcmp(rec.storage, "SD") ? "SD card" : "Internal flash");
  snprintf(vals[1], 24, "%.1f MB", rd_f.bytes / 1048576.0f);
  for (int k = 0; k < 2; k++) {
    int y = 200 + k * 30;
    lv_obj_t *kl = ui_label(rd, &lv_font_montserrat_14, C_GREY, 0, keys[k]); lv_obj_align(kl, LV_ALIGN_TOP_MID, -60, y);
    lv_obj_t *vl = ui_label(rd, &lv_font_montserrat_14, C_TEXT, 120, vals[k]); lv_obj_set_style_text_align(vl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(vl, LV_ALIGN_TOP_MID, 46, y);
    ui_divider(rd, 220, y + 24);
  }
  lv_obj_t *db = ui_btn(rd, 120, 34, LV_SYMBOL_TRASH "  Delete", NULL);
  lv_obj_set_style_text_color(db, lv_color_hex(C_RED), 0);
  lv_obj_align(db, LV_ALIGN_TOP_MID, 0, 270); lv_obj_add_event_cb(db, rd_del_cb, LV_EVENT_CLICKED, NULL);
}
static void rd_tick() {
  if (!rd) return;
  if (rd_del_pending && !rec_playing) {
    rd_del_pending = false;
    if (rec_delete(rd_f.name)) { rd_close(); return; }
    lv_label_set_text(rd_time, "Delete failed");
  }
  bool playing = rec_playing && !rec_play_paused;
  const char *want = playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
  if (strcmp(lv_label_get_text(rd_play_lbl), want)) lv_label_set_text(rd_play_lbl, want);
  uint32_t tot = wav_secs(rd_f.bytes);
  uint32_t pos = rec_playing ? rec_play_pos / 32000 : 0;
  lv_bar_set_value(rd_bar, rec_playing && rec_play_total ? (int)((uint64_t)rec_play_pos * 1000 / rec_play_total) : 0, LV_ANIM_OFF);
  char a[16], c[16], b[40]; fmt_mmss(a, sizeof a, pos); fmt_mmss(c, sizeof c, tot); snprintf(b, sizeof b, "%s / %s", a, c);
  if (strcmp(lv_label_get_text(rd_time), b) && (rec_playing || !strchr(lv_label_get_text(rd_time), 'y'))) lv_label_set_text(rd_time, b);
}

// ---------- recent list ----------
static void row_cb(lv_event_t *e) { rd_open((int)(intptr_t)lv_event_get_user_data(e)); }
static void refresh_list() {
  list_dirty = false; list_t = millis();
  if (rec.state != REC_IDLE) return;
  nfiles = rec_list(files, 40);
  lv_obj_clean(r_list);
  if (nfiles == 0) {
    lv_obj_t *e = ui_label(r_list, &lv_font_montserrat_14, C_GREY, 236, "No recordings yet");
    lv_obj_set_style_pad_top(e, 24, 0);
  }
  for (int i = 0; i < nfiles; i++) {
    lv_obj_t *r = lv_obj_create(r_list); lv_obj_remove_style_all(r);
    lv_obj_set_size(r, 236, 46);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(r, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(C_PRESS), LV_STATE_PRESSED);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0); lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, lv_color_hex(C_DIV), 0);
    lv_obj_add_event_cb(r, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    char t[32], d[16], dt[24];
    rec_title(files[i], t, sizeof t); fmt_mmss(d, sizeof d, wav_secs(files[i].bytes)); rec_date(files[i], dt, sizeof dt);
    lv_obj_t *l1 = ui_label(r, &lv_font_montserrat_14, C_TEXT, 0, t); lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 10, 6);
    lv_obj_t *l2 = ui_label(r, &lv_font_montserrat_12, C_GREY, 0, d); lv_obj_align(l2, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_t *l3 = ui_label(r, &lv_font_montserrat_12, C_GREY, 0, dt); lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 10, 26);
    // sync slot (future Notion upload): hollow mark = not uploaded yet
    lv_obj_t *mk = lv_obj_create(r); lv_obj_remove_style_all(mk);
    lv_obj_set_size(mk, 8, 8); lv_obj_set_style_radius(mk, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(mk, 1, 0); lv_obj_set_style_border_color(mk, lv_color_hex(C_LINE), 0);
    lv_obj_clear_flag(mk, LV_OBJ_FLAG_CLICKABLE); lv_obj_align(mk, LV_ALIGN_TOP_RIGHT, -12, 30);
  }
  list_seen_count = rec.count;
}

static void ru_build(lv_obj_t *t0) {
  lv_obj_clear_flag(t0, LV_OBJ_FLAG_SCROLLABLE);
  // READY: title, record button, recent list
  rp_ready = ru_panel(t0);
  ui_title(rp_ready, "Recorder");
  // heart-shaped record button: a big hot-pink heart with a cream dot
  lv_obj_t *rb = lv_obj_create(rp_ready); lv_obj_remove_style_all(rb);
  lv_obj_set_size(rb, 78, 71); lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, 54);
  lv_obj_add_flag(rb, LV_OBJ_FLAG_CLICKABLE); lv_obj_clear_flag(rb, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_ext_click_area(rb, 8);
  lv_obj_set_style_translate_y(rb, 2, LV_STATE_PRESSED);
  lv_obj_add_event_cb(rb, ru_rec_cb, LV_EVENT_CLICKED, NULL);
  theme_art(rb, &art_heart_big, C_HOT, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *dot = lv_obj_create(rb); lv_obj_remove_style_all(dot);
  lv_obj_set_size(dot, 24, 24); lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(dot, lv_color_hex(C_CARD), 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE); lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 22);
  lv_obj_t *tl = ui_label(rp_ready, T_FONT_S, C_GREY, 0, "Tap the heart to record"); lv_obj_align(tl, LV_ALIGN_TOP_MID, 0, 130);
  ui_divider(rp_ready, 236, 150);
  r_list = lv_obj_create(rp_ready); lv_obj_remove_style_all(r_list);
  lv_obj_set_size(r_list, 236, 180); lv_obj_align(r_list, LV_ALIGN_TOP_MID, 0, 151);
  lv_obj_set_flex_flow(r_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(r_list, LV_DIR_VER); lv_obj_set_scrollbar_mode(r_list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_bottom(r_list, 50, 0);

  // RECORDING / PAUSED
  rp_rec = ru_panel(t0);
  lv_obj_t *row = lv_obj_create(rp_rec); lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 160, 22); lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 8, 0); lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  r_dot = lv_obj_create(row); lv_obj_remove_style_all(r_dot);
  lv_obj_set_size(r_dot, 10, 10); lv_obj_set_style_radius(r_dot, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_opa(r_dot, LV_OPA_COVER, 0);
  r_state = ui_label(row, &lv_font_montserrat_16, C_TEXT, 0, "Recording");
  r_hs = ui_btn(rp_rec, 104, 104, LV_SYMBOL_AUDIO, NULL);   // pink mic badge with a little bow
  lv_obj_set_style_radius(r_hs, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_color(r_hs, lv_color_hex(C_SURF), 0); lv_obj_set_style_text_color(r_hs, lv_color_hex(C_HOT), 0); lv_obj_set_style_border_width(r_hs, 3, 0); lv_obj_set_style_text_font(r_hs, &lv_font_montserrat_48, 0);
  lv_obj_clear_flag(r_hs, LV_OBJ_FLAG_CLICKABLE); lv_obj_align(r_hs, LV_ALIGN_TOP_MID, 0, 72);
  theme_bow(rp_rec, LV_ALIGN_TOP_MID, 30, 60);
  r_timer = ui_label(rp_rec, &lv_font_montserrat_48, C_TEXT, 0, "00:00"); lv_obj_align(r_timer, LV_ALIGN_TOP_MID, 0, 198);
  r_bar = lv_bar_create(rp_rec); lv_obj_set_size(r_bar, 140, 8); lv_obj_align(r_bar, LV_ALIGN_TOP_MID, 0, 256);
  lv_bar_set_range(r_bar, 0, 100);
  theme_bar(r_bar, 8);   // pink level bar
  r_pause_btn = ui_btn(rp_rec, 104, 42, LV_SYMBOL_PAUSE "  Pause", &r_pause_lbl);
  lv_obj_set_style_radius(r_pause_btn, 21, 0); lv_obj_align(r_pause_btn, LV_ALIGN_TOP_MID, -58, 270);
  lv_obj_add_event_cb(r_pause_btn, ru_pause_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl;
  r_stop_btn = ui_btn(rp_rec, 104, 42, "", &sl);
  lv_label_set_recolor(sl, true); lv_label_set_text(sl, C_RED_TXT " " LV_SYMBOL_STOP "#  Stop");
  lv_obj_set_style_radius(r_stop_btn, 21, 0); lv_obj_align(r_stop_btn, LV_ALIGN_TOP_MID, 58, 270);
  lv_obj_add_event_cb(r_stop_btn, ru_stop_cb, LV_EVENT_CLICKED, NULL);

  // SAVING
  rp_saving = ru_panel(t0);
  lv_obj_t *sp = lv_spinner_create(rp_saving, 1000, 60); lv_obj_set_size(sp, 56, 56); lv_obj_align(sp, LV_ALIGN_TOP_MID, 0, 118);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_SURF), LV_PART_MAIN); lv_obj_set_style_arc_width(sp, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_HOT), LV_PART_INDICATOR); lv_obj_set_style_arc_width(sp, 4, LV_PART_INDICATOR);
  lv_obj_t *sv = ui_label(rp_saving, &lv_font_montserrat_16, C_TEXT, 0, "Saving..."); lv_obj_align(sv, LV_ALIGN_TOP_MID, 0, 194);
  r_stage = ui_label(rp_saving, &lv_font_montserrat_12, C_GREY, 220, ""); lv_obj_align(r_stage, LV_ALIGN_TOP_MID, 0, 220);

  // SAVED
  rp_saved = ru_panel(t0);
  lv_obj_t *ck = ui_btn(rp_saved, 64, 64, LV_SYMBOL_OK, NULL);
  lv_obj_set_style_radius(ck, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_text_font(ck, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(ck, lv_color_hex(C_HOT), 0); lv_obj_clear_flag(ck, LV_OBJ_FLAG_CLICKABLE); lv_obj_align(ck, LV_ALIGN_TOP_MID, 0, 114);
  theme_heart(rp_saved, -52, 120, true); theme_heart(rp_saved, 52, 120, true);
  r_saved_lbl = ui_label(rp_saved, &lv_font_montserrat_16, C_TEXT, 260, "Saved"); lv_obj_align(r_saved_lbl, LV_ALIGN_TOP_MID, 0, 194);

  // ERROR
  rp_err = ru_panel(t0);
  lv_obj_t *wi = ui_label(rp_err, &lv_font_montserrat_28, C_RED, 0, LV_SYMBOL_WARNING); lv_obj_align(wi, LV_ALIGN_TOP_MID, 0, 98);
  r_err_lbl = ui_label(rp_err, &lv_font_montserrat_14, C_RED, 230, ""); lv_obj_align(r_err_lbl, LV_ALIGN_TOP_MID, 0, 146);
  lv_obj_t *rt = ui_btn(rp_err, 96, 38, "Retry", NULL); lv_obj_align(rt, LV_ALIGN_TOP_MID, -54, 226);
  lv_obj_add_event_cb(rt, ru_retry_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *ok = ui_btn(rp_err, 96, 38, "OK", NULL); lv_obj_align(ok, LV_ALIGN_TOP_MID, 54, 226);
  lv_obj_add_event_cb(ok, ru_ok_cb, LV_EVENT_CLICKED, NULL);

  ru_set(RU_READY);
}

// ~30 Hz from loop()
static void ru_tick(uint32_t now) {
  int st = rec.state;
  // follow the recorder core
  if (st == REC_RECORDING && ru != RU_REC) ru_set(RU_REC);
  else if (st == REC_PAUSED && ru != RU_PAUSED) ru_set(RU_PAUSED);
  else if (st == REC_FINALIZING && ru != RU_SAVING) { ru_saved_secs = rec.secs; ru_set(RU_SAVING); }
  else if (st == REC_IDLE && (ru == RU_REC || ru == RU_PAUSED || ru == RU_SAVING)) {
    ru_sd_fb = !strncmp(rec.msg, "Saved to internal", 17);   // v4: SD fallback is still a successful save
    if (!strcmp(rec.msg, "saved") || ru_sd_fb) { ru_saved_secs = rec.secs; ru_set(RU_SAVED); }
    else ru_error(rec.msg);
  }
  uint32_t el = now - ru_t0;
  switch (ru) {
    case RU_REC: case RU_PAUSED: {
      static uint32_t shown = 0xFFFFFFFF; uint32_t s = rec.secs;
      if (s != shown) { shown = s; char b[16]; fmt_mmss(b, sizeof b, s); lv_label_set_text(r_timer, b); }
      if (ru == RU_REC) {
        lv_bar_set_value(r_bar, rec.level, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(r_dot, (lv_opa_t)(150 + 105 * sinf(now / 250.0f)), 0);   // pulse
      } else lv_obj_set_style_opa(r_timer, (el / 500) % 2 ? LV_OPA_40 : LV_OPA_COVER, 0);   // frozen timer blinks
      break;
    }
    case RU_SAVING:   // watchdog: never leave the screen stuck on "Saving"
      if (el > 5000) { lv_label_set_text(r_stage, rec.msg); lv_obj_clear_flag(r_stage, LV_OBJ_FLAG_HIDDEN); }
      if (el > 10000 && lock_rec) { lock_rec = false; nav_apply(); }
      if (el > 30000 && st == REC_FINALIZING) rec_force_idle("Save timed out. Restart the puck.");
      break;
    case RU_SAVED: if (el > 2000) ru_set(RU_READY); break;
    case RU_READY: if (!rd && ui_settled && (list_dirty || rec.count != list_seen_count)) refresh_list(); break;   // v4: never mid-swipe
  }
  rd_tick();
}
