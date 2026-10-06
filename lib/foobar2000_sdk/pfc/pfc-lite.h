#pragma once


#ifdef _MSC_VER // MSVC sucks, doesn't set __cplusplus properly by default
#if _MSVC_LANG < 202002L
#error C++20 please
#endif
#else // not MSVC
#if __cplusplus < 202002L
#error C++20 please
#endif
#endif

// Global flag - whether it's OK to leak static objects as they'll be released anyway by process death
#ifndef PFC_LEAK_STATIC_OBJECTS
#define PFC_LEAK_STATIC_OBJECTS 1
#endif


#ifdef __clang__
// Suppress a warning for a common practice in pfc/fb2k code
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

#if !defined(_WINDOWS) && (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(_WIN32_WCE))
#define _WINDOWS
#endif


#ifdef _WINDOWS
#include "targetver.h"

#ifndef STRICT
#define STRICT
#endif

#ifndef _SYS_GUID_OPERATOR_EQ_
#define _NO_SYS_GUID_OPERATOR_EQ_	//fix retarded warning with operator== on GUID returning int
#endif

// WinSock2.h *before* Windows.h or else VS2017 15.3 breaks
#include <WinSock2.h>
#include <windows.h>

#if !defined(PFC_WINDOWS_STORE_APP) && !defined(PFC_WINDOWS_DESKTOP_APP)

#ifdef WINAPI_FAMILY_PARTITION
#if ! WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define PFC_WINDOWS_STORE_APP // Windows store or Windows phone app, not a desktop app
#endif // #if ! WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#endif // #ifdef WINAPI_FAMILY_PARTITION

#ifndef PFC_WINDOWS_STORE_APP
#define PFC_WINDOWS_DESKTOP_APP
#endif

#endif // #if !defined(PFC_WINDOWS_STORE_APP) && !defined(PFC_WINDOWS_DESKTOP_APP)

#ifndef _SYS_GUID_OPERATOR_EQ_
constexpr __inline bool __InlineIsEqualGUID(REFGUID v1, REFGUID v2)
{
    if (v1.Data1 != v2.Data1 || v1.Data2 != v2.Data2 || v1.Data3 != v2.Data3) return false;
    for (unsigned i = 0; i < 8; ++i) {
        if (v1.Data4[i] != v2.Data4[i]) return false;
    }
    return true;
}

constexpr inline bool operator==(REFGUID guidOne, REFGUID guidOther) { return __InlineIsEqualGUID(guidOne, guidOther); }
constexpr inline bool operator!=(REFGUID guidOne, REFGUID guidOther) { return !__InlineIsEqualGUID(guidOne, guidOther); }
#endif

#include <tchar.h>

#else // not Windows

#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h> // memcmp

#ifndef GUID_DEFINED
#define GUID_DEFINED


struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[ 8 ];
} __attribute__((packed));

constexpr inline bool _GUID_equals(GUID const & v1, GUID const & v2) {
    if (v1.Data1 != v2.Data1 || v1.Data2 != v2.Data2 || v1.Data3 != v2.Data3) return false;
    for( unsigned i = 0; i < 8; ++ i) {
        if (v1.Data4[i] != v2.Data4[i]) return false;
    }
    return true;
}

constexpr inline bool operator==(const GUID & v1,const GUID & v2) {
    return _GUID_equals(v1,v2);
}

constexpr inline bool operator!=(const GUID & v1,const GUID & v2) {
    return !_GUID_equals(v1,v2);
}

typedef const GUID & REFGUID;
typedef GUID CLSID;

typedef REFGUID REFCLSID;
typedef REFGUID REFIID;

#endif // GUID_DEFINED

#endif



#define PFC_MEMORY_SPACE_LIMIT ((t_uint64)1<<(sizeof(void*)*8-1))

#define PFC_ALLOCA_LIMIT (4096)

#include <exception>
#include <stdexcept>
#include <new>

#define _PFC_WIDESTRING(_String) L ## _String
#define PFC_WIDESTRING(_String) _PFC_WIDESTRING(_String)

#if defined(_DEBUG) || defined(DEBUG)
#define PFC_DEBUG 1
#else
#define PFC_DEBUG 0
#ifndef NDEBUG
#pragma message("WARNING: release build without NDEBUG")
#endif
#endif // _DEBUG || DEBUG


#ifdef PFC_ASSERT
// already defined
#elif ! PFC_DEBUG
#define PFC_ASSERT(_Expression)     ((void)0)
#define PFC_ASSERT_NO_EXCEPTION(_Expression) { _Expression; }
#else // PFC_DEBUG

#ifdef _WIN32
namespace pfc { void myassert_win32(const wchar_t* _Message, const wchar_t* _File, unsigned _Line); }
#define PFC_ASSERT(_Expression) (void)( (!!(_Expression)) || (pfc::myassert_win32(PFC_WIDESTRING(#_Expression), PFC_WIDESTRING(__FILE__), __LINE__), 0) )
#else // _WIN32 or not
namespace pfc { void myassert(const char* _Message, const char* _File, unsigned _Line); }
#define PFC_ASSERT(_Expression) (void)( (!!(_Expression)) || (pfc::myassert(#_Expression, __FILE__, __LINE__), 0) )
#endif // _WIN32 or not

#endif // PFC_DEBUG

#ifndef PFC_ASSERT_NO_EXCEPTION
#define PFC_ASSERT_NO_EXCEPTION(_Expression) { try { _Expression; } catch(...) { PFC_ASSERT(!"Should not get here - unexpected exception"); } }
#endif
#define PFC_ASSERT_SUCCESS(_Expression) { bool _Expression_Val = !! (_Expression); PFC_ASSERT(_Expression_Val); }

#ifdef _MSC_VER

#if PFC_DEBUG
#define NOVTABLE
#else
#define NOVTABLE _declspec(novtable)
#endif

#if PFC_DEBUG
#define ASSUME(X) PFC_ASSERT(X)
#else
#define ASSUME(X) __assume(X)
#endif

#define PFC_DEPRECATE(X) // __declspec(deprecated(X))   don't do this since VS2015 defaults to erroring these
#define PFC_NORETURN __declspec(noreturn)
#define PFC_NOINLINE __declspec(noinline)
#else // else not MSVC

#define NOVTABLE
#define ASSUME(X) PFC_ASSERT(X)
#define PFC_DEPRECATE(X)
#define PFC_NORETURN __attribute__ ((noreturn))
#define PFC_NOINLINE

#endif // end not MSVC

#define PFC_NO_OP ((void)0)

#include "int_types.h"
#include "string-interface.h"
#include "string-lite.h"

namespace pfc {
    // forward types
    class bit_array;
    class bit_array_var;

    template<typename T> class list_base_const_t;
    template<typename T> class list_base_t;
}
