// ============================================================================
//  投影 / 取位核心（本版重写）
//
//  这里不要写 #import "SDK.h" —— 游戏目录下有两套 SDK.h(暗区/三角洲),
//  相对解析会撞车。所有包含本文件的调用方(主线程.h)都已先 import 自己的 SDK.h / 绘图.h(含 ImGui)。
//
//  跟上一版的区别(就是"框乱飞"的根子):
//   1. 局部→世界 的矩阵乘法修正: 上一版 Y/Z 两行把 vz 的系数取错了
//      (写成 ZPlane.X / ZPlane.Y, 正确是 ZPlane.Y / ZPlane.Z), 等于整列错位一位。
//      结果: 骨骼世界坐标随目标朝向乱变 → 框跟着飞。
//   2. 投影不再绕"旋转矩阵 + 三次点乘", 直接建相机三轴(前/右/上)再投影, 每步可自证。
//   3. 网格变换(ComponentToWorld)不再认死 0x1A0 —— 那个偏移 dumper 导不出来, 是猜的。
//      改成以 dump 确证有的 CachedWorldToLocalTransform(0x890) 求逆为主,
//      再用"网格原点必须贴着角色根位置"当场判定走哪条; 两条都不成立就不用骨骼。
//   4. 所有数值带 NaN/无穷 检查, 一帧里出现脏数据不会把框甩到天边。
// ============================================================================

#pragma once

#include <map>
#include <vector>
#include <string>
#include <stdio.h>
#include <math.h>

using namespace std;

namespace UE4算法 {

// ============================================================================
//  一、基础
// ============================================================================
inline UE4类型::FVector 向量减(UE4类型::FVector A, UE4类型::FVector B) {
    return {A.X - B.X, A.Y - B.Y, A.Z - B.Z};
};

inline UE4类型::FVector 向量加(UE4类型::FVector A, UE4类型::FVector B) {
    return {A.X + B.X, A.Y + B.Y, A.Z + B.Z};
};

inline float 点乘(UE4类型::FVector A, UE4类型::FVector B) {
    return A.X * B.X + A.Y * B.Y + A.Z * B.Z;
};

inline float 长度(UE4类型::FVector 值) {
    return sqrtf(值.X * 值.X + 值.Y * 值.Y + 值.Z * 值.Z);
};

// 数值是不是干净的数 (挡住 NaN / 无穷 / 明显跑飞的值)
inline bool 是数(float 值) {
    return 值 == 值 && 值 != HUGE_VALF && 值 != -HUGE_VALF && fabsf(值) < 1.0e10f;
};

inline bool 是数(UE4类型::FVector 值) {
    return 是数(值.X) && 是数(值.Y) && 是数(值.Z);
};

// 世界 = M × 局部    (UE4 约定: 每个 Plane 是一行, 平移放在第四行 WPlane)
inline UE4类型::FVector 变换点(UE4类型::FMatrix M, UE4类型::FVector 局部) {
    return {
        局部.X * M.XPlane.X + 局部.Y * M.YPlane.X + 局部.Z * M.ZPlane.X + M.WPlane.X,
        局部.X * M.XPlane.Y + 局部.Y * M.YPlane.Y + 局部.Z * M.ZPlane.Y + M.WPlane.Y,
        局部.X * M.XPlane.Z + 局部.Y * M.YPlane.Z + 局部.Z * M.ZPlane.Z + M.WPlane.Z,
    };
};

// ============================================================================
//  二、相机 POV / 旋转矩阵
// ============================================================================
UE4类型::FMinimalViewInfo 取POV(uint64_t POV) {
    return {
        kfd.取vreadbuf<UE4类型::FVector>(POV + SDK::FMinimalViewInfo::Location),
        kfd.取vreadbuf<UE4类型::FRotator>(POV + SDK::FMinimalViewInfo::Rotation),
        kfd.取vreadbuf<float>(POV + SDK::FMinimalViewInfo::FOV),
    };
};

// ---------------------------------------------------------------------------
//  相机视角: 不只是 Location/Rotation/FOV, 还要带上"FOV 是哪个方向"和"锁没锁宽高比"。
//  这个游戏的 FMinimalViewInfo 自带自定义 FOVMode(水平/垂直/固定比例垂直),
//  FOV 数值本身不变但焦距算法完全不同 —— 不读它就是"越往屏幕边上偏得越多"。
// ---------------------------------------------------------------------------
struct 相机视角 {
    UE4类型::FMinimalViewInfo POV;
    uint64_t 起点 = 0;        // 这个 POV 所在的 CameraCacheEntry 起点
    int      来源 = 0;        // 1 = CameraCache  2 = CameraCachePrivate  3 = ViewTarget
    float    宽高比 = 0.0f;
    bool     锁宽高比 = false;
    uint8_t  模式 = 0;
    float    偏心X = 0.0f;
    float    偏心Y = 0.0f;
    bool     有效 = false;
};

// 从一个 CameraCacheEntry 起点把整套视角读出来
相机视角 读视角(uint64_t 缓存起点, int 来源) {
    相机视角 结果;
    结果.起点 = 缓存起点;
    结果.来源 = 来源;
    if (!kfd.地址判断(缓存起点)) return 结果;

    结果.POV = 取POV(缓存起点 + SDK::FCameraCacheEntry::POV);
    if (!(结果.POV.FOV > 1.0f && 结果.POV.FOV < 179.0f)) return 结果;
    if (!是数(结果.POV.Location) || !是数(结果.POV.Rotation.Pitch) || !是数(结果.POV.Rotation.Yaw)) return 结果;

    uint64_t 视图 = 缓存起点 + SDK::FCameraCacheEntry::POV;
    结果.宽高比 = kfd.取vreadbuf<float>(视图 + SDK::FMinimalViewInfo补::AspectRatio);
    结果.锁宽高比 = (kfd.取vreadbuf<uint8_t>(视图 + SDK::FMinimalViewInfo补::bConstrainAspectRatio) & 0x01) != 0;
    结果.模式 = kfd.取vreadbuf<uint8_t>(视图 + SDK::FMinimalViewInfo补::FOVMode);
    uint64_t 偏心地址 = 视图 + SDK::FMinimalViewInfo补::OffCenterProjectionOffset;
    结果.偏心X = kfd.取vreadbuf<float>(偏心地址);
    结果.偏心Y = kfd.取vreadbuf<float>(偏心地址 + 4);

    if (!是数(结果.宽高比) || 结果.宽高比 < 0.2f || 结果.宽高比 > 5.0f) {
        结果.宽高比 = 0.0f;
        结果.锁宽高比 = false;
    };
    if (结果.模式 > SDK::FOV模式::固定比例垂直) 结果.模式 = SDK::FOV模式::水平;
    if (!是数(结果.偏心X) || fabsf(结果.偏心X) > 1.0f) 结果.偏心X = 0.0f;
    if (!是数(结果.偏心Y) || fabsf(结果.偏心Y) > 1.0f) 结果.偏心Y = 0.0f;

    结果.有效 = true;
    return 结果;
};

// ============================================================================
//  ★ 修复版12 —— 相机来源: 单一可指定 + 缓存差异上屏
//
//  为什么动这里:
//    画面是"渲染管线的一帧", 投影必须用**跟那帧同一个时间点**的相机。
//    相机管理器上并排放着三格视角缓存(当前/私有/上一帧, dump 实锤):
//         CameraCache        0x420   (引擎 GetCameraCachePOV() 返回的就是它)
//         CameraCachePrivate 0x1F70  (写在稍后, 时间戳更新)
//         LastFrameCameraCache 0xAE0 (上一帧那格)
//    上一版是"谁时间戳新用谁" —— 于是**每帧的相机来源可能在这一格那一格之间跳**,
//    两格之间差的就是一帧的转动量。表现正好是: 转视角时框整体朝反方向偏、
//    停手又贴回来; 而且同一目标这帧用A格、下帧用B格 → 框来回抖。
//    跟"方框混三源"是同一个病: 同一帧里跑两套时基, 谁都说不清哪个对。
//
//  现在: 来源**定死一格**(plist 里 相机源密钥 指定), 不再帧间跳;
//        自动模式只做兜底。三格时间戳 + 主/私两格的差角全部上屏 ——
//        差角就是"框会偏多少"的直读值: 静止时≈0, 转动时才有值 → 就是时基差。
// ============================================================================
int   g_相机源       = 2;         // 1=CameraCache(0x420) 2=CameraCachePrivate(0x1F70,默认) 3=LastFrameCameraCache(0xAE0) 0=自动择新
int   g_FOV基准      = 3;         // 0=按游戏给的 FOVMode 1=强制按高度 2=强制按宽度 3=皮卡丘口径(默认)
float g_缓时间戳[3]  = {0, 0, 0}; // 1/2/3 号缓存的时间戳(诊断)
// 本帧实际算出来的量(纯诊断, 上屏用 —— 投影错了一眼能判死)
float g_焦距像素     = 0.0f;      // 本帧用的像素焦距
float g_等效视场角   = 0.0f;      // 本帧焦距对应的等效全视场角(度)
const char* g_焦距口径名 = "皮卡丘(16:9基准)";
float g_缓存差角     = 0.0f;      // 主/私两格 POV 的旋转差(度)
float g_缓存差焦     = 0.0f;      // 主/私两格的 FOV 差

// 主/私两格差多少 —— 直读"时基差"
void 算缓存差异(uint64_t 相机管理器) {
    g_缓存差角 = 0.0f;
    g_缓存差焦 = 0.0f;
    if (!kfd.地址判断(相机管理器)) return;

    相机视角 甲 = 读视角(相机管理器 + SDK::APlayerCameraManager::CameraCache, 1);
    相机视角 乙 = 读视角(相机管理器 + SDK::APlayerCameraManager::CameraCachePrivate, 2);
    if (!甲.有效 || !乙.有效) return;

    float 偏航 = fabsf(甲.POV.Rotation.Yaw - 乙.POV.Rotation.Yaw);
    while (偏航 > 180.0f) 偏航 = fabsf(偏航 - 360.0f);
    float 俯仰 = fabsf(甲.POV.Rotation.Pitch - 乙.POV.Rotation.Pitch);
    float 翻滚 = fabsf(甲.POV.Rotation.Roll - 乙.POV.Rotation.Roll);
    g_缓存差角 = fmaxf(偏航, fmaxf(俯仰, 翻滚));
    g_缓存差焦 = fabsf(甲.POV.FOV - 乙.POV.FOV);
};

// 取当前用来渲染的那个视角。
相机视角 取视角(uint64_t 相机管理器) {
    if (!kfd.地址判断(相机管理器)) return {};

    const uint64_t 三格[3] = {
        SDK::APlayerCameraManager::CameraCache,           // 0x420   当前帧
        SDK::APlayerCameraManager::CameraCachePrivate,    // 0x1F70  私有(写得稍晚)
        SDK::APlayerCameraManager::LastFrameCameraCache,  // 0xAE0   上一帧
    };

    for (int i = 0; i < 3; i++) {
        float t = kfd.取vreadbuf<float>(相机管理器 + 三格[i]);
        g_缓时间戳[i] = ((t == t) && fabsf(t) < 1.0e9f) ? t : 0.0f;
    };

    // ① 指定来源: 定了就**只用这一格**, 每帧同一个来源, 不再跳
    if (g_相机源 >= 1 && g_相机源 <= 3) {
        相机视角 指定 = 读视角(相机管理器 + 三格[g_相机源 - 1], g_相机源);
        if (指定.有效) return 指定;
    };

    // ② 自动(兜底): 两个主缓存里取时间戳新的
    相机视角 结果;
    float 最新时间 = -3.0e38f;
    for (int i = 0; i < 2; i++) {
        相机视角 候选 = 读视角(相机管理器 + 三格[i], i + 1);
        if (!候选.有效) continue;

        bool 时戳可用 = (g_缓时间戳[i] != 0.0f);
        if (结果.有效 && 时戳可用 && g_缓时间戳[i] <= 最新时间) continue;
        最新时间 = 时戳可用 ? g_缓时间戳[i] : 最新时间;
        结果 = 候选;
    };
    if (结果.有效) return 结果;

    // ③ 兜底: 上一帧缓存 → ViewTarget.POV
    相机视角 上一帧 = 读视角(相机管理器 + 三格[2], 3);
    if (上一帧.有效) return 上一帧;

    return 读视角(相机管理器 + SDK::APlayerCameraManager::ViewTarget + SDK::FCameraCacheEntry::POV, 4);
};

// 老接口兼容
UE4类型::FMinimalViewInfo 取相机POV(uint64_t 相机管理器, int& 用的哪个) {
    相机视角 视角 = 取视角(相机管理器);
    用的哪个 = 视角.来源;
    return 视角.POV;
};

// UE4 的 FRotationMatrix(旋转角) —— 行 = 前/右/上
UE4类型::FMatrix 取矩阵(UE4类型::FRotator Rotation) {
    UE4类型::FVector2D Pitch = {
        sinf(Rotation.Pitch * M_PI / 180.0f),
        cosf(Rotation.Pitch * M_PI / 180.0f),
    };
    UE4类型::FVector2D Yaw = {
        sinf(Rotation.Yaw * M_PI / 180.0f),
        cosf(Rotation.Yaw * M_PI / 180.0f),
    };
    UE4类型::FVector2D Roll = {
        sinf(Rotation.Roll * M_PI / 180.0f),
        cosf(Rotation.Roll * M_PI / 180.0f),
    };

    return {
        Pitch.Y * Yaw.Y,
        Pitch.Y * Yaw.X,
        Pitch.X,
        0.0f,

        Pitch.X * Yaw.Y * Roll.X - Yaw.X * Roll.Y,
        Pitch.X * Yaw.X * Roll.X + Yaw.Y * Roll.Y,
        Pitch.Y * -Roll.X,
        0.0f,

        - (Pitch.X * Yaw.Y * Roll.Y + Yaw.X * Roll.X),
        Yaw.Y * Roll.X - Pitch.X * Yaw.X * Roll.Y,
        Pitch.Y * Roll.Y,
        0.0f,

        0.0f,
        0.0f,
        0.0f,
        1.0f,
    };
};

// ============================================================================
//  三、距离 / 屏幕投影
// ============================================================================
UE4类型::FVector 取坐标差(UE4类型::FVector 坐标, UE4类型::FVector Location, float 比例) {
    return {
        (坐标.X - Location.X) / 比例,
        (坐标.Y - Location.Y) / 比例,
        (坐标.Z - Location.Z) / 比例,
    };
};

float 取距离(UE4类型::FVector RelativeLocation, UE4类型::FVector Location) {
    UE4类型::FVector 坐标差 = 取坐标差(RelativeLocation, Location, 100.0f);
    return sqrtf(powf(坐标差.X, 2.0f) + powf(坐标差.Y, 2.0f) + powf(坐标差.Z, 2.0f));
};

// 相机三轴 + 焦距(像素) + 渲染区
struct 相机信息 {
    UE4类型::FVector 位置;
    UE4类型::FVector 前;
    UE4类型::FVector 右;
    UE4类型::FVector 上;
    float 焦距 = 1.0f;          // 像素焦距(横竖同一把 —— 像素是方的)
    float 视场角 = 0.0f;        // 原始 FOV, 诊断用
    int   模式 = 0;             // 0=水平 1=垂直 2=固定比例垂直
    float 基准边 = 0.0f;        // 焦距是按哪条边算的(诊断) —— 宽还是高, 一眼看出取错没
    float 中心X = 0.0f;         // 渲染区中心(锁宽高比时会缩进去)
    float 中心Y = 0.0f;
    float 渲染宽 = 1.0f;
    float 渲染高 = 1.0f;
    bool 有效 = false;
};

// ============================================================================
//  建相机 —— 本版重点
//   1. 锁宽高比(bConstrainAspectRatio)时, 渲染区不是整块屏幕, 要按 UE4 的规矩缩出黑边
//   2. 焦距按 FOVMode 决定用哪条边算:
//        HorizontalFOV         -> 用渲染区宽
//        VerticalFOV / 固定比例 -> 用渲染区高
//      这一条就是"摆动视角跟着偏"的根: 模式读错, 焦距差一整个宽高比(手机上是 2 倍多),
//      表现正好是"离屏幕中心越远偏得越多"。
//   3. 偏心投影(某些开镜方式会用到)一并算进去
// ============================================================================
相机信息 建相机(const 相机视角& 视角, float 屏宽, float 屏高, float 焦距倍率 = 1.0f) {
    相机信息 相机;
    if (!视角.有效) return 相机;
    if (!(屏宽 > 1.0f) || !(屏高 > 1.0f)) return 相机;

    // ---- 1. 渲染区 ----
    float 左 = 0.0f, 上 = 0.0f, 宽 = 屏宽, 高 = 屏高;
    if (视角.锁宽高比 && 视角.宽高比 > 0.0f) {
        float 屏幕比 = 屏宽 / 屏高;
        if (屏幕比 < 视角.宽高比) {
            高 = 屏宽 / 视角.宽高比;          // 以宽为准 → 上下黑边
            上 = (屏高 - 高) * 0.5f;
        } else {
            宽 = 屏高 * 视角.宽高比;          // 以高为准 → 左右黑边
            左 = (屏宽 - 宽) * 0.5f;
        };
    };
    相机.渲染宽 = 宽;
    相机.渲染高 = 高;

    // ---- 2. 焦距 ----
    float 半角 = 视角.POV.FOV * M_PI / 360.0f;
    float 正切 = tanf(半角);
    if (!(正切 > 0.0001f)) return 相机;

    float 基准边 = 宽;
    float 焦距 = 0.0f;
    if (g_FOV基准 == 3) {
        // ★ 修复版18 —— 皮卡丘口径(照它二进制里那条算式等价化简过来的):
        //   它先算   等效半角 = atan( tan(FOV/2) × (屏宽/屏高) ÷ (16/9) )
        //   再拿     像素焦距 = 半宽 ÷ tan(等效半角)
        //   两步合起来、约掉宽高比, 就是下面这一条:
        //                像素焦距 = (屏高/2) × (16/9) ÷ tan(FOV/2)
        //   跟旧口径("按 FOVMode 选基准边, 半高/tan 或 半宽/tan")相比, 在 FOV 被当纵向视场时
        //   差整整一个 16/9 倍 —— 表现为"离屏幕中心越远偏得越多", 也就是框跟着视角漂。
        焦距 = (高 * 0.5f) * (16.0f / 9.0f) / 正切;
        基准边 = 高 * (16.0f / 9.0f);
        g_焦距口径名 = "皮卡丘(16:9基准)";
    } else {
        float 基准边值 = 宽;
        if (g_FOV基准 == 1) {
            基准边值 = 高;                                        // 强制: FOV 当纵向视场
            g_焦距口径名 = "旧:强制按高";
        } else if (g_FOV基准 == 2) {
            基准边值 = 宽;                                        // 强制: FOV 当横向视场
            g_焦距口径名 = "旧:强制按宽";
        } else {
            基准边值 = (视角.模式 == SDK::FOV模式::水平) ? 宽 : 高; // 按游戏给的模式(老口径)
            g_焦距口径名 = (视角.模式 == SDK::FOV模式::水平) ? "旧:模式→宽" : "旧:模式→高";
        };
        基准边 = 基准边值;
        焦距 = (基准边值 * 0.5f) / 正切;
    };
    if (焦距倍率 > 0.01f && 焦距倍率 < 100.0f) 焦距 *= 焦距倍率;
    if (!(焦距 > 1.0f)) return 相机;
    相机.焦距 = 焦距;
    相机.视场角 = 视角.POV.FOV;
    相机.模式 = 视角.模式;
    相机.基准边 = 基准边;
    g_焦距像素 = 焦距;
    g_等效视场角 = 2.0f * atanf(高 * 0.5f / 焦距) * 180.0f / M_PI;

    // ---- 3. 相机三轴 ----
    float 俯仰 = 视角.POV.Rotation.Pitch * M_PI / 180.0f;
    float 偏航 = 视角.POV.Rotation.Yaw   * M_PI / 180.0f;
    float 翻滚 = 视角.POV.Rotation.Roll  * M_PI / 180.0f;

    float CP = cosf(俯仰), SP = sinf(俯仰);
    float CY = cosf(偏航), SY = sinf(偏航);
    float CR = cosf(翻滚), SR = sinf(翻滚);

    相机.位置 = 视角.POV.Location;
    相机.前 = { CP * CY, CP * SY, SP };
    相机.右 = { SP * CY * SR - SY * CR, SP * SY * SR + CY * CR, -CP * SR };
    相机.上 = { -(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP };

    // ---- 4. 渲染区中心(+ 偏心投影) ----
    相机.中心X = 左 + 宽 * 0.5f - 视角.偏心X * 宽 * 0.5f;
    相机.中心Y = 上 + 高 * 0.5f + 视角.偏心Y * 高 * 0.5f;

    相机.有效 = 是数(相机.前) && 是数(相机.右) && 是数(相机.上) && 是数(相机.中心X) && 是数(相机.中心Y);
    return 相机;
};

// 老接口: 直接给 POV 建相机(不知道 FOV 模式, 按水平算)
相机信息 建相机(UE4类型::FMinimalViewInfo POV, float 宽, float 高 = 0.0f) {
    相机视角 视角;
    视角.POV = POV;
    视角.模式 = SDK::FOV模式::水平;
    视角.有效 = (POV.FOV > 1.0f && POV.FOV < 179.0f);
    if (高 <= 1.0f) 高 = 宽;
    return 建相机(视角, 宽, 高, 1.0f);
};

// ★★★ r36 —— 世界 → 屏幕: 1:1 照参考实现的算式 ★★★
//
//  它那条投影(反汇编 0x100091c64 那一段)对深度只做一件事:
//      纵深 = max(纵深, 1.0)
//  然后就除。**没有"在相机后面就失败"、没有"太近就不算"、没有任何失败返回。**
//  这就是它不漏框的结构性原因: 目标走到多近、退到多远、是偏是正,
//  只要算得出一个屏幕点就画 —— 投影这一层从不替上层决定"这个人要不要画"。
//
//  我们原来那句 `if (!(纵深 > 20.0f)) return false;` 是"背后/贴脸直接判失败":
//     · 贴脸(2 米内的近战距离, 纵深可能不到 20cm) → 整个人没框
//     · 相机背后那零点几秒(转身瞬间目标从身后扫过) → 当帧没框
//  两道都是"明明有人、框不给"的纯丢框口, 现在按它的口径钳掉。
bool 投影点(UE4类型::FVector 世界, const 相机信息& 相机, float 宽, float 高, ImVec2& 输出) {
    if (!相机.有效 || !是数(世界)) return false;

    UE4类型::FVector 差 = 向量减(世界, 相机.位置);
    float 纵深 = 点乘(差, 相机.前);          // 相机正前方多远
    if (!是数(纵深)) return false;
    if (纵深 < 1.0f) 纵深 = 1.0f;             // ★ 照参考实现: 深度钳到 1, 不做"背后就丢"

    float 横向 = 点乘(差, 相机.右);
    float 竖向 = 点乘(差, 相机.上);

    输出 = ImVec2(相机.中心X + (横向 / 纵深) * 相机.焦距,
                  相机.中心Y - (竖向 / 纵深) * 相机.焦距);
    return 是数(输出.x) && 是数(输出.y);
};

// 老接口(三角洲模块也在用): 世界 → 屏幕, 算不出来返回 (0,0)
ImVec2 取屏幕坐标(UE4类型::FVector 世界坐标, UE4类型::FMinimalViewInfo POV, UE4类型::FMatrix 矩阵) {
    ImVec2 结果 = ImVec2(0.0f, 0.0f);
    相机信息 相机 = 建相机(POV, 设备类型.设备信息.宽度);
    if (!投影点(世界坐标, 相机, 设备类型.设备信息.宽度, 设备类型.设备信息.高度, 结果)) {
        return ImVec2(0.0f, 0.0f);
    };
    return 结果;
};

// ============================================================================
//  四、变换 / 矩阵
// ============================================================================
UE4类型::FTransform 取Transform(uint64_t ComponentToWorld) {
    return {
        kfd.取vreadbuf<UE4类型::FQuat>(ComponentToWorld + SDK::FTransform::Rotation),
        kfd.取vreadbuf<UE4类型::FVector>(ComponentToWorld + SDK::FTransform::translation),
        kfd.取vreadbuf<UE4类型::FVector>(ComponentToWorld + SDK::FTransform::Scale3D),
    };
};

// FTransform → 局部转世界矩阵 (平移落在 WPlane)
UE4类型::FMatrix 取Transform矩阵(UE4类型::FTransform Transform) {
    return {
        1.0f - (Transform.Rotation.Y * Transform.Rotation.Y * 2.0f + Transform.Rotation.Z * Transform.Rotation.Z * 2.0f) * Transform.Scale3D.X,
        (Transform.Rotation.X * Transform.Rotation.Y * 2.0f + Transform.Rotation.W * Transform.Rotation.Z * 2.0f) * Transform.Scale3D.X,
        (Transform.Rotation.X * Transform.Rotation.Z * 2.0f - Transform.Rotation.W * Transform.Rotation.Y * 2.0f) * Transform.Scale3D.X,
        0.0f,

        (Transform.Rotation.X * Transform.Rotation.Y * 2.0f - Transform.Rotation.W * Transform.Rotation.Z * 2.0f) * Transform.Scale3D.Y,
        1.0f - (Transform.Rotation.X * Transform.Rotation.X * 2.0f + Transform.Rotation.Z * Transform.Rotation.Z * 2.0f) * Transform.Scale3D.Y,
        (Transform.Rotation.Y * Transform.Rotation.Z * 2.0f + Transform.Rotation.W * Transform.Rotation.X * 2.0f) * Transform.Scale3D.Y,
        0.0f,

        (Transform.Rotation.X * Transform.Rotation.Z * 2.0f + Transform.Rotation.W * Transform.Rotation.Y * 2.0f) * Transform.Scale3D.Z,
        (Transform.Rotation.Y * Transform.Rotation.Z * 2.0f - Transform.Rotation.W * Transform.Rotation.X * 2.0f) * Transform.Scale3D.Z,
        1.0f - (Transform.Rotation.X * Transform.Rotation.X * 2.0f + Transform.Rotation.Y * Transform.Rotation.Y * 2.0f) * Transform.Scale3D.Z,
        0.0f,

        Transform.Translation.X,
        Transform.Translation.Y,
        Transform.Translation.Z,
        1.0f,
    };
};

// 四元数像不像"正常旋转"(模长≈1)
bool 变换可用(UE4类型::FTransform 变换) {
    float 模 = 变换.Rotation.X * 变换.Rotation.X + 变换.Rotation.Y * 变换.Rotation.Y
             + 变换.Rotation.Z * 变换.Rotation.Z + 变换.Rotation.W * 变换.Rotation.W;
    if (!(模 > 0.85f && 模 < 1.15f)) return false;
    float 缩放大 = fabsf(变换.Scale3D.X) + fabsf(变换.Scale3D.Y) + fabsf(变换.Scale3D.Z);
    if (!(缩放大 > 0.01f)) return false;
    return 是数(变换.Translation);
};

// 矩阵是不是个像样的旋转(三行模长接近 1)
bool 矩阵可用(UE4类型::FMatrix 矩阵) {
    float 行0 = sqrtf(矩阵.XPlane.X * 矩阵.XPlane.X + 矩阵.XPlane.Y * 矩阵.XPlane.Y + 矩阵.XPlane.Z * 矩阵.XPlane.Z);
    float 行1 = sqrtf(矩阵.YPlane.X * 矩阵.YPlane.X + 矩阵.YPlane.Y * 矩阵.YPlane.Y + 矩阵.YPlane.Z * 矩阵.YPlane.Z);
    float 行2 = sqrtf(矩阵.ZPlane.X * 矩阵.ZPlane.X + 矩阵.ZPlane.Y * 矩阵.ZPlane.Y + 矩阵.ZPlane.Z * 矩阵.ZPlane.Z);
    if (!是数(矩阵.WPlane.X) || !是数(矩阵.WPlane.Y) || !是数(矩阵.WPlane.Z)) return false;
    return 行0 > 0.6f && 行0 < 1.4f && 行1 > 0.6f && 行1 < 1.4f && 行2 > 0.6f && 行2 < 1.4f;
};

// 把"世界→局部"矩阵反过来, 得到"局部→世界"
//   W2L = [A | b]  即 local = A·世界 + b
//   → 世界 = A⁻¹·(local - b) = Aᵀ·local - Aᵀ·b      (A 是正交旋转, A⁻¹ = Aᵀ)
//   结果矩阵: 行 = Aᵀ 的三行(也就是 A 的三列), 平移放 WPlane
UE4类型::FMatrix 求逆成局部转世界(UE4类型::FMatrix 世界到局部) {
    UE4类型::FMatrix M;

    M.XPlane.X = 世界到局部.XPlane.X;  M.XPlane.Y = 世界到局部.YPlane.X;  M.XPlane.Z = 世界到局部.ZPlane.X;  M.XPlane.W = 0.0f;
    M.YPlane.X = 世界到局部.XPlane.Y;  M.YPlane.Y = 世界到局部.YPlane.Y;  M.YPlane.Z = 世界到局部.ZPlane.Y;  M.YPlane.W = 0.0f;
    M.ZPlane.X = 世界到局部.XPlane.Z;  M.ZPlane.Y = 世界到局部.YPlane.Z;  M.ZPlane.Z = 世界到局部.ZPlane.Z;  M.ZPlane.W = 0.0f;

    UE4类型::FVector b = {世界到局部.WPlane.X, 世界到局部.WPlane.Y, 世界到局部.WPlane.Z};
    M.WPlane.X = -(M.XPlane.X * b.X + M.YPlane.X * b.Y + M.ZPlane.X * b.Z);
    M.WPlane.Y = -(M.XPlane.Y * b.X + M.YPlane.Y * b.Y + M.ZPlane.Y * b.Z);
    M.WPlane.Z = -(M.XPlane.Z * b.X + M.YPlane.Z * b.Y + M.ZPlane.Z * b.Z);
    M.WPlane.W = 1.0f;
    return M;
};

// 网格原点离角色根位置多远就算不靠谱(网格一般就在胶囊中心附近)
constexpr float 网格原点容差 = 250.0f;

// ============================================================================
//  ★ 修复版11 —— 网格变换: 单一来源, 一条通道
//
//  写法照全网同行的实现(iOS 越狱插件 / 移动端 ESP 那几个开源版)收敛成一句:
//      骨骼世界坐标 = 网格的 ComponentToWorld  组合  骨骼在网格空间里的变换
//  全程只认 ComponentToWorld 一个字段。不求逆、不双通道、不做运行时扫偏移。
//
//  为什么把那两样砍掉(它们正是"框乱飞 / 转身偏"的来源):
//    · CachedWorldToLocalTransform(0x890) 求逆 —— 那几个开源实现里一次都没出现过。
//      它是 Transient 字段, 不保证和当前帧同步; 拿它当主链, 转视角那一下拿到的
//      是上一帧的姿势 → 拖影、滞后。
//    · "运行时扫偏移 + 两通道交叉互证" 听着严谨, 实际把一个固定常量变成了每帧
//      浮动的选择: 同一个角色这帧走A、下帧走B, 框就会换形状。
//    · 单一来源才可判读: 错了眼睛一看就知道是哪条链, 改也改得动。
//
//  ComponentToWorld 用 0x120 (修复版13 勘误, 旧值 0x1A0 是错的), 依据:
//    ① 硬锚点: dump 明确导出 RelativeLocation = 0x154;
//    ② 它前面 0x120 起是一段 Pad_120[0x34] 空洞, 长度 0x34;
//    ③ UE 的 FTransform = 0x30, 后补 4 字节对齐 = 0x34 —— 与这段空洞严丝合缝;
//    ④ UE 源码里 ComponentToWorld 就是紧贴 RelativeLocation 之前的成员。
//    旧值 0x1A0 落在 RelativeScale3D(0x16C)/ComponentVelocity(0x178) 之后的 bitfield 区,
//    读到的是垃圾 —— 骨骼画不出来就是它。屏幕上的"甲丙差"可以在实机上当场验这一条。
//  ★ 这个字段只有骨骼链在用; 方框链默认走根位置(坐标源 0), 一个字节都不碰它。
// ============================================================================

struct 网格变换信息 {
    bool 有效 = false;
    UE4类型::FTransform 变换;
    UE4类型::FMatrix    矩阵;
    float 原点离角色 = 0.0f;
    string 说明 = "无";
};

// 取网格的"局部→世界"变换 —— 唯一通道: 网格的 ComponentToWorld
网格变换信息 取网格变换(uint64_t Mesh, UE4类型::FVector 角色位置) {
    网格变换信息 结果;
    if (!kfd.地址判断(Mesh)) { 结果.说明 = "网格地址无效"; return 结果; };

    UE4类型::FTransform 变换 = 取Transform(Mesh + SDK::USceneComponent::ComponentToWorld);
    if (!变换可用(变换)) { 结果.说明 = "变换读不出"; return 结果; };

    结果.变换 = 变换;
    结果.矩阵 = 取Transform矩阵(变换);
    结果.有效 = true;
    结果.说明 = "网格CTW";

    // 合理性: 网格原点得贴着角色根位置(网格就在胶囊中心附近, 不该离几百厘米)
    if (是数(角色位置) && 长度(角色位置) > 1.0f) {
        结果.原点离角色 = 长度(向量减(变换.Translation, 角色位置));
        if (结果.原点离角色 > 网格原点容差) { 结果.有效 = false; 结果.说明 = "原点跑飞"; };
    };
    return 结果;
};

// 一条骨骼的组件空间坐标是否像样: 离网格原点 1cm ~ 250cm
inline bool 骨骼坐标合理(UE4类型::FVector 局部) {
    if (!是数(局部)) return false;
    float 平方 = 局部.X * 局部.X + 局部.Y * 局部.Y + 局部.Z * 局部.Z;
    return 平方 > 1.0f && 平方 < 62500.0f;
};

// ============================================================================
//  五、骨骼世界坐标
// ============================================================================

// 老接口(三角洲模块在用): 组件空间骨骼 × ComponentToWorld
//   这里就是修掉错位的那处: Y 行用 ZPlane.Y, Z 行用 ZPlane.Z
UE4类型::FVector 取骨骼世界坐标(uint64_t ComponentToWorld, uint64_t StaticMesh, int 骨骼点) {
    UE4类型::FTransform Transform = 取Transform(ComponentToWorld);
    UE4类型::FMatrix Transform矩阵 = 取Transform矩阵(Transform);
    UE4类型::FVector Translation = kfd.取vreadbuf<UE4类型::FVector>(StaticMesh + SDK::FTransform::translation + 骨骼点 * 0x30);
    return {
        Translation.X * Transform矩阵.XPlane.X + Translation.Y * Transform矩阵.YPlane.X + Translation.Z * Transform矩阵.ZPlane.X + Transform矩阵.WPlane.X,
        Translation.X * Transform矩阵.XPlane.Y + Translation.Y * Transform矩阵.YPlane.Y + Translation.Z * Transform矩阵.ZPlane.Y + Transform矩阵.WPlane.Y,
        Translation.X * Transform矩阵.XPlane.Z + Translation.Y * Transform矩阵.YPlane.Z + Translation.Z * Transform矩阵.ZPlane.Z + Transform矩阵.WPlane.Z,
    };
};

// 骨骼世界坐标 —— 用已经判定好的网格矩阵 (推荐走这条)
UE4类型::FVector 取骨骼世界坐标_用矩阵(uint64_t 骨骼数组, int 骨骼点, UE4类型::FMatrix 网格矩阵) {
    if (!kfd.地址判断(骨骼数组) || 骨骼点 < 0) return {0.0f, 0.0f, 0.0f};
    UE4类型::FVector 局部 = kfd.取vreadbuf<UE4类型::FVector>(骨骼数组 + SDK::FTransform::translation + 骨骼点 * 0x30);
    if (!骨骼坐标合理(局部)) return {0.0f, 0.0f, 0.0f};
    return 变换点(网格矩阵, 局部);
};

// 读一条骨骼的组件空间坐标
UE4类型::FVector 取骨骼局部坐标(uint64_t 骨骼数组, int 骨骼点) {
    if (!kfd.地址判断(骨骼数组) || 骨骼点 < 0) return {0.0f, 0.0f, 0.0f};
    return kfd.取vreadbuf<UE4类型::FVector>(骨骼数组 + SDK::FTransform::translation + 骨骼点 * 0x30);
};

// ============================================================================
//  六、角度(自瞄) / FString
// ============================================================================
UE4类型::FRotator 取朝向角度(UE4类型::FVector 本地, UE4类型::FVector 目标) {
    UE4类型::FVector 差 = 向量减(目标, 本地);
    float 水平距离 = sqrtf(差.X * 差.X + 差.Y * 差.Y);
    const float 弧度 = 180.0f / 3.14159265358979323846f;

    UE4类型::FRotator 结果;
    结果.Pitch = atan2f(差.Z, 水平距离) * 弧度;
    结果.Yaw   = atan2f(差.Y, 差.X) * 弧度;
    结果.Roll  = 0.0f;
    return 结果;
};

float 归一角度(float 角度) {
    while (角度 > 180.0f) 角度 -= 360.0f;
    while (角度 < -180.0f) 角度 += 360.0f;
    return 角度;
};

float 限制(float 值, float 下限, float 上限) {
    if (值 < 下限) return 下限;
    if (值 > 上限) return 上限;
    return 值;
};

string 取FString(uint64_t FString) {
    unsigned short UTF_16[28] = {0};
    char UTF_8[448] = {0};
    kfd.取vreadbuf列表(FString, UTF_16, sizeof(UTF_16));
    unsigned short* UTF_16缓冲区 = UTF_16;
    char* UTF_8缓冲区 = UTF_8;
    char *UTF_8缓冲区末尾 = UTF_8 + sizeof(UTF_8);
    while (UTF_16缓冲区 < UTF_16 + 28) {
        unsigned short 转码 = *UTF_16缓冲区;
        if (转码 <= 0x007F && UTF_8缓冲区 + 1 < UTF_8缓冲区末尾) {
            *UTF_8缓冲区++ = (char)转码;
        } else if (转码 >= 0x0080 && 转码 <= 0x07FF && UTF_8缓冲区 + 2 < UTF_8缓冲区末尾) {
            *UTF_8缓冲区++ = (char)((转码 >> 6) | 0xC0);
            *UTF_8缓冲区++ = (char)((转码 & 0x3F) | 0x80);
        } else if (转码 >= 0x0800 && 转码 <= 0xFFFF && UTF_8缓冲区 + 3 < UTF_8缓冲区末尾) {
            *UTF_8缓冲区++ = (char)((转码 >> 12) | 0xE0);
            *UTF_8缓冲区++ = (char)(((转码 >> 6) & 0x3F) | 0x80);
            *UTF_8缓冲区++ = (char)((转码 & 0x3F) | 0x80);
        } else {
            break;
        };
        UTF_16缓冲区++;
    };
    return string(UTF_8);
};

};
