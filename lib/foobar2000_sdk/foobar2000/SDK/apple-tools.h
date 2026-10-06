#pragma once
#ifdef __OBJC__
@class NSError;
#endif

namespace fb2k {
    void appleCopyFile( const char * from_native, const char * to_native, bool overwrite, abort_callback& a);
#ifdef __OBJC__
    [[noreturn]] void appleThrowIOError(NSError * error);
#endif
}
