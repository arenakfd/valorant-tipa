
#import <MacTypes.h>
#import <CoreFoundation/CFBase.h>
#import <Foundation/NSPathUtilities.h>
#import <signal.h>
#import <unistd.h>

typedef struct __IOHIDEvent* IOHIDEvent;
typedef struct __IOHIDNotification* IOHIDNotification;
typedef struct __IOHIDService* IOHIDService;
typedef struct __GSEvent* GSEvent;

extern "C" {
    void GSInitialize();
    void UIApplicationInitialize();
    void BKSDisplayServicesStart();
    void GSEventPushRunLoopMode(CFStringRef mode);
    void GSEventInitialize(Boolean registerPurple);
    void UIApplicationInstantiateSingleton(id aclass);
    void *BKSHIDEventRegisterEventCallback(void (*)(void *, void *, IOHIDService, IOHIDEvent));
};

@interface UIApplication (Private)

- (void)_accessibilityInit;
- (void)__completeAndRunAsPlugin;
- (void)_enqueueHIDEvent:(IOHIDEvent)IOHIDEvent;

@end

static __used void 挂载应用(void*, void*, IOHIDService IOHIDService, IOHIDEvent IOHIDEvent) {
    static UIApplication* 应用 = [UIApplication sharedApplication];
    [应用 _enqueueHIDEvent:IOHIDEvent];
};

NSString* 存储进程名() {
    static dispatch_once_t onceToken;
    static NSString* 进程名 = nil;
    dispatch_once(&onceToken, ^{
        static NSString* 路径 = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject];
        进程名 = [路径 stringByAppendingPathComponent:@"进程名"];
    });
    return 进程名;
};

// ============================================================================
//  ★ r28 —— "关闭HUD"不再直接 SIGKILL
//
//  旧写法是 kill(pid, SIGKILL): 插件进程被**无条件、无清理**地干掉。后果是
//  内核原语 / 页表句柄 / proc 引用全留在内核里没人释放, 下次再开 HUD 走一遍
//  初始化时, 越狱库看到的是脏状态 —— 表现就是"关掉软件绘制重开, 内核就失败,
//  必须重启游戏"。这就是那个现象的正主之一。
//
//  现在: 先发 SIGTERM, 插件进程里注册了处理函数(见 HUD.mm 的 注册退出信号),
//        它会 kfd.释放kfd() 把句柄放干净再 _exit(0)。1.5 秒还没退再 SIGKILL 兜底 ——
//        清理是加分项, 绝不会因为清理卡住而关不掉。
// ============================================================================

int main(int argc, char* argv[]) {
    @autoreleasepool {
        if (argc <= 1) {
            return UIApplicationMain(argc, argv, @"应用", @"应用挂载方法");
        };
        if (strcmp(argv[1], "开启HUD") == 0) {
            NSString* 进程ID = [NSString stringWithFormat:@"%d", getpid()];
            [进程ID writeToFile:存储进程名() atomically:YES encoding:NSUTF8StringEncoding error:nil];
            [UIScreen initialize];
            CFRunLoopGetCurrent();
            GSInitialize();
            BKSDisplayServicesStart();
            UIApplicationInitialize();
            UIApplicationInstantiateSingleton(objc_getClass("HUD"));
            static id<UIApplicationDelegate> HUD挂载方法 = [[objc_getClass("HUD挂载方法") alloc] init];
            [UIApplication.sharedApplication setDelegate:HUD挂载方法];
            
            [UIApplication.sharedApplication _accessibilityInit];
            [UIApplication.sharedApplication __completeAndRunAsPlugin];
            
            
            [NSRunLoop currentRunLoop];
            BKSHIDEventRegisterEventCallback(挂载应用);
            
            GSEventInitialize(0);
            GSEventPushRunLoopMode(kCFRunLoopDefaultMode);
            
            CFRunLoopRun();
        } else {
            NSString* 进程名 = [NSString stringWithContentsOfFile:存储进程名() encoding:NSUTF8StringEncoding error:nil];
            if (进程名) {
                pid_t 进程ID = (pid_t)[进程名 intValue];
                if (strcmp(argv[1], "关闭HUD") == 0) {
                    // ★ r28: 先 SIGTERM —— 给插件进程一个把内核句柄放干净的机会;
                    //   1.5 秒还没走再 SIGKILL 兜底(清理是加分项, 不许因此关不掉)。
                    kill(进程ID, SIGTERM);
                    for (int i = 0; i < 30; i++) {
                        usleep(50 * 1000);
                        if (kill(进程ID, 0) != 0) break;   // 已经退了
                    };
                    if (kill(进程ID, 0) == 0) kill(进程ID, SIGKILL);
                    unlink(存储进程名().UTF8String);
                };
            };
        };
        return EXIT_SUCCESS;
    };
};
