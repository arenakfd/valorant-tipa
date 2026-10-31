
#import "按钮存储.h"

// 全局设置实例定义
struct 绘制设置 全局绘制设置;

// 共享配置文件路径
static NSString* 取配置文件路径() {
    return @"/var/mobile/Library/Preferences/com.map.settings.plist";
}

@implementation 按钮存储方法

+ (instancetype) 初始化 {
    static 按钮存储方法* 方法 = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        方法 = [[self alloc] init];
    });
    return 方法;
};

+ (NSString*)绘图密钥 { return @"绘图密钥"; };
+ (NSString*)方框密钥 { return @"方框密钥"; };
+ (NSString*)骨骼密钥 { return @"骨骼密钥"; };
+ (NSString*)人机密钥 { return @"人机密钥"; };
+ (NSString*)距离密钥 { return @"距离密钥"; };
+ (NSString*)后坐密钥 { return @"后坐密钥"; };
+ (NSString*)过直播密钥 { return @"过直播密钥"; };
+ (NSString*)射线密钥 { return @"射线密钥"; };
+ (NSString*)在场密钥 { return @"在场密钥"; };
+ (NSString*)头甲密钥 { return @"头甲密钥"; };
+ (NSString*)名字密钥 { return @"名字密钥"; };   // ★ r35

- (void) 设置按钮状态:(BOOL)状态 密钥:(NSString *)密钥 {
    NSString *路径 = 取配置文件路径();
    NSMutableDictionary *配置 = [NSMutableDictionary dictionaryWithContentsOfFile:路径];
    if (!配置) {
        配置 = [NSMutableDictionary dictionary];
    }
    [配置 setObject:@(状态 ? YES : NO) forKey:密钥];
    [配置 writeToFile:路径 atomically:YES];
};

- (BOOL) 取按钮状态:(NSString *)密钥 {
    NSString *路径 = 取配置文件路径();
    NSDictionary *配置 = [NSDictionary dictionaryWithContentsOfFile:路径];
    if (!配置) return NO;
    NSNumber *值 = [配置 objectForKey:密钥];
    return [值 boolValue] ? YES : NO;
};

- (void) 同步全局设置 {
};

@end
