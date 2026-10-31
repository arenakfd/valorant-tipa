#!/bin/sh
PASSWORD="0000"

set -euo pipefail

echo "📌 目录: $TARGET_BUILD_DIR"

echo "目录 $SRCROOT"
echo "开始处理 Payload 文件夹及应用..."
rm -rf "$TARGET_BUILD_DIR/Payload"
rm -rf "$TARGET_BUILD_DIR/$INFOPLIST_KEY_CFBundleDisplayName.tipa"

echo "创建 Payload 文件夹"
mkdir -p "$TARGET_BUILD_DIR/Payload"

EXE_PATH="$TARGET_BUILD_DIR/$EXECUTABLE_PATH"

echo "对应用签名 $EXE_PATH"

# 等待可执行文件生成（最多等10秒）
for i in {1..10}; do
    if [ -f "$EXE_PATH" ]; then
        break
    fi
    echo "⏳ 等待可执行文件生成... ($i/10)"
    sleep 1
done

# 调试：检查文件是否存在
if [ ! -f "$EXE_PATH" ]; then
    echo "❌ 找不到可执行文件: $EXE_PATH"
    echo "📂 TARGET_BUILD_DIR 内容:"
    ls -la "$TARGET_BUILD_DIR/"
    echo "📂 .app 目录内容:"
    ls -la "$TARGET_BUILD_DIR/"*.app/ 2>/dev/null || echo "没找到 .app 目录"
    exit 1
fi

ENT_PATH="$PROJECT_DIR/权限/Entitlements.plist"

# 检查权限文件
if [ ! -f "$ENT_PATH" ]; then
    echo "❌ 找不到权限文件: $ENT_PATH"
    exit 1
fi

/opt/homebrew/bin/ldid -S"$ENT_PATH" "$EXE_PATH"

if [ $? -eq 0 ]; then
    echo "签名成功！"
else
    echo "签名失败！"
    exit 1
fi

# 依赖库改过 install name,原签名失效,这里重新签一遍(否则 dyld 拒载)
for LIB in "$CODESIGNING_FOLDER_PATH"/ljb_dp.dylib "$CODESIGNING_FOLDER_PATH"/ljb_rh.dylib "$CODESIGNING_FOLDER_PATH"/libchoma.dylib; do
    if [ -f "$LIB" ]; then
        /opt/homebrew/bin/ldid -S "$LIB" || echo "⚠️ $(basename "$LIB") 签名失败,继续"
        echo "已重签 $(basename "$LIB")"
    else
        echo "⚠️ 缺依赖库 $(basename "$LIB")"
    fi
done

echo "删除 PkgInfo 文件"
rm -rf "$CODESIGNING_FOLDER_PATH/PkgInfo"

echo "删除 Entitlements.plist 文件"
rm -rf "$CODESIGNING_FOLDER_PATH/Entitlements.plist"

echo "移动 $CONTENTS_FOLDER_PATH 文件夹"
cp -R "$CODESIGNING_FOLDER_PATH" "$TARGET_BUILD_DIR/Payload/$CONTENTS_FOLDER_PATH"

echo "不清理隐藏文件"

echo "压缩生成 ipa 文件"
cd "$TARGET_BUILD_DIR"
zip -rq "$INFOPLIST_KEY_CFBundleDisplayName.tipa" Payload
rm -rf Payload
cd - > /dev/null

echo "$INFOPLIST_KEY_CFBundleDisplayName.tipa 已生成！"

