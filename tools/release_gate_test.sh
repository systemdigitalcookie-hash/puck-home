#!/usr/bin/env bash
# Checks the release decision without talking to GitHub and without publishing.
set -euo pipefail
cd "$(dirname "$0")/.."
G=tools/release_gate.sh
fail=0

expect() {
  local want=$1
  shift
  local got
  got=$("$G" "$@")
  if [ "$got" != "$want" ]; then
    echo "FAIL: $* => [$got] want [$want]" >&2
    fail=1
  else
    echo "ok: $* => $want"
  fi
}

expect publish 5 v1 v2 v3 v4
expect skip 4 v4
expect skip 5 v5
expect skip 5 v4 v5
expect publish 5
expect skip 3 v4
expect publish 10 v4 v9
expect skip 6 v10
expect publish 1
expect publish 5 not-a-release
expect publish 4 ""

if "$G" abc v1 >/dev/null 2>&1; then
  echo "FAIL: a non-numeric version should be rejected" >&2
  fail=1
else
  echo "ok: non-numeric version rejected"
fi

if "$G" >/dev/null 2>&1; then
  echo "FAIL: a missing version should be rejected" >&2
  fail=1
else
  echo "ok: missing version rejected"
fi

if [ "$fail" -ne 0 ]; then
  echo "release gate tests failed" >&2
  exit 1
fi
echo "release gate tests passed"
