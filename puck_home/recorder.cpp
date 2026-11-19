// Meeting recorder: ES7210 dual mic (I2S, 16 kHz) -> mono 16-bit WAV on microSD or internal FAT.
#include "recorder.h"
#include "ESP_I2S.h"
#include "es7210.h"
#include "es8311.h"
#include <algorithm>
#include "FS.h"
#include "SD_MMC.h"
#include "FFat.h"
#include "Wire.h"
#include "esp_heap_caps.h"
#include <errno.h>
#include <stdarg.h>

#define PIN_MCK GPIO_NUM_2
#define PIN_BCK GPIO_NUM_48
#define PIN_WS  GPIO_NUM_38
#define PIN_DIN GPIO_NUM_39
#define PIN_DOUT GPIO_NUM_47
#define SR 16000
#define BYTES_PER_SEC (SR * 2)
#define RING_SZ (256 * 1024)
#define CHUNK_FRAMES 512

extern int es7210_last_wire_err, es7210_last_reg;
RecInfo rec;
volatile int g_spk_volume = 85;   // 0..100 -> ES8311 DAC -30..+12 dB (Settings > Sound)
volatile int g_mic_gain = 42;     // total mic gain dB 0..60 = PGA (<=33 dB) + ES7210 digital volume (Settings > Microphone)
static uint8_t es_addr = 0x40;
static I2SClass i2s;
static es7210_dev_handle_t es_h = NULL;
static fs::FS *store = nullptr;
static bool sd_ok = false, ffat_ok = false, codec_ok = false;
static uint8_t *ring = nullptr;
static volatile uint32_t head = 0, tail = 0;     // monotonically increasing byte counters
static volatile bool stop_req = false, cap_done = true;
static File wf;
static uint32_t data_bytes = 0;
static uint32_t start_free_bytes = 0;

// soft limiter: linear up to 24000, then smoothly compresses towards 32767 (no hard clipping)
static inline int soft_limit(int x) {
  const int K = 24000;
  int a = x < 0 ? -x : x;
  if (a <= K) return x;
  int over = a - K;                                  // 0..~41000 for a 2x sum
  int y = K + (int)(8767.0f * over / (over + 8767.0f));
  return x < 0 ? -y : y;
}

static void put_msg(const char *m) { strncpy(rec.msg, m, sizeof rec.msg - 1); rec.msg[sizeof rec.msg - 1] = 0; }

static void wav_header(uint8_t *h, uint32_t data_len) {
  uint32_t riff = 36 + data_len, sr = SR, br = BYTES_PER_SEC, fmtlen = 16;
  uint16_t fmt = 1, ch = 1, ba = 2, bps = 16;
  memcpy(h, "RIFF", 4); memcpy(h + 4, &riff, 4); memcpy(h + 8, "WAVEfmt ", 8);
  memcpy(h + 16, &fmtlen, 4); memcpy(h + 20, &fmt, 2); memcpy(h + 22, &ch, 2);
  memcpy(h + 24, &sr, 4); memcpy(h + 28, &br, 4); memcpy(h + 32, &ba, 2); memcpy(h + 34, &bps, 2);
  memcpy(h + 36, "data", 4); memcpy(h + 40, &data_len, 4);
}

volatile bool rec_writer_alive = false;   // writer task still running (save in progress or stuck)
static char cur_path[40];
static fs::FS *wstore = nullptr;          // storage the current file lives on (fixed for the whole recording)
static char fail_step[64];                // first failing save step, with errno / byte counts (shown on screen)
static void fail(const char *fmt, ...) {
  if (fail_step[0]) return;
  va_list ap; va_start(ap, fmt); vsnprintf(fail_step, sizeof fail_step, fmt, ap); va_end(ap);
  Serial.printf("[rec] FAIL %s\n", fail_step);
}
// Header rewrite. v11: seek back to the END (not a remembered position) and report failures: an unchecked failed
// seek-back would have written audio over the start of the file.
static bool rewrite_header(uint32_t n, bool final_ = false) {
  uint8_t h[44]; wav_header(h, n);
  errno = 0;
  if (!wf.seek(0)) { if (final_) fail("header seek, errno %d", errno); return false; }
  if (wf.write(h, 44) != 44) { if (final_) fail("header write, errno %d", errno); return false; }
  if (final_) return true;                 // followed by close(), which flushes
  wf.flush();
  if (!wf.seek(0, SeekEnd)) { fail("seek to end, errno %d", errno); return false; }
  return true;
}

static void scan_files(int *count, int *maxidx, char *lastname) {
  *count = 0; *maxidx = 0; if (lastname) lastname[0] = 0;
  if (!store) return;
  File d = store->open("/rec");
  if (!d || !d.isDirectory()) return;
  File f = d.openNextFile();
  while (f) {
    const char *nm = f.name();
    int idx;
    if (!f.isDirectory() && sscanf(nm, "rec_%d.wav", &idx) == 1) {
      (*count)++;
      if (idx >= *maxidx) { *maxidx = idx; if (lastname) strncpy(lastname, nm, 23), lastname[23] = 0; }
    }
    f = d.openNextFile();
  }
}

// v16: free/total space is cached here; the UI (Settings) reads the cache instead of hitting the SD card (f_getfree can be slow)
static volatile uint64_t g_free_cache = 0, g_total_cache = 0;
static uint32_t free_bytes() {
  uint64_t f = 0, t = 0;
  if (store == &SD_MMC) { t = SD_MMC.totalBytes(); f = t - SD_MMC.usedBytes(); }
  else if (store == &FFat) { t = FFat.totalBytes(); f = FFat.freeBytes(); }
  g_free_cache = f; g_total_cache = t;
  return (uint32_t)min<uint64_t>(f, 0xFFFFFFFFULL);
}

static bool sd_failed = false, sd_fellback = false;   // sd_failed: a write test / real write failed -> internal flash until reboot
// 8 KB write + read-back through an internal, 4-byte-aligned DMA buffer (never PSRAM)
static bool sd_selftest(int *err) {
  uint8_t *b = (uint8_t *)heap_caps_aligned_alloc(4, 8192, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!b) { *err = ENOMEM; return false; }
  for (int i = 0; i < 8192; i++) b[i] = (uint8_t)(i * 7 + 3);
  bool ok = false; errno = 0;
  File f = SD_MMC.open("/_selftest.bin", FILE_WRITE);
  if (f) { ok = f.write(b, 8192) == 8192; *err = errno; f.close(); } else *err = errno ? errno : EIO;
  if (ok) {
    memset(b, 0, 8192); errno = 0;
    File r = SD_MMC.open("/_selftest.bin", FILE_READ);
    ok = r && r.read(b, 8192) == 8192; *err = errno;
    if (r) r.close();
    for (int i = 0; ok && i < 8192; i++) if (b[i] != (uint8_t)(i * 7 + 3)) ok = false;
  }
  SD_MMC.remove("/_selftest.bin");
  heap_caps_free(b);
  return ok;
}
static bool sd_mount_try() {   // 4-bit 20 MHz -> 1-bit 20 -> 10 -> 5 MHz; each must pass the write test
  struct Cfg { bool one; int khz; } cfgs[4] = { {false, 20000}, {true, 20000}, {true, 10000}, {true, 5000} };
  bool pins = SD_MMC.setPins(15, 14, 16, 17, 12, 13);
  if (!pins) { snprintf(rec.sd, sizeof rec.sd, "SD pins error"); return false; }
  int lasterr = 0; bool card = false;
  for (auto &c : cfgs) {
    SD_MMC.end();
    if (!SD_MMC.begin("/sdcard", c.one, false, c.khz) || SD_MMC.cardType() == CARD_NONE) { SD_MMC.end(); continue; }
    card = true;
    int err = 0;
    if (sd_selftest(&err)) { snprintf(rec.sd, sizeof rec.sd, "SD OK (%s %d MHz)", c.one ? "1-bit" : "4-bit", c.khz / 1000); Serial.printf("[sd] %s\n", rec.sd); return true; }
    lasterr = err; Serial.printf("[sd] %s %d kHz failed write test, errno %d\n", c.one ? "1-bit" : "4-bit", c.khz, err);
  }
  SD_MMC.end();
  if (card) { sd_failed = true; snprintf(rec.sd, sizeof rec.sd, "SD error %s, using internal", lasterr == EIO ? "EIO" : (lasterr == EROFS ? "read-only" : "write")); }
  else snprintf(rec.sd, sizeof rec.sd, "No SD card");
  return false;
}

static bool mount_storage() {
  if (!sd_ok && !sd_failed) sd_ok = sd_mount_try();
  if (sd_ok) { store = &SD_MMC; strcpy(rec.storage, "SD"); }
  else {
    if (!ffat_ok) ffat_ok = FFat.begin(true);
    if (ffat_ok) { store = &FFat; strcpy(rec.storage, "Flash"); } else store = nullptr;
  }
  if (!store) { put_msg("no storage"); return false; }
  if (!store->exists("/rec")) store->mkdir("/rec");
  int c, m; scan_files(&c, &m, rec.last);
  rec.count = c;
  rec.free_secs = free_bytes() / BYTES_PER_SEC;
  return true;
}

static void capture_task(void *) {
  static int16_t st[CHUNK_FRAMES * 2];
  static int16_t mono[CHUNK_FRAMES];
  int skip = 4;  // discard first chunks (codec settling)
  while (!stop_req) {
    size_t got = i2s.readBytes((char *)st, sizeof st);
    if (got < sizeof st) { vTaskDelay(1); continue; }   // never spin: would starve the writer on this core
    if (skip > 0) { skip--; continue; }
    int peak = 0, pl = 0, pr = 0;
    for (int i = 0; i < CHUNK_FRAMES; i++) {
      int l = st[2 * i], r = st[2 * i + 1];
      int al = l < 0 ? -l : l, ar = r < 0 ? -r : r; if (al > pl) pl = al; if (ar > pr) pr = ar;
      int m = soft_limit(l + r);   // v10: SUM both mics (v1-v9 averaged them = -6 dB), soft-limited instead of clipping
      mono[i] = (int16_t)m;
      int a = m < 0 ? -m : m; if (a > peak) peak = a;
    }
    { static uint32_t lt = 0; static int mpl = 0, mpr = 0, mo = 0;
      mpl = max(mpl, pl); mpr = max(mpr, pr); mo = max(mo, peak);
      if (millis() - lt > 2000) { lt = millis(); Serial.printf("[mic] peak L=%d R=%d out=%d gain=%ddB\n", mpl, mpr, mo, (int)g_mic_gain); mpl = mpr = mo = 0; } }
    if (rec.state == REC_PAUSED) { rec.level = 0; continue; }   // keep I2S flowing, but write nothing while paused
    rec.level = (int)(sqrtf(peak / 12000.0f) * 100.0f); if (rec.level > 100) rec.level = 100;
    uint32_t n = sizeof mono;
    if (RING_SZ - (head - tail) < n) { rec.dropped += n; continue; }
    uint32_t off = head % RING_SZ, first = min<uint32_t>(n, RING_SZ - off);
    memcpy(ring + off, mono, first); if (n > first) memcpy(ring, (uint8_t *)mono + first, n - first);
    head += n;
  }
  i2s.end();          // the capture task owns I2S: stop it here, never from another task
  rec.level = 0;
  cap_done = true;
  vTaskDelete(NULL);
}

static void writer_task(void *) {
  static uint8_t *blk = NULL; const uint32_t BLK_SZ = 16384;
  if (!blk) blk = (uint8_t *)heap_caps_aligned_alloc(4, BLK_SZ, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);   // DMA-capable internal RAM, never PSRAM
  if (!blk) { fail("no DMA buffer"); stop_req = true; }
  uint32_t last_hdr = 0;
  bool werr = false, hdr_live = true;
  rec_writer_alive = true;
  for (;;) {
    uint32_t avail = head - tail;
    bool finishing = cap_done;
    if (avail >= 8192 || (finishing && avail > 0)) {
      uint32_t n = blk ? min<uint32_t>(avail, BLK_SZ) : avail;
      if (!blk) { tail += n; continue; }
      uint32_t off = tail % RING_SZ, first = min<uint32_t>(n, RING_SZ - off);
      memcpy(blk, ring + off, first); if (n > first) memcpy(blk + first, ring, n - first);
      errno = 0;
      size_t w = wf.write(blk, n);
      if (w != n && wstore == &SD_MMC && data_bytes == 0 && !sd_fellback) {   // SD refused the very first write: move this recording to internal flash
        Serial.printf("[rec] SD write failed (%u/%u, errno %d) -> falling back to internal flash\n", (unsigned)w, (unsigned)n, errno);
        wf.close(); SD_MMC.remove(cur_path);
        if (!ffat_ok) ffat_ok = FFat.begin(true);
        if (ffat_ok) {
          if (!FFat.exists("/rec")) FFat.mkdir("/rec");
          wf = FFat.open(cur_path, FILE_WRITE); uint8_t h[44]; wav_header(h, 0);
          if (wf && wf.write(h, 44) == 44) {
            wstore = &FFat; store = &FFat; strcpy(rec.storage, "Flash"); sd_fellback = true; sd_failed = true; sd_ok = false;
            snprintf(rec.sd, sizeof rec.sd, "SD error EIO, using internal");
            start_free_bytes = (uint32_t)FFat.freeBytes();
            errno = 0; w = wf.write(blk, n);
          }
        }
      }
      if (w != n) { fail("write %u/%u B at %lus, errno %d", (unsigned)w, (unsigned)n, (unsigned long)(data_bytes / BYTES_PER_SEC), errno); werr = true; stop_req = true; }
      tail += n; data_bytes += w;               // v11: count what was really written (was: what was asked)
      rec.secs = data_bytes / BYTES_PER_SEC;
      if (data_bytes + 65536 > start_free_bytes && !stop_req) { put_msg("storage full"); stop_req = true; }
      if (hdr_live && !werr && rec.secs >= last_hdr + 15) {   // crash safety: keep the header roughly current
        last_hdr = rec.secs;
        if (!rewrite_header(data_bytes)) { hdr_live = false; Serial.printf("[rec] live header rewrite failed, errno %d\n", errno); if (fail_step[0]) { werr = true; stop_req = true; } }
      }
      uint32_t used_secs = data_bytes / BYTES_PER_SEC, total = start_free_bytes / BYTES_PER_SEC;
      rec.free_secs = total > used_secs ? total - used_secs : 0;
    } else if (finishing) {
      break;
    } else vTaskDelay(pdMS_TO_TICKS(10));
  }
  // finalize (runs here, never on the UI task); each stage is shown on screen so a stall can be located
  put_msg("saving: header");
  bool hdr_ok = rewrite_header(data_bytes, true);
  put_msg("saving: close");
  wf.close();
  put_msg("saving: check");
  uint32_t expect = data_bytes + 44, sz = 0; bool riff = false;
  errno = 0;
  File chk = wstore->open(cur_path, FILE_READ);
  if (!chk) fail("check: reopen %s, errno %d", cur_path + 5, errno);
  else {
    sz = chk.size(); uint8_t h[12] = {0};
    riff = chk.read(h, 12) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4);
    chk.close();
  }
  Serial.printf("[rec] save %s: expected %lu B, file %lu B, header %s, riff %d, werr %d\n", cur_path,
                (unsigned long)expect, (unsigned long)sz, hdr_ok ? "ok" : "FAILED", riff, werr);
  bool kept = chk && sz > 44;
  // the file is the truth: if it holds audio but the header disagrees (or never got written), repair the header in place
  if (kept && (!hdr_ok || !riff || sz != expect)) {
    uint32_t dl = (sz - 44) & ~1u;
    File fx = wstore->open(cur_path, "r+");
    uint8_t h[44]; wav_header(h, dl);
    bool fixed = fx && fx.seek(0) && fx.write(h, 44) == 44;
    if (fx) fx.close();
    Serial.printf("[rec] header repaired to %lu B data: %s\n", (unsigned long)dl, fixed ? "ok" : "FAILED");
    if (!fixed) fail("header repair (expected %lu B, file %lu B)", (unsigned long)expect, (unsigned long)sz);
    else if (!werr) fail_step[0] = 0;   // size/header mismatch only: file is valid now, not a failure
  }
  if (chk && sz <= 44 && data_bytes > 0) fail("check: file %lu B, expected %lu B", (unsigned long)sz, (unsigned long)expect);
  int c, m; scan_files(&c, &m, rec.last);
  rec.count = c;
  if (!fail_step[0]) put_msg(sd_fellback ? "Saved to internal (SD error)" : "saved");
  else {
    char b[sizeof rec.msg]; snprintf(b, sizeof b, "Save failed: %s%s", fail_step, kept ? " (partial file kept)" : "");
    put_msg(b);
  }
  rec.free_secs = free_bytes() / BYTES_PER_SEC;
  rec.state = REC_IDLE;
  rec_writer_alive = false;
  vTaskDelete(NULL);
}

static void i2c_scan() {
  char *p = rec.i2c; size_t left = sizeof rec.i2c;
  int n = snprintf(p, left, "I2C:"); p += n; left -= n;
  for (int a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0 && left > 5) { n = snprintf(p, left, " %02X", a); p += n; left -= n; }
  }
}

static bool addr_present(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

// NOTE: es7210_config_volume() only accepts -95..32 dB; v2/v3 passed 40 (copied from Waveshare's sample) -> ESP_ERR_INVALID_ARG
// with no I2C error, shown as "es7210 cfg err". Now 0 dB (analog gain is set separately to 30 dB).
// Configure ES7210 at es_addr. Call with I2S already running so MCLK is present (as in Waveshare's apps).
static bool codec_configure(char *err, size_t en) {
  if (es_h) { es7210_del_codec(es_h); es_h = NULL; }
  es7210_i2c_config_t cfg = {}; cfg.i2c_addr = es_addr;
  es7210_last_wire_err = 0; es7210_last_reg = -1;
  if (es7210_new_codec(&cfg, &es_h) != ESP_OK) { snprintf(err, en, "es7210 alloc fail"); return false; }
  es7210_codec_config_t cc = {};
  cc.i2s_format = ES7210_I2S_FMT_I2S; cc.mclk_ratio = 256; cc.sample_rate_hz = SR;
  cc.bit_width = ES7210_I2S_BITS_16B; cc.mic_bias = ES7210_MIC_BIAS_2V87; int tg = constrain((int)g_mic_gain, 0, 60), pga = min(tg, 33) / 3;   // PGA index 0..11 = 0..33 dB
  cc.mic_gain = (es7210_mic_gain_t)pga;
  cc.flags.tdm_enable = false;
  if (es7210_config_codec(es_h, &cc) != ESP_OK || es7210_config_volume(es_h, (int8_t)(tg - pga * 3)) != ESP_OK) {
    if (es7210_last_wire_err) snprintf(err, en, "i2c e%d @0x%02X reg 0x%02X", es7210_last_wire_err, es_addr, es7210_last_reg);
    else snprintf(err, en, "es7210 cfg err @0x%02X", es_addr);
    return false;
  }
  return true;
}

static bool i2s_open() {
  i2s.setPins(PIN_BCK, PIN_WS, PIN_DOUT, PIN_DIN, PIN_MCK);
  i2s.setTimeout(1000);
  return i2s.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
}

// Open I2S (MCLK on), configure codec; tries addresses 0x40-0x43 that answer on the bus.
static bool mic_open(char *err, size_t en) {
  if (!i2s_open()) { snprintf(err, en, "i2s begin failed"); return false; }
  delay(30);
  bool found = false;
  for (uint8_t a = 0x40; a <= 0x43; a++) {
    if (!addr_present(a)) continue;
    found = true; es_addr = a;
    if (codec_configure(err, en)) { delay(20); return true; }
  }
  if (!found) snprintf(err, en, "no ES7210 at 0x40-0x43");
  i2s.end();
  return false;
}

bool rec_init() {
  memset((void *)&rec, 0, sizeof rec);
  strcpy(rec.mic, "mic: not tested");
  i2c_scan();
  ring = (uint8_t *)heap_caps_malloc(RING_SZ, MALLOC_CAP_SPIRAM);
  if (!ring) { put_msg("no PSRAM"); snprintf(rec.mic, sizeof rec.mic, "mic: no PSRAM"); return false; }
  char err[48];
  if (mic_open(err, sizeof err)) {
    // self-test: read ~0.25 s and report level so we know audio really flows
    static int16_t t[CHUNK_FRAMES * 2]; int peak = 0, ok = 0;
    for (int i = 0; i < 12; i++) {
      size_t g = i2s.readBytes((char *)t, sizeof t);
      if (g == sizeof t) { ok++; if (i >= 4) for (int k = 0; k < CHUNK_FRAMES * 2; k++) { int a = abs((int)t[k]); if (a > peak) peak = a; } }
    }
    i2s.end();
    codec_ok = ok >= 8;
    if (codec_ok) snprintf(rec.mic, sizeof rec.mic, "mic: ok @0x%02X peak %d", es_addr, peak);
    else snprintf(rec.mic, sizeof rec.mic, "mic: i2s read timeout @0x%02X (%d/12)", es_addr, ok);
  } else snprintf(rec.mic, sizeof rec.mic, "mic: %s", err);
  bool ok = mount_storage();
  if (ok && codec_ok) put_msg("ready"); else if (!codec_ok) put_msg("mic problem");
  return ok && codec_ok;
}

void rec_start() {
  if (rec.state != REC_IDLE || !ring) return;
  if (rec_writer_alive) { put_msg("previous save stuck - restart puck"); return; }
  if (rec_playing) { put_msg("stop playback first"); return; }
  if (!mount_storage()) return;
  int c, maxidx; scan_files(&c, &maxidx, nullptr);
  char path[40]; int idx = maxidx + 1;
  do snprintf(path, sizeof path, "/rec/rec_%04d.wav", idx++); while (store->exists(path) && idx < 10000);   // never overwrite
  strcpy(cur_path, path); wstore = store; fail_step[0] = 0; sd_fellback = false;
  start_free_bytes = free_bytes();
  if (start_free_bytes < 200000) { put_msg("storage full"); return; }
  wf = store->open(path, FILE_WRITE);
  errno = 0;
  if (!wf) { char b[sizeof rec.msg]; snprintf(b, sizeof b, "Can't create %s (errno %d)", path + 5, errno); put_msg(b); return; }
  uint8_t h[44]; wav_header(h, 0);
  if (wf.write(h, 44) != 44) { char b[sizeof rec.msg]; snprintf(b, sizeof b, "Can't write %s (errno %d)", path + 5, errno); put_msg(b); wf.close(); store->remove(path); return; }
  data_bytes = 0; head = tail = 0; rec.dropped = 0; rec.secs = 0; rec.level = 0;
  char err[48];
  if (!mic_open(err, sizeof err)) {
    snprintf(rec.mic, sizeof rec.mic, "mic: %s", err);
    put_msg("mic start failed"); wf.close(); store->remove(path); return;
  }
  stop_req = false; cap_done = false;
  put_msg("recording");
  rec.state = REC_RECORDING;
  // capture on core 0 (high prio, short bursts); writer on core 1 at low prio so slow flash/SD writes never starve capture
  xTaskCreatePinnedToCore(writer_task, "wr", 8192, NULL, 2, NULL, 0);   // v16: core 0 (was core 1, sharing time with the UI loop)
  xTaskCreatePinnedToCore(capture_task, "cap", 6144, NULL, 5, NULL, 0);
}

void rec_stop() {   // non-blocking: the writer task finalizes and sets REC_IDLE
  if (rec.state != REC_RECORDING && rec.state != REC_PAUSED) return;
  put_msg("saving...");
  rec.state = REC_FINALIZING;
  stop_req = true;
}

void rec_pause() { if (rec.state == REC_RECORDING) { rec.state = REC_PAUSED; put_msg("paused"); } }
void rec_resume() { if (rec.state == REC_PAUSED) { rec.state = REC_RECORDING; put_msg("recording"); } }

void rec_force_idle(const char *why) {   // UI watchdog: give the screen back even if the writer is stuck
  if (rec.state == REC_IDLE) return;
  stop_req = true;
  put_msg(why);
  rec.state = REC_IDLE;
}

// ---------------- recordings list / delete ----------------
int rec_list(RecFile *out, int max) {
  int n = 0;
  if (!store || rec.state != REC_IDLE) return 0;
  File d = store->open("/rec");
  if (!d || !d.isDirectory()) return 0;
  File f = d.openNextFile();
  while (f && n < max) {
    int idx;
    if (!f.isDirectory() && sscanf(f.name(), "rec_%d.wav", &idx) == 1) {
      strncpy(out[n].name, f.name(), 23); out[n].name[23] = 0; out[n].bytes = f.size(); out[n].t = f.getLastWrite(); n++;
    }
    f = d.openNextFile();
  }
  std::sort(out, out + n, [](const RecFile &a, const RecFile &b) { return strcmp(a.name, b.name) > 0; });
  return n;
}

bool rec_delete(const char *name) {
  if (!store || rec.state != REC_IDLE || rec_playing) return false;
  char p[40]; snprintf(p, sizeof p, "/rec/%s", name);
  bool ok = store->remove(p);
  int c, m; scan_files(&c, &m, rec.last); rec.count = c;
  rec.free_secs = free_bytes() / BYTES_PER_SEC;
  put_msg(ok ? "deleted" : "delete failed");
  return ok;
}

// ---------------- playback (ES8311 DAC + speaker amp on GPIO9) ----------------
#define PIN_PA 9
volatile bool rec_playing = false;
volatile bool rec_play_paused = false;
volatile uint32_t rec_play_pos = 0, rec_play_total = 0;   // bytes of PCM played / total
static volatile bool play_stop = false;
static File pf;
static es8311_handle_t dac_h = NULL;

static float play_gain = 1.0f;
// Volume 0..100 -> ES8311 DAC register 0x32 (0xBF = 0 dB, 0.5 dB/step). 0 = mute, 1..100 = -30..+12 dB.
// (v1-v9 used es8311_voice_volume_set(70/80) = -6.5 / +6 dB.)
static esp_err_t spk_volume_apply() {
  int v = constrain((int)g_spk_volume, 0, 100);
  int reg = v == 0 ? 0 : 0xBF + (int)lroundf(2.0f * (-30.0f + 42.0f * v / 100.0f));
  return es8311_write_reg_pub(dac_h, 0x32, (uint8_t)constrain(reg, 0, 255));
}
static void play_task(void *) {
  static int16_t buf[2048];
  int pk = 0; uint32_t lt = millis();
  while (!play_stop) {
    if (rec_play_paused) { memset(buf, 0, sizeof buf); i2s.write((uint8_t *)buf, 512); continue; }   // feed silence (DMA would loop old data)
    int n = pf.read((uint8_t *)buf, sizeof buf) / 2;
    if (n <= 0) break;
    for (int i = 0; i < n; i++) {
      int v = (int)(buf[i] * play_gain);
      int a = v < 0 ? -v : v;
      if (a > 30000) { play_gain *= 30000.0f / a; v = (int)(buf[i] * play_gain); }   // limiter: never clip, only back off
      buf[i] = (int16_t)v; if (a > pk) pk = a;
    }
    i2s.write((uint8_t *)buf, n * 2);
    rec_play_pos += n * 2;
    if (millis() - lt > 2000) { lt = millis(); Serial.printf("[play] peak=%d gain=%.2f vol=%d\n", pk, play_gain, (int)g_spk_volume); pk = 0; }
  }
  delay(120);                       // let DMA drain
  digitalWrite(PIN_PA, LOW);
  pf.close();
  i2s.end();
  rec_playing = false;
  vTaskDelete(NULL);
}

void rec_play(const char *name) {
  if (rec.state != REC_IDLE || rec_playing || !store) return;
  char p[40]; snprintf(p, sizeof p, "/rec/%s", name);
  pf = store->open(p, FILE_READ);
  if (!pf) { put_msg("cannot open file"); return; }
  uint8_t h[44];
  if (pf.read(h, 44) != 44 || memcmp(h, "RIFF", 4)) { pf.close(); put_msg("bad wav"); return; }
  // auto playback gain from the first ~2 s (quiet old recordings get up to +18 dB; limiter in play_task prevents clipping)
  {
    static int16_t pre[4096]; int pk = 1;
    size_t pos = pf.position();
    for (int k = 0; k < 8; k++) { int n = pf.read((uint8_t *)pre, sizeof pre) / 2; if (n <= 0) break; for (int i = 0; i < n; i++) { int a = abs((int)pre[i]); if (a > pk) pk = a; } }
    pf.seek(pos);
    play_gain = constrain(22000.0f / pk, 1.0f, 8.0f);
    Serial.printf("[play] %s first-2s peak=%d -> gain %.2f\n", name, pk, play_gain);
  }
  pinMode(PIN_PA, OUTPUT); digitalWrite(PIN_PA, LOW);
  i2s.setPins(PIN_BCK, PIN_WS, PIN_DOUT, PIN_DIN, PIN_MCK);
  i2s.setTimeout(1000);
  if (!i2s.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    pf.close(); put_msg("play: i2s failed"); return;
  }
  // codec setup runs here (UI task) so I2C is never used from two tasks at once
  if (!dac_h) dac_h = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  const es8311_clock_config_t clk = { .mclk_inverted = false, .sclk_inverted = false, .mclk_from_mclk_pin = true,
                                      .mclk_frequency = SR * 256, .sample_frequency = SR };
  if (!dac_h || es8311_init(dac_h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK ||
      spk_volume_apply() != ESP_OK || es8311_microphone_config(dac_h, false) != ESP_OK) {
    pf.close(); i2s.end(); put_msg("play: es8311 failed"); return;
  }
  digitalWrite(PIN_PA, HIGH);
  play_stop = false; rec_play_paused = false; rec_play_pos = 0; rec_play_total = pf.size() > 44 ? pf.size() - 44 : 0;
  rec_playing = true; put_msg("playing");
  xTaskCreatePinnedToCore(play_task, "play", 6144, NULL, 3, NULL, 0);
}

void rec_play_stop() { play_stop = true; rec_play_paused = false; }
void rec_play_toggle_pause() { if (rec_playing) rec_play_paused = !rec_play_paused; }

// ---------------- capture to RAM / playback from RAM (Settings tests) ----------------
volatile uint32_t talk_cap_n = 0;
volatile bool talk_playing = false;
volatile uint32_t talk_play_pos = 0;
volatile int talk_level = 0;
static int16_t *tc_buf = nullptr;
static uint32_t tc_max = 0;
static volatile bool tc_stop = false, tc_done = true;

static void talk_cap_task(void *) {
  static int16_t st[CHUNK_FRAMES * 2];
  int skip = 3;
  while (!tc_stop) {
    size_t got = i2s.readBytes((char *)st, sizeof st);
    if (got < sizeof st) continue;
    if (skip > 0) { skip--; continue; }
    int peak = 0;
    for (int i = 0; i < CHUNK_FRAMES && talk_cap_n < tc_max; i++) {
      int m = soft_limit((int)st[2 * i] + (int)st[2 * i + 1]);   // sum both mics, soft-limited
      tc_buf[talk_cap_n++] = (int16_t)m;
      int a = m < 0 ? -m : m; if (a > peak) peak = a;
    }
    talk_level = (int)(sqrtf(peak / 16000.0f) * 100.0f); if (talk_level > 100) talk_level = 100;
    if (talk_cap_n >= tc_max) break;
  }
  i2s.end();
  talk_level = 0;
  tc_done = true;
  vTaskDelete(NULL);
}

bool talk_cap_start(int16_t *buf, uint32_t max_samples) {
  if (rec.state != REC_IDLE || rec_playing || talk_playing || !tc_done || !buf) return false;
  char err[48];
  if (!mic_open(err, sizeof err)) { snprintf(rec.mic, sizeof rec.mic, "mic: %s", err); return false; }
  tc_buf = buf; tc_max = max_samples; talk_cap_n = 0; tc_stop = false; tc_done = false;
  xTaskCreatePinnedToCore(talk_cap_task, "tcap", 6144, NULL, 5, NULL, 0);
  return true;
}

uint32_t talk_cap_stop() {
  tc_stop = true;
  for (int i = 0; i < 150 && !tc_done; i++) delay(10);
  return talk_cap_n;
}

static const int16_t *tp_pcm = nullptr;
static uint32_t tp_n = 0;
static volatile bool tp_stop = false;

static void talk_play_task(void *) {
  const uint32_t CH = 1024;
  uint32_t i = 0;
  while (!tp_stop && i < tp_n) {
    uint32_t n = min<uint32_t>(CH, tp_n - i);
    int peak = 0;
    for (uint32_t k = 0; k < n; k += 8) { int a = abs((int)tp_pcm[i + k]); if (a > peak) peak = a; }
    talk_level = (int)(sqrtf(peak / 20000.0f) * 100.0f); if (talk_level > 100) talk_level = 100;
    i2s.write((const uint8_t *)(tp_pcm + i), n * 2);
    i += n; talk_play_pos = i;
  }
  static const int16_t zeros[512] = {0};
  i2s.write((const uint8_t *)zeros, sizeof zeros);   // flush with silence, avoids a click
  delay(120);
  digitalWrite(PIN_PA, LOW);
  i2s.end();
  talk_level = 0;
  talk_playing = false;
  vTaskDelete(NULL);
}

bool talk_play_start(const int16_t *pcm, uint32_t samples) {
  if (rec.state != REC_IDLE || rec_playing || talk_playing || !tc_done || !pcm || !samples) return false;
  pinMode(PIN_PA, OUTPUT); digitalWrite(PIN_PA, LOW);
  i2s.setPins(PIN_BCK, PIN_WS, PIN_DOUT, PIN_DIN, PIN_MCK);
  i2s.setTimeout(1000);
  if (!i2s.begin(I2S_MODE_STD, SR, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) { put_msg("play: i2s failed"); return false; }
  if (!dac_h) dac_h = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
  const es8311_clock_config_t clk = { .mclk_inverted = false, .sclk_inverted = false, .mclk_from_mclk_pin = true,
                                      .mclk_frequency = SR * 256, .sample_frequency = SR };
  if (!dac_h || es8311_init(dac_h, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK ||
      spk_volume_apply() != ESP_OK || es8311_microphone_config(dac_h, false) != ESP_OK) {
    i2s.end(); put_msg("play: es8311 failed"); return false;
  }
  digitalWrite(PIN_PA, HIGH);
  tp_pcm = pcm; tp_n = samples; tp_stop = false; talk_play_pos = 0; talk_playing = true;
  xTaskCreatePinnedToCore(talk_play_task, "tplay", 4096, NULL, 4, NULL, 0);
  return true;
}

void talk_play_stop() { tp_stop = true; }

uint64_t rec_free_bytes() { return store ? g_free_cache : 0; }    // v16: cached (refreshed on mount, record start, save, delete)
uint64_t rec_total_bytes() { return store ? g_total_cache : 0; }
