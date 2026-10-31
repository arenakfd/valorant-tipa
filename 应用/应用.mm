#import <UIKit/UIKit.h>
#import <CoreGraphics/CGAffineTransform.h>
#import "伪装.h"
#import "按钮存储.h"

OBJC_EXTERN void 开启或关闭HUD(bool 标识符);

#define 主题背景色 [UIColor colorWithRed:15.0f/255.0f green:15.0f/255.0f blue:20.0f/255.0f alpha:1.0f]
#define 卡片背景色 [UIColor colorWithRed:25.0f/255.0f green:25.0f/255.0f blue:32.0f/255.0f alpha:1.0f]
#define 粉色 [UIColor colorWithRed:236.0f/255.0f green:72.0f/255.0f blue:153.0f/255.0f alpha:1.0f]
#define 紫色 [UIColor colorWithRed:168.0f/255.0f green:85.0f/255.0f blue:247.0f/255.0f alpha:1.0f]
#define 主文字色 [UIColor whiteColor]
#define 副文字色 [UIColor colorWithWhite:0.5f alpha:1.0f]
#define 红色开关 [UIColor colorWithRed:239.0f/255.0f green:68.0f/255.0f blue:68.0f/255.0f alpha:1.0f]
#define 绿色开关 [UIColor colorWithRed:34.0f/255.0f green:197.0f/255.0f blue:94.0f/255.0f alpha:1.0f]

@interface 应用 : UIApplication
@end

@implementation 应用
@end

@interface 圆形进度视图 : UIView
@property (nonatomic, assign) CGFloat 进度;
@property (nonatomic, strong) CAShapeLayer* 背景层;
@property (nonatomic, strong) CAShapeLayer* 进度层;
@property (nonatomic, strong) UILabel* 数值标签;
@property (nonatomic, strong) UILabel* 总数标签;
@property (nonatomic, strong) UILabel* 百分比标签;
@end

@implementation 圆形进度视图

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self 初始化];
    }
    return self;
}

- (void)初始化 {
    CGFloat 半径 = 35.0f;
    CGPoint 中心 = CGPointMake(50, 50);
    UIBezierPath* 路径 = [UIBezierPath bezierPathWithArcCenter:中心 radius:半径 startAngle:-M_PI_2 endAngle:M_PI_2 * 3 clockwise:YES];
    
    self.背景层 = [CAShapeLayer layer];
    self.背景层.path = 路径.CGPath;
    self.背景层.fillColor = UIColor.clearColor.CGColor;
    self.背景层.strokeColor = [UIColor colorWithWhite:0.2f alpha:1.0f].CGColor;
    self.背景层.lineWidth = 6.0f;
    self.背景层.lineCap = kCALineCapRound;
    [self.layer addSublayer:self.背景层];
    
    self.进度层 = [CAShapeLayer layer];
    self.进度层.path = 路径.CGPath;
    self.进度层.fillColor = UIColor.clearColor.CGColor;
    self.进度层.strokeColor = 紫色.CGColor;
    self.进度层.lineWidth = 6.0f;
    self.进度层.lineCap = kCALineCapRound;
    self.进度层.strokeEnd = 0;
    [self.layer addSublayer:self.进度层];
    
    self.数值标签 = [[UILabel alloc] init];
    self.数值标签.font = [UIFont systemFontOfSize:24.0f weight:UIFontWeightBold];
    self.数值标签.textColor = 主文字色;
    self.数值标签.textAlignment = NSTextAlignmentCenter;
    self.数值标签.text = @"0";
    self.数值标签.frame = CGRectMake(0, 28, 100, 30);
    [self addSubview:self.数值标签];
    
    self.总数标签 = [[UILabel alloc] init];
    self.总数标签.font = [UIFont systemFontOfSize:12.0f weight:UIFontWeightRegular];
    self.总数标签.textColor = 副文字色;
    self.总数标签.textAlignment = NSTextAlignmentCenter;
    self.总数标签.text = @"/ 13";
    self.总数标签.frame = CGRectMake(0, 52, 100, 20);
    [self addSubview:self.总数标签];
    
    self.百分比标签 = [[UILabel alloc] init];
    self.百分比标签.font = [UIFont systemFontOfSize:11.0f weight:UIFontWeightMedium];
    self.百分比标签.textColor = 紫色;
    self.百分比标签.textAlignment = NSTextAlignmentRight;
    self.百分比标签.text = @"0%";
    self.百分比标签.frame = CGRectMake(60, 0, 40, 20);
    [self addSubview:self.百分比标签];
}

- (void)设置进度:(int)当前 总数:(int)总数 {
    CGFloat 进度 = 总数 > 0 ? (CGFloat)当前 / 总数 : 0;
    self.数值标签.text = [NSString stringWithFormat:@"%d", 当前];
    self.总数标签.text = [NSString stringWithFormat:@"/ %d", 总数];
    self.百分比标签.text = [NSString stringWithFormat:@"%.0f%%", 进度 * 100];
    
    [CATransaction begin];
    [CATransaction setAnimationDuration:0.3f];
    self.进度层.strokeEnd = 进度;
    [CATransaction commit];
}

@end

@interface 主开关卡片 : UIView
@property (nonatomic, strong) UIView* 开关容器;
@property (nonatomic, strong) UIImageView* 勾选图标;
@property (nonatomic, strong) UILabel* 状态标签;
@property (nonatomic, assign) BOOL isOn;
@property (nonatomic, copy) void (^切换回调)(BOOL);
@end

@implementation 主开关卡片

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self 初始化];
    }
    return self;
}

- (void)初始化 {
    self.layer.cornerRadius = 16.0f;
    self.clipsToBounds = YES;
    
    CAGradientLayer* 渐变 = [CAGradientLayer layer];
    渐变.frame = CGRectMake(0, 0, 400, 80);
    渐变.colors = @[(id)粉色.CGColor, (id)紫色.CGColor];
    渐变.startPoint = CGPointMake(0, 0.5);
    渐变.endPoint = CGPointMake(1, 0.5);
    [self.layer addSublayer:渐变];
    
    UILabel* 图标 = [[UILabel alloc] initWithFrame:CGRectMake(16, 16, 30, 30)];
    图标.text = @"⏻";
    图标.font = [UIFont systemFontOfSize:18.0f weight:UIFontWeightBold];
    图标.textColor = [UIColor colorWithWhite:1.0f alpha:0.9f];
    [self addSubview:图标];
    
    UILabel* 标题 = [[UILabel alloc] initWithFrame:CGRectMake(40, 12, 100, 20)];
    标题.text = @"主开关";
    标题.font = [UIFont systemFontOfSize:12.0f weight:UIFontWeightMedium];
    标题.textColor = [UIColor colorWithWhite:1.0f alpha:0.8f];
    [self addSubview:标题];
    
    self.状态标签 = [[UILabel alloc] initWithFrame:CGRectMake(16, 38, 200, 30)];
    self.状态标签.text = @"HUD 已关闭";
    self.状态标签.font = [UIFont systemFontOfSize:22.0f weight:UIFontWeightBold];
    self.状态标签.textColor = 主文字色;
    [self addSubview:self.状态标签];
    
    self.开关容器 = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 50, 50)];
    self.开关容器.backgroundColor = [UIColor colorWithWhite:1.0f alpha:0.25f];
    self.开关容器.layer.cornerRadius = 25.0f;
    [self addSubview:self.开关容器];
    
    self.勾选图标 = [[UIImageView alloc] initWithFrame:CGRectMake(13, 13, 24, 24)];
    if (@available(iOS 13.0, *)) {
        self.勾选图标.image = [[UIImage systemImageNamed:@"checkmark" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:20 weight:UIImageSymbolWeightBold]] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }
    self.勾选图标.tintColor = 主文字色;
    self.勾选图标.alpha = 0;
    [self.开关容器 addSubview:self.勾选图标];
    
    UITapGestureRecognizer* 点击 = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(切换)];
    [self addGestureRecognizer:点击];
    
    self.isOn = [[按钮存储方法 初始化] 取按钮状态:按钮存储方法.绘图密钥];
    [self 更新状态:NO];
}

- (void)layoutSubviews {
    [super layoutSubviews];
    self.开关容器.frame = CGRectMake(self.bounds.size.width - 66, 15, 50, 50);
    for (CALayer* layer in self.layer.sublayers) {
        if ([layer isKindOfClass:[CAGradientLayer class]]) {
            layer.frame = self.bounds;
        }
    }
}

- (void)切换 {
    self.isOn = !self.isOn;
    [self 更新状态:YES];
    if (self.切换回调) {
        self.切换回调(self.isOn);
    }
}

- (void)更新状态:(BOOL)动画 {
    NSTimeInterval 时长 = 动画 ? 0.25f : 0;
    [UIView animateWithDuration:时长 animations:^{
        self.勾选图标.alpha = self.isOn ? 1.0f : 0;
        self.开关容器.backgroundColor = self.isOn ? [UIColor colorWithWhite:1.0f alpha:0.4f] : [UIColor colorWithWhite:1.0f alpha:0.25f];
    }];
    self.状态标签.text = self.isOn ? @"HUD 已启用" : @"HUD 已关闭";
}

@end


@interface 开关行视图 : UIView
@property (nonatomic, strong) UILabel* 标题标签;
@property (nonatomic, strong) UILabel* 状态标签;
@property (nonatomic, strong) UIView* 开关轨道;
@property (nonatomic, strong) UIView* 开关滑块;
@property (nonatomic, assign) BOOL isOn;
@property (nonatomic, copy) NSString* 密钥;
@property (nonatomic, copy) void (^切换回调)(BOOL);
@end

@implementation 开关行视图

- (instancetype)initWithIcon:(NSString*)图标名 标题:(NSString*)标题 密钥:(NSString*)密钥 {
    self = [super initWithFrame:CGRectZero];
    if (self) {
        self.密钥 = 密钥;
        self.backgroundColor = 卡片背景色;
        self.layer.cornerRadius = 12.0f;
        
        UIView* 图标背景 = [[UIView alloc] init];
        图标背景.translatesAutoresizingMaskIntoConstraints = NO;
        图标背景.backgroundColor = [紫色 colorWithAlphaComponent:0.15f];
        图标背景.layer.cornerRadius = 8.0f;
        [self addSubview:图标背景];
        
        UIImageView* 图标 = [[UIImageView alloc] init];
        图标.translatesAutoresizingMaskIntoConstraints = NO;
        if (@available(iOS 13.0, *)) {
            图标.image = [[UIImage systemImageNamed:图标名 withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:14 weight:UIImageSymbolWeightMedium]] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
        }
        图标.tintColor = 紫色;
        图标.contentMode = UIViewContentModeScaleAspectFit;
        [图标背景 addSubview:图标];
        
        self.标题标签 = [[UILabel alloc] init];
        self.标题标签.translatesAutoresizingMaskIntoConstraints = NO;
        self.标题标签.text = 标题;
        self.标题标签.font = [UIFont systemFontOfSize:15.0f weight:UIFontWeightMedium];
        self.标题标签.textColor = 主文字色;
        [self addSubview:self.标题标签];
        
        self.状态标签 = [[UILabel alloc] init];
        self.状态标签.translatesAutoresizingMaskIntoConstraints = NO;
        self.状态标签.text = @"已关闭";
        self.状态标签.font = [UIFont systemFontOfSize:11.0f weight:UIFontWeightRegular];
        self.状态标签.textColor = 副文字色;
        [self addSubview:self.状态标签];
        
        self.开关轨道 = [[UIView alloc] init];
        self.开关轨道.translatesAutoresizingMaskIntoConstraints = NO;
        self.开关轨道.backgroundColor = [UIColor colorWithWhite:0.25f alpha:1.0f];
        self.开关轨道.layer.cornerRadius = 15.0f;
        [self addSubview:self.开关轨道];
        
        self.开关滑块 = [[UIView alloc] initWithFrame:CGRectMake(2, 2, 26, 26)];
        self.开关滑块.backgroundColor = 主文字色;
        self.开关滑块.layer.cornerRadius = 13.0f;
        [self.开关轨道 addSubview:self.开关滑块];
        
        [NSLayoutConstraint activateConstraints:@[
            [图标背景.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:12],
            [图标背景.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [图标背景.widthAnchor constraintEqualToConstant:32],
            [图标背景.heightAnchor constraintEqualToConstant:32],
            
            [图标.centerXAnchor constraintEqualToAnchor:图标背景.centerXAnchor],
            [图标.centerYAnchor constraintEqualToAnchor:图标背景.centerYAnchor],
            [图标.widthAnchor constraintEqualToConstant:16],
            [图标.heightAnchor constraintEqualToConstant:16],
            
            [self.标题标签.leadingAnchor constraintEqualToAnchor:图标背景.trailingAnchor constant:10],
            [self.标题标签.topAnchor constraintEqualToAnchor:self.topAnchor constant:12],
            
            [self.状态标签.leadingAnchor constraintEqualToAnchor:self.标题标签.leadingAnchor],
            [self.状态标签.topAnchor constraintEqualToAnchor:self.标题标签.bottomAnchor constant:2],
            
            [self.开关轨道.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-12],
            [self.开关轨道.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [self.开关轨道.widthAnchor constraintEqualToConstant:50],
            [self.开关轨道.heightAnchor constraintEqualToConstant:30],
            
            [self.heightAnchor constraintEqualToConstant:56],
        ]];
        
        UITapGestureRecognizer* 点击 = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(切换)];
        [self addGestureRecognizer:点击];
        
        self.isOn = [[按钮存储方法 初始化] 取按钮状态:密钥];
        [self 更新状态:NO];
    }
    return self;
}

- (void)切换 {
    self.isOn = !self.isOn;
    [self 更新状态:YES];
    if (self.切换回调) {
        self.切换回调(self.isOn);
    }
}

- (void)更新状态:(BOOL)动画 {
    CGFloat 滑块X = self.isOn ? 22 : 2;
    UIColor* 轨道色 = self.isOn ? 红色开关 : [UIColor colorWithWhite:0.25f alpha:1.0f];
    NSString* 状态文字 = self.isOn ? @"已启用" : @"已关闭";
    UIColor* 状态色 = self.isOn ? 红色开关 : 副文字色;
    
    NSTimeInterval 时长 = 动画 ? 0.25f : 0;
    [UIView animateWithDuration:时长 delay:0 usingSpringWithDamping:0.7f initialSpringVelocity:0.5f options:0 animations:^{
        self.开关滑块.frame = CGRectMake(滑块X, 2, 26, 26);
        self.开关轨道.backgroundColor = 轨道色;
    } completion:nil];
    
    self.状态标签.text = 状态文字;
    self.状态标签.textColor = 状态色;
}

@end



@interface 菜单控制器 : UIViewController
@property (nonatomic, strong) UIScrollView* 滚动视图;
@property (nonatomic, strong) 圆形进度视图* 进度视图;
@property (nonatomic, assign) int 已启用数量;
@property (nonatomic, assign) int 总功能数;
@end

// ---- 让"默认开"的两个开关在 plist 里真的有值 ----
//   引擎侧读的是"没设=开"(射线/在场过滤都是这么写的), 面板读的是 boolValue(没设=关)。
//   不补这一步就会出现"面板显示已关闭、实际却在生效"的错位。
static void 补默认开关() {
    NSString* 路径 = @"/var/mobile/Library/Preferences/com.map.settings.plist";
    NSMutableDictionary* 配置 = [NSMutableDictionary dictionaryWithContentsOfFile:路径];
    if (!配置) 配置 = [NSMutableDictionary dictionary];
    BOOL 改了 = NO;
    if ([配置 objectForKey:@"射线密钥"] == nil) { [配置 setObject:@YES forKey:@"射线密钥"]; 改了 = YES; };
    if ([配置 objectForKey:@"在场密钥"] == nil) { [配置 setObject:@YES forKey:@"在场密钥"]; 改了 = YES; };
    if (改了) [配置 writeToFile:路径 atomically:YES];
};

@implementation 菜单控制器

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = 主题背景色;
    补默认开关();          // 射线 / 实体过滤 这两个"默认开"的键, 先落进 plist, 面板状态才跟实际一致
    self.已启用数量 = 0;
    self.总功能数 = 10;   // r35: 9 → 10(新增"玩家名字")
    
    self.滚动视图 = [[UIScrollView alloc] init];
    self.滚动视图.translatesAutoresizingMaskIntoConstraints = NO;
    self.滚动视图.showsVerticalScrollIndicator = NO;
    self.滚动视图.alwaysBounceVertical = YES;
    [self.view addSubview:self.滚动视图];
    
    [NSLayoutConstraint activateConstraints:@[
        [self.滚动视图.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.滚动视图.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.滚动视图.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.滚动视图.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
    ]];
    
    UIView* 内容 = [[UIView alloc] init];
    内容.translatesAutoresizingMaskIntoConstraints = NO;
    [self.滚动视图 addSubview:内容];
    
    [NSLayoutConstraint activateConstraints:@[
        [内容.topAnchor constraintEqualToAnchor:self.滚动视图.contentLayoutGuide.topAnchor],
        [内容.bottomAnchor constraintEqualToAnchor:self.滚动视图.contentLayoutGuide.bottomAnchor],
        [内容.leadingAnchor constraintEqualToAnchor:self.滚动视图.contentLayoutGuide.leadingAnchor],
        [内容.trailingAnchor constraintEqualToAnchor:self.滚动视图.contentLayoutGuide.trailingAnchor],
        [内容.widthAnchor constraintEqualToAnchor:self.滚动视图.frameLayoutGuide.widthAnchor],
    ]];
    
    UIView* 头部 = [self 创建头部];
    头部.translatesAutoresizingMaskIntoConstraints = NO;
    [内容 addSubview:头部];
    
    主开关卡片* 主开关 = [[主开关卡片 alloc] init];
    主开关.translatesAutoresizingMaskIntoConstraints = NO;
    [内容 addSubview:主开关];
    
    UIView* 已启用卡片 = [self 创建已启用卡片];
    已启用卡片.translatesAutoresizingMaskIntoConstraints = NO;
    [内容 addSubview:已启用卡片];
    
    UIView* 功能列表 = [self 创建功能列表];
    功能列表.translatesAutoresizingMaskIntoConstraints = NO;
    [内容 addSubview:功能列表];
    
    UIStackView* 开关列表 = [self 创建开关列表];
    开关列表.translatesAutoresizingMaskIntoConstraints = NO;
    [内容 addSubview:开关列表];
    
    [NSLayoutConstraint activateConstraints:@[
        [头部.topAnchor constraintEqualToAnchor:内容.topAnchor constant:16],
        [头部.leadingAnchor constraintEqualToAnchor:内容.leadingAnchor constant:16],
        [头部.trailingAnchor constraintEqualToAnchor:内容.trailingAnchor constant:-16],
        
        [主开关.topAnchor constraintEqualToAnchor:头部.bottomAnchor constant:20],
        [主开关.leadingAnchor constraintEqualToAnchor:内容.leadingAnchor constant:16],
        [主开关.trailingAnchor constraintEqualToAnchor:内容.trailingAnchor constant:-16],
        [主开关.heightAnchor constraintEqualToConstant:80],
        
        [已启用卡片.topAnchor constraintEqualToAnchor:主开关.bottomAnchor constant:16],
        [已启用卡片.leadingAnchor constraintEqualToAnchor:内容.leadingAnchor constant:16],
        [已启用卡片.trailingAnchor constraintEqualToAnchor:内容.trailingAnchor constant:-16],
        [已启用卡片.heightAnchor constraintEqualToConstant:100],
        
        [功能列表.topAnchor constraintEqualToAnchor:已启用卡片.bottomAnchor constant:20],
        [功能列表.leadingAnchor constraintEqualToAnchor:内容.leadingAnchor constant:16],
        [功能列表.trailingAnchor constraintEqualToAnchor:内容.trailingAnchor constant:-16],
        
        [开关列表.topAnchor constraintEqualToAnchor:功能列表.bottomAnchor constant:16],
        [开关列表.leadingAnchor constraintEqualToAnchor:内容.leadingAnchor constant:16],
        [开关列表.trailingAnchor constraintEqualToAnchor:内容.trailingAnchor constant:-16],
        [开关列表.bottomAnchor constraintEqualToAnchor:内容.bottomAnchor constant:-30],
    ]];
    
    __weak typeof(self) weakSelf = self;
    主开关.切换回调 = ^(BOOL 状态) {
        //if (sub_m889283YhOXNKlS0tZExbRameuKLu09M0FzgpOosYk3HJw0c1b0dpt) {
            [[按钮存储方法 初始化] 设置按钮状态:状态 密钥:按钮存储方法.绘图密钥];
            开启或关闭HUD(状态);
            [weakSelf 更新计数];
       // }
    };
    
    [self 计算初始启用数量];
}

- (UIView*)创建头部 {
    UIView* 容器 = [[UIView alloc] init];
    
    UIView* 头像容器 = [[UIView alloc] init];
    头像容器.translatesAutoresizingMaskIntoConstraints = NO;
    头像容器.backgroundColor = 紫色;
    头像容器.layer.cornerRadius = 22.0f;
    [容器 addSubview:头像容器];
    
    UILabel* 头像文字 = [[UILabel alloc] init];
    头像文字.translatesAutoresizingMaskIntoConstraints = NO;
    头像文字.text = @"S";
    头像文字.font = [UIFont systemFontOfSize:20.0f weight:UIFontWeightBold];
    头像文字.textColor = 主文字色;
    [头像容器 addSubview:头像文字];
    
    UILabel* 标题 = [[UILabel alloc] init];
    标题.translatesAutoresizingMaskIntoConstraints = NO;
    标题.text = @"Superman";
    标题.font = [UIFont systemFontOfSize:18.0f weight:UIFontWeightBold];
    标题.textColor = 主文字色;
    [容器 addSubview:标题];
    
    UILabel* 副标题 = [[UILabel alloc] init];
    副标题.translatesAutoresizingMaskIntoConstraints = NO;
    副标题.text = @"控制中心";
    副标题.font = [UIFont systemFontOfSize:12.0f weight:UIFontWeightRegular];
    副标题.textColor = 副文字色;
    [容器 addSubview:副标题];
    
    UIButton* 设置按钮 = [UIButton buttonWithType:UIButtonTypeCustom];
    设置按钮.translatesAutoresizingMaskIntoConstraints = NO;
    if (@available(iOS 13.0, *)) {
        [设置按钮 setImage:[[UIImage systemImageNamed:@"gearshape.fill" withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:18 weight:UIImageSymbolWeightMedium]] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    }
    设置按钮.tintColor = 副文字色;
    [容器 addSubview:设置按钮];
    
    [NSLayoutConstraint activateConstraints:@[
        [头像容器.leadingAnchor constraintEqualToAnchor:容器.leadingAnchor],
        [头像容器.topAnchor constraintEqualToAnchor:容器.topAnchor],
        [头像容器.widthAnchor constraintEqualToConstant:44],
        [头像容器.heightAnchor constraintEqualToConstant:44],
        
        [头像文字.centerXAnchor constraintEqualToAnchor:头像容器.centerXAnchor],
        [头像文字.centerYAnchor constraintEqualToAnchor:头像容器.centerYAnchor],
        
        [标题.leadingAnchor constraintEqualToAnchor:头像容器.trailingAnchor constant:12],
        [标题.topAnchor constraintEqualToAnchor:容器.topAnchor constant:4],
        
        [副标题.leadingAnchor constraintEqualToAnchor:标题.leadingAnchor],
        [副标题.topAnchor constraintEqualToAnchor:标题.bottomAnchor constant:2],
        
        [设置按钮.trailingAnchor constraintEqualToAnchor:容器.trailingAnchor],
        [设置按钮.centerYAnchor constraintEqualToAnchor:头像容器.centerYAnchor],
        [设置按钮.widthAnchor constraintEqualToConstant:44],
        [设置按钮.heightAnchor constraintEqualToConstant:44],
        
        [容器.heightAnchor constraintEqualToConstant:50],
    ]];
    
    return 容器;
}

- (UIView*)创建已启用卡片 {
    UIView* 容器 = [[UIView alloc] init];
    容器.backgroundColor = 卡片背景色;
    容器.layer.cornerRadius = 16.0f;
    
    UIView* 点 = [[UIView alloc] init];
    点.translatesAutoresizingMaskIntoConstraints = NO;
    点.backgroundColor = 紫色;
    点.layer.cornerRadius = 4.0f;
    [容器 addSubview:点];
    
    UILabel* 标签 = [[UILabel alloc] init];
    标签.translatesAutoresizingMaskIntoConstraints = NO;
    标签.text = @"已启用";
    标签.font = [UIFont systemFontOfSize:13.0f weight:UIFontWeightMedium];
    标签.textColor = 副文字色;
    [容器 addSubview:标签];
    
    self.进度视图 = [[圆形进度视图 alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
    self.进度视图.translatesAutoresizingMaskIntoConstraints = NO;
    [容器 addSubview:self.进度视图];
    
    [NSLayoutConstraint activateConstraints:@[
        [点.leadingAnchor constraintEqualToAnchor:容器.leadingAnchor constant:16],
        [点.topAnchor constraintEqualToAnchor:容器.topAnchor constant:16],
        [点.widthAnchor constraintEqualToConstant:8],
        [点.heightAnchor constraintEqualToConstant:8],
        
        [标签.leadingAnchor constraintEqualToAnchor:点.trailingAnchor constant:8],
        [标签.centerYAnchor constraintEqualToAnchor:点.centerYAnchor],
        
        [self.进度视图.trailingAnchor constraintEqualToAnchor:容器.trailingAnchor constant:-16],
        [self.进度视图.centerYAnchor constraintEqualToAnchor:容器.centerYAnchor],
        [self.进度视图.widthAnchor constraintEqualToConstant:100],
        [self.进度视图.heightAnchor constraintEqualToConstant:100],
    ]];
    
    return 容器;
}


- (UIView*)创建功能列表 {
    UIView* 容器 = [[UIView alloc] init];
    
    UIView* 线 = [[UIView alloc] init];
    线.translatesAutoresizingMaskIntoConstraints = NO;
    线.backgroundColor = 粉色;
    线.layer.cornerRadius = 2.0f;
    [容器 addSubview:线];
    
    UILabel* 标题 = [[UILabel alloc] init];
    标题.translatesAutoresizingMaskIntoConstraints = NO;
    标题.text = @"功能列表";
    标题.font = [UIFont systemFontOfSize:16.0f weight:UIFontWeightBold];
    标题.textColor = 主文字色;
    [容器 addSubview:标题];
    
    UILabel* 副标题 = [[UILabel alloc] init];
    副标题.translatesAutoresizingMaskIntoConstraints = NO;
    副标题.text = @"一键开关与状态";
    副标题.font = [UIFont systemFontOfSize:11.0f weight:UIFontWeightRegular];
    副标题.textColor = 副文字色;
    [容器 addSubview:副标题];
    
    [NSLayoutConstraint activateConstraints:@[
        [线.leadingAnchor constraintEqualToAnchor:容器.leadingAnchor],
        [线.topAnchor constraintEqualToAnchor:容器.topAnchor],
        [线.widthAnchor constraintEqualToConstant:4],
        [线.heightAnchor constraintEqualToConstant:36],
        
        [标题.leadingAnchor constraintEqualToAnchor:线.trailingAnchor constant:10],
        [标题.topAnchor constraintEqualToAnchor:容器.topAnchor],
        
        [副标题.leadingAnchor constraintEqualToAnchor:标题.leadingAnchor],
        [副标题.topAnchor constraintEqualToAnchor:标题.bottomAnchor constant:2],
        
        [容器.heightAnchor constraintEqualToConstant:50],
    ]];
    
    return 容器;
}

- (UIStackView*)创建开关列表 {
    UIStackView* 列表 = [[UIStackView alloc] init];
    列表.axis = UILayoutConstraintAxisVertical;
    列表.spacing = 10.0f;
    
    NSArray* 配置 = @[
        @{@"图标": @"rectangle.dashed", @"标题": @"方框透视", @"密钥": 按钮存储方法.方框密钥},
        @{@"图标": @"figure.stand", @"标题": @"骨骼透视", @"密钥": 按钮存储方法.骨骼密钥},
        @{@"图标": @"person.fill.questionmark", @"标题": @"人机显示", @"密钥": 按钮存储方法.人机密钥},
        @{@"图标": @"ruler.fill", @"标题": @"显示距离", @"密钥": 按钮存储方法.距离密钥},
        @{@"图标": @"scope", @"标题": @"无后坐力", @"密钥": 按钮存储方法.后坐密钥},
        @{@"图标": @"arrow.down.to.line", @"标题": @"射线", @"密钥": 按钮存储方法.射线密钥},
        @{@"图标": @"shield.lefthalf.filled", @"标题": @"头甲显示", @"密钥": 按钮存储方法.头甲密钥},
        @{@"图标": @"textformat.abc", @"标题": @"玩家名字", @"密钥": 按钮存储方法.名字密钥},
        @{@"图标": @"eye.slash.fill", @"标题": @"实体过滤", @"密钥": 按钮存储方法.在场密钥},
        @{@"图标": @"video.fill", @"标题": @"直播隐藏", @"密钥": 按钮存储方法.过直播密钥},
    ];
    
    __weak typeof(self) weakSelf = self;
    for (NSDictionary* 项 in 配置) {
        开关行视图* 行 = [[开关行视图 alloc] initWithIcon:项[@"图标"] 标题:项[@"标题"] 密钥:项[@"密钥"]];
        行.translatesAutoresizingMaskIntoConstraints = NO;
        行.切换回调 = ^(BOOL 状态) {
          //  if (sub_m889283YhOXNKlS0tZExbRameuKLu09M0FzgpOosYk3HJw0c1b0dpt) {
                [[按钮存储方法 初始化] 设置按钮状态:状态 密钥:项[@"密钥"]];
                [weakSelf 更新计数];
          //  }
        };
        [列表 addArrangedSubview:行];
    }
    
    return 列表;
}

- (void)计算初始启用数量 {
    self.已启用数量 = 0;
    NSArray* 所有密钥 = @[
        按钮存储方法.方框密钥, 按钮存储方法.骨骼密钥, 按钮存储方法.人机密钥,
        按钮存储方法.距离密钥, 按钮存储方法.后坐密钥, 按钮存储方法.过直播密钥,
        按钮存储方法.射线密钥, 按钮存储方法.在场密钥,
        按钮存储方法.头甲密钥, 按钮存储方法.名字密钥
    ];
    
    for (NSString* 密钥 in 所有密钥) {
        if ([[按钮存储方法 初始化] 取按钮状态:密钥]) {
            self.已启用数量++;
        }
    }
    
    [self.进度视图 设置进度:self.已启用数量 总数:self.总功能数];
}

- (void)更新计数 {
    [self 计算初始启用数量];
}

@end

@interface 加载控制器 : UIViewController
@property (nonatomic, strong) CAGradientLayer* 进度渐变;
@property (nonatomic, strong) UILabel* 状态标签;
@property (nonatomic, strong) UIButton* 进入按钮;
@end

@implementation 加载控制器

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = 主题背景色;
    
    UIView* logoContainer = [[UIView alloc] init];
    logoContainer.translatesAutoresizingMaskIntoConstraints = NO;
    logoContainer.layer.shadowColor = 紫色.CGColor;
    logoContainer.layer.shadowOffset = CGSizeZero;
    logoContainer.layer.shadowOpacity = 0.6f;
    logoContainer.layer.shadowRadius = 25.0f;
    [self.view addSubview:logoContainer];
    
    UIImageView* logo = [[UIImageView alloc] init];
    logo.translatesAutoresizingMaskIntoConstraints = NO;
    logo.contentMode = UIViewContentModeScaleAspectFill;
    logo.layer.cornerRadius = 30.0f;
    logo.clipsToBounds = YES;
    NSString* 图片路径 = [[NSBundle mainBundle] pathForResource:@"三角洲行动" ofType:@"jpg"];
    logo.image = [UIImage imageWithContentsOfFile:图片路径];
    [logoContainer addSubview:logo];
    
    UILabel* 标题 = [[UILabel alloc] init];
    标题.translatesAutoresizingMaskIntoConstraints = NO;
    标题.text = @"Superman";
    标题.font = [UIFont systemFontOfSize:28.0f weight:UIFontWeightBold];
    标题.textColor = 主文字色;
    [self.view addSubview:标题];
    
    UIView* 加载容器 = [[UIView alloc] init];
    加载容器.translatesAutoresizingMaskIntoConstraints = NO;
    加载容器.backgroundColor = [UIColor colorWithWhite:0.2f alpha:1.0f];
    加载容器.layer.cornerRadius = 4.0f;
    加载容器.clipsToBounds = YES;
    [self.view addSubview:加载容器];
    
    self.进度渐变 = [CAGradientLayer layer];
    self.进度渐变.frame = CGRectMake(0, 0, 0, 8);
    self.进度渐变.cornerRadius = 4.0f;
    self.进度渐变.colors = @[(id)粉色.CGColor, (id)紫色.CGColor];
    self.进度渐变.startPoint = CGPointMake(0, 0.5);
    self.进度渐变.endPoint = CGPointMake(1, 0.5);
    [加载容器.layer addSublayer:self.进度渐变];
    
    self.状态标签 = [[UILabel alloc] init];
    self.状态标签.translatesAutoresizingMaskIntoConstraints = NO;
    self.状态标签.text = @"正在初始化...";
    self.状态标签.font = [UIFont systemFontOfSize:14.0f weight:UIFontWeightMedium];
    self.状态标签.textColor = 副文字色;
    [self.view addSubview:self.状态标签];
    
    self.进入按钮 = [UIButton buttonWithType:UIButtonTypeCustom];
    self.进入按钮.translatesAutoresizingMaskIntoConstraints = NO;
    [self.进入按钮 setTitle:@"进入应用" forState:UIControlStateNormal];
    [self.进入按钮 setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    self.进入按钮.titleLabel.font = [UIFont systemFontOfSize:17.0f weight:UIFontWeightSemibold];
    self.进入按钮.layer.cornerRadius = 14.0f;
    self.进入按钮.clipsToBounds = YES;
    self.进入按钮.alpha = 0;
    self.进入按钮.transform = CGAffineTransformMakeTranslation(0, 20);
    [self.进入按钮 addTarget:self action:@selector(进入菜单) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:self.进入按钮];
    
    CAGradientLayer* 按钮渐变 = [CAGradientLayer layer];
    按钮渐变.frame = CGRectMake(0, 0, 220, 56);
    按钮渐变.cornerRadius = 14.0f;
    按钮渐变.colors = @[(id)粉色.CGColor, (id)紫色.CGColor];
    按钮渐变.startPoint = CGPointMake(0, 0.5);
    按钮渐变.endPoint = CGPointMake(1, 0.5);
    [self.进入按钮.layer insertSublayer:按钮渐变 atIndex:0];
    
    [NSLayoutConstraint activateConstraints:@[
        [logoContainer.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [logoContainer.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor constant:-100],
        [logoContainer.widthAnchor constraintEqualToConstant:100],
        [logoContainer.heightAnchor constraintEqualToConstant:100],
        
        [logo.topAnchor constraintEqualToAnchor:logoContainer.topAnchor],
        [logo.leadingAnchor constraintEqualToAnchor:logoContainer.leadingAnchor],
        [logo.trailingAnchor constraintEqualToAnchor:logoContainer.trailingAnchor],
        [logo.bottomAnchor constraintEqualToAnchor:logoContainer.bottomAnchor],
        
        [标题.topAnchor constraintEqualToAnchor:logoContainer.bottomAnchor constant:20],
        [标题.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        
        [加载容器.topAnchor constraintEqualToAnchor:标题.bottomAnchor constant:40],
        [加载容器.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [加载容器.widthAnchor constraintEqualToConstant:220],
        [加载容器.heightAnchor constraintEqualToConstant:8],
        
        [self.状态标签.topAnchor constraintEqualToAnchor:加载容器.bottomAnchor constant:16],
        [self.状态标签.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        
        [self.进入按钮.topAnchor constraintEqualToAnchor:self.状态标签.bottomAnchor constant:30],
        [self.进入按钮.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [self.进入按钮.widthAnchor constraintEqualToConstant:220],
        [self.进入按钮.heightAnchor constraintEqualToConstant:56],
    ]];
    
    [self 开始加载动画];
}

- (void)开始加载动画 {
    [UIView animateWithDuration:1.5f delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        self.进度渐变.frame = CGRectMake(0, 0, 220, 8);
    } completion:^(BOOL finished) {
        self.状态标签.text = @"准备就绪";
        [UIView animateWithDuration:0.5f delay:0.2f usingSpringWithDamping:0.8f initialSpringVelocity:0.5f options:0 animations:^{
            self.进入按钮.alpha = 1;
            self.进入按钮.transform = CGAffineTransformIdentity;
        } completion:nil];
    }];
}

- (void)进入菜单 {
   // if (sub_m889283YhOXNKlS0tZExbRameuKLu09M0FzgpOosYk3HJw0c1b0dpt) {
        [UIView animateWithDuration:0.1f animations:^{
            self.进入按钮.transform = CGAffineTransformMakeScale(0.95f, 0.95f);
        } completion:^(BOOL finished) {
            [UIView animateWithDuration:0.1f animations:^{
                self.进入按钮.transform = CGAffineTransformIdentity;
            } completion:^(BOOL finished) {
                菜单控制器* 菜单 = [[菜单控制器 alloc] init];
                菜单.modalPresentationStyle = UIModalPresentationFullScreen;
                菜单.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
                [self presentViewController:菜单 animated:YES completion:nil];
            }];
        }];
    }
//}

@end

@interface 应用挂载方法 : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* UIWindow;
@end

@implementation 应用挂载方法

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey,id> *)launchOptions {
    [[NSProcessInfo processInfo] setProcessName:@"com.apple.Maps"];
    
    self.UIWindow = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.UIWindow.backgroundColor = UIColor.clearColor;
    self.UIWindow.rootViewController = [[加载控制器 alloc] init];
    [self.UIWindow makeKeyAndVisible];
   
    本地通讯伪装();
    return YES;
}

@end
