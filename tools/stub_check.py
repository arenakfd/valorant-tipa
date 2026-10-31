#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""语法桩检查: 把游戏层(暗区国际服)的头文件放进一个纯 C++ 环境里编一遍。
目的: 把语法/类型错误挡在推 CI 之前, 省一轮 macOS 往返。
只做 -fsyntax-only, 不链接, 不产出二进制。"""
import os, re, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
HUD = os.path.join(ROOT, "HUD", "绘制")
OUT = "/tmp/zaka-stub"
GAME = os.path.join(HUD, "游戏", "暗区突围", "国际服")

shutil.rmtree(OUT, ignore_errors=True)
for d in ("override", "src/游戏/暗区突围/国际服"):
    os.makedirs(os.path.join(OUT, d), exist_ok=True)

# ---- 替身: 内核层 (真实 kfd.h 依赖 Darwin 的 mach/dyld) ----
open(os.path.join(OUT, "override", "kfd.h"), "w").write('''#pragma once
#include <cstdint>
#include <cstddef>
#include <csignal>
#include <string>
#include <vector>
using namespace std;
inline string g_内核诊断 = "桩";
inline string g_内核库名 = "桩库";
inline vector<string> g_内核日志;
inline int    g_重连成功次数 = 0;
inline int    g_重连失败次数 = 0;
inline volatile sig_atomic_t g_要退出 = 0;
inline void 内核记日志(const string&) {};
struct {
    template<typename 类型> 类型 取vreadbuf(uint64_t) { return 类型{}; };
    bool 取vreadbuf列表(uint64_t, void*, size_t) { return true; };
    template<typename 类型> bool 写vwritebuf(uint64_t, 类型) { return true; };
    bool 写vwritebuf列表(uint64_t, void*, size_t) { return true; };
    bool 写布尔(uint64_t, bool) { return true; };
    bool 写原始(uint64_t, void*, size_t) { return true; };
    bool 解页表句柄() { return true; };
    bool 重连(string) { return true; };
    bool 目标还在() { return true; };
    void 释放kfd() {};
    uint64_t 取目标模块地址(string) { return 0x100000000ULL; };
    bool 读取自检(uint64_t, string& s) { s = "桩"; return true; };
    bool 取目标进程ID(string) { return true; };
    bool 地址判断(uint64_t 地址) { return 地址 > 0x100000000ULL && 地址 < 0x10000000000ULL; };
} kfd;
inline void 内核环境自检() {};
inline bool 初始化内核链路(string, string, uint64_t&) { return true; };
''')

# ---- 替身: 绘图层 (真实 绘图.h 拉 Objective-C 的 Metal 头) ----
open(os.path.join(OUT, "override", "绘图.h"), "w").write('''#pragma once
#include "imgui.h"
#include <string>
using namespace std;
void 绘制矩形(ImDrawList*, ImVec4, ImU32, float 圆角 = 0.0f, bool 瞄边 = false);
void 绘制圆形(ImDrawList*, float, ImVec2, ImU32, bool 瞄边 = false);
void 绘制文字(ImDrawList*, string, float, ImVec2, ImU32, int 方向 = 0, bool 瞄边 = false, bool 背景 = false);
void 绘制血条(ImDrawList*, ImVec2, float, float, bool);
''')

# ---- 真实文件: 设备类型 ----
shutil.copy(os.path.join(HUD, "设备类型.h"), os.path.join(OUT, "override", "设备类型.h"))

def 净化(源, 目标):
    文本 = open(源, encoding="utf-8").read()
    文本 = 文本.replace("#import", "#include")
    # Objective-C 片段 -> 等价的 C++ 桩 (只影响桩检查, 真工程不动)
    文本 = re.sub(r'NSDictionary \*配置 = \[NSDictionary dictionaryWithContentsOfFile:@[^\]]*\];',
                  'void* 配置 = nullptr;', 文本)
    文本 = re.sub(r'NSNumber \*(\w+) = \[配置 objectForKey:@"[^"]*"\];',
                  r'void* \1 = nullptr;', 文本)
    文本 = re.sub(r'\[\[配置 objectForKey:@"[^"]*"\] boolValue\]', 'false', 文本)
    文本 = re.sub(r'\[(\w+) floatValue\]', '0.0f', 文本)
    文本 = re.sub(r'\[(\w+) intValue\]', '0', 文本)
    文本 = re.sub(r'\[(\w+) boolValue\]', 'false', 文本)
    open(目标, "w", encoding="utf-8").write(文本)

净化(os.path.join(HUD, "游戏", "绘制类型.h"), os.path.join(OUT, "src/游戏/绘制类型.h"))
净化(os.path.join(HUD, "游戏", "绘制算法.h"), os.path.join(OUT, "src/游戏/绘制算法.h"))
for 名 in ("SDK.h", "子线程.h", "主线程.h", "游戏类型.h"):
    净化(os.path.join(GAME, 名), os.path.join(OUT, "src/游戏/暗区突围/国际服", 名))

open(os.path.join(OUT, "override", "objc_stub.h"), "w").write('''#pragma once
#include <unistd.h>
// Objective-C 的 nil/YES/NO/BOOL 在纯 C++ 桩里给个等价物
#ifndef nil
#define nil nullptr
#endif
#define YES true
#define NO false
#ifndef BOOL
#define BOOL int
#endif
''')

open(os.path.join(OUT, "src", "main.cpp"), "w").write('''#include <string>
#include "objc_stub.h"
#include <vector>
#include <map>
#include <atomic>
#include <cmath>
#include <ctime>
using namespace std;
#include "游戏/暗区突围/国际服/主线程.h"
#include "游戏/暗区突围/国际服/子线程.h"

int main() {
    ImDrawList* 画布 = nullptr;
    int 真人 = 0, 人机 = 0, 盒子 = 0;
    主线程(画布, 真人, 人机, 盒子);
    return 0;
}
''')

命令 = ["g++", "-std=gnu++17", "-fsyntax-only", "-Wno-changes-meaning",
        "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-Wno-unused-function",
        "-I" + os.path.join(OUT, "override"),
        "-I" + os.path.join(OUT, "src", "游戏"),
        "-I" + os.path.join(OUT, "src", "游戏", "暗区突围", "国际服"),
        "-I" + os.path.join(HUD, "ImGui"),
        os.path.join(OUT, "src", "main.cpp")]
结果 = subprocess.run(命令, capture_output=True, text=True)
输出 = (结果.stdout + 结果.stderr).strip()
print(输出 if 输出 else "桩检查通过: 没有语法/类型错误")
sys.exit(结果.returncode)
