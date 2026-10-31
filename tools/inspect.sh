#!/bin/bash
# Verify the produced tipa: payload layout, binary arch, entitlements.
set -e
cd "$(dirname "$0")/.."

echo "=== build dirs ==="
find "$PWD/build" -maxdepth 2 -type d

TIPA=$(find "$PWD/build" -name '*.tipa' | head -1)
echo "TIPA=$TIPA"
test -n "$TIPA"
ls -la "$TIPA"

echo "=== zip listing (head) ==="
unzip -l "$TIPA" | head -40

rm -rf verify
mkdir -p verify
cd verify
unzip -o -q "$TIPA"

APP=$(find . -maxdepth 2 -name '*.app' | head -1)
echo "APP=$APP"
ls -la "$APP"

BIN="$APP/$(basename "$APP" .app)"
echo "BIN=$BIN"
file "$BIN"

echo "=== entitlements ==="
/opt/homebrew/bin/ldid -e "$BIN" | head -60

echo "=== dyld deps ==="
otool -L "$BIN" | head -20

echo "=== frameworks in bundle ==="
ls -la "$APP/Frameworks" 2>/dev/null || echo "(no Frameworks dir)"

echo "=== kernel libs in bundle ==="
ls -la "$APP"/ljb_dp.dylib "$APP"/ljb_rh.dylib "$APP"/libchoma.dylib
for L in "$APP"/ljb_dp.dylib "$APP"/ljb_rh.dylib "$APP"/libchoma.dylib; do
  echo "--- $L"
  /opt/homebrew/bin/ldid -e "$L" >/dev/null 2>&1 && echo "signature ok" || echo "signature MISSING"
done

echo "=== app load commands for jailbreak libs (should be empty) ==="
otool -L "$BIN" | grep -i -E "jailbreak|ljb_" || echo "(none)"
