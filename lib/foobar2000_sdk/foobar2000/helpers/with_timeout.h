#pragma once
#include <functional>

namespace fb2k {
    typedef std::function<void (abort_callback&)> with_timeout_t;
    bool with_timeout( with_timeout_t, double );
}
