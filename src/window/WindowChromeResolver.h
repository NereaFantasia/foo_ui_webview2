#pragma once

#include "window/WindowChromeState.h"

class WindowChromeResolver {
public:
    static ResolvedWindowChromeState Resolve(const WindowChromeResolverSnapshot& snapshot);
    static std::string NormalizeBackdropEffect(const std::string& effect, bool allowSystem,
                                               const std::string& fallback);
};