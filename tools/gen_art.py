#!/usr/bin/env python3
"""Draws the cute shapes (heart, bow, flower, bunny ears, original plush face) and writes puck_home/art.h.
All shapes are ORIGINAL generic doodles made from circles and curves, drawn here in code.
Each one is an 8-bit "alpha mask": the firmware colours it from theme.h, so changing a
colour never needs new artwork. Run:  python3 tools/gen_art.py   (needs: pip install pillow)"""
import json, math, os
from PIL import Image, ImageDraw, ImageFilter
SS = 6  # supersampling for smooth edges

def canvas(w, h):
    return Image.new("L", (w * SS, h * SS), 0)

def finish(im, w, h):
    return im.resize((w, h), Image.LANCZOS)

def heart(w=48, h=44):
    im = canvas(w, h); d = ImageDraw.Draw(im)
    pts = []
    for i in range(400):
        t = i / 400 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append(((w / 2 + x * w / 36) * SS, (h * 0.42 - y * h / 32) * SS))
    d.polygon(pts, fill=255)
    return finish(im, w, h)

def bow_wings(w=44, h=28):
    """Two rounded loops tilted away from the centre knot."""
    im = canvas(w, h); S = SS
    for sign in (-1, 1):
        e = Image.new("L", (w * S, h * S), 0); d = ImageDraw.Draw(e)
        cx, cy = w / 2 + sign * w * 0.25, h / 2
        ew, eh = w * 0.27, h * 0.36
        d.ellipse(((cx - ew) * S, (cy - eh) * S, (cx + ew) * S, (cy + eh) * S), fill=255)
        e = e.rotate(sign * 22, center=(cx * S, cy * S), resample=Image.BICUBIC)
        im = Image.composite(Image.new("L", im.size, 255), im, e)
    return finish(im, w, h)

def bow_knot(w=44, h=28):
    im = canvas(w, h); d = ImageDraw.Draw(im); S = SS
    r = h * 0.2
    d.ellipse(((w / 2 - r) * S, (h / 2 - r) * S, (w / 2 + r) * S, (h / 2 + r) * S), fill=255)
    return finish(im, w, h)

def flower(w=30, h=30):
    im = canvas(w, h); d = ImageDraw.Draw(im); S = SS
    pr = w * 0.19
    for k in range(5):
        a = k / 5 * 2 * math.pi - math.pi / 2
        cx = w / 2 + math.cos(a) * w * 0.27; cy = h / 2 + math.sin(a) * h * 0.27
        d.ellipse(((cx - pr) * S, (cy - pr) * S, (cx + pr) * S, (cy + pr) * S), fill=255)
    r = w * 0.1
    d.ellipse(((w / 2 - r) * S, (h / 2 - r) * S, (w / 2 + r) * S, (h / 2 + r) * S), fill=255)
    return finish(im, w, h)

def flower_core(w=30, h=30):
    im = canvas(w, h); d = ImageDraw.Draw(im); S = SS; r = w * 0.11
    d.ellipse(((w / 2 - r) * S, (h / 2 - r) * S, (w / 2 + r) * S, (h / 2 + r) * S), fill=255)
    return finish(im, w, h)

def ears(w=96, h=62, inner=False):
    """Two upright bunny ears leaning slightly outwards (original silhouette)."""
    im = canvas(w, h); S = SS
    for sign in (-1, 1):
        e = Image.new("L", (w * S, h * S), 0); d = ImageDraw.Draw(e)
        ew, eh = (w * 0.17, h * 0.92) if not inner else (w * 0.085, h * 0.62)
        cx, cy = w / 2 + sign * w * 0.2, h * (0.5 if not inner else 0.56)
        d.ellipse(((cx - ew) * S, (cy - eh / 2) * S, (cx + ew) * S, (cy + eh / 2) * S), fill=255)
        e = e.rotate(-sign * 14, center=(cx * S, (h * 0.95) * S), resample=Image.BICUBIC)
        im = Image.composite(Image.new("L", im.size, 255), im, e)
    return finish(im, w, h)


# ---------- original full-screen plush face (360 canvas, then cropped) ----------
# Generic plush-bunny features: wide round eyes, pink rounded-triangle nose,
# two-curve Y mouth. Not a branded character (no ω mouth, no hatched blush, no ears).
FACE = 360
EYE_R, EYES_Y, EYES_SPREAD = 28, 152, 82
HI_R, HI_DX, HI_DY = 8, -9, -9
NOSE_X, NOSE_Y, NOSE_W, NOSE_H = 180, 199, 20, 16
MOUTH_Y = 213
BLUSH_SPREAD, BLUSH_Y, BLUSH_EW, BLUSH_EH = 108, 200, 46, 30

def _bezier(p0, p1, p2, n=50):
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0]
        y = u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]
        pts.append((x, y))
    return pts

def crop_mask(im, pad=4):
    bbox = im.getbbox()
    if not bbox:
        return im, 0, 0
    x0 = max(0, bbox[0] - pad); y0 = max(0, bbox[1] - pad)
    x1 = min(im.width, bbox[2] + pad); y1 = min(im.height, bbox[3] + pad)
    return im.crop((x0, y0, x1, y1)), x0, y0

def face_blush():
    """Soft gradient ovals. No hatch lines."""
    im = canvas(FACE, FACE); d = ImageDraw.Draw(im); S = SS
    for sign in (-1, 1):
        cx, cy = 180 + sign * BLUSH_SPREAD, BLUSH_Y
        d.ellipse(((cx - BLUSH_EW) * S, (cy - BLUSH_EH) * S, (cx + BLUSH_EW) * S, (cy + BLUSH_EH) * S), fill=255)
    im = im.filter(ImageFilter.GaussianBlur(radius=7 * S / 2))
    im = finish(im, FACE, FACE).point(lambda v: int(v * 0.55))
    return crop_mask(im, pad=8)

def face_eyes():
    """Two large round eyes, set wide apart."""
    im = canvas(FACE, FACE); d = ImageDraw.Draw(im); S = SS
    for sign in (-1, 1):
        cx, cy = 180 + sign * EYES_SPREAD, EYES_Y
        d.ellipse(((cx - EYE_R) * S, (cy - EYE_R) * S, (cx + EYE_R) * S, (cy + EYE_R) * S), fill=255)
    return crop_mask(finish(im, FACE, FACE))

def face_hi():
    """White highlight dots, upper-left of each eye."""
    im = canvas(FACE, FACE); d = ImageDraw.Draw(im); S = SS
    for sign in (-1, 1):
        cx, cy = 180 + sign * EYES_SPREAD + HI_DX, EYES_Y + HI_DY
        d.ellipse(((cx - HI_R) * S, (cy - HI_R) * S, (cx + HI_R) * S, (cy + HI_R) * S), fill=255)
    return crop_mask(finish(im, FACE, FACE))

def face_nose():
    """Tiny soft pink rounded-triangle nose, pointing down."""
    im = canvas(FACE, FACE); d = ImageDraw.Draw(im); S = SS
    nx, ny, w, h = NOSE_X, NOSE_Y, NOSE_W, NOSE_H
    top, bot, left, right = ny - h / 2, ny + h / 2, nx - w / 2, nx + w / 2
    d.polygon([( (left + 1.5) * S, (top + 5) * S), ( (right - 1.5) * S, (top + 5) * S), (nx * S, bot * S)], fill=255)
    for px, py, r in ((left + 6, top + 5.5, 5.2), (right - 6, top + 5.5, 5.2), (nx, bot - 3.2, 4.2)):
        d.ellipse(((px - r) * S, (py - r) * S, (px + r) * S, (py + r) * S), fill=255)
    d.rounded_rectangle(((left + 3) * S, top * S, (right - 3) * S, (top + 10) * S),
                        radius=max(1, int(5.5 * S)), fill=255)
    im = finish(im.filter(ImageFilter.GaussianBlur(radius=0.7 * S / 2)), FACE, FACE)
    im = im.point(lambda v: min(255, int(v * 1.15)) if v > 8 else 0)
    return crop_mask(im)

def face_mouth():
    """Short stem + two little outward curves (soft Y). Not an omega / cat mouth."""
    im = canvas(FACE, FACE); d = ImageDraw.Draw(im); S = SS
    rad = 2.15
    nose_bot = NOSE_Y + NOSE_H / 2 - 3  # overlap the nose tip so they join
    sx, sy = 180, MOUTH_Y
    for i in range(28):
        t = i / 27
        x, y = 180, nose_bot + (sy - nose_bot) * t
        d.ellipse(((x - rad) * S, (y - rad) * S, (x + rad) * S, (y + rad) * S), fill=255)
    for sign in (-1, 1):
        for x, y in _bezier((sx, sy), (sx + sign * 8, sy + 5.5), (sx + sign * 15.5, sy + 6.5), 56):
            d.ellipse(((x - rad) * S, (y - rad) * S, (x + rad) * S, (y + rad) * S), fill=255)
    return crop_mask(finish(im, FACE, FACE))

def render_face_rgb(scale=1):
    """Full cream face for previews, drawn natively at the given scale (1=360, 2=720)."""
    cream, eye, white = (0xFF, 0xF6, 0xE3), (0x1A, 0x1A, 0x1A), (255, 255, 255)
    nose_c, mouth_c, blush_c = (0xF4, 0xA0, 0xB0), (0x8A, 0x6E, 0x79), (0xF4, 0xB6, 0xC8)
    S = FACE * scale
    W = S * SS
    im = Image.new("RGB", (W, W), cream)
    def X(x): return x * scale * SS
    def Y(y): return y * scale * SS
    def paste(col, mask):
        im.paste(Image.new("RGB", (W, W), col), (0, 0), mask)

    m = Image.new("L", (W, W), 0); d = ImageDraw.Draw(m)
    for sign in (-1, 1):
        cx, cy = 180 + sign * BLUSH_SPREAD, BLUSH_Y
        d.ellipse((X(cx - BLUSH_EW), Y(cy - BLUSH_EH), X(cx + BLUSH_EW), Y(cy + BLUSH_EH)), fill=255)
    m = m.filter(ImageFilter.GaussianBlur(radius=7 * scale * SS / 2)).point(lambda v: int(v * 0.55))
    paste(blush_c, m)

    m = Image.new("L", (W, W), 0); d = ImageDraw.Draw(m)
    for sign in (-1, 1):
        cx, cy = 180 + sign * EYES_SPREAD, EYES_Y
        d.ellipse((X(cx - EYE_R), Y(cy - EYE_R), X(cx + EYE_R), Y(cy + EYE_R)), fill=255)
    paste(eye, m)

    m = Image.new("L", (W, W), 0); d = ImageDraw.Draw(m)
    for sign in (-1, 1):
        cx, cy = 180 + sign * EYES_SPREAD + HI_DX, EYES_Y + HI_DY
        d.ellipse((X(cx - HI_R), Y(cy - HI_R), X(cx + HI_R), Y(cy + HI_R)), fill=255)
    paste(white, m)

    m = Image.new("L", (W, W), 0); d = ImageDraw.Draw(m)
    nx, ny, w, h = NOSE_X, NOSE_Y, NOSE_W, NOSE_H
    top, bot, left, right = ny - h / 2, ny + h / 2, nx - w / 2, nx + w / 2
    d.polygon([(X(left + 1.5), Y(top + 5)), (X(right - 1.5), Y(top + 5)), (X(nx), Y(bot))], fill=255)
    for px, py, r in ((left + 6, top + 5.5, 5.2), (right - 6, top + 5.5, 5.2), (nx, bot - 3.2, 4.2)):
        d.ellipse((X(px - r), Y(py - r), X(px + r), Y(py + r)), fill=255)
    d.rounded_rectangle((X(left + 3), Y(top), X(right - 3), Y(top + 10)),
                        radius=max(1, int(5.5 * scale * SS)), fill=255)
    m = m.filter(ImageFilter.GaussianBlur(radius=0.7 * scale * SS / 2))
    m = m.point(lambda v: min(255, int(v * 1.15)) if v > 8 else 0)
    paste(nose_c, m)

    m = Image.new("L", (W, W), 0); d = ImageDraw.Draw(m)
    rad = 2.15
    nose_bot = NOSE_Y + NOSE_H / 2 - 3
    sx, sy = 180, MOUTH_Y
    for i in range(28):
        t = i / 27
        x, y = 180, nose_bot + (sy - nose_bot) * t
        d.ellipse((X(x - rad), Y(y - rad), X(x + rad), Y(y + rad)), fill=255)
    for sign in (-1, 1):
        for x, y in _bezier((sx, sy), (sx + sign * 8, sy + 5.5), (sx + sign * 15.5, sy + 6.5), 56):
            d.ellipse((X(x - rad), Y(y - rad), X(x + rad), Y(y + rad)), fill=255)
    paste(mouth_c, m)
    return im.resize((S, S), Image.Resampling.LANCZOS)

ART = {
    "heart": heart(), "heart_s": heart(22, 20), "bow": bow_wings(), "bow_knot": bow_knot(),
    "flower": flower(), "flower_core": flower_core(), "flower_s": flower(20, 20),
    "ears": ears(), "ears_in": ears(inner=True), "heart_big": heart(78, 71),
    "face_eyes": face_eyes(), "face_hi": face_hi(), "face_blush": face_blush(),
    "face_nose": face_nose(), "face_mouth": face_mouth(),
}

def write_art():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "..", "puck_home", "art.h")
    art_dir = os.path.join(here, "..", "mock", "art")
    os.makedirs(art_dir, exist_ok=True)
    pos = {}
    with open(out, "w") as f:
        f.write("// GENERATED by tools/gen_art.py. Do not edit by hand. Original doodles, 8-bit alpha masks (recoloured in theme.h).\n#pragma once\n#include <lvgl.h>\n")
        for n, item in ART.items():
            if isinstance(item, tuple):
                im, x, y = item
                pos[n] = [x, y]
                f.write(f"#define ART_{n}_X {x}\n#define ART_{n}_Y {y}\n")
            else:
                im = item
            im.save(os.path.join(art_dir, n + ".png"))
            w, h = im.size
            data = ",".join(str(v) for v in im.tobytes())
            f.write(f"static const uint8_t art_{n}_map[] = {{{data}}};\n")
            f.write(f"static const lv_img_dsc_t art_{n} = {{ {{LV_IMG_CF_ALPHA_8BIT, 0, 0, {w}, {h}}}, {w*h}, art_{n}_map }};\n")
    with open(os.path.join(art_dir, "pos.json"), "w") as jf:
        json.dump(pos, jf)
    print("wrote", out, os.path.getsize(out), "bytes of source")

if __name__ == "__main__":
    write_art()
