#import "NSView+ppsubviews.h"

#if 0
@implementation NSView (ppsubviews)

- (NSView*) recurFindSubViewOfClass: (Class) cls identifier: (NSString*) identifier {
    return NSViewFindSubViewRecursive ( self, cls, identifier );
}

- (NSView*) findSubViewOfClass: (Class) cls identifier: (NSString*) identifier {
    return NSViewFindSubView( self, cls, identifier );
}
- (NSView *)findSubViewOfClass:(Class)cls {
    return NSViewFindSubView( self, cls, nil );
}
- (NSButton *)findButton {
    return (NSButton*) [self findSubViewOfClass: [NSButton class]];
}
- (NSTextView *)findTextView {
    return (NSTextView*) [self findSubViewOfClass: [NSTextView class]];
}
- (NSTextField *) findTextField {
    return (NSTextField*) [self findSubViewOfClass: [NSTextField class]];
}
- (NSImageView*) findImageView {
    return (NSImageView*) [self findSubViewOfClass: [NSImageView class]];
}
@end
#endif

__kindof NSView * NSViewFindSubView( NSView * parent, Class cls, NSUserInterfaceItemIdentifier identifier ) {
    for (__kindof NSView * v in parent.subviews) {
        if ( (cls == nil || [v isKindOfClass: cls]) && ( identifier == nil || [ v.identifier isEqualToString: identifier ] ) ) {
            return v;
        }
    }
    return nil;
}

__kindof NSView * NSViewFindSubViewRecursive( NSView * parent, Class cls, NSUserInterfaceItemIdentifier identifier ) {
    @autoreleasepool {
        NSMutableArray<__kindof NSView*> * arrAll = [NSMutableArray arrayWithArray: parent.subviews];

        for ( NSUInteger w = 0; w < arrAll.count; ++ w ) {
            __kindof NSView * thisView = arrAll[w];
            
            if ( (cls == nil || [thisView isKindOfClass: cls]) && ( identifier == nil || [thisView.identifier isEqualToString: identifier] ) ) {
                return thisView;
            }
            [arrAll addObjectsFromArray: thisView.subviews];
            
            if ( w >= 200 ) {
                [arrAll removeObjectsInRange: NSMakeRange(0, w) ];
                w = 0;
            }
        }
        return nil;
    }
}
