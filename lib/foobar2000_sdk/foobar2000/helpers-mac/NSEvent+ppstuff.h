#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSEvent (ppstuff)

@property (readonly) BOOL pp_isKeyDown;
@property (readonly) BOOL pp_isCmdKeyDown;
@property (readonly) BOOL pp_isShiftKeyDown;
@property (readonly) NSEventModifierFlags pp_modifierFlagsDI;
@end

NS_ASSUME_NONNULL_END
