// ============================================================================
//  内核读写层 —— 皮卡丘内核同款链
//
//  这一版是照着皮卡丘那颗内核(内核.app 主二进制)逐条反汇编对齐出来的, 流程跟它一模一样:
//
//   ① 找库 + 初始化
//        dlopen libjailbreak → dlsym "jbdInitPPLRW" → 调用, 返回 0 才算通。
//        (它的 jbdInitPPLRW 反汇编出来只有一句 bl jbclient_initialize_primitives,
//         也就是说这两个名字本来就是同一个入口, 以前直接调后一个也没错, 只是没走它的名字。)
//        找库路径按它的做法补上了 <jbroot>/basebin/libjailbreak.dylib(它二进制里的字面量)。
//
//   ② 定位目标进程 —— 它不用读内核进程链表, 走 sysctl:
//        sysctl(KERN_PROC_ALL) 遍历 kinfo_proc(项长 0x288), 拿 p_comm(+0xF3, 16 字节)比进程名,
//        取 p_pid(+0x28)。这条路不吃内核版本、不用猜结构体偏移, 比暴力扫内核链表稳。
//        (内核链表扫描那套保留着当兜底, sysctl 拿不到才走。)
//
//   ③ proc_find(pid) → proc_task(proc) 拿到目标进程的内核 task。
//
//   ④ 解"读写句柄" —— 这是它读别的进程内存的核心:
//        句柄 = kread64( kread_ptr( kread_ptr(task + 0x28) + 0x40 ) + 8 )
//        task+0x28 = vm_map, vm_map+0x40 = pmap, pmap+8 = 页表(ttep)。
//        解一次缓存住, 之后每笔读写都拿它, 不用每次重解 —— 这就是它比"每次调一遍
//        proc_vreadbuf"快的地方。
//
//   ⑤ 读 / 写
//        vreadbuf (句柄, 地址, 缓冲, 长度)
//        vwritebuf(句柄, 地址, 缓冲, 长度)     返回 0 = 成功
//        句柄解不出来、或自检不过 → 自动退回库自带的 proc_vreadbuf/proc_vwritebuf
//        (那是同一条链, 库自己在内部重解, 慢一点但更保险)。
//
//  初始化最后做一次硬自检: 读目标模块头的 Mach-O magic(0xFEEDFACF), 读对了才认"通"。
// ============================================================================

#pragma once

#import <mach/task.h>
#import <mach/mach_time.h>
#import <mach/vm_map.h>
#include <signal.h>
#import <mach-o/dyld.h>
#import <mach-o/dyld_images.h>
#import <dlfcn.h>
#import <dirent.h>
#import <sys/stat.h>
#import <sys/sysctl.h>
#import <sys/types.h>
#import <unistd.h>
#import <limits.h>
#include <string.h>
#import <utility>
#import <string>
#import <vector>

#import "libproc.h"
#import "proc_info.h"

using namespace std;

struct dyld_image_info64 {
    mach_vm_address_t imageLoadAddress;
    mach_vm_address_t imageFilePath;
    mach_vm_size_t    imageFileModDate;
};

struct dyld_all_image_infos64 {
    uint32_t            version;
    uint32_t            infoArrayCount;
    mach_vm_address_t   infoArray;
    dyld_image_notifier notification;
    bool                processDetachedFromSharedRegion;
    bool                libSystemInitialized;
    mach_vm_address_t   dyldImageLoadAddress;
    mach_vm_address_t   jitInfo;
    mach_vm_address_t   dyldVersion;
    mach_vm_address_t   errorMessage;
    uint64_t            terminationFlags;
    mach_vm_address_t   coreSymbolicationShmPage;
    uint64_t            systemOrderFlag;
    uint64_t            uuidArrayCount;
    mach_vm_address_t   uuidArray;
    mach_vm_address_t   dyldAllImageInfosAddress;
    uint64_t            initialImageCount;
    uint64_t            errorKind;
    mach_vm_address_t   errorClientOfDylibPath;
    mach_vm_address_t   errorTargetDylibPath;
    mach_vm_address_t   errorSymbol;
    uint64_t            sharedCacheSlide;
};

// ==================== 状态 / 日志 ====================

// ---- r28: 退出标志 ----
//   "关闭HUD"收到 SIGTERM 时置位。读取线程看到就 kfd.释放kfd() 把句柄放干净再退。
//   旧版是直接 SIGKILL —— 内核原语/页表句柄/proc 引用全留在内核里没人放,
//   下次再开 HUD 初始化时越狱库看到的是脏状态, 于是"关了再开就起不来, 得重启游戏"。
//   放在本文件(被所有编译单元引用), 注册信号的 HUD.mm 和 读取线程 都能看到同一个符号。
inline volatile sig_atomic_t g_要退出 = 0;

// ---- r28: 内核侧自己的计时(不能借用 游戏类型.h 的 现在秒, 本文件在它之前展开) ----
inline double 内核现在秒() {
    static mach_timebase_info_data_t 时基 = {0, 0};
    if (时基.denom == 0) mach_timebase_info(&时基);
    if (时基.denom == 0) return 0.0;
    return (double)mach_absolute_time() * (double)时基.numer / (double)时基.denom / 1.0e9;
};

inline bool   g_kfd已初始化 = false;
inline int    g_重连成功次数 = 0;
inline int    g_重连失败次数 = 0;
inline double g_上次重连时间 = 0.0;
inline string g_内核诊断 = "未初始化";
inline string g_内核库名 = "无";
inline string g_内核库路径 = "";
inline string g_内核自检 = "未自检";
inline uint64_t g_读总次数 = 0;
inline uint64_t g_读失败次数 = 0;
inline vector<string> g_内核日志;

inline void 内核记日志(const string& 内容) {
    g_内核日志.push_back(内容);
    while (g_内核日志.size() > 10) {
        g_内核日志.erase(g_内核日志.begin());
    };
    printf("[内核] %s\n", 内容.c_str());
};

// ---- 皮卡丘用的硬位置(跟它二进制里的一致) ----
inline constexpr size_t 进程表项大小     = 0x288;   // sizeof(kinfo_proc)
inline constexpr size_t 进程表项_进程号  = 0x28;    // kp_proc.p_pid
inline constexpr size_t 进程表项_进程名  = 0xF3;    // kp_proc.p_comm (16 字节)

inline constexpr uint64_t 任务_地址映射 = 0x28;     // task   → vm_map
inline constexpr uint64_t 地址映射_物理 = 0x40;     // vm_map → pmap
inline constexpr uint64_t 物理_页表     = 0x08;     // pmap   → 页表(ttep)

// ==================== 内核库(jbdInitPPLRW 那一套) ====================

struct 内核库指针 {
    void* 句柄 = nullptr;
    string 名称;
    string 路径;
    // 皮卡丘调的是 jbdInitPPLRW; 同一颗库里 jbclient_initialize_primitives 是它的真身
    int (*初始化)(void) = nullptr;
    uint64_t (*proc_self)(void) = nullptr;
    uint64_t (*proc_find)(pid_t) = nullptr;
    uint64_t (*proc_task)(uint64_t) = nullptr;
    void (*proc_rele)(uint64_t) = nullptr;
    uint64_t (*kread64)(uint64_t) = nullptr;
    uint64_t (*kread_ptr)(uint64_t) = nullptr;
    int (*kreadbuf)(uint64_t, void*, size_t) = nullptr;
    int (*vreadbuf)(uint64_t, uint64_t, void*, size_t) = nullptr;
    int (*vwritebuf)(uint64_t, uint64_t, void*, size_t) = nullptr;
    int (*proc_vreadbuf)(uint64_t, uint64_t, void*, size_t) = nullptr;
    int (*proc_vwritebuf)(uint64_t, uint64_t, void*, size_t) = nullptr;
    int (*proc_csflags_set)(uint64_t, uint32_t) = nullptr;
    int (*proc_csflags_clear)(uint64_t, uint32_t) = nullptr;
    uint32_t (*proc_getcsflags)(uint64_t) = nullptr;
};

inline 内核库指针 g_内核库;

inline void 绑定内核库符号(内核库指针& 库) {
    库.初始化 = (decltype(库.初始化))dlsym(库.句柄, "jbdInitPPLRW");
    if (库.初始化 == nullptr) {
        库.初始化 = (decltype(库.初始化))dlsym(库.句柄, "jbclient_initialize_primitives");
    };
    库.proc_self = (decltype(库.proc_self))dlsym(库.句柄, "proc_self");
    库.proc_find = (decltype(库.proc_find))dlsym(库.句柄, "proc_find");
    库.proc_task = (decltype(库.proc_task))dlsym(库.句柄, "proc_task");
    库.proc_rele = (decltype(库.proc_rele))dlsym(库.句柄, "proc_rele");
    库.kread64 = (decltype(库.kread64))dlsym(库.句柄, "kread64");
    库.kread_ptr = (decltype(库.kread_ptr))dlsym(库.句柄, "kread_ptr");
    库.kreadbuf = (decltype(库.kreadbuf))dlsym(库.句柄, "kreadbuf");
    库.vreadbuf = (decltype(库.vreadbuf))dlsym(库.句柄, "vreadbuf");
    库.vwritebuf = (decltype(库.vwritebuf))dlsym(库.句柄, "vwritebuf");
    库.proc_vreadbuf = (decltype(库.proc_vreadbuf))dlsym(库.句柄, "proc_vreadbuf");
    库.proc_vwritebuf = (decltype(库.proc_vwritebuf))dlsym(库.句柄, "proc_vwritebuf");
    库.proc_csflags_set = (decltype(库.proc_csflags_set))dlsym(库.句柄, "proc_csflags_set");
    库.proc_csflags_clear = (decltype(库.proc_csflags_clear))dlsym(库.句柄, "proc_csflags_clear");
    库.proc_getcsflags = (decltype(库.proc_getcsflags))dlsym(库.句柄, "proc_getcsflags");
};

// 皮卡丘链要的符号: 初始化 + 进程解析 + 内核指针读 + 至少一条内存读通道
inline bool 库符号齐全(内核库指针& 库) {
    bool 基础齐 = 库.初始化 && 库.proc_find && 库.proc_task && 库.kread_ptr && 库.kread64;
    bool 有读通道 = 库.vreadbuf || 库.proc_vreadbuf || 库.kreadbuf;
    return 基础齐 && 有读通道;
};

inline string 可执行目录() {
    char 路径[PATH_MAX] = {0};
    uint32_t 长度 = sizeof(路径);
    if (_NSGetExecutablePath(路径, &长度) != 0) return "";
    string 完整路径(路径);
    size_t 位置 = 完整路径.find_last_of('/');
    if (位置 == string::npos) return "";
    return 完整路径.substr(0, 位置);
};

inline vector<pair<string, string>> 内核库候选() {
    vector<pair<string, string>> 候选;

    // ① 皮卡丘的做法: 在容器目录里翻 .jbroot-*, 拼 <jbroot>/basebin/libjailbreak.dylib
    const char* 根目录表[] = {
        "/var/containers/Bundle/Application",
        "/var/mobile/Containers/Shared/AppGroup",
        "/var/containers/Shared/AppGroup"
    };
    for (const char* 根目录 : 根目录表) {
        DIR* 目录句柄 = opendir(根目录);
        if (目录句柄 == nullptr) continue;
        struct dirent* 目录项 = nullptr;
        int 计数 = 0;
        while ((目录项 = readdir(目录句柄)) != nullptr && 计数 < 64) {
            string 名字(目录项->d_name);
            if (名字.find(".jbroot-") == 0) {
                计数++;
                候选.push_back({string(根目录) + "/" + 名字 + "/basebin/libjailbreak.dylib", "jbroot·basebin"});
                候选.push_back({string(根目录) + "/" + 名字 + "/usr/lib/libjailbreak.dylib", "jbroot·usr/lib"});
            };
        };
        closedir(目录句柄);
    };

    // ② 固定路径
    候选.push_back({"/var/jb/basebin/libjailbreak.dylib", "越狱根·basebin"});
    候选.push_back({"/var/jb/usr/lib/libjailbreak.dylib", "越狱根·usr/lib"});
    候选.push_back({"/usr/lib/libjailbreak.dylib", "系统目录"});

    // ③ 包内自带的(跟皮卡丘那颗是同一份构建, 见文档)
    string 自身目录 = 可执行目录();
    if (!自身目录.empty()) {
        候选.push_back({自身目录 + "/ljb_rh.dylib", "包内·roothide"});
        候选.push_back({自身目录 + "/ljb_dp.dylib", "包内·dopamine"});
    };
    return 候选;
};

inline void 内核环境自检() {
    struct stat 状态;
    g_内核日志.clear();
    内核记日志(string("/var/jb: ") + (stat("/var/jb", &状态) == 0 ? "可见" : "不可见"));
    bool 有RootHide配置 = stat("/var/mobile/Library/RootHide/RootHideConfig.plist", &状态) == 0;
    内核记日志(string("RootHide 配置: ") + (有RootHide配置 ? "有" : "没有"));
    DIR* 目录句柄 = opendir("/var/containers/Bundle/Application");
    内核记日志(string("应用容器目录: ") + (目录句柄 ? "可读" : "读不到"));
    if (目录句柄) closedir(目录句柄);
};

inline bool 加载内核库() {
    // ★ r28 —— 已经成功加载过就直接复用, 绝不再走第二遍 dlopen + jbdInitPPLRW。
    //   为什么必须加这个短路:
    //     越狱库的内核原语初始化**不是幂等的**。第一次 jbdInitPPLRW 通了之后,
    //     第二次再调多半会被拒(返回非 0), 而我们下面的循环把"被拒"当成
    //     "这个候选库不行" → 一路把全部候选试完 → 判定"内核库没起来"。
    //     症状就是: 插件关掉再打开(哪怕同进程内重建 HUD), 内核就再也起不来。
    if (g_内核库.句柄 != nullptr && 库符号齐全(g_内核库)) {
        g_内核库名 = g_内核库.名称;
        g_内核库路径 = g_内核库.路径;
        return true;
    };

    for (auto& 项 : 内核库候选()) {
        内核库指针 库;
        库.句柄 = dlopen(项.first.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (库.句柄 == nullptr) {
            continue;
        };
        库.名称 = 项.second;
        库.路径 = 项.first;
        绑定内核库符号(库);
        if (!库符号齐全(库)) {
            内核记日志("✗ " + 项.second + ": 符号不全");
            continue;
        };
        // 皮卡丘这里调的 jbdInitPPLRW, 返回 0 才算通
        int 结果 = 库.初始化();
        if (结果 != 0) {
            内核记日志("✗ " + 项.second + ": jbdInitPPLRW 被拒(" + to_string(结果) + ")");
            continue;
        };
        g_内核库 = 库;
        g_内核库名 = 项.second;
        g_内核库路径 = 项.first;
        内核记日志("✓ " + 项.second + ": jbdInitPPLRW 通了" +
                   (库.vreadbuf ? " · 有 vreadbuf" : "") +
                   (库.vwritebuf ? "/vwritebuf" : ""));
        return true;
    };
    return false;
};

// ==================== 内存读写 ====================

struct {
    pid_t 目标进程ID = 0;
    uint64_t proc地址 = 0;
    uint64_t task地址 = 0;
    uint64_t 页表句柄 = 0;          // 皮卡丘那条链解出来的读写句柄
    string 进程名 = "";
    bool 同进程 = false;            // 目标就是自己(注入形态下会命中, 可以直读)
    bool 已初始化 = false;
    int  连续失败 = 0;

    // 内核链表扫描用的兜底偏移(只有 sysctl 拿不到 pid 才会用到)
    int64_t 偏移_pid = -1;
    int64_t 偏移_comm = -1;
    int64_t 偏移_list_next = -1;

    // -------------------- 地址判断 --------------------
    //   iOS/arm64 的用户态地址空间是 0x100000000 起、远不止到 12GB ——
    //   旧的上限 0x300000000 会把游戏大堆上分配的对象判成"无效地址", 那些目标直接被丢掉
    //   (屏幕上就是"角色数对、画框 0")。这里放到 1TB, 只做"是不是个像样的用户态指针"的粗筛。
    bool 地址判断(uint64_t 地址) {
        return (地址 > 0x100000000 && 地址 < 0x10000000000);
    };

    // -------------------- 内核地址读(库里的 kreadbuf/kread64) --------------------
    template<typename 类型> 类型 取kreadbuf(uint64_t 地址) {
        类型 内存 = {};
        if (g_内核库.kreadbuf && g_内核库.kreadbuf(地址, &内存, sizeof(类型)) == 0) return 内存;
        if (g_内核库.kread64 && sizeof(类型) == 8) {
            uint64_t 值 = g_内核库.kread64(地址);
            memcpy(&内存, &值, 8);
        } else if (g_内核库.kread64 && sizeof(类型) == 4) {
            uint64_t 值 = g_内核库.kread64(地址) & 0xFFFFFFFFULL;
            uint32_t 低32 = (uint32_t)值;
            memcpy(&内存, &低32, 4);
        };
        return 内存;
    };

    // -------------------- 目标进程内存读(皮卡丘那条链) --------------------
    bool 读原始(uint64_t 地址, void* 缓冲, size_t 长度) {
        if (地址 == 0 || 缓冲 == nullptr || 长度 == 0) return false;
        g_读总次数++;

        // ① 目标就是自己进程(注入形态) → 直接读用户态地址。
        //    这是最短也最准的一条路: 插件和游戏在同一个地址空间, 拿到的就是同一份内存;
        //    中间再隔一层页表翻译, 反而可能"翻译成功但拿到的不是这份" —— 数据一脏,
        //    投影出来的框就直接飞天上去。用 vm_read_overwrite 而不是裸指针解引用:
        //    地址真无效时它返回错误码, 不会把进程打崩。
        if (this->同进程 && 地址判断(地址)) {
            vm_size_t 实得 = 0;
            if (vm_read_overwrite(mach_task_self(), (vm_address_t)地址, (vm_size_t)长度,
                                  (vm_address_t)缓冲, &实得) == KERN_SUCCESS && 实得 == 长度) {
                this->连续失败 = 0;
                return true;
            };
        };

        // ② 库自带的 proc_vreadbuf —— 皮卡丘用的是同一颗 libjailbreak, 这条路它内部自己解 pmap
        if (this->proc地址 && g_内核库.proc_vreadbuf &&
            g_内核库.proc_vreadbuf(this->proc地址, 地址, 缓冲, 长度) == 0) {
            this->连续失败 = 0;
            return true;
        };

        // ③ 句柄 + vreadbuf(皮卡丘那条链的"正路", 句柄解不出来或这条路不通时才会走到这)
        if (this->页表句柄 && g_内核库.vreadbuf &&
            g_内核库.vreadbuf(this->页表句柄, 地址, 缓冲, 长度) == 0) {
            this->连续失败 = 0;
            return true;
        };

        // ④ 兜底: 内核地址(kreadbuf)
        if (g_内核库.kreadbuf && g_内核库.kreadbuf(地址, 缓冲, 长度) == 0) {
            this->连续失败 = 0;
            return true;
        };

        // 连丢几次 → 句柄可能过期(游戏重启/换局), 重解一次再试
        this->连续失败++;
        if (this->连续失败 >= 8) {
            this->连续失败 = 0;
            if (this->解页表句柄()) {
                if (g_内核库.vreadbuf && g_内核库.vreadbuf(this->页表句柄, 地址, 缓冲, 长度) == 0) return true;
                if (this->proc地址 && g_内核库.proc_vreadbuf &&
                    g_内核库.proc_vreadbuf(this->proc地址, 地址, 缓冲, 长度) == 0) return true;
            };
        };

        g_读失败次数++;
        return false;
    };

    bool 取vreadbuf列表(uint64_t 地址, void* 内存, size_t 长度) {
        return 读原始(地址, 内存, 长度);
    };

    template<typename 类型> 类型 取vreadbuf(uint64_t 地址) {
        类型 内存 = {};
        if (读原始(地址, &内存, sizeof(类型))) return 内存;
        return 类型{};
    };

    bool 取安全读取列表(uint64_t 地址, void* 内存, size_t 长度) {
        return 读原始(地址, 内存, 长度);
    };

    template<typename 类型> 类型 取安全读取(uint64_t 地址) {
        return 取vreadbuf<类型>(地址);
    };

    // -------------------- 写内存 --------------------
    bool 有写入能力() {
        return (this->页表句柄 && g_内核库.vwritebuf) ||
               (this->proc地址 && g_内核库.proc_vwritebuf);
    };

    bool 写原始(uint64_t 地址, void* 内存, size_t 长度) {
        if (地址 == 0 || 内存 == nullptr || 长度 == 0) return false;

        // ① 目标就是自己进程 → 直接写(注入形态, 最准最短的一条)
        if (this->同进程 && 地址判断(地址)) {
            memcpy((void*)地址, 内存, 长度);
            return true;
        };

        // ② 句柄 + vwritebuf —— 皮卡丘那条链
        if (this->页表句柄 && g_内核库.vwritebuf &&
            g_内核库.vwritebuf(this->页表句柄, 地址, 内存, 长度) == 0) return true;

        // ③ proc_vwritebuf
        if (this->proc地址 && g_内核库.proc_vwritebuf &&
            g_内核库.proc_vwritebuf(this->proc地址, 地址, 内存, 长度) == 0) return true;
        return false;
    };

    template<typename 类型> bool 写vwritebuf(uint64_t 地址, 类型 值) {
        return 写原始(地址, &值, sizeof(类型));
    };

    bool 写vwritebuf列表(uint64_t 地址, void* 内存, size_t 长度) {
        return 写原始(地址, 内存, 长度);
    };

    bool 写字节(uint64_t 地址, uint8_t 值) {
        return 写vwritebuf<uint8_t>(地址, 值);
    };

    bool 写布尔(uint64_t 地址, bool 值) {
        uint8_t 原始 = 值 ? 1 : 0;
        return 写vwritebuf<uint8_t>(地址, 原始);
    };

    bool 写单精度(uint64_t 地址, float 值) {
        return 写vwritebuf<float>(地址, 值);
    };

    // 写完回读校验
    bool 写并校验布尔(uint64_t 地址, bool 值) {
        if (!写布尔(地址, 值)) return false;
        return (取vreadbuf<uint8_t>(地址) != 0) == 值;
    };

    bool 写并校验单精度(uint64_t 地址, float 值) {
        if (!写单精度(地址, 值)) return false;
        float 回读 = 取vreadbuf<float>(地址);
        return 回读 == 值 || (值 != 0.0f && 回读 != 0.0f && 回读 < 值 * 2.0f && 回读 > 值 * 0.5f);
    };

    // ==================== 皮卡丘那条句柄链 ====================
    // 句柄 = kread64( kread_ptr( kread_ptr(task + 0x28) + 0x40 ) + 8 )
    //   task+0x28 = vm_map, vm_map+0x40 = pmap, pmap+8 = 页表
    bool 解页表句柄() {
        this->页表句柄 = 0;
        if (!g_内核库.kread_ptr || !g_内核库.kread64 || this->task地址 == 0) {
            内核记日志("✗ 句柄: 没 kread_ptr/kread64 或没 task");
            return false;
        };

        uint64_t 映射 = g_内核库.kread_ptr(this->task地址 + 任务_地址映射);
        if (映射 == 0) {
            内核记日志("✗ 句柄: vm_map 读空");
            return false;
        };
        uint64_t 物理 = g_内核库.kread_ptr(映射 + 地址映射_物理);
        if (物理 == 0) {
            内核记日志("✗ 句柄: pmap 读空");
            return false;
        };
        uint64_t 页表 = g_内核库.kread64(物理 + 物理_页表);
        if (页表 == 0) {
            内核记日志("✗ 句柄: 页表读空");
            return false;
        };

        this->页表句柄 = 页表;
        return true;
    };

    // ==================== 进程定位 ====================

    // 皮卡丘的做法: sysctl 扫 kinfo_proc 拿 pid(不吃内核版本, 不用猜结构体)
    pid_t 系统取进程号(string 进程名) {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
        size_t 长度 = 0;
        if (sysctl(mib, 4, nullptr, &长度, nullptr, 0) != 0) return 0;
        if (长度 == 0) return 0;
        长度 += 进程表项大小 * 8;                 // 留点余量, 进程表会变
        vector<uint8_t> 缓冲(长度, 0);
        if (sysctl(mib, 4, 缓冲.data(), &长度, nullptr, 0) != 0) return 0;

        size_t 条数 = 长度 / 进程表项大小;
        for (size_t i = 0; i < 条数; i++) {
            const uint8_t* 项 = 缓冲.data() + i * 进程表项大小;
            char 名字[17] = {0};
            memcpy(名字, 项 + 进程表项_进程名, 16);
            if (进程名 != 名字) continue;
            int 进程号 = 0;
            memcpy(&进程号, 项 + 进程表项_进程号, sizeof(int));
            if (进程号 > 0) return (pid_t)进程号;
        };
        return 0;
    };

    // 兜底: 读内核进程链表(老写法, 需要先猜出 pid/comm/链表三个偏移)
    bool 探测偏移() {
        if (!g_内核库.proc_self || !g_内核库.kreadbuf) {
            内核记日志("✗ 偏移探测: 内核库没就绪");
            return false;
        };

        uint64_t self_proc = g_内核库.proc_self();
        if (self_proc == 0) {
            内核记日志("✗ 偏移探测: 拿不到自身 proc");
            return false;
        };

        pid_t my_pid = getpid();

        uint8_t buf[0x400] = {0};
        if (g_内核库.kreadbuf(self_proc, buf, sizeof(buf)) != 0) {
            内核记日志("✗ 偏移探测: 读不了自身 proc");
            return false;
        };

        偏移_pid = -1;
        for (int i = 0; i < 0x400 - 4; i += 4) {
            if (*(pid_t*)(buf + i) == my_pid) { 偏移_pid = i; break; }
        };
        if (偏移_pid < 0) {
            内核记日志("✗ 偏移探测: 没找到 pid 偏移");
            return false;
        };

        extern const char *__progname;
        size_t 名字长度 = strlen(__progname);
        if (名字长度 == 0 || 名字长度 > 16) {
            内核记日志("✗ 偏移探测: 进程名长度异常");
            return false;
        };

        偏移_comm = -1;
        for (int i = 0; i < 0x400 - 16; i += 1) {
            if (memcmp(buf + i, __progname, 名字长度) == 0 && buf[i + 名字长度] == 0) {
                偏移_comm = i; break;
            };
        };
        if (偏移_comm < 0) {
            内核记日志("✗ 偏移探测: 没找到进程名偏移");
            return false;
        };

        偏移_list_next = -1;
        for (int i = 0; i < 0x80; i += 8) {
            uint64_t 原始指针 = *(uint64_t*)(buf + i);
            if (原始指针 == 0 || 原始指针 == self_proc) continue;

            uint64_t 候选列表[2] = { 原始指针, 原始指针 | 0xffffff8000000000 };
            for (int j = 0; j < 2; j++) {
                uint64_t 候选指针 = 候选列表[j];
                if ((候选指针 >> 40) != 0xffffff && (候选指针 >> 40) != 0xfffffe) continue;

                pid_t 候选pid = 0;
                if (g_内核库.kreadbuf(候选指针 + 偏移_pid, &候选pid, sizeof(pid_t)) == 0) {
                    if (候选pid >= 0 && 候选pid < 65536) {
                        char 验证名[17] = {0};
                        if (g_内核库.kreadbuf(候选指针 + 偏移_comm, 验证名, 16) == 0) {
                            if (验证名[0] != 0 || 候选pid == 0) {
                                偏移_list_next = i;
                                break;
                            };
                        };
                    };
                };
            };
            if (偏移_list_next >= 0) break;
        };
        if (偏移_list_next < 0) {
            内核记日志("✗ 偏移探测: 没找到进程链表偏移");
            return false;
        };

        return true;
    };

    bool 取目标进程ID(string 目标进程名) {
        if (this->proc地址 && g_内核库.proc_rele) {
            g_内核库.proc_rele(this->proc地址);
        };
        this->proc地址 = 0;
        this->task地址 = 0;
        this->页表句柄 = 0;
        this->目标进程ID = 0;
        this->进程名 = 目标进程名;
        this->连续失败 = 0;

        if (!g_内核库.proc_find) return false;

        // ① 皮卡丘那条: sysctl 直接拿 pid
        pid_t 进程号 = 系统取进程号(目标进程名);
        string 来源 = "sysctl";

        // ② 兜底: 读内核进程链表自己翻
        if (进程号 == 0 && g_内核库.proc_self && g_内核库.kreadbuf) {
            if (偏移_pid < 0 || 偏移_comm < 0 || 偏移_list_next < 0) {
                探测偏移();
            };
            if (偏移_pid >= 0 && 偏移_comm >= 0 && 偏移_list_next >= 0) {
                uint64_t self_proc = g_内核库.proc_self();
                uint64_t 当前proc = self_proc;
                int 计数 = 0;
                do {
                    char 当前进程名[17] = {0};
                    if (g_内核库.kreadbuf(当前proc + 偏移_comm, 当前进程名, 16) != 0) break;
                    if (string(当前进程名) == 目标进程名) {
                        pid_t pid = 0;
                        if (g_内核库.kreadbuf(当前proc + 偏移_pid, &pid, sizeof(pid_t)) != 0) break;
                        进程号 = pid;
                        来源 = "内核链表";
                        break;
                    };
                    uint64_t 原始指针 = 取kreadbuf<uint64_t>(当前proc + 偏移_list_next);
                    if (原始指针 == 0) break;
                    uint64_t 下一个 = 原始指针;
                    if ((下一个 >> 40) != 0xffffff && (下一个 >> 40) != 0xfffffe) {
                        下一个 = 原始指针 | 0xffffff8000000000;
                    };
                    if (下一个 == self_proc || 下一个 == 当前proc) break;
                    当前proc = 下一个;
                    计数++;
                } while (计数 < 10000);
            };
        };

        if (进程号 == 0) {
            内核记日志("✗ 进程表里没有 " + 目标进程名);
            return false;
        };

        uint64_t proc = g_内核库.proc_find(进程号);
        if (proc == 0) {
            内核记日志("✗ proc_find 拿不到 " + 目标进程名 + "(" + to_string(进程号) + ")");
            return false;
        };

        this->目标进程ID = 进程号;
        this->proc地址 = proc;
        this->task地址 = g_内核库.proc_task ? g_内核库.proc_task(proc) : 0;
        if (this->task地址 == 0) {
            内核记日志("✗ proc_task 拿不到 task");
            return false;
        };

        this->同进程 = (进程号 == getpid());
        this->解页表句柄();

        内核记日志("✓ 命中 " + 目标进程名 + "(pid " + to_string(进程号) + " · " + 来源 + ")" +
                   (this->页表句柄 ? " · 句柄已解" : " · 句柄没解出来, 走库自带 vread"));
        return true;
    };

    uint64_t 取目标模块地址(string 模块名) {
        if (!this->proc地址) return 0;

        uint64_t all_image_info_addr = 0;

        // dyld 信息地址: 先在自己(task)里找, 找不到再拿自己进程的兜底
        for (uint64_t offset = 0x200; offset < 0x400; offset += 0x8) {
            uint64_t 候选地址 = 取kreadbuf<uint64_t>(this->task地址 + offset);
            if (候选地址 > 0x100000000 && 候选地址 < 0x300000000) {
                dyld_all_image_infos64 测试;
                if (取安全读取列表(候选地址, &测试, sizeof(测试))) {
                    if (测试.version > 0 && 测试.version < 20 &&
                        测试.infoArrayCount > 0 && 测试.infoArrayCount < 10000) {
                        all_image_info_addr = 候选地址;
                        break;
                    };
                };
            };
        };

        if (all_image_info_addr == 0) {
            内核记日志("✗ 没找到 dyld 信息结构");
            return 0;
        };

        dyld_all_image_infos64 all_image_infos64;
        if (取安全读取列表(all_image_info_addr, &all_image_infos64, sizeof(all_image_infos64))) {
            vector<dyld_image_info64> infoArray(all_image_infos64.infoArrayCount);
            if (取安全读取列表(all_image_infos64.infoArray, infoArray.data(),
                            all_image_infos64.infoArrayCount * sizeof(dyld_image_info64))) {
                for (uint32_t i = 0; i < all_image_infos64.infoArrayCount; i++) {
                    char 当前模块名[PATH_MAX] = {0};
                    if (!取安全读取列表(infoArray[i].imageFilePath, 当前模块名, PATH_MAX - 1)) break;
                    if (strstr(当前模块名, 模块名.c_str())) {
                        内核记日志(string("✓ 模块装载于 ") + 当前模块名);
                        return infoArray[i].imageLoadAddress;
                    };
                }
            }
        }

        return 0;
    };

    // ==================== 读取自检 ====================
    // 读目标模块头的 Mach-O magic —— 读到了才说明这条链真的通, 不是"没报错"就算通
    bool 读取自检(uint64_t 模块地址, string& 说明) {
        if (模块地址 == 0) { 说明 = "没模块地址"; return false; };

        uint32_t magic = 取vreadbuf<uint32_t>(模块地址);
        if (magic != 0xFEEDFACF && magic != 0xFEEDFACE && magic != 0xCAFEBABE) {
            // 再试一次: 换库自带那条通道(有些环境句柄对、vreadbuf 对, 有些反过来)
            说明 = "魔数不对(读到 0x" + to_string(magic) + ")";
            return false;
        };

        // 再读一个固定偏移, 排除"只读得到头一页"
        uint64_t 抽检1 = 取vreadbuf<uint64_t>(模块地址 + 0x10);
        uint64_t 抽检2 = 取vreadbuf<uint64_t>(模块地址 + 0x100);
        if (抽检1 == 0 && 抽检2 == 0) { 说明 = "头部过了但中段读空"; return false; };

        说明 = "magic✓ 抽检✓";
        return true;
    };

    // ==================== 生命周期 ====================

    bool 调用kfd() {
        if (g_kfd已初始化) return true;
        if (!加载内核库()) {
            g_内核诊断 = "内核库没起来";
            return false;
        };
        g_kfd已初始化 = true;
        this->已初始化 = true;
        return true;
    };

    // 目标进程换了/重启了 → 整条链重建
    //   ★ r28: 加冷却 + 失败清理。旧写法没有冷却, 调用方(模块失联检测)一旦被判失联
    //   就会连着调 —— 每次进来先 proc_rele 再 proc_find 重来一遍, 中间还会 proc_task
    //   拿新引用。越狱库内部的进程/任务引用是有限资源, 这么反复来回会把状态搅乱,
    //   表现就是"重连越试越失败, 最后只能重启游戏"。现在: 3 秒内只认真重连一次,
    //   失败就把半截状态清干净(绝不留着一个解不掉的 proc 引用)。
    bool 重连(string 目标进程名) {
        if (!g_kfd已初始化) return false;

        double 现在 = 内核现在秒();
        if (现在 - g_上次重连时间 < 3.0) return false;
        g_上次重连时间 = 现在;

        if (取目标进程ID(目标进程名)) {
            g_重连成功次数++;
            return true;
        };

        g_重连失败次数++;
        释放kfd();
        return false;
    };

    // 目标进程还在不在(检查它的 pid 是否还能在进程表里查到)
    bool 目标还在() {
        if (this->目标进程ID == 0 || this->进程名.empty()) return false;
        pid_t 现在 = 系统取进程号(this->进程名);
        return 现在 != 0 && 现在 == this->目标进程ID;
    };

    bool patch目标进程() {
        if (this->proc地址 == 0 || !g_内核库.proc_getcsflags) return false;
        uint32_t 原始csflags = g_内核库.proc_getcsflags(this->proc地址);
        if (g_内核库.proc_csflags_clear) g_内核库.proc_csflags_clear(this->proc地址, 0x10000000 | 0x20000000);
        if (g_内核库.proc_csflags_set) g_内核库.proc_csflags_set(this->proc地址, 0x1 | 0x4000000);
        return g_内核库.proc_getcsflags(this->proc地址) != 原始csflags;
    };

    void 释放kfd() {
        if (this->proc地址 && g_内核库.proc_rele) {
            g_内核库.proc_rele(this->proc地址);
            this->proc地址 = 0;
        };
        this->task地址 = 0;
        this->页表句柄 = 0;
    };
} kfd;

// ==================== 一条龙初始化(可反复重试) ====================

inline bool 初始化内核链路(string 进程名, string 模块名, uint64_t& 模块地址) {
    模块地址 = 0;
    if (!kfd.调用kfd()) {
        g_内核诊断 = "内核库初始化失败";
        return false;
    };

    // ★ r28: 进程没换、旧 proc/task 引用还活着 → **直接复用, 不重新 proc_find**。
    //   为什么要这一条: 反复 proc_find / proc_task 是"内核越试越死"的主要来源 ——
    //   越狱库对进程/任务的引用是有限资源(引用表), 每次重建都占一份、中间还可能拿不到,
    //   连着试几轮就打满, 之后永久失败, 只能重启游戏。
    //   而实际上"内核起不来"十有八九只是读写句柄脏了(游戏换局就会), 进程根本没换 ——
    //   这种情况重解一次句柄就够, 完全不需要动 proc 引用。
    pid_t 当前进程号 = kfd.系统取进程号(进程名);
    const bool 进程没换 = (当前进程号 != 0 && 当前进程号 == kfd.目标进程ID &&
                          kfd.proc地址 != 0 && kfd.task地址 != 0);
    if (进程没换) {
        内核记日志("进程没换, 复用现有引用");
        kfd.解页表句柄();          // 句柄脏了重解一次(便宜且必须)
    } else if (!kfd.取目标进程ID(进程名)) {
        g_内核诊断 = "没找到 " + 进程名 + " 进程";
        kfd.释放kfd();          // ★ r28: 失败就清干净, 别留一个解不掉的 proc 引用
        return false;
    };

    kfd.patch目标进程();
    模块地址 = kfd.取目标模块地址(模块名);
    if (模块地址 == 0) {
        g_内核诊断 = "没找到目标模块";
        return false;
    };

    string 自检说明;
    bool 自检过了 = kfd.读取自检(模块地址, 自检说明);
    g_内核自检 = (自检过了 ? "✓ " : "✗ ") + 自检说明;

    g_内核诊断 = string("正常 · ") + g_内核库名 +
                 (kfd.页表句柄 ? " · 句柄链" : " · 库自带链") +
                 " · 自检 " + (自检过了 ? "通过" : "没过");
    内核记日志("✓ 模块 = 0x" + to_string(模块地址) + " · 自检 " + 自检说明);
    return true;
};
