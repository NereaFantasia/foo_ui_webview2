// Auto-generated SDK injection script - do NOT manually maintain
// Note: This file is #include'd by WebViewHost.cpp and injected into all pages

#pragma once

#include <string>

// MSVC has string literal length limits (C2026),
// so we use manual s.append() concatenation to build the SDK script.
//
// This script injects window.fb into pages (alongside window.fb2k / bridge).

static std::wstring GetInjectedFbSdkScript() {
    std::wstring s;
    s.reserve(20000);

    s.append(L"/**\n");
    s.append(L" * foo_ui_webview2 SDK - Bridge API layer\n");
    s.append(L" * \n");
    s.append(L" * This script is auto-injected by C++ into all pages as the frontend SDK entry.\n");
    s.append(L" */\n\n");

    s.append(L"(function(window) {\n");
    s.append(L"    'use strict';\n\n");

    s.append(L"    class Bridge {\n");
    s.append(L"        constructor() {\n");
    s.append(L"            this.nativeFb2k = this.getNativeFb2k();\n");
    s.append(L"            this.isAvailable = !!this.nativeFb2k;\n");
    s.append(L"            this.checkAvailability();\n");
    s.append(L"        }\n\n");

    s.append(L"        getNativeFb2k() {\n");
    s.append(L"            if (window._nativeFb2k) return window._nativeFb2k;\n");
    s.append(L"            if (window.fb2k?.invoke && window.fb2k?._handleResponse) {\n");
    s.append(L"                window._nativeFb2k = window.fb2k;\n");
    s.append(L"                return window.fb2k;\n");
    s.append(L"            }\n");
    s.append(L"            return undefined;\n");
    s.append(L"        }\n\n");

    s.append(L"        checkAvailability() {\n");
    s.append(L"            if (this.nativeFb2k) {\n");
    s.append(L"                console.log('[SDK] WebView2 environment detected');\n");
    s.append(L"            } else if (window.chrome?.webview) {\n");
    s.append(L"                console.log('[SDK] WebView2 present, waiting for fb2k init...');\n");
    s.append(L"                setTimeout(() => {\n");
    s.append(L"                    this.nativeFb2k = this.getNativeFb2k();\n");
    s.append(L"                    if (this.nativeFb2k) {\n");
    s.append(L"                        this.isAvailable = true;\n");
    s.append(L"                        console.log('[SDK] fb2k ready');\n");
    s.append(L"                    }\n");
    s.append(L"                }, 100);\n");
    s.append(L"            } else {\n");
    s.append(L"                console.warn('[SDK] WebView2 not available, using Mock mode');\n");
    s.append(L"            }\n");
    s.append(L"        }\n\n");

    s.append(L"        async invoke(method, params) {\n");
    s.append(L"            const native = this.getNativeFb2k();\n");
    s.append(L"            if (native) {\n");
    s.append(L"                return native.invoke(method, params);\n");
    s.append(L"            }\n");
    s.append(L"            console.log('[SDK Mock] invoke:', method, params);\n");
    s.append(L"            return new Promise(resolve => {\n");
    s.append(L"                setTimeout(() => resolve({ mock: true, method }), 100);\n");
    s.append(L"            });\n");
    s.append(L"        }\n\n");

    s.append(L"        on(event, handler) {\n");
    s.append(L"            const native = this.getNativeFb2k();\n");
    s.append(L"            if (native) {\n");
    s.append(L"                native.on(event, handler);\n");
    s.append(L"                return () => this.off(event, handler);\n");
    s.append(L"            }\n");
    s.append(L"            console.log('[SDK Mock] on:', event);\n");
    s.append(L"            return () => {};\n");
    s.append(L"        }\n\n");

    s.append(L"        off(event, handler) {\n");
    s.append(L"            const native = this.getNativeFb2k();\n");
    s.append(L"            if (native) {\n");
    s.append(L"                native.off(event, handler);\n");
    s.append(L"            }\n");
    s.append(L"        }\n\n");

    s.append(L"        once(event, handler) {\n");
    s.append(L"            const wrapper = (data) => {\n");
    s.append(L"                this.off(event, wrapper);\n");
    s.append(L"                handler(data);\n");
    s.append(L"            };\n");
    s.append(L"            return this.on(event, wrapper);\n");
    s.append(L"        }\n\n");

    s.append(L"        emit(event, data) {\n");
    s.append(L"            const native = this.getNativeFb2k();\n");
    s.append(L"            if (native) {\n");
    s.append(L"                native._emit(event, data);\n");
    s.append(L"            }\n");
    s.append(L"        }\n");
    s.append(L"    }\n\n");

    s.append(L"    const bridge = new Bridge();\n\n");

    // Register only the most-used session APIs: artwork + shell
    // Extend here if full SDK (player/playlist/library/...) is also needed
    s.append(L"    const artwork = {\n");
    s.append(L"        getCurrent: (type) => bridge.invoke('artwork.getCurrent', { type }),\n");
    s.append(L"        getByPath: (path, type) => bridge.invoke('artwork.getByPath', { path, type }),\n");
    s.append(L"        getFb2kUrl: async (type, options) => {\n");
    s.append(L"            const res = await bridge.invoke('artwork.getFb2kUrl', { type, ...(options || {}) });\n");
    s.append(L"            if (res && res.available && !res.url && res.dataUrl) res.url = res.dataUrl;\n");
    s.append(L"            return res;\n");
    s.append(L"        },\n");
    s.append(L"        getFb2kUrlByPath: async (path, type, options) => {\n");
    s.append(L"            const res = await bridge.invoke('artwork.getFb2kUrlByPath', { path, type, ...(options || {}) });\n");
    s.append(L"            if (res && res.available && !res.url && res.dataUrl) res.url = res.dataUrl;\n");
    s.append(L"            return res;\n");
    s.append(L"        },\n");
    s.append(L"        withMaxSize: (url, maxSize) => {\n");
    s.append(L"            if (!url || !maxSize || maxSize <= 0) return url;\n");
    s.append(L"            const hasQuery = url.includes('?');\n");
    s.append(L"            const sep = hasQuery ? '&' : '?';\n");
    s.append(L"            return url + sep + 'maxSize=' + encodeURIComponent(String(maxSize));\n");
    s.append(L"        }\n");
    s.append(L"    };\n\n");
    // Shell API
    s.append(L"    const shell = {\n");
    s.append(L"        showInExplorer: (path) => bridge.invoke('shell.showInExplorer', { path }),\n");
    s.append(L"        openWith: (path) => bridge.invoke('shell.openWith', { path }),\n");
    s.append(L"        openExternal: (url) => bridge.invoke('shell.openExternal', { url }),\n");
    s.append(L"        exec: (command, options) => bridge.invoke('shell.exec', { command, ...(options || {}) }),\n");
    s.append(L"        spawn: (executable, options) => bridge.invoke('shell.spawn', { executable, ...(options || {}) })\n");
    s.append(L"    };\n\n");
    s.append(L"    window.fb = window.fb || {};\n");
    s.append(L"    window.fb.artwork = artwork;\n");
    s.append(L"    window.fb.shell = shell;\n");
    s.append(L"    window.fb.on = (event, handler) => bridge.on(event, handler);\n");
    s.append(L"    window.fb.off = (event, handler) => bridge.off(event, handler);\n");
    s.append(L"    window.fb.once = (event, handler) => bridge.once(event, handler);\n");
    s.append(L"    window.fb.emit = (event, data) => bridge.emit(event, data);\n");
    s.append(L"    window.fb.isAvailable = () => bridge.isAvailable;\n\n");

    s.append(L"    console.log('[SDK] foo_ui_webview2 SDK loaded (auto-injected), global: fb');\n");
    s.append(L"})(typeof window !== 'undefined' ? window : global);\n");

    return s;
}