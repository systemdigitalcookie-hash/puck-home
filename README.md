# Puck Home

A tiny starter firmware for the **Waveshare ESP32-S3-Touch-LCD-1.85B** (a round 1.85" touch screen with a speaker, microphone and Wi-Fi).

It does only four things:
- Wi-Fi setup through a phone-friendly page, with no passwords in the code
- Shows a big clock in Malaysia time (MYT, UTC+8), set over the internet
- Shows the Wi-Fi status and the firmware version
- **Updates itself over Wi-Fi** from this repo's GitHub releases

It's a clean base. You can ask an AI coding tool to build apps on top of it, and each new version reaches the puck over Wi-Fi with no USB cable.

## 1. First time only: flash over USB
1. Download **`merged.bin`** from the [latest release](../../releases/latest).
2. Plug the puck into your computer with a USB-C data cable.
3. Open <https://espressif.github.io/esptool-js/> in **Chrome or Edge**.
4. Click **Connect** and pick the puck's serial port. If it doesn't show up, hold **BOOT** while plugging it in.
5. Set the flash address to **`0x0`**, choose `merged.bin`, and click **Program**.
6. When it finishes, press **RESET** or replug the puck.

## 2. Connect Wi-Fi
1. The puck shows "Join Wi-Fi: Puck-Home-Setup".
2. On your phone, join the Wi-Fi network **Puck-Home-Setup**. A setup page opens. If it doesn't, go to `192.168.4.1`.
3. Choose your home Wi-Fi, enter the password and save. The clock appears after a few seconds.

Wi-Fi is remembered. To change it, take the puck away from the old Wi-Fi and the setup network comes back.

## 3. Updates
The puck checks for a newer version when it starts, and whenever you tap **Check update**. If the newest release has a higher number than the puck's version, it downloads `firmware.bin`, installs it and restarts.

## 4. Build your own app with AI
Everything is in `puck_home/puck_home.ino`. The screen is built in `build_ui()` and refreshed in `loop()`.

1. Install an AI coding tool such as Cursor, Claude Code or Codex.
2. Open this repo's folder in it. It reads `AGENTS.md`, which has the rules.
3. Ask for what you want, for example: *"Add a second page with a timer I can start and stop by touch."*
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
| SD card | | SDMMC |

Waveshare's wiki and example code: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B>. The ES7210 and ES8311 drivers and the audio examples live there. Copy them in when you want sound.

## Limits
- App size must stay under **3 MB**. `release.sh` stops if it's bigger.
- Don't put passwords or keys in the code. This repo is public.
- If updates stop working years from now, the HTTPS root certificates in `certs.h` may have expired. Ask an AI to refresh them.
