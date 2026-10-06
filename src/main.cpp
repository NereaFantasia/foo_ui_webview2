#include "pch.h"
#include "version.h"
#include "ComponentDescription.inc"

// ============================================
// Component Version Declaration
// ============================================
DECLARE_COMPONENT_VERSION(
    "WebView2 UI",
    PLUGIN_VERSION_STR,
    kComponentDescription
);

// 组件文件名验证 - 防止 DLL 被重命名后运行
VALIDATE_COMPONENT_FILENAME("foo_ui_webview2.dll");
