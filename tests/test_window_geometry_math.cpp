// test_window_geometry_math.cpp - WindowGeometryMath 纯逻辑覆盖
//
// P0-b / R3: 本文件**直接链接真实生产符号** window_geometry::*，
// 不在测试内重新实现被测逻辑。生产代码语义漂移时这些断言会失败。
//
// 断言依据（P0 不改变行为，故测试锁定的是**现状**语义）：
//   - DIP→物理 == MainWindowDwm.cpp / MainWindow.cpp 的 MulDiv(value, dpi, 96)
//   - 钳制顺序先 min 后 max（MainWindow::RestoreWindowPosition）
//   - max == 0 表示无上限（MainWindow::SetMaxSize / WM_GETMINMAXINFO）
#include "pch.h"
#include "../src/window/WindowGeometryMath.h"

using namespace window_geometry;

// ============================================
// DipToPhysical
// ============================================

TEST(WindowGeometryDip, BaselineDpiIsIdentity) {
    EXPECT_EQ(DipToPhysical(800, 96), 800);
    EXPECT_EQ(DipToPhysical(0, 96), 0);
    EXPECT_EQ(DipToPhysical(1, 96), 1);
}

TEST(WindowGeometryDip, ScalesUpAtHigherDpi) {
    // 125% / 150% / 200%
    EXPECT_EQ(DipToPhysical(100, 120), 125);
    EXPECT_EQ(DipToPhysical(800, 144), 1200);
    EXPECT_EQ(DipToPhysical(800, 192), 1600);
}

// 与 Win32 MulDiv 一致：四舍五入，而非截断。
TEST(WindowGeometryDip, RoundsRatherThanTruncates) {
    // 1 * 120 / 96 == 1.25 -> 1
    EXPECT_EQ(DipToPhysical(1, 120), 1);
    // 3 * 120 / 96 == 3.75 -> 4（截断会得到 3）
    EXPECT_EQ(DipToPhysical(3, 120), 4);
    // 1 * 144 / 96 == 1.5 -> 2（.5 远离零）
    EXPECT_EQ(DipToPhysical(1, 144), 2);
}

// dpi <= 0 是不可能从 GetDpiForWindow 得到的值，但作为纯函数必须避免除零。
TEST(WindowGeometryDip, NonPositiveDpiFallsBackToBaseline) {
    EXPECT_EQ(DipToPhysical(640, 0), 640);
    EXPECT_EQ(DipToPhysical(640, -1), 640);
    EXPECT_EQ(PhysicalToDip(640, 0), 640);
    EXPECT_EQ(PhysicalToDip(640, -1), 640);
}

TEST(WindowGeometryDip, NegativeValuesKeepSign) {
    // 位置类字段可为负（多显示器左侧/上方）
    EXPECT_EQ(DipToPhysical(-100, 192), -200);
    EXPECT_EQ(PhysicalToDip(-200, 192), -100);
}

TEST(WindowGeometryDip, LargeValuesDoNotOverflow) {
    // 中间量若用 int 会溢出：1'000'000 * 192 > INT_MAX
    EXPECT_EQ(DipToPhysical(1000000, 192), 2000000);
}

// ============================================
// PhysicalToDip
// ============================================

TEST(WindowGeometryPhysicalToDip, InvertsExactRatios) {
    EXPECT_EQ(PhysicalToDip(125, 120), 100);
    EXPECT_EQ(PhysicalToDip(1200, 144), 800);
    EXPECT_EQ(PhysicalToDip(1600, 192), 800);
}

// 往返换算不保证恒等 —— 这正是 SizeMatchesWithinTolerance 存在的原因。
TEST(WindowGeometryPhysicalToDip, RoundTripMayDriftByOnePixel) {
    const int dpi = 120;
    const int dip = 3;
    const int physical = DipToPhysical(dip, dpi);   // 4
    const int back = PhysicalToDip(physical, dpi);  // 3.2 -> 3
    EXPECT_TRUE(SizeMatchesWithinTolerance(back, dip, 1));
}

// ============================================
// ClampSize
// ============================================

TEST(WindowGeometryClamp, NoConstraintsPassesThrough) {
    auto r = ClampSize(800, 600, 0, 0, 0, 0);
    EXPECT_EQ(r.width, 800);
    EXPECT_EQ(r.height, 600);
    EXPECT_FALSE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByNone);
}

TEST(WindowGeometryClamp, WithinConstraintsIsNotClamped) {
    auto r = ClampSize(800, 600, 400, 300, 1600, 1200);
    EXPECT_EQ(r.width, 800);
    EXPECT_EQ(r.height, 600);
    EXPECT_FALSE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByNone);
}

TEST(WindowGeometryClamp, BelowMinIsRaised) {
    auto r = ClampSize(100, 80, 400, 300, 0, 0);
    EXPECT_EQ(r.width, 400);
    EXPECT_EQ(r.height, 300);
    EXPECT_TRUE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByMin);
}

TEST(WindowGeometryClamp, AboveMaxIsLowered) {
    auto r = ClampSize(3000, 2000, 0, 0, 1600, 1200);
    EXPECT_EQ(r.width, 1600);
    EXPECT_EQ(r.height, 1200);
    EXPECT_TRUE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByMax);
}

// 宽被 min 抬、高被 max 压 —— 两族同时触发。
TEST(WindowGeometryClamp, MinAndMaxCanBothTrigger) {
    auto r = ClampSize(100, 2000, 400, 300, 1600, 1200);
    EXPECT_EQ(r.width, 400);
    EXPECT_EQ(r.height, 1200);
    EXPECT_TRUE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByBoth);
}

// 单维度也可同时触发两族（宽超上限、高低于下限）。
TEST(WindowGeometryClamp, PerAxisConstraintsAreIndependent) {
    auto r = ClampSize(3000, 100, 400, 300, 1600, 1200);
    EXPECT_EQ(r.width, 1600);
    EXPECT_EQ(r.height, 300);
    EXPECT_STREQ(r.clampedBy, kClampedByBoth);
}

TEST(WindowGeometryClamp, ZeroMaxMeansUnlimited) {
    auto r = ClampSize(5000, 4000, 400, 300, 0, 0);
    EXPECT_EQ(r.width, 5000);
    EXPECT_EQ(r.height, 4000);
    EXPECT_FALSE(r.clamped);
}

TEST(WindowGeometryClamp, NegativeMaxAlsoMeansUnlimited) {
    auto r = ClampSize(5000, 4000, 0, 0, -1, -1);
    EXPECT_EQ(r.width, 5000);
    EXPECT_EQ(r.height, 4000);
    EXPECT_FALSE(r.clamped);
}

TEST(WindowGeometryClamp, NonPositiveMinMeansNoLowerBound) {
    auto r = ClampSize(10, 10, 0, 0, 0, 0);
    EXPECT_EQ(r.width, 10);
    EXPECT_EQ(r.height, 10);
    EXPECT_FALSE(r.clamped);
}

// 矛盾输入的**现状**行为：先 min 后 max，故 max 胜出。
// D6 要求的规范化属 P2 范围；此处锁定现状以便 P2 的行为变更可见。
TEST(WindowGeometryClamp, ContradictoryConstraintsMaxWins) {
    auto r = ClampSize(800, 600, 1000, 900, 500, 400);
    EXPECT_EQ(r.width, 500);
    EXPECT_EQ(r.height, 400);
    EXPECT_TRUE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByBoth);
}

TEST(WindowGeometryClamp, ExactBoundsAreNotClamped) {
    auto r = ClampSize(400, 300, 400, 300, 400, 300);
    EXPECT_EQ(r.width, 400);
    EXPECT_EQ(r.height, 300);
    EXPECT_FALSE(r.clamped);
    EXPECT_STREQ(r.clampedBy, kClampedByNone);
}

// ============================================
// SizeMatchesWithinTolerance
// ============================================

TEST(WindowGeometryTolerance, ExactMatch) {
    EXPECT_TRUE(SizeMatchesWithinTolerance(800, 800));
}

TEST(WindowGeometryTolerance, DefaultToleranceIsOnePixel) {
    EXPECT_TRUE(SizeMatchesWithinTolerance(801, 800));
    EXPECT_TRUE(SizeMatchesWithinTolerance(799, 800));
    EXPECT_FALSE(SizeMatchesWithinTolerance(802, 800));
    EXPECT_FALSE(SizeMatchesWithinTolerance(798, 800));
}

TEST(WindowGeometryTolerance, ExplicitToleranceIsSymmetric) {
    EXPECT_TRUE(SizeMatchesWithinTolerance(795, 800, 5));
    EXPECT_TRUE(SizeMatchesWithinTolerance(805, 800, 5));
    EXPECT_FALSE(SizeMatchesWithinTolerance(806, 800, 5));
}

TEST(WindowGeometryTolerance, ZeroToleranceRequiresExact) {
    EXPECT_TRUE(SizeMatchesWithinTolerance(800, 800, 0));
    EXPECT_FALSE(SizeMatchesWithinTolerance(801, 800, 0));
}

TEST(WindowGeometryTolerance, NegativeToleranceTreatedAsZero) {
    EXPECT_TRUE(SizeMatchesWithinTolerance(800, 800, -5));
    EXPECT_FALSE(SizeMatchesWithinTolerance(801, 800, -5));
}
