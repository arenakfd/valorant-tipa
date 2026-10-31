namespace UE4类型 {

struct TArray {
    uint64_t AllocatorInstance;
    int ArrayNum;
    int ArrayMax;
};

struct FVector {
    float X;
    float Y;
    float Z;
};

struct FRotator {
    float Pitch;
    float Yaw;
    float Roll;
};

struct FMinimalViewInfo {
    FVector Location;
    FRotator Rotation;
    float FOV;
};

struct FPlane : FVector {
    float W;
};

struct FMatrix {
    FPlane XPlane;
    FPlane YPlane;
    FPlane ZPlane;
    FPlane WPlane;
};

struct FVector2D {
    float X;
    float Y;
    
    FVector2D(float X = 0.0f, float Y = 0.0f) {
        this -> X = X;
        this -> Y = Y;
    };
};

struct FQuat {
    float X;
    float Y;
    float Z;
    float W;
};

struct FTransform {
    FQuat Rotation;
    FVector Translation;
    FVector Scale3D;
};

};

#import "暗区突围/国际服/游戏类型.h"
#import <pthread.h>
#import <atomic>

struct 绘制帧 {
    vector<物品数据> 物品;
    vector<盒子数据> 盒子;
    vector<玩家数据> 玩家;
    vector<类名数据> 类名;
};

struct {
    string 进程名 = "UAGame";
    string 模块名 = "UAGame.app/UAGame";
    uint64_t 模块地址;
    bool 内核;
    
    绘制帧 缓冲区[3];
    atomic<int> 读取索引{0};
    atomic<int> 最新索引{0};
    int 写入索引 = 1;
    
    void 更新数据(vector<物品数据>& 物品, vector<盒子数据>& 盒子, vector<玩家数据>& 玩家, vector<类名数据>& 类名) {
        缓冲区[写入索引].物品.swap(物品);
        缓冲区[写入索引].盒子.swap(盒子);
        缓冲区[写入索引].玩家.swap(玩家);
        缓冲区[写入索引].类名.swap(类名);
        
        int 旧最新 = 最新索引.exchange(写入索引);
        int 当前读取 = 读取索引.load();
        
        if (旧最新 != 当前读取) {
            写入索引 = 旧最新;
        } else {
            for (int i = 0; i < 3; i++) {
                if (i != 当前读取 && i != 最新索引.load()) {
                    写入索引 = i;
                    break;
                }
            }
        }
    }
    
    绘制帧& 获取最新帧() {
        读取索引.store(最新索引.load());
        return 缓冲区[读取索引.load()];
    }
    
    void 获取数据(vector<物品数据>& 物品, vector<盒子数据>& 盒子, vector<玩家数据>& 玩家, vector<类名数据>& 类名) {
        绘制帧& 帧 = 获取最新帧();
        物品 = 帧.物品;
        盒子 = 帧.盒子;
        玩家 = 帧.玩家;
        类名 = 帧.类名;
    }
    
    // 只要玩家列表的版本(绘制线程用)
    void 获取数据(vector<玩家数据>& 玩家) {
        绘制帧& 帧 = 获取最新帧();
        玩家 = 帧.玩家;
    }
} 绘制类型;
