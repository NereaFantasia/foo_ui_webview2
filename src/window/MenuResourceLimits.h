#pragma once
// MenuResourceLimits.h - defensive resource caps for tray / self-drawn menus
// (DESIGN 8.5). Header-only so GoogleTest can drive the same preflight the API
// layer uses. Counts are structural (per-field sizes / node walks), never
// json.dump() byte length.

#include <nlohmann/json.hpp>
#include <string>

namespace menu_limits {

inline constexpr long long kMaxMenuItems = 512;
inline constexpr long long kMaxShowMenuDepth = 8;       // legacy menu.show only
inline constexpr long long kMaxSegmentOptions = 64;
inline constexpr long long kMaxSingleSvgBytes = 32LL * 1024;
inline constexpr long long kMaxTotalSvgBytes = 256LL * 1024;
inline constexpr long long kMaxCssBytes = 256LL * 1024;

struct CheckResult {
    bool ok = true;
    std::string field;
    long long limit = 0;
    long long actual = 0;

    static CheckResult Ok() { return {}; }
    static CheckResult Breach(const char* field, long long limit, long long actual) {
        CheckResult r;
        r.ok = false;
        r.field = field;
        r.limit = limit;
        r.actual = actual;
        return r;
    }
};

// Drop a single SVG whose content exceeds 32 KiB (clear viewBox + content).
// Oversized singles are discarded and do NOT count toward the 256 KiB total.
inline bool StripOneSvgIfOversized(std::string& viewBox, std::string& content) {
    if (static_cast<long long>(content.size()) <= kMaxSingleSvgBytes) return false;
    viewBox.clear();
    content.clear();
    return true;
}

inline void StripOversizedSvgInJsonItem(nlohmann::json& item) {
    if (!item.is_object()) return;
    if (item.contains("iconSvg") && item["iconSvg"].is_object()) {
        auto& iv = item["iconSvg"];
        std::string vb = iv.value("viewBox", "");
        std::string ct = iv.value("content", "");
        if (StripOneSvgIfOversized(vb, ct)) {
            iv["viewBox"] = vb;
            iv["content"] = ct;
        }
    }
    if (item.contains("segments") && item["segments"].is_array()) {
        for (auto& seg : item["segments"]) {
            if (!seg.is_object()) continue;
            if (!seg.contains("iconSvg") || !seg["iconSvg"].is_object()) continue;
            auto& iv = seg["iconSvg"];
            std::string vb = iv.value("viewBox", "");
            std::string ct = iv.value("content", "");
            if (StripOneSvgIfOversized(vb, ct)) {
                iv["viewBox"] = vb;
                iv["content"] = ct;
            }
        }
    }
    if (item.contains("submenu") && item["submenu"].is_array()) {
        for (auto& sub : item["submenu"]) StripOversizedSvgInJsonItem(sub);
    }
}

inline void StripOversizedSvgInJsonItems(nlohmann::json& items) {
    if (!items.is_array()) return;
    for (auto& it : items) StripOversizedSvgInJsonItem(it);
}

namespace detail {

inline void AddSvgBytes(long long& total, const std::string& content, CheckResult& breach) {
    if (!breach.ok) return;
    const long long n = static_cast<long long>(content.size());
    if (n <= 0) return;
    // Caller must already have stripped >32 KiB singles; remaining always count.
    total += n;
    if (total > kMaxTotalSvgBytes) {
        breach = CheckResult::Breach("svgTotal", kMaxTotalSvgBytes, total);
    }
}

inline void TallyJsonItem(const nlohmann::json& item, int depth, long long& itemCount,
                          long long& svgTotal, CheckResult& breach) {
    if (!breach.ok) return;
    if (depth > kMaxShowMenuDepth) {
        breach = CheckResult::Breach("depth", kMaxShowMenuDepth, depth);
        return;
    }
    ++itemCount;
    if (itemCount > kMaxMenuItems) {
        breach = CheckResult::Breach("items", kMaxMenuItems, itemCount);
        return;
    }
    if (!item.is_object()) return;

    if (item.contains("iconSvg") && item["iconSvg"].is_object()) {
        AddSvgBytes(svgTotal, item["iconSvg"].value("content", ""), breach);
        if (!breach.ok) return;
    }

    if (item.contains("segments") && item["segments"].is_array()) {
        const long long segCount = static_cast<long long>(item["segments"].size());
        if (segCount > kMaxSegmentOptions) {
            breach = CheckResult::Breach("segments", kMaxSegmentOptions, segCount);
            return;
        }
        for (const auto& seg : item["segments"]) {
            if (!seg.is_object()) continue;
            if (seg.contains("iconSvg") && seg["iconSvg"].is_object()) {
                AddSvgBytes(svgTotal, seg["iconSvg"].value("content", ""), breach);
                if (!breach.ok) return;
            }
        }
    }

    if (item.contains("submenu") && item["submenu"].is_array()) {
        for (const auto& sub : item["submenu"]) {
            TallyJsonItem(sub, depth + 1, itemCount, svgTotal, breach);
            if (!breach.ok) return;
        }
    }
}

}  // namespace detail

// Shared css byte cap. Tray config css and menu.show css are the same renderer
// resource, so both preflights must report the same field/limit/actual breach
// instead of each caller rolling its own comparison.
inline CheckResult ValidateCssBytes(const std::string& css) {
    const long long cssBytes = static_cast<long long>(css.size());
    if (cssBytes > kMaxCssBytes) {
        return CheckResult::Breach("css", kMaxCssBytes, cssBytes);
    }
    return CheckResult::Ok();
}

// Validate menu.show items JSON AFTER StripOversizedSvgInJsonItems. Depth > 8
// fails (legacy menu.show). Does not check css (callers preflight css through
// ValidateCssBytes).
inline CheckResult ValidateShowMenuResources(const nlohmann::json& items) {
    if (!items.is_array()) {
        return CheckResult::Breach("items", kMaxMenuItems, 0);
    }
    CheckResult breach = CheckResult::Ok();
    long long itemCount = 0;
    long long svgTotal = 0;
    for (const auto& it : items) {
        detail::TallyJsonItem(it, /*depth=*/1, itemCount, svgTotal, breach);
        if (!breach.ok) return breach;
    }
    return breach;
}

inline nlohmann::json DetailsJson(const CheckResult& r) {
    return nlohmann::json{
        {"field", r.field},
        {"limit", r.limit},
        {"actual", r.actual},
    };
}

}  // namespace menu_limits
