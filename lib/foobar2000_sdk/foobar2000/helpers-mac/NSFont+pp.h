//
//  NSFont+pp.h
//  foobar2000
//
//  Created by Piotr Pawłowski on 30/06/2024.
//  Copyright © 2024 Piotr Pawłowski. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface NSFont (pp)
// Recommended font for all text data, because default fixed-width digit behavior is horribly annoying
// PROTIP if NSTableView is ignoring font alterations, make sure row height is set explicitly
@property (class, readonly) NSFont* pp_monospacedDigitFont;
@property (class, readonly) NSFont* pp_monospacedFont;
@end
