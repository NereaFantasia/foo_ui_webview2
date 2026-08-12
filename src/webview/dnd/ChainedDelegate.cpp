// ChainedDelegate.cpp
#include "pch.h"
#include "webview/dnd/ChainedDelegate.h"

namespace fb2k_dnd {
namespace {

// Undocumented ole32 implementation detail: the name RegisterDragDrop stores its
// IDropTarget argument under on the target window.
constexpr wchar_t kOleDropTargetProperty[] = L"OleDropTargetInterface";

// The WebView child window tree is three levels deep, so this leaves ample room
// while keeping the recursion bounded should a window ever parent itself.
constexpr int kMaxSearchDepth = 8;

// Whether the window was created by this process.
//
// Decisive for chaining: the property below holds a raw pointer that is only
// meaningful in the address space of whoever called RegisterDragDrop. WebView2
// creates its Chrome_WidgetWin_* windows in a separate browser process, so the
// pointer stored on them belongs to that process and calling through it here
// faults immediately. A window failing this check must be left alone.
bool IsOwnedByCurrentProcess(HWND window) {
    DWORD pid = 0;
    if (GetWindowThreadProcessId(window, &pid) == 0) {
        return false;
    }
    return pid == GetCurrentProcessId();
}

// Walks children only, in sibling order, descending into each before moving on.
// Only ever calls GetWindow with GW_CHILD and GW_HWNDNEXT, so the search can
// never reach a parent or a sibling of the window it started from.
HWND SearchChildren(HWND parent, int depth) {
    if (!parent || depth > kMaxSearchDepth) {
        return nullptr;
    }
    for (HWND child = GetWindow(parent, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT)) {
        // Descends into out-of-process children but never selects one, because
        // GetWindowDropTarget rejects them.
        if (GetWindowDropTarget(child) != nullptr) {
            return child;
        }
        if (HWND found = SearchChildren(child, depth + 1)) {
            return found;
        }
    }
    return nullptr;
}

}  // namespace

IDropTarget* GetWindowDropTarget(HWND window) {
    if (!window) {
        return nullptr;
    }
    // The ownership test guards the cast below, so it belongs here rather than in
    // each caller: the property holds a raw pointer valid only in the address
    // space that registered it, and calling through a foreign one faults.
    if (!IsOwnedByCurrentProcess(window)) {
        return nullptr;
    }
    return static_cast<IDropTarget*>(GetPropW(window, kOleDropTargetProperty));
}

HWND FindDescendantWithDropTarget(HWND host) {
    if (!host || !IsWindow(host)) {
        return nullptr;
    }
    // The host itself is skipped on purpose: in a panel layout it belongs to the
    // player's own window tree, and displacing a drop target there would take
    // over drag handling the player relies on.
    return SearchChildren(host, 0);
}

ChainedDelegate::ChainedDelegate(HWND target, wil::com_ptr<IDropTarget> inner)
    : target_(target), inner_(std::move(inner)) {}

HRESULT ChainedDelegate::Enter(IDataObject* data, DWORD keyState, const DragPoint& pt,
                               DWORD* effect) {
    if (!inner_) return E_FAIL;
    return inner_->DragEnter(data, keyState, pt.screen, effect);
}

HRESULT ChainedDelegate::Over(DWORD keyState, const DragPoint& pt, DWORD* effect) {
    if (!inner_) return E_FAIL;
    return inner_->DragOver(keyState, pt.screen, effect);
}

HRESULT ChainedDelegate::Leave() {
    if (!inner_) return E_FAIL;
    return inner_->DragLeave();
}

HRESULT ChainedDelegate::Drop(IDataObject* data, DWORD keyState, const DragPoint& pt,
                              DWORD* effect) {
    if (!inner_) return E_FAIL;
    return inner_->Drop(data, keyState, pt.screen, effect);
}

bool ChainedDelegate::IsValid() const {
    return inner_ != nullptr;
}

bool ChainedDelegate::Restore() {
    if (!inner_) {
        // Idempotent: reports what the first call reached instead of counting a
        // repeated call as a fresh failure.
        return restored_;
    }

    // Moved out before anything can fail, so every path below releases our
    // reference exactly once and destruction afterwards is a no-op. On success
    // OLE holds a reference of its own; otherwise keeping ours would only leak an
    // interface nothing can reach.
    const wil::com_ptr<IDropTarget> inner = std::move(inner_);
    const HWND target = target_;
    target_ = nullptr;

    if (!target || !IsWindow(target)) {
        // The window normally dies before teardown reaches this point, and one
        // that no longer exists cannot be left without a drop target.
        restored_ = true;
        return true;
    }

    // Our own registration is already revoked by now, so a target found here
    // belongs to someone else - Chromium re-registering around a navigation or a
    // renderer swap. Displacing it would orphan a live registration in order to
    // reinstate a stale one.
    if (GetWindowDropTarget(target) != nullptr) {
        LOG("ChainedDelegate: window already carries a drop target, restore skipped");
        restored_ = true;
        return true;
    }

    const HRESULT hr = RegisterDragDrop(target, inner.get());
    restored_ = SUCCEEDED(hr);
    if (!restored_) {
        LOG("ERROR: ChainedDelegate: failed to restore inner drop target, window "
            "left without one, hr=", pfc::format_hex((uint32_t)hr));
    }
    return restored_;
}

}  // namespace fb2k_dnd
