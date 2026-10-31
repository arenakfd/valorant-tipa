#import "FBSOrientationUpdate.h"
#import "FBSOrientationObserver.h"

#import "过直播/过直播.h"

@interface HUD控制器 : UIViewController <MTKViewDelegate>
@property (nonatomic, strong) 过直播窗口* 过直播窗口;
@property (nonatomic, strong) MTKView* MTKView;
@property (nonatomic, strong) id <MTLCommandQueue> MTLCommandQueue;
@end
