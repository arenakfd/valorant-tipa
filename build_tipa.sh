#!/bin/bash
# ============================================================================
#  CI 编译脚本: Xcode 编 .app → Build.sh 打 .tipa
#
#  工程里那个 Build.sh 是 Xcode 的 Run Script 阶段(靠 Xcode 传进来的环境变量
#  找产物、ldid 签名、压 tipa), 本脚本只负责把 xcodebuild 跑起来。
# ============================================================================

set -euo pipefail

cd "$(dirname "$0")"

echo "📌 当前目录: $(pwd)"

echo "=== Xcode 版本 ==="
xcodebuild -version
xcode-select -p

echo "=== 工程文件结构自检 ==="
python3 tools/check_pbxproj.py 地图.xcodeproj/project.pbxproj

echo "=== 剥掉混淆器编译参数 ==="
# 工程里每个文件都挂了 -mllvm -enable-bcfobf 这类 OLLVM 混淆 pass,
# 那是本地装了混淆插件(Xcode 自定义 clang)才认的参数, GitHub 的标准 clang 不认,
# 会直接卡在预编译头。所以 CI 出的是"不混淆"版本 —— 要混淆版得本地带插件的 Xcode 编。
python3 tools/strip_compiler_flags.py

echo "=== 开始编译 ==="
# 注意: 用 -target 时不能配 -derivedDataPath(会报错), 所以直接把产物目录指到 ./build
xcodebuild \
  -project 地图.xcodeproj \
  -target 地图 \
  -configuration Release \
  -sdk iphoneos \
  CONFIGURATION_BUILD_DIR="$PWD/build" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  CODE_SIGN_ENTITLEMENTS="" \
  build

echo "=== 找 tipa 产物 ==="
find ./build -maxdepth 3 -name "*.app" -o -maxdepth 3 -name "*.tipa" | head -20

TIPA=$(find ./build -name "*.tipa" | head -1 || true)
if [ -z "$TIPA" ]; then
  echo "❌ 没找到 tipa 产物"
  exit 1
fi

cp "$TIPA" ./地图.tipa
ls -lh ./地图.tipa

echo "=== 校验包内容 ==="
unzip -l ./地图.tipa | head -30
rm -rf _check
unzip -o ./地图.tipa -d _check >/dev/null
APP=$(find ./_check/Payload -maxdepth 1 -name "*.app" | head -1)
if [ -z "$APP" ]; then
  echo "❌ 包里没有 .app"
  exit 1
fi
EXE=$(find "$APP" -maxdepth 1 -type f -perm +111 | head -1)
if [ -z "$EXE" ]; then
  echo "❌ .app 里没有可执行文件"
  exit 1
fi
echo "✓ 可执行文件: $EXE"
file "$EXE"
rm -rf _check

echo "✅ 地图.tipa 已生成"
