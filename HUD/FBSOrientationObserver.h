
@interface FBSOrientationObserver : NSObject
- (long long)activeInterfaceOrientation;
- (void)activeInterfaceOrientationWithCompletion:(id)ID;
- (void)invalidate;
- (void)setHandler:(id)ID;
- (id)handler;
@end
