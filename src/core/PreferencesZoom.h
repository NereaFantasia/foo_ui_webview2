#pragma once

// ============================================
// PreferencesZoom.h - 默认内容缩放的纯换算
//
// Performance 子页把缩放存成整数百分比（50–200，步进 25），WebView2 要的是因子
// （ICoreWebView2Controller3::put_ZoomFactor，1.0 = 100%）。本头文件负责两者之间的换算、
// 越界钳制与下拉项下标，与 HWND、WebView2 和 foobar2000 SDK 都无关，让 GoogleTest 直接覆盖。
//
// 缩放叠加在系统 DPI 之上：100 表示不叠加。
// 步进 25 与 Windows 显示缩放的档位（100 / 125 / 150 / 175 / 200）一致。
// ============================================

namespace prefs_zoom {

inline constexpr int kMinPercent = 50;
inline constexpr int kMaxPercent = 200;
inline constexpr int kStepPercent = 25;
inline constexpr int kDefaultPercent = 100;
inline constexpr int kOptionCount = (kMaxPercent - kMinPercent) / kStepPercent + 1;  // 7

// 百分比是否恰好是下拉框里的一项。
inline bool IsValidPercent(int percent) {
    return percent >= kMinPercent && percent <= kMaxPercent && (percent - kMinPercent) % kStepPercent == 0;
}

// 存储值可能来自旧版本或被外部改写：钳到范围内并对齐到步进（四舍五入），越界不当成错误。
inline int SanitizePercent(int percent) {
    if (percent < kMinPercent) return kMinPercent;
    if (percent > kMaxPercent) return kMaxPercent;
    const int offset = percent - kMinPercent;
    const int rounded = (offset + kStepPercent / 2) / kStepPercent * kStepPercent;
    return kMinPercent + (rounded > kMaxPercent - kMinPercent ? kMaxPercent - kMinPercent : rounded);
}

inline double FactorFromPercent(int percent) {
    return SanitizePercent(percent) / 100.0;
}

// 下拉框第 index 项对应的百分比；越界下标回落到默认值。
inline int PercentAt(int index) {
    if (index < 0 || index >= kOptionCount) return kDefaultPercent;
    return kMinPercent + index * kStepPercent;
}

inline int IndexOf(int percent) {
    return (SanitizePercent(percent) - kMinPercent) / kStepPercent;
}

}  // namespace prefs_zoom
