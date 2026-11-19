// Notion-inspired minimal palette + shared styles (v7)
#pragma once
#define C_BG     0xFFFFFF   // page background
#define C_SURF   0xF7F6F3   // subtle surface (pressed / list hover)
#define C_TEXT   0x37352F   // primary text
#define C_GREY   0x787774   // secondary text
#define C_DIV    0xE9E9E7   // dividers
#define C_LINE   0xDCDBD8   // outlines (a touch darker than dividers so they read on the round panel)
#define C_RED    0xE03E3E   // record dot / stop only
#define C_PRESS  0xEFEFED

static lv_style_t st_btn, st_btn_pr;
static void ui_styles_init() {
  lv_style_init(&st_btn);
  lv_style_set_bg_color(&st_btn, lv_color_hex(C_BG)); lv_style_set_bg_opa(&st_btn, LV_OPA_COVER);
  lv_style_set_bg_grad_dir(&st_btn, LV_GRAD_DIR_NONE);
  lv_style_set_border_color(&st_btn, lv_color_hex(C_LINE)); lv_style_set_border_width(&st_btn, 1); lv_style_set_border_opa(&st_btn, LV_OPA_COVER);
  lv_style_set_shadow_width(&st_btn, 0); lv_style_set_outline_width(&st_btn, 0);
  lv_style_set_text_color(&st_btn, lv_color_hex(C_TEXT)); lv_style_set_text_font(&st_btn, &lv_font_montserrat_14);
  lv_style_set_radius(&st_btn, 8);
  lv_style_init(&st_btn_pr);
  lv_style_set_bg_color(&st_btn_pr, lv_color_hex(C_PRESS)); lv_style_set_color_filter_opa(&st_btn_pr, LV_OPA_TRANSP);
  lv_style_set_shadow_width(&st_btn_pr, 0); lv_style_set_translate_y(&st_btn_pr, 0);
}
static lv_obj_t *ui_btn(lv_obj_t *p, int w, int h, const char *txt, lv_obj_t **lbl_out) {
  lv_obj_t *b = lv_btn_create(p);
  lv_obj_add_style(b, &st_btn, 0); lv_obj_add_style(b, &st_btn_pr, LV_STATE_PRESSED);
  lv_obj_set_size(b, w, h);
  lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, txt); lv_obj_center(l);
  if (lbl_out) *lbl_out = l;
  return b;
}
static lv_obj_t *ui_label(lv_obj_t *p, const lv_font_t *f, uint32_t col, int w, const char *txt) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0); lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  if (w > 0) { lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); lv_obj_set_width(l, w); }
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(l, txt);
  return l;
}
static lv_obj_t *ui_title(lv_obj_t *p, const char *txt) {   // Notion-style page heading
  lv_obj_t *l = ui_label(p, &lv_font_montserrat_20, C_TEXT, 0, txt);
  lv_obj_set_style_text_letter_space(l, 0, 0);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 28);
  return l;
}
static lv_obj_t *ui_divider(lv_obj_t *p, int w, int y) {
  lv_obj_t *d = lv_obj_create(p); lv_obj_remove_style_all(d);
  lv_obj_set_size(d, w, 1); lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0); lv_obj_set_style_bg_color(d, lv_color_hex(C_DIV), 0);
  lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE); lv_obj_align(d, LV_ALIGN_TOP_MID, 0, y);
  return d;
}
static void ui_style_msgbox(lv_obj_t *m) {
  lv_obj_set_style_bg_color(m, lv_color_hex(C_BG), 0); lv_obj_set_style_shadow_width(m, 0, 0);
  lv_obj_set_style_border_color(m, lv_color_hex(C_LINE), 0); lv_obj_set_style_border_width(m, 1, 0);
  lv_obj_set_style_radius(m, 10, 0); lv_obj_set_style_text_color(m, lv_color_hex(C_TEXT), 0);
  lv_obj_t *bm = lv_msgbox_get_btns(m);
  if (bm) {
    lv_obj_set_style_bg_color(bm, lv_color_hex(C_BG), LV_PART_ITEMS); lv_obj_set_style_text_color(bm, lv_color_hex(C_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_border_color(bm, lv_color_hex(C_LINE), LV_PART_ITEMS); lv_obj_set_style_border_width(bm, 1, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(bm, 0, LV_PART_ITEMS); lv_obj_set_style_radius(bm, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bm, lv_color_hex(C_PRESS), LV_PART_ITEMS | LV_STATE_PRESSED);
  }
}
