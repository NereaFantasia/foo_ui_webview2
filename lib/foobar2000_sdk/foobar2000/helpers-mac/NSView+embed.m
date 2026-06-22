#import "NSView+embed.h"

#if 0
@implementation NSView (embed)

- (void)embedInView:(NSView *)superview {
    NSViewEmbed( superview, self );
}
- (void)embedInViewV2:(NSView *)superview {
    NSViewEmbedConstraints( superview, self );
}

@end
#endif

void NSViewEmbed( NSView * superview, NSView * childview ) {
    if ( childview == nil || superview == nil ) return;
    childview.autoresizingMask = NSViewHeightSizable | NSViewWidthSizable;
    childview.frame = superview.bounds;
    [superview addSubview: childview];
}

void NSViewEmbedConstraints( NSView * superview, NSView * childview ) {
    if ( childview == nil || superview == nil ) return;
    childview.autoresizingMask = 0;
    [superview addSubview: childview];
    NSMutableArray <NSLayoutConstraint * > * constraints = [NSMutableArray arrayWithCapacity: 4];
    static const NSLayoutAttribute params[4] = { NSLayoutAttributeLeft, NSLayoutAttributeRight, NSLayoutAttributeTop, NSLayoutAttributeBottom };
    for( unsigned i = 0; i < 4; ++ i) {
        NSLayoutAttribute a = params[i];
        [constraints addObject: [NSLayoutConstraint constraintWithItem: childview attribute:a relatedBy:NSLayoutRelationEqual toItem:superview attribute:a multiplier:1 constant:0]];
    }

    [superview addConstraints: constraints];
}
