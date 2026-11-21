#!/usr/bin/env bash
# Decide whether this firmware version should become a GitHub release.
# Usage: release_gate.sh VERSION [TAG ...]
# Prints "publish" when VERSION is a whole number higher than every tag, otherwise "skip".
# Tags look like v4. A tag with no number is ignored. No tags means publish.
# This script only prints a word. It does not build or upload anything.
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: release_gate.sh VERSION [TAG ...]" >&2
  exit 2
fi

VER=$1
shift
case $VER in
  ''|*[!0-9]*)
    echo "VERSION must be a whole number, got: $VER" >&2
    exit 2
    ;;
esac

latest=0
for tag in "$@"; do
  [ -n "$tag" ] || continue
  n=$(printf '%s\n' "$tag" | grep -oE '[0-9]+' | head -1 || true)
  if [ -n "$n" ] && [ "$n" -gt "$latest" ]; then
    latest=$n
  fi
done

if [ "$VER" -gt "$latest" ]; then
  echo publish
else
  echo skip
fi
