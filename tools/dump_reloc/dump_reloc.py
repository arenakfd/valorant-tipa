#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dump_reloc.py —— 暗区突围 偏移重定位工具

游戏更新后怎么用（三步）：
   1. 把新的 MobileDumper-7 dump 目录放到任意位置（要有 Dumpspace/*.json）
   2. python3 dump_reloc.py --dump <新dump目录> --对照 <旧dump目录>
   3. 看输出的漂移清单，改 SDK.h，或者直接用 --出断言 生成的头文件卡住编译

常用：
   python3 dump_reloc.py --dump ../../dump                     # 只核对，出报告
   python3 dump_reloc.py --dump 新dump --对照 旧dump            # 两版对比，只看动了什么
   python3 dump_reloc.py --dump 新dump --出断言 SDK偏移_断言.h   # 生成编译期断言
   python3 dump_reloc.py --dump 新dump --严格                   # 有漂移就返回非 0

清单文件：同目录下 偏移清单.json
"""

import argparse
import json
import os
import re
import sys

BASE = 0x100000000  # dump 里的静态全局 = 0x100000000 + 相对主镜像偏移

源名 = {
    "类": "反射-类字段",
    "结构": "反射-结构体字段",
    "全局": "dump静态全局",
    "锚": "锚点推导",
    "原生": "引擎native",
    "运行期": "运行期自检",
    "内核": "jailbreak符号",
    "参考实现": "参考App常量",
    "无依据": "❗无dump依据",
}


# ----------------------------------------------------------------------------
# dump 加载
# ----------------------------------------------------------------------------
class Dump:
    def __init__(self, 目录):
        self.目录 = os.path.abspath(目录)
        ds = os.path.join(self.目录, "Dumpspace")
        if not os.path.isdir(ds):
            # 允许直接指向 Dumpspace 或它的上一层
            for cand in (self.目录, os.path.join(self.目录, "..", "Dumpspace")):
                if os.path.isfile(os.path.join(cand, "ClassesInfo.json")):
                    ds = os.path.abspath(cand)
                    break
            else:
                raise SystemExit("找不到 Dumpspace/ClassesInfo.json，请把 --dump 指到 dump 根目录")
        self.类 = self._读表(os.path.join(ds, "ClassesInfo.json"))
        self.结构 = self._读表(os.path.join(ds, "StructsInfo.json"))
        self.全局 = self._读全局(os.path.join(ds, "OffsetsInfo.json"))
        self.类数 = len(self.类)
        self.结构数 = len(self.结构)

    @staticmethod
    def _读表(路径):
        if not os.path.isfile(路径):
            return {}
        raw = json.load(open(路径, "r", encoding="utf-8"))
        out = {}
        for e in raw.get("data", []):
            for 名, 体 in e.items():
                d = {}
                for it in 体:
                    for k, v in it.items():
                        d[k] = v
                out[名] = d
        return out

    @staticmethod
    def _读全局(路径):
        if not os.path.isfile(路径):
            return {}
        raw = json.load(open(路径, "r", encoding="utf-8"))
        out = {}
        for k, v in raw.get("data", []):
            if isinstance(v, int):
                out[k] = v
        return out

    def 取表(self, 类名):
        return self.类.get(类名) or self.结构.get(类名)

    def 找字段(self, 类名, 字段, 深度=0):
        """在类/结构体及其继承链上找字段，返回 (偏移, 来自哪个类, 类型, 大小) 或 None"""
        if 深度 > 12:
            return None
        d = self.取表(类名)
        if not d:
            return None
        v = d.get(字段)
        if isinstance(v, list) and len(v) > 2 and isinstance(v[1], int):
            t = v[0][0] if isinstance(v[0], list) else v[0]
            return (v[1], 类名, t, v[2])
        for p in d.get("__InheritInfo", []) or []:
            r = self.找字段(p, 字段, 深度 + 1)
            if r:
                return r
        return None

    def 类大小(self, 类名):
        d = self.取表(类名)
        if d and isinstance(d.get("__MDKClassSize"), int):
            return d["__MDKClassSize"]
        return None

    def 邻居(self, 类名, 偏移):
        """返回该偏移在类里紧邻的左右已导出字段，用于给锚点做交叉校验"""
        d = self.取表(类名)
        if not d:
            return (None, None)
        ps = sorted(
            [(v[1], v[2], k) for k, v in d.items()
             if isinstance(v, list) and len(v) > 2 and isinstance(v[1], int)],
            key=lambda x: x[0])
        左 = None
        右 = None
        for o, s, k in ps:
            if o <= 偏移:
                左 = (o, k, s)
            elif 右 is None:
                右 = (o, k, s)
        return (左, 右)


# ----------------------------------------------------------------------------
# 解析
# ----------------------------------------------------------------------------
def 转数(x):
    if x is None:
        return None
    if isinstance(x, int):
        return x
    s = str(x).strip()
    m = re.match(r"^(0[xX][0-9A-Fa-f]+|\d+)$", s)
    if not m:
        return None
    return int(s, 0)


def 解析条目(dump, e):
    """返回 dict(状态, 值, 证据...)"""
    r = {"键": e["键"], "源": e.get("源", "?"), "功": e.get("功") or [],
         "现行": 转数(e.get("现行")), "现行原文": e.get("现行"),
         "断言": e.get("断言"), "断言值": 转数(e.get("断言值")), "说明": e.get("说明", ""), "位宽": e.get("位宽")}
    源 = r["源"]

    if 源 == "类" or 源 == "结构":
        命中 = dump.找字段(e.get("类") or e.get("结构"), e["字段"])
        if not 命中:
            r["状态"] = "查不到"
            r["注"] = "dump 里没有 %s::%s" % (e.get("类") or e.get("结构"), e["字段"])
            return r
        偏, 宿主, 类型, 大小 = 命中
        r["值"] = 偏
        r["类型"] = 类型
        r["大小"] = 大小
        r["宿主"] = 宿主
        r["证据"] = "%s::%s @0x%X size=%s" % (宿主, e["字段"], 偏, 大小)

    elif 源 == "全局":
        k = e["全局"]
        if k not in dump.全局:
            r["状态"] = "查不到"
            r["注"] = "OffsetsInfo 里没有 " + k
            return r
        r["值"] = dump.全局[k] - BASE
        r["证据"] = "%s = 0x%X → 减基址 0x%X" % (k, dump.全局[k], BASE)

    elif 源 == "锚":
        if e.get("锚字段") == "__MDKClassSize":
            锚命中 = ((dump.类大小(e["锚类"]), e["锚类"], "类大小", 0)
                      if dump.类大小(e["锚类"]) is not None else None)
        else:
            锚命中 = dump.找字段(e["锚类"], e["锚字段"])
        if not 锚命中:
            r["状态"] = "锚没了"
            r["注"] = "锚字段 %s::%s 在 dump 里找不到，锚点失效，必须人工重定" % (e["锚类"], e["锚字段"])
            return r
        锚值 = 锚命中[0]
        if e.get("锚字段") == "__MDKClassSize":
            锚值 = dump.类大小(e["锚类"])
        if e.get("锚差") is None:
            r["状态"] = "参考"
            r["值"] = None
            r["证据"] = "锚 = %s::%s @0x%X（跟随锚，不产生绝对值）" % (e["锚类"], e["锚字段"], 锚值)
            r["锚当前"] = 锚值
            return r
        r["值"] = 锚值 + int(e["锚差"])
        r["证据"] = "锚 %s::%s @0x%X %+d → 0x%X" % (e["锚类"], e["锚字段"], 锚值, int(e["锚差"]), r.get("值"))
        左, 右 = dump.邻居(e["锚类"], r.get("值"))
        if 左 and 右:
            r["证据"] += "（左邻 %s@0x%X，右邻 %s@0x%X）" % (左[1], 左[0], 右[1], 右[0])
        # 越界告警：锚出来的位置如果跑出类大小，说明结构大改
        类大小 = dump.类大小(e["锚类"])
        if 类大小 and r.get("值") >= 类大小:
            r["状态"] = "越界"
            r["注"] = "锚出的 0x%X 已超出 %s 类大小 0x%X" % (r.get("值"), e["锚类"], 类大小)
            return r

    elif 源 in ("原生", "运行期", "内核", "参考实现", "无依据"):
        r["状态"] = "不适用"
        r["值"] = r["现行"]
        r["证据"] = "该来源不由反射 dump 决定（%s）" % 源名.get(源, 源)
        return r
    else:
        r["状态"] = "源未知"
        return r

    # 比对
    if r["现行"] is None:
        r["状态"] = "待填"
    elif r.get("值") == r["现行"]:
        r["状态"] = "一致"
    else:
        r["状态"] = "漂移"
        r["差"] = r.get("值") - r["现行"]
    return r


# ----------------------------------------------------------------------------
# 报告
# ----------------------------------------------------------------------------
功能序 = ["内核链", "世界指针", "类匹配", "相机", "方框", "骨骼", "头甲", "距离", "射线",
        "真人判定", "实体过滤", "死亡判定", "血量", "无后坐力"]


def 写报告(dump, 结果, 对照=None, 对照结果=None, 路径=None):
    L = []
    def P(s=""):
        L.append(s)

    统计 = {}
    for r in 结果:
        统计[r["状态"]] = 统计.get(r["状态"], 0) + 1

    P("# 暗区突围 国际服 偏移核对报告")
    P()
    P("- dump 目录：`%s`" % dump.目录)
    P("- dump 规模：类 %d 个 / 结构体 %d 个 / 静态全局 %d 个" % (dump.类数, dump.结构数, len(dump.全局)))
    P("- 清单条目：%d 条" % len(结果))
    P("- 核对结果：" + "，".join("%s %d" % (k, v) for k, v in sorted(统计.items(), key=lambda x: -x[1])))
    P()

    漂 = [r for r in 结果 if r["状态"] in ("漂移", "查不到", "锚没了", "越界")]
    if 漂:
        P("## ❗ 需要处理的条目（%d 条）" % len(漂))
        P()
        P("| 键 | 功能 | 源 | 工程现值 | dump 现值 | 状态 | 证据 |")
        P("|---|---|---|---|---|---|---|")
        for r in 漂:
            P("| %s | %s | %s | %s | %s | %s | %s |" % (
                r["键"], "/".join(r["功"]), 源名.get(r["源"], r["源"]),
                ("0x%X" % r["现行"]) if r["现行"] is not None else r["现行原文"],
                ("0x%X" % r.get("值")) if r.get("值") is not None else "-",
                r["状态"], r.get("证据") or r.get("注", "")))
        P()
    else:
        P("## ✅ 全部条目与 dump 一致，没有漂移")
        P()

    if 对照 and 对照结果:
        动了 = []
        for a, b in zip(结果, 对照结果):
            if a.get("值") is None or b.get("值") is None:
                continue
            if a.get("值") != b.get("值"):
                动了.append((a["键"], b.get("值"), a.get("值"), a.get("值") - b.get("值")))
        P("## 📦 两版 dump 对比（%s → %s）" % (对照.目录, dump.目录))
        P()
        if 动了:
            P("| 键 | 旧 dump | 新 dump | 变化 |")
            P("|---|---|---|---|")
            for k, o, n, d in 动了:
                P("| %s | 0x%X | 0x%X | %+d |" % (k, o, n, d))
        else:
            P("两版 dump 之间所有可反射偏移**一个都没动**。")
        P()

    P("## 按功能分组明细")
    P()
    用过 = set()
    for f in 功能序:
        组 = [r for r in 结果 if f in r["功"]]
        if not 组:
            continue
        用过.add(f)
        P("### %s" % f)
        P()
        P("| 键 | 源 | 值 | 状态 | 证据 |")
        P("|---|---|---|---|---|")
        for r in 组:
            P("| %s | %s | %s | %s | %s |" % (
                r["键"], 源名.get(r["源"], r["源"]),
                ("0x%X" % r.get("值")) if r.get("值") is not None else (r["现行原文"] or "-"),
                r["状态"], r.get("证据", "")))
        P()

    其它 = [r for r in 结果 if not (set(r["功"]) & 用过) and not r["功"]]
    if 其它:
        P("### 未挂功能（备用/引擎基础）")
        P()
        P("| 键 | 源 | 值 | 状态 | 说明 |")
        P("|---|---|---|---|---|")
        for r in 其它:
            P("| %s | %s | %s | %s | %s |" % (
                r["键"], 源名.get(r["源"], r["源"]),
                ("0x%X" % r.get("值")) if r.get("值") is not None else (r["现行原文"] or "-"),
                r["状态"], r.get("说明", "")))
        P()

    内 = [r for r in 结果 if r["源"] == "内核"]
    if 内:
        P("### 内核链（与游戏版本无关）")
        P()
        for r in 内:
            P("- %s：%s" % (r["键"], r.get("说明", "")))
        P()

    txt = "\n".join(L)
    if 路径:
        open(路径, "w", encoding="utf-8").write(txt)
    return txt


def 出断言(结果, 路径):
    L = []
    L.append("// 自动生成，别手改。生成器：tools/dump_reloc/dump_reloc.py --出断言")
    L.append("// 作用：dump 一更新就先跑工具再编。哪个偏移漂了，编译直接不过，报错里带 dump 现值，")
    L.append("//       省得带着旧地址上机白测一轮。")
    L.append("// 挂载方式：SDK.h 末尾 #include 本文件（不要本文件反过来 include SDK.h，会成环）")
    L.append("#pragma once")
    L.append("")
    条 = 0
    for r in 结果:
        if not r.get("断言") or r.get("值") is None:
            continue
        if r["源"] not in ("类", "结构", "全局", "锚"):
            continue
        条 += 1
        值 = r.get("断言值") or r.get("值")
        L.append('static_assert(%s == %dULL, "偏移漂移：%s 应为 0x%X（dump 实证），去 SDK.h 改" );'
                 % (r["断言"], 值, r["键"], 值))
    L.append("")
    L.append("// 本表共 %d 条硬断言。源为 原生/运行期/内核/参考实现 的条目无法由此机制校验。" % 条)
    open(路径, "w", encoding="utf-8").write("\n".join(L) + "\n")
    return 条


def main():
    ap = argparse.ArgumentParser(description="暗区突围偏移重定位")
    ap.add_argument("--dump", required=True, help="新 dump 根目录（含 Dumpspace/）")
    ap.add_argument("--对照", help="旧 dump 根目录，用来出两版差异")
    ap.add_argument("--清单", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "偏移清单.json"))
    ap.add_argument("--报告", default="偏移核对报告.md")
    ap.add_argument("--出json", help="把解析结果写成 json")
    ap.add_argument("--出断言", help="生成编译期断言头文件")
    ap.add_argument("--严格", action="store_true", help="有漂移/查不到就返回非 0")
    a = ap.parse_args()

    dump = Dump(a.dump)
    清单 = json.load(open(a.清单, "r", encoding="utf-8"))
    结果 = [解析条目(dump, e) for e in 清单["条目"]]

    对照 = None
    对照结果 = None
    if a.对照:
        对照 = Dump(a.对照)
        对照结果 = [解析条目(对照, e) for e in 清单["条目"]]

    txt = 写报告(dump, 结果, 对照, 对照结果, a.报告)
    print(txt)

    if a.出json:
        json.dump({"dump目录": dump.目录, "条目": 结果}, open(a.出json, "w", encoding="utf-8"),
                  ensure_ascii=False, indent=1)
        print("已写 %s" % a.出json)
    if a.出断言:
        n = 出断言(结果, a.出断言)
        print("已写 %s（%d 条硬断言）" % (a.出断言, n))

    漂 = [r for r in 结果 if r["状态"] in ("漂移", "查不到", "锚没了", "越界")]
    if 漂:
        print("\n需要处理 %d 条：%s" % (len(漂), "，".join(r["键"] for r in 漂)))
    if a.严格 and 漂:
        sys.exit(2)


if __name__ == "__main__":
    main()
