#import <Cocoa/Cocoa.h>

@interface NSMenu (ppaddons)

- (NSMenuItem*) pp_addItemWithTitle: (NSString*) title action: (SEL) action target: (id) target;
- (void) pp_addSeparator;
- (NSMenuItem*) pp_addSubMenu: (NSMenu*) menu withTitle: (NSString*) title;
- (void) pp_popUpForView:(NSView *)view;

@end
