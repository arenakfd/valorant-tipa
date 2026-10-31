// ============================================================================
//  暗区突围 国际服 数据结构
// ============================================================================

#pragma once

#import <pthread.h>
#include <string>
#include <ctime>
#include <unistd.h>

using namespace std;

// 单调时钟(秒): 读取线程节流 / 骨架解析重试用
inline double 现在秒() {
    struct timespec 时;
    clock_gettime(CLOCK_MONOTONIC, &时);
    return (double)时.tv_sec + (double)时.tv_nsec / 1.0e9;
};

// ============================================================================
//  无后坐力 —— 子线程负责"找到组件", 绘制线程负责"每帧按住"
//  组件是 Transient 的, 换枪/换局都会换对象, 所以子线程每轮重新校验一次。
// ============================================================================
inline uint64_t g_后坐力组件 = 0;          // USGWeaponRecoilComponent*
inline uint64_t g_后坐力武器 = 0;          // 它挂在哪把武器上(用来判断要不要重找)
inline string   g_后坐力诊断 = "未定位";    // 屏幕上那一行
inline bool     g_内存可写   = false;       // 内核库带不带写符号
inline int      g_后坐力写入 = 0;           // 累计写入成功次数(诊断用)
inline int      g_死源有效数 = 0;           // 本轮找到权威死亡组件的角色数(诊断用)
inline int      g_死源无效数 = 0;           // 本轮没找到的(诊断用)

namespace 暗区突围 {

    // 菜单/自身数据用
    struct FBigMapWorldRangeInfo {
        float X;
        float Y;
        float W;
        float H;
        int MapX;
        int MapY;
    };

};

struct 类名数据 {
    uint64_t RootComponent;
    string FName;
};

struct 物品数据 {
    uint64_t Actor;
    uint64_t RootComponent;
    uint64_t ItemID;        // 暗区是 FName 型物品ID(走名字表), 这里存原始值
    int Quality;
    int MallRecyclePrice;
};

struct 盒子数据 {
    uint64_t RootComponent;
    uint64_t PlayerName_Buffer;
    bool bLooted;
};

struct 玩家数据 {
    uint64_t Actor;
    string FName;
    int 队号 = 0;                // ★ 无畏: 本目标 CampID(角色+0x6A0) —— 队友主判据
    uint64_t RootComponent;
    uint64_t Mesh;
    uint64_t PlayerState;
    bool bIsAI;
    uint64_t HealthSet;         // ★ 无畏: 存 ASC(USGASComponent*)，绘制帧再走 +0x20F8 拿 AttributeBasic
    uint64_t 死亡组件 = 0;       // ★ 无畏无死亡组件，保留字段占位(恒0)，死亡判定走血量

    // ---- r27: 两条"防漏人"的取证标记(全部只用来决定"能不能判死/能不能丢") ----
    //   有身份 = 有 PlayerState 或 Controller —— 被 possess 过的角色就是活人(人机也有
    //            AIController), 这种角色在任何情况下都不许被在场过滤丢掉。
    //   血量集可信 = HealthSet 是**按类名**命中的 USGActorHealthAttributeSet, 不是
    //            退出到"第 0 个属性集"的兜底。兜底集里的 +0x44/+0x48 根本不是血,
    //            拿它判死会把活人判成死的(漏人)。
    bool 有身份 = false;
    bool 血量集可信 = false;

    // ---- r38/r40: 队伍/自身标记 ----
    //  这两条在**收集线程里**就判死, 判完直接决定"进不进名单"。
    //  为什么放收集侧而不是绘制帧: 绘制帧只吃名单, 名单剔掉 = 绘制和计数**同时**正确,
    //  不会出现"框不画了但顶上人数还数着"这种两套口径打架的情况。
    //  ★ 无畏: 判据换成角色身上的 CampID(0x6A0) 数值相等 —— 自己 CampID vs 目标 CampID。
    bool 是队友 = false;
    bool 是自己 = false;

    // 在场过滤本轮把这条标成"可疑"但先留着 —— 全灭保险用(见收集循环尾部)
    bool 在场可疑 = false;

    // ---- r29: 名单排序档 ----
    //  绘制帧每帧只吃前 候选名单上限 个名额, 名单顺序原来就是 Actor 表创建顺序,
    //  于是"后刷出来的活人"被前面的尸体/池化空壳顶出名单 = 漏人。
    //  0 = 活着 且 有身份(PlayerState/Controller)   ← 最该画的
    //  1 = 活着 但 没身份(可能是还没 possess 的池化角色)
    //  2 = 死亡组件明确说死(画不画仍由绘制帧自己判, 这里只把它沉到名单尾部)
    int 排序档 = 1;
    UE4类型::TArray 骨骼数组;    // CachedComponentSpaceTransforms

    uint64_t gid = 0;           // 角色唯一号(遍历去重)
    float 距离 = 0.0f;           // 世界距离(米)

    // ---- 修复版13: 三个"目标世界坐标"来源, 同帧一起取, 互相印证 ----
    //   甲 = RootComponent + RelativeLocation(0x154)      相对父组件的位置缓存
    //   乙 = RootComponent + ComponentToWorld(0x120+0x10) 引擎每帧刷新的绝对世界变换
    //   丙 = Mesh          + ComponentToWorld(0x120+0x10) 网格的世界位置(渲染原点)
    //   三个都在读取线程里取(5秒级), 差值直接上屏 —— 哪个是真的, 一眼可判。
    UE4类型::FVector 源甲;      // 相对位置
    UE4类型::FVector 源乙;      // 根世界变换
    UE4类型::FVector 源丙;      // 网格世界变换
    bool 甲有效 = false, 乙有效 = false, 丙有效 = false;
};

struct {
    uint64_t PlayerCameraManager;
    uint64_t LocalPawn;
    // ---- 无畏: 自身身份 ----
    //   LocalPlayerState: 排除"自己"用(比只比 Pawn 指针硬 —— 换局/观战/复活那一瞬
    //                     Pawn 会换, 只比 Actor 会把自己漏进来, 顶上真人数 +1)
    //   Local队号       : 自己角色 CampID(0x6A0) —— 队友判据的基准值
    //   ⚠️ 这两个**绝不能跨局留**: 换局后旧的 PS 是尸体指针、旧的队号是本局残留,
    //      拿它比对就是"漏人"的写法。所以在换局/清空全部时一并归零(见 清空全部)。
    uint64_t LocalPlayerState = 0;
    int      Local队号 = 0;
    pthread_mutex_t 锁 = PTHREAD_MUTEX_INITIALIZER;

    void 设置相机(uint64_t 相机) {
        pthread_mutex_lock(&锁);
        PlayerCameraManager = 相机;
        pthread_mutex_unlock(&锁);
    }

    void 设置自己(uint64_t Pawn) {
        pthread_mutex_lock(&锁);
        LocalPawn = Pawn;
        pthread_mutex_unlock(&锁);
    }

    uint64_t 获取相机() {
        pthread_mutex_lock(&锁);
        uint64_t 结果 = PlayerCameraManager;
        pthread_mutex_unlock(&锁);
        return 结果;
    }

    uint64_t 获取自己() {
        pthread_mutex_lock(&锁);
        uint64_t 结果 = LocalPawn;
        pthread_mutex_unlock(&锁);
        return 结果;
    }

    // ---- r38/r40: 自身身份 ----
    void 设置自身身份(uint64_t PS, int 队号) {
        pthread_mutex_lock(&锁);
        LocalPlayerState = PS;
        Local队号 = 队号;
        pthread_mutex_unlock(&锁);
    }

    uint64_t 获取自身PS() {
        pthread_mutex_lock(&锁);
        uint64_t 结果 = LocalPlayerState;
        pthread_mutex_unlock(&锁);
        return 结果;
    }

    int 获取自身队号() {
        pthread_mutex_lock(&锁);
        int 结果 = Local队号;
        pthread_mutex_unlock(&锁);
        return 结果;
    }

    // ★ 清空分成两级 —— 这是本版最关键的一处结构改动。
    //   上一版无论哪一步读失败都调 清空(), 把相机地址也一起抹了。
    //   而绘制线程第一件事就是取相机, 相机一没就整帧直接 return,
    //   于是"某一轮读取链抖了一下" = 屏幕上计数 0/0、一个框都没有(全灭)。
    //   现在: 读取链中途失败只清角色列表, 相机/自己一旦拿到就留着 ——
    //   相机地址是稳定量, 没理由因为 Actor 数组读失败就把它丢掉。
    void 清空角色() {
        pthread_mutex_lock(&锁);
        角色代次++;          // 让绘制线程知道"这批角色数据是本轮的"
        pthread_mutex_unlock(&锁);
    }

    void 清空全部() {
        pthread_mutex_lock(&锁);
        PlayerCameraManager = 0;
        LocalPawn = 0;
        // ★ r38/r40: 换局/全灭时自身身份一起归零 —— 旧的 PS 是尸体指针、
        //   旧的队号是本局残留, 留着做比对 = 真会漏人。
        LocalPlayerState = 0;
        Local队号 = 0;
        pthread_mutex_unlock(&锁);
    }

    // 兼容旧调用名(旧代码里都是 清空()) —— 一律按"只清角色"处理, 绝不动相机
    void 清空() { 清空角色(); }

    int 角色代次 = 0;
} 自身数据;
