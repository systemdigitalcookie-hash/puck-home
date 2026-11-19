#pragma once
#include <Arduino.h>

enum RecStateE { REC_IDLE = 0, REC_RECORDING, REC_FINALIZING, REC_PAUSED };

struct RecInfo {
  volatile int state;          // RecStateE
  volatile int level;          // 0..100 live mic level
  volatile uint32_t secs;      // elapsed seconds of current/last recording
  volatile uint32_t free_secs; // estimated remaining recording time on current storage
  volatile int count;          // number of saved rec_XXXX.wav files
  volatile uint32_t dropped;   // bytes dropped because storage was too slow (should stay 0)
  char storage[8];             // "SD" or "Flash"
  char last[24];               // last saved file name
  char msg[72];                // short status/error message
  char mic[64];                // mic diagnostics (init + self-test result)
  char sd[32];                 // SD diagnostics
  char i2c[72];                // I2C scan result (7-bit hex addresses)
};

extern RecInfo rec;

bool rec_init();    // call once after I2C (Wire) is up
void rec_start();   // no-op unless idle
void rec_stop();    // asks recorder to stop and finalize the WAV (non-blocking)
void rec_pause();   // stop writing samples (same file)
void rec_resume();
void rec_force_idle(const char *why);   // UI watchdog escape hatch
extern volatile bool rec_writer_alive;

// ---- recordings list / playback / delete ----
struct RecFile { char name[24]; uint32_t bytes; time_t t; };
int  rec_list(RecFile *out, int max);        // newest first; empty while recording
bool rec_delete(const char *name);
void rec_play(const char *name);             // plays through ES8311 + speaker pads; call from the UI task
void rec_play_stop();
void rec_play_toggle_pause();
extern volatile bool rec_play_paused;
extern volatile uint32_t rec_play_pos, rec_play_total;
extern volatile bool rec_playing;

// ---- capture into RAM / playback of PCM from RAM (Settings mic + speaker tests) ----
// Call start/stop/play from the UI task (they touch I2C codecs).
bool talk_cap_start(int16_t *buf, uint32_t max_samples);  // 16 kHz mono
uint32_t talk_cap_stop();                                  // returns samples captured (waits for capture task)
extern volatile uint32_t talk_cap_n;                       // live sample count
bool talk_play_start(const int16_t *pcm, uint32_t samples);
void talk_play_stop();
extern volatile bool talk_playing;
extern volatile uint32_t talk_play_pos;                     // samples played so far
extern volatile int talk_level;                             // 0..100 live level (mic or speaker)

// ---- settings shared with the UI ----
extern volatile int g_spk_volume;   // 0..100, applied at next playback start
extern volatile int g_mic_gain;     // total mic gain dB 0..60 (PGA <=33 dB + ES7210 digital), applied at next mic open
uint64_t rec_free_bytes();
uint64_t rec_total_bytes();
