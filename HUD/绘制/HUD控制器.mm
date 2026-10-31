#import <pthread.h>
#import <sys/utsname.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOKitKeys.h>

#import "HUD控制器.h"

#import "游戏/暗区突围/国际服/子线程.h"
#import "游戏/暗区突围/国际服/主线程.h"
#import "内核/kfd.h"


@implementation HUD控制器 {
    FBSOrientationObserver* FBSOrientationObserver;
    BOOL 当前过直播状态;
};

- (void) viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.clearColor;
    
    // 读取过直播开关状态
    NSDictionary *配置 = [NSDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Preferences/com.map.settings.plist"];
    当前过直播状态 = [[配置 objectForKey:@"过直播密钥"] boolValue];
    
    self.过直播窗口 = [[过直播窗口 alloc] initWithFrame:self.view.bounds];
    self.过直播窗口.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.MTKView = [[MTKView alloc] initWithFrame:self.view.bounds];
    self.MTKView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.MTKView.preferredFramesPerSecond = 120.0f;
    self.MTKView.device = MTLCreateSystemDefaultDevice();
    self.MTKView.opaque = NO;
    self.MTKView.backgroundColor = UIColor.clearColor;
    self.MTKView.clearColor = MTLClearColorMake(0, 0, 0, 0);
    self.MTKView.delegate = self;
    
    // 根据过直播状态决定添加方式
    if (当前过直播状态) {
        self.MTKView.frame = self.过直播窗口.bounds;
        [self.过直播窗口 addSubview:self.MTKView];
        [self.view addSubview:self.过直播窗口];
    } else {
        [self.view addSubview:self.MTKView];
    };
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    
    ImGuiIO& ImGuiIO = ImGui::GetIO();
    ImGuiIO.Fonts->Clear();
    ImFontConfig ImFontConfig;
    ImFontConfig.FontDataOwnedByAtlas = false;
    ImVector<ImWchar> ImVector;
    ImFontGlyphRangesBuilder ImFontGlyphRangesBuilder;
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesDefault());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesGreek());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesKorean());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesJapanese());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesChineseFull());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesCyrillic());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesThai());
    ImFontGlyphRangesBuilder.AddRanges(ImGuiIO.Fonts->GetGlyphRangesVietnamese());
    ImFontGlyphRangesBuilder.BuildRanges(&ImVector);
    
    ImGuiIO.Fonts->Clear();
    NSString* 字体路径 = [[NSBundle mainBundle] pathForResource:@"阿里巴巴惠普体" ofType:@"ttf"];
    ImGuiIO.Fonts->AddFontFromFileTTF([字体路径 UTF8String], 30.0f, &ImFontConfig, ImVector.Data);
    ImGuiIO.Fonts->Build();
    
    // ★ r28: 读取线程只起一次。
    //   旧写法直接 pthread_create —— 控制器/窗口重建时(关闭过直播开关、旋转重建等)
    //   viewDidLoad 会再跑一遍, 于是又起一个读取线程。两个线程同时往游戏里读内存:
    //   读频率凭空翻倍, 帧时间被拖毛 → 更容易被客户端那条"帧率抖动上报"抓到,
    //   而且两个线程共用一套缓存状态, 互相当抄。
    static pthread_t 子线程ID;
    static bool 子线程已起 = false;
    if (!子线程已起) {
        子线程已起 = true;
        pthread_create(&子线程ID, nullptr, 子线程, nullptr);
    };
    
    UIDevice *Device = [UIDevice currentDevice];
    struct utsname sname;
    uname(&sname);
    
    string systemVersion = [Device.systemVersion UTF8String];
    string machine = [[NSString stringWithCString:sname.machine encoding:NSUTF8StringEncoding] UTF8String];
    if (machine == "iPad14,1") {
        设备类型.设备信息.设备名 = "iPad mini (第六代) [ " + systemVersion + " ]";
    } else if (machine == "iPhone14,5") {
        设备类型.设备信息.设备名 = "iPhone13 [ " + systemVersion + " ]";
    } else {
        设备类型.设备信息.设备名 = machine + " [ " + systemVersion + " ]";
    };
    
    io_service_t IOPlatformExpertDevice = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    CFTypeRef SerialNumber = IORegistryEntryCreateCFProperty(IOPlatformExpertDevice, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);
    IOObjectRelease(IOPlatformExpertDevice);
    设备类型.设备信息.序列号 = [(__bridge NSString *)SerialNumber UTF8String];
    CFRelease(SerialNumber);
    
    内核环境自检();
    绘制类型.内核 = 初始化内核链路(绘制类型.进程名, 绘制类型.模块名, 绘制类型.模块地址);
    g_内存可写 = kfd.有写入能力();
    if (!绘制类型.内核) {
        内核记日志("✗ 首轮没起来,后台每 5 秒自动重试");
    };
    
    ImGui_ImplMetal_Init(self.MTKView.device);
    self.MTLCommandQueue = [self.MTKView.device newCommandQueue];
};

- (void) drawInMTKView:(nonnull MTKView *)view {
    // 检测过直播开关变化
    NSDictionary *配置 = [NSDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Preferences/com.map.settings.plist"];
    BOOL 过直播开关 = [[配置 objectForKey:@"过直播密钥"] boolValue];
    
    if (过直播开关 != 当前过直播状态) {
        当前过直播状态 = 过直播开关;
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.MTKView removeFromSuperview];
            [self.过直播窗口 removeFromSuperview];
            
            if (过直播开关) {
                self.MTKView.frame = self.过直播窗口.bounds;
                [self.过直播窗口 addSubview:self.MTKView];
                [self.view addSubview:self.过直播窗口];
            } else {
                self.MTKView.frame = self.view.bounds;
                [self.view addSubview:self.MTKView];
            };
        });
    };
    
    // 检查设备和命令队列是否有效
    if (!self.MTKView.device || !self.MTLCommandQueue) {
        return;
    };
    
    ImGuiIO& ImGuiIO = ImGui::GetIO();
    // ★ 绘制坐标系 = Metal 真实在画的那块画布(drawableSize), 不是自己拿 bounds 乘缩放猜的。
    //   平板旋转/多任务下这两个值会不一致, 一不一致整个 HUD 就被画到屏幕外(看着像"全没效果")。
    //   用 drawableSize 之后, 手机/平板、横屏/竖屏一律对齐。
    CGSize 绘制尺寸 = view.drawableSize;
    if (!(绘制尺寸.width > 1.0) || !(绘制尺寸.height > 1.0)) {
        绘制尺寸 = CGSizeMake(view.bounds.size.width * UIScreen.mainScreen.scale,
                               view.bounds.size.height * UIScreen.mainScreen.scale);
    };
    设备类型.设备信息.宽度 = 绘制尺寸.width;
    设备类型.设备信息.高度 = 绘制尺寸.height;
    
    // 检查屏幕尺寸是否有效
    if (!(设备类型.设备信息.宽度 > 1.0f) || !(设备类型.设备信息.高度 > 1.0f)) {
        return;
    };
    
    ImGuiIO.DisplaySize = ImVec2(设备类型.设备信息.宽度, 设备类型.设备信息.高度);
    ImGuiIO.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    
    id<MTLCommandBuffer> MTLCommandBuffer = [self.MTLCommandQueue commandBuffer];
    if (!MTLCommandBuffer) {
        return;
    };
    
    MTLRenderPassDescriptor* MTLRenderPassDescriptor = view.currentRenderPassDescriptor;
    if (MTLRenderPassDescriptor == nil) {
        [MTLCommandBuffer commit];
        return;
    };
    
    // 检查drawable是否有效
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!drawable) {
        [MTLCommandBuffer commit];
        return;
    };
    
    ImGui_ImplMetal_NewFrame(MTLRenderPassDescriptor);
    ImGui::NewFrame();
    
    ImDrawList* ImDrawList = ImGui::GetForegroundDrawList();
    int 玩家数量 = 0, 人机数量 = 0, 盒子数量 = 0;
    
    // 只有内核初始化成功才进行绘制
    if (绘制类型.内核) {
        主线程(ImDrawList, 玩家数量, 人机数量, 盒子数量);
    };
    
    // ★ 顶部计数条: 只留这一条(附近 真人 X  人机 Y), 黑底圆角, 居中偏上。
    //   这是面板外唯一保留的数字信息 —— 你要的"真人/AI 数"就在这。
    //   口径: 我为中心 400m 内、还活着的人, 跟朝向/屏内屏外无关。
    绘制矩形(ImDrawList, ImVec4(设备类型.设备信息.宽度 * 0.5f - 190.0f, 60.0f, 380.0f, 58.0f), IM_COL32(0, 0, 0, 130), 29.0f);
    string 计数行 = "附近 真人 " + to_string(玩家数量) + "   人机 " + to_string(人机数量);
    if (g_帧状态 != "正常") 计数行 += "   [" + g_帧状态 + "]";
    绘制文字(ImDrawList, 计数行, 30.0f, ImVec2(设备类型.设备信息.宽度 * 0.5f, 92.0f), IM_COL32(255, 255, 255, 255), 0, true);

    // ★ 屏幕左下角唯一一行: 内核有没有加载成功。
    //   三行诊断 / 屏幕中间保底行 / 内核未加载日志堆已全部移除, 不再上屏。
    //   内核状态本来就会变(后台每 5 秒自动重试), 所以这里实时取, 不缓存。
    {
        bool 内核好了 = (bool)绘制类型.内核;
        绘制文字(ImDrawList, 内核好了 ? "内核加载成功" : "内核加载失败", 26.0f,
                ImVec2(26.0f, 设备类型.设备信息.高度 - 48.0f),
                内核好了 ? IM_COL32(120, 255, 140, 255) : IM_COL32(255, 90, 90, 255), 1, true);
    };
    
    ImGui::Render();
    id<MTLRenderCommandEncoder> MTLRenderCommandEncoder = [MTLCommandBuffer renderCommandEncoderWithDescriptor:MTLRenderPassDescriptor];
    if (!MTLRenderCommandEncoder) {
        [MTLCommandBuffer commit];
        return;
    };
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), MTLCommandBuffer, MTLRenderCommandEncoder);
    [MTLRenderCommandEncoder endEncoding];
    [MTLCommandBuffer presentDrawable:drawable];
    [MTLCommandBuffer commit];
};

- (void) mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    
};

- (instancetype) init {
    self = [super init];
    if (self) {
        FBSOrientationObserver = [[objc_getClass("FBSOrientationObserver") alloc] init];
        __weak HUD控制器* weakSelf = self;
        [FBSOrientationObserver setHandler:^(FBSOrientationUpdate* FBSOrientationUpdate) {
            HUD控制器* strongSelf = weakSelf;
            dispatch_async(dispatch_get_main_queue(), ^{
                [strongSelf updateOrientation:(UIInterfaceOrientation)FBSOrientationUpdate.orientation animateWithDuration:FBSOrientationUpdate.duration];
            });
        }];
    };
    return self;
};

- (void) dealloc {
    [FBSOrientationObserver invalidate];
};

static inline CGFloat orientationAngle(UIInterfaceOrientation orientation) {
    switch (orientation) {
        case UIInterfaceOrientationPortraitUpsideDown:
            return M_PI;
        case UIInterfaceOrientationLandscapeLeft:
            return -M_PI_2;
        case UIInterfaceOrientationLandscapeRight:
            return M_PI_2;
        default:
            return 0;
    };
};

static inline CGRect orientationBounds(UIInterfaceOrientation orientation, CGRect bounds) {
    switch (orientation) {
        case UIInterfaceOrientationLandscapeLeft:
        case UIInterfaceOrientationLandscapeRight:
            return CGRectMake(0, 0, bounds.size.height, bounds.size.width);
        default:
            return bounds;
    };
};

- (void) updateOrientation:(UIInterfaceOrientation)orientation animateWithDuration:(NSTimeInterval)duration {
    CGRect Rect = orientationBounds(orientation, UIScreen.mainScreen.bounds);
    [self.view setNeedsUpdateConstraints];
    [self.view setBounds:Rect];
    
    __weak typeof(self) weakSelf = self;
    [UIView animateWithDuration:duration animations:^{
        [weakSelf.view setTransform:CGAffineTransformMakeRotation(orientationAngle(orientation))];
    } completion:^(BOOL finished) {
        // 更新 MTKView 和过直播窗口的 frame
        weakSelf.过直播窗口.frame = weakSelf.view.bounds;
        if (当前过直播状态) {
            weakSelf.MTKView.frame = weakSelf.过直播窗口.bounds;
        } else {
            weakSelf.MTKView.frame = weakSelf.view.bounds;
        };
        [self.view setNeedsLayout];
        [self.view layoutIfNeeded];
    }];
};

@end

