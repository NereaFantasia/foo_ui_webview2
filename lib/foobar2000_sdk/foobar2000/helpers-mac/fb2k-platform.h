#pragma once

#import <SDK/foobar2000.h>
#import <Cocoa/Cocoa.h>
#import <SDK/apple-tools.h>

namespace fb2k {
    // 2.25.1 semantic change: strToPlatform() family of function never returns null as it's used all over the place without retval checks.
    // Attempts to recover if passed string contains bad UTF-8
    // If you want null on bad UTF-8, use NSString methods directly, or version with returnIfError arg.
    NSString * strToPlatform( const char * );
    NSString * strToPlatform( const char * , size_t );
    NSString * strToPlatform( stringRef );
    NSString * strToPlatform( const char *, NSString * returnIfError );

    stringRef strFromPlatform( NSString * );
    

    stringRef urlFromPlatform( id url /* can be NSString or NSURL */ );
    NSURL * urlToPlatform(const char * arg);


    typedef NSImage* platformImage_t;
    platformImage_t imageToPlatform( fb2k::objRef );
    

    // These two functions do the same, openWebBrowser() was added for compatiblity with fb2k mobile
    void openWebBrowser(const char * URL);
    void openURL( const char * URL);

    NSFont * fontFromParams(NSDictionary<NSString*, NSString*> *, NSFont * base = nil);
    BOOL testFontParams(NSDictionary<NSString*, NSString*> *);
    CGFloat tableViewRowHeightForFont( NSFont * );
    void tableViewPrepareForFont( NSTableView * tableView, NSFont * font );

    NSWindow * mainWindow();
    void showWindow(NSWindow*);

    void popupMessage(id parent, const char * msg, const char * title = "Information");

    NSData * wrapData( memBlockRef const & arg );

    imageRef wrapNSImage(NSImage*);
}

namespace pfc {
    string8 strFromPlatform(NSString*);
    NSString * strToPlatform( const char * );
    NSString * strToPlatform(string8 const&);
    string8 strFromPlatform(CFStringRef);
}
