#import "NSMenu+ppaddons.h"

@implementation NSMenu (ppaddons)

- (NSMenuItem*) pp_addItemWithTitle: (NSString*) title action: (SEL) action target: (id) target {
    NSMenuItem * item = [[NSMenuItem alloc] initWithTitle: title action: action keyEquivalent: @""];
    item.target = target;
    [self addItem: item];
    return item;
}

- (void) pp_addSeparator {
    [self addItem: [NSMenuItem separatorItem]];
}

- (NSMenuItem*) pp_addSubMenu: (NSMenu*) menu withTitle: (NSString*) title {
    NSMenuItem * item = [[NSMenuItem alloc] init];
    item.title = title;
    item.submenu = menu;
    [self addItem: item];
    return item;
}

- (void)pp_popUpForView:(NSView *)view {
    BOOL pullsDown = YES;
    NSMenu *popMenu = [self copy];
    NSRect frame = [view frame];
    frame.origin.x = 0.0;
    frame.origin.y = 0.0;
    
    if (pullsDown) [popMenu insertItemWithTitle:@"" action:NULL keyEquivalent:@"" atIndex:0];
    
    NSPopUpButtonCell *popUpButtonCell = [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:pullsDown];
    [popUpButtonCell setMenu:popMenu];
    if (!pullsDown) [popUpButtonCell selectItem:nil];
    [popUpButtonCell performClickWithFrame:frame inView:view];
}

@end
