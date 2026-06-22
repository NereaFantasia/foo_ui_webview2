#pragma once
#include "pch.h"
#include <string>
#include <unordered_map>

/**
 * IconLoader converts frontend-provided base64 icon data to HICON handles.
 * Default icons come from fb_ui->get_main_icon() / load_main_icon().
 */
class IconLoader {
public:
    /**
     * Decode a base64 ICO payload and return a cached HICON.
     * The returned handle is owned by IconLoader and must not be destroyed by callers.
     */
    static HICON FromBase64(const std::string& base64);

    /** Clear the cache and destroy all owned HICON handles. */
    static void Clear();

private:
    static std::unordered_map<std::string, HICON> s_cache;
};
