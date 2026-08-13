#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace menu_overlay_geometry {

constexpr std::int64_t kMaxMeasuredEdgePx = 100000;
constexpr std::int64_t kMaxPanelCoordinatePx = kMaxMeasuredEdgePx * 2;

struct Size {
    std::int64_t w = 0;
    std::int64_t h = 0;
};

struct MeasureReport {
    Size root;
    Size submenu;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Placement {
    int viewportW = 0;
    int viewportH = 0;
    int rootSlotW = 0;
    int subSlotW = 0;
    int rootTop = 0;
    int rootVisibleH = 0;
    // 水平翻转形态：true = 子菜单预留槽在虚拟画布【左】侧、根面板在右
    //（光标贴近屏幕右缘时的原生语义：根菜单边缘贴光标、子菜单向左展开）。
    // false = 现状：根在左、子槽在右。由锚定计算决定，ComputePlacement 不感知。
    bool subOnLeft = false;
};

// 根面板在虚拟画布内的 X 起点（子槽在左时根被推到子槽右侧）。
inline int RootSlotX(const Placement& placement) {
    return placement.subOnLeft ? placement.subSlotW : 0;
}

struct SubmenuPanelRequest {
    bool visible = false;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t w = 0;
    std::int64_t h = 0;
    std::uint64_t sequence = 0;
};

struct SubmenuPanelUpdate {
    bool accepted = false;
    std::optional<Rect> submenu;
    // Independent DWM-backed HWND extents. Unlike `regions`, each extent is a
    // complete rectangle, so it can be used as an actual native window bound
    // without relying on SetWindowRgn to crop a system backdrop.
    Rect rootWindow;
    std::optional<Rect> submenuWindow;
};

// Root-close policy is deliberately independent from HWND operations so the
// lifecycle rule remains testable: a whole-menu close always removes the
// separate submenu surface immediately, while only the root may fade out.
// The child must neither restore root focus nor notify the root renderer here;
// either action could reactivate a stale layer during a root close animation.
struct RootClosePolicy {
    bool animateRoot = false;
    bool hideSubmenuImmediately = true;
    bool clearSubmenuStateImmediately = true;
    bool restoreRootFocus = false;
    bool notifyRootRenderer = false;
};

inline bool IsAnimatedRootCloseReason(std::string_view reason) {
    return reason == "outside" || reason == "escape" || reason == "select" || reason == "blur";
}

inline RootClosePolicy ResolveRootClosePolicy(std::string_view reason,
                                              bool rootWindowValid,
                                              int closeAnimationMs) {
    RootClosePolicy policy;
    policy.animateRoot = rootWindowValid && closeAnimationMs > 0 &&
        IsAnimatedRootCloseReason(reason);
    return policy;
}

inline bool IsValidPositiveSize(const Size& size) {
    return size.w > 0 && size.h > 0 &&
        size.w <= kMaxMeasuredEdgePx && size.h <= kMaxMeasuredEdgePx;
}

inline bool IsValidMeasureReport(const MeasureReport& report, bool hasFirstLevelSubmenu) {
    if (!IsValidPositiveSize(report.root)) return false;
    if (!hasFirstLevelSubmenu) {
        return report.submenu.w == 0 && report.submenu.h == 0;
    }
    return IsValidPositiveSize(report.submenu);
}

inline std::optional<Placement> ComputePlacement(const MeasureReport& report,
                                                 bool hasFirstLevelSubmenu,
                                                 int workWidth,
                                                 int workHeight) {
    if (workWidth <= 0 || workHeight <= 0 ||
        !IsValidMeasureReport(report, hasFirstLevelSubmenu)) {
        return std::nullopt;
    }

    const std::int64_t maxW = workWidth;
    const std::int64_t maxH = workHeight;
    std::int64_t rootSlotW = std::min(report.root.w, maxW);
    std::int64_t subSlotW = 0;

    if (hasFirstLevelSubmenu) {
        const std::int64_t desiredWidth = report.root.w + report.submenu.w;
        if (desiredWidth <= maxW) {
            rootSlotW = report.root.w;
            subSlotW = report.submenu.w;
        } else if (report.root.w <= maxW / 2) {
            rootSlotW = report.root.w;
            subSlotW = maxW - rootSlotW;
        } else if (report.submenu.w <= maxW / 2) {
            subSlotW = report.submenu.w;
            rootSlotW = maxW - subSlotW;
        } else {
            rootSlotW = maxW / 2;
            subSlotW = maxW - rootSlotW;
        }
    }

    const std::int64_t desiredHeight = hasFirstLevelSubmenu
        ? std::max(report.root.h, report.submenu.h)
        : report.root.h;
    const std::int64_t viewportH = std::min(desiredHeight, maxH);
    const std::int64_t rootVisibleH = std::min(report.root.h, viewportH);

    Placement placement;
    placement.rootSlotW = static_cast<int>(rootSlotW);
    placement.subSlotW = static_cast<int>(subSlotW);
    placement.viewportW = static_cast<int>(rootSlotW + subSlotW);
    placement.viewportH = static_cast<int>(viewportH);
    placement.rootVisibleH = static_cast<int>(rootVisibleH);
    placement.rootTop = static_cast<int>(viewportH - rootVisibleH);
    return placement;
}

inline Rect RootPanelRect(const Placement& placement) {
    return Rect{RootSlotX(placement), placement.rootTop,
                placement.rootSlotW, placement.rootVisibleH};
}

// The root surface is always a tight native window around the root panel. The
// virtual placement still reserves the maximum first-level submenu slot for
// monitor-boundary decisions, but that reservation is never part of the root
// HWND's DWM-backed rectangle.
inline Rect RootWindowExtent(const Placement& placement) {
    return RootPanelRect(placement);
}

inline std::optional<Rect> ClampSubmenuRect(const Placement& placement, const Rect& requested) {
    if (placement.subSlotW <= 0 || requested.w <= 0 || requested.h <= 0) {
        return std::nullopt;
    }
    const int maxW = placement.subSlotW;
    const int w = std::min(requested.w, maxW);
    // 子菜单面板恒贴根面板的展开侧边缘：子槽在右 = 面板左缘贴根右缘（x=rootSlotW）；
    // 子槽在左 = 面板右缘贴根左缘（x = 子槽宽 - 面板宽）。渲染器上报的 x 仅作
    // 校验输入，落位由本函数裁决。
    const int x = placement.subOnLeft ? std::max(0, placement.subSlotW - w)
                                      : placement.rootSlotW;
    const int h = std::min(requested.h, placement.viewportH);
    const int maxTop = std::max(0, placement.viewportH - h);
    const int y = std::clamp(requested.y, 0, maxTop);
    return Rect{x, y, w, h};
}

// A visible first-level submenu receives its own tight native window. Its
// coordinates remain in the virtual root/submenu layout so the host can keep
// the root's screen anchor fixed while choosing a left or right screen edge
// placement for the separate HWND.
inline std::optional<Rect> SubmenuWindowExtent(const Placement& placement,
                                                const Rect& requested) {
    return ClampSubmenuRect(placement, requested);
}

inline Rect TranslateRect(const Rect& rect, int dx, int dy) {
    return Rect{rect.x + dx, rect.y + dy, rect.w, rect.h};
}

struct Point {
    int x = 0;
    int y = 0;
};

// 根窗锚定策略（与 windowModel / ownerMode 正交）：
//   BottomUp = 托盘现状：根左 = 光标 x，根底 = 光标 y，整体向上展开（锚点是托盘图标，
//              菜单只可能朝屏幕内侧长）。
//   Cursor   = 标准右键菜单：根左上 = 光标，向下展开，空间不足才翻转。
enum class AnchorPolicy { BottomUp, Cursor };

inline AnchorPolicy ParseAnchorPolicy(std::string_view policy) {
    return policy == "cursor" ? AnchorPolicy::Cursor : AnchorPolicy::BottomUp;
}

// 锚定结果：虚拟画布左上角 + 是否采用"子槽在左"的翻转形态。
struct AnchoredOrigin {
    Point origin;
    bool subOnLeft = false;
};

// 计算根/子菜单虚拟画布左上角的屏幕物理像素坐标，并决定子菜单槽的展开侧。
// full = 目标显示器矩形（rcMonitor，可压任务栏），以左上原点 + 宽高表示。
// dropAlignLeft = SPI_GETMENUDROPALIGNMENT 左手模式：菜单优先向光标左侧展开。
inline AnchoredOrigin ComputeAnchoredOrigin(Point anchor, const Placement& placement,
                                            const Rect& full, AnchorPolicy policy,
                                            bool dropAlignLeft) {
    const int left = full.x;
    const int top = full.y;
    const int right = full.x + full.w;
    const int bottom = full.y + full.h;
    const int virtualW = placement.viewportW;
    const int virtualH = placement.viewportH;

    if (policy == AnchorPolicy::BottomUp) {
        // 与托盘现状逐行等价：右侧预留超屏时整组左移，左/上边界 clamp；
        // 下边界不参与（永远向上展开，底边即锚点）。子槽恒在右（现状形态）。
        int x = anchor.x;
        int y = anchor.y - virtualH;
        if (x + virtualW > right) x = right - virtualW;
        if (x < left) x = left;
        if (y < top) y = top;
        return AnchoredOrigin{Point{x, y}, false};
    }

    // Cursor：锚定语义作用于【根面板】而非整张画布——预留的子菜单槽不得改变
    // 根菜单与光标的相对位置（修复：右缘翻转曾按画布宽整体平移，根右缘离光标
    // 差一个子槽宽）。
    const int rootW = placement.rootSlotW;
    const int subW = placement.subSlotW;

    // 垂直：根顶贴光标向下展开；下方放不下且上方放得下 → 根底贴光标向上翻。
    int y = anchor.y - placement.rootTop;
    if (anchor.y + placement.rootVisibleH > bottom &&
        anchor.y - placement.rootVisibleH >= top) {
        y = anchor.y - placement.rootTop - placement.rootVisibleH;
    }

    // 水平候选形态（origin = 画布左上角）：
    //   rightAll = 根左贴光标、子槽在根右（现状形态）
    //   subLeft  = 根左贴光标、子槽换到根左（根仍贴光标，子菜单向左展开）
    //   flipped  = 根右贴光标、子槽在根左（原生右缘翻转）
    const bool fitsRightAll = anchor.x + rootW + subW <= right;
    const bool fitsSubLeft = (anchor.x + rootW <= right) && (anchor.x - subW >= left);
    const bool fitsFlipped = anchor.x - rootW - subW >= left;

    int x = anchor.x;
    bool subOnLeft = false;
    if (!dropAlignLeft) {
        if (fitsRightAll) {
            x = anchor.x;
        } else if (subW > 0 && fitsSubLeft) {
            x = anchor.x - subW;
            subOnLeft = true;
        } else if (fitsFlipped) {
            x = anchor.x - rootW - subW;
            subOnLeft = subW > 0;
        }
        // 都放不下：维持根左贴光标，交给最终 clamp（左上优先）。
    } else {
        // 左手模式镜像：优先根右贴光标向左展开，其次根贴光标子槽在左，最后回翻向右。
        if (fitsFlipped) {
            x = anchor.x - rootW - subW;
            subOnLeft = subW > 0;
        } else if (subW > 0 && fitsSubLeft) {
            x = anchor.x - subW;
            subOnLeft = true;
        } else if (fitsRightAll) {
            x = anchor.x;
        }
    }

    // 翻转后仍可能越界（菜单比屏幕大 / 锚点贴边）：最终 clamp 进 full，左上优先。
    x = std::max(left, std::min(x, right - virtualW));
    y = std::max(top, std::min(y, bottom - virtualH));
    return AnchoredOrigin{Point{x, y}, subOnLeft};
}

// 兼容包装（单矩形语义 = 无子菜单槽）：既有调用方与测试的稳定入口。
inline Point ComputeContentAnchorOrigin(Point anchor, int virtualW, int virtualH,
                                        const Rect& full, AnchorPolicy policy,
                                        bool dropAlignLeft) {
    Placement single;
    single.viewportW = virtualW;
    single.viewportH = virtualH;
    single.rootSlotW = virtualW;
    single.subSlotW = 0;
    single.rootTop = 0;
    single.rootVisibleH = virtualH;
    return ComputeAnchoredOrigin(anchor, single, full, policy, dropAlignLeft).origin;
}

inline bool IsValidSubmenuPanelCoordinates(const SubmenuPanelRequest& request) {
    if (request.x < -kMaxPanelCoordinatePx || request.y < -kMaxPanelCoordinatePx ||
        request.w <= 0 || request.h <= 0) {
        return false;
    }
    if (request.x > kMaxPanelCoordinatePx || request.y > kMaxPanelCoordinatePx ||
        request.w > kMaxMeasuredEdgePx || request.h > kMaxMeasuredEdgePx) {
        return false;
    }
    return request.x <= kMaxPanelCoordinatePx - request.w &&
        request.y <= kMaxPanelCoordinatePx - request.h;
}

// The renderer monotonically increments this for open and close reports. A
// stale asynchronous hover report cannot resurrect a submenu after a newer
// sibling-hover or close report.
inline bool IsNewerSubmenuPanelSequence(std::uint64_t previous, std::uint64_t candidate) {
    return candidate != 0 && candidate > previous;
}

// Pure gate for the overlay-private menu.__submenuPanel IPC. A request is
// accepted only for an established ContentSized placement. Raw renderer
// coordinates are range-checked before the placement clamp is evaluated, so a
// rejected request never produces a candidate region update.
inline SubmenuPanelUpdate EvaluateSubmenuPanelUpdate(
    bool isContentSized,
    const std::optional<Placement>& placement,
    const SubmenuPanelRequest& request) {
    SubmenuPanelUpdate update;
    if (!isContentSized || !placement.has_value()) return update;
    if (!IsNewerSubmenuPanelSequence(0, request.sequence)) return update;

    if (!request.visible) {
        update.accepted = true;
        update.rootWindow = RootWindowExtent(*placement);
        return update;
    }
    if (!IsValidSubmenuPanelCoordinates(request)) return update;

    const Rect requested{
        static_cast<int>(request.x), static_cast<int>(request.y),
        static_cast<int>(request.w), static_cast<int>(request.h)
    };
    update.submenu = ClampSubmenuRect(*placement, requested);
    if (!update.submenu.has_value()) return SubmenuPanelUpdate{};
    update.accepted = true;
    update.rootWindow = RootWindowExtent(*placement);
    update.submenuWindow = SubmenuWindowExtent(*placement, requested);
    return update;
}

class MeasureGate {
public:
    void Begin() { awaiting_ = true; }
    void Cancel() { awaiting_ = false; }
    bool IsAwaiting() const { return awaiting_; }

    bool TryConsume(bool valid) {
        if (!awaiting_ || !valid) return false;
        awaiting_ = false;
        return true;
    }

private:
    bool awaiting_ = false;
};

}  // namespace menu_overlay_geometry