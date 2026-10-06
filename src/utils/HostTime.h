#pragma once
// HostTime.h - the host timestamp carried by events and results as `hostTime`.
//
// Unix epoch milliseconds with a fractional part, read from the system clock
// that Date.now() also reads, so a page on the same machine can subtract the
// two to get the delivery delay without estimating a clock offset. It follows
// system time adjustments, so a value taken across one can look negative or
// very late; callers on the page side drop such samples.
#include <Windows.h>
#include <cstdint>

namespace host_time {

inline double NowUnixMs() noexcept {
    FILETIME ft{};
    GetSystemTimePreciseAsFileTime(&ft);
    // FILETIME counts 100 ns ticks since 1601-01-01.
    const std::uint64_t ticks = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    constexpr std::uint64_t kUnixEpochTicks = 116444736000000000ULL;
    return static_cast<double>(ticks - kUnixEpochTicks) / 10000.0;
}

}  // namespace host_time
