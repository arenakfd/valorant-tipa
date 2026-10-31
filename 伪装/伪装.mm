#import "伪装.h"
#import "环境隐藏.h"
#import "kfd.h"

static CFMessagePortRef MessagePortRef = nullptr;
static CFDataRef callout(CFMessagePortRef MessagePortRef, SInt32 Int32, CFDataRef DataRef, void*) {
    return nullptr;
};

void 本地通讯伪装() {
    if (MessagePortRef != nullptr) {
        return;
    };
    CFMessagePortContext context = {0, nullptr, nullptr, nullptr, nullptr};
    Boolean shouldFreeInfo = FALSE;
    MessagePortRef = CFMessagePortCreateLocal(kCFAllocatorDefault, CFSTR("com.apple.Maps.mach"), callout, &context, &shouldFreeInfo);
    CFRunLoopSourceRef RunLoopSourceRef = CFMessagePortCreateRunLoopSource(kCFAllocatorDefault, MessagePortRef, 0);
    if (RunLoopSourceRef != nullptr) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), RunLoopSourceRef, kCFRunLoopDefaultMode);
        CFRelease(RunLoopSourceRef);
    };
};

// 初始化环境隐藏系统
void 初始化环境隐藏系统() {
    NSLog(@"\n[伪装] 正在初始化环境隐藏系统...");
    
    if (!kfd.调用kfd()) {
        NSLog(@"[伪装] ❌ KFD 初始化失败");
        return;
    }
    NSLog(@"[伪装] ✅ KFD 已初始化");
    
    if (!环境隐藏.初始化()) {
        NSLog(@"[伪装] ❌ 环境隐藏模块初始化失败");
        return;
    }
    NSLog(@"[伪装] ✅ 环境隐藏系统初始化完成\n");
}

// 执行完整环境检测
void 执行完整环境检测() {
    NSLog(@"\n[伪装] 开始执行完整环境检测...\n");
    
    环境隐藏.检测roothide环境();
    环境隐藏.检测巨魔环境();
    环境隐藏.检测kfd环境();
    
    环境隐藏.打印检测结果();
}

// 执行完整环境隐藏
void 执行完整环境隐藏() {
    NSLog(@"\n[伪装] 开始执行完整环境隐藏...\n");
    
    bool 成功 = 环境隐藏.隐藏所有越狱特征();
    
    if (成功) {
        NSLog(@"[伪装] ✅ 环境隐藏执行成功");
    } else {
        NSLog(@"[伪装] ⚠️  环境隐藏执行完成，但部分特征可能未完全隐藏");
    }
    
    NSLog(@"\n[伪装] 重新检测环境以验证隐藏效果...\n");
    执行完整环境检测();
}
