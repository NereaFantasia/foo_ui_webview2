#include "pch.h"
#include "window/WindowGeometryMath.h"

namespace window_geometry {

namespace {

// 复刻 Win32 MulDiv 的取整语义（四舍五入，.5 远离零），但不依赖 Windows.h。
// 用 int64_t 中间量避免 value * numerator 溢出。
int MulDivRound(int value, int numerator, int denominator) {
    if (denominator == 0) return value;

    const std::int64_t product =
        static_cast<std::int64_t>(value) * static_cast<std::int64_t>(numerator);
    const std::int64_t den = static_cast<std::int64_t>(denominator);

    // 结果符号决定 .5 的取整方向：远离零。
    const bool negativeResult = (product < 0) != (den < 0);
    const std::int64_t absProduct = product < 0 ? -product : product;
    const std::int64_t absDen = den < 0 ? -den : den;

    const std::int64_t rounded = (absProduct + absDen / 2) / absDen;
    return static_cast<int>(negativeResult ? -rounded : rounded);
}

int NormalizeDpi(int dpi) {
    return dpi > 0 ? dpi : kBaselineDpi;
}

}  // namespace

int DipToPhysical(int dip, int dpi) {
    return MulDivRound(dip, NormalizeDpi(dpi), kBaselineDpi);
}

int PhysicalToDip(int physical, int dpi) {
    return MulDivRound(physical, kBaselineDpi, NormalizeDpi(dpi));
}

SizeClampResult ClampSize(int reqW, int reqH, int minW, int minH, int maxW, int maxH) {
    SizeClampResult result;
    result.width = reqW;
    result.height = reqH;

    bool hitMin = false;
    bool hitMax = false;

    // 步骤 1: 下限。min <= 0 视为该维度无下限。
    if (minW > 0 && result.width < minW) {
        result.width = minW;
        hitMin = true;
    }
    if (minH > 0 && result.height < minH) {
        result.height = minH;
        hitMin = true;
    }

    // 步骤 2: 上限。max <= 0 视为该维度无上限。
    // 在 min > max 的矛盾输入下，本步骤覆盖步骤 1 的结果（max 胜出）。
    if (maxW > 0 && result.width > maxW) {
        result.width = maxW;
        hitMax = true;
    }
    if (maxH > 0 && result.height > maxH) {
        result.height = maxH;
        hitMax = true;
    }

    result.clamped = hitMin || hitMax;
    if (hitMin && hitMax) {
        result.clampedBy = kClampedByBoth;
    } else if (hitMin) {
        result.clampedBy = kClampedByMin;
    } else if (hitMax) {
        result.clampedBy = kClampedByMax;
    } else {
        result.clampedBy = kClampedByNone;
    }

    return result;
}

bool SizeMatchesWithinTolerance(int actual, int expected, int tolerance) {
    const int allowed = tolerance < 0 ? 0 : tolerance;
    const int delta = actual - expected;
    return (delta < 0 ? -delta : delta) <= allowed;
}

}  // namespace window_geometry
