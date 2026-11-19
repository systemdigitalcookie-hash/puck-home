#!/usr/bin/env python3
"""Approximate preview of the pink theme (NOT a real screenshot). Output: mock/pink_v3.png (gitignored)."""
from PIL import Image, ImageDraw, ImageFont
import glob, math, os
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
fs = glob.glob('/usr/share/fonts/**/DejaVuSans*.ttf', recursive=True)
def F(s, b=False):
    p = [f for f in fs if (('Bold' in f) == b and 'Oblique' not in f and 'Mono' not in f and 'Serif' not in f)]
    return ImageFont.truetype(p[0], s)
def H(x): return tuple(int(x[i:i+2], 16) for i in (1, 3, 5))
BG, SURF, CARD, TEXT, GREY, DIV, LINE, ACC, HOT, BUTTER = map(H, ["#FFF0F5","#FFE4EC","#FFFBF7","#6D4C5B","#A98A97","#F8CCDB","#F4B6CB","#F48FB1","#EC407A","#FFE082"])
S = 360
def art(im, name, col, x, y, cx=False):
    m = Image.open(f"{R}/mock/art/{name}.png"); 
    if cx: x = x - m.width // 2
    im.paste(Image.new("RGB", m.size, col), (x, y), m)
def c(d, t, y, f, fill=TEXT, x=S/2): d.text((x, y), t, font=f, fill=fill, anchor="mm")
def shadow_btn(im, box, r, fill=CARD, border=ACC):
    sh = Image.new("RGBA", im.size, (0,0,0,0)); sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle((box[0], box[1]+3, box[2], box[3]+3), r, fill=ACC+(60,))
    from PIL import ImageFilter
    sh = sh.filter(ImageFilter.GaussianBlur(4)); im.paste(sh, (0,0), sh)
    ImageDraw.Draw(im).rounded_rectangle(box, r, fill=fill, outline=border, width=2)
def bar(d, x0, x1, y, frac, h=8):
    d.rounded_rectangle((x0, y, x1, y+h), h//2, fill=SURF)
    if frac > 0: d.rounded_rectangle((x0, y, x0+(x1-x0)*frac, y+h), h//2, fill=ACC)
def base():
    im = Image.new("RGB", (S, S), BG); return im
def ring(im):
    d = ImageDraw.Draw(im)
    for i in range(64):
        a = i * 2 * math.pi / 64; x = 180 + 169*math.cos(a); y = 180 + 169*math.sin(a)
        d.ellipse((x-2+0, y-2, x+2, y+2), fill=ACC if i % 4 == 0 else LINE)
def dots(im, active):
    d = ImageDraw.Draw(im)
    for i in range(3):
        x = 180 + (i-1)*14; d.ellipse((x-3, 335, x+3, 341), fill=HOT if i == active else LINE)
def heart_s(im, x, y): art(im, "heart_s", ACC, x - 11, y)
def clock():
    im = base(); d = ImageDraw.Draw(im)
    art(im, "ears", ACC, 180, 22, True); art(im, "ears_in", SURF, 180, 22, True)
    c(d, "21:41", 132, F(60, True)); c(d, "Thu, 19 Nov 2026", 174, F(14), GREY)
    heart_s(im, 80, 168); heart_s(im, 280, 168)
    c(d, "Wi-Fi: Home-5G", 206, F(14)); c(d, "Up to date", 228, F(14), GREY)
    shadow_btn(im, (105, 252, 255, 292), 22); c(ImageDraw.Draw(im), "Check update", 272, F(14))
    for x in (82, 278):
        art(im, "flower_s", ACC, x-10, 262)
    c(ImageDraw.Draw(im), "v3", 318, F(11), GREY)
    dots(im, 1); ring(im); return im
def rec_ready():
    im = base(); d = ImageDraw.Draw(im)
    c(d, "Recorder", 44, F(20, True))
    art(im, "heart_big", HOT, 180-39, 54); d.ellipse((168, 76, 192, 100), fill=CARD)
    c(d, "Tap the heart to record", 138, F(11), GREY)
    d.line((62, 150, 298, 150), fill=DIV, width=2)
    for i, (n, t) in enumerate([("Recording 3", "0:12"), ("Recording 2", "0:41"), ("Recording 1", "1:05")]):
        y = 172 + i*34; d.text((72, y), n, font=F(14), fill=TEXT, anchor="lm"); d.text((270, y), t, font=F(12), fill=GREY, anchor="rm")
        d.line((62, y+17, 298, y+17), fill=DIV)
    dots(im, 0); ring(im); return im
def rec_on():
    im = base(); d = ImageDraw.Draw(im)
    d.ellipse((165, 22, 175, 32), fill=HOT); d.text((184, 27), "Recording", font=F(16), fill=TEXT, anchor="lm")
    d.ellipse((128, 72, 232, 176), fill=SURF, outline=ACC, width=3); c(d, "\u266A", 124, F(48), HOT)
    art(im, "bow", ACC, 180+30-22, 60); art(im, "bow_knot", HOT, 180+30-22, 60)
    c(d, "00:23", 220, F(48, True)); bar(d, 110, 250, 252, .55)
    for x, t, col in ((-58, "II  Pause", TEXT), (58, "\u25A0  Stop", HOT)):
        shadow_btn(im, (180+x-52, 270, 180+x+52, 312), 21); c(ImageDraw.Draw(im), t, 291, F(14), col, 180+x)
    ring(im); return im
def settings():
    im = base(); d = ImageDraw.Draw(im); c(d, "Settings", 44, F(20, True))
    for i, (t, v) in enumerate([("Sound", "Vol 70%"), ("Microphone", "Gain 12"), ("Display", "Bright 80"), ("Storage", "SD 12 GB")]):
        y = 96 + i*44; d.line((62, y+20, 298, y+20), fill=DIV, width=1)
        d.text((74, y), t, font=F(16), fill=TEXT, anchor="lm"); d.text((284, y), v, font=F(12), fill=GREY, anchor="rm")
    d.rounded_rectangle((70, 276, 290, 306), 14, fill=SURF); c(d, "Wi-Fi  \u2022  Battery  \u2022  About", 291, F(12), GREY)
    art(im, "flower_s", ACC, 20, 300) if False else None
    dots(im, 2); ring(im); return im
def settings_sound():
    im = base(); d = ImageDraw.Draw(im); c(d, "Sound", 44, F(20, True))
    d.ellipse((46, 24, 82, 60), fill=CARD, outline=ACC, width=2); c(d, "<", 42, F(16), TEXT, 64)
    c(d, "Volume", 110, F(14), GREY); bar(d, 75, 285, 130, .7)
    d.ellipse((75+210*.7-9, 125, 75+210*.7+9, 143), fill=CARD, outline=HOT, width=3)
    shadow_btn(im, (105, 180, 255, 220), 18); c(ImageDraw.Draw(im), "Test speaker", 200, F(14))
    ring(im); return im
def wifi():
    im = base(); d = ImageDraw.Draw(im); c(d, "Wi-Fi", 44, F(20, True))
    for i, (n, b, lock, cur) in enumerate([("Home-5G", 4, 1, 1), ("Neighbour", 2, 1, 0), ("CafeFree", 1, 0, 0), ("Office", 3, 1, 0)]):
        y = 92 + i*44; d.line((62, y+22, 298, y+22), fill=DIV)
        d.text((74, y), n, font=F(15), fill=TEXT, anchor="lm")
        if cur: d.text((74+len(n)*9+10, y), "connected", font=F(10), fill=HOT, anchor="lm")
        for k in range(4):
            h = 4 + k*3; d.rounded_rectangle((246+k*5, y+6-h, 249+k*5, y+6), 1, fill=HOT if k < b else DIV)
        if lock: d.rounded_rectangle((276, y-1, 284, y+7), 2, outline=GREY, width=2)
    shadow_btn(im, (115, 276, 245, 312), 18); c(ImageDraw.Draw(im), "Scan again", 294, F(14))
    ring(im); return im
pages = [("Clock", clock()), ("Recorder", rec_ready()), ("Recording", rec_on()), ("Settings", settings()), ("Sound", settings_sound()), ("Wi-Fi", wifi())]
P = 14; W = 3*(S+2*P)+P; Hh = 2*(S+2*P+26)+P
out = Image.new("RGB", (W, Hh), (252, 244, 247)); od = ImageDraw.Draw(out)
for i, (n, im) in enumerate(pages):
    m = Image.new("L", (S, S), 0); ImageDraw.Draw(m).ellipse((0,0,S-1,S-1), fill=255)
    cx = P + (i % 3)*(S+2*P); cy = P + (i // 3)*(S+2*P+26)
    od.ellipse((cx-6, cy-6, cx+S+6, cy+S+6), fill=(60, 40, 48)); out.paste(im, (cx, cy), m)
    od.text((cx+S/2, cy+S+20), n, font=F(14), fill=TEXT, anchor="mm")
p = os.path.join(R, "mock", "pink_v3.png"); out.save(p); print(p, out.size, os.path.getsize(p))
