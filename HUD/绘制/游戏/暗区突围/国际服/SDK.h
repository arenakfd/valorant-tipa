// ============================================================================
//  无畏契约手游 (VALORANT Mobile / com.tencent.tmgp.codev) v1.25.0
//  全部偏移取自 MobileDumper-7 dump 的 CppSDK 真实类定义，逐条核对。
//
//  静态全局换算:  dump 里给的是 "0x100000000 + 相对主镜像(__TEXT)基址的偏移"
//     相对偏移 = dump值 - 0x100000000
//     运行时地址 = 绘制类型.模块地址 + 相对偏移
//
//  ★ 命名空间/字段名保持与暗区源码一致，只替换偏移值 —— 调用方不动。
// ============================================================================

#pragma once

#import "kfd.h"
#import "设备类型.h"
#import "绘制类型.h"
#import "游戏类型.h"

namespace SDK {

// ======================= 引擎静态全局 =======================
// dump: GWorld 0x00000000 (无全局 GWorld 符号，走 GEngine) | GNames 0x109EB4C10
//       GObjects 0x10A0B2488 | GEngine 0x10A1549F8
constexpr uint64_t GWorld   = 0x00000000;   // 无畏契约无全局 GWorld，用 GEngine 遍历
constexpr uint64_t GNames   = 0x09EB4C10;   // FNamePool（已按 Valorant profile 解密）
constexpr uint64_t GObjects = 0x0A0B2488;   // FChunkedUObjectArray
constexpr uint64_t GEngine  = 0x0A1549F8;   // UEngine*

// ======================= 对象数组 (FChunkedUObjectArray) =======================
namespace FUObjectArray {
    constexpr uint64_t Objects     = 0x10;  // FUObjectItem**
    constexpr uint64_t MaxElements = 0x20;  // int32
    constexpr uint64_t NumElements = 0x24;  // int32  <- 实际对象数
    constexpr uint64_t MaxChunks   = 0x28;  // int32
    constexpr uint64_t NumChunks   = 0x2C;  // int32
    // 遍历参数（UE4.27 标准，与暗区一致）
    constexpr uint64_t ChunkSize   = 0x10000;  // 每块对象数 65536
    constexpr uint64_t ItemStride  = 0x18;     // FUObjectItem 大小 24 字节
    constexpr uint64_t 备用Objects     = 0x00;  // 历史布局兜底（0x00 偏移，基本废弃）
    constexpr uint64_t 备用NumElements = 0x14;  // 历史布局兜底（旧版对象数偏移）
}

// ======================= UObject 基础 =======================
namespace UObject {
    constexpr uint64_t ClassPrivate = 0x10;  // UClass*（标准，dump 不变）
    constexpr uint64_t NamePrivate  = 0x18;  // FName（标准，dump 不变）
    constexpr uint64_t OuterPrivate = 0x20;  // UObject*
}

// ======================= 世界遍历（无畏契约走 GEngine，GWorld 无符号） =======================
// GEngine(0x0A1549F8) → +0x07D8 GameViewport → +0x70 World → +0x30 PersistentLevel → +0x98 Actors[]
// 依据：Engine_classes.hpp 54437 行 UEngine::GameViewport = 0x07D8（实例指针）
//       44891 行 UGameViewportClient::World = 0x70
//       注意：0x110 是 GameViewportClientClass（TSubclassOf 类对象），不是实例指针，别搞混
namespace UEngine {
    constexpr uint64_t GameViewport       = 0x07D8;  // UGameViewportClient*（Engine_classes.hpp:54437）
    constexpr uint64_t OwningGameInstance = 0x218;  // UGameInstance*
}
namespace UGameViewportClient {
    constexpr uint64_t World        = 0x70;  // UWorld*
    constexpr uint64_t GameInstance = 0x78;  // UGameInstance*
}
namespace UWorld {
    constexpr uint64_t PersistentLevel     = 0x30;   // ULevel*
    constexpr uint64_t OwningGameInstance  = 0x220;  // UGameInstance*
    constexpr uint64_t GameState           = 0x2E8;  // AGameStateBase*
}
namespace ULevel {
    constexpr uint64_t Actors = 0x98;  // TArray<AActor*>  ← 遍历所有 Actor
}
namespace UGameInstance {
    constexpr uint64_t LocalPlayers = 0x38;  // TArray<ULocalPlayer*>
}
namespace UPlayer {
    constexpr uint64_t PlayerController = 0x30;  // APlayerController*
}

// ======================= AActor / APawn / ACharacter =======================
namespace AActor {
    constexpr uint64_t RootComponent = 0x198;  // USceneComponent*（无畏 0x198，暗区是 0x158）
    constexpr uint64_t Owner         = 0x140;  // AActor*
    constexpr uint64_t Role          = 0x148;  // ENetRole
}
namespace AActor标志 {
    constexpr uint64_t bHidden                = 0x61;  // uint8:1 (BitIndex 0)
    constexpr uint64_t bActorIsBeingDestroyed = 0x68;  // uint8:1 (BitIndex 2)
}
namespace USceneComponent标志 {
    constexpr uint64_t bHiddenInGame = 0x165;  // uint8:1 (BitIndex 3)
    constexpr uint64_t bVisible      = 0x165;  // uint8:1 (BitIndex 6)
}
namespace APawn {
    constexpr uint64_t PlayerState = 0x2E8;  // APlayerState*（无畏 0x2E8，暗区 0x3D8）
    constexpr uint64_t Controller  = 0x300;  // AController*
}
namespace ACharacter {
    constexpr uint64_t Mesh               = 0x328;  // USkeletalMeshComponent*（标准 ACharacter::Mesh）
    constexpr uint64_t CharacterMovement  = 0x330;
    constexpr uint64_t CapsuleComponent   = 0x338;
    constexpr uint64_t BaseEyeHeight      = 0x3A8;  // float
    constexpr uint64_t CrouchedEyeHeight  = 0x3AC;  // float
}

// ======================= 相机链（世界转屏幕） =======================
// 自己控制器 → PlayerCameraManager(0x360) → CameraCachePrivate(0x1BD0) → POV(0x10) → FMinimalViewInfo
namespace APlayerController {
    constexpr uint64_t PlayerCameraManager = 0x360;  // APlayerCameraManager*
    constexpr uint64_t AcknowledgedPawn    = 0x348;  // APawn*
}
namespace APlayerCameraManager {
    constexpr uint64_t CameraCachePrivate           = 0x1BD0;  // FCameraCacheEntry
    constexpr uint64_t LastFrameCameraCachePrivate  = 0x21E0;
    constexpr uint64_t CameraCache                  = 0x340;
    constexpr uint64_t LastFrameCameraCache         = 0x950;
    constexpr uint64_t ViewTarget                   = 0x0F60;  // FTViewTarget（0x620 大小）
}
namespace FCameraCacheEntry {
    constexpr uint64_t POV = 0x10;  // FMinimalViewInfo
}
namespace FMinimalViewInfo {
    constexpr uint64_t Location = 0x00;   // FVector
    constexpr uint64_t Rotation = 0x18;   // FRotator
    constexpr uint64_t FOV      = 0x24;   // float
}

// ---- 补：暗区调用方用的完整 FMinimalViewInfo 字段（UE4 标准布局，无畏同源）----
namespace FMinimalViewInfo补 {
    constexpr uint64_t Location     = 0x00;  // FVector
    constexpr uint64_t Rotation     = 0x18;  // FRotator (无畏比暗区多了 LocationExcludeOffset 0x0C)
    constexpr uint64_t FOV          = 0x24;  // float
    constexpr uint64_t DesiredFOV   = 0x28;  // float
    constexpr uint64_t AspectRatio  = 0x3C;  // float   锁定的宽高比
    constexpr uint64_t bConstrainAspectRatio = 0x40; // uint8 bit0  是否锁宽高比
    constexpr uint64_t FOVMode      = 0x41;  // uint8  无畏无FOVMode字段, 用 ProjectionMode(0x41)
    constexpr uint64_t OffCenterProjectionOffset = 0x5B0; // FVector2D 偏心投影
}

// ---- 补：FOV 模式枚举（dump: enum EECameraFOVMode）----
namespace FOV模式 {
    constexpr uint8_t 水平         = 0;   // HorizontalFOV
    constexpr uint8_t 垂直         = 1;   // VerticalFOV
    constexpr uint8_t 固定比例垂直 = 2;   // FixedAspectVerticalFOV
}

// ======================= FTransform / FMatrix =======================
namespace FTransform {
    constexpr uint64_t rotation    = 0x00;  // FQuat
    constexpr uint64_t translation = 0x10;  // FVector
    constexpr uint64_t scale       = 0x1C;  // FVector
    // 暗区调用方用大写别名
    constexpr uint64_t Rotation    = 0x00;  // = rotation
    constexpr uint64_t Scale3D     = 0x1C;  // = scale
}

// ======================= 玩家名字 =======================
namespace APlayerState {
    constexpr uint64_t PlayerNamePrivate = 0x3A0;  // FString（TArray<TCHAR>，UTF-16）
    constexpr uint64_t Score            = 0x2C8;   // float
}

// ======================= 场景组件（坐标） =======================
namespace USceneComponent {
    constexpr uint64_t RelativeLocation = 0x134;  // FVector  ★方框透视取值
    constexpr uint64_t RelativeRotation = 0x140;  // FRotator
    constexpr uint64_t RelativeScale3D  = 0x14C;  // FVector
    // ComponentToWorld 是私有成员 dump 没导出，按 UE4 标准布局反推：
    // RelativeLocation(0x134) 前是 FTransform(0x30) + 4 字节对齐 → 0x100
    constexpr uint64_t ComponentToWorld = 0x100;  // FTransform（推断：0x134 - 0x30 - 0x4）
}

// ======================= 骨骼组件（骨骼 ESP） =======================
namespace USkinnedMeshComponent {
    constexpr uint64_t SkeletalMesh        = 0x550;  // USkeletalMesh*
    constexpr uint64_t MasterPoseComponent = 0x558;
}
namespace USkeletalMeshComponent {
    constexpr uint64_t CachedBoneSpaceTransforms       = 0x810;  // TArray<FTransform> 骨骼空间
    constexpr uint64_t CachedComponentSpaceTransforms  = 0x820;  // TArray<FTransform> 组件空间
}
namespace USkeletalMesh {
    constexpr uint64_t Skeleton = 0x398;  // USkeleton*
}
namespace USkeleton {
    constexpr uint64_t BoneTree = 0x2B0;  // TArray<FBoneNode>
}
namespace FBoneNode {
    constexpr uint64_t Name = 0x00;  // FName
    constexpr uint64_t ParentIndex = 0x08;  // int32
    constexpr uint64_t 步进        = 0x0C;  // 每个 FBoneNode 大小
}
namespace FMeshBoneInfo {
    constexpr uint64_t 步进甲 = 0x0C;  // FName(8) + int32(4)
    constexpr uint64_t 步进乙 = 0x10;  // 带对齐
    constexpr uint64_t 父索引 = 0x08;  // int32 ParentIndex
}

// ======================= GAS 属性集（血量 / 护盾 / 后座 / 射速） =======================
// ★ 无畏契约血量/后座都走 GAS，不是裸字段。
//   ASC = 角色 + 0x06E8 (USGASComponent*)
//   → +0x20F8 AttributeBasic（血量/护盾）
//   → +0x2100 AttributePawn（后座/散布/射速）
namespace ASGAbilityCharacter {
    constexpr uint64_t AbilitySystemComponent = 0x6E8;  // USGASComponent*
}
namespace USGASComponent {
    constexpr uint64_t AttributeBasic = 0x20F8;  // USGAttributeBasic*
    constexpr uint64_t AttributePawn  = 0x2100;  // USGAttributePawn*
}
// USGAttributeBasic 里的属性（FGameplayAttributeData，CurrentValue 在 +0x0）
namespace USGActorHealthAttributeSet {
    // 兼容暗区源码里 USGActorHealthAttributeSet::Health / HealthMax 的调用名
    // 无畏契约对应 USGAttributeBasic::Health(0xD8) / MaxHealth(0x170)
    constexpr uint64_t Health    = 0xD8;   // 当前血量（FGameplayAttributeData，读 +0x0 float）
    constexpr uint64_t HealthMax = 0x170;  // 最大血量
    constexpr uint64_t Shield    = 0x1A0;  // 护盾
    constexpr uint64_t MaxShield = 0x218;  // 最大护盾
}
namespace USGCharacterHealthAttributeSet {
    // 暗区有 DBNO（倒地），无畏契约没有，这两个兼容名指向无效偏移，避免误用
    constexpr uint64_t DBNOHealth    = 0x00;  // 无畏无倒地，禁用
    constexpr uint64_t DBNOHealthMax = 0x00;  // 无畏无倒地，禁用
}
// USGAttributePawn 里的武器属性（后座/散布/射速）
namespace USGWeaponRecoilComponent {
    // 兼容暗区源码的调用名，实际指向无畏的 AttributePawn 属性
    // ★ 无畏后座是 GAS 属性 RecoilRate，写 0 即无后座
    constexpr uint64_t RecoilRate         = 0xE88;  // 后座力倍率（写 0 = 无后座）
    constexpr uint64_t InAccuracyMultiple = 0xE08;  // 精准度倍率（写 0 = 无散布）
    constexpr uint64_t ShootIntervalRate  = 0xC08;  // 射击间隔（写小 = 加速射）
    constexpr uint64_t RecRecoverRate     = 0xF08;  // 后座恢复速率
}

// ======================= 队伍（判敌我） =======================
// ★ 无畏契约用 CampID（阵营）判敌我，不是暗区的 TeamIndex
namespace ASGBasePawn {
    constexpr uint64_t CampID = 0x6A0;  // int32 阵营ID（自己 vs 敌人 不相等 = 敌人）
}
// 兼容暗区源码 ASGPlayerState::TeamIndex 的调用名
namespace ASGPlayerState {
    constexpr uint64_t TeamIndex = 0x4C0;  // 无畏 ASGBasePlayerState::CampID（0x4C0）
}

// ======================= 角色基类（ASGBaseCharacter 专属） =======================
// 骨骼网格：ASGBaseCharacter::MasterCoreMeshComp（第三人称骨骼）
namespace ASGBaseCharacter {
    constexpr uint64_t MasterCoreMeshComp = 0xE28;  // USkeletalMeshComponent*
}

// ======================= 死亡判定 =======================
// ★ 无畏契约无 USGCharacterDeathComponent，改用血量判死（Health.CurrentValue <= 0）
//   兼容暗区源码的命名空间保留，但指向无效偏移，实际死亡判定走血量
namespace USGCharacterDeathComponent {
    constexpr uint64_t DeathInfo = 0x00;  // 无畏无此组件，禁用
}
namespace FCharacterDeathInfo {
    constexpr uint64_t bIsDead = 0x00;  // 无畏无此字段，禁用
}

// ======================= TArray 通用 =======================
// ---- 补：名单上限（暗区调用方引用）----
constexpr int 候选名单上限 = 1024;

namespace TArray长度 {
    constexpr uint64_t ArrayNum偏移 = 0x08;  // int32（TArray 第 2 个字段）
    constexpr uint64_t ArrayMax偏移 = 0x0C;  // int32
    constexpr int 合理上限 = 0x10000;   // 65536，正常数组不会超过（防野指针）
    constexpr int 容量上限 = 0x10000;   // 同上
}

// ======================= GNames 解密（Valorant profile） =======================
// ★ 无畏契约 GNames 是加密的，必须用 MobileDumper 的 Valorant profile 解密。
//   解密逻辑（MJx0/MobileDumper-7 的 Valorant.h）：
//     NamesPtr 从 GNames+128 读 8 个偏移逐字节拼回
//   这套解密在子线程.h 的 取对象名 里实现，这里只给偏移标记。
namespace GNames解密 {
    constexpr uint64_t NamesPtrOffset = 0x80;  // GNames + 0x80 处 8 字节解密种子
    constexpr uint64_t 块步进         = 0xD0;  // iOS 上 name chunk 块步进
}

}  // namespace SDK

// ============================================================================
//  ★ 暗区专属命名空间兼容层（无畏契约改造后残留引用，指向无害值/禁用）
//  这些命名空间在暗区源码里被"组件缓存表扫表"等函数引用，无畏契约没有对应物，
//  全部指向 0 或无害值，保证编译通过（这些函数在 #if 0 块内不会被调用）。
// ============================================================================
namespace SDK {
namespace ASGCharacter组件表 {
    constexpr uint64_t CmpCacheArray = 0;   // 无畏无组件缓存表
    constexpr uint64_t 槽数           = 0;
}
namespace ASGWeapon组件表 {
    constexpr uint64_t WeaponCmpCacheArray = 0;
    constexpr uint64_t 槽数                = 0;
}
namespace ASGInventory组件表 {
    constexpr uint64_t CmpCacheArray = 0;
    constexpr uint64_t 槽数          = 0;
}
namespace ASGTacticalCharacter {
    constexpr uint64_t CharacterWeaponManagerComponent = 0;
}
namespace USGCharacterWeaponManagerComponent {
    constexpr uint64_t CurrentWeapon    = 0;
    constexpr uint64_t LeftHandWeapon   = 0;
}
namespace UAbilitySystemComponent {
    constexpr uint64_t SpawnedAttributes = 0;
}
namespace UStruct {
    constexpr uint64_t SuperStruct = 0x30;  // 标准 UStruct::SuperStruct（保留，名字池遍历用）
}
namespace 组件类名 {
    constexpr const char* 后坐力组件 = "";
    constexpr const char* 武器管理器 = "";
}
namespace 组件类名补 {
    constexpr const char* 死亡组件         = "";
    constexpr const char* 死亡组件关键字   = "";
}
namespace 角色基类名 {
    constexpr const char* AI角色 = "";
}
// 主线程.h 里的暗区专属残留引用（检测面/回放取证/头甲/压枪，无畏契约没有）
namespace ASGGameState {
    constexpr uint64_t GameStateStatisComponent = 0;
}
namespace USGGameStateStatisComponent {
    constexpr uint64_t FrameRateJitterCount = 0;
    constexpr uint64_t FrameRateJitterThreshold = 0;
    constexpr uint64_t ReportPeriod = 0;
}
namespace ASGPlayerState补 {
    constexpr uint64_t SGPlayerStateStatisComponent = 0;
}
namespace USGPlayerStateStatisComponent {
    constexpr uint64_t FrameRateJitterCount = 0;
    constexpr uint64_t FrameRateJitterThreshold = 0;
    constexpr uint64_t ReportPeriod = 0;
}
namespace ASGPlayerCameraManager补 {
    constexpr uint64_t bEnableModifyRotationByWeaponRecoil = 0;
}
namespace USGGameInstanceReplaySubsystem {
    constexpr uint64_t IsOpenReport = 0;
    constexpr uint64_t IsOpenRep = 0;
    constexpr uint64_t LocalFileKillCamInfo = 0;
    constexpr uint64_t HttpDownloadKillCamInfoList = 0;
}
namespace FSGLocalFileKillCamInfo {
    constexpr uint64_t 大小 = 0;
    constexpr uint64_t KillCamFragments = 0;
    constexpr uint64_t ReplayFileName = 0;
}
namespace USGGameStateStatisComponent {
    constexpr uint64_t ClientPendingResolveCheatInfos = 0;
}
namespace USGPlayerStateStatisComponent {
    constexpr uint64_t AllAntiCheatFlowStructs = 0;
    constexpr uint64_t AllWeaponValidateFailInfo = 0;
}
namespace USGCharacterArmorManagerComponent {
    constexpr uint64_t ArmorList = 0;
}
namespace ASGInventory {
    constexpr uint64_t ArmorLevel = 0;
}
namespace USceneComponent补 {
    constexpr uint64_t ComponentToWorld = 0;
    constexpr uint64_t 世界变换指针     = 0x100;  // 无畏 USceneComponent::ComponentToWorld（推断 0x100）
}
namespace USkinnedMeshComponent补 {
    constexpr uint64_t 参考骨架扫描起  = 0;
    constexpr uint64_t 骨骼数组族     = 0x820;  // 无畏 CachedComponentSpaceTransforms
    constexpr uint64_t 骨骼数组索引   = 0x820;  // 无畏 CachedComponentSpaceTransforms
}
namespace USkeletalMesh {
    constexpr uint64_t 参考骨架扫描起 = 0;
    constexpr uint64_t 参考骨架扫描止 = 0;
}
namespace USkeleton {
    constexpr uint64_t 参考骨架扫描起 = 0;
    constexpr uint64_t 参考骨架扫描止 = 0;
}
namespace 压枪系数 {
    constexpr float 默认 = 0.0f;
    constexpr float 下限 = 0.0f;
    constexpr float 上限 = 0.0f;
    constexpr float 抖动幅度 = 0.0f;
}
namespace 头甲链 {
    constexpr uint64_t 装备层 = 0;
    constexpr uint64_t 护甲管理器缓存槽 = 0;
}
}  // namespace SDK
