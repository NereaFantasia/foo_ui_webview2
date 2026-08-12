#pragma once

// ============================================
// WindowGeometryMath.h - 窗口几何纯逻辑
//
// 目的：把 DPI 换算与尺寸约束规范化从 Win32 消息循环
// 与 HWND 依赖中剥离出来，使其可被 GoogleTest 直接覆盖。
//
// 本头文件**不依赖** HWND / Windows.h / foobar2000 SDK，故测试项目可以链接
// 并测试真实生产符号，而不必在测试文件内重新实现被测逻辑。
//
// 语义与现有生产代码对齐（不改变任何既有行为）：
//   - DIP → 物理：等价于 MainWindowDwm.cpp 的 MulDiv(value, dpi, 96)
//   - 约束钳制顺序：先 min 后 max，与 MainWindow::RestoreWindowPosition 一致
//   - max == 0 表示"无上限"，与 MainWindow::SetMaxSize / WM_GETMINMAXINFO 一致
//
// 注意：本文件目前只是可测 seam，生产调用点尚未接入，接入留待后续。
// ============================================

#include <cstdint>

namespace window_geometry {

// 逻辑 DPI 基准（96 DPI == 100% 缩放）
inline constexpr int kBaselineDpi = 96;

// ============================================
// DPI 换算
//
// 与 Win32 MulDiv 的取整语义一致：四舍五入，.5 远离零取整。
// 之所以不直接调用 MulDiv，是为了让本文件保持零 Windows.h 依赖，
// 同时把取整规则显式化为可测行为。
// ============================================

// 将 DIP（逻辑像素）换算为指定 DPI 下的物理像素。
// dpi <= 0 时按基准 DPI 处理（不缩放），避免除零。
int DipToPhysical(int dip, int dpi);

// 将物理像素换算回 DIP。
// dpi <= 0 时按基准 DPI 处理（不缩放），避免除零。
int PhysicalToDip(int physical, int dpi);

// ============================================
// 尺寸约束钳制
// ============================================

// clampedBy 的取值。使用稳定字符串常量而非枚举，便于直接进入 JSON 响应。
inline constexpr const char* kClampedByNone = "none";
inline constexpr const char* kClampedByMin = "min";
inline constexpr const char* kClampedByMax = "max";
inline constexpr const char* kClampedByBoth = "min-and-max";

struct SizeClampResult {
    int width = 0;
    int height = 0;
    // 请求值是否被任一约束改写
    bool clamped = false;
    // 触发钳制的约束族。宽高可能分别被不同族钳制，故 "min-and-max" 表示两族都有触发。
    const char* clampedBy = kClampedByNone;
};

// 对请求尺寸施加 min/max 约束。
//
// 与生产语义一致的三点：
//   1. 先应用 min，再应用 max。故 min > max 的矛盾输入下 **max 胜出**。
//      （这是既有行为，非本次设计选择；矛盾输入的规范化不在本文件范围内。）
//   2. maxW/maxH 为 0（或负）表示该维度无上限。
//   3. minW/minH 为 0（或负）表示该维度无下限——注意这与
//      MainWindow::SetMinSize 的入口钳制（<=0 归一为 1）是**不同层**的规则：
//      SetMinSize 负责规范化存储值，本函数只消费已给定的约束值。
SizeClampResult ClampSize(int reqW, int reqH, int minW, int minH, int maxW, int maxH);

// ============================================
// 达成度比较
// ============================================

// 判断实际尺寸是否在容差内命中期望值。
// 默认容差 1px：DIP↔物理往返换算与浏览器上取整都会产生 1px 残留，
// 把这种残留判为"未达成"会产生误报。
bool SizeMatchesWithinTolerance(int actual, int expected, int tolerance = 1);

}  // namespace window_geometry
