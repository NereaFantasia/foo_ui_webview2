// DragTokenStore.h - mints and redeems the one-shot tokens that authorise a drag-out.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fb2k_dnd {

// Milliseconds a freshly minted token stays usable. Sized for "user presses the
// button, hesitates, then starts dragging": the page must mint before dragstart
// because dataTransfer is read-only once that handler returns.
constexpr int64_t kDragTokenTtlMs = 30000;

// The text/plain payload a page must put in dataTransfer: this prefix followed
// by the token. Its presence is what tells an ordinary page drag apart from a
// drag-out this component is meant to take over, so it is contract, not detail.
// The TypeScript side carries its own copy of this literal; change both or
// neither.
constexpr wchar_t kDragTokenPrefix[] = L"fb2k-dnd-token/1:";

// Maps a token handed to the page back to the paths that already cleared the
// origin and path gates.
//
// At drag time the page sends a token, never paths, and this store is the only
// place the corresponding paths come from. That is what lets the drag-start
// handler avoid trusting anything the page put in dataTransfer.
//
// Threading: both entry points run on the main thread, one from the invoke
// handler and one from the drag-start handler, so there is no locking here. The
// drag thread never reaches this store; it is handed its own copy of the paths.
//
// The clock and the token source are injected rather than read here, so this
// file stays free of fb2k SDK and Win32 dependencies and can be unit-tested.
// Production must supply a monotonic clock and a cryptographically strong
// random source; a counter, a timestamp or a hash of the paths would all be
// predictable enough to forge.
class DragTokenStore {
public:
    using Clock = std::function<int64_t()>;            // monotonic milliseconds
    using TokenSource = std::function<std::string()>;  // high-entropy string

    DragTokenStore(Clock clock, TokenSource source);

    // Records paths against a fresh token and returns it, or an empty string
    // when nothing could be minted: an empty path list, or an injected source
    // that produced no token. Callers must treat an empty return as a failure
    // and must not proceed with the drag.
    //
    // Replaces any token this window already holds, because a page able to
    // stockpile tokens would defeat single use and the TTL together.
    std::string Mint(intptr_t ownerWindow, std::vector<std::wstring> nativePaths);

    // Hands back the paths minted under token and consumes it, so a second
    // attempt fails. Also fails for an unknown or expired token, and for one
    // minted by a window other than callerWindow. outPaths is left untouched
    // unless the call succeeds.
    bool Consume(const std::string& token, intptr_t callerWindow,
                 std::vector<std::wstring>& outPaths);

    // Drops the token a closing window holds, so a recycled HWND cannot inherit
    // it before the TTL would have expired it.
    void ClearWindow(intptr_t ownerWindow);

    void Clear();

private:
    struct Entry {
        std::string token;
        int64_t mintedAtMs = 0;
        std::vector<std::wstring> paths;
    };

    Clock clock_;
    TokenSource source_;

    // Keyed by window rather than by token, which makes two of the guarantees
    // structural rather than checked: a window cannot hold two tokens at once,
    // and a token cannot be redeemed from a window that did not mint it.
    std::unordered_map<intptr_t, Entry> byWindow_;
};

}  // namespace fb2k_dnd
