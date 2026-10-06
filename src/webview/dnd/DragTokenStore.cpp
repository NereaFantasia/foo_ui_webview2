// DragTokenStore.cpp
#include "pch.h"
#include "webview/dnd/DragTokenStore.h"

#include <utility>

namespace fb2k_dnd {

DragTokenStore::DragTokenStore(Clock clock, TokenSource source)
    : clock_(std::move(clock)), source_(std::move(source)) {}

std::string DragTokenStore::Mint(intptr_t ownerWindow,
                                 std::vector<std::wstring> nativePaths) {
    if (nativePaths.empty() || !clock_ || !source_) {
        return {};
    }

    // A source that yields nothing must fail the mint rather than fall back to
    // something guessable; the token is the only thing standing between a page
    // and a drag it did not get authorised for.
    std::string token = source_();
    if (token.empty()) {
        return {};
    }

    Entry entry;
    entry.token = token;
    entry.mintedAtMs = clock_();
    entry.paths = std::move(nativePaths);

    // Assignment rather than an insert that could fail: whatever this window
    // still held is replaced, which is what stops a page stockpiling tokens.
    byWindow_[ownerWindow] = std::move(entry);
    return token;
}

bool DragTokenStore::Consume(const std::string& token, intptr_t callerWindow,
                             std::vector<std::wstring>& outPaths) {
    if (token.empty() || !clock_) {
        return false;
    }

    const auto it = byWindow_.find(callerWindow);
    if (it == byWindow_.end()) {
        return false;
    }

    // A mismatch leaves the entry alone. The page chooses the text the
    // drag-start handler reads, so letting a wrong value consume the real token
    // would hand it a way to cancel a drag the user legitimately started.
    if (it->second.token != token) {
        return false;
    }

    // A clock that appears to run backwards is treated as expiry too: the only
    // safe reading of an unusable clock is that the token is no longer fresh.
    const int64_t elapsed = clock_() - it->second.mintedAtMs;
    if (elapsed < 0 || elapsed >= kDragTokenTtlMs) {
        byWindow_.erase(it);
        return false;
    }

    outPaths = std::move(it->second.paths);
    byWindow_.erase(it);
    return true;
}

void DragTokenStore::ClearWindow(intptr_t ownerWindow) {
    byWindow_.erase(ownerWindow);
}

void DragTokenStore::Clear() {
    byWindow_.clear();
}

}  // namespace fb2k_dnd
