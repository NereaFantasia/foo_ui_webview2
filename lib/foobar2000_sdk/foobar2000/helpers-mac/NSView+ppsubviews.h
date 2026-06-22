#import <Cocoa/Cocoa.h>

#if 0
@interface NSView (ppsubviews)
- (NSView*) recurFindSubViewOfClass: (Class) cls identifier: (NSString*) identifier;
- (NSView*) findSubViewOfClass: (Class) cls identifier: (NSString*) identifier;
- (NSView*) findSubViewOfClass: (Class) cls;
- (NSButton*) findButton;
- (NSTextView*) findTextView;
- (NSTextField*) findTextField;
- (NSImageView*) findImageView;
@end
#endif

#ifdef __cplusplus
extern "C" {
#endif

__kindof NSView * NSViewFindSubView( NSView * parent, Class clsOrNull, NSUserInterfaceItemIdentifier idOrNull );
__kindof NSView * NSViewFindSubViewRecursive( NSView * parent, Class clsOrNull, NSUserInterfaceItemIdentifier idOrNull );

#ifdef __cplusplus
} // extern "C"
#endif
