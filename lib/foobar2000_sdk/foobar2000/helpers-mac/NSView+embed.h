#import <Cocoa/Cocoa.h>

#if 0
@interface NSView (embed)
- (void) embedInView: (NSView*) superview; // uses autoresizingMask
- (void) embedInViewV2: (NSView*) superview; // adds autolayout constraints
@end
#endif

#ifdef __cplusplus
extern "C" {
#endif

void NSViewEmbed( NSView * superview, NSView * childview );
void NSViewEmbedConstraints( NSView * superview, NSView * childview );

#ifdef __cplusplus
} // extern "C"
#endif
