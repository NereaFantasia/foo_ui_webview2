#pragma once

// ============================================
// PreferencesFields.h - 各偏好设置页的字段表
//
// 只声明“有哪些字段、什么类型、默认值多少、怎么校验”，与 HWND、cfg_var 和 SDK 无关，
// 让 GoogleTest 能用真实字段表覆盖草稿逻辑。字段的读写落在各页面类里。
// 下标常量与字段表顺序一一对应，页面代码用下标访问快照。
// ============================================

#include <functional>
#include <string>

#include "prefs/PreferencesCdpPort.h"
#include "prefs/PreferencesDevServerUrl.h"
#include "prefs/PreferencesDraft.h"
#include "prefs/PreferencesZoom.h"

namespace prefs_fields {

// 总览页（Display › WebView2 UI）的三个字段，顺序即 Diff / 冲突报告的顺序。
namespace overview {

enum Index : size_t {
    Template = 0,
    Backdrop,
    Language,
    Count,
};

inline constexpr int kBackdropCount = 5;   // None / Mica / MicaAlt / Acrylic / Tabbed
inline constexpr int kLanguageCount = 3;   // Auto / English / Chinese
inline constexpr int kDefaultBackdrop = 1; // Mica
inline constexpr int kDefaultLanguage = 0; // Auto

// templateIsUsable 由调用方提供“目录存在且含有效 index.html”的判定；名字先按字符集规则查，
// 不合法就不去碰磁盘。
inline prefs_draft::FieldTable Table(std::function<bool(const std::string&)> templateIsUsable) {
    using prefs_draft::FieldSpec;
    using prefs_draft::Kind;
    using prefs_draft::ValidationError;
    using prefs_draft::Value;

    prefs_draft::FieldTable fields(Count);
    fields[Template] = FieldSpec{"template", Kind::String, Value::Str("default"), 0, false,
        [usable = std::move(templateIsUsable)](const Value& v) {
            if (!prefs_draft::IsValidTemplateName(v.s)) return ValidationError::Invalid;
            if (!usable || !usable(v.s)) return ValidationError::Missing;
            return ValidationError::None;
        }};
    fields[Backdrop] = FieldSpec{"backdrop", Kind::Int, Value::Int(kDefaultBackdrop), kBackdropCount, false, nullptr};
    // 语言与本进程启动值不同就要求重启：宿主缓存的菜单 / 面板描述不会因 Apply 更新。
    fields[Language] = FieldSpec{"language", Kind::Int, Value::Int(kDefaultLanguage), kLanguageCount, true, nullptr};
    return fields;
}

}  // namespace overview

// 窗口子页（WebView2 UI › Window）。七个开关，顺序即控件顺序：窗口 / 托盘 / 任务栏三组。
namespace window {

enum Index : size_t {
    RememberPosition = 0,
    BackgroundMode,
    RestoreVisibility,
    MinimizeToTray,
    CloseToTray,
    TaskbarButtons,
    TaskbarProgress,
    Count,
};

inline prefs_draft::FieldTable Table() {
    using prefs_draft::FieldSpec;
    using prefs_draft::Kind;
    using prefs_draft::Value;

    prefs_draft::FieldTable fields(Count);
    fields[RememberPosition] = FieldSpec{"rememberPosition", Kind::Bool, Value::Bool(true), 0, false, nullptr};
    // 后台服务在启动时初始化，改动后要重启才生效。
    fields[BackgroundMode] = FieldSpec{"backgroundMode", Kind::Bool, Value::Bool(false), 0, true, nullptr};
    fields[RestoreVisibility] = FieldSpec{"restoreVisibility", Kind::Bool, Value::Bool(true), 0, false, nullptr};
    fields[MinimizeToTray] = FieldSpec{"minimizeToTray", Kind::Bool, Value::Bool(false), 0, false, nullptr};
    fields[CloseToTray] = FieldSpec{"closeToTray", Kind::Bool, Value::Bool(false), 0, false, nullptr};
    // 缩略图工具栏只能在任务栏按钮创建时装一次，开关改动后重启才生效。
    fields[TaskbarButtons] = FieldSpec{"taskbarButtons", Kind::Bool, Value::Bool(true), 0, true, nullptr};
    fields[TaskbarProgress] = FieldSpec{"taskbarProgress", Kind::Bool, Value::Bool(false), 0, false, nullptr};
    return fields;
}

}  // namespace window

// 性能子页（WebView2 UI › Performance）：预热、深度挂起、默认内容缩放（整数百分比）。
namespace performance {

enum Index : size_t {
    Preheat = 0,
    DeepSuspend,
    ZoomPercent,
    Count,
};

inline prefs_draft::FieldTable Table() {
    using prefs_draft::FieldSpec;
    using prefs_draft::Kind;
    using prefs_draft::ValidationError;
    using prefs_draft::Value;

    prefs_draft::FieldTable fields(Count);
    // 环境在 initquit::on_init 里预热，改动后下次启动才生效。
    fields[Preheat] = FieldSpec{"preheat", Kind::Bool, Value::Bool(true), 0, true, nullptr};
    // 主窗口每次算可见性策略时实时读取，不重启。
    fields[DeepSuspend] = FieldSpec{"deepSuspend", Kind::Bool, Value::Bool(true), 0, false, nullptr};
    // 只接受下拉框里的取值；enumCount 留 0，范围校验在这里做。
    fields[ZoomPercent] = FieldSpec{"zoomPercent", Kind::Int, Value::Int(prefs_zoom::kDefaultPercent), 0, false,
        [](const Value& v) {
            return prefs_zoom::IsValidPercent(v.i) ? ValidationError::None : ValidationError::OutOfRange;
        }};
    return fields;
}

}  // namespace performance

// 开发者子页（WebView2 UI › Developer）：开发者工具、开发服务器（开关 + URL）、CDP（开关 + 端口）。
// 顺序即控件顺序。
namespace developer {

enum Index : size_t {
    DevTools = 0,
    UseDevServer,
    DevServerUrl,
    CdpRemote,
    CdpPort,
    Count,
};

// URL 允许为空（空即回退本地模板）；非空时必须是 http(s):// 起头、带主机名的地址。
// 开发服务器两项在加载前端时读取，改动不要求重启；开发者工具在新建 controller 时读取，
// CDP 开关与端口在创建 WebView2 环境时读取，三者改动后重启才生效。
inline prefs_draft::FieldTable Table() {
    using prefs_draft::FieldSpec;
    using prefs_draft::Kind;
    using prefs_draft::ValidationError;
    using prefs_draft::Value;

    prefs_draft::FieldTable fields(Count);
    fields[DevTools] = FieldSpec{"devTools", Kind::Bool, Value::Bool(false), 0, true, nullptr};
    fields[UseDevServer] = FieldSpec{"useDevServer", Kind::Bool, Value::Bool(false), 0, false, nullptr};
    fields[DevServerUrl] = FieldSpec{"devServerUrl", Kind::String, Value::Str(""), 0, false,
        [](const Value& v) {
            return prefs_devserver::CheckUrl(v.s) == prefs_devserver::UrlError::None ? ValidationError::None
                                                                                     : ValidationError::Invalid;
        }};
    fields[CdpRemote] = FieldSpec{"cdpRemote", Kind::Bool, Value::Bool(false), 0, true, nullptr};
    // 输入框解析失败时草稿里存 prefs_cdp::kInvalidPort，在这里被判为越界；enumCount 留 0。
    fields[CdpPort] = FieldSpec{"cdpPort", Kind::Int, Value::Int(prefs_cdp::kDefaultPort), 0, true,
        [](const Value& v) {
            return prefs_cdp::IsValidPort(v.i) ? ValidationError::None : ValidationError::OutOfRange;
        }};
    return fields;
}

}  // namespace developer

}  // namespace prefs_fields
