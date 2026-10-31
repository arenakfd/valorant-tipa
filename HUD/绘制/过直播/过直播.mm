#import "过直播.h"

@implementation 过直播窗口

- (instancetype) init {
    self = [super init];
    if (self) {
        [self 过直播方法];
    };
    return self;
};

- (instancetype) initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self 过直播方法];
    };
    return self;
};

- (instancetype) initWithCoder:(NSCoder *)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self 过直播方法];
    };
    return self;
};

- (void)layoutSubviews {
    [super layoutSubviews];
    self.UITextField.frame = self.bounds;
    self.UIView.frame = self.bounds;
    self.UIView.transform = self.transform;
    for (UIView* subview in self._UIView.subviews) {
        subview.frame = self._UIView.bounds;
    };
}

- (void) 过直播方法 {
    [self addSubview:self._UITextField];
    self._UITextField.subviews.firstObject.userInteractionEnabled = YES;
    [self._UITextField.subviews.firstObject addSubview:self._UIView];
};

- (void) addSubview:(UIView *)view {
    [super addSubview:view];
    if (self._UITextField != view) {
        [self._UIView addSubview:view];
    };
};

- (UITextField*) _UITextField {
    if (!_UITextField) {
        _UITextField = [[UITextField alloc] init];
        _UITextField.secureTextEntry = YES;
        _UITextField.userInteractionEnabled = NO;
    };
    return _UITextField;
};

- (UIView*) _UIView {
    if (!_UIView) {
        _UIView = [[UIView alloc] init];
    };
    return _UIView;
};
 
@end
