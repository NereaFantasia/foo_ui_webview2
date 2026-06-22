#import "NSEvent+ppstuff.h"

@implementation NSEvent (ppstuff)

- (BOOL)pp_isKeyDown {
    return self.type == NSEventTypeKeyDown;
}
- (BOOL)pp_isCmdKeyDown {
    return self.pp_isKeyDown && self.pp_modifierFlagsDI == NSEventModifierFlagCommand;
}
- (BOOL)pp_isShiftKeyDown {
    return self.pp_isKeyDown && self.pp_modifierFlagsDI == NSEventModifierFlagShift;
}
- (NSEventModifierFlags)pp_modifierFlagsDI {
    return self.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
}
@end
