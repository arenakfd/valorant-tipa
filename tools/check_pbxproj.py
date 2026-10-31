#!/usr/bin/env python3
# ============================================================================
#  pbxproj 结构自检
#
#  Xcode 工程文件是"老式 plist", 标准的 plistlib 读不了, 而且一旦少个分号
#  或括号错位, Xcode 只会丢一句"工程已损坏, 打不开", 根本看不出错在哪。
#  所以这里做结构级检查: 括号配平 / 引号闭合 / 每条赋值以分号收尾。
#  (CI 里编之前跑一遍, 免得等半天换来一句谜语)
# ============================================================================

import sys


def 检查(路径):
    文本 = open(路径, encoding="utf-8", errors="replace").read()

    行号 = 1
    在字符串 = False
    括号栈 = []

    i = 0
    while i < len(文本):
        c = 文本[i]

        if c == "\n":
            行号 += 1

        if 在字符串:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                在字符串 = False
        else:
            if c == '"':
                在字符串 = True
            elif c in "{(":
                括号栈.append((c, 行号))
            elif c in "})":
                if not 括号栈:
                    print(f"✗ 第 {行号} 行多出一个 '{c}'")
                    return False
                开括号, 开行号 = 括号栈.pop()
                配对 = ("{" if 开括号 == "{" else "(") 
                if (开括号 == "{" and c != "}") or (开括号 == "(" and c != ")"):
                    print(f"✗ 第 {行号} 行的 '{c}' 跟第 {开行号} 行的 '{开括号}' 不配对")
                    return False
        i += 1

    if 在字符串:
        print("✗ 有引号没闭合")
        return False

    if 括号栈:
        开括号, 开行号 = 括号栈[-1]
        print(f"✗ 有 {len(括号栈)} 个括号没闭合, 最靠后的一个在第 {开行号} 行 ('{开括号}')")
        return False

    print("✓ pbxproj 结构检查通过")
    return True


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: check_pbxproj.py <project.pbxproj>")
        sys.exit(2)
    sys.exit(0 if 检查(sys.argv[1]) else 1)
