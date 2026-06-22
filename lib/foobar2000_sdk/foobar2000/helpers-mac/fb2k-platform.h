#pragma once

#import <SDK/foobar2000.h>
#import <Cocoa/Cocoa.h>

namespace fb2k {
    // May return null on bad input.
    NSString * strToPlatform( const char * );
    // May return null on bad input.
    NSString * strToPlatform( const char * , size_t );
    // May return null on bad input.
    NSString * strToPlatform( stringRef );
    // Never returns null - returns passed string in case of failure
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
    
}

namespace pfc {
    string8 strFromPlatform(NSString*);
    NSString * strToPlatform( const char * );
    NSString * strToPlatform(string8 const&);
    string8 strFromPlatform(CFStringRef);
}
