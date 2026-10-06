#pragma once

// ============================================
// PreferencesPageInternal.h - 总览页的控件与菜单命令 ID
// 只供 PreferencesPage.cpp、PreferencesTemplateActions.cpp、ApiInventoryWindow.cpp 共用。
// ============================================

namespace webview_prefs {

// ============================================
// Dialog Control IDs
// ============================================

enum ControlIds {
    // Web template 组
    IDC_GROUP_TEMPLATE = 1000,
    IDC_STATIC_TEMPLATE = 1001,
    IDC_COMBO_TEMPLATE = 1002,
    IDC_BTN_MANAGE = 1003,
    IDC_STATIC_PATH = 1004,
    IDC_EDIT_PATH = 1005,
    IDC_BTN_OPEN_FOLDER = 1006,

    // Appearance 组
    IDC_GROUP_APPEARANCE = 1020,
    IDC_STATIC_LANGUAGE = 1021,
    IDC_COMBO_LANGUAGE = 1022,
    IDC_STATIC_BACKDROP = 1023,
    IDC_COMBO_BACKDROP = 1024,
    IDC_STATIC_LANGUAGE_NOTE = 1025,

    // 1010–1013 不复用。

    // Tools 组
    IDC_GROUP_TOOLS = 1050,
    IDC_STATIC_DEV_SERVER = 1051,
    IDC_STATIC_DEV_SERVER_STATUS = 1052,
    IDC_BTN_ADVANCED = 1053,
    IDC_BTN_SHOW_API_LIST = 1054,
    IDC_STATIC_TOOLS_NOTE = 1055,
};

// Manage... 弹出菜单的命令 ID
enum ManageMenuIds {
    IDM_TEMPLATE_CREATE = 2001,
    IDM_TEMPLATE_RENAME = 2002,
    IDM_TEMPLATE_DELETE = 2003,
};

} // namespace webview_prefs
