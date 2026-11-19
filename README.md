# Puck Home

A tiny starter firmware for the **Waveshare ESP32-S3-Touch-LCD-1.85B** (a round 1.85" touch screen with a speaker, microphone and Wi-Fi).

Swipe left/right between three pages:
- **Recorder**: tap to record, Pause/Resume, Stop. It saves WAV files to the microSD card, or internal flash if there's no card. Recent recordings are listed below. Tap one to play, pause or delete it.
- **Clock**: a big clock in Malaysia time (MYT, UTC+8), with the Wi-Fi status, a Check update button and the version.
- **Settings**: Sound (volume, test speaker), Microphone (gain, level meter, 3 s mic test), Display (brightness), Storage (where recordings go, free space, recording time left, count), Wi-Fi (pick a network, saved networks), Battery, and About (version, Check update).

It **updates itself over Wi-Fi** from this repo's GitHub releases. No passwords are stored in the code.

It's a clean base. You can ask an AI coding tool to build apps on top of it, and each new version reaches the puck over Wi-Fi with no USB cable.

## 1. First time only: flash over USB
1. Download **`merged.bin`** from the [latest release](../../releases/latest).
2. Plug the puck into your computer with a USB-C data cable.
3. Open <https://espressif.github.io/esptool-js/> in **Chrome or Edge**.
4. Click **Connect** and pick the puck's serial port. If it doesn't show up, hold **BOOT** while plugging it in.
5. Set the flash address to **`0x0`**, choose `merged.bin`, and click **Program**.
6. When it finishes, press **RESET** or replug the puck.

## 2. Connect Wi-Fi
**On the puck:** go to Settings > Wi-Fi and tap your network. Type the password on the keyboard (the eye button shows or hides it) and tap Connect. The puck remembers up to 5 networks and joins the strongest one it can see when it starts. To remove one, tap **Forget** under Saved networks. If a new network fails, the puck goes back to the previous one.

**Fallback (first setup, or no saved network in range):** the puck shows "WiFi: join AP Puck-Home-Setup".
1. On your phone, join the Wi-Fi network **Puck-Home-Setup**. A setup page opens. If it doesn't, go to `192.168.4.1`.
2. Choose your home Wi-Fi, enter the password and save. That network is also added to the saved list.

## 3. Updates
The puck checks for a newer version when it starts, and whenever you tap **Check update**. If the newest release has a higher number than the puck's version, it downloads `firmware.bin`, installs it and restarts.

## 4. Build your own app with AI
The pages are built in `build_ui()` in `puck_home/puck_home.ino` and refreshed in `loop()`. The Recorder is in `recorder_ui.h` and Settings in `settings_ui.h`. `AGENTS.md` lists every file.

1. Install an AI coding tool such as Cursor, Claude Code or Codex.
2. Open this repo's folder in it. It reads `AGENTS.md`, which has the rules.
3. Ask for what you want, for example: *"Add a page with a timer I can start and stop by touch."*
4. Run `./release.sh`. It builds, publishes a new release, and the puck updates on its next check.

### Computer setup (Mac or Linux)
```
# 1. GitHub CLI, logged in to YOUR account
brew install gh git arduino-cli      # Mac. On Linux, use your package manager
gh auth login
# 2. ESP32 core (exactly this version)
arduino-cli core install esp32:esp32@3.2.0
# 3. Your own copy of this repo (fork it, or create a new repo and push this folder)
gh repo fork systemdigitalcookie-hash/puck-home --clone && cd puck-home
```
Then **change `OTA_REPO` in `puck_home/puck_home.ino`** to your repo, `yourname/puck-home`, and flash once over USB. Otherwise your puck will keep updating from the original.

`./release.sh` runs these steps:
1. Raises the number in `puck_home/version.h` by 1. `./release.sh 7` sets it to 7.
2. Compiles with the right board settings (16 MB flash, PSRAM, partition `app3M_fat9M_16MB`).
3. Makes `dist/firmware.bin` (used for updates), `dist/merged.bin` (full image for USB) and `dist/version.txt`.
4. Commits, pushes, and creates a GitHub release with all three files.

To check that it compiles without publishing, run the `arduino-cli compile ...` line inside `release.sh`.

## Board reference
| Part | Chip | Details |
|---|---|---|
| Display | ST77916, 360x360 round, QSPI | CS 21, SCK 40, D0 46, D1 45, D2 42, D3 41, TE 18, RST 3. See `Display_ST77916.h`. |
| Touch | CST816S | I2C address `0x15` |
| Microphone ADC | ES7210 | I2C `0x40` |
| Speaker codec | ES8311 | I2C `0x18`, power amp enable on **GPIO9** (HIGH = on) |
| I2S audio | | MCLK **2**, BCLK **48**, LRCK **38**, DIN **39** (mic), DOUT **47** (speaker) |
| I2C bus | | SDA **11**, SCL **10** |
| IMU (motion) | QMI8658 | I2C `0x6B` |
| RTC clock | PCF85063 | I2C `0x51` |
| Battery gauge | BQ27220 | I2C `0x55` |
| SD card | | SDMMC: CLK 15, CMD 14, D0 16, D1 17, D2 12, D3 13 (FAT32) |

Waveshare's wiki and example code: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B>. The ES7210/ES8311 audio drivers are already included in `puck_home/`.

## Limits
- App size must stay under **3 MB**. `release.sh` stops if it's bigger.
- Don't put passwords or keys in the code. This repo is public.
- If updates stop working years from now, the HTTPS root certificates in `certs.h` may have expired. Ask an AI to refresh them.

## Changes
- **v2**: Recorder page (record/pause/stop, recent list, detail screen with play/pause/progress/delete). It saves with exact error messages and repairs the header when only the size check disagrees. Louder mics and speaker: both mics are summed with a soft limiter, mic gain is 0-60 dB (default 42), volume reaches +12 dB, and playback has auto-gain. Settings with the on-device Wi-Fi picker and Storage. The clock now keeps time offline with the RTC.
- **v1**: Wi-Fi portal, clock, OTA.

## Changing the look (v3 pink theme)
Everything about colours and shapes is in `puck_home/theme.h`:
- Colours: `C_BG` (page), `C_CARD` (buttons), `C_TEXT`, `C_ACCENT` (rose pink), `C_HOT` (hot pink for record, bow knots and the active page dot).
- Roundness: `T_RADIUS`, `T_RADIUS_PILL`. Dotted ring around the screen: `T_RING_DOTS`, `T_RING_R`.
- Fonts: `T_FONT_S/M/L/XL/XXL` (Montserrat, built in).
Change a value, build, and every page follows. The little hearts, bows, flowers and bunny ears are original drawings made by `python3 tools/gen_art.py` into `puck_home/art.h`. They are tinted by the theme colours, so recolouring needs no new art.
