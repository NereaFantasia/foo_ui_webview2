#include "foobar2000-sdk-pch.h"
#import <Foundation/Foundation.h>
#import "apple-tools.h"
namespace fb2k {
    [[noreturn]] void appleThrowIOError(NSError * error) {
        if (error == nil) {
            PFC_ASSERT(!"null NSError");
            throw exception_io();
        }
        if ( error.domain == NSPOSIXErrorDomain ) {
            exception_io_from_nix((int) error.code);
        }
        // NSCocoaErrorDomain code 516 = already exists
        if ( error.domain == NSCocoaErrorDomain ) {
            switch (error.code) {
                case NSFileWriteFileExistsError:
                    throw exception_io_already_exists();
                case NSFileNoSuchFileError:
                case NSFileReadNoSuchFileError:
                    throw exception_io_not_found();
                case NSFileLockingError:
                    throw exception_io_sharing_violation();
                case NSFileReadNoPermissionError:
                case NSFileWriteNoPermissionError:
                    throw exception_io_denied();
                case NSFileWriteOutOfSpaceError:
                    throw exception_io_device_full();
                case NSFileWriteVolumeReadOnlyError:
                    throw exception_io_denied_readonly();
                default:
                    break;
            }
        }
        pfc::throw_exception_with_message<exception_io>( error.localizedDescription.UTF8String );
    }
    void appleCopyFile( const char * from_native, const char * to_native, bool overwrite, abort_callback& a) {
        @autoreleasepool {
            a.check();
            NSFileManager * mgr = NSFileManager.defaultManager;
            NSString * from = [NSString stringWithUTF8String: from_native];
            NSString * to = [NSString stringWithUTF8String: to_native];
            if ( ! from || ! to ) throw pfc::exception_invalid_params();
            if ( overwrite ) [mgr removeItemAtPath: to error: nil];
            NSError * error = nil;
            if (![mgr copyItemAtPath: from toPath: to error: &error]) {
                fb2k::appleThrowIOError(error);
            }
            a.check();
        }
    }
}
