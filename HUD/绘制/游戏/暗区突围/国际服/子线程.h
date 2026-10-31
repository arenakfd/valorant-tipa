// ============================================================================
//  暗区突围 国际服 —— 数据读取线程
//  读取链全部按 UAMO 1.0.249 的 dump 重写:
//     GWorld -> UWorld
//     UWorld + 0x180 -> UGameInstance + 0x38 -> LocalPlayers[0] (UPlayer + 0x30 -> PlayerController)
//     UWorld + 0x30  -> PersistentLevel + 0x98 -> Actors[]
// ============================================================================

#pragma once

#import "SDK.h"

#include <algorithm>     // r29: 名单排序(stable_sort)

// 版本标记(读取线程): 会跟诊断条一起显示, 用来确认设备上跑的是哪一版
inline const char* g_读取线程版本 = "r40";

// ---- r28: 退出标志定义在内核层 kfd.h(g_要退出), 读取线程循环里检查它 ----

// ---- r28: 相机生命周期状态(治"第一把打完开第二把 → 相机没数据") ----
//   根子: 相机地址是"一旦拿到就永久留着"的(清空() 只清角色, 注释里写明不动相机)。
//         而换局时游戏会重建 PlayerController / 相机管理器 —— 旧地址成了上一把的尸体。
//         更麻烦的是换局那一瞬读取链必然断一次, 断了只清角色 → 那个死相机永远不会被清,
//         也永远不会触发重认(扫表兜底只在"地址非法"时才走, 而死地址是"合法但已死")。
//   修法: 给相机加心跳 —— 每轮验一次那格 POV 读不读得出合理值; 验不过当场作废 + 当场重认;
//         另外 World / PlayerController 一变(＝换局)也立刻重认。
inline uint64_t g_上轮World    = 0;
inline uint64_t g_上轮控制器   = 0;
inline int      g_相机失效次数 = 0;
inline int      g_相机重认次数 = 0;
inline int      g_换局次数     = 0;
inline double   g_上次重认时间 = 0.0;
inline bool     g_要重认相机   = false;   // 主线程置位 → 读取线程下一轮优先重认
inline string   g_相机状态     = "未认";
inline string   g_相机状态说明 = "";

// ---- r28: 内核重试退避(治"关掉软件再打开, 内核就失败") ----
//   旧写法: 内核没起来时, 每个 25ms 循环都猛冲一次初始化 —— 注释写的是"每 5 秒自动重试",
//   代码实际是每秒 40 次 dlopen + 内核原语初始化 + proc_find。越狱库内部的进程/任务
//   引用是有限资源, 这么冲几轮就打满 —— 之后永久失败, 只能重启游戏。
inline double g_下次内核重试 = 0.0;
inline int    g_内核连败     = 0;

// ---- 修复版13: 坐标三源差值诊断(读取线程每轮重置, 上屏) ----
//   甲 = 根相对位置(0x154)  乙 = 根世界变换(0x120)  丙 = 网格世界变换(0x120)
//   甲乙差 > 0  → 根组件被挂到别的组件上了, 相对位置≠世界坐标
//   甲丙差 ≈ 身高以下 → 网格就在角色身上(正常); 差得离谱 → 网格偏移/缩放不对
float g_坐标差甲乙 = 0.0f;
float g_坐标差甲丙 = 0.0f;
int   g_乙可用数   = 0;
int   g_丙可用数   = 0;

inline bool 坐标可用(UE4类型::FVector 值) {
    if (!(值.X == 值.X) || !(值.Y == 值.Y) || !(值.Z == 值.Z)) return false;
    if (fabsf(值.X) > 1.0e7f || fabsf(值.Y) > 1.0e7f || fabsf(值.Z) > 1.0e7f) return false;
    return true;
}
inline float 坐标间距(UE4类型::FVector 甲, UE4类型::FVector 乙) {
    float dx = 甲.X - 乙.X, dy = 甲.Y - 乙.Y, dz = 甲.Z - 乙.Z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// ============================================================================
//  在场判定 —— 治"没人的空地上也冒框"(修复版19 引入 / 修复版20 改成"绝不误杀活人")
//
//  症状: 场地的空白处(还没刷人的空地)也画出框。
//  根因: 目标是从 UWorld->PersistentLevel->Actors[] 全表捞的, 旧版只筛了
//        "类继承自 SGCharacterBase + 指针有效 + 位置有数 + 没死"。
//        而预生成待刷新 / 刚被收走还没清干净的角色**这三条全过** ——
//        指针在、根组件在、网格在、位置也有数, 唯独被藏起来了。
//        所以要按"在不在场"再筛一道, 而不是按"人有没有被画到"。
//
//  ── 修复版20 为什么把判据全部重写 ───────────────────────────────────
//  r19 是"命中任意一条就作废", 上机发现**有些活人也被丢掉不画**。
//  根子在于那几条标志都不是"只有预生成角色才会为 1":
//    · bHidden / bHiddenInGame —— 游戏和引擎自己也会拿来用(切视角、开关镜、
//      性能裁剪、蓝图逻辑), 活人也会瞬时被置位;
//    · bVisible —— 那是**组件级**可见性, 任何一个部件的 SetVisibility 都能让它抖;
//    · 而且这三个位号是从 dump 抄的, 只要跟运行时差一位, 命中就变成随机的,
//      那就必然隔三差五误杀 —— 单条判死 = 拿活人赌位号。
//
//  所以改成"先认身份, 再看证据, 最后才作废"(三档):
//    门0 身份门:  有 PlayerState  → 有玩家身份的活角色 → **硬豁免, 永不丢**
//                 有 Controller   → 被 possess 过的角色 → 作废门槛提到 3 票
//                 (预生成待刷新 / 池化未激活的角色, 这两样都没有 —— 它们才该被筛)
//    门1 硬作废:  bActorIsBeingDestroyed = 1 → 丢(但仍过身份门)
//    门2 多票:    bHidden / bHiddenInGame / bVisible 三条里**同时**命中
//                 ≥2 条(有控制器的要 ≥3 条)才叫"藏着"
//    门3 连续帧:  门2 命中的目标还要**连续 6 轮**都命中才真丢 ——
//                 单帧抖动、忽隐忽现的一律照画
//
//  单条命中 = 只计数(软命中), 照画不上屏就丢。
//  网格 + SkeletalMesh 指针无效 = 空壳, 只计数不作废。
//  总原则: **宁可多画, 绝不丢人**。
//
//  ── r27 为什么还要再改一次(r20 之后仍然漏人) ────────────────────────
//  上机反馈"还是会漏掉人"。逐条回查, r20 这版有三处在**活人身上也会成立**:
//    ① 第三条证据 = RootComponent 的 bVisible(0x184 位6)。
//       位号本身是对的(dump 实证), 但它是**组件级可见性**, 角色的胶囊根本组件
//       常态就被游戏设成 0(胶囊体不该被渲染)。也就是说这一票对**每个角色恒真**,
//       "≥2 票"实际等于"再中任意一条就丢" —— 门槛形同虚设。
//       → 这一票删掉。r19 段头自己写明"SetActorHiddenInGame(true) 会把 Actor::bHidden
//         和组件上的 bHiddenInGame **一起**置位", 所以"① Actor::bHidden ② 网格
//         bHiddenInGame 两条同时中"才是真隐藏, 单条一律只计数。
//    ② 身份门只豁免"有 PlayerState"的。人机身上没有 PlayerState, 只有 AIController →
//       活的人机拿不到豁免, 还得靠票数自保。
//       → 改成"有 PlayerState **或** Controller 任一即硬豁免": 被 possess 过的就是活人。
//    ③ bActorIsBeingDestroyed 单帧命中就丢, 没有连续帧确认。
//       → 改成连续 2 轮确认, 且只在无身份时生效。
//  另外加了**全灭保险**: 一轮里如果判定把**所有**候选都标成可疑 → 全部放行。
//  判据整体失准的时候(偏移漂了、位号变了), 你得到的是"多画", 不是"全黑"。
//
//  ── ★ r34 定论: 这道门正式降级为「只上报, 绝不剔人」 ───────────────────
//  绕了三版(19 引入 → 20 收紧 → 27 放宽), 最后靠**成品内核的实证**结案:
//
//  把皮卡丘那份内核(实测不漏人)的丢人链逐指令重建出来 —— 它只有 5 道门, 全是游戏
//  自己的客观事实: ① 空指针 ② 是自己 ③ 人形判据(看字段值 0x4CC==40.0f, 不看名字)
//  ④ 同队 ⑤ 死亡组件位。**整条链上压根没有"判隐藏"这一环。**
//
//  它靠这个不漏人 = 反证了: 这个游戏不需要这道门也能把人画全。
//  而我们的漏人和"名额"是两种漏法 —— 名额不够是"排在后面被顶掉", 这道门是
//  "人还没进名单就被吃掉"; 位子放到 192、放到 1920 都治不到它。
//
//  代价算得很清楚:
//    · 多画一个被藏起来的目标 = 画面上多一个框, 无害
//    · 漏掉一个活人        = 判定失败
//  所以 r34 把剔除动作整个拿掉: 判定照算、计数照上报(诊断第二行数字照旧),
//  但**一个都不从名单里删** —— 本帧丢恒为 false。
//  想找回"空地不冒框"? 那本来就跟"不漏人"冲突, 这里按不漏人优先。
// ============================================================================
#include <map>

enum { 在场_没丢 = 0, 在场_丢隐藏 = 1, 在场_丢销毁 = 2, 在场_空壳 = 3, 在场_软命中 = 4 };

int  g_在场丢隐藏 = 0;   // 被隐藏丢掉的个数(已过连续帧确认)
int  g_在场丢销毁 = 0;   // 正在销毁丢掉的个数(已过连续帧确认)
int  g_在场丢空壳 = 0;   // 没有骨骼资源的个数(只上报, 不作废)
int  g_在场无控制 = 0;   // 没有控制器的个数(只上报)
int  g_在场软命中 = 0;   // 证据不足 / 连续帧不够 —— 只上报, 照画
int  g_在场豁免   = 0;   // 有身份(PlayerState 或 Controller)被硬豁免的个数
int  g_在场全灭放行 = 0; // r27: 判定要把整轮候选全丢掉 → 触发保险, 全部放行的个数
bool g_在场过滤开关 = true;  // 绘制线程每帧按 plist "在场密钥" 写; 默认开

// ★★★ r40: 队友过滤 —— 判据 100% 照搬成品内核(皮卡丘), 实测不漏真人 ★★★
//
//  【r38 那套为什么不行】(用户实测反馈: 最后一版会漏真人)
//    r38 把判据换成 ASGTeamInfo **指针**相等。听着硬, 但漏了一个前提:
//    独狼(solo)玩家在游戏里**没有真实小队** —— TeamComponent->Team 要么是空,
//    要么指向同一个"无队伍"默认对象。于是同局两个不相干的独狼**指针相等** → 被当队友
//    剔掉。全灭保险只挡得住"整轮全剔", 挡不住"一部分真人被剔" —— 表现就是漏真人。
//    那条 SquadIndex 兜底更软: 独狼的 SquadIndex 常常同值(0/-1), 一退到它就是误剔。
//
//  【r40 用回来的 = 成品内核那一套】—— 不是"参考", 是逐指令原样搬:
//    ① 队友: 目标PS(目标+0x3D8) 的 TeamIndex(+0x648, int32)
//             == 自己PS(自己Pawn+0x3D8) 的 TeamIndex(+0x648)
//       成品内核反汇编 0x100090bec / 0x100090c48 / 0x100090cc0 三处, 就这一条比较。
//       它只在**目标 PS != 0** 时做(人机 PS 为 0 → 整段跳过, 不参与队伍判定)。
//    ② 自身两条: 目标 Actor == 自己Pawn(它查两遍, 0x100090a4c / 0x100090ad0),
//                或 目标 PS == 自己PS(0x100090ccc)。
//    ③ 全灭保险保留(护栏): 一轮里全员被判队友 → 判据悬空(换局残影/读串),
//       全部放行。绝不给你空屏, 也绝不会把某一个人单独误剔。
bool g_队友过滤开关 = true;
int  g_队友滤掉数 = 0;     // 本轮滤掉几个队友(上屏, 可验证)
int  g_自己剔掉数 = 0;     // 本轮剔掉几个"自己"(换局/观战那一瞬会漏进来)
int  g_队友判据数 = 0;     // r40: 本轮靠"队号相等"判出几个(证明这条判据真的在工作)
string g_队友诊断 = "未初始化";  // r40: 自身身份取数状态 + 自己队号(上屏)

// 连续帧确认表: Actor → 这目标已经连续几轮命中"隐藏" / "正在销毁"
struct 隐藏跟踪 { int 连续 = 0; int 销毁连续 = 0; int 最后轮 = -1; };
std::map<uint64_t, 隐藏跟踪> g_隐藏跟踪;
int g_收集轮号 = 0;

// ---- r29: 名额账(治"后刷出来的人不画") ----
//   名单总数 = 这一轮一共收了多少个角色
//   截断数   = 超出 候选名单上限 被绘制帧顶掉的个数(>0 就说明"有人因为排太后面没画到")
//   死档数   = 被沉到最后那一档的个数(死亡组件明确说死了)
inline int g_名单总数   = 0;
inline int g_名单截断数 = 0;
inline int g_名单死档数 = 0;
constexpr int 隐藏确认轮数 = 8;   // 连续这么多轮都命中才真丢(r27: 6 → 8, 更保守)
constexpr int 销毁确认轮数 = 2;   // "正在销毁"也要连续 2 轮确认(r27 新增)
constexpr int 隐藏证据门槛 = 2;   // 两条硬证据**同时**中才算藏着

inline bool 位有(uint8_t 字节, int 位) { return ((字节 >> 位) & 1) != 0; };

// 返回 在场_* ; 票数 = 命中几条隐藏证据; 门槛 = 本次作废需要几票; 豁免 = 是否硬豁免
//   r27: 豁免 = 有 PlayerState 或 Controller(任一) —— 有身份的角色就地放行, 一个字节都不再读
//        证据只剩两条硬货: ① Actor::bHidden(0x68 位5) ② 网格 bHiddenInGame(0x185 位3)
//        必须**两条同时中**才算"藏着"; 单条命中一律只计数、照画
int 在场判定(uint64_t Actor, uint64_t Mesh, uint64_t Root, int &票数, int &门槛, bool &豁免) {
    票数 = 0; 门槛 = 隐藏证据门槛; 豁免 = false;
    if (!kfd.地址判断(Actor)) return 在场_没丢;

    // ---- 门0: 身份门(r27 放宽) ----
    //  有 PlayerState(真人) 或 有 Controller(被 possess 过 —— 人机也有 AIController)
    //  → 硬豁免, 永不丢。预生成待刷新 / 池化未激活的角色这两样都没有, 它们才该被筛。
    bool 有控   = kfd.地址判断(kfd.取vreadbuf<uint64_t>(Actor + SDK::APawn::Controller));
    bool 有玩家 = kfd.地址判断(kfd.取vreadbuf<uint64_t>(Actor + SDK::APawn::PlayerState));
    if (有控 || 有玩家) { 豁免 = true; return 在场_没丢; };

    // ---- 证据票(r27: 删掉 bVisible 那条恒真票, 只剩两条硬证据) ----
    //   ① Actor::bHidden —— SetActorHiddenInGame 会置它
    //   ② 网格 bHiddenInGame —— 同一个调用会一起置它
    //   两条独立读、各算一票, 门槛 = 2 → 必须同时中。
    if (位有(kfd.取vreadbuf<uint8_t>(Actor + SDK::AActor标志::bHidden), 5)) 票数++;
    if (kfd.地址判断(Mesh) &&
        位有(kfd.取vreadbuf<uint8_t>(Mesh + SDK::USceneComponent标志::bHiddenInGame), 3)) 票数++;

    // ---- 门1: 正在销毁(由调用方做连续帧确认) ----
    if (位有(kfd.取vreadbuf<uint8_t>(Actor + SDK::AActor标志::bActorIsBeingDestroyed), 3)) return 在场_丢销毁;

    // ---- 门2: 两条硬证据同时中才作废(连续帧确认由调用方做) ----
    if (票数 >= 门槛) return 在场_丢隐藏;
    if (票数 > 0)     return 在场_软命中;

    // ---- 空壳: 没有骨骼网格资源 —— 只计数, 不作废 ----
    if (kfd.地址判断(Mesh) &&
        !kfd.地址判断(kfd.取vreadbuf<uint64_t>(Mesh + SDK::USkinnedMeshComponent::SkeletalMesh))) {
        return 在场_空壳;
    };

    return 在场_没丢;
};

#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cmath>

// ======================= FName 解码 (UAMO 是 UTF-16 名字池) =======================
// dump 里明确写了: Game build is likely using UTF16 FName strings.
// 名字池头部偏移各版本不一样, 这里运行期自检, 试出来哪个通用哪个。

int   g_名字池头偏移 = 0x10;    // FNamePool.Blocks[] 相对池起点的偏移(标准 UE4 = 0x10)
int   g_名字加密模式 = 0;       // 0 = 明文存放, 1 = 按长度 XOR
bool  g_名字池已自检 = false;
string g_名字池诊断 = "未自检";
int   g_名字池块位 = 16;        // FNameBlockOffsetBits: 16 = ID>>16, 15 = ID>>15
int   g_名字字符宽 = 2;         // 2 = UTF-16, 1 = ANSI

// ---- 对象表: 运行期自适应 (dump 优先 0x10/0x24, 兜底旧布局 0x00/0x14) ----
uint64_t g_对象表指针   = 0;    // 块表基址
int      g_对象数       = 0;
bool     g_对象表已定位 = false;
uint64_t g_对象项目偏移 = SDK::FUObjectArray::Objects;
uint64_t g_对象数偏移   = SDK::FUObjectArray::NumElements;
string   g_对象表诊断   = "未定位";
string g_读取诊断     = "读取链未启动";
string g_相机诊断     = "相机未读";

// ============================================================================
//  链路状态 —— 每一层通了没有, 全部上报到屏幕上。
//  上一版这些信息只存在变量里、根本没画出去, 所以出了事只能靠猜(白烧好几轮)。
//  现在断在哪一层, 屏幕上直接写名字。
// ============================================================================
struct 链路状态 {
    string 断点       = "启动";
     string 说明       = "";
    bool 内核 = false, 对象表 = false, 名字池 = false, 基类 = false;
    bool 世界 = false, 控 = false, 相机 = false, 自己 = false, 角色表 = false;
    int  角色数   = 0;
    int  相机路径 = 0;      // 1=控制器链 2=Pawn→PlayerState链 3=对象表扫
    int  轮数     = 0;
    double 最后心跳 = 0.0;
} g_链路;

// ============================================================================
//  分片找类 —— 绝不一次扫完整个对象表。
//  旧实现一次 for 到 g_对象数(几十万~几百万), 每个对象 2 次内核读 + 2 次名字解码,
//  一轮就是几百万次读、几十秒起步; 而且失败后每 25ms 又重来一遍 ——
//  读取线程被永久钉死在这里, World/相机/Actor 列表永远走不到 = 屏幕全灭。
//  现在一次只扫一段, 扫完即返回, 下一轮接着扫, 谁都不卡。
// ============================================================================
struct 找类任务 {
    string   目标;
    int      游标 = 0;
    uint64_t 结果 = 0;
    bool     完成 = false;
    double   开始 = 0.0;
};
找类任务 g_找类任务;

// 每轮最多扫多少个对象(够快, 又不至于把一轮拖太久)
constexpr int 找类每轮步长 = 6000;

// 取第 i 个对象指针(按已定位的布局)
uint64_t 取对象(int i) {
    if (!g_对象表已定位 || i < 0 || i >= g_对象数) return 0;
    uint64_t 块号 = (uint64_t)i / SDK::FUObjectArray::ChunkSize;
    uint64_t 块内 = (uint64_t)i % SDK::FUObjectArray::ChunkSize;
    uint64_t 块 = kfd.取vreadbuf<uint64_t>(g_对象表指针 + 块号 * 8);
    if (!kfd.地址判断(块)) return 0;
    uint64_t 对象 = kfd.取vreadbuf<uint64_t>(块 + 块内 * SDK::FUObjectArray::ItemStride);
    if (!kfd.地址判断(对象)) return 0;
    return 对象;
};

// 定位对象表: 三种布局都试, 用「前几个对象的 ClassPrivate 是否有效」交叉验证
bool 定位对象数组(uint64_t 模块地址) {
    if (g_对象表已定位) return true;

    // {块表指针偏移, 对象数偏移, 名字} —— 第一条是本版本 dump 实测, 后面两条是历史布局兜底
    const uint64_t 候选指针[3] = { SDK::FUObjectArray::Objects,   SDK::FUObjectArray::备用Objects,     SDK::FUObjectArray::Objects };
    const uint64_t 候选数量[3] = { SDK::FUObjectArray::NumElements, SDK::FUObjectArray::备用NumElements, SDK::FUObjectArray::MaxElements };
    const char*    候选名  [3] = { "0x10/0x24", "0x00/0x14", "0x10/0x20" };

    uint64_t 指针基址 = 模块地址 + SDK::GObjects;
    string 诊断;

    for (int c = 0; c < 3; c++) {
        // GObjects 符号处就是 FChunkedFixedUObjectArray 本体(内联), 直接读字段
        uint64_t 表 = kfd.取vreadbuf<uint64_t>(指针基址 + 候选指针[c]);
        if (!kfd.地址判断(表)) {
            诊断 += string("[") + 候选名[c] + ":块表空]";
            continue;
        };

        int 数 = kfd.取vreadbuf<int>(指针基址 + 候选数量[c]);
        if (数 < 1000 || 数 > 3000000) {
            诊断 += string("[") + 候选名[c] + ":数=" + to_string(数) + "]";
            continue;
        };

        uint64_t 块0 = kfd.取vreadbuf<uint64_t>(表);
        if (!kfd.地址判断(块0)) {
            诊断 += string("[") + 候选名[c] + ":块0空]";
            continue;
        };

        int 命中 = 0;
        for (int i = 0; i < 8 && i < 数; i++) {
            uint64_t 对象 = kfd.取vreadbuf<uint64_t>(块0 + (uint64_t)i * SDK::FUObjectArray::ItemStride);
            if (!kfd.地址判断(对象)) continue;
            if (kfd.地址判断(kfd.取vreadbuf<uint64_t>(对象 + SDK::UObject::ClassPrivate))) 命中++;
        };
        if (命中 < 3) {
            诊断 += string("[") + 候选名[c] + ":命中" + to_string(命中) + "/8]";
            continue;
        };

        g_对象表指针 = 表;
        g_对象数 = 数;
        g_对象项目偏移 = 候选指针[c];
        g_对象数偏移 = 候选数量[c];
        g_对象表已定位 = true;
        g_对象表诊断 = string("布局") + 候选名[c] + " 对象数=" + to_string(数);
        return true;
    };

    g_对象表诊断 = "定位失败 " + 诊断;
    return false;
};

uint16_t 算XOR密钥(int 长度) {
    switch (长度 % 9) {
        case 0: return ((((长度 >> 6) & 0x1F) + 长度) | 0x7F) + 0x80;
        case 1: return (((长度 ^ 0xDF) + 长度) | 0x7F) + 0x80;
        case 2: return (((长度 | 0xCF) + 长度) | 0x7F) + 0x80;
        case 3: return ((0x21 * 长度) | 0x7F) + 0x80;
        case 4: return ((长度 + (长度 >> 8)) | 0x7F) + 0x80;
        case 5: return ((3 * 长度 + 5) | 0x7F) + 0x80;
        case 6: return ((((4 * 长度) | 5) + 长度) | 0x7F) + 0x80;
        case 7: return ((((长度 >> 10) | 7) + 长度) | 0x7F) + 0x80;
        case 8: return (((长度 ^ 0xC) + 长度) | 0x7F) + 0x80;
    };
    return 0;
};

// 从 FNamePool 里把一个 index 解成字符串; 失败返回空串
string 名字池取名字(uint64_t 池, int ID) {
    if (ID <= 0 || ID > 0x3FFFFF) return "";

    // 块 = (ID >> 块位) * 8, 块内 = (ID & mask) * 2   (块位由自检定, 默认 16)
    uint64_t 块 = (uint64_t)(ID >> g_名字池块位) * 8;
    uint64_t 偏移 = (uint64_t)(ID & ((1 << g_名字池块位) - 1)) * 2;

    uint64_t 块指针 = kfd.取vreadbuf<uint64_t>(池 + g_名字池头偏移 + 块);
    if (!kfd.地址判断(块指针)) return "";

    uint64_t 条目 = 块指针 + 偏移;
    uint16_t 头 = kfd.取vreadbuf<uint16_t>(条目);
    int 长度 = 头 >> 6;
    if (长度 <= 0 || 长度 > 200) return "";

    uint16_t 缓冲[200] = {0};
    kfd.取vreadbuf列表(条目 + 2, 缓冲, 长度 * 2);

    if (g_名字字符宽 == 1) {
        // ANSI: 每字符 1 字节
        uint16_t 密钥 = 算XOR密钥(长度);
        const uint8_t* 字节 = (const uint8_t*)缓冲;
        char 输出[256] = {0};
        int 写 = 0;
        for (int i = 0; i < 长度 && 写 < 250; i++) {
            uint8_t b = 字节[i];
            if (g_名字加密模式 == 1) b ^= (uint8_t)(密钥 & 0xFF);
            if (b == 0) break;
            输出[写++] = (char)b;
        };
        return string(输出);
    };

    if (g_名字加密模式 == 1) {
        uint16_t 密钥 = 算XOR密钥(长度);
        for (int i = 0; i < 长度; i++) 缓冲[i] ^= 密钥;
    };

    // UTF-16 -> UTF-8
    char 输出[640] = {0};
    int 写 = 0;
    for (int i = 0; i < 长度 && 写 < 636; i++) {
        uint16_t c = 缓冲[i];
        if (c == 0) break;
        if (c < 0x80) {
            输出[写++] = (char)c;
        } else if (c < 0x800) {
            输出[写++] = (char)(0xC0 | (c >> 6));
            输出[写++] = (char)(0x80 | (c & 0x3F));
        } else {
            输出[写++] = (char)(0xE0 | (c >> 12));
            输出[写++] = (char)(0x80 | ((c >> 6) & 0x3F));
            输出[写++] = (char)(0x80 | (c & 0x3F));
        };
    };
    输出[写] = 0;
    return string(输出);
};

// 对象名 (UObject + 0x18 是 FName)
string 取对象名(uint64_t 对象, uint64_t 名字池) {
    if (!kfd.地址判断(对象)) return "";
    return 名字池取名字(名字池, kfd.取vreadbuf<int>(对象 + SDK::UObject::NamePrivate));
};

// 名字是否像真的 UE 对象名 (全可打印 + 首字符是字母/下划线)
bool 名字可读(string 名) {
    if (名.size() < 2 || 名.size() > 100) return false;
    unsigned char 首 = (unsigned char)名[0];
    if (!(isalpha(首) || 首 == '_')) return false;
    for (size_t i = 0; i < 名.size(); i++) {
        unsigned char u = (unsigned char)名[i];
        if (u < 0x20 || u > 0x7E) return false;
    };
    return true;
};

// 名字池自检: 拿 GObjects 第 0 个对象的名字试各种组合, 挑第一个读出可打印名字的
void 名字池自检(uint64_t 模块地址) {
    if (g_名字池已自检) return;
    g_名字池已自检 = true;

    uint64_t 池 = 模块地址 + SDK::GNames;

    // 先定位对象表(块表指针 + 对象数), 这是整条读取链的地基
    if (!定位对象数组(模块地址)) {
        g_名字池诊断 = "✗ 对象表读不出 (" + g_对象表诊断 + ")";
        内核记日志(g_名字池诊断);
        return;
    };

    int 候选头[]   = {0x10, 0x8, 0x0, 0x18, 0x20, 0x30, 0xC8, 0xD8};
    int 候选块位[] = {16, 15};
    int 候选宽[]   = {2, 1};
    int 候选模式[] = {0, 1};
    int 试过 = 0;

    for (int h = 0; h < (int)(sizeof(候选头) / sizeof(int)); h++) {
        for (int b = 0; b < 2; b++) {
            for (int w = 0; w < 2; w++) {
                for (int m = 0; m < 2; m++) {
                    g_名字池头偏移 = 候选头[h];
                    g_名字池块位   = 候选块位[b];
                    g_名字字符宽   = 候选宽[w];
                    g_名字加密模式 = 候选模式[m];
                    试过++;

                    // ① 取对象表里前几个对象, 名字可读的个数
                    int 命中 = 0;
                    string 样例 = "";
                    for (int k = 0; k < 6; k++) {
                        uint64_t 对象 = 取对象(k);
                        if (!kfd.地址判断(对象)) continue;
                        string 名字 = 取对象名(对象, 池);
                        if (名字可读(名字)) {
                            命中++;
                            if (样例.empty()) 样例 = 名字;
                        };
                    };
                    if (命中 < 2) continue;

                    // ② 强校验: 从对象表中段挑对象, 看它的「类对象的类」名字是不是 "Class"
                    //    (UClass 的元类就是 Class 自己) —— 乱码内存不可能凑出这个
                    int 强命中 = 0;
                    int 起点 = g_对象数 / 2;
                    for (int k = 起点; k < g_对象数 && k < 起点 + 60 && 强命中 < 2; k++) {
                        uint64_t 对象 = 取对象(k);
                        if (!kfd.地址判断(对象)) continue;
                        uint64_t 元类 = kfd.取vreadbuf<uint64_t>(对象 + SDK::UObject::ClassPrivate);
                        if (!kfd.地址判断(元类)) continue;
                        uint64_t 元元类 = kfd.取vreadbuf<uint64_t>(元类 + SDK::UObject::ClassPrivate);
                        if (!kfd.地址判断(元元类)) continue;
                        if (取对象名(元元类, 池) == "Class") 强命中++;
                    };
                    if (强命中 < 1) continue;

                    char 头十六进制[16];
                    snprintf(头十六进制, sizeof(头十六进制), "%X", g_名字池头偏移);
                    g_名字池诊断 = "✓ 名字池OK 头=0x" + string(头十六进制)
                                 + " 块位=" + to_string(g_名字池块位)
                                 + " 宽=" + to_string(g_名字字符宽)
                                 + (g_名字加密模式 == 1 ? " XOR" : " 明文")
                                 + " 样例=" + 样例;
                    内核记日志(g_名字池诊断);
                    return;
                };
            };
        };
    };

    g_名字池头偏移 = 0x10;
    g_名字池块位 = 16;
    g_名字字符宽 = 2;
    g_名字加密模式 = 0;

    g_名字池诊断 = "✗ " + to_string(试过) + " 种组合都没过校验 (对象数="
                 + to_string(g_对象数) + " 对象表=0x" + to_string(g_对象表指针) + ")";
    内核记日志(g_名字池诊断);
};


// ======================= 类查找 =======================
uint64_t g_角色基类 = 0;      // ASGCharacterBase 的 UClass*
uint64_t g_名字池   = 0;
bool     g_类已定位 = false;

// 从 GObjects 里按名字找一个 UClass —— 分片增量版, 绝不阻塞读取线程
//   返回非 0            = 找到了
//   返回 0 且 完成=false = 还在扫, 下一轮接着来
//   返回 0 且 完成=true  = 扫到底了没找到(调用方自行决定要不要重开一轮)
uint64_t 找类增量(uint64_t 模块地址, const string& 目标名, bool& 完成) {
    完成 = false;
    if (!定位对象数组(模块地址)) return 0;

    if (g_找类任务.目标 != 目标名) {
        g_找类任务 = 找类任务();
        g_找类任务.目标 = 目标名;
        g_找类任务.开始 = 现在秒();
    };
    if (g_找类任务.完成) { 完成 = true; return g_找类任务.结果; };

    uint64_t 池 = g_名字池 ? g_名字池 : (模块地址 + SDK::GNames);

    int 结束 = g_找类任务.游标 + 找类每轮步长;
    if (结束 > g_对象数) 结束 = g_对象数;

    for (int i = g_找类任务.游标; i < 结束; i++) {
        uint64_t 对象 = 取对象(i);
        if (!kfd.地址判断(对象)) continue;

        // 只认 UClass: 它的 ClassPrivate 名字得是 "Class"
        uint64_t 元类 = kfd.取vreadbuf<uint64_t>(对象 + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(元类)) continue;
        if (取对象名(元类, 池) != "Class") continue;

        if (取对象名(对象, 池) == 目标名) {
            g_找类任务.结果 = 对象;
            g_找类任务.完成 = true;
            完成 = true;
            return 对象;
        };
    };

    g_找类任务.游标 = 结束;
    if (g_找类任务.游标 >= g_对象数) {
        g_找类任务.完成 = true;
        完成 = true;          // 扫完了, 确实没有
        return 0;
    };
    return 0;
};

// ============================================================================
//  ★ r41 根治"相机重认中 / 没框" —— 找 GameEngine 实例(不是读 GEngine 符号)
//
//  病根(已用 OffsetsInfo.json + GObjects-Dump.txt 双重验证):
//    无畏契约的 OFFSET_GWORLD = 0, 且根本没有导出 GEngine 全局符号;
//    CppSDK 里那个 GEngine = 0x10A1549F8 是"装饰值", dump 官方注释自己都写了
//    "GEngine and GWorld offset are not used by the SDK, it's just there for
//     decoration, use the provided UEngine::GetEngine() instead"。
//    UEngine::GetEngine() 的权威实现 = 遍历 GObjects 找 UEngine 实例,
//    根本不读 GEngine 符号。所以上一版"模块地址 + 0x0A1549F8 读指针"读到垃圾,
//    GameViewport→World 整条链从第一跳就断 → 相机永远重认失败 → 没框。
//
//  修法(走 dump 唯一可信入口 OFFSET_GOBJECTS):
//    增量遍历 GObjects, 找"类名含 GameEngine 的实例对象"(非 CDO/非 UClass),
//    拿到实例后走 +0x07D8 GameViewport → +0x70 World, 与 dump 对象树完全对上:
//      CodeVGameEngine Transient.CodeVGameEngine_2147482591
//        └─ GameViewportClient_2147482428
//        └─ CodeVGameInstance_2147482581
//  找到后缓存, 换局才重扫(实例在单局内稳定, 跟相机一样)。
// ============================================================================
struct 找引擎任务 {
    int      游标 = 0;
    uint64_t 结果 = 0;
    bool     完成 = false;
};
找引擎任务 g_找引擎任务;
uint64_t g_引擎实例缓存 = 0;   // 找到一次就缓存, 不用每帧重扫

uint64_t 找引擎实例增量(uint64_t 模块地址, bool& 完成) {
    完成 = false;

    // 已缓存且还合法 → 直接返回
    if (kfd.地址判断(g_引擎实例缓存)) { 完成 = true; return g_引擎实例缓存; };
    if (g_找引擎任务.完成) { 完成 = true; return g_找引擎任务.结果; };

    if (!定位对象数组(模块地址)) return 0;

    uint64_t 池 = g_名字池 ? g_名字池 : (模块地址 + SDK::GNames);

    int 结束 = g_找引擎任务.游标 + 找类每轮步长;
    if (结束 > g_对象数) 结束 = g_对象数;

    for (int i = g_找引擎任务.游标; i < 结束; i++) {
        uint64_t 对象 = 取对象(i);
        if (!kfd.地址判断(对象)) continue;

        // 读对象的类(ClassPrivate), 再读类的名字 = 类名
        uint64_t 类 = kfd.取vreadbuf<uint64_t>(对象 + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(类)) continue;
        string 类名 = 取对象名(类, 池);
        if (类名.empty() || !名字可读(类名)) continue;

        // 只要类名含 "GameEngine" 且对象本身不是 CDO(名字不含 Default__) 就算实例
        //   CodeVGameEngine / GameEngine 都命中; CDO 名是 "Default__CodeVGameEngine", 跳过
        if (类名.find("GameEngine") == string::npos) continue;
        string 对象名 = 取对象名(对象, 池);
        if (对象名.find("Default__") != string::npos) continue;

        g_找引擎任务.结果 = 对象;
        g_找引擎任务.完成 = true;
        g_引擎实例缓存 = 对象;
        完成 = true;
        return 对象;
    };

    g_找引擎任务.游标 = 结束;
    if (g_找引擎任务.游标 >= g_对象数) {
        g_找引擎任务.完成 = true;
        完成 = true;
        return 0;
    };
    return 0;
};

// 对象的类是否继承自 目标基类
bool 是子类(uint64_t 类, uint64_t 目标基类) {
    if (!kfd.地址判断(类) || !kfd.地址判断(目标基类)) return false;
    for (int i = 0; i < 40; i++) {
        if (类 == 目标基类) return true;
        uint64_t 父 = kfd.取vreadbuf<uint64_t>(类 + SDK::UStruct::SuperStruct);
        if (!kfd.地址判断(父) || 父 == 类) return false;
        类 = 父;
    };
    return false;
};

// ---- 类判定缓存: 同一个类只判一次。别对场上每个 Actor 都走一遍 40 层父链 ----
map<uint64_t, int> g_角色类缓存;
map<uint64_t, int> g_AI类缓存;

bool 是角色类(uint64_t 类) {
    if (!kfd.地址判断(类)) return false;
    auto 缓存 = g_角色类缓存.find(类);
    if (缓存 != g_角色类缓存.end()) return 缓存->second != 0;

    bool 结果 = false;
    if (kfd.地址判断(g_角色基类)) 结果 = 是子类(类, g_角色基类);

    // ★ 基类没定位到(或者定位错了)时的兜底 —— 上一版完全依赖基类,
    //   找类那一步一出问题就是"全场 0 个角色" = 屏幕上计数 0/0、一个框没有。
    if (!结果 && kfd.地址判断(g_名字池)) {
        string 名 = 取对象名(类, g_名字池);
        if (!名.empty() &&
            名.find("Character") != string::npos &&
            名.find("Default__") == string::npos &&
            名.find("SKEL_") == string::npos &&
            名.find("SkeletalMesh") == string::npos &&
            名.find("CharacterMovement") == string::npos &&
            名.find("AnimInstance") == string::npos &&
            名.find("Camera") == string::npos) 结果 = true;
    };

    if (g_角色类缓存.size() > 8000) g_角色类缓存.clear();
    g_角色类缓存[类] = 结果 ? 1 : 0;
    return 结果;
};

// Actor -> ClassPrivate 是否属于角色
bool 是角色(uint64_t Actor) {
    return 是角色类(kfd.取vreadbuf<uint64_t>(Actor + SDK::UObject::ClassPrivate));
};

// ★★★ r34: 人形判据的字段值旁路 (照皮卡丘实证) ★★★
//   本机 dump 实证: ACharacter::CrouchedEyeHeight [float] @ 0x4CC (1228, 大小 4) ——
//   所有 ACharacter 派生类默认就是 40.0f。皮卡丘那条链的第 3 道门用的就是这个字段值,
//   它**不看名字、不走继承链**, 只看"这个值是不是 40.0"。
//
//   我们原来的两条判据: ① 继承链(native 类名, 稳) ② 类名里含 "Character"(字符串, 会漂)。
//   ② 一漂就是整批人漏掉 —— 这正是"收集侧判据挡的"那类漏人。
//   现在补第 ③ 条: 前两条都不中时, 直接读字段值。
//
//   安全性: 读失败/vreadbuf 返回 0 → 0 不等于 40 → 不命中, 绝不会因为读不到就误收。
//   代价: 极少数非角色对象碰巧 0x4CC 处是 40.0f → 多收一个, 后面身份门/死亡判定各自把关,
//        最坏结果是多画一个框。多画一个无害, 漏一个就是判定失败。
bool 是人形字段(uint64_t Actor) {
    if (!kfd.地址判断(Actor)) return false;
    float 蹲姿眼高 = kfd.取vreadbuf<float>(Actor + SDK::ACharacter::CrouchedEyeHeight);
    return (蹲姿眼高 > 39.5f && 蹲姿眼高 < 40.5f);
};

// ============================================================================
//  ★★★ r35 新增: 取真人玩家名字 ★★★
//
//  偏移出处(本机 dump 实证, 不是抄的):
//    GObjects-Dump-WithProperties → SGFramework.SGPlayerState 名下
//      [000004E8] StrProperty PlayerNamePrivate
//    即 0x4E8, 类型 FString。(SDK.h 里这条偏移一直在, 这次才第一次接线)
//
//  ⚠️ UE4 的 FString = TArray<TCHAR>, 结构是:
//        +0x00  TCHAR* Data    ← 字符本体在**这个指针指向的堆里**
//        +0x08  int32  Num     ← 字符数(不含结尾 0)
//     所以必须**两次读**: 先读指针, 再读内容。
//     拿 FString 结构体本身当字符缓冲读 = 读到"指针的字节" = 满屏乱码。
//     (绘制算法.h 里那个旧的 取FString 就是那么写的, 一直没人调用过 —— 不要用它。)
//
//  代价控制: 一次名字 = 两次内核读(指针 + 内容)。名单里真人通常十几个, 但帧时间
//  不能这么烧 → 按 PlayerState 地址**缓存**, 命中直接返回;
//  没读到名字时 5 秒后再试(刚进局那一小段名字还没同步完)。
// ============================================================================

// 读一个 FString(标准两次读法)
string 读FString(uint64_t 串地址) {
    if (!kfd.地址判断(串地址)) return "";

    uint64_t 数据 = kfd.取vreadbuf<uint64_t>(串地址);
    int32_t  个数 = kfd.取vreadbuf<int32_t>(串地址 + 8);
    if (!kfd.地址判断(数据)) return "";
    if (个数 <= 0 || 个数 > 24) return "";     // 游戏 ID 不会超过 24 字符; 超了就是读歪了

    uint16_t 缓冲[26] = {0};
    kfd.取vreadbuf列表(数据, 缓冲, (size_t)个数 * 2);

    string 出;
    出.reserve((size_t)个数 * 2);
    for (int32_t i = 0; i < 个数; i++) {
        uint32_t 码 = 缓冲[i];
        if (码 == 0) break;
        if (码 < 0x20 || 码 == 0x7F) continue;                    // 控制字符/换行: 扔掉(不然把 HUD 排版撑开)
        if (码 < 0x80) {
            出.push_back((char)码);
        } else if (码 < 0x800) {
            出.push_back((char)(0xC0 | (码 >> 6)));
            出.push_back((char)(0x80 | (码 & 0x3F)));
        } else if (码 >= 0xD800 && 码 <= 0xDFFF) {
            continue;                                             // 代理区(emoji 的半个码点): 扔掉, 免得画出方块
        } else {
            出.push_back((char)(0xE0 | (码 >> 12)));
            出.push_back((char)(0x80 | ((码 >> 6) & 0x3F)));
            出.push_back((char)(0x80 | (码 & 0x3F)));
        };
    };
    return 出;
};

map<uint64_t, string> g_名字高速缓存;      // PlayerState 地址 → 名字
map<uint64_t, double> g_名字重试时刻;

string 取玩家名(uint64_t PlayerState) {
    if (!kfd.地址判断(PlayerState)) return "";

    auto 命中 = g_名字高速缓存.find(PlayerState);
    if (命中 != g_名字高速缓存.end() && !命中->second.empty()) return 命中->second;

    // 没读过 / 上次读到空 → 按 5 秒节流重试一次, 别每帧都白烧两次内核读
    double 现在 = 现在秒();
    auto 时刻 = g_名字重试时刻.find(PlayerState);
    if (命中 != g_名字高速缓存.end() && 时刻 != g_名字重试时刻.end() &&
        现在 - 时刻->second < 5.0) {
        return "";
    };
    g_名字重试时刻[PlayerState] = 现在;

    string 名 = 读FString(PlayerState + SDK::APlayerState::PlayerNamePrivate);

    // 缓存表长期跑会一直涨(每局的人都是新地址) → 过 1024 条整体清一次, 下一秒自然重建
    if (g_名字高速缓存.size() > 1024) {
        g_名字高速缓存.clear();
        g_名字重试时刻.clear();
    };

    g_名字高速缓存[PlayerState] = 名;
    return 名;
};

// ======================= AI 角色基类 (继承链判据) =======================
//  旧工程(2024 那版)判人机靠的是蓝图类名里带 "BP_UamAICharacter_C"。
//  蓝图名每个赛季都可能改, 所以这里不认蓝图名, 认**它爹是谁**:
//  ASGAICharacter 是个 native 类, 版本之间不会改名, 继承链也不会变。
uint64_t g_AI角色基类   = 0;
bool     g_AI基类已定位 = false;

// 类是否 AI (带缓存, 同一类只走一次父链)
bool 是AI类(uint64_t 类) {
    if (!kfd.地址判断(类)) return false;
    auto 缓存 = g_AI类缓存.find(类);
    if (缓存 != g_AI类缓存.end()) return 缓存->second != 0;
    bool 结果 = kfd.地址判断(g_AI角色基类) && 是子类(类, g_AI角色基类);
    if (g_AI类缓存.size() > 8000) g_AI类缓存.clear();
    g_AI类缓存[类] = 结果 ? 1 : 0;
    return 结果;
};

// ======================= 按类名在组件缓存表里找组件 =======================
//  ★ 无畏契约改造: 以下暗区专属的"组件缓存表扫表"函数全部作废。
//    无畏血量/后座都走 GAS 属性集（ASC 直连），不需要按类名扫组件表。
//    整段用 #if 0 保留，方便回退。
#if 0
uint64_t 找组件按类名(uint64_t 基址, uint32_t 槽数, uint64_t 名字池, const char* 目标类名) {
    if (!kfd.地址判断(基址) || !kfd.地址判断(名字池)) return 0;
    for (uint32_t i = 0; i < 槽数; i++) {
        uint64_t 组件 = kfd.取vreadbuf<uint64_t>(基址 + (uint64_t)i * 8);
        if (!kfd.地址判断(组件)) continue;
        uint64_t 类 = kfd.取vreadbuf<uint64_t>(组件 + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(类)) continue;
        if (取对象名(类, 名字池) == 目标类名) return 组件;
    };
    return 0;
};

// 找"类名里含关键字"的组件(蓝图子类带前后缀时用)
uint64_t 找组件按关键字(uint64_t 基址, uint32_t 槽数, uint64_t 名字池, const char* 关键字) {
    if (!kfd.地址判断(基址) || !kfd.地址判断(名字池)) return 0;
    string 键(关键字);
    for (uint32_t i = 0; i < 槽数; i++) {
        uint64_t 组件 = kfd.取vreadbuf<uint64_t>(基址 + (uint64_t)i * 8);
        if (!kfd.地址判断(组件)) continue;
        uint64_t 类 = kfd.取vreadbuf<uint64_t>(组件 + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(类)) continue;
        string 名 = 取对象名(类, 名字池);
        if (!名.empty() && 名.find(键) != string::npos) return 组件;
    };
    return 0;
};

// 角色身上那个权威死亡组件 (找到就缓存进玩家数据, 绘制帧直接读它的 bIsDead)
uint64_t 找死亡组件(uint64_t 角色) {
    if (!kfd.地址判断(角色) || !kfd.地址判断(g_名字池)) return 0;

    uint64_t 组件 = 找组件按类名(角色 + SDK::ASGCharacter组件表::CmpCacheArray,
                               SDK::ASGCharacter组件表::槽数, g_名字池, SDK::组件类名补::死亡组件);
    if (!kfd.地址判断(组件)) {
        组件 = 找组件按关键字(角色 + SDK::ASGCharacter组件表::CmpCacheArray,
                             SDK::ASGCharacter组件表::槽数, g_名字池, SDK::组件类名补::死亡组件关键字);
    };
    return 组件;
};

// 自己手上这把枪 + 它的后坐力组件 (找到就返回组件, 顺带把武器地址带出去)
uint64_t 找后坐力组件(uint64_t 自己Pawn, uint64_t& 武器输出) {
    武器输出 = 0;
    if (!kfd.地址判断(自己Pawn)) return 0;

    // ① 角色组件缓存表里找武器管理器
    uint64_t 武器管理器 = 找组件按类名(自己Pawn + SDK::ASGCharacter组件表::CmpCacheArray,
                                     SDK::ASGCharacter组件表::槽数, g_名字池, SDK::组件类名::武器管理器);
    if (!kfd.地址判断(武器管理器)) {
        // ② 没进缓存表就用关键字再扫一遍
        武器管理器 = 找组件按关键字(自己Pawn + SDK::ASGCharacter组件表::CmpCacheArray,
                                   SDK::ASGCharacter组件表::槽数, g_名字池, "WeaponManager");
    };
    if (!kfd.地址判断(武器管理器)) {
        // ③ 战术角色身上有直连成员, 兜底
        武器管理器 = kfd.取vreadbuf<uint64_t>(自己Pawn + SDK::ASGTacticalCharacter::CharacterWeaponManagerComponent);
    };
    if (!kfd.地址判断(武器管理器)) return 0;

    // ④ 当前手持武器
    uint64_t 武器 = kfd.取vreadbuf<uint64_t>(武器管理器 + SDK::USGCharacterWeaponManagerComponent::CurrentWeapon);
    if (!kfd.地址判断(武器)) {
        武器 = kfd.取vreadbuf<uint64_t>(武器管理器 + SDK::USGCharacterWeaponManagerComponent::LeftHandWeapon);
    };
    if (!kfd.地址判断(武器)) return 0;
    武器输出 = 武器;

    // ⑤ 武器组件表里找后坐力组件
    uint64_t 后坐力 = 找组件按类名(武器 + SDK::ASGWeapon组件表::WeaponCmpCacheArray,
                                  SDK::ASGWeapon组件表::槽数, g_名字池, SDK::组件类名::后坐力组件);
    if (!kfd.地址判断(后坐力)) {
        后坐力 = 找组件按类名(武器 + SDK::ASGInventory组件表::CmpCacheArray,
                             SDK::ASGInventory组件表::槽数, g_名字池, SDK::组件类名::后坐力组件);
    };
    return 后坐力;
};




// ============================================================================
//  找"血量属性集" —— 别再当"第 0 个属性集就是血量"
//  一个角色身上挂着一堆属性集(血量/耐力/武器…), 第 0 个不一定是血量。
//  上一版拿 SpawnedAttributes[0] 当血量读, 偏移对不上时读出来的上限是垃圾,
//  死亡判定整个失效 —— 这就是"死透了还在画"的根子。
//  现在按类名挑, 挑不到再退回第 0 个(读出来合不合理由绘制线程那边再验一遍)。
//
//  ★ r27: 新增出参 按类名命中 —— 兜底那个"第 0 个"**只准说活, 不准判死**。
//    它里面 +0x44/+0x48 根本不是血量, 碰巧落在合法区间就会把人判成死的(漏人)。
//    这个出参就是给死亡判定划的线, 见 主线程.h 四、死亡判定。
// ============================================================================
uint64_t 找健康属性集(uint64_t ASC, bool& 按类名命中) {
    按类名命中 = false;
    if (!kfd.地址判断(ASC)) return 0;

    UE4类型::TArray 属性集 = kfd.取vreadbuf<UE4类型::TArray>(
        ASC + SDK::UAbilitySystemComponent::SpawnedAttributes);
    if (!kfd.地址判断(属性集.AllocatorInstance) || 属性集.ArrayNum <= 0 || 属性集.ArrayNum > 64) return 0;

    uint64_t 兜底 = 0;
    for (int i = 0; i < 属性集.ArrayNum; i++) {
        uint64_t 集 = kfd.取vreadbuf<uint64_t>(属性集.AllocatorInstance + (uint64_t)i * 8);
        if (!kfd.地址判断(集)) continue;
        if (i == 0) 兜底 = 集;
        string 名 = 取对象名(kfd.取vreadbuf<uint64_t>(集 + SDK::UObject::ClassPrivate), g_名字池);
        if (名.find("HealthAttributeSet") != string::npos) { 按类名命中 = true; return 集; };
    };
    return 兜底;
};
#endif  // 暗区专属组件扫表函数

// ============================================================================
//  ★ r28 —— 相机生命周期: 心跳 / 重认 / 扫表兜底
//
//  要解决的问题(实机): 第一把打完, 开第二把 → 直接提示"相机没数据", 关软件重开也不行,
//  只能重启游戏。
//
//  根子逐条:
//    ① 相机地址在 自身数据 里是"一旦拿到就永久留着"的 —— 清空() 只清角色列表,
//       注释里明确写着"相机地址是稳定量, 不用丢"。在单局内这是对的(换枪换装不换相机),
//       但**换局会换相机**: 游戏重建 PlayerController / 相机管理器, 旧地址就成了尸体。
//    ② 换局那一刻读取链必然断一次(GameInstance/LocalPlayers/PlayerController 之间有
//       空窗), 断了只走 清空() → 死的旧相机永远不会被清掉。
//    ③ 原来那条"扫场上找 PlayerController"的兜底只在 `!地址判断(相机)` 时才走 ——
//       而死相机地址是**合法但已死**(地址本身还在用户态范围内), 判定永远为真,
//       于是兜底永远不触发。这就是"关了重开也救不回来"的原因。
//
//  修法(三条一起才治得住):
//    ① 心跳: 每轮读一次那格 POV, FOV/Location/Rotation 读不出合理值 = 当场判定已死;
//    ② 作废: 判死就 设置相机(0) —— 绝不让主线程继续拿尸体算投影;
//    ③ 重认: 判死 / 换局(World 或 PlayerController 变了) / 主线程请求 → 立刻重认。
//       重认先走 PC 链(最短), 拿不到就扫 Actor 表找 PlayerController(兜底)。
//       重认放在 250ms 全表节流**之前**, 所以换局后第一轮就能恢复, 不用等节流窗口。
// ============================================================================

// 这台相机现在还能读出像样的视角吗?
bool 相机还活着(uint64_t 相机) {
    if (!kfd.地址判断(相机)) return false;

    const uint64_t 视图 = 相机 + SDK::APlayerCameraManager::CameraCachePrivate + SDK::FCameraCacheEntry::POV;
    UE4类型::FVector  位置 = kfd.取vreadbuf<UE4类型::FVector>(视图 + SDK::FMinimalViewInfo::Location);
    UE4类型::FRotator 朝向 = kfd.取vreadbuf<UE4类型::FRotator>(视图 + SDK::FMinimalViewInfo::Rotation);
    float FOV = kfd.取vreadbuf<float>(视图 + SDK::FMinimalViewInfo::FOV);

    // FOV 是最干净的一条: 真视角永远在 1~179 度, 尸体内存 / 被复用的地址读出来是 0
    // 或天文数字。这一条不过就直接判死, 不再往下读。
    if (!(FOV > 1.0f && FOV < 179.0f)) return false;
    if (!坐标可用(位置)) return false;
    if (朝向.Pitch != 朝向.Pitch || 朝向.Yaw != 朝向.Yaw || 朝向.Roll != 朝向.Roll) return false;
    if (fabsf(朝向.Yaw) > 3600.0f || fabsf(朝向.Pitch) > 3600.0f) return false;

    return true;
};

// 扫 Actor 表找 PlayerController, 返回它的 CameraManager(找不到返回 0)
//   这条是 PC 链断掉时的兜底。扫表比较贵, 只在重认时才走。
uint64_t 扫表找相机(uint64_t 当前World) {
    if (!kfd.地址判断(当前World) || !kfd.地址判断(g_名字池)) return 0;

    uint64_t Level = kfd.取vreadbuf<uint64_t>(当前World + SDK::UWorld::PersistentLevel);
    if (!kfd.地址判断(Level)) return 0;

    UE4类型::TArray Actors = kfd.取vreadbuf<UE4类型::TArray>(Level + SDK::ULevel::Actors);
    if (!kfd.地址判断(Actors.AllocatorInstance) || Actors.ArrayNum <= 0 || Actors.ArrayNum > 200000) return 0;

    for (int i = 0; i < Actors.ArrayNum; i++) {
        uint64_t Actor = kfd.取vreadbuf<uint64_t>(Actors.AllocatorInstance + i * 8);
        if (!kfd.地址判断(Actor)) continue;

        uint64_t 类 = kfd.取vreadbuf<uint64_t>(Actor + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(类)) continue;

        if (取对象名(类, g_名字池).find("PlayerController") == string::npos) continue;

        uint64_t 相机 = kfd.取vreadbuf<uint64_t>(Actor + SDK::APlayerController::PlayerCameraManager);
        if (相机还活着(相机)) return 相机;
    };
    return 0;
};

// 重认: 先走 PC 链, 不行就扫表。返回可用相机, 认不到返回 0
uint64_t 重认相机(uint64_t 当前World, uint64_t PlayerController) {
    // ① PC 链(最短, 平时走这条 —— 换局后 PlayerController 通常还在, 换的是相机管理器)
    if (kfd.地址判断(PlayerController)) {
        uint64_t 相机 = kfd.取vreadbuf<uint64_t>(PlayerController + SDK::APlayerController::PlayerCameraManager);
        if (相机还活着(相机)) { g_相机状态说明 = "PC链"; return 相机; };
    };

    // ② 扫表兜底: PlayerController 指针本身也换了 / 链断了
    uint64_t 扫到 = 扫表找相机(当前World);
    if (kfd.地址判断(扫到)) { g_相机状态说明 = "扫表"; return 扫到; };

    g_相机状态说明 = "都没找到";
    return 0;
};

// ======================= 主线程数据抓取 =======================
void* 子线程(void*) {
    int 自检重试 = 0;
    int 基类候选序 = 0;

    while (true) {
        // ★ 节拍: 25ms 一拍。重的活(全 Actor 扫描 / 找类)分片或节流, 绝不整轮卡住 ——
        //   上一版找类一次扫完整个对象表(几十万~几百万个), 一轮几十秒且失败就重来,
        //   相机/角色永远走不到 → 屏幕上 0/0 全灭。
        usleep(25 * 1000);

        // ★ r28: 收到"关闭HUD"的信号 → 把内核引用放干净再退。
        //   放干净这一步是关键: 不放就走, 内核原语/句柄留在内核里,
        //   下次初始化时越狱库看到的是脏状态 → "关了再开就起不来, 得重启游戏"。
        if (g_要退出) {
            kfd.释放kfd();
            内核记日志("收到退出信号, 句柄已释放");
            _exit(0);
        };

        g_链路.轮数++;
        g_链路.最后心跳 = 现在秒();

        // 内核没起来就重试
        //   ★ r28: 带退避。旧写法是**每个 25ms 循环都猛冲一次**(注释写着"每 5 秒自动重试",
        //   代码实际是每秒 40 次) —— 每次冲都是 dlopen + 内核原语初始化 + proc_find,
        //   越狱库内部的进程/任务引用表会被打满, 之后永久失败。这就是
        //   "关掉软件绘制重开, 内核就失败, 必须重启游戏"的正主。
        //   现在: 失败后 3s → 6s → 12s → 20s 递增, 成功立刻复位。
        if (!绘制类型.内核) {
            g_链路.断点 = "内核未起";
            g_链路.内核 = false;
            double 此刻 = 现在秒();
            if (此刻 >= g_下次内核重试) {
                if (初始化内核链路(绘制类型.进程名, 绘制类型.模块名, 绘制类型.模块地址)) {
                    绘制类型.内核 = true;
                    g_内核连败 = 0;
                    g_下次内核重试 = 0.0;
                    内核记日志("✓ 后台重试成功,开始接管");
                    g_名字池已自检 = false;
                    g_类已定位 = false;
                    g_对象表已定位 = false;
                    基类候选序 = 0;
                    g_找类任务 = 找类任务();
                } else {
                    // 连败 → 拉长间隔, 绝不连续猛冲
                    g_内核连败++;
                    double 等待 = 3.0;
                    for (int i = 1; i < g_内核连败 && i < 4; i++) 等待 *= 2.0;
                    if (等待 > 20.0) 等待 = 20.0;
                    g_下次内核重试 = 此刻 + 等待;
                    内核记日志("内核未起, " + to_string((int)等待) + " 秒后再试(第 " +
                               to_string(g_内核连败) + " 次)");
                };
            };
            自身数据.清空全部();
            continue;
        };
        g_链路.内核 = true;
        g_内核连败 = 0;
        g_下次内核重试 = 0.0;

        // 模块失联检测
        // 读不通先别急着拆链 —— 皮卡丘那套是把"读写句柄"(目标进程页表)缓存住的,
        // 游戏换局/重启会让它过期: 这时重解一次句柄就能接着读, 不用重新找进程。
        static int 模块失联计数 = 0;
        if (kfd.取vreadbuf<uint64_t>(绘制类型.模块地址) == 0) {
            模块失联计数++;
            if (模块失联计数 == 2) {
                内核记日志("模块读不通,先重解读写句柄");
                kfd.解页表句柄();
            };
            if (模块失联计数 >= 6) {
                模块失联计数 = 0;
                if (kfd.重连(绘制类型.进程名)) {
                    内核记日志("✓ 进程重新接上,句柄已重建");
                    绘制类型.模块地址 = kfd.取目标模块地址(绘制类型.模块名);
                    g_名字池已自检 = false;
                    g_类已定位 = false;
                    g_对象表已定位 = false;
                    基类候选序 = 0;
                    g_找类任务 = 找类任务();
                } else {
                    内核记日志("模块读不通,重新找进程");
                    绘制类型.内核 = false;
                };
            };
            g_链路.断点 = "模块失联";
            自身数据.清空全部();
            continue;
        };
        模块失联计数 = 0;

        // ---- 1. 名字池 + 类定位 (只做一次) ----
        g_链路.断点 = "名字池";
        g_名字池 = 绘制类型.模块地址 + SDK::GNames;
        if (!g_名字池已自检) {
            名字池自检(绘制类型.模块地址);
            g_链路.对象表 = g_对象表已定位;
            if (g_名字池诊断.rfind("✓", 0) != 0) {
                // 没通过: 允许重试(游戏可能还没完全起来/版本变了), 对象表重新定位一遍
                g_名字池已自检 = false;
                g_对象表已定位 = false;
                自检重试++;
                g_读取诊断 = "名字池未过(第 " + to_string(自检重试) + " 次)";
                自身数据.清空();
                sleep(1);
                continue;   // 屏幕上会显示诊断
            };
        };
        g_链路.名字池 = true;
        g_链路.对象表 = g_对象表已定位;

        // ---- 1.2 角色基类: 分片扫描, 一次只扫一段, 扫完就往下走 ----
        //      ★ 这里不再 continue —— 基类没定位也照样往下跑。
        //        角色识别(是角色类)自带类名兜底, 不必卡在这一步上。
        if (!g_类已定位) {
            g_链路.断点 = "找角色基类";
            static const char* 候选基类[3] = { "SGCharacterBase", "SGCharacter", "ASGCharacter" };
            bool 完成 = false;
            uint64_t 基类 = 找类增量(绘制类型.模块地址, 候选基类[基类候选序], 完成);
            if (kfd.地址判断(基类)) {
                g_角色基类 = 基类;
                g_类已定位 = true;
                g_角色类缓存.clear();       // 缓存作废, 用真基类重判一遍
                内核记日志(string("✓ 角色基类已锁定: ") + 候选基类[基类候选序]);
            } else if (完成) {
                // 这个候选扫完了、确实没有 → 换下一个; 三个都没有就歇一会儿重开
                if (基类候选序 < 2) {
                    基类候选序++;
                } else {
                    基类候选序 = 0;
                    自检重试++;
                };
            };
        };
        g_链路.基类 = g_类已定位;

        // ---- 1.5 AI 角色基类 (人机判定的硬判据, 一次就够; 同样走分片) ----
        if (!g_AI基类已定位) {
            bool 完成 = false;
            uint64_t AI类 = 找类增量(绘制类型.模块地址, SDK::角色基类名::AI角色, 完成);
            if (kfd.地址判断(AI类)) {
                g_AI角色基类 = AI类;
                g_AI基类已定位 = true;
                g_AI类缓存.clear();
                内核记日志("✓ AI 基类已锁定 (继承链判定可用)");
            };
        };

        // ---- 2. World ----
        // ★ r41 根治: 无畏契约无 GWorld 符号、GEngine 符号也是装饰值, 直接读必得垃圾。
        //   正确路线: 遍历 GObjects 找 GameEngine 实例(CodeVGameEngine) →
        //             +0x07D8 GameViewport → +0x70 World
        g_链路.断点 = "World";
        uint64_t GameViewport = 0;
        uint64_t World = 0;
        {
            bool 引擎完成 = false;
            uint64_t Engine = 找引擎实例增量(绘制类型.模块地址, 引擎完成);
            if (kfd.地址判断(Engine)) {
                GameViewport = kfd.取vreadbuf<uint64_t>(Engine + SDK::UEngine::GameViewport);
            } else if (引擎完成 && !kfd.地址判断(Engine)) {
                // 扫完整张表都没有 → 重扫一遍(游戏可能刚重建引擎实例)
                g_找引擎任务 = 找引擎任务();
                g_引擎实例缓存 = 0;
            };
        };
        if (kfd.地址判断(GameViewport)) {
            World = kfd.取vreadbuf<uint64_t>(GameViewport + SDK::UGameViewportClient::World);
        };
        if (!kfd.地址判断(World)) {
            自身数据.清空();      // ★ 只清角色, 相机/自己留着(下一版起不再因一步失败就全灭)
            continue;
        };
        g_链路.世界 = true;

        // ---- 3. 本地玩家: World -> GameInstance -> LocalPlayers[0] -> PlayerController ----
        g_链路.断点 = "本地玩家";
        uint64_t GameInstance = kfd.取vreadbuf<uint64_t>(World + SDK::UWorld::OwningGameInstance);
        uint64_t PlayerController = 0;
        if (kfd.地址判断(GameInstance)) {
            UE4类型::TArray LocalPlayers = kfd.取vreadbuf<UE4类型::TArray>(GameInstance + SDK::UGameInstance::LocalPlayers);
            if (kfd.地址判断(LocalPlayers.AllocatorInstance) && LocalPlayers.ArrayNum > 0 && LocalPlayers.ArrayNum < 16) {
                uint64_t LocalPlayer = kfd.取vreadbuf<uint64_t>(LocalPlayers.AllocatorInstance);
                if (kfd.地址判断(LocalPlayer)) {
                    PlayerController = kfd.取vreadbuf<uint64_t>(LocalPlayer + SDK::UPlayer::PlayerController);
                };
            };
        };
        if (!kfd.地址判断(PlayerController)) {
            // 控制器的偏移万一改了(版本更新), 别把已经拿到的相机丢掉, 靠缓存继续跑
            g_链路.控 = false;
            自身数据.清空();
            continue;
        };
        g_链路.控 = true;

        // ---- 4. 相机 (r28: 心跳 + 换局重认) ----
        //  旧写法只有一句"读到有效地址就覆盖缓存"。问题是**读不到的时候什么都不做** ——
        //  缓存里那个上一把的死相机就一直挂着, 而它在主线程里看起来是"合法地址",
        //  于是不会被丢、也不会触发任何重认 → 屏幕上就是"相机没数据"。
        g_链路.断点 = "相机";
        {
            // ① 常规更新: PC 链读到的相机还活着才写进缓存
            uint64_t 读到的相机 = kfd.取vreadbuf<uint64_t>(PlayerController + SDK::APlayerController::PlayerCameraManager);
            uint64_t 缓存相机   = 自身数据.获取相机();
            if (kfd.地址判断(读到的相机)) {
                g_链路.相机路径 = 1;
                // 地址没变就不用再验一遍(下面心跳会统一验) —— 少一次读就少烧一点帧时间
                if (读到的相机 == 缓存相机 || 相机还活着(读到的相机)) {
                    自身数据.设置相机(读到的相机);
                    缓存相机 = 读到的相机;
                };
            };

            // ② 心跳节流: 每 4 轮(≈100ms)验一次。读内存是有代价的 ——
            //   这条链当初就是因为"每帧读太多次"才会被客户端那条帧率抖动上报盯上,
            //   修 bug 不该把老毛病带回来。真出事(换局/主线程喊)时不受节流限制。
            static int 心跳计数 = 0;
            心跳计数++;
            const bool 换局了 = (g_上轮World != 0 && g_上轮控制器 != 0) &&
                               (World != g_上轮World || PlayerController != g_上轮控制器);
            const bool 被请求 = g_要重认相机;
            g_要重认相机 = false;    // 无论做不做都清掉 —— 主线程是每帧无条件置位的

            // r41: 换局可能连 GameEngine 实例都换(引擎重建), 清缓存让下一轮重扫
            if (换局了) {
                g_引擎实例缓存 = 0;
                g_找引擎任务 = 找引擎任务();
            };

            const bool 该验心跳 = ((心跳计数 % 4) == 0) || 换局了 || 被请求;
            if (!该验心跳) {
                g_相机状态 = "正常";
            } else {
                const bool 缓存死了 = !相机还活着(缓存相机);

                // 纯"主线程说相机没数据"触发的重认要限流: 主线程每帧都置位, 不限流就变成
                // 每 25ms 扫一遍 Actor 表, 那是拿绘制帧去换一次重认, 不划算。
                // 缓存真死了 / 真换局了不受限流 —— 那两种情况必须立刻恢复。
                bool 允许 = true;
                if (被请求 && !缓存死了 && !换局了) {
                    double 此刻 = 现在秒();
                    if (此刻 - g_上次重认时间 < 3.0) 允许 = false;
                };

                if (允许 && (缓存死了 || 换局了 || 被请求)) {
                    g_上次重认时间 = 现在秒();
                    if (缓存死了) g_相机失效次数++;
                    if (换局了)   g_换局次数++;

                    uint64_t 新相机 = 重认相机(World, PlayerController);
                    if (kfd.地址判断(新相机)) {
                        自身数据.设置相机(新相机);
                        g_相机重认次数++;
                        g_相机状态 = (换局了 ? "换局重认" : "失效重认");
                    } else {
                        自身数据.设置相机(0);      // 认不回来就清掉 —— 绝不留尸体给主线程
                        g_相机状态 = "重认失败";
                    };
                } else if (缓存死了) {
                    g_相机状态 = "已失效";
                } else {
                    g_相机状态 = "正常";
                };
            };

            g_上轮World  = World;
            g_上轮控制器 = PlayerController;
        };
        g_链路.相机 = kfd.地址判断(自身数据.获取相机());

        // ---- 5. 自己 (pawn + 队伍) ----
        g_链路.断点 = "自己";
        uint64_t 自己Pawn = kfd.取vreadbuf<uint64_t>(PlayerController + SDK::APlayerController::AcknowledgedPawn);
        // ★ r38: 读不到就**保留上一轮的** —— 原来是无条件写, 于是那一瞬时 自己Pawn=0,
        //   紧接着收集循环里 `Actor == 自己Pawn` 恒不成立 → **自己漏进名单**,
        //   顶上真人数平白 +1、脚下还可能多一个自己的框。这条就是这么来的。
        //   保留是安全的: 换局走 清空全部() 会归零, 不会把上一局的尸指针带过来。
        if (!kfd.地址判断(自己Pawn)) {
            自己Pawn = 自身数据.获取自己();
        } else {
            自身数据.设置自己(自己Pawn);
        };
        g_链路.自己 = kfd.地址判断(自己Pawn);

        // ---- 5. 无后坐力: 定位自己角色的 AttributePawn（ASC + 0x2100） ----
        //   ★ 无畏后座是 GAS 属性 RecoilRate，挂在 ASC 的 AttributePawn 上，
        //     不是暗区那种"武器上的 RecoilComponent"。ASC 换局/换角色才换，
        //     每 1 秒重新读一次足够。
        static double 上次找后坐 = 0.0;
        double 此刻 = 现在秒();
        if (此刻 - 上次找后坐 >= 1.0) {
            上次找后坐 = 此刻;
            if (kfd.地址判断(自己Pawn)) {
                uint64_t 武器 = 0;
                // 无畏: ASC = 自己Pawn + 0x6E8，AttributePawn = ASC + 0x2100
                uint64_t 自己ASC = kfd.取vreadbuf<uint64_t>(自己Pawn + SDK::ASGAbilityCharacter::AbilitySystemComponent);
                uint64_t 新后坐力 = 0;
                if (kfd.地址判断(自己ASC)) {
                    新后坐力 = kfd.取vreadbuf<uint64_t>(自己ASC + SDK::USGASComponent::AttributePawn);
                };
                if (kfd.地址判断(新后坐力)) {
                    bool 换了 = (新后坐力 != g_后坐力组件);
                    g_后坐力组件 = 新后坐力;
                    g_后坐力武器 = 武器;
                    g_后坐力诊断 = "就绪";
                    if (换了) 内核记日志("✓ 后坐力 AttributePawn 已锁定");
                } else {
                    g_后坐力组件 = 0;
                    g_后坐力武器 = 0;
                    g_后坐力诊断 = "没找到 (ASC/AttributePawn 没读到)";
                };
            } else {
                g_后坐力组件 = 0;
                g_后坐力武器 = 0;
                g_后坐力诊断 = "自己没读到";
            };
        };

        // ---- 5.5 ★ r40: 自身身份 (队友判据的基准值) ----
        //   链路(全部 dump 实证, 见 SDK.h "队伍判据链"):
        //     自己Pawn(APawn::PlayerState @0x3D8) -> 自己PS
        //     自己PS + 0x648 -> TeamIndex  (int32)  ← ★ 队友主判据的基准值(成品内核同款)
        //     自己PS + 0x680 -> SquadIndex (int32)  ← 只上屏对照, 不参与过滤
        //   换局/全灭时 清空全部() 会一并归零(防上一局的队号残留被判成"同队")。
        {
            uint64_t 自身PS = 0;
            int 自身队号 = 0, 自身小队号 = -1;
            if (kfd.地址判断(自己Pawn)) {
                自身PS = kfd.取vreadbuf<uint64_t>(自己Pawn + SDK::APawn::PlayerState);
                // ★ 无畏: 自身队号读角色身上 CampID(0x6A0)，不是 PlayerState。
                自身队号   = kfd.取vreadbuf<int>(自己Pawn + SDK::ASGBasePawn::CampID);
                自身小队号 = -1;   // 无畏无 SquadIndex，置 -1 占位
            } else {
                自身PS = 0;
                自身队号 = 0;
            };
            自身数据.设置自身身份(自身PS, 自身队号);

            // 上屏诊断: PS 拿到 √ 且队号有值 = 判据是实的。PS 拿不到(×)时队号只能是 0,
            // 那时候队友过滤靠全灭保险兜住(宁可不滤, 绝不漏人)。小队号只作对照。
            g_队友诊断 = "自身PS" + string(自身PS ? "√" : "×")
                       + " 队号" + to_string(自身队号)
                       + "(小队" + to_string(自身小队号) + ")";
        };

        // ---- 6. Actor 列表 ----
        //   全表扫描比较重(几千个 Actor × 每个十来次读), 节流一轮一次。
        //   死亡/AI/位置/骨骼都不在这里判 —— 那些在绘制帧里实时读, 不受此节流影响。
        //   ★ r34: 250ms → 180ms。新 spawn 的角色最坏要等一个节流窗口才进名单,
        //     这是"延迟性漏"(忽隐忽现那种)。压缩窗口直接缩短这个空窗期。
        //     只压到 180 不压更狠: 全表扫描是内核读最重的一段, 压太狠会拖慢相机/自身那些
        //     每轮都要走的链, 得不偿失。这一步是"顺手收窄", 主刀还是在场门那刀。
        g_链路.断点 = "Actor列表";
        static double 上次全表 = 0.0;
        // ★ r35: 180ms → 100ms。成品内核是"渲染帧现扫 Actors 全表", 新刷出来的人
        //   当帧就上屏(延迟 0 帧)。我们做不到每帧全扫, 这个节流窗口就是"新人的等待时间":
        //   窗口多长, 一个刚 spawn 的活人就最多多久不在名单里。
        //   100ms ≈ 6~10 帧 —— 从"忽隐忽现能看出来"压到"基本看不出来"。
        //   为什么不压到 0/30ms: 全表扫描是整条链里最贵的一段(20 万条 × 十几次内核读),
        //   压到每帧跑会把帧时间拖毛, 反倒撞上客户端那条"帧率抖动上报"(见检测面维护)。
        //   100ms 是"漏人"和"检测面"之间那条线。
        if (此刻 - 上次全表 < 0.10) continue;
        上次全表 = 此刻;

        uint64_t Level = kfd.取vreadbuf<uint64_t>(World + SDK::UWorld::PersistentLevel);
        if (!kfd.地址判断(Level)) {
            自身数据.清空();
            continue;
        };
        UE4类型::TArray Actors = kfd.取vreadbuf<UE4类型::TArray>(Level + SDK::ULevel::Actors);
        // ★ 这里只校验 ArrayNum 本身合理。
        //   上一版还要求 ArrayNum <= ArrayMax —— 但游戏的 ArrayMax 在扩容/收缩的瞬间
        //   经常是过期的(甚至读到 0), 这一条会把正常数组判成垃圾, 然后清空 →
        //   相机被一起抹掉 → 整帧直接 return → 屏幕上 0/0 全灭。这条删了。
        if (!kfd.地址判断(Actors.AllocatorInstance) || Actors.ArrayNum <= 0 ||
            Actors.ArrayNum > 200000) {
            自身数据.清空();
            continue;
        };
        g_链路.角色表 = true;
        g_链路.断点 = "无";

        vector<物品数据> 物品数据数组;
        vector<盒子数据> 盒子数据数组;
        vector<玩家数据> 玩家数据数组;
        vector<类名数据> 类名数据数组;
        g_死源有效数 = 0;
        g_死源无效数 = 0;
        g_在场丢隐藏 = 0;
        g_在场丢销毁 = 0;
        g_在场丢空壳 = 0;
        g_在场无控制 = 0;
        g_在场软命中 = 0;
        g_在场豁免   = 0;
        g_在场全灭放行 = 0;
        g_链路.说明 = "";      // r27: 每轮清一次 —— 不然"全灭已放行"这种提示会一直粘在诊断串上
        g_收集轮号++;
        g_坐标差甲乙 = 0.0f;
        g_坐标差甲丙 = 0.0f;
        g_乙可用数 = 0;
        g_丙可用数 = 0;

        for (int i = 0; i < Actors.ArrayNum; i++) {
            uint64_t Actor = kfd.取vreadbuf<uint64_t>(Actors.AllocatorInstance + i * 8);
            if (!kfd.地址判断(Actor)) continue;

            uint64_t RootComponent = kfd.取vreadbuf<uint64_t>(Actor + SDK::AActor::RootComponent);
            // ★ r30: 根组件读不到的**不再丢人**(与 r27 "Mesh 读不到不丢人"同一口径)。
            //   原来这里是一句 `if (!地址判断(RootComponent)) continue;` —— 而新 spawn /
            //   刚从对象池里取出来的角色, 组件注册是分步完成的, 根组件指针会晚一两轮才就绪。
            //   这一句在整条收集链的**最前面**, 一旦命中, 后面 r27 那些放宽(身份豁免、Mesh
            //   放宽、宁可多画)全都轮不到执行 —— 人就这一个 continue 直接从名单里消失,
            //   表现正好是"后刷出来的人不绘制"。所以这一条必须放开:
            //   人留下, 位置源各自守(甲/乙源在下面按地址有效性单独判断), 画不画仍由绘制帧裁决。
            const bool 根可用 = kfd.地址判断(RootComponent);

            uint64_t 类 = kfd.取vreadbuf<uint64_t>(Actor + SDK::UObject::ClassPrivate);

            // ★ 相机兜底路径②(r28 修正判据): 相机管理器那条链万一断了, 就在场上找控制器实例。
            //   控制器一定带着 CameraManager, 而且是同一套偏移, 不需要新知识。
            //   ★ 这里原来判的是 `!地址判断(相机)` —— 换局之后那个死相机地址是**合法但已死**
            //     (地址本身还在用户态范围内), 判定永远为真, 兜底永远不触发。
            //     这就是"关了软件重开也救不回来、只能重启游戏"的直接原因。
            //     现在改用**心跳判据**(读得出合理 POV 才算活着), 而且顺带这一轮本来就要
            //     遍历 Actor 表, 撞见控制器就补一手, 不额外花销。
            if (!kfd.地址判断(自身数据.获取相机()) && kfd.地址判断(类) && kfd.地址判断(g_名字池)) {
                string 控名 = 取对象名(类, g_名字池);
                if (控名.find("PlayerController") != string::npos) {
                    uint64_t 备用相机 = kfd.取vreadbuf<uint64_t>(Actor + SDK::APlayerController::PlayerCameraManager);
                    if (相机还活着(备用相机)) {
                        g_链路.相机路径 = 2;
                        自身数据.设置相机(备用相机);
                        g_链路.相机 = true;
                        g_相机状态 = "扫表补认";
                        g_相机状态说明 = "扫表(遍历中)";
                    };
                };
            };

            // 只抓角色
            // ★ r34: 人形判据三路合一 —— ① 继承链 ② 类名兜底 ③ 字段值旁路(0x4CC == 40.0f)
            //   原来只有 ①②, ②一漂就整批漏; 现在 ③ 兜住。多收无害, 漏人才是问题。
            if (!是角色类(类) && !是人形字段(Actor)) continue;
            if (Actor == 自己Pawn) continue;

            uint64_t PlayerState = kfd.取vreadbuf<uint64_t>(Actor + SDK::APawn::PlayerState);
            uint64_t Mesh = kfd.取vreadbuf<uint64_t>(Actor + SDK::ACharacter::Mesh);
            // ★ r27: Mesh 读不到的**不再丢人**。
            //   原来这里一句 `if (!地址判断(Mesh)) continue;` —— 网格组件在换装/换皮/
            //   流式加载那一瞬会换指针, 只要那一轮(250ms 一次)读到 0, 这人在列表里
            //   就直接消失, 表现就是"漏人", 而且下一轮又回来 —— 忽隐忽现。
            //   方框只需要"根组件 + 位置", 跟网格半毛钱关系没有; 网格没有最多是没骨骼。
            //   所以: 人留下照画, 需要网格的东西(骨骼/源丙)各自在里面自己守。

            // ---- 在场判定(修复版19 引入 / r20 收紧 / r27 再放宽) ----
            //  门0 身份门: 有 PlayerState **或** Controller 任一 → 硬豁免, 一个字节都不再读
            //  门1 正在销毁 → 连续 2 轮确认才丢
            //  门2 两条硬证据(Actor::bHidden + 网格 bHiddenInGame)**同时**中
            //      + 连续 8 轮都命中 → 才真丢
            //  单条命中一律只计数、照画 —— 宁可多画, 绝不丢人
            int 票数 = 0, 门槛 = 隐藏证据门槛; bool 豁免 = false;
            int 在场丢因 = 在场判定(Actor, Mesh, RootComponent, 票数, 门槛, 豁免);
            if (豁免) g_在场豁免++;

            // ★ r27: 判定结果先记成"可疑", **这一轮照常进列表**, 等整轮扫完再决定丢不丢。
            //   为什么要这样: 万一判据整体失准(偏移漂了/位号变了), 原来的写法会把
            //   场上的人**一次全丢光** —— 屏幕上一个人都没有。现在有全灭保险兜底:
            //   如果一轮里"可疑"的占了全部候选, 那就不是他们可疑, 是判据可疑 → 全放行。
            // ★★★ r34 主刀: 在场判定正式降级为"只上报, 不剔人" ★★★
            //   实证依据(皮卡丘成品内核, 实测不漏人): 它整条丢人链只有 5 道门, 全是游戏自己的
            //   客观事实 —— ① 空指针 ② 自己 ③ 人形判据(字段值, 不看名字) ④ 同队 ⑤ 死亡组件位。
            //   **整条链上压根没有"判隐藏"这一环**。它靠这个不漏人, 反证了:
            //     这个游戏不需要这道门也能把人画全; 而这道门是"人在进名单之前就被吃掉",
            //     跟名额/排序是两种漏法 —— 位子放到多少都治不到它。
            //   代价对比: 多画一个被藏起来的目标 = 无害; 漏掉一个活人 = 判定失败。
            //   所以按"宁可多画, 绝不丢人"把剔除动作整个拿掉:
            //     判定照算、计数照上报(诊断第二行的数字照旧), 但**一个都不从名单里删**。
            bool 本帧丢 = false;    // ★ r34: 恒 false —— 只上报。名字保留是为了下游字段不动
            if (在场丢因 == 在场_丢销毁) {
                隐藏跟踪 &跟踪 = g_隐藏跟踪[Actor];
                if (跟踪.最后轮 == g_收集轮号 - 1) { if (跟踪.销毁连续 < 1000) 跟踪.销毁连续++; }
                else 跟踪.销毁连续 = 1;
                跟踪.最后轮 = g_收集轮号;
                if (跟踪.销毁连续 >= 销毁确认轮数) g_在场丢销毁++;   // ★ r34: 只计数, 不剔人
                else g_在场软命中++;
            } else if (在场丢因 == 在场_丢隐藏) {
                隐藏跟踪 &跟踪 = g_隐藏跟踪[Actor];
                if (跟踪.最后轮 == g_收集轮号 - 1) { if (跟踪.连续 < 1000) 跟踪.连续++; }
                else 跟踪.连续 = 1;
                跟踪.最后轮 = g_收集轮号;
                if (跟踪.连续 >= 隐藏确认轮数) g_在场丢隐藏++;      // ★ r34: 只计数, 不剔人
                else g_在场软命中++;
            } else if (在场丢因 == 在场_软命中) {
                g_在场软命中++;
            } else if (在场丢因 == 在场_空壳) {
                g_在场丢空壳++;
            };

            玩家数据 一条;
            一条.在场可疑 = 本帧丢;              // 先标记, 整轮扫完统一裁决(见尾部全灭保险)
            一条.有身份   = 豁免;
            一条.Actor = Actor;
            一条.FName = 取对象名(类, g_名字池);
            一条.RootComponent = RootComponent;
            一条.Mesh = Mesh;
            一条.PlayerState = PlayerState;

            // ---- 修复版13: 同帧取三个"目标世界坐标"来源, 互相印证 ----
            //   甲 = 根 + RelativeLocation(0x154)          —— 相对父组件的位置缓存
            //   乙 = 根 + ComponentToWorld(0x120 + 0x10)   —— 引擎每帧刷新的绝对世界变换
            //   丙 = 网格 + ComponentToWorld(0x120 + 0x10) —— 角色网格的世界位置
            //   三个都是"那个角色自己身上的值", 跟我的位置、我的朝向没有任何关系。
            {
                一条.源甲 = UE4类型::FVector{0.0f, 0.0f, 0.0f};
                一条.源乙 = UE4类型::FVector{0.0f, 0.0f, 0.0f};
                // ★ r30: 根组件没读到就别拿 0 当基址去读(和下面 Mesh 那条同一口径)。
                //   这两条读不到不算作废 —— 人已经在名单里了, 还有源丙(网格)可以顶上,
                //   三个源全无效时绘制帧自己会按"没有可靠坐标"处理(不下沉、不删)。
                if (根可用) {
                    一条.源甲 = kfd.取vreadbuf<UE4类型::FVector>(RootComponent + SDK::USceneComponent::RelativeLocation);
                    一条.源乙 = kfd.取vreadbuf<UE4类型::FVector>(RootComponent + SDK::USceneComponent::ComponentToWorld + SDK::FTransform::translation);
                };
                // ★ r27: 网格没有就别拿 0 当基址去读(读到的是垃圾, 还会白花一次内核读)
                一条.源丙 = UE4类型::FVector{0.0f, 0.0f, 0.0f};
                if (kfd.地址判断(Mesh)) {
                    一条.源丙 = kfd.取vreadbuf<UE4类型::FVector>(Mesh + SDK::USceneComponent::ComponentToWorld + SDK::FTransform::translation);
                };
                一条.甲有效 = 坐标可用(一条.源甲);
                一条.乙有效 = 坐标可用(一条.源乙);
                一条.丙有效 = 坐标可用(一条.源丙);
                if (一条.乙有效) g_乙可用数++;
                if (一条.丙有效) g_丙可用数++;
                if (一条.甲有效 && 一条.乙有效) {
                    float d = 坐标间距(一条.源甲, 一条.源乙);
                    if (d > g_坐标差甲乙) g_坐标差甲乙 = d;
                };
                if (一条.甲有效 && 一条.丙有效) {
                    float d = 坐标间距(一条.源甲, 一条.源丙);
                    if (d > g_坐标差甲丙) g_坐标差甲丙 = d;
                };
            };

            // ---- 真人 / 人机  ★★★ r40: 照搬成品内核(皮卡丘), 一条不多 ★★★
            //  逐指令从成品内核重建, 它的判据就是这一行(没有第二行)。
            //  证据 —— 它顶部那两个计数就是按这条分流的(反汇编 0x100092f5c / 0x100093000):
            //     "玩家%d"  ← 计数器 0x100fc151c, 只在 PlayerState != 0 时 +1
            //     "人机%d"  ← 计数器 0x100fc199c, 只在 PlayerState == 0 时 +1
            //  分流点(0x100090d98~0x100090dc4)原文:
            //     x8 = [目标 PlayerState]; cmp x8, 0; csel 计数槽 ← 就这一个比较。
            //
            //  它**不读** PlayerStateType(0x5F4), **不读** CachedCharacterType(0x1679),
            //  也不看类名/继承链。上一版那几条自研判据(以及它们互相打架、把真人判成
            //  人机的路)全部删掉 —— "人机开关关着时真人被误判成人机不画"这条路就此消失。
            一条.bIsAI = !kfd.地址判断(PlayerState);
            一条.gid = Actor;   // ★ 无畏无 CharacterGid，用 Actor 地址判重（换局/复活都是新 Actor）

            // 骨骼数组 (组件空间变换)
            一条.骨骼数组 = UE4类型::TArray{};
            if (kfd.地址判断(Mesh)) {
                一条.骨骼数组 = kfd.取vreadbuf<UE4类型::TArray>(Mesh + SDK::USkeletalMeshComponent::CachedComponentSpaceTransforms);
            };

            // 血量: ASGAbilityCharacter + 0x6E8 (USGASComponent) → +0x20F8 (AttributeBasic)
            //  ★ 无畏契约血量走 GAS，ASC 是稳定的属性集指针（换装/换局都不换），
            //    AttributeBasic 挂在 ASC 上。直接读，不用按类名找（暗区那套"找健康属性集"删掉）。
            uint64_t ASC = kfd.取vreadbuf<uint64_t>(Actor + SDK::ASGAbilityCharacter::AbilitySystemComponent);
            bool 血集按类名 = kfd.地址判断(ASC);   // 无畏 ASC 有效即血集可信，不靠类名挑选
            一条.HealthSet = ASC;                  // ★ 存 ASC，绘制帧再走 +0x20F8 拿 AttributeBasic
            一条.血量集可信 = 血集按类名;

            // ---- 死亡判定（无畏契约无死亡组件，改血量） ----
            //  无畏没有 USGCharacterDeathComponent，死亡判定挪到绘制帧用血量判：
            //  AttributeBasic::Health.CurrentValue <= 0 → 死。这里不缓存死亡组件。
            一条.死亡组件 = 0;
            if (kfd.地址判断(一条.死亡组件)) g_死源有效数++; else g_死源无效数++;

            // ---- ★★★ r40: 自身 / 队友判定 —— 照搬成品内核(皮卡丘)原链 ★★★
            //
            //  这一段的每一条都是从成品内核逐指令重建的(地址: 0x100090a40~0x100090cc4),
            //  顺序、判据、比较方式全部照原样, 不多加一条, 也不少一条:
            //
            //    门② 0x100090a4c: 目标 Actor == 自己Pawn(缓存全局 0x100fc16a0) → 丢
            //    门③ 0x100090a70: read32f(目标 + 0x4CC) != 40.0f → 丢   ← 人形门(见上, 已接)
            //    门④ 0x100090ad0: 目标 Actor == 自己Pawn (它的确查了第二遍) → 丢
            //    门⑤ 0x100090ae8: 目标PS = read64(目标 + 0x3D8)
            //         目标PS != 0 时:
            //           0x100090bec: w24 = read32(目标PS + 0x648)   ← ASGPlayerState::TeamIndex
            //           0x100090b5c: x21 = read64(自己Pawn + 0x3D8) ← 自己PS
            //           0x100090c48: w8  = read32(自己PS + 0x648)
            //           0x100090cc0: cmp w24, w8 → 相等就 continue  ← ★ 队友过滤就是这一比较
            //           0x100090ccc: 目标PS == 自己PS → 丢            ← 自己(第二道)
            //         目标PS == 0 时: **整段跳过**, 不做队伍比较(人机不参与队伍判定)
            //    门⑥ 0x100090cdc: 死亡组件位(0x18E8 → +0x1F8 位0) → 丢(见下面死亡源那一段)
            //
            //  dump 1.0.249 实证: 0x3D8 = APawn::PlayerState, 0x648 = ASGPlayerState::TeamIndex(int32)
            //  —— 所以它判的是**游戏自己发的队伍编号**, 不解释内存布局, 独狼各有各的号。
            //  r38 那套 "ASGTeamInfo 指针相等" 已彻底作废(独狼的 Team 指针会互相撞车 = 漏真人)。
            {
                const uint64_t 自身PS   = 自身数据.获取自身PS();
                const int      自身队号 = 自身数据.获取自身队号();
                // 成品内核是每帧直接读自己 PS 来当基准值。我们这里由收集线程每轮刷一次,
                // 拿不到(换局/观战那一两轮)时**不比较** —— 只会多画, 绝不误剔, 方向一致。
                const bool 判据可用 = kfd.地址判断(自身PS);

                if (Actor == 自己Pawn) 一条.是自己 = true;                        // 门②
                if (判据可用 && kfd.地址判断(PlayerState) && PlayerState == 自身PS)
                    一条.是自己 = true;                                           // 门⑤的自身那道

                if (判据可用 && !一条.是自己 && kfd.地址判断(PlayerState)) {
                    // ★ 无畏: 队友判据 = 角色身上 CampID(0x6A0) 数值相等。
                    //   读自己角色 +0x6A0 和目标角色 +0x6A0 比对，相等 = 同队。
                    一条.队号 = kfd.取vreadbuf<int>(
                        Actor + SDK::ASGBasePawn::CampID);
                    if (一条.队号 == 自身队号) {
                        一条.是队友 = true;      // ★ CampID 相等 = 同队
                        g_队友判据数++;
                    };
                };
            };

            // 软计数(只上报, 不作废): 没有控制器的角色 —— 预生成 / 还没被 possess 的池化角色
            // 常有这特征。这版不拿它作废(怕误伤没 controller 的野怪), 先让它上屏, 用数说话。
            if (!kfd.地址判断(kfd.取vreadbuf<uint64_t>(Actor + SDK::APawn::Controller))) g_在场无控制++;

            玩家数据数组.push_back(一条);
        };

        // ---- 连续帧确认表清理(修复版20) ----
        //  本轮没再出现的 Actor 记录直接丢掉, 防表无限增长(场上角色数级别, 开销可忽略)
        for (auto 它 = g_隐藏跟踪.begin(); 它 != g_隐藏跟踪.end(); ) {
            if (它->second.最后轮 != g_收集轮号) 它 = g_隐藏跟踪.erase(它); else ++它;
        };

        // ---- r27 在场裁决(全灭保险) ----
        //  上面每一轮只是把目标**标成可疑**, 进没进列表现在才决定。
        //  保险逻辑: 一轮里如果"可疑"的占了**全部**候选 —— 那就不是他们可疑, 是判据可疑
        //  (偏移漂了 / 位号变了), 全部放行。判据失准时你得到的是"多画", 不是"全黑"。
        {
            int 可疑数 = 0;
            for (size_t k = 0; k < 玩家数据数组.size(); k++) if (玩家数据数组[k].在场可疑) 可疑数++;
            if (!玩家数据数组.empty() && 可疑数 == (int)玩家数据数组.size()) {
                g_在场全灭放行 = 可疑数;
                for (size_t k = 0; k < 玩家数据数组.size(); k++) 玩家数据数组[k].在场可疑 = false;
                g_链路.说明 = "在场过滤全灭→已放行";
            } else if (可疑数 > 0) {
                vector<玩家数据> 留下;
                留下.reserve(玩家数据数组.size());
                for (size_t k = 0; k < 玩家数据数组.size(); k++) {
                    if (玩家数据数组[k].在场可疑) continue;
                    留下.push_back(玩家数据数组[k]);
                };
                玩家数据数组.swap(留下);
            };
        };

        // ★★★ r38: 自身 / 队友过滤 ★★★
        //  判据**不在这**, 在收集循环里(每条已经打好 是自己 / 是队友 两个标记)。
        //  这里只做取舍 —— 这么分的好处: 判定只写一遍, 绘制和计数吃的是同一份结论,
        //  不会出现"框不画了但顶上人数还数着"这种两套口径打架。
        //
        //  两条取舍规则:
        //    ① 自己 → 永远剔。这是硬同一性(PlayerState 相等 / Actor 相等), 不存在
        //       "判据可疑"的余地。剔除自己是"顶上真人数"不把自己算进去的最后一道。
        //    ② 队友 → 按开关剔。**带全灭保险**: 如果一轮里所有人都被判成队友,
        //       那不是"满场都是自己人", 是判据本身可疑(偏移漂了 / 组件链读串了) ——
        //       这时候全部放行, 绝不给你一个空屏。宁可不滤, 绝不漏人。
        {
            g_自己剔掉数 = 0;
            g_队友滤掉数 = 0;
            g_队友判据数 = 0;

            int 队友数 = 0;
            for (size_t k = 0; k < 玩家数据数组.size(); k++) {
                if (玩家数据数组[k].是队友) 队友数++;
            };
            const bool 队友全灭 = (!玩家数据数组.empty() && 队友数 == (int)玩家数据数组.size());

            vector<玩家数据> 过滤后;
            过滤后.reserve(玩家数据数组.size());
            for (size_t k = 0; k < 玩家数据数组.size(); k++) {
                const 玩家数据& 一条 = 玩家数据数组[k];
                if (一条.是自己) { g_自己剔掉数++; continue; };
                if (g_队友过滤开关 && 一条.是队友 && !队友全灭) { g_队友滤掉数++; continue; };
                过滤后.push_back(一条);
            };
            if (队友全灭 && g_队友过滤开关) {
                g_链路.说明 = "队友过滤全灭→已放行(" + to_string(队友数) + ")";
            } else if (g_自己剔掉数 > 0 && g_链路.说明.empty()) {
                g_链路.说明 = "已剔自己" + to_string(g_自己剔掉数);
            };
            玩家数据数组.swap(过滤后);
        };

        // ---- r29: 名单排序 + 名额账(治"后刷出来的人不画") ----
        //  绘制帧每帧只吃前 候选名单上限 个名额, 而名单顺序原来就是 Actor 表创建顺序:
        //  先刷出来的(尸体 / 池化未激活 / 早刷的 AI)永远排在前面, 后刷出来的活人排后面,
        //  总数一过上限就被整段顶掉 —— 画面上就是"后来的人一直没框", 而且这把补不回来。
        //  这里按三条排一遍(**只决定谁先占名额, 一条都不删**; 画不画仍由绘制帧自己判):
        //    ① 死亡组件明确说死的 → 沉到最后
        //    ② 没死的: 有身份(PlayerState / Controller)的优先
        //    ③ 同档: 距离近的优先
        //  自己位置只读一轮(两三次内核读, 250ms 一轮), 开销可忽略。
        {
            UE4类型::FVector 自己位置{0.0f, 0.0f, 0.0f};
            bool 有自己位置 = false;
            if (kfd.地址判断(自己Pawn)) {
                uint64_t 自己根 = kfd.取vreadbuf<uint64_t>(自己Pawn + SDK::AActor::RootComponent);
                if (kfd.地址判断(自己根)) {
                    UE4类型::FVector 乙 = kfd.取vreadbuf<UE4类型::FVector>(
                        自己根 + SDK::USceneComponent::ComponentToWorld + SDK::FTransform::translation);
                    UE4类型::FVector 甲 = kfd.取vreadbuf<UE4类型::FVector>(
                        自己根 + SDK::USceneComponent::RelativeLocation);
                    if (坐标可用(乙)) { 自己位置 = 乙; 有自己位置 = true; }
                    else if (坐标可用(甲)) { 自己位置 = 甲; 有自己位置 = true; };
                };
            };

            int 死档数 = 0;
            for (size_t k = 0; k < 玩家数据数组.size(); k++) {
                玩家数据& 一条 = 玩家数据数组[k];

                // 排序档: 明确死 → 2; 活着且有身份 → 0; 活着没身份 → 1
                // 死亡标志只认严格 0/1, 读到别的值当没读到(不猜) —— 跟绘制帧同一把尺子
                bool 明确死 = false;
                if (kfd.地址判断(一条.死亡组件)) {
                    uint8_t 死 = kfd.取vreadbuf<uint8_t>(
                        一条.死亡组件 + SDK::USGCharacterDeathComponent::DeathInfo +
                        SDK::FCharacterDeathInfo::bIsDead);
                    if (死 == 1) 明确死 = true;
                };
                一条.排序档 = 明确死 ? 2 : (一条.有身份 ? 0 : 1);
                if (明确死) 死档数++;

                // 距离(拿三源里第一个能用的; 一个都没有就丢到最远档, 但仍在名单里)
                UE4类型::FVector 位置 = 一条.源乙;
                bool 有位 = 一条.乙有效;
                if (!有位 && 一条.甲有效) { 位置 = 一条.源甲; 有位 = true; };
                if (!有位 && 一条.丙有效) { 位置 = 一条.源丙; 有位 = true; };
                if (有自己位置 && 有位) 一条.距离 = 坐标间距(位置, 自己位置) * 0.01f;
                else                    一条.距离 = 1.0e6f;
            };

            std::stable_sort(玩家数据数组.begin(), 玩家数据数组.end(),
                [](const 玩家数据& A, const 玩家数据& B) {
                    if (A.排序档 != B.排序档) return A.排序档 < B.排序档;
                    return A.距离 < B.距离;
                });

            g_名单总数   = (int)玩家数据数组.size();
            g_名单死档数 = 死档数;
            g_名单截断数 = (g_名单总数 > SDK::候选名单上限)
                          ? (g_名单总数 - SDK::候选名单上限) : 0;
        };

        g_链路.角色数 = (int)玩家数据数组.size();
        g_链路.断点 = "无";

        g_读取诊断 = "对象:" + to_string(g_对象数)
                   + " 基类:" + string(g_类已定位 ? "OK" : "NO")
                   + " 相机:" + string(kfd.地址判断(自身数据.获取相机()) ? "OK" : "NO")
                   + " 自己:" + string(kfd.地址判断(自己Pawn) ? "OK" : "NO")
                   + " 角色:" + to_string(玩家数据数组.size());
        // r28: 相机生命周期单列一行 —— 换没换局、失没失效、认没认回来, 一眼可判。
        g_相机诊断 = "相机 " + g_相机状态
                   + (g_相机状态说明.empty() ? "" : "(" + g_相机状态说明 + ")")
                   + " 失效" + to_string(g_相机失效次数)
                   + " 重认" + to_string(g_相机重认次数)
                   + " 换局" + to_string(g_换局次数)
                   + " | 句柄 重连" + to_string(g_重连成功次数)
                   + "/败" + to_string(g_重连失败次数);

        绘制类型.更新数据(物品数据数组, 盒子数据数组, 玩家数据数组, 类名数据数组);
    };
};
