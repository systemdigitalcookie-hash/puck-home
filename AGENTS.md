# Rules for AI coding tools working on this repo

This is firmware for a Waveshare ESP32-S3-Touch-LCD-1.85B (round 360x360 touch screen). The owner is a beginner. Be careful and explain what you change.

1. **Keep the OTA/Wi-Fi code intact.** `net_task`, `check_update`, `certs.h`, the `OTA_BASE`/`OTA_REPO` defines and the "Check update" button are how the puck gets updates without USB. If they break, the puck needs a USB cable again. You can add features around them. Don't remove or rewrite them.
2. **Always bump the version.** Never publish with an unchanged version, because the puck only updates when the remote `version.txt` is higher than its `FW_VERSION`. Use `./release.sh`, which bumps it for you.
3. **Never commit secrets.** No Wi-Fi passwords, API keys, tokens or personal data in source, README or releases. Keep them in a gitignored `secrets.h`. This repo is public.
4. **Keep the app under 3 MB** (partition `app3M_fat9M_16MB`). `release.sh` fails the build if `firmware.bin` is too big. Avoid big images and fonts in flash.
5. **Don't change the partition scheme or FQBN** in `release.sh`. A different layout breaks OTA on devices already in use.
6. **Don't change the Wi-Fi AP name** `Puck-Home-Setup`.
7. Keep the style simple and Notion-like: white background `#FFFFFF`, text `#37352F`, thin grey lines. The colors are in `ui_style.h`.
8. Only edit `build_ui()` and `loop()` in `puck_home/puck_home.ino`, plus new files. Leave the display, touch and LVGL drivers alone unless you really need to.
9. Don't call `Wire.begin()` again. `I2C_Init()` does it (SDA 11, SCL 10).
10. Build and test with `./release.sh` only when the user asks to publish. To only check that it compiles, run the `arduino-cli compile` line from `release.sh`.
