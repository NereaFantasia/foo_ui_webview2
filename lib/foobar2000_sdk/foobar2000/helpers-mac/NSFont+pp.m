//
//  NSFont+pp.m
//  foobar2000
//
//  Created by Piotr Pawłowski on 30/06/2024.
//  Copyright © 2024 Piotr Pawłowski. All rights reserved.
//

#import "NSFont+pp.h"

static NSFont * g_monospacedDigitFont;
static NSFont * g_monospacedFont;

@implementation NSFont (pp)
+ (NSFont*) pp_monospacedDigitFont {
    assert( NSThread.isMainThread );
    if ( g_monospacedDigitFont == nil ) g_monospacedDigitFont = [NSFont monospacedDigitSystemFontOfSize: NSFont.systemFontSize weight: NSFontWeightRegular];
    return g_monospacedDigitFont;
}
+ (NSFont*) pp_monospacedFont {
    assert( NSThread.isMainThread );
    if ( g_monospacedFont == nil ) {
        g_monospacedFont = [NSFont monospacedSystemFontOfSize: NSFont.systemFontSize weight: NSFontWeightRegular];
    }
    return g_monospacedFont;
}
@end
