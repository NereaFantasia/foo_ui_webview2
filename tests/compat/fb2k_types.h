#pragma once
// Minimal foobar2000 SDK type shim for unit tests.
// Provides only the typedefs needed by headers under test,
// avoiding a full SDK dependency in the test project.

#include <cstdint>
#include <cstdio>

typedef uint32_t t_uint32;
typedef uint64_t t_uint64;
typedef int32_t  t_int32;
typedef size_t   t_size;

// Stub for foobar2000 console::printf used in FailureHook
namespace console {
    inline void printf(const char*, ...) {}
    inline void print(const char*) {}
}
