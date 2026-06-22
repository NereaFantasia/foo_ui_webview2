#include "fb2k-platform.h"
#include "NSFont+pp.h"

namespace fb2k {
    NSString * strToPlatform( const char * s, size_t len ) {
        pfc::string8 temp( s, len );
        return strToPlatform( temp );
    }
    NSString * strToPlatform( const char * arg, NSString * returnIfError ) {
        if ( arg ) @try {
            NSString * ret = [NSString stringWithUTF8String: arg];
            if ( ret ) return ret;
        } @catch(NSException *) {}
        return returnIfError;
    }
    NSString * strToPlatform( const char * s ) {
        return [NSString stringWithUTF8String: s];
    }
    NSString * strToPlatform( stringRef s ) {
        if ( s.is_empty( ) ) return nil;
        return strToPlatform( s->c_str() );
    }
    stringRef strFromPlatform( NSString * s ) {
        return makeString( s.UTF8String );
    }

    stringRef urlFromPlatform( id obj ) {
        if ( [obj isKindOfClass: [NSURL class] ] ) {
            NSURL * URL = obj;
            obj = URL.absoluteString;
            if ([obj hasPrefix:@"file://"])
                obj = URL.path;
        }
        if ( [obj respondsToSelector: @selector(UTF8String)]) {
            pfc::string8 temp;
            filesystem::g_get_canonical_path( [obj UTF8String], temp );
            return makeString( temp );
        }
        return nullptr;
    }

    NSURL * urlToPlatform(const char * arg) {
        pfc::string8 native;
        if (filesystem::g_get_native_path(arg, native)) {
            return [NSURL fileURLWithPath: [NSString stringWithUTF8String: native ] ];
        }
        return [NSURL URLWithString: [NSString stringWithUTF8String: arg]];
    }

    void openURL( const char * URL ) {
        @autoreleasepool {
            [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: [NSString stringWithUTF8String: URL]]];
        }
    }
    void openWebBrowser( const char * URL) {
        openURL(URL);
    }

    NSImage * imageToPlatform( fb2k::objRef obj ) {
        fb2k::image::ptr img;
        if ( img &= obj ) {
            return (__bridge NSImage *) img->getNative();
        }
        return nil;
    }
    BOOL testFontParams(NSDictionary<NSString*, NSString*> * arg) {
        return arg[@"font-name"] || arg[@"font-size"];
    }
    NSFont * fontFromParams(NSDictionary<NSString*, NSString*> * arg, NSFont * base) {
        NSString * fontName = arg[@"font-name"];
        NSString * fontSize = arg[@"font-size"];
        NSFont * font = nil;
        if ( fontName && fontSize ) {
            font = [NSFont fontWithName: fontName size: fontSize.floatValue];
        } else if ( fontName ) {
            font = [NSFont fontWithName: fontName size: base ? base.pointSize : NSFont.systemFontSize];
        } else if ( fontSize ) {
            if ( base ) {
                font = [ base fontWithSize: fontSize.floatValue ];
            } else {
                font = [NSFont monospacedDigitSystemFontOfSize: fontSize.floatValue weight: NSFontWeightRegular];
            }
        }
        if ( font ) return font;
        if ( base ) return base;
        // Reuse shared font object
        return [NSFont pp_monospacedDigitFont];
    }
    void tableViewPrepareForFont( NSTableView * tableView, NSFont * font ) {
        
        // Must use NSTableViewRowSizeStyleCustom
        // Other options either discard our font or break down with multiple cell templates used (autolayout)
        // Height is fixed anyway, better precalculate it
        assert( tableView.rowSizeStyle == NSTableViewRowSizeStyleCustom );
        
        tableView.rowHeight = tableViewRowHeightForFont(font);
    }
    CGFloat tableViewRowHeightForFont( NSFont * f ) {
        return (CGFloat) round( f.pointSize * 1.5 );
    }
}

namespace pfc {
    string8 strFromPlatform(NSString* str) {
        string8 ret;
        if ( str ) ret = str.UTF8String;
        return ret;
    }
    NSString * strToPlatform( const char * str) {
        return fb2k::strToPlatform( str );
    }
    NSString * strToPlatform(string8 const& str) {
        return fb2k::strToPlatform( str.c_str() );
    }
    string8 strFromPlatform(CFStringRef str) {
        return strFromPlatform( (__bridge NSString*) str );
    }
}
