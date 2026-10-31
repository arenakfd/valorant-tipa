// 全局绘制设置结构体 - 用于跨线程传值
struct 绘制设置 {
    BOOL 绘图开关 = NO;     // HUD 主开关
    BOOL 方框显示 = NO;     // 方框透视
    BOOL 骨骼显示 = NO;     // 骨骼透视
    BOOL 人机显示 = NO;     // 显示人机
    BOOL 距离显示 = NO;     // 显示距离
    BOOL 后坐力   = NO;     // 无后坐力
    BOOL 过直播 = NO;       // 直播隐藏
};

// 全局设置实例声明
extern struct 绘制设置 全局绘制设置;

@interface 按钮存储方法 : NSObject

+ (instancetype) 初始化;

- (void) 设置按钮状态:(BOOL)状态 密钥:(NSString*)密钥;
- (BOOL) 取按钮状态:(NSString*)密钥;

// 同步设置到全局结构体
- (void) 同步全局设置;

@property (class, nonatomic, readonly) NSString* 绘图密钥;
@property (class, nonatomic, readonly) NSString* 方框密钥;
@property (class, nonatomic, readonly) NSString* 骨骼密钥;
@property (class, nonatomic, readonly) NSString* 人机密钥;
@property (class, nonatomic, readonly) NSString* 距离密钥;
@property (class, nonatomic, readonly) NSString* 后坐密钥;
@property (class, nonatomic, readonly) NSString* 过直播密钥;
@property (class, nonatomic, readonly) NSString* 射线密钥;
@property (class, nonatomic, readonly) NSString* 在场密钥;
@property (class, nonatomic, readonly) NSString* 头甲密钥;
// ★ r35: 真人玩家名字开关
@property (class, nonatomic, readonly) NSString* 名字密钥;
@end
