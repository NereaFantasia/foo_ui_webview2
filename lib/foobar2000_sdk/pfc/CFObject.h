#pragma once

#include <CoreFoundation/CoreFoundation.h>

namespace pfc {
    template<typename type_t = CFTypeRef>
    class CFObject {
    public:
        typedef CFObject<type_t> self_t;
        type_t p = nullptr;
        
        ~CFObject() {
            if ( p ) CFRelease(p);
        }
        
        void Retain(type_t arg) {
            if ( p ) CFRelease(p);
            p = arg;
            if ( p ) CFRetain(p);
        }
        
        void Attach(type_t arg) {
            if ( p ) CFRelease(p);
            p = arg;
        }
        
        void operator=( self_t const & arg ) {
            if ( p ) CFRelease(p);
            p = arg.p;
            if ( p ) CFRetain(p);
        }
        CFObject() {}
        CFObject( self_t const & arg ) {
            p = arg.p;
            if ( p ) CFRetain(p);
        }
        
        CFObject(self_t && arg ) {
            p = arg.p; arg.p = NULL;
        }
        void operator=(self_t && arg) {
            if ( p ) CFRelease(p);
            p = arg.p; arg.p = NULL;
        }
        
        operator bool() const { return p != NULL; }
        operator type_t() const { return p;}
        
        
        void reset() {
            if ( p ) CFRelease(p);
            p = NULL;
        }
        
        void operator=(nullptr_t) {
            reset();
        }
    };

    // Turn any CFTypeRef into our wrapper, transferring ownership
    auto wrapCFObject(auto arg) {
        CFObject<decltype(arg)> ret; ret.p = arg;
        return ret;
    }
}
