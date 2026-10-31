// ============================================================================
//  暗区突围 国际服 —— 绘制线程 (本版: 治"偏移/没骨骼/没后坐/死人还在")
//
//  只保留五个功能:
//    · 方框透视      (方框密钥)   真人红 / 人机绿
//    · 骨骼透视      (骨骼密钥)
//    · 人机显示      (人机密钥, 关掉 = 只画真人)
//    · 显示距离      (距离密钥)
//    · 直播隐藏      (过直播密钥, 在 HUD控制器 里挂过直播窗口)
//  外加: 无后坐力 (后坐密钥)
//
//  ── r26 本版重点: 防检测(不改任何显示行为, 上去看跟 r25 一模一样) ─────────
//   这版专门处理"别被一眼抓"的三件事:
//   ① 后坐力: 删掉对 bEnableRecoil_Pitch / bEnableRecoil_Yaw / RepRecoilInfo 的写入。
//      这三个是**上行同步字段**, 清零 = 主动上报"我没有后坐力", 服务器拿同武器
//      同段位的基准一减就是恒定最大差值。现在只关相机总闸, 上报数据全是真值。
//   ② 掉帧上报: 客户端自己统计 FrameRateJitterCount 并主动上报给服务器 ——
//      外部读内存把帧时间拖毛它就涨, 等于自己举报自己。现在每 2 秒清一次,
//      写后读回校验(见 五之二、检测面维护)。
//   ③ 从源头降帧开销: 绘制帧里"三格纯诊断的相机读数"从每帧 4 次降到每 8 帧 1 次
//      (每格都要建相机 + 遍历全部目标算屏内命中, 是绘制帧里最贵的一段)。
//   刻意**不碰**的: 上报总闸 IsOpenReport / 自瞄统计 / 武器校验失败数组 /
//   反外挂流水数组 —— 理由逐条写在 五之二 段头, 简言之"在干净数据上别留指纹"。
//
//  ── r27 本版重点: 堵"漏人"(显示行为只放宽, 不收紧) ───────────────────
//  上机反馈 r26 之后**还是会漏掉人**。逐条回查, 有六处会让活人消失, 全部堵掉:
//    ① Mesh 读不到就跳过整个人(收集阶段 + 绘制帧各一处) —— 方框只靠根组件+位置,
//       网格那一瞬读不到不该让人消失。已删。
//    ② 在场判定第三条证据用的是 RootComponent 的 bVisible —— 位号没错, 但胶囊根本组件
//       常态就为 0, 这一票对每个角色恒真, "≥2 票"实际等于"再中一条就丢"。已删该票。
//    ③ 身份门只豁免有 PlayerState 的 —— 人机没有 PlayerState, 只有 AIController。
//       现在"有 PlayerState **或** Controller 任一"即硬豁免, 永不丢。
//    ④ bActorIsBeingDestroyed 单帧命中就丢 → 改连续 2 轮确认。
//    ⑤ 隐藏判定从"连续 6 轮"放宽到"连续 8 轮"。
//    ⑥ 新增**全灭保险**: 一轮里判定要把全部候选都丢掉 → 全部放行(判据失准只会多画, 不会全黑)。
//  另外两处也是漏人来源:
//    · 死亡判定删掉"主源说活 + 血量说死 = 判死"那条补刀路径; 血量兜底的那个"第 0 个
//      属性集"只准说活, 不准判死(它不是血量, 碰巧读到 0 就把活人判死)。
//    · 队友过滤默认**关**(TeamIndex 0x648 是 dump 有、实机没验过的字段, 拿它决定
//      "谁不画"就是漏人)。要开: plist "队友过滤密钥" = true。
//  还有一条小的: 贴脸目标(框比屏幕还高)原来直接不画, 现在改成夹取后照画。
//
//  本版重点(修复版12): 两处 —— 计数口径 + 相机时基
//
//  ① 计数与视角解耦 ★ (这是"顶上人数跟我视角涨落"的正主)
//     旧写法把 真人/人机 +1 放在第二个循环里, 而那个循环的前置条件是
//     "这一帧真画出了框" —— 框要靠投影, 投影在目标跑到我背后时必然失败。
//     于是: 我一转身 → 背后的人不计数 → 顶上数字跟着视角涨落。
//     本版把 +1 挪到取数阶段: 只要"角色有效 + 位置是个数 + 距离 400m 内 + 没死"
//     就计数,**跟朝向、屏内屏外、投影成不成功一点关系都没有**。
//     人机开关从此只管画不画, 不再影响计数。顶部改写成 "附近 真人 X 人机 Y"。
//
//  ② 相机时基: 来源定死一格, 不再帧间跳
//     相机管理器上并排放着三格视角(dump 实锤): CameraCache(0x420) /
//     CameraCachePrivate(0x1F70) / LastFrameCameraCache(0xAE0)。
//     旧写法"谁时间戳新用谁" —— 于是相机来源会在两格之间跳, 两格差的就是一帧的
//     转动量 → 转视角时框整体朝反方向偏、停手又贴回来。
//     本版: 来源由 相机源密钥 指定(0自动 1当前 2私有 3上一帧), 定了就只用那一格;
//     并用 相机 FOV基准密钥 强制焦距按宽/按高算(治"越靠边越偏")。
//     三格时间戳 + 主私两格差角全部打在左下第三行上: 静止时≈0, 一转身才有值,
//     那个值就是"框会偏多少"的直读数。
//
//  方框/骨骼仍是两条互不相干的单链(修复版11 的成果, 不动):
//     方框 = 角色根位置 ± 身高/2 两点投影; 骨骼 = 网格 ComponentToWorld × 骨骼数组。
//  死亡判定、真人/人机判据、后坐力、直播隐藏一行没碰。
//
//  更早的四处改动, 每一处都对着 CppSDK dump 核过:
//
//  ① 投影: FOV 有模式(EECameraFOVMode)。以前一律按"水平视场"算焦距,
//     模式是垂直时焦距整整差一个屏幕宽高比(手机 2 倍多) —— 表现就是
//     "离屏幕中心越远偏得越多、一摆动视角框就跟着飘"。现在按模式取边,
//     锁宽高比的黑边也按 UE4 的规矩算进渲染区, 偏心投影也算。
//
//  ② 骨骼: 不再赌写死的骨头下标。从 Mesh → SkeletalMesh → Skeleton → BoneTree
//     把骨头名读出来, 按名字定位 16 个关节; 名字读不出来就退回两套兜底表,
//     最后**用几何校验挑**: 哪套方案算出来真像个人(头高于骨盆、身高 1~2.3m、
//     所有点都在角色 3m 内)就用哪套。矩阵也两条候选(0x1A0 组件变换 / 0x890 求逆)
//     同样用几何校验挑。选错的直接不画 —— 宁可不画, 不给你画一根棍。
//
//  ③ 无后坐力: 用游戏自己在相机管理器上的总闸
//     ASGPlayerCameraManager::bEnableModifyRotationByWeaponRecoil (0x2E59),
//     后坐力照样算、动画照样演, 但不再推准星。可逆: 开关一关就把 true 写回去。
//     ★ r26 修正: 原来"第二层"那套(bEnableRecoil_Pitch/Yaw 写 0 +
//       RepRecoilInfo 清零)动的是**上行同步字段**, 等于主动上报"我后坐力是零" ——
//       已整段删除, 换成可选的压枪系数。见 五、无后坐力 段头。
//
//  ④ 死亡过滤: 判定挪到绘制帧里做(读取线程只有几十毫秒一次), 血量和倒地血量
//     每帧现场读 —— 人一死当帧就消失, 不会留 5 秒。
//     (修复版7 起改成三源独立取证, 主源是游戏自己的 bIsDead)
// ============================================================================

#pragma once

#import "SDK.h"
#import "绘图.h"
#import "子线程.h"      // 名字池 / 组件查找这些工具在读取线程里, 显式依赖, 别靠包含顺序
#import "绘制算法.h"

#include <map>
#include <vector>
#include <string>
#include <cstring>      // r26: 检测面维护里按两种类型解释同一块内存用

// ============================================================================
//  一、关节槽位 + 候选骨头名
// ============================================================================
enum 关节槽 {
    槽头 = 0, 槽颈, 槽脊椎, 槽骨盆,
    槽左肩, 槽左肘, 槽左手,
    槽右肩, 槽右肘, 槽右手,
    槽左髋, 槽左膝, 槽左脚,
    槽右髋, 槽右膝, 槽右脚,
    槽位总数 = 16
};

// 归一化(全小写 + 只留字母数字)之后逐个比: 先精确匹配, 精确全落空再来一轮"包含"
static const char* 关节候选名[槽位总数][11] = {
    { "head",       "bip01head",   "head1",       "head01",    "jhead",       nullptr },
    { "neck",       "neck1",       "neck01",      "bip01neck", "jneck",       nullptr },
    { "spine2",     "spine02",     "spine3",      "spine03",   "upperchest",
      "chest",      "spine1",      "spine01",     "spine",     "jspine" },
    { "pelvis",     "hips",        "bip01pelvis", "hip",       "jpelvis",     nullptr },
    { "claviclel",  "shoulderl",   "leftshoulder","upperarml", "bip01lclavicle",
      "lclavicle",  "lshoulder",   "larm",        nullptr },
    { "lowerarml",  "forearml",    "leftforearm", "elbowl",    "bip01lforearm",
      "llowerarm",  "lforearm",    "lelbow",      nullptr },
    { "handl",      "lefthand",    "bip01lhand",  "handl1",    "lhand",       nullptr },
    { "clavicler",  "shoulderr",   "rightshoulder","upperarmr","bip01rclavicle",
      "rclavicle",  "rshoulder",   "rarm",        nullptr },
    { "lowerarmr",  "forearmr",    "rightforearm","elbowr",    "bip01rforearm",
      "rlowerarm",  "rforearm",    "relbow",      nullptr },
    { "handr",      "righthand",   "bip01rhand",  "handr1",    "rhand",       nullptr },
    { "thighl",     "upperlegl",   "leftupleg",   "lthigh",    "bip01lthigh",
      "lthigh",     "leftleg",     "lupleg",      nullptr },
    { "calfl",      "lowerlegl",   "leftleg",     "kneel",     "bip01lcalf",
      "lcalf",      "llowerleg",   "lknee",       nullptr },
    { "footl",      "leftfoot",    "bip01lfoot",  "toebasel",  "lfoot",       nullptr },
    { "thighr",     "upperlegr",   "rightupleg",  "rthigh",    "bip01rthigh",
      "rthigh",     "rightleg",    "rupleg",      nullptr },
    { "calfr",      "lowerlegr",   "rightleg",    "kneer",     "bip01rcalf",
      "rcalf",      "rlowerleg",   "rknee",       nullptr },
    { "footr",      "rightfoot",   "bip01rfoot",  "toebaser",  "rfoot",       nullptr },
};

// 兜底下标表(名字解析不出来时用, 还要过几何校验才算数)
struct 兜底骨架索引 { const char* 名称; int 序号[16]; };
static const 兜底骨架索引 兜底表[2] = {
    { "UE4标准", { 6, 5, 4, 1,  7, 9, 10,  11, 13, 14,  15, 16, 17,  19, 20, 21 } },
    { "Mixamo",  { 5, 4, 3, 0,  7, 9, 10,  11, 13, 14,  15, 16, 17,  19, 20, 21 } },
};

// 颜色
static const ImU32 真人色 = IM_COL32(255, 82, 82, 255);
static const ImU32 人机色 = IM_COL32(102, 255, 102, 255);
static const ImU32 距离色 = IM_COL32(255, 255, 255, 255);
// ★ r35: 玩家名字色 —— 用金黄, 跟框色(h红/AI绿)分开, 一眼能认出那是名字不是判定结果
static const ImU32 名字色 = IM_COL32(255, 214, 102, 255);

// 人体尺寸(位置框兜底用, cm)
constexpr float 默认身高   = 176.0f;
constexpr float 默认框宽比 = 0.42f;

// ============================================================================
//  二、骨架解析(带缓存 + 几何校验)
// ============================================================================
struct 骨架方案 {
    int         关节[槽位总数];
    vector<int> 父;
    int         骨头数   = 0;
    int         名字命中 = 0;    // 靠名字命中了几个关节
    int         来源     = 0;    // 1 = 按名字  2 = 兜底表0  3 = 兜底表1
    double      上次尝试 = 0.0;  // 失败后隔一会儿再试, 但绝不永久放弃
    bool        有效     = false;
};

map<uint64_t, 骨架方案> g_骨架方案表;   // key = 骨架资产(USkeleton*)
map<uint64_t, uint64_t> g_网格骨架表;   // Mesh -> USkeleton*
map<uint64_t, uint64_t> g_网格骨骼数组; // Mesh -> 组件空间骨骼数组地址(二选一, 由校验挑)
map<uint64_t, int>      g_网格兜底表;   // Mesh -> 上次几何校验选中的兜底表号

string g_诊断行 = "";
string g_诊断行2 = "";      // 第二行: 绘制层细节(第一行放链路断点)
string g_诊断行3 = "";      // 第三行: 相机时基(来源/三格时间戳/主私差角)
string g_帧状态 = "正常";    // 顶部计数旁边显示 —— 0/0 时要能看出为什么是 0/0
string g_骨架诊断 = "未解析";
string g_骨源诊断 = "未选路";     // r22: 骨骼数组/变换这一帧最终挑了哪条(诊断用)
int    g_名字命中诊断 = -1;

double 现在秒();   // 定义在 游戏类型.h

// 骨头名归一: 全小写, 只留字母数字 (Head_01 / mixamorig:Head → head01 / mixamorighead)
string 骨头名归一(const char* 原) {
    string 结果;
    if (原 == nullptr) return 结果;
    for (const char* 指 = 原; *指 != 0; 指++) {
        char c = *指;
        if (c >= 'A' && c <= 'Z') 结果.push_back((char)(c - 'A' + 'a'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) 结果.push_back(c);
    };
    return 结果;
};

// Mesh → USkeleton (缓存)
uint64_t 取网格骨架(uint64_t Mesh) {
    if (!kfd.地址判断(Mesh)) return 0;
    auto 缓存 = g_网格骨架表.find(Mesh);
    if (缓存 != g_网格骨架表.end()) return 缓存->second;

    uint64_t 骨架 = 0;
    uint64_t 网格资产 = kfd.取vreadbuf<uint64_t>(Mesh + SDK::USkinnedMeshComponent::SkeletalMesh);
    if (kfd.地址判断(网格资产)) {
        骨架 = kfd.取vreadbuf<uint64_t>(网格资产 + SDK::USkeletalMesh::Skeleton);
    };
    if (!kfd.地址判断(骨架)) 骨架 = 0;
    if (g_网格骨架表.size() > 6000) g_网格骨架表.clear();
    g_网格骨架表[Mesh] = 骨架;
    return 骨架;
};

// 名字匹配: 精确 / 包含
bool 名字精确(const string& 骨头名, const char* 候选) {
    return !骨头名.empty() && 候选 != nullptr && 骨头名 == 候选;
};
bool 名字包含(const string& 骨头名, const char* 候选) {
    if (骨头名.empty() || 候选 == nullptr) return false;
    if (strlen(候选) < 4) return false;      // 太短的候选名不做包含, 免得乱咬
    return 骨头名.find(候选) != string::npos;
};

// ============================================================================
//  修复版14 —— 参考骨架: 骨名与父表的权威来源
//    为什么不用旧路、dump 依据 → 见 SDK.h「修复版14」那段。
//    这里只干一件事: 在网格资产(再到骨架资产)的内存窗口里, 扫出
//    TArray<FMeshBoneInfo> { FName 名; int32 父索引 } 那一条表, 并校验:
//      · 名字能解析成真实字符串、且至少 3/4 像骨头名(是字母数字串)
//      · 父索引整棵合法(每个父 < 自己的下标, 根为 -1)  ← 这条几乎不能伪造
//      · 后面紧跟一个同数量的 TArray(参考姿势 RawRefBonePose)  → 强证据加分
//    扫到就永久缓存, 扫不到最多试 3 次(别每次重试都全内存扫)。
// ============================================================================
struct 参考骨架 {
    bool     有效 = false;
    int      已试 = 0;
    int      分 = 0;
    uint64_t 对象 = 0;
    uint64_t 偏移 = 0;
    int      骨头数 = 0;
    int      名好 = 0;
    int      父好 = 0;
    bool     有参考姿势 = false;
    uint32_t 步进 = 0;
    vector<string> 名表;
    vector<int>    父表;
    string   样例;
};
map<uint64_t, 参考骨架> g_参考骨架表;

static string 十六进制短(uint64_t 值) {
    char 缓冲[32];
    snprintf(缓冲, sizeof(缓冲), "%llX", (unsigned long long)值);
    return string(缓冲);
};

// 扫一个对象的一段内存, 挑出最像"参考骨架骨表"的那条
void 扫参考骨架窗口(uint64_t 对象, uint64_t 起, uint64_t 止, int 骨头数提示, 参考骨架& 最好) {
    if (!kfd.地址判断(对象) || 止 <= 起 || (止 - 起) > 0x1000) return;
    vector<uint8_t> 块((size_t)(止 - 起), 0);
    if (!kfd.取vreadbuf列表(对象 + 起, 块.data(), 块.size())) return;

    for (size_t 偏 = 0; 偏 + 0x20 <= 块.size(); 偏 += 8) {
        uint64_t 表 = *(const uint64_t*)(块.data() + 偏);
        int 数 = *(const int*)(块.data() + 偏 + 8);
        int 上限 = *(const int*)(块.data() + 偏 + 12);
        if (!kfd.地址判断(表)) continue;
        if (数 < 8 || 数 > 1024) continue;
        if (上限 < 数 || 上限 > 4096) continue;

        // 紧跟其后的那个 TArray 若数量一致, 就是参考姿势 → 强证据
        bool 有姿 = false;
        if (偏 + 0x20 <= 块.size()) {
            uint64_t 表2 = *(const uint64_t*)(块.data() + 偏 + 16);
            int 数2 = *(const int*)(块.data() + 偏 + 24);
            if (kfd.地址判断(表2) && 数2 == 数) 有姿 = true;
        };

        const uint32_t 步进组[2] = { SDK::FMeshBoneInfo::步进甲, SDK::FMeshBoneInfo::步进乙 };
        for (int s = 0; s < 2; s++) {
            uint32_t 步进 = 步进组[s];
            size_t 需 = (size_t)数 * 步进;
            if (需 < 64 || 需 > 64 * 1024) continue;
            vector<uint8_t> 元(需, 0);
            if (!kfd.取vreadbuf列表(表, 元.data(), 元.size())) continue;

            vector<string> 名(数);
            vector<int> 父(数, -1);
            int 名好 = 0, 父好 = 0;
            string 样例;
            for (int i = 0; i < 数; i++) {
                const uint8_t* e = 元.data() + (size_t)i * 步进;
                int 名字索引 = *(const int*)e;
                int 父索引 = *(const int*)(e + SDK::FMeshBoneInfo::父索引);
                if (父索引 >= -1 && 父索引 < i) { 父好++; 父[i] = 父索引; };

                string 原 = 名字池取名字(g_名字池, 名字索引);
                if (!名字可读(原)) continue;
                名好++;
                名[i] = 骨头名归一(原.c_str());
                if (样例.size() < 60) 样例 += (样例.empty() ? "" : " ") + 名[i];
            };

            // 名字: 至少 8 个能解析成正常骨头名 + 过半(名字池可能只覆盖一部分)
            // 父表: 必须整棵合法(每个父 < 自己的下标) —— 随机数据不可能过这一条,
            //       所以名字阈值可以放宽, 漏判会丢骨骼, 但不会误判到错表。
            if (名好 < 8 || 名好 * 2 < 数) continue;
            if (父好 < 数) continue;

            int 分 = 名好 * 4 + 父好 + (有姿 ? 24 : 0) +
                     ((骨头数提示 > 0 && 数 == 骨头数提示) ? 8 : 0);
            if (分 <= 最好.分) continue;

            最好.有效 = true;
            最好.分 = 分;
            最好.对象 = 对象;
            最好.偏移 = 起 + 偏;
            最好.骨头数 = 数;
            最好.名好 = 名好;
            最好.父好 = 父好;
            最好.有参考姿势 = 有姿;
            最好.步进 = 步进;
            最好.名表 = 名;
            最好.父表 = 父;
            最好.样例 = 样例;
        };
    };
};

// 取(带缓存): 先在网格资产里扫, 扫不到再往骨架资产里兜一层
参考骨架 取参考骨架(uint64_t 网格资产, uint64_t 骨架, int 骨头数提示) {
    auto 缓 = g_参考骨架表.find(网格资产);
    if (缓 != g_参考骨架表.end() && (缓->second.有效 || 缓->second.已试 >= 3)) return 缓->second;

    参考骨架 最好;
    最好.已试 = (缓 != g_参考骨架表.end()) ? (缓->second.已试 + 1) : 1;
    扫参考骨架窗口(网格资产, SDK::USkeletalMesh::参考骨架扫描起,
                   SDK::USkeletalMesh::参考骨架扫描止, 骨头数提示, 最好);
    if (!最好.有效 && kfd.地址判断(骨架)) {
        扫参考骨架窗口(骨架, SDK::USkeleton::参考骨架扫描起,
                       SDK::USkeleton::参考骨架扫描止, 骨头数提示, 最好);
    };
    if (g_参考骨架表.size() > 6000) g_参考骨架表.clear();
    g_参考骨架表[网格资产] = 最好;
    return 最好;
};

// ============================================================================
//  解析一个骨架
//    读数: Mesh → SkeletalMesh → Skeleton → BoneTree(TArray<FBoneNode>)
//    返回: 骨架信息(哪怕名字没对上, 父表也留着给"骨骼空间累乘"那条兜底用)
//    名字够(>=12/16)才算"名字可用"; 不够就交给几何校验去选兜底表。
//    失败不永久放弃 —— 隔 3 秒重试一次(刚进局时资产可能还没加载完,
//    上一版就是"第一次读失败就永久记死", 于是骨骼再也没出来)。
// ============================================================================
骨架方案* 取骨架信息(uint64_t Mesh, bool& 名字可用) {
    名字可用 = false;
    uint64_t 骨架 = 取网格骨架(Mesh);
    if (!kfd.地址判断(骨架)) return nullptr;

    auto 找到 = g_骨架方案表.find(骨架);
    if (找到 != g_骨架方案表.end()) {
        if (找到->second.骨头数 > 0 &&
            (找到->second.名字命中 >= 12 || 现在秒() - 找到->second.上次尝试 < 3.0)) {
            名字可用 = (找到->second.名字命中 >= 12);
            return &找到->second;
        };
    };

    骨架方案 方案;
    方案.上次尝试 = 现在秒();
    for (int i = 0; i < 槽位总数; i++) 方案.关节[i] = -1;

    if (!kfd.地址判断(g_名字池)) return nullptr;

    // ---- 修复版14: 骨名/父表的权威来源 = 网格资产的"参考骨架" ----
    //   旧路(骨架资产 BoneTree)读不出名字是老代码遗留问题: dump 里 FBoneNode
    //   的 Name/ParentIndex 都标着 Deprecated, 运行时引擎不写值。详见 SDK.h。
    uint64_t 网格资产 = kfd.取vreadbuf<uint64_t>(Mesh + SDK::USkinnedMeshComponent::SkeletalMesh);
    if (!kfd.地址判断(网格资产)) 网格资产 = 0;

    参考骨架 参 = 取参考骨架(网格资产, 骨架, 0);
    int 骨头数 = 0;
    vector<string> 骨头名表;
    string 名来源;

    if (参.有效 && 参.骨头数 > 0 && 参.名好 >= 8) {
        骨头数 = 参.骨头数;
        方案.骨头数 = 骨头数;
        方案.父 = 参.父表;
        骨头名表 = 参.名表;
        名来源 = "参考骨架+" + 十六进制短(参.偏移) + " 名" + to_string(参.名好) + "/" +
                 to_string(骨头数) + (参.有参考姿势 ? " 带姿" : "");
    } else {
        // ---- 兜底: 原来的 BoneTree 那条路(实证读不出名字, 留着不删) ----
        名来源 = "旧骨头表";
        UE4类型::TArray 骨头表 = kfd.取vreadbuf<UE4类型::TArray>(骨架 + SDK::USkeleton::BoneTree);
        骨头数 = 骨头表.ArrayNum;
        // ★ 不再校验 ArrayNum <= ArrayMax —— 游戏扩容/压缩数组的瞬间 ArrayMax 是过期的,
        //   这一条会把正常骨骼表判成垃圾 → 骨架解析失败 → 一个骨骼都不画。
        if (!kfd.地址判断(骨头表.AllocatorInstance) || 骨头数 <= 0 || 骨头数 > 4096) {
            g_骨架诊断 = "骨架表读不出 (n=" + to_string(骨头数) + ") [参考骨架未命中]";
            g_骨架方案表[骨架] = 方案;
            return nullptr;
        };
        if (骨头数 > 512) 骨头数 = 512;

        方案.骨头数 = 骨头数;
        方案.父.assign(骨头数, -1);

        vector<uint8_t> 原始((size_t)骨头数 * SDK::FBoneNode::步进, 0);
        if (!kfd.取vreadbuf列表(骨头表.AllocatorInstance, 原始.data(), 原始.size())) {
            g_骨架诊断 = "骨头表读失败 [参考骨架未命中]";
            g_骨架方案表[骨架] = 方案;
            return nullptr;
        };

        骨头名表.assign(骨头数, string());
        for (int b = 0; b < 骨头数; b++) {
            const uint8_t* 元素 = 原始.data() + (size_t)b * SDK::FBoneNode::步进;
            int 父 = *(const int*)(元素 + SDK::FBoneNode::ParentIndex);
            if (父 >= -1 && 父 < 骨头数) 方案.父[b] = 父;
            int 名字索引 = *(const int*)(元素 + SDK::FBoneNode::Name);
            if (名字索引 <= 0) continue;
            骨头名表[b] = 骨头名归一(名字池取名字(g_名字池, 名字索引).c_str());
        };
    };

    // ---- 第一轮: 精确匹配 ----
    for (int 槽 = 0; 槽 < 槽位总数; 槽++) {
        for (int 骨 = 0; 骨 < 骨头数; 骨++) {
            if (骨头名表[骨].empty()) continue;
            bool 中 = false;
            for (int c = 0; c < 11 && 关节候选名[槽][c] != nullptr; c++) {
                if (名字精确(骨头名表[骨], 关节候选名[槽][c])) { 中 = true; break; };
            };
            if (!中) continue;
            方案.关节[槽] = 骨;
            break;
        };
    };
    int 命中 = 0;
    for (int 槽 = 0; 槽 < 槽位总数; 槽++) if (方案.关节[槽] >= 0) 命中++;

    // ---- 第二轮: 还没对上的, 用"包含"再来一遍 ----
    if (命中 < 槽位总数) {
        for (int 槽 = 0; 槽 < 槽位总数; 槽++) {
            if (方案.关节[槽] >= 0) continue;
            for (int 骨 = 0; 骨 < 骨头数; 骨++) {
                if (骨头名表[骨].empty()) continue;
                bool 中 = false;
                for (int c = 0; c < 11 && 关节候选名[槽][c] != nullptr; c++) {
                    if (名字包含(骨头名表[骨], 关节候选名[槽][c])) { 中 = true; break; };
                };
                if (!中) continue;
                方案.关节[槽] = 骨;
                break;
            };
        };
    };
    命中 = 0;
    for (int 槽 = 0; 槽 < 槽位总数; 槽++) if (方案.关节[槽] >= 0) 命中++;

    方案.名字命中 = 命中;
    方案.来源 = (命中 >= 12) ? 1 : 0;
    g_名字命中诊断 = 命中;

    if (命中 >= 12) {
        g_骨架诊断 = 名来源 + " 命中" + to_string(命中) + "/16";
    } else if (参.有效 && 参.名好 >= 8) {
        // 表扫到了、名字却没对上候选 —— 把前几个真名摆上屏, 一眼看出是命名不同还是扫错了表
        g_骨架诊断 = 名来源 + " 命中" + to_string(命中) + "/16 例:" + 参.样例;
    } else {
        g_骨架诊断 = 名来源 + " 名字只认 " + to_string(命中) + "/16, 试兜底表";
    };

    g_骨架方案表[骨架] = 方案;
    auto 再看 = g_骨架方案表.find(骨架);
    if (再看 == g_骨架方案表.end()) return nullptr;
    名字可用 = (再看->second.名字命中 >= 12);
    return &再看->second;
};

// ============================================================================
//  三、网格变换(局部→世界) + 骨骼点位
// ============================================================================
// ============================================================================
//  修复版13 —— 目标世界坐标: 三个来源, 可指定, 默认第一版口径
//
//  为什么要做成可指定:
//    甲 RelativeLocation(0x154) 是"相对父组件的位置缓存"。
//      RootComponent 没有父组件时, 它**碰巧**等于世界坐标 —— 第一版就是这么出的框。
//      但只要这个组件被挂到别的组件上(载具/挂点/特殊状态), 它就≠世界坐标, 框当场偏。
//    乙 ComponentToWorld(0x120) 是引擎**每帧刷新、专门给渲染用**的绝对世界变换,
//      不受挂载影响, 也不会有相对位置的缓存滞后。这才是"准确目标"。
//    丙 网格的 ComponentToWorld 是角色模型的世界原点(渲染真正落笔的地方)。
//
//  默认 0(甲) = 你实测有效的那版口径, 一个数不动。
//  想看哪个对, 切 1/2, 屏幕上的甲乙/甲丙差值是直读证据。
// ============================================================================
int g_坐标源 = 0;   // 0=根相对位置 1=根世界变换 2=网格世界变换

UE4类型::FVector 取角色位置(uint64_t RootComponent, uint64_t Mesh, int 来源) {
    if (来源 == 2) {
        if (!kfd.地址判断(Mesh)) return {0.0f, 0.0f, 0.0f};
        return kfd.取vreadbuf<UE4类型::FVector>(
            Mesh + SDK::USceneComponent::ComponentToWorld + SDK::FTransform::translation);
    };
    if (!kfd.地址判断(RootComponent)) return {0.0f, 0.0f, 0.0f};
    if (来源 == 1) {
        return kfd.取vreadbuf<UE4类型::FVector>(
            RootComponent + SDK::USceneComponent::ComponentToWorld + SDK::FTransform::translation);
    };
    return kfd.取vreadbuf<UE4类型::FVector>(RootComponent + SDK::USceneComponent::RelativeLocation);
};

// 老接口(其它调用点): 默认走第一版口径
UE4类型::FVector 取角色位置(uint64_t RootComponent) {
    return 取角色位置(RootComponent, 0, 0);
};

// 老接口(归并到绘制算法.h 的统一解析器):
//   以前这里自己写了一份"两条候选", 而且写成了**短路**——候选1一命中就 return,
//   候选2(真正的 ComponentToWorld)永远走不到, 等于把权威通道整个废掉。
//   现在统一走 UE4算法::取网格变换(两条独立通道都算 + 交叉互证), 不再各写一套各猜一套。
//   第二参数(来源)保留只为兼容调用点, 真正的来源由解析器内部判定并上报诊断。
UE4类型::FMatrix 取网格矩阵(uint64_t Mesh, int 来源, UE4类型::FVector 角色位置, bool& 可用) {
    (void)来源;
    UE4算法::网格变换信息 信息 = UE4算法::取网格变换(Mesh, 角色位置);
    可用 = 信息.有效;
    if (!信息.有效) return UE4类型::FMatrix();
    return 信息.矩阵;
};

// 读一根骨头(组件空间)的平移
bool 读骨头局部(uint64_t 骨骼数组, int 骨头, UE4类型::FVector& 出) {
    if (!kfd.地址判断(骨骼数组) || 骨头 < 0) return false;
    出 = kfd.取vreadbuf<UE4类型::FVector>(骨骼数组 + SDK::FTransform::translation + (uint64_t)骨头 * 0x30);
    if (!UE4算法::是数(出)) return false;
    float 平方 = 出.X * 出.X + 出.Y * 出.Y + 出.Z * 出.Z;
    return 平方 > 1.0f && 平方 < 4000000.0f;   // 离网格原点 1cm ~ 20m
};

// 骨骼空间累乘(兜底): 沿父链把旋转平移乘上去, 得到组件空间坐标
UE4类型::FTransform 累乘父链(uint64_t 骨骼空间数组, const vector<int>& 父, int 骨头, int 上限) {
    UE4类型::FTransform 结果 = kfd.取vreadbuf<UE4类型::FTransform>(骨骼空间数组 + (uint64_t)骨头 * 0x30);
    if (!UE4算法::变换可用(结果)) return 结果;

    int 当前 = 骨头;
    for (int 步 = 0; 步 < 上限 && 步 < 64; 步++) {
        int 父下标 = (当前 >= 0 && 当前 < (int)父.size()) ? 父[当前] : -1;
        if (父下标 < 0 || 父下标 >= (int)父.size() || 父下标 == 当前) break;

        UE4类型::FTransform 父变换 = kfd.取vreadbuf<UE4类型::FTransform>(骨骼空间数组 + (uint64_t)父下标 * 0x30);
        if (!UE4算法::变换可用(父变换)) break;

        UE4类型::FMatrix 父矩阵 = UE4算法::取Transform矩阵(父变换);
        UE4类型::FVector 平移 = UE4算法::变换点(父矩阵, 结果.Translation);
        结果.Translation = 平移;
        结果.Rotation.X += 父变换.Rotation.X;   // 近似: 人形骨架每级旋转都不大
        结果.Rotation.Y += 父变换.Rotation.Y;
        结果.Rotation.Z += 父变换.Rotation.Z;
        结果.Rotation.W += 父变换.Rotation.W;
        当前 = 父下标;
    };
    return 结果;
};

// 几何校验: 这些世界坐标像不像一个站着的人
bool 人形校验(UE4类型::FVector 世界[槽位总数], bool 有效[槽位总数], UE4类型::FVector 角色位置) {
    if (!有效[槽头] || !有效[槽骨盆]) return false;

    float 脚Z = 1.0e30f;
    bool 有脚 = false;
    for (int i = 槽左脚; i <= 槽右脚; i += 3) {
        if (!有效[i]) continue;
        if (世界[i].Z < 脚Z) 脚Z = 世界[i].Z;
        有脚 = true;
    };
    if (!有脚) return false;

    float 身高 = 世界[槽头].Z - 脚Z;
    if (!(身高 > 90.0f && 身高 < 260.0f)) return false;

    float 躯干 = 世界[槽头].Z - 世界[槽骨盆].Z;
    if (!(躯干 > 20.0f && 躯干 < 130.0f)) return false;
    if (!(世界[槽骨盆].Z > 脚Z + 20.0f)) return false;

    if (有效[槽左肩] && 有效[槽右肩]) {
        float 肩宽 = UE4算法::长度(UE4算法::向量减(世界[槽左肩], 世界[槽右肩]));
        if (!(肩宽 > 5.0f && 肩宽 < 130.0f)) return false;
    };

    // 所有点必须落在角色附近(3 米内) —— 这条能挡住"矩阵选错/骨头串位"
    if (UE4算法::是数(角色位置) && UE4算法::长度(角色位置) > 1.0f) {
        for (int i = 0; i < 槽位总数; i++) {
            if (!有效[i]) continue;
            if (UE4算法::长度(UE4算法::向量减(世界[i], 角色位置)) > 300.0f) return false;
        };
    };
    return true;
};

struct 目标点位 {
    ImVec2 点[槽位总数];
    bool  有效[槽位总数];
    UE4类型::FVector 世界[槽位总数];
    UE4类型::FVector 头世界;
    bool  头有效 = false;
    bool  用骨骼 = false;
    int   有效数 = 0;
    float 框顶 = 0.0f;
    float 框底 = 0.0f;
    float 框左 = 0.0f;
    float 框右 = 0.0f;
    float 中心X = 0.0f;
    float 框宽 = 0.0f;
    // ★ r37: 头甲/名字的落点 —— 骨骼最高点的屏幕 y(骨骼好就是真头顶),
    //   骨骼没出来时退回框顶, 保证这两个功能不会没地方画。
    float 头顶屏y = 0.0f;
    bool  头顶有效 = false;
};

// ============================================================================
//  ★ 修复版21 —— 骨骼 / 方框 / 头甲: 按参考实现的写法复现
//
//  从参考实现的二进制里逐条读出来的三条(不是猜的):
//    ① 骨骼: 不查骨架名字、不走父链 —— 直接写死 18 个**骨骼下标**,
//       按下标取关节; 连线段也是写死的 17 段。
//    ② 方框: 只投两个点 —— 角色中心 + 头顶(中心 + 半身高)。
//       h = 两点在屏幕上的纵向距离 → **框宽 = h、框高 = 2h**,
//       四条边只画两端(角框), 不是完整矩形。
//    ③ 头甲: 角色 → +0x1A78 → +0x260 → 取两个指针 → 各自 +0x730 的整数。
// ============================================================================

constexpr int 参考骨数 = 18;

// 18 个骨骼下标(顺序即取点顺序, 不要动)
static const int 参考骨骼序号[参考骨数] = {
    16, 15, 14, 13, 12, 1, 53, 54, 56, 23, 24, 26, 2, 4, 5, 7, 9, 10
};

// 17 段连线(下标指向上面那张表)
static const int 参考骨段[17][2] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 },
    { 1, 6 }, { 6, 7 }, { 7, 8 },
    { 1, 9 }, { 9, 10 }, { 10, 11 },
    { 5, 12 }, { 12, 13 }, { 13, 14 },
    { 5, 15 }, { 15, 16 }, { 16, 17 },
};

struct 参考骨骼点位 {
    ImVec2 点[参考骨数];
    bool  有效[参考骨数];
    UE4类型::FVector 世界[参考骨数];   // ★ r37: 世界坐标点 —— 方框/头甲/距离共用这一条链
    int   成数 = 0;
};

// ============================================================================
//  ★ 修复版22 —— 骨骼数据源: 照参考实现的取法重取
//
//  参考实现里那三条(反汇编逐条读出来的, 不是猜的):
//    数组: idx  = *(int32*)(Mesh + 0x744)              // 当前用哪一路缓冲
//          数组 = *(void**)(Mesh + 0x700 + idx*16)      // 组件空间变换数组族(每项 16 字节 = TArray)
//    变换: t    = *(FTransform*)*(void**)(Mesh + 0x200)  // ★ 不是内联的 ComponentToWorld
//    点  : 世界 = t.TransformPosition(数组[骨下标].Translation)
//
//  r21 取的是 0x9B8(CachedComponentSpaceTransforms) + 0x120(内联 ComponentToWorld) ——
//  跟参考实现是两套完全不同的数据源, 点对不上就是这里(一个不变换/一个错基)
//  本版把两族都列成候选, 用"骨架几何体检"(17 段连线有几段像人形)自动挑,
//  并把挑中的那条写进诊断行 —— 下一版就不用再猜。
// ============================================================================

// 值全部来自 SDK.h（那边有 dump/反汇编出处），这里只起别名，不再各存一份数字。
constexpr uint64_t 参考骨_数组族   = SDK::USkinnedMeshComponent补::骨骼数组族;    // = 0x700
constexpr uint64_t 参考骨_数组索引 = SDK::USkinnedMeshComponent补::骨骼数组索引;  // = 0x744
constexpr uint64_t 参考骨_变换指针 = SDK::USceneComponent补::世界变换指针;        // = 0x200

// ============================================================================
//  ★★★ r36 —— 骨骼: 1:1 复刻参考实现的取法 ★★★
//
//  那份成品内核的骨骼链只有三步(反汇编 0x1000917d8 ~ 0x100091d24 逐条读出来的),
//  中间**没有任何"这条数据像不像人"的判断**:
//      ① idx  = *(int32*)(Mesh + 0x744)
//         数组 = *(void**)(Mesh + 0x700 + idx*16)
//      ② 变换指针 = *(void**)(Mesh + 0x200)  → FTransform → 局部转世界矩阵
//      ③ 世界 = 矩阵 × 数组[写死下标].Translation → 投影 → 按写死的 17 段连线
//
//  上一版在这里堆了三层自研判定, 每一层都是"整条骨骼不画"的口子, 这一版整段删掉:
//    · 「骨架体检」—— 17 段里合格几段 / 骨架世界高度必须落在 60~220cm /
//      每个关节必须离角色 3m 内。这是拿"像不像人"决定"画不画", 参考实现里没有这道门。
//    · 「数组 5 候选 × 变换 2 候选 = 10 路试读, 谁像人用谁」+ 跨帧选中缓存 + 重挑冷却
//      —— 把一条固定链变成每帧浮动的选择, 选中一漂骨骼就换形状。
//    · 「投出来的点必须在屏幕 1.5 倍范围内」+「成数 >= 8 才算成功」
//      —— 边缘/贴脸目标少几个点位就整条不画。
//      参考实现是"这一段的两端都在就画这一根", 从不因为"点太少"放弃整个人。
//
//  现在只有一条路: 读得到就读, 读不到就是这一帧没数据(下一帧再读), 不缓存、不挑路、
//  不做几何判定。点的独立性交给 17 段连线那一层(哪一段两端齐了就画哪一段)。
// ============================================================================
bool 算参考骨骼(uint64_t Mesh, UE4类型::FVector 角色位置, const UE4算法::相机信息& 相机,
              参考骨骼点位& 输出) {
    输出 = 参考骨骼点位();
    if (!kfd.地址判断(Mesh)) return false;

    // ① 数组族: 当前用哪一路 + 那一路的数组指针
    int idx = kfd.取vreadbuf<int>(Mesh + 参考骨_数组索引);
    if (idx < 0 || idx > 7) { g_骨源诊断 = "索引越界"; return false; };

    uint64_t 数组 = kfd.取vreadbuf<uint64_t>(Mesh + 参考骨_数组族 + (uint64_t)idx * 16);

    // ② 变换指针 → 局部转世界矩阵
    uint64_t 变换指针 = kfd.取vreadbuf<uint64_t>(Mesh + 参考骨_变换指针);
    if (!kfd.地址判断(数组) || !kfd.地址判断(变换指针)) { g_骨源诊断 = "指针空"; return false; };

    UE4类型::FTransform 变换 = UE4算法::取Transform(变换指针);
    if (!UE4算法::是数(变换.Translation)) { g_骨源诊断 = "变换脏"; return false; };
    UE4类型::FMatrix 矩阵 = UE4算法::取Transform矩阵(变换);

    // ③ 18 个写死下标 → 组件空间 → 世界 → 屏幕
    //    每个点各自独立投影: 投不出来的只是这个点缺席, 不是"这个目标不画"。
    float 宽 = 设备类型.设备信息.宽度;
    float 高 = 设备类型.设备信息.高度;
    int 成数 = 0;
    for (int i = 0; i < 参考骨数; i++) {
        int 骨 = 参考骨骼序号[i];
        UE4类型::FVector 局部 = kfd.取vreadbuf<UE4类型::FVector>(
            数组 + SDK::FTransform::translation + (uint64_t)骨 * 0x30);
        if (!UE4算法::是数(局部)) continue;          // 只挡 NaN/无穷(脏内存), 不是"不像人"
        UE4类型::FVector 世界 = UE4算法::变换点(矩阵, 局部);
        ImVec2 屏幕;
        if (!UE4算法::投影点(世界, 相机, 宽, 高, 屏幕)) continue;
        输出.点[i] = 屏幕;
        输出.世界[i] = 世界;      // ★ r37: 世界点留着 —— 方框/头甲/距离直接用
        输出.有效[i] = true;
        成数++;
    };
    输出.成数 = 成数;
    g_骨源诊断 = "单链(idx" + to_string(idx) + ") 投" + to_string(成数) + "/18";
    return 成数 > 0;      // 有一个点就有机会画出一段线 —— 不再要求"成数 >= 8"
}


// ============================================================================
//  ★★★ r37 —— 骨骼定位点: 方框/头甲/名字/距离 全部改挂这条链 ★★★
//
//  为什么这么改(实机现象 → 根因):
//    现象: 骨骼不偏, 方框/头甲/距离"有时候脱离人物画在别处"。
//    根因: 两条链的坐标源根本不是同一个东西 ——
//      · 骨骼   = 网格的变换指针(0x200)* 组件空间点   ← 实测不偏, 这条路是对的
//      · 其它   = Actor 层三源投票(相对位置/根世界变换/网格世界变换互证)
//                 投票会**逐帧翻源**, 只要当帧挑中的那个源有偏差, 框整块就跳走;
//                 头甲/距离又是挂在"框顶/框底"上的, 于是跟着一起跳。
//
//  治法: 位置类功能不再自己找坐标, 直接花骨骼算出来的世界点 —— 骨骼准, 它们就准。
//    · 质心 = 所有有效世界点的平均 → 一定落在人体内部(比 Actor 根位置更贴身)
//    · 头点 = 世界 Z 最高的那个点 → 头甲/名字贴真头顶, 不再间接挂在框顶
//  不做任何"像不像人"的判断 —— 点有效就用, 跟骨骼链一个口径。
// ============================================================================
bool 取骨骼定位点(const 参考骨骼点位& 骨, UE4类型::FVector& 质心, UE4类型::FVector& 头点) {
    int 有效数 = 0;
    int 最高 = -1;
    float 和X = 0.0f, 和Y = 0.0f, 和Z = 0.0f;
    for (int i = 0; i < 参考骨数; i++) {
        if (!骨.有效[i]) continue;
        if (!UE4算法::是数(骨.世界[i])) continue;
        和X += 骨.世界[i].X; 和Y += 骨.世界[i].Y; 和Z += 骨.世界[i].Z;
        有效数++;
        if (最高 < 0 || 骨.世界[i].Z > 骨.世界[最高].Z) 最高 = i;
    };
    if (有效数 <= 0) return false;
    质心 = { 和X / (float)有效数, 和Y / (float)有效数, 和Z / (float)有效数 };
    头点 = (最高 >= 0) ? 骨.世界[最高] : 质心;
    return true;
};

// ============================================================================
//  ★★★ r36 —— 方框: 1:1 复刻参考实现的画法 ★★★
//
//  它那条框(反汇编 0x100091a08 ~ 0x100092be4 逐条读出来的)只做四件事:
//    ① 两个世界点各投一次: 角色根位置, 和"角色根位置 + 90cm"
//    ② h = |两个屏幕点的纵向差|
//    ③ 以"根位置的屏幕 X"为中心: 左 = X - h/2, 右 = X + h/2, 上 = Y - h, 下 = Y + h
//       → **框宽 = h、框高 = 2h**; 四条边各自只画两端各 1/4(角框, 中间留空)
//    ④ 画。没了。
//
//  它这条链里**一道门都没有**: 没有"框太小不画"、没有"框太大不画"、
//  没有"距离太远不画"、没有"跑到屏幕外不画"。投得出两个点就出框 —— 这就是它不漏框的全部秘密。
//  上一版这里还留着 `h < 0.5f 判失败`(等于"框高不到 1 像素就整个人不画"), 已删。
// ============================================================================
bool 算参考框(UE4类型::FVector 角色位置, const UE4算法::相机信息& 相机,
             float 身高, float 框宽倍率, 目标点位& 输出) {
    输出 = 目标点位();
    if (!UE4算法::是数(角色位置)) return false;

    float 宽 = 设备类型.设备信息.宽度;
    float 高 = 设备类型.设备信息.高度;

    // 参考实现: 第二个采样点固定 = 根位置 + 90cm, 不读身高、不做区间判断、不做合理性校验
    UE4类型::FVector 头顶世界 = { 角色位置.X, 角色位置.Y, 角色位置.Z + 90.0f };

    ImVec2 屏幕中心, 屏幕头顶;
    // 投影本身已经不丢目标了(深度钳 1); 这里投不出来只可能是相机无效/坐标彻底不是数
    if (!UE4算法::投影点(角色位置, 相机, 宽, 高, 屏幕中心)) return false;
    if (!UE4算法::投影点(头顶世界, 相机, 宽, 高, 屏幕头顶)) return false;

    float h = fabsf(屏幕中心.y - 屏幕头顶.y);
    if (!UE4算法::是数(h)) return false;      // 只挡 NaN(喂给 ImGui 会画出乱线), 不判大小

    // 框宽 = h; 可乘 plist 里那条微调倍率(默认 1.0 = 参考实现原样)
    float 宽倍 = (框宽倍率 > 0.1f && 框宽倍率 < 5.0f) ? 框宽倍率 : 1.0f;

    输出.框顶 = 屏幕中心.y - h;
    输出.框底 = 屏幕中心.y + h;
    输出.中心X = 屏幕中心.x;
    输出.框宽 = h * 宽倍;
    输出.框左 = 屏幕中心.x - h * 宽倍 * 0.5f;
    输出.框右 = 屏幕中心.x + h * 宽倍 * 0.5f;
    输出.头有效 = true;
    输出.头世界 = 头顶世界;
    输出.用骨骼 = false;
    输出.有效数 = 2;
    return true;
}

// ---- 头甲: 角色 → 装备层(0x1A78 = CmpCacheArray+0x1A0, 第52号缓存槽)
//            → +0x260(ArmorList) → 两个指针 → 各自 +0x730(ArmorLevel, 整数) ----
// 值全部来自 SDK.h（那边有 dump/反汇编出处），这里只起别名。
constexpr uint64_t 参考头甲_装备层 = SDK::ASGCharacter组件表::CmpCacheArray + SDK::头甲链::护甲管理器缓存槽;
constexpr uint64_t 参考头甲_槽位层 = SDK::USGCharacterArmorManagerComponent::ArmorList;
constexpr uint64_t 参考头甲_数值   = SDK::ASGInventory::ArmorLevel;

struct 头甲数据 {
    int  头 = -1;
    int  甲 = -1;
    bool 有效 = false;
};

头甲数据 取头甲(uint64_t Actor) {
    头甲数据 出;
    if (!kfd.地址判断(Actor)) return 出;

    uint64_t 甲层 = kfd.取vreadbuf<uint64_t>(Actor + 参考头甲_装备层);
    if (!kfd.地址判断(甲层)) return 出;

    uint64_t 槽层 = kfd.取vreadbuf<uint64_t>(甲层 + 参考头甲_槽位层);
    if (!kfd.地址判断(槽层)) return 出;

    uint64_t 头对象 = kfd.取vreadbuf<uint64_t>(槽层);
    uint64_t 甲对象 = kfd.取vreadbuf<uint64_t>(槽层 + 8);

    if (kfd.地址判断(头对象)) 出.头 = kfd.取vreadbuf<int>(头对象 + 参考头甲_数值);
    if (kfd.地址判断(甲对象)) 出.甲 = kfd.取vreadbuf<int>(甲对象 + 参考头甲_数值);

    bool 头可用 = (出.头 >= 0 && 出.头 < 1000000);
    bool 甲可用 = (出.甲 >= 0 && 出.甲 < 1000000);
    出.有效 = 头可用 || 甲可用;
    if (!头可用) 出.头 = 0;
    if (!甲可用) 出.甲 = 0;
    return 出;
}

// 用一套下标把 16 个关节的世界坐标算出来
//   ★ 本版矩阵只有一条通道: 网格的 ComponentToWorld。
//     骨骼读法两条(同一个矩阵下读哪个数组, 只是读法不同, 不是两个来源):
//        1 = CachedComponentSpaceTransforms            (首选, 组件空间)
//        2 = CachedBoneSpaceTransforms 沿父链累乘      (兜底)
bool 算关节世界(uint64_t Mesh, UE4类型::FVector 角色位置, 骨架方案* 方案, int 兜底号,
                const int* 序号, int 骨骼来源优先,
                UE4类型::FVector 世界[槽位总数], bool 有效[槽位总数], int& 用的矩阵来源) {
    for (int i = 0; i < 槽位总数; i++) { 有效[i] = false; 世界[i] = {0.0f, 0.0f, 0.0f}; };

    UE4类型::TArray 组件空间 = kfd.取vreadbuf<UE4类型::TArray>(
        Mesh + SDK::USkeletalMeshComponent::CachedComponentSpaceTransforms);
    UE4类型::TArray 骨骼空间 = kfd.取vreadbuf<UE4类型::TArray>(
        Mesh + SDK::USkeletalMeshComponent::CachedBoneSpaceTransforms);

    bool 组件可用 = kfd.地址判断(组件空间.AllocatorInstance) && 组件空间.ArrayNum > 0 &&
                    组件空间.ArrayNum < 2048 && 组件空间.ArrayNum <= 组件空间.ArrayMax;
    bool 骨骼可用 = kfd.地址判断(骨骼空间.AllocatorInstance) && 骨骼空间.ArrayNum > 0 &&
                    骨骼空间.ArrayNum < 2048 && 骨骼空间.ArrayNum <= 骨骼空间.ArrayMax;
    if (!组件可用 && !骨骼可用) return false;

    // ---- 矩阵: 唯一通道, 网格的 ComponentToWorld (不求逆 / 不双通道 / 不扫偏移) ----
    UE4算法::网格变换信息 网格变换 = UE4算法::取网格变换(Mesh, 角色位置);
    if (!网格变换.有效) { 用的矩阵来源 = 0; return false; };
    UE4类型::FMatrix 矩阵 = 网格变换.矩阵;
    用的矩阵来源 = 1;

    // ---- 骨骼读法: 组件空间优先, 累乘兜底(都没过人形校验就放弃这一帧) ----
    int 来源序[2] = { 1, 2 };   // 1 = 组件空间  2 = 骨骼空间累乘
    for (int s2 = 0; s2 < 2; s2++) {
        int 骨骼来源 = 来源序[s2];
        if (骨骼来源优先 != 0 && 骨骼来源 != 骨骼来源优先) continue;
        if (骨骼来源 == 1 && !组件可用) continue;
        if (骨骼来源 == 2 && !骨骼可用) continue;

        int 成数 = 0;
        for (int 槽 = 0; 槽 < 槽位总数; 槽++) {
            int 骨 = -1;
            if (兜底号 >= 0 && 序号 != nullptr) 骨 = 序号[槽];
            else if (方案 != nullptr) 骨 = 方案->关节[槽];
            if (骨 < 0) continue;

            UE4类型::FVector 局部;
            if (骨骼来源 == 1) {
                if (!读骨头局部(组件空间.AllocatorInstance, 骨, 局部)) continue;
            } else {
                UE4类型::FTransform 累好 = 累乘父链(骨骼空间.AllocatorInstance,
                                                    方案 != nullptr ? 方案->父 : vector<int>(),
                                                    骨, 32);
                if (!UE4算法::变换可用(累好)) continue;
                局部 = 累好.Translation;
            };

            if (!UE4算法::骨骼坐标合理(局部)) continue;
            UE4类型::FVector 世界点 = UE4算法::变换点(矩阵, 局部);
            if (!UE4算法::是数(世界点)) continue;

            世界[槽] = 世界点;
            有效[槽] = true;
            成数++;
        };

        if (成数 >= 12 && 人形校验(世界, 有效, 角色位置)) {
            g_网格骨骼数组[Mesh] = (骨骼来源 == 1) ? 1 : 2;
            return true;
        };
    };
    return false;
};

//  ★ 修复版16 —— 目标世界坐标: 三源两两互证, 逐目标选(治"框顺着视角在那附近漂")
//
//  三个源都有可能是脏的, 但脏得不一样:
//    甲 = Root + RelativeLocation(0x154)    第一版口径; 一旦根组件被挂到父级下,
//                                            它给的是**相对父组件**的位置, 不是世界坐标
//    乙 = Root + ComponentToWorld(0x120)    引擎每帧刷新的绝对世界变换
//    丙 = Mesh + ComponentToWorld(0x120)    网格世界位置(渲染原点, 必然贴角色)
//
//  实机实测:v14 那局里 甲 与 乙/丙 差了 21955cm(219 米) —— 有角色的相对位置根本不是
//  世界坐标, 用甲画框就会整框顺着视角漂到别处。所以这里不再"认死一个源", 改成互证:
//    ① 甲 跟 丙 对得上(1.5m 内) → 用甲(第一版口径, 优先)
//    ② 乙 跟 丙 对得上           → 用乙
//    ③ 甲 跟 乙 对得上           → 两个都真, 用甲
//    ④ 甲被证伪而乙有效          → 用乙
//    ⑤ 都不成立                  → 有乙用乙, 有丙用丙, 都没有才回甲
//  每一条都要求"另一个独立源点头", 没有"读不到就放行"。
// ============================================================================
constexpr float 源一致容差 = 150.0f;   // 厘米级容差(1.5 米)

UE4类型::FVector 选目标位置(const 玩家数据& 玩家, int& 用了哪个) {
    const bool 甲可用 = 玩家.甲有效 && UE4算法::长度(玩家.源甲) > 1.0f;
    const bool 乙可用 = 玩家.乙有效 && UE4算法::长度(玩家.源乙) > 1.0f;
    const bool 丙可用 = 玩家.丙有效 && UE4算法::长度(玩家.源丙) > 1.0f;

    if (甲可用 && 丙可用 &&
        UE4算法::取距离(玩家.源甲, 玩家.源丙) * 100.0f <= 源一致容差) { 用了哪个 = 0; return 玩家.源甲; };
    if (乙可用 && 丙可用 &&
        UE4算法::取距离(玩家.源乙, 玩家.源丙) * 100.0f <= 源一致容差) { 用了哪个 = 1; return 玩家.源乙; };
    if (甲可用 && 乙可用 &&
        UE4算法::取距离(玩家.源甲, 玩家.源乙) * 100.0f <= 源一致容差) { 用了哪个 = 0; return 玩家.源甲; };
    if (乙可用) { 用了哪个 = 1; return 玩家.源乙; };
    if (丙可用) { 用了哪个 = 2; return 玩家.源丙; };
    用了哪个 = 0;
    return 玩家.源甲;
};

// ============================================================================
//  四、死亡判定 —— 三源独立取证, 逐角色出结论 (不靠推算、不留后门)
//
//  【主源·权威】 USGCharacterDeathComponent + 0x1F8 = FCharacterDeathInfo
//        +0x00 bool  bIsDead      ← 游戏内部 IsDead() 读的就是它 (Net, 服务端同步)
//        +0x01 bool  bForceDied   ← 强制死亡
//        +0x04 float Timestamp    ← 死亡时间戳, 死过必然 >0
//        +0x08 int32 DeathCount
//     判读规则是硬的, 不给含糊空间:
//        ① bIsDead 必须是严格 0/1 —— 读到别的值就是地址错, 该源当场作废
//        ② bIsDead=1 与 Timestamp>0 互为交叉验证, 两个都成立 = 铁证
//        ③ bIsDead=1 但时间戳读不出来 → 仍判死 (主标志已通过严格校验)
//        ④ bIsDead=0 → 再看血量补一刀 (服务端标志还没翻的极限情况)
//
//  【辅源·交叉】 血量属性集: 只在主源作废时接手, 三个条件全中才判死
//        (上限合理 + 当前值合理 + 真归零), 缺一个就转"未定"。
//        ★ r27: 只有**按类名命中的** USGActorHealthAttributeSet 才有资格判死;
//               兜底拿到的"第 0 个属性集"只准说活。理由见下面 确认死亡 的段首。
//
//  【未定】 两源都没拿到有效证据 → 沿用该角色 1.5 秒内的上一次结论,
//        超时按活画, 但**未定数会打在屏幕上**。
//        ★ 和上一版的区别: 上一版是"读不到就静默放行"(你永远不知道),
//          现在是"读不到就明说", 而且主源正常时压根用不到兜底。
//        ★ 倒地(DBNO)不是死: 倒地时 bIsDead 仍为 0, 用血量法会把人误判成死。
// ============================================================================

// 死亡结论
enum { 结论未定 = -1, 结论活 = 0, 结论死 = 1 };

// 死源标记(诊断用): 0=两源全断 1=主源生效 2=血量兜底 3=主源说活但血量补刀
enum { 源全断 = 0, 源主源 = 1, 源血量 = 2, 源补刀 = 3 };   // 源补刀 r27 起已停用(保留只为读旧日志)

// ---- 未定兜底缓存: 按角色记上一次确认结论, 1.5 秒内有效 ----
//  ★ r29: 槽位 64 → 256。这个表是按角色地址哈希落槽的, 名单一涨(现在能到 128),
//    64 个槽必然互相踩 —— A 的"死"结论会被套到 B 头上, 表现就是偶发漏人。
//    256 槽 + 时间戳校验, 撞槽概率降到可忽略。
//  ★ r30: 256 → 512。名单上限放宽到 192 之后, 256 槽的负载又逼近 0.75, 撞槽重新出现;
//    槽表必须始终显著大于名单, 否则扩容名额反而换来新的误判漏人。
//  ★ r34: 512 → 2048。名单上限提到 512, 槽表同步提到 4 倍(负载 ~0.25)。
//    这一条不许漏: 只放名额不放槽表 = 撞槽率回升 = 又一种"把 A 的死结论套到 B 头上"的漏人。
//    内存代价 2048×(8+1+8) ≈ 35KB, 可忽略。
// ★ r35: 槽表 2048 → 4096。名单上限提到 1024 之后, 2048 槽的负载又逼近 0.5,
//   撞槽会把 A 的结论套到 B 头上。规矩照旧: 槽表始终 ≥ 名单的 4 倍。
//   内存代价 4096×(8+1+8) ≈ 70KB, 可忽略。
//   (注: r35 起这张表只做"证据留档", 不再参与'把谁剔掉'的决定 —— 判定只认现场取证。)
static uint64_t g_死缓存角色[4096] = {0};
static bool     g_死缓存结论[4096] = {false};
static double   g_死缓存时间[4096] = {0.0};

inline int 死缓存槽(uint64_t 角色) { return (int)((角色 >> 4) & 4095); };

// r35: 判定链不再调用它(没证据就是画, 不拿历史结论补刀), 留作诊断/留档用途。
[[maybe_unused]] bool 查上次结论(uint64_t 角色, bool& 有结论) {
    有结论 = false;
    int 槽 = 死缓存槽(角色);
    if (g_死缓存角色[槽] != 角色) return false;
    if (现在秒() - g_死缓存时间[槽] > 1.5) return false;
    有结论 = true;
    return g_死缓存结论[槽];
};

void 存结论(uint64_t 角色, bool 死了) {
    int 槽 = 死缓存槽(角色);
    g_死缓存角色[槽] = 角色;
    g_死缓存结论[槽] = 死了;
    g_死缓存时间[槽] = 现在秒();
};

// ---- 主源取证（无畏契约：无死亡组件，此函数恒返回"未定"，血量是唯一源） ----
int 主源取证(uint64_t 死亡组件, bool& 可用) {
    可用 = false;
    return 结论未定;   // ★ 无畏无 USGCharacterDeathComponent，主源永远不可用
};

// ---- 血源取证（无畏契约：血量走 GAS，健康集参数 = ASC 指针） ----
//  健康集(ASC) → +0x20F8 AttributeBasic → +0xD8 Health(FGameplayAttributeData)
//  读 Health.CurrentValue(0x0) 和 MaxHealth.CurrentValue(0x170+0x0)
int 血源取证(uint64_t 健康集, bool& 可用) {
    可用 = false;
    if (!kfd.地址判断(健康集)) return 结论未定;

    // ★ 无畏: 健康集参数是 ASC，先走 +0x20F8 拿 AttributeBasic
    uint64_t AttributeBasic = kfd.取vreadbuf<uint64_t>(健康集 + SDK::USGASComponent::AttributeBasic);
    if (!kfd.地址判断(AttributeBasic)) return 结论未定;

    float 上限 = kfd.取vreadbuf<float>(AttributeBasic + SDK::USGActorHealthAttributeSet::HealthMax);
    float 当前 = kfd.取vreadbuf<float>(AttributeBasic + SDK::USGActorHealthAttributeSet::Health);
    if (!(上限 > 0.5f && 上限 < 1000000.0f)) return 结论未定;
    if (!UE4算法::是数(当前)) return 结论未定;
    if (当前 < -1.0f || 当前 > 上限 * 2.0f) return 结论未定;

    可用 = true;
    if (当前 > 0.05f) return 结论活;      // 血还在 → 活着
    return 结论死;                        // ★ 无畏无倒地(DBNO)，血 ≤ 0 就是死
};

// ---- 总判定: 返回 true = 死透了, 不画 ----
//  ★ 无畏契约: 只有血量这一个源。ASC/AttributeBasic 读不到 → 按活画（不误剔）。
bool 确认死亡(uint64_t 角色, uint64_t 死亡组件, uint64_t 健康集, bool 血量集可信, int& 死源标记) {
    // 无畏无死亡组件，直接走血量
    int 血 = 血源取证(健康集, 血量集可信);
    if (血量集可信) {
        死源标记 = 源血量;
        if (血 == 结论死) { 存结论(角色, true); return true; };
        存结论(角色, false);
        return false;
    };

    // 血量也读不到 → 按活画，不拿历史结论补刀（同暗区 r35 的思路）
    死源标记 = 源全断;
    return false;
};

// ============================================================================
//  五、无后坐力 (r26: 改成"只关相机、一个字不碰后坐力数据")
//
//  ── 为什么把 r25 那两行删了 ──────────────────────────────────────────────
//  r25 做三件事:
//    ① 相机总闸 bEnableModifyRotationByWeaponRecoil 写 0        ← 保留
//    ② 武器组件 bEnableRecoil_Pitch / bEnableRecoil_Yaw 写 0    ← 删(危险)
//    ③ RepRecoilInfo 那 48 字节清零                              ← 删(最危险)
//
//  ② ③ 是在**主动上报"我的后坐力是零"**。这两个字段是上行同步的:
//    dump: USGWeaponRecoilComponent::RepRecoilInfo @0xF8 size=48,
//    结构 FWeaponRecoilRepInfo 里同时装着 ShotNum / FireRecoil / RotatorDelta /
//    TargetRecoil —— 也就是"第几发 / 实际后坐力 / 视角转了多少 / 这把枪应有的后坐力"。
//    服务器手里有同武器同段位的基准, 一减就是差值:
//      实际值恒 0  →  差值恒等于"应有值"  →  恒定最大异常, 一眼看得出的结构性改动。
//
//  ① 这一刀就够用, 而且**一个字都不碰后坐力数据**:
//    后坐力照样算、照样上报(全是真值), 只是不再拿去修改相机旋转 ——
//    手感上就是"枪口跳、准星不跳"。服务器要抓这个, 得把"上报的后坐力曲线"
//    和"你的视角轨迹"做联合建模才看得出来, 跟"数据本身是零"完全不是一个难度。
//
//  ── 压枪系数(可选, 默认关) ─────────────────────────────────────────────
//  plist: 后坐系数密钥 = 0.20 ~ 0.90(倍率)。没设 / 设 0 → 完全不启用(只走 ①)。
//    启用后只缩放 MaxVerticalRecoil(0x150) / MaxHorizontalRecoil(0x154) 两个上限:
//      · 带 ±10% 随机抖动 —— 恒定值本身就是指纹, 随机值才像人
//      · 刷新节流 0.4 秒一次, 写前比对(变化不到 2% 就不写) —— 少写一次少一分痕迹
//      · 换枪自动重新认原值; 关掉立即把原值写回
//      · 低于 0.2 直接钳到 0.2 —— 再低就等于把后坐力抹平, 又变成异常数据了
//    散布/弹道比(RecoilAccuracyRatios @0x180)一个字都不动: 动了 = 子弹太准, 更扎眼。
// ============================================================================
int g_后坐命中 = 0;          // ① 相机总闸写成功次数
int g_后坐组件命中 = 0;      // ② 压枪系数写成功 / 恢复写回次数
int g_后坐组件跳过 = 0;      // ② 值本来就对 / 没到刷新点 → 跳过次数
bool g_后坐闸读回 = false;   // 关掉之后读回来确认
bool g_后坐待恢复 = false;   // 曾经关过总闸, 还需要写回 true
float g_压枪系数实际 = 0.0f; // 本帧生效的压枪系数(0 = 没启用)

// ---- 压枪: 原值备份 + 写入节流 + 自有随机数(不引 libc 的 rand) ----
static uint64_t g_压枪原武器   = 0;
static float    g_压枪原垂直   = 0.0f;
static float    g_压枪原水平   = 0.0f;
static double   g_压枪上次刷新 = 0.0;
static bool     g_压枪已生效   = false;
static uint32_t g_压枪随机种子 = 0x9E3779B9u;

static inline float 压枪随机() {
    g_压枪随机种子 ^= g_压枪随机种子 << 13;
    g_压枪随机种子 ^= g_压枪随机种子 >> 17;
    g_压枪随机种子 ^= g_压枪随机种子 << 5;
    return (float)(g_压枪随机种子 & 0xFFFFu) / 65535.0f;   // [0,1]
};

void 无后坐力每帧(uint64_t 相机管理器, bool 开关, uint64_t 武器组件, float 压枪系数) {
    // ================================================================
    //  ★ 无畏契约改造: 无后座 = 写 AttributePawn::RecoilRate = 0
    //  无畏后座是 GAS 属性（USGAttributePawn::RecoilRate @ 0xE88），
    //  不是暗区那种"相机总闸 + 压枪系数"。直接把 RecoilRate 的
    //  CurrentValue 和 BaseValue 都写成 0，就是无后座。
    //  武器组件参数在这里是 ASC 的 AttributePawn 指针（子线程已定位）。
    // ================================================================
    g_压枪系数实际 = 0.0f;
    const bool 有属性集 = kfd.地址判断(武器组件);   // 武器组件参数 = AttributePawn 指针
    if (!有属性集) {
        g_后坐闸读回 = false;
        g_后坐组件命中 = 0;
        return;
    };

    // RecoilRate @ 0xE88，FSGBaseAttributeData 布局：CurrentValue@0 BaseValue@4
    const uint64_t 后座址 = 武器组件 + SDK::USGWeaponRecoilComponent::RecoilRate;
    const uint64_t 散布址 = 武器组件 + SDK::USGWeaponRecoilComponent::InAccuracyMultiple;

    if (开关) {
        // 无后座: 写 RecoilRate 的 CurrentValue + BaseValue = 0
        float 零 = 0.0f;
        float 现后座 = kfd.取vreadbuf<float>(后座址);
        if (fabsf(现后座) > 0.0001f) {
            if (kfd.写vwritebuf列表(后座址, &零, 4)) g_后坐命中++;
            kfd.写vwritebuf列表(后座址 + 4, &零, 4);   // BaseValue 也写 0
        };
        g_后坐闸读回 = true;
        g_压枪系数实际 = 0.0f;
    } else {
        // 关掉无后座: 不动内存，让游戏自己算后座（无畏 GAS 属性恢复靠游戏自己）
        g_后坐闸读回 = false;
        g_压枪系数实际 = 0.0f;
    };
};

// ============================================================================
//  五之二、检测面维护 (r26 新增) —— 只治一条: "外部读内存的副作用"别被上报
//
//  为什么只做这一条, 不做别的:
//    游戏客户端自己统计"我这一帧抖了没有", 抖了就累加 FrameRateJitterCount,
//    再**主动上报**给服务器 —— 我们靠内核高频读内存, 一旦把帧时间拖毛, 这个
//    计数就涨, 等于自己举报自己。这是外部读写这条路唯一"自带"的检测面。
//
//  做两件事:
//    ① 从源头降: 绘制帧里那三格纯诊断的相机读数降采样(见主线程里的 相机诊断帧)
//    ② 兜个底: 每 2 秒看一次这个计数, 涨了就清零(写后读回校验)
//
//  刻意**不做**的事(做了反而更糟):
//    · 不动 USGPCStatisComponent::IsOpenReport(上报总闸) —— 服务器收不到数据
//      本身就是异常信号, 比收到正常数据扎眼得多
//    · 不动自瞄统计(AntiHackStatis / ServerRecordNum / InvalidRecordNum) ——
//      本来就没有自瞄功能, 这几个数本来就该是 0, 去写它 = 在干净数据上留指纹
//    · 不动武器校验失败数组 / 反外挂流水数组 —— 那是 TArray, 外部乱写必崩
// ============================================================================
int  g_抖动清零次数 = 0;
int  g_抖动最后读数 = 0;
bool g_抖动链路通   = false;

// ---- ★ r38: 反外挂上报面 ----
//  清的原则跟 r26 一脉相承, 但把"同一性质"的清零点一次捞齐:
//    ① FrameRateJitterCount            帧率抖动计数      —— 外部读内存的副作用, r26 已接
//    ② AllAntiCheatFlowStructs         反外挂流水        —— {WeaponName, AntiType, Param1~5}
//    ③ AllWeaponValidateFailInfo       武器校验失败记录  —— {WeaponId, FailType, ReasonStr}
//    ④ ClientPendingResolveCheatInfos  客户端作弊上报队列 —— {bHasReport, GID, 7 个作弊位}
//
//  为什么清的是"逻辑长度"而不是内容:
//    在干净数据上**改写内容**才是留指纹(内容变过 = 有人动过), 而"这个数组是空的"
//    是完全正常的状态(没触发过就是空的)。清长度 = 让客户端自己认为"我没什么可报的",
//    比抹内容干净得多。
int  g_流水清条数   = 0;
int  g_武器失败清条数 = 0;
int  g_上报队列清条数 = 0;
int  g_清点链路     = 0;    // 位图: 1=反外挂流水通 2=武器校验通 4=上报队列通
int  g_清点本轮     = 0;    // 本轮实际清了几个数组(0 = 本来就是空的)

// 安全清 TArray 逻辑长度 —— 只把 ArrayNum 写 0, **绝不碰 AllocatorInstance**。
//   UE4 TArray 头: { AllocatorInstance*@0, ArrayNum@8(int32), ArrayMax@0xC(int32) }
//   清 Num 不释放任何内存、不触发 free/realloc —— 后续 Add 从 0 号位覆盖重写,
//   数组依然是个完全合法的 TArray。这是"只读为主"这条路上最轻的一种写。
//
//   四道护栏全过才写, 写后读回校验; 任何一道不过 = 判定"读串了/链错了", 一律不碰:
//     · 数组地址有效
//     · 0 < Num <= 4096          (本来就是空的就别写 —— 少写一次少一次痕迹)
//     · Num <= Max <= 65536      (容量关系不可能倒挂, 倒挂就是读串)
//     · AllocatorInstance 有效   (有长度就必须有分配)
//   返回: 实际清掉的条数(0 = 没清 / 本来就是空)
int 清TArray长度(uint64_t 数组址) {
    if (!kfd.地址判断(数组址)) return 0;

    const int Num = kfd.取vreadbuf<int>(数组址 + SDK::TArray长度::ArrayNum偏移);
    const int Max = kfd.取vreadbuf<int>(数组址 + SDK::TArray长度::ArrayMax偏移);

    if (Num <= 0 || Num > SDK::TArray长度::合理上限) return 0;
    if (Max < Num || Max > SDK::TArray长度::容量上限) return 0;

    const uint64_t 分配 = kfd.取vreadbuf<uint64_t>(数组址);
    if (!kfd.地址判断(分配)) return 0;

    int 零 = 0;
    if (!kfd.写vwritebuf列表(数组址 + SDK::TArray长度::ArrayNum偏移, &零, 4)) return 0;
    if (kfd.取vreadbuf<int>(数组址 + SDK::TArray长度::ArrayNum偏移) != 0) return 0;

    return Num;
};

// ============================================================================
//  ★ 无畏契约改造: 暗区专属的"反外挂检测面维护"整段禁用。
//    无畏契约没有 USGPlayerStateStatisComponent / ASGGameState::GameStateStatisComponent
//    这些上报子系统，清理它们等于在无畏里写野指针。用 #if 0 保留代码可回退。
// ============================================================================
#if 0
void 检测面维护(uint64_t 自己Pawn) {
    static double 上次维护 = 0.0;
    double 此刻 = 现在秒();
    if (此刻 - 上次维护 < 1.0) return;      // r38: 2s → 1s。上报队列是有窗口的, 清得越勤越难漏
                                             //        读量约十次/秒级别, 相对全表扫描可以忽略
    上次维护 = 此刻;

    g_清点链路 = 0;
    g_清点本轮 = 0;

    // ---- 玩家统计组件链: 自己Pawn -> PlayerState -> SGPlayerStateStatisComponent ----
    uint64_t 统计 = 0;
    if (kfd.地址判断(自己Pawn)) {
        uint64_t 状态 = kfd.取vreadbuf<uint64_t>(自己Pawn + SDK::APawn::PlayerState);
        if (kfd.地址判断(状态)) {
            统计 = kfd.取vreadbuf<uint64_t>(状态 + SDK::ASGPlayerState补::SGPlayerStateStatisComponent);
        };
    };
    g_抖动链路通 = kfd.地址判断(统计);

    // ① 帧率抖动计数 (r26 刀, 原样保留)
    //   这个字段 dump 只给了 4 字节、没给类型(int32 / float 都有先例),
    //   所以同一块内存按两种解释各看一遍, **只有像"计数"才动**:
    //   明显是垃圾数据(读串了 / 组件链错了)一律不碰 —— 宁可清不掉, 绝不乱写。
    if (g_抖动链路通) {
        const uint64_t 计数址 = 统计 + SDK::USGPlayerStateStatisComponent::FrameRateJitterCount;
        uint32_t 原始 = kfd.取vreadbuf<uint32_t>(计数址);

        int   原始整数 = 0;  memcpy(&原始整数, &原始, 4);
        float 原始浮点 = 0.0f; memcpy(&原始浮点, &原始, 4);
        g_抖动最后读数 = 原始整数;

        const bool 像整数计数 = (原始整数 > 0 && 原始整数 < 1000000);
        const bool 像浮点计数 = (原始浮点 > 0.0f && 原始浮点 < 1.0e6f);

        if (像整数计数 || 像浮点计数) {
            // 0 的字节表示对 int / float 是同一个(全 0), 所以不用管它到底是哪种
            uint32_t 零 = 0;
            if (kfd.写vwritebuf列表(计数址, &零, 4)) {
                if (kfd.取vreadbuf<uint32_t>(计数址) == 0) g_抖动清零次数++;
            };
        };

        // ---- ② 反外挂流水 / ③ 武器校验失败记录 ----
        //   dump: USGPlayerStateStatisComponent
        //     AllAntiCheatFlowStructs    @0x2A8 TArray<FAntiCheatFlowData>
        //       { FString WeaponName / int32 AntiType / float Param1~5 }
        //     AllWeaponValidateFailInfo  @0x288 TArray<FDSWeaponValidateFail>
        //       { FString WeaponId / int32 WeaponType / FString FailType / FString ReasonStr }
        //   这两条是**客户端自己攒、之后整段上报**的取证记录 —— 无后坐力这类"改过
        //   相机行为"的操作, 一旦被武器侧校验记进来, 留的就是最直白的证据。
        //   清的是逻辑长度, 不是内容(理由见段头)。护栏不过一律不碰。
        g_清点链路 |= 1;
        g_清点链路 |= 2;

        int 清掉流水 = 清TArray长度(统计 + SDK::USGPlayerStateStatisComponent::AllAntiCheatFlowStructs);
        if (清掉流水 > 0) { g_流水清条数 += 清掉流水; g_清点本轮++; };

        int 清掉失败 = 清TArray长度(统计 + SDK::USGPlayerStateStatisComponent::AllWeaponValidateFailInfo);
        if (清掉失败 > 0) { g_武器失败清条数 += 清掉失败; g_清点本轮++; };
    };

    // ---- ④ 客户端作弊上报队列 ----
    //   链路(dump 实证): UWorld::GameState@0x120 -> ASGGameState::GameStateStatisComponent@0x5E8
    //                    -> USGGameStateStatisComponent::ClientPendingResolveCheatInfos@0x348
    //   队列里每条是 { bHasReport / PlayerGID / TimeSeconds / 7 个作弊位 } ——
    //   这是"客户端判定谁作弊 -> 排队 -> 上报服务器"的通路。空 = 无话可说。
    //   ⚠️ World 来源用的是 g_上轮World(收集线程每轮刷新的当前 World), 换局后它立刻更新,
    //      不存在拿上一局的 GameState 指针乱写的问题。
    {
        const uint64_t World = g_上轮World;
        if (kfd.地址判断(World)) {
            const uint64_t GameState = kfd.取vreadbuf<uint64_t>(World + SDK::UWorld补::GameState);
            if (kfd.地址判断(GameState)) {
                const uint64_t GS统计 = kfd.取vreadbuf<uint64_t>(
                    GameState + SDK::ASGGameState::GameStateStatisComponent);
                if (kfd.地址判断(GS统计)) {
                    g_清点链路 |= 4;
                    int 清掉队列 = 清TArray长度(
                        GS统计 + SDK::USGGameStateStatisComponent::ClientPendingResolveCheatInfos);
                    if (清掉队列 > 0) { g_上报队列清条数 += 清掉队列; g_清点本轮++; };
                };
            };
        };
    };
};
#endif  // 检测面维护

// ============================================================================
//  五之三、回放链维护 (r39 新增) —— 治举报之后真正致命的那一样: 本机那份录像
//
//  完整链(dump 逐条实证, 详版见 SDK.h 的 r39 段):
//    环形录制一直在滚
//      -> 死亡时 USGPCLocalFileKillCamComponent::UpdateReplayCache() 切出一段
//      -> 写进 USGGameInstanceReplaySubsystem::LocalFileKillCamInfo @0xB8
//      -> 服务端 USGPCServerCommandComponent::PlayHttpReplay(RoomID, VictimGID) 点名,
//         经 OnRep_HttpReplayDeathInfo() 复制到本机
//      -> CheckAndPrepareHttpReplay() 从那份元数据里取 ReplayFileName + KillCamFragments
//      -> UHttpTask::SendReplayTask(ZipName) 走 HTTP 直传(不走游戏网络通道, 几秒的事)
//
//  本版只做一件事: 把本机那份回放元数据里的**两个数组**清成长度 0。
//    清掉之后 CheckAndPrepareHttpReplay() 手上没有文件名、没有逐帧片段 —— 没东西可交。
//
//  为什么只清这两个, 别的字段(GID / 名字 / 坐标 / 时间)一律不动:
//    改数值是"内容被改过", 服务端做一致性对账(拿它和击杀事件里的 GID/坐标比)时最扎眼;
//    而"这段缓存是空的"是**游戏自己就有的状态** —— 同一个子系统上挂着
//    DeleteLocalfileKillCams / DeleteTimeOutAndNumOutLocalfileKillCams / ResetKillCamInfo,
//    都是游戏自己在清这些。清数组长度跟它们的落点同性质。
//
//  ⚠️ 代价(必须清楚): 服务端点名要录像、这边拿不出来, 这件事本身就是个信号。
//     它挡的是"取证", 挡不住"行为" —— 服务端算的是录制数据里的几何关系
//     (预瞄角度 / 隔墙跟人 / 双方位置间有没有遮挡), 那是它自己算的, 不读你进程一个字节。
//
//  ⚠️ 时限性: UpdateReplayCache() 是死亡那一刻才填的, 填完到被点名通常有几秒到几十秒;
//     这里跟检测面维护同频(1 秒一次), 撞进同一秒的窗口极小 —— 但**不是零**。
// ============================================================================
int      g_回放清片段   = 0;    // 累计清掉的回放片段条数
int      g_回放清队列   = 0;    // 累计清掉的待下载条数
int      g_回放清文件名 = 0;    // 累计清掉的文件名次数
int      g_回放链路     = 0;    // 位图: 1=子系统已定位 2=本轮清到东西
uint64_t g_回放子系统   = 0;

// ---- 按类名找**实例**(跳过 CDO) ----
//   GObjects 里既有 CDO(对象名带 "Default__" 前缀)也有真实例; 回放元数据挂在真实例上,
//   所以类对上之后还得把 CDO 那一个剔掉。
//   分片扫 —— 一轮只扫一段, 扫完即返回, 绝不把读取线程钉死(同 找类增量 的思路)。
struct 实例定位任务 {
    string   目标;
    int      游标 = 0;
    uint64_t 结果 = 0;
    bool     完成 = false;
    double   失败时刻 = 0.0;
};
实例定位任务 g_实例定位;
constexpr int 实例每轮步长 = 4000;   // 比找类的 6000 保守: 这里每个对象多一次类名解码

uint64_t 找实例增量(const string& 目标类名) {
    if (g_实例定位.结果) return g_实例定位.结果;
    if (!g_对象表已定位 || !kfd.地址判断(g_名字池)) return 0;

    if (g_实例定位.完成) {
        // 扫完一遍没找到 —— 退避 20 秒再重来, 别每轮都从 0 号位重扫(白烧读量)
        if (现在秒() - g_实例定位.失败时刻 < 20.0) return 0;
        g_实例定位 = 实例定位任务();
    };
    if (g_实例定位.目标 != 目标类名) {
        g_实例定位 = 实例定位任务();
        g_实例定位.目标 = 目标类名;
    };

    int 结束 = g_实例定位.游标 + 实例每轮步长;
    if (结束 > g_对象数) 结束 = g_对象数;

    for (int i = g_实例定位.游标; i < 结束; i++) {
        uint64_t 对象 = 取对象(i);
        if (!kfd.地址判断(对象)) continue;

        uint64_t 类 = kfd.取vreadbuf<uint64_t>(对象 + SDK::UObject::ClassPrivate);
        if (!kfd.地址判断(类)) continue;
        if (取对象名(类, g_名字池) != 目标类名) continue;

        // 类对上了 —— 剔掉 CDO, 只要真实例
        string 名 = 取对象名(对象, g_名字池);
        if (名.rfind("Default__", 0) == 0) continue;

        g_实例定位.结果 = 对象;
        return 对象;
    };

    g_实例定位.游标 = 结束;
    if (g_实例定位.游标 >= g_对象数) {
        g_实例定位.完成 = true;
        g_实例定位.失败时刻 = 现在秒();
    };
    return 0;
}

// ============================================================================
//  ★ 无畏契约改造: 暗区专属"回放链维护"整段禁用。
//    无畏没有 USGGameInstanceReplaySubsystem / KillCam 录像上报子系统。
// ============================================================================
#if 0
void 回放链维护() {
    static double 上次 = 0.0;
    double 此刻 = 现在秒();
    if (此刻 - 上次 < 1.0) return;   // 跟检测面维护同频
    上次 = 此刻;

    g_回放链路 = 0;

    // 子系统地址是缓存下来的 —— GameInstance 换(重进游戏/换模式)后指针会失效, 失效就重找
    if (g_回放子系统 && !kfd.地址判断(g_回放子系统)) {
        g_回放子系统 = 0;
        g_实例定位 = 实例定位任务();
    };
    if (!g_回放子系统) {
        g_回放子系统 = 找实例增量("SGGameInstanceReplaySubsystem");
        if (!g_回放子系统) return;   // 还在扫 / 这一遍没扫到 —— 这轮什么都不做
    };
    g_回放链路 |= 1;

    const uint64_t 缓存 = g_回放子系统 + SDK::USGGameInstanceReplaySubsystem::LocalFileKillCamInfo;

    // (1) 逐帧片段 —— 上传的核心数据
    int 清片段 = 清TArray长度(缓存 + SDK::FSGLocalFileKillCamInfo::KillCamFragments);
    if (清片段 > 0) { g_回放清片段 += 清片段; g_回放链路 |= 2; };

    // (2) 录制文件名 —— 上传时靠它定位本地那个文件
    //     (FString 布局跟 TArray 一致: Data@0 / Num@8 / Max@0xC, 同一个清法)
    int 清文件名 = 清TArray长度(缓存 + SDK::FSGLocalFileKillCamInfo::ReplayFileName);
    if (清文件名 > 0) { g_回放清文件名++; g_回放链路 |= 2; };

    // (3) 待下载列表(服务端发来待拉取的项)
    int 清队列 = 清TArray长度(g_回放子系统 + SDK::USGGameInstanceReplaySubsystem::HttpDownloadKillCamInfoList);
    if (清队列 > 0) { g_回放清队列 += 清队列; g_回放链路 |= 2; };
};
#endif  // 回放链维护

// ============================================================================
//  六、绘制线程主体
// ============================================================================
// 链路状态 → 一行诊断(断点放最前面, 一眼看到断在哪一层)
string 链路诊断串() {
    string 串 = "断点:" + g_链路.断点;
    if (!g_链路.说明.empty()) 串 += "(" + g_链路.说明 + ")";
    串 += " [内" + string(g_链路.内核 ? "通" : "×")
        + "箱" + string(g_链路.对象表 ? "通" : "×")
        + "名" + string(g_链路.名字池 ? "通" : "×")
        + "类" + string(g_链路.基类 ? "通" : "×")
        + "世" + string(g_链路.世界 ? "通" : "×")
        + "控" + string(g_链路.控 ? "通" : "×")
        + "相" + string(g_链路.相机 ? "通" : "×") + to_string(g_链路.相机路径)
        + "己" + string(g_链路.自己 ? "通" : "×") + "]";
    return 串;
};

// ============================================================================
//  ★ 修复版15 —— 相机"哪一格是当前帧的"由命中数投票判定
//
//  现象(实机截图): 屏幕写着"角色32 候选32", 但**画框 0** —— 32 个目标一个框都没有。
//  根因不在画框那几步, 在相机: 相机管理器上并排放着 4 格视角缓存, 只有一格是
//  "正在渲染的那一帧"。拿错格子的后果不是"稍微偏一点", 是**朝向整格差几十度**
//  (实机: 主/私两格差 95.84°) —— 我附近的人全跑到相机背后, 投影集体失败,
//  框还没开始画就被丢光了。上一版让你一格一格切 plist 试, 试错成本全在你身上。
//
//  这一版改成让代码自己数:
//      每格相机 × 每个目标坐标源 → 数"有多少目标落在屏幕里"。
//      哪一格数出来的最多, 哪一格就是当前帧的那格。
//  判据是"我附近真有这么多人", 跟朝向无关 —— 拿错朝向的格子必然数不出人,
//  所以这条判据不会自证失败, 也不依赖任何猜测的偏移。
//
//  同时: 坐标源(甲/乙/丙)也一起投票。甲=根相对位置(第一版口径)只在"没有父组件"
//  时才等于世界坐标, 只要它数不出人而乙/丙数得出, 就自动切过去并在屏幕上写明。
// ============================================================================
struct 相机候选格 {
    UE4算法::相机视角 视角;
    UE4算法::相机信息 相机;
    int 来源 = 0;      // 1=CameraCache 2=CameraCachePrivate 3=LastFrame 4=ViewTarget
    int 命中 = 0;      // 该格在最优坐标源下, 落在屏幕里的目标数
    int 命中源 = 0;    // 该格最合适的坐标源(0=甲 1=乙 2=丙)
    bool 有效 = false;
};

static 相机候选格 g_相机候选[4];
static int g_相机选中格   = 0;            // 1..4, 0=没选出来
static int g_相机自动切   = 0;            // 1=指定的那格数不出人, 自动切到别的格
static int g_坐标自动切   = 0;            // 1=指定的坐标源数不出人, 自动切到别的源
static int g_相机格分[4]  = {0, 0, 0, 0}; // 四格各自的命中数(诊断)
static int g_源命中[3]    = {0, 0, 0};    // 选中格下 甲/乙/丙 各自的命中数(诊断)
static int g_本帧命中     = 0;            // 选中格 + 选中源下的命中数(诊断)

// 取某个"目标世界坐标来源"的位置
inline UE4类型::FVector 取源坐标(const 玩家数据& 玩家, int 源) {
    if (源 == 2) return 玩家.源丙;
    if (源 == 1) return 玩家.源乙;
    return 玩家.源甲;
};

// 某相机 + 某坐标源下, 有多少目标落在屏幕可见区(放宽 50%, 边缘目标也算)
//   源 < 0 = 逐目标三源互证自选(修复版16 的默认口径)
int 屏幕内命中数(const vector<玩家数据>& 列表, const UE4算法::相机信息& 相机,
                int 源, float 宽, float 高) {
    if (!相机.有效) return 0;
    int 命中 = 0;
    size_t 数 = 列表.size();
    if (数 > (size_t)SDK::候选名单上限) 数 = (size_t)SDK::候选名单上限;
    for (size_t i = 0; i < 数; i++) {
        int 用了 = 0;
        UE4类型::FVector 位置 = (源 < 0) ? 选目标位置(列表[i], 用了) : 取源坐标(列表[i], 源);
        if (!UE4算法::是数(位置)) continue;
        if (位置.X == 0.0f && 位置.Y == 0.0f && 位置.Z == 0.0f) continue;
        ImVec2 屏幕;
        if (!UE4算法::投影点(位置, 相机, 宽, 高, 屏幕)) continue;
        if (屏幕.x < -宽 * 0.5f || 屏幕.x > 宽 * 1.5f) continue;
        if (屏幕.y < -高 * 0.5f || 屏幕.y > 高 * 1.5f) continue;
        命中++;
    };
    return 命中;
};

// ★ 相机视角持久缓存 —— 读取线程某一轮抖一下, 不该让整帧画不出来。
//   上一版这里拿不到相机就直接 return, 屏幕上的表现就是"计数 0/0、一个框都没有",
//   看着像插件死了, 其实只是数据链断了一环(相机地址本身是稳定量)。
static UE4算法::相机视角 g_上次视角;
static double g_上次视角时间 = 0.0;

void 主线程(ImDrawList* ImDrawList, int &玩家数量, int &人机数量, int &盒子数量) {
    g_帧状态 = "正常";

    // ★★ 配置必须在"取相机"之前读 ★★
    //   相机来源(相机源密钥) 和 FOV 基准边(FOV基准密钥) 是两个**可切换的口径开关**:
    //   改 plist 当场生效(不用重启、不用重进局), 一格一格试, 哪个口径让框贴住人就是哪个。
    //   为什么要开关: 这两处是"投影取值"里唯一还没被实机证死的两个点, 其它(画布/偏移/
    //   旋转矩阵/投影公式)都已经逐条对 dump 核过、也对得上屏幕上的数。
    NSDictionary *配置 = [NSDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Preferences/com.map.settings.plist"];

    // ★ 修复版18 的默认值:
    //   相机源 = 2 (CameraCachePrivate, 皮卡丘内核认的那一格)
    //   FOV基准 = 3 (皮卡丘那条焦距算式)
    //   两个键都还能切回去自己试, 但默认就是皮卡丘口径。
    NSNumber *相机源配置 = [配置 objectForKey:@"相机源密钥"];
    UE4算法::g_相机源 = (相机源配置 != nil) ? [相机源配置 intValue] : 2;
    if (UE4算法::g_相机源 < 0 || UE4算法::g_相机源 > 3) UE4算法::g_相机源 = 2;

    NSNumber *基准配置 = [配置 objectForKey:@"FOV基准密钥"];
    UE4算法::g_FOV基准 = (基准配置 != nil) ? [基准配置 intValue] : 3;
    if (UE4算法::g_FOV基准 < 0 || UE4算法::g_FOV基准 > 3) UE4算法::g_FOV基准 = 3;

    // 目标世界坐标来源(修复版16):
    //   0 = 自洽(默认) —— 甲/乙/丙 三源两两互证, 逐目标挑最可信的那个, 屏幕上写明用了哪个
    //   1 = 强制 甲: 根 + RelativeLocation(0x154)        第一版口径
    //   2 = 强制 乙: 根 + ComponentToWorld(0x120+0x10)   引擎刷新的绝对世界变换
    //   3 = 强制 丙: 网格 + ComponentToWorld(0x120+0x10) 角色网格世界位置
    //   强制档只在调试用(想手验某一源时切); 默认自洽 —— 实机实测过甲被挂父级后不是世界坐标,
    //   认死甲就会"框顺着视角漂", 所以默认交给互证。
    NSNumber *坐标源配置 = [配置 objectForKey:@"坐标源密钥"];
    g_坐标源 = (坐标源配置 != nil) ? [坐标源配置 intValue] : 0;
    if (g_坐标源 < 0 || g_坐标源 > 3) g_坐标源 = 0;

    uint64_t 相机地址 = 自身数据.获取相机();

    // ★ 修复版15: 目标列表提前取 —— 相机判定要拿它投票。
    //   这一步跟相机完全无关(纯粹从帧缓冲拷一份角色列表), 提到前面没有副作用。
    vector<玩家数据> 本地玩家;
    绘制类型.获取数据(本地玩家);

    UE4算法::相机视角 视角;
    if (kfd.地址判断(相机地址)) {
        UE4算法::算缓存差异(相机地址);   // 主/私两格的差角 —— 诊断用
    };

    BOOL 人机显示开关 = [[配置 objectForKey:@"人机密钥"] boolValue];
    // ★ r35: 玩家名字开关(默认关, 面板上自己开)
    BOOL 名字显示开关 = [[配置 objectForKey:@"名字密钥"] boolValue];
    BOOL 骨骼显示开关 = [[配置 objectForKey:@"骨骼密钥"] boolValue];
    BOOL 距离显示开关 = [[配置 objectForKey:@"距离密钥"] boolValue];
    BOOL 方框显示开关 = [[配置 objectForKey:@"方框密钥"] boolValue];
    BOOL 后坐力开关   = [[配置 objectForKey:@"后坐密钥"] boolValue];
    // ★ r26: 压枪系数(可选, 默认关)。plist "后坐系数密钥" = 0.20 ~ 0.90(倍率)。
    //   没设 / 设 0 → 完全不启用, 后坐力只走相机总闸那一刀(上报数据全是真值, 最安全)。
    //   设了才动 MaxVertical/MaxHorizontalRecoil 两个上限, 且带 ±10% 抖动。
    float 压枪系数 = 0.0f;
    NSNumber *压枪配置 = [配置 objectForKey:@"后坐系数密钥"];
    if (压枪配置 != nil) {
        float 值 = [压枪配置 floatValue];
        if (值 >= SDK::压枪系数::下限 && 值 <= SDK::压枪系数::上限) 压枪系数 = 值;
    };
    // ★ 修复版21: 头甲显示(人物头顶上方一行 "头:x 甲:y")
    BOOL 头甲显示开关 = [[配置 objectForKey:@"头甲密钥"] boolValue];
    // 射线(从屏幕顶端垂直落到框顶): plist 里没设这一项时**默认开** —— 这是你要的那条
    NSNumber *射线配置 = [配置 objectForKey:@"射线密钥"];
    BOOL 射线显示开关 = (射线配置 == nil) ? YES : [射线配置 boolValue];

    // ★ 修复版19 引入 / 修复版20 改宽: 在场过滤 —— 治"没人的空地上也冒框"
    //   r20 三档: 有玩家身份的硬豁免 / 多票隐藏(≥2条证据)+连续6轮才丢 / 单条命中只计数照画。
    //   目的: 空地上别冒框, 但**绝不丢活人**。plist 没设这一项时默认开。
    //   关掉它 = 回到旧行为(所有 SGCharacterBase 子类全画), 用来对比验证。
    // ★ r34: 这道门已**降级为只上报** —— 判定照算、诊断照显示, 但一个都不从名单里剔。
    //   实证依据见 子线程.h 的在场判定段头(皮卡丘那条链上压根没有判隐藏这一环, 它不漏人)。
    //   所以这个 plist 开关现在**不影响画不画人**了, 保留读取只为兼容老配置、不报错。
    NSNumber *在场配置 = [配置 objectForKey:@"在场密钥"];
    g_在场过滤开关 = (在场配置 == nil) ? false : ([在场配置 boolValue] ? true : false);

    // ★ r27: 队友过滤 —— 默认**关**。
    //   它拿的是 SGPlayerState::TeamIndex(0x648), 这个字段 dump 有、实机没验过;
    //   拿一个没验过的字段决定"哪些人不画", 代价就是漏人(读错 → 人凭空消失)。
    //   要开就在 plist 里加 "队友过滤密钥" = true, 引擎每帧读一次, 改完立刻生效。
    // ★ r38: 队友过滤 —— 默认**开**, 判据整套换掉(见 SDK.h 队伍判据链)。
    //   旧版默认关是因为 TeamIndex(0x648) 是阵营字段, 一开就可能清空全场;
    //   现在主判据改成 ASGTeamInfo **指针相等**, 指针比较不解释数值,
    //   不存在"读错值整队消失"这条路, 所以可以默认开。
    //   想关: plist 里 "队友过滤密钥" = false(引擎每帧读一次, 改完立刻生效)。
    NSNumber *队友配置 = [配置 objectForKey:@"队友过滤密钥"];
    g_队友过滤开关 = (队友配置 == nil) ? true : ([队友配置 boolValue] ? true : false);

    // 焦距微调: 千分比, 1000 = 不动
    float 焦距倍率 = 1.0f;
    NSNumber *焦距配置 = [配置 objectForKey:@"焦距密钥"];
    if (焦距配置 != nil) {
        float 值 = [焦距配置 floatValue];
        if (值 >= 300.0f && 值 <= 3000.0f) 焦距倍率 = 值 / 1000.0f;
    };
    float 身高 = 默认身高, 框宽比 = 默认框宽比;
    NSNumber *身高配置 = [配置 objectForKey:@"身高密钥"];
    NSNumber *框宽配置 = [配置 objectForKey:@"框宽密钥"];
    if (身高配置 != nil) { float 值 = [身高配置 floatValue]; if (值 >= 120.0f && 值 <= 260.0f) 身高 = 值; };
    if (框宽配置 != nil) { float 值 = [框宽配置 floatValue]; if (值 >= 150.0f && 值 <= 1200.0f) 框宽比 = 值 / 1000.0f; };

    // 外框宽度微调: 千分比, 1000 = 跟着模型走(默认)。只作用于包围盒出来的框。
    float 外框宽倍率 = 1.0f;
    NSNumber *外框宽配置 = [配置 objectForKey:@"外框宽密钥"];
    if (外框宽配置 != nil) {
        float 值 = [外框宽配置 floatValue];
        if (值 >= 400.0f && 值 <= 2500.0f) 外框宽倍率 = 值 / 1000.0f;
    };

    // ---- 相机(带 FOV 模式) ----
    float 宽 = 设备类型.设备信息.宽度;
    float 高 = 设备类型.设备信息.高度;
    bool 用缓存视角 = false;    // 诊断用: 本帧用的是不是"上一帧缓存"那格

    // ★ 修复版18 —— 相机不再投票、不再帧间换格, 照皮卡丘内核的写法"认死一格"。
    //   皮卡丘那颗内核(实机上跑得住的那套)读相机就一条链、三个读数, 从头到尾同一格:
    //       相机管理器 + 0x1F70 (CameraCachePrivate)
    //         + 0x10 → POV 起点
    //              POV + 0x00 = Location (FVector)
    //              POV + 0x0C = Rotation (FRotator)
    //              POV + 0x18 = FOV      (float)
    //   它没有"谁时间戳新用谁", 更没有"谁命中多用谁" —— 时基一格一跳, 整片框就跟着跳。
    //   这版也只认一格(plist "相机源密钥" 切 1/2/3), 另外三格只读出来上屏当诊断。
    {
        const uint64_t 四点[4] = {
            相机地址 + SDK::APlayerCameraManager::CameraCache,
            相机地址 + SDK::APlayerCameraManager::CameraCachePrivate,
            相机地址 + SDK::APlayerCameraManager::LastFrameCameraCache,
            相机地址 + SDK::APlayerCameraManager::ViewTarget + SDK::FCameraCacheEntry::POV,
        };
        // ★ r26 性能/防检测 —— 这四格里有三格**只是诊断读数**(不参与选格),
        //   而每格都要"建相机 + 遍历全部目标算屏幕内命中", 一帧算四次纯属白烧。
        //   这是绘制帧里最贵的一段, 烧掉的帧时间最后会变成客户端上报的"帧率抖动"。
        //   现在: 认死那一格每帧读(它是唯一正式读数), 诊断三格每 8 帧刷一次。
        static uint32_t 相机诊断帧   = 0;
        static bool     相机诊断已读 = false;
        相机诊断帧++;
        const bool 刷诊断格 = ((相机诊断帧 % 8) == 0) || !相机诊断已读;
        if (刷诊断格) 相机诊断已读 = true;

        for (int i = 0; i < 4; i++) {
            if (!刷诊断格) break;      // 非诊断帧: 整个诊断循环跳过, 保留上次读数
            g_相机格分[i] = -1;
            g_相机候选[i].有效 = false;
            if (!kfd.地址判断(相机地址)) continue;

            UE4算法::相机视角 候选 = UE4算法::读视角(四点[i], i + 1);
            g_相机候选[i].视角 = 候选;
            g_相机候选[i].来源 = i + 1;
            g_相机候选[i].有效 = 候选.有效;
            // 这一格能看到几个目标 —— 只当诊断读数, 不参与选格
            if (候选.有效) {
                UE4算法::相机信息 候选相机 = UE4算法::建相机(候选, 宽, 高, 焦距倍率);
                if (候选相机.有效) g_相机格分[i] = 屏幕内命中数(本地玩家, 候选相机, -1, 宽, 高);
            };
        };

        视角 = UE4算法::取视角(相机地址);       // 内部就按 "相机源密钥" 认死一格, 不跳
        g_相机选中格 = (视角.来源 >= 1 && 视角.来源 <= 4) ? 视角.来源 : 0;
        g_相机自动切 = 0;
        g_本帧命中 = (g_相机选中格 >= 1) ? g_相机格分[g_相机选中格 - 1] : 0;

        // 定死那格真读不出来 → 才按固定顺序退(私有 → 当前 → 上帧 → ViewTarget), 依旧不跳。
        // ★ r26: 退序改成**现场读那一格** —— 诊断三格现在是 8 帧才刷一次,
        //   拿 8 帧前的旧视角当兜底正好是"框会偏"的来源, 不能省这一次读。
        if (!视角.有效) {
            const int 退序[4] = { 2, 1, 3, 4 };
            for (int i = 0; i < 4; i++) {
                UE4算法::相机视角 现场 = UE4算法::读视角(四点[退序[i] - 1], 退序[i]);
                if (现场.有效) {
                    视角 = 现场;
                    g_相机选中格 = 退序[i];
                    g_本帧命中 = (g_相机格分[退序[i] - 1] >= 0) ? g_相机格分[退序[i] - 1] : 0;
                    break;
                };
            };
        };
        用缓存视角 = (视角.来源 == 3);
    };

    UE4算法::相机信息 相机 = UE4算法::建相机(视角, 宽, 高, 焦距倍率);
    if (!相机.有效) {
        // ★ r28: 相机读不出参数 —— 这多半是"换局了, 手里这个相机管理器是上一把的尸体"。
        //   光在这儿 return 什么也不会变(旧版就是这样, 于是屏幕上一直"相机没数据",
        //   关了软件重开也一样)。现在主动向读取线程喊一声"重认", 它下一轮就走
        //   PC链→扫表把新相机认回来; 3 秒内只喊一次, 不会变成扫表风暴。
        g_要重认相机 = true;
        g_帧状态 = "相机重认中";
        g_诊断行 = g_相机诊断 + " | 画布" + to_string((int)宽) + "x" + to_string((int)高)
                 + " | " + 链路诊断串();
        return;
    };

    // ---- 无后坐力: 每 2 帧按住一次(相机总闸只在"值不对"时才写) ----
    //   ★ r26: bEnableRecoil_Pitch / bEnableRecoil_Yaw / RepRecoilInfo 一律不碰,
    //     理由见 无后坐力每帧 上面的整段说明。
    static int 后坐帧 = 0;
    后坐帧++;
    if ((后坐帧 % 2) == 0) {
        无后坐力每帧(相机地址, 后坐力开关 ? true : false, g_后坐力组件, 压枪系数);
    };

    // ---- 检测面维护: 内部自带 1 秒节流（无畏契约无此子系统，已禁用） ----
    // ★ 无畏改造: 检测面维护已 #if 0 禁用，这里不再调用。
    // 检测面维护(自身数据.获取自己());

    // ---- 回放链维护 (r39): 内部自带 1 秒节流（无畏契约无此子系统，已禁用） ----
    // ★ 无畏改造: 回放链维护已 #if 0 禁用，这里不再调用。
    // 回放链维护();

    // (目标列表已在上面提前取过一份 —— 相机格投票要用它, 这里不重复取)

    size_t 候选数 = 本地玩家.size();
    // ★ r29: 上限不再写死 64 —— 名单现在是"活着+有身份+近的"排在前面(见读取线程尾部的排序),
    //   池化空壳和尸体沉在后面。上限 128 留两倍余量, 但也不无脑放大:
    //   绘制帧每个目标都要读一次骨骼, 帧时间不能乱花。
    if (候选数 > (size_t)SDK::候选名单上限) 候选数 = (size_t)SDK::候选名单上限;
    int 判死数 = 0, 画框数 = 0, 参考骨数 = 0, 头甲数 = 0, 名字数 = 0;
    int 主源活数 = 0, 血源兜底数 = 0, 未定数 = 0;   // 死亡取证的来源分布(诊断用)
    int 用甲数 = 0, 用乙数 = 0, 用丙数 = 0;         // 本帧选中的坐标源分布(诊断用)
    int 用骨位置数 = 0;                            // ★ r37: 位置由骨骼链给出的个数

    static 目标点位 全部框[SDK::候选名单上限];
    static 参考骨骼点位 全部参考骨骼[SDK::候选名单上限];
    bool 可用[SDK::候选名单上限] = {false};
    bool 参考可用[SDK::候选名单上限] = {false};

    for (size_t 序号 = 0; 序号 < 候选数; 序号++) {
        玩家数据& 玩家 = 本地玩家[序号];
        if (!kfd.地址判断(玩家.Actor)) continue;

        // ★★★ r35 主刀之一: 删掉"根组件不在就跳过" ★★★
        //   (照搬成品内核的丢人链 —— 它整条链的第一道门**只有"指针为空"**, 没有"根组件在不在"。)
        //
        //   根子在哪: 收集链 r30 已经把"根组件读不到就丢人"这一条放开了(新 spawn /
        //   对象池取出来的角色, 组件注册是分步完成的, 根组件指针会晚一两轮才就绪),
        //   人因此能进名单。**但绘制帧的第一句又把它续上了** —— 于是人进了名单、却在这里
        //   被跳过, 表现正好是"名单里有、画面上没框"。同一个漏口在两个地方各堵了一半,
        //   收集侧白放宽。
        //
        //   这一句删掉之后会不会拿垃圾坐标画框? 不会, 位置源是各自守的:
        //     甲/乙 需要根组件(根无效时收集线程已把两源写成 0 并标无效)
        //     丙  = 网格的 ComponentToWorld, 网格还在就仍然能出真实位置
        //     三个源全无效 → 选目标位置 返回的必然是 0 向量 → 下面那句"全 0 就跳过"挡住。
        //   也就是说: 能拿到真实坐标就画, 一个坐标源都没有才不画 —— 跟成品内核一个口径。
        // ★ r27: 这里原来还有一句 `if (!地址判断(玩家.Mesh)) continue;` —— 已删。
        //   方框只靠"根组件 + 位置", 网格在不在跟能不能画框没关系;
        //   网格那一瞬读不到就把整个人跳过 = 抓不住的漏人。
        //   需要网格的骨骼链在里面各自守(算参考骨骼/算骨骼点位第一句就是 Mesh 判空)。

        // ★ 修复版16: 位置不再"认死一个源", 改成三源两两互证, 逐目标挑(见 选目标位置)
        //   甲(相对位置)被证伪时自动走乙(引擎绝对世界变换) —— 这就是"框顺着视角漂"的根。
        int 用了源 = 0;
        UE4类型::FVector 角色位置;
        if (g_坐标源 == 0) {
            角色位置 = 选目标位置(玩家, 用了源);
        } else {
            // 用户在 plist 里显式指定了坐标源 → 尊重他的选择(调试用)
            用了源 = g_坐标源 - 1;
            角色位置 = 取源坐标(玩家, 用了源);
        };

        // ★★★ r37 主刀: 骨骼先算, 位置类功能全部改挂它 ★★★
        //   骨骼链(网格变换指针 × 组件空间点)是实测不偏的那一条; 上面投票出来的位置
        //   从这一版起只当**兜底** —— 骨骼这帧没数据时才轮到它上场(刚刷出来 / 网格没加载完)。
        //   于是"骨骼不偏、框偏"这种两条链打架的情况从根上没有了: 它们现在是同一个坐标。
        参考骨骼点位 参考骨骼;
        bool 有参考骨 = 算参考骨骼(玩家.Mesh, 角色位置, 相机, 参考骨骼);
        if (有参考骨) 全部参考骨骼[序号] = 参考骨骼;
        参考可用[序号] = 有参考骨;
        if (有参考骨) 参考骨数++;

        UE4类型::FVector 骨_质心 = { 0.0f, 0.0f, 0.0f };
        UE4类型::FVector 骨_头点 = { 0.0f, 0.0f, 0.0f };
        const bool 位置来自骨骼 = 有参考骨 && 取骨骼定位点(参考骨骼, 骨_质心, 骨_头点);
        if (位置来自骨骼) {
            角色位置 = 骨_质心;                    // 方框 / 距离 / 计数 → 统一用骨骼质心
            用骨位置数++;
        } else {
            if (用了源 == 0) 用甲数++; else if (用了源 == 1) 用乙数++; else 用丙数++;
        };

        // ★ r37: 位置合法性判断挪到**骨骼覆盖之后** ——
        //   原来它在最前面: 角色刚刷出来、根组件还没注册, 投票位置是 0 向量, 人就整条被
        //   丢掉了 —— 而这时候网格其实已经有数据、骨骼算得出位置。那是个白丢人的口子。
        if (!UE4算法::是数(角色位置)) continue;
        if (角色位置.X == 0.0f && 角色位置.Y == 0.0f && 角色位置.Z == 0.0f) continue;

        float 距离 = UE4算法::取距离(角色位置, 相机.位置);
        // ★★★ r35 主刀之二: 400m 距离门拿掉 —— 只在"距离本身不合法"时才跳过 ★★★
        //   照搬成品内核: 它那条链从"取到坐标"到"投到屏幕上"之间**没有任何距离门**,
        //   凡是投影能出来的一律画框。我们原来那句 `距离 <= 400` 等于给"看得见但离得远"
        //   的人判了死刑 —— 三百多米外的活人在倍镜里明明有人形, 框就是不出。纯丢框口。
        //
        //   两个口径要分开(上一版把它们绑在同一句 continue 上了):
        //     · 画框   → 不受距离限制(这一刀)
        //     · 顶部"附近 真人 X 人机 Y"的计数 → 仍是 400m 圈内(见下面 在计数圈内)
        const bool 在计数圈内 = (距离 >= 0.0f && 距离 <= 400.0f);
        if (!UE4算法::是数(距离) || 距离 < 0.0f) continue;
        玩家.距离 = 距离;

        // ---- 死亡过滤: 每帧现场读, 两源取证 ----
        //   主源 = 游戏自己的死亡组件 bIsDead(权威);
        //   主源拿不到才用血量兜底, 且兜底属性集只准说活(见 四、死亡判定 r27 段)。
        //   两源全断记"未定", 沿用上次结论, 没有就按活画 —— 绝不静默把人抹掉。
        int 死源标记 = 源全断;
        if (确认死亡(玩家.Actor, 玩家.死亡组件, 玩家.HealthSet, 玩家.血量集可信, 死源标记)) { 判死数++; continue; };
        if (死源标记 == 源主源) 主源活数++;
        else if (死源标记 == 源血量) 血源兜底数++;
        else 未定数++;

        // ★★★ 修复版12 的核心一处: 计数在这里定死 ★★★
        //  数到这一步, 已经过的全是**与视角无关**的检验:
        //    角色指针有效 / 网格有效 / 根组件有效 / 位置是个数 / 距离在我 400m 内 / 没死
        //  在这里就 +1 —— 后面不管是投影失败、目标在我背后、还是跑到屏幕外,
        //  都**不再影响计数**。这才是"以我为中心附近的总人数"。
        //  旧写法把 +1 放在第二个循环里(要求"这一帧真画出了框"), 于是:
        //    我一转身 → 背后的人投影不出来 → 当帧不计数 → 顶上数字跟着视角涨落。
        //  那不是我附近人变少了, 是计数口径错了。
        // ★ r35: 计数口径不变(400m 圈内), 只是不再受"画框"的影响 ——
        //   画框那一刀把距离门拿掉了, 计数必须自己把圈子守住, 不然顶上数字会跟着视距涨。
        if (在计数圈内) { if (玩家.bIsAI) 人机数量++; else 玩家数量++; };

        // 人机开关只管"画不画", 不再影响计数
        if (玩家.bIsAI && !人机显示开关) continue;

        目标点位 框点位;

        // ---- 方框: 照参考实现 —— 两个采样点各投一次, h 定框宽/框高 ----
        //   这里**没有任何门**: 框多小、多大、在不在屏幕里, 一律照画。
        bool 有框 = 算参考框(角色位置, 相机, 身高, 外框宽倍率, 框点位);

        // ---- 骨骼: 已在本帧开头算过(位置类功能共用同一批世界点) ----
        //   ★ r37: 再补一件事 —— 把骨骼最高的那个点投到屏幕上, 作为头甲/名字的落点。
        //     以前这两个是挂在"框顶 - 30 / - 52"上的, 框一偏它们就跟着偏;
        //     现在直接钉在真头顶上, 跟骨骼同一条链出坐标。
        if (位置来自骨骼) {
            ImVec2 头顶屏;
            if (UE4算法::投影点(骨_头点, 相机, 设备类型.设备信息.宽度, 设备类型.设备信息.高度, 头顶屏)) {
                框点位.头顶屏y = 头顶屏.y;
                框点位.头顶有效 = true;
            };
        };

        if (!有框) continue;

        全部框[序号] = 框点位;
        可用[序号] = true;
    };

    for (size_t 序号 = 0; 序号 < 候选数; 序号++) {
        if (!可用[序号]) continue;
        玩家数据& 玩家 = 本地玩家[序号];
        目标点位& 框 = 全部框[序号];

        // 计数已在取数阶段完成(与视角无关), 这里只管画
        const ImU32 主色 = 玩家.bIsAI ? 人机色 : 真人色;

        // ★★★ r36 —— 这一段照参考实现: **一道门都没有** ★★★
        //   参考实现里画框就一句"两个点投出来就连线", 没有"框太小不画"、没有"框太大不画"、
        //   没有"跑到屏幕外不画"、没有"框宽不够就换估算值"。上一版这里还留着三道:
        //     · 框高 <= 1px 直接 continue            → 远处的人/蹲着的人整个消失
        //     · 框高 > 屏幕 1.2 倍就夹取             → 贴脸的人框被改形状
        //     · 框宽 < 2px 就用框高比例顶替           → 拿估算值盖掉真实几何
        //   全删。留下的只有 NaN 保护(帧坐标是 NaN 会让 ImGui 画出乱线), 那不是判定。
        float 框高 = 框.框底 - 框.框顶;
        if (!UE4算法::是数(框高)) continue;

        float 中心x = 框.中心X;
        if (!UE4算法::是数(中心x)) continue;

        float 框宽值 = 框.框宽;
        if (!UE4算法::是数(框宽值)) continue;

        // 参考实现的线宽是两个写死的常量: 框 1.0 / 骨骼 1.6 (不随框高缩放)
        const float 线宽 = 1.0f;

        if (方框显示开关) {
            // 角框: 四条边各画两端 1/4, 中间留空 —— 与参考实现同一种画法。
            // 段长不再设下限(旧写法"太短就不画"又是一道丢框门), 小目标短就短着画。
            float 左 = 中心x - 框宽值 * 0.5f;
            float 右 = 中心x + 框宽值 * 0.5f;
            float 上 = 框.框顶;
            float 下 = 框.框底;
            float 横段 = 框宽值 * 0.25f;
            float 竖段 = 框高 * 0.25f;
            {
                ImDrawList->AddLine(ImVec2(左, 上), ImVec2(左 + 横段, 上), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(右 - 横段, 上), ImVec2(右, 上), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(左, 下), ImVec2(左 + 横段, 下), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(右 - 横段, 下), ImVec2(右, 下), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(左, 上), ImVec2(左, 上 + 竖段), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(右, 上), ImVec2(右, 上 + 竖段), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(左, 下), ImVec2(左, 下 - 竖段), 主色, 线宽);
                ImDrawList->AddLine(ImVec2(右, 下), ImVec2(右, 下 - 竖段), 主色, 线宽);
                画框数++;
            };
        };

        // ── 射线: 从屏幕顶端垂直落到目标框顶上面一点 ──────────
        //   (第一版原样: 从上面打下来, 不是从自己身上射出去)
        if (射线显示开关 && 框.框顶 > 4.0f) {
            ImDrawList->AddLine(ImVec2(中心x, 0.0f), ImVec2(中心x, 框.框顶 - 2.0f), 主色, 1.6f);
        };

        if (骨骼显示开关 && 参考可用[序号]) {
            // 参考实现的连法: 18 个写死下标, 按写死的 17 段顺序连。
            // 每段的门槛只有一条 —— **两端都投出来了就画这一根**。
            // 旧写法里那句"两点屏幕距离不到 1 像素就跳过"已删: 它会让局部骨骼段凭空消失,
            // 参考实现里没有这一条。
            const float 骨线宽 = 1.6f;      // 参考实现里写死的线宽
            参考骨骼点位& 参考骨 = 全部参考骨骼[序号];
            for (int s = 0; s < 17; s++) {
                int 甲 = 参考骨段[s][0];
                int 乙 = 参考骨段[s][1];
                if (!参考骨.有效[甲] || !参考骨.有效[乙]) continue;
                ImDrawList->AddLine(参考骨.点[甲], 参考骨.点[乙], 主色, 骨线宽);
            };
        };

        if (距离显示开关) {
            绘制文字(ImDrawList, "[ " + to_string((int)玩家.距离) + " m ]", 20.0f,
                    ImVec2(中心x, 框.框底 + 16.0f), 距离色, 0, true);
        };

        // ★ r37: 头甲/名字的纵向基准 —— 骨骼在就用真头顶(骨骼最高点的屏幕 y),
        //   骨骼这一帧没数据才退回框顶。两者都是"头顶上方", 但前者不跟框的跳动走。
        const float 头顶基准 = 框.头顶有效 ? 框.头顶屏y : 框.框顶;

        // ★★★ r35 新增: 真人玩家名字 —— 画在**头甲上面一行** ★★★
        //   只画真人(人机没有 PlayerState 名字, 也没必要画)。
        //   位置: 头顶基准 - 52 (头甲在 头顶基准 - 30, 名字再往上 22 像素, 两层不打架)。
        //   名字读不到(刚进局还没同步 / 名字池没换出来)就这一帧不画 —— 不占位、不留空行。
        if (名字显示开关 && !玩家.bIsAI) {
            string 玩家名 = 取玩家名(玩家.PlayerState);
            if (!玩家名.empty()) {
                绘制文字(ImDrawList, 玩家名, 20.0f,
                        ImVec2(中心x, 头顶基准 - 52.0f), 名字色, 0, true);
                名字数++;
            };
        };

        // ★ r21: 头甲 —— 头顶上方一行 "头:x 甲:y", 取不到就不画
        if (头甲显示开关) {
            头甲数据 头甲 = 取头甲(玩家.Actor);
            if (头甲.有效) {
                char 头甲文字[64];
                snprintf(头甲文字, sizeof(头甲文字), "头:%d 甲:%d", 头甲.头, 头甲.甲);
                绘制文字(ImDrawList, 头甲文字, 20.0f,
                        ImVec2(中心x, 头顶基准 - 30.0f), 主色, 0, true);
                头甲数++;
            };
        };
    };

    // ---- 左下角诊断(截图发我就能定位) ----
    static const char* 绘制版本 = "v40";
    static const char* 坐标源名[3] = {
        "根.RelativeLocation(旧口径)",
        "根.ComponentToWorld(引擎)",
        "网格.ComponentToWorld",
    };
    NSNumber *调试配置 = [配置 objectForKey:@"调试密钥"];
    bool 显示诊断 = (调试配置 == nil) ? true : [调试配置 boolValue];
    if (显示诊断) {
        char 缓冲[1024];
        // 这一行是"目标坐标"的判死证据:
        //   甲乙差 = 根相对位置 vs 根世界变换 差多少厘米
        //            恒等于 0 → 两者等价(根组件没被挂到别处), 用哪个都一样
        //            > 0     → 根组件被挂住了, 旧的相对位置≠世界坐标, 框就会偏
        //   甲丙差 = 根相对位置 vs 网格世界位置
        //            一个人身高以内(≈90~100)属正常(网格原点在角色身上);
        //            几百以上 → 网格原点不在角色位置, 靠"根位置±身高"画的框会有系统性偏差
        snprintf(缓冲, sizeof(缓冲),
                 "%s/%s 画布%.0fx%.0f | 相机格%d%s 命中%d [当前%d 私有%d 上帧%d 视图%d] | 焦距%.0fpx[%s] FOV%.1f 模式%d | 相机位(%.0f,%.0f,%.0f) 视角(%.1f,%.1f,%.1f)",
                 绘制版本,
                 g_读取线程版本,
                 (double)宽, (double)高,
                 g_相机选中格,
                 g_相机自动切 ? "(自动)" : (用缓存视角 ? "(缓存)" : "(指定)"),
                 g_本帧命中,
                 g_相机格分[0], g_相机格分[1], g_相机格分[2], g_相机格分[3],
                 (double)UE4算法::g_焦距像素,
                 UE4算法::g_焦距口径名,
                 (double)视角.POV.FOV,
                 相机.模式,
                 (double)相机.位置.X, (double)相机.位置.Y, (double)相机.位置.Z,
                 (double)视角.POV.Rotation.Pitch, (double)视角.POV.Rotation.Yaw, (double)视角.POV.Rotation.Roll);
        g_诊断行 = string(缓冲);

        snprintf(缓冲, sizeof(缓冲),
                 "坐标源%s(骨位%d 甲%d乙%d丙%d 差%.0f/%.0f) | 角色%d 候选%d 画框%d | 名单共%d 截%d 死档%d | 在场只报(绝不剔) 疑%d(隐%d销%d) 软%d 豁免%d 空壳%d 无控%d 全灭放行%d | 队友%s 剔己%d 滤友%d 据%d | 死源主%d血%d未%d 判死%d 表%d/%d | 骨%s | 后坐%s 压枪%.2f",
                 (g_坐标源 == 0) ? "自洽" : 坐标源名[g_坐标源 - 1],
                 用骨位置数, 用甲数, 用乙数, 用丙数, (double)g_坐标差甲乙, (double)g_坐标差甲丙,
                 g_链路.角色数, (int)候选数, 画框数,
                 g_名单总数, g_名单截断数, g_名单死档数,
                 g_在场丢隐藏 + g_在场丢销毁,
                 g_在场丢隐藏, g_在场丢销毁, g_在场软命中, g_在场豁免, g_在场丢空壳, g_在场无控制,
                 g_在场全灭放行,
                 g_队友过滤开关 ? "过滤开" : "过滤关",
                 g_自己剔掉数, g_队友滤掉数, g_队友判据数,
                 主源活数, 血源兜底数, 未定数, 判死数,
                 g_死源有效数, g_死源有效数 + g_死源无效数,
                 g_骨架诊断.c_str(),
                 g_后坐闸读回 ? "已关(OK)" : (后坐力开关 ? "没写上" : "关"),
                 (double)g_压枪系数实际);

        // ---- 第三行: 反检测面总览 ----
        //  上机怎么看这一行:
        //    · "队" 那栏是自身身份三件套的取数状态 —— PS√ 队√ 号<数> = 队伍判据是实的。
        //      三个全 × = 判据悬空, 那时候队友过滤会靠全灭保险兜住(宁可不滤, 绝不漏人)。
        //    · 反外挂面 4 个清零点: "链" 是链路是否打通(位图), "清" 是累计清掉几条。
        //      链通 + 清 一直是 0 = 这些数组本来就是空的(最理想, 说明没被记进来)。
        //      链断 = 组件链没走到(换局/读失败), 不是"越权没做成"。
        //    · 回放面 (r39): "链" 位1=子系统已定位(定位要分片扫一遍 GObjects, 进局后
        //      十几秒内才会亮), 位2=本轮清到东西。后面括号是累计: 片=回放片段条数,
        //      名=文件名次数, 队=待下载条数。
        //      ⚠️ 这一栏**清到东西才是好事**: 片/名 涨说明"刚有一次回放被切出来、
        //         又被清掉了" —— 也就是没交出去。一直 0 的分两种: 要么这一局没人举报你
        //         (压根没切过回放), 要么子系统还没定位到(看链那一栏有没有位1)。
        snprintf(缓冲, sizeof(缓冲),
                 "[r40] 参考骨%d 骨位%d 头甲%d 名%d 画框%d | 骨骼源: %s | 队: %s | 防检测 抖动清零%d 现值%d %s | 反外挂面 链%d 清%d(流%d 武%d 队%d) | 回放面 链%d 清%d(片%d 名%d 队%d)",
                 参考骨数, 用骨位置数, 头甲数, 名字数, 画框数, g_骨源诊断.c_str(),
                 g_队友诊断.c_str(),
                 g_抖动清零次数, g_抖动最后读数, (g_抖动链路通 ? "链通" : "链断"),
                 g_清点链路, g_清点本轮,
                 g_流水清条数, g_武器失败清条数, g_上报队列清条数,
                 g_回放链路, g_回放清片段 + g_回放清文件名 + g_回放清队列,
                 g_回放清片段, g_回放清文件名, g_回放清队列);
        g_诊断行 += "  ||  ";
        g_诊断行 += 缓冲;
        g_诊断行2 = string(缓冲);

        // 相机时基一行: 源1/2/3 = 当前/私有/上一帧; 时间戳是那格视角的生成时刻。
        // "主私差" = 两格视角差多少度 —— 静止时≈0, 一转身就有值, 这个值就是"框偏多少"。
        // "帧转"   = 本帧相对上帧相机转了多少度 —— 用来估"一帧的滞后值多少像素"。
        static UE4类型::FRotator 上帧角 = {0.0f, 0.0f, 0.0f};
        float 帧转 = 0.0f;
        if (视角.有效) {
            float 偏 = fabsf(视角.POV.Rotation.Yaw - 上帧角.Yaw);
            while (偏 > 180.0f) 偏 = fabsf(偏 - 360.0f);
            帧转 = fmaxf(偏, fmaxf(fabsf(视角.POV.Rotation.Pitch - 上帧角.Pitch),
                                   fabsf(视角.POV.Rotation.Roll - 上帧角.Roll)));
            上帧角 = 视角.POV.Rotation;
        };
        snprintf(缓冲, sizeof(缓冲),
                 "相机 FOV%.0f 模%d 基准%.0f 焦%.0f 源%d | 主私差%.2f°/%.2f° 帧转%.1f° | POV 偏航%.1f 俯仰%.1f 翻滚%.1f | ts 当前%.1f 私有%.1f 上帧%.1f",
                 相机.视场角, 相机.模式, 相机.基准边, 相机.焦距, 视角.来源,
                 UE4算法::g_缓存差角, UE4算法::g_缓存差焦, 帧转,
                 视角.POV.Rotation.Yaw, 视角.POV.Rotation.Pitch, 视角.POV.Rotation.Roll,
                 UE4算法::g_缓时间戳[0], UE4算法::g_缓时间戳[1], UE4算法::g_缓时间戳[2]);
        g_诊断行3 = g_相机诊断 + " | " + string(缓冲);
    } else {
        g_诊断行 = "";
        g_诊断行2 = "";
        g_诊断行3 = "";
    };
};
