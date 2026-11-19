#!/usr/bin/env bash
# One-command release:  ./release.sh      -> bump version by 1, build, commit, push, publish GitHub release
#                       ./release.sh 5    -> use version 5 explicitly
# Needs: git, gh (logged in: `gh auth login`), arduino-cli with esp32:esp32@3.2.0 (see README).
set -euo pipefail
cd "$(dirname "$0")"
[ -x bin/arduino-cli ] && export PATH="$PWD/bin:$PATH"
command -v arduino-cli >/dev/null || { echo "arduino-cli not found (see README)"; exit 1; }
REPO_SLUG=$(git remote get-url origin | sed -E 's#.*github.com[:/]##; s#\.git$##')
SK=puck_home
WS_COMMIT=5a98b8da1948340c07032a408ac0d93c47ca558f
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc,FlashMode=qio,USBMode=hwcdc"

# Waveshare's bundled libraries (lvgl etc.), pinned
if [ ! -d repo/examples/Arduino-V3.2.0/libraries ]; then
  git clone https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.85B repo
  git -C repo checkout "$WS_COMMIT"
fi
arduino-cli lib list 2>/dev/null | grep -q WiFiManager || arduino-cli lib install WiFiManager

CUR=$(grep -oE '[0-9]+' $SK/version.h | head -1)
NEW=${1:-$((CUR+1))}
echo "#define FW_VERSION $NEW" > $SK/version.h
echo "Building v$NEW"
rm -rf build
arduino-cli compile --fqbn "$FQBN" --libraries repo/examples/Arduino-V3.2.0/libraries --output-dir build $SK

mkdir -p dist; rm -f dist/*
cp build/$SK.ino.bin dist/firmware.bin          # app-only, used by OTA
cp build/$SK.ino.merged.bin dist/merged.bin     # full image, one-time USB flash at 0x0
printf '%s' "$NEW" > dist/version.txt
SIZE=$(stat -c %s dist/firmware.bin 2>/dev/null || stat -f %z dist/firmware.bin)
echo "App size: $SIZE bytes (limit 3145728)"
[ "$SIZE" -lt 3145728 ] || { echo "App too big for the 3MB partition"; exit 1; }

git add -A && git commit -m "Release v$NEW" || true
git push origin HEAD
gh release create "v$NEW" dist/version.txt dist/firmware.bin dist/merged.bin \
  --repo "$REPO_SLUG" --title "v$NEW" --notes "Puck Home v$NEW. First flash over USB: merged.bin at 0x0. After that the puck updates itself from firmware.bin."
echo "Published: https://github.com/$REPO_SLUG/releases/tag/v$NEW"
