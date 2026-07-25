#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace artwork_request {

enum class TokenKind : std::size_t {
    WebResourceRequested = 0,
    NavigationStarting,
    NavigationCompleted,
    Count,
};

enum class TerminalReason {
    Completed,
    NavigationSuperseded,
    HostSuperseded,
    HostClosed,
};

enum class CompletionResult {
    Accepted,
    Stale,
    AlreadyTerminal,
    UnknownRequest,
};

struct RequestCapture {
    std::uint64_t requestId = 0;
    std::uint64_t navigationGeneration = 0;
    std::uint64_t hostGeneration = 0;
};

struct TokenRegistrationState {
    bool registered = false;
    std::uint64_t addCount = 0;
    std::uint64_t removeCount = 0;
};

class ArtworkRequestLifecycle {
public:
    std::uint64_t OnNavigationStarting() {
        std::scoped_lock lock(mutex_);
        if (!CanAccessHostLocked()) {
            return navigationGeneration_;
        }
        return ++navigationGeneration_;
    }

    std::uint64_t OnNavigationCompleted() const {
        std::scoped_lock lock(mutex_);
        return navigationGeneration_;
    }

    std::uint64_t OnControllerCreated() {
        std::scoped_lock lock(mutex_);
        if (closing_) {
            return hostGeneration_;
        }
        alive_ = true;
        return ++hostGeneration_;
    }

    std::uint64_t OnControllerRecreated() {
        return ReplaceController();
    }

    std::uint64_t OnControllerRecovery() {
        return ReplaceController();
    }

    std::uint64_t OnClose() {
        std::scoped_lock lock(mutex_);
        if (closing_) {
            return hostGeneration_;
        }
        closing_ = true;
        alive_ = false;
        RemoveAllTokensLocked();
        return ++hostGeneration_;
    }

    bool RecordTokenAdd(TokenKind kind, bool addSucceeded) {
        std::scoped_lock lock(mutex_);
        if (!addSucceeded || !CanAccessHostLocked()) {
            return false;
        }

        auto& token = tokens_[TokenIndex(kind)];
        if (token.registered) {
            return false;
        }
        token.registered = true;
        ++token.addCount;
        return true;
    }

    bool RemoveToken(TokenKind kind) {
        std::scoped_lock lock(mutex_);
        return RemoveTokenLocked(kind);
    }

    std::optional<RequestCapture> CaptureRequest() {
        std::scoped_lock lock(mutex_);
        if (!CanAccessHostLocked()) {
            return std::nullopt;
        }

        RequestCapture capture{
            ++nextRequestId_,
            navigationGeneration_,
            hostGeneration_,
        };
        requests_.emplace(capture.requestId, RequestState{capture, std::nullopt});
        return capture;
    }

    bool CanCallbackAccessHost(const RequestCapture& capture) const {
        std::scoped_lock lock(mutex_);
        return CanAccessHostLocked() && IsCurrentLocked(capture);
    }

    CompletionResult Complete(const RequestCapture& capture) {
        std::scoped_lock lock(mutex_);
        const auto it = requests_.find(capture.requestId);
        if (it == requests_.end() || !SameCapture(it->second.capture, capture)) {
            return CompletionResult::UnknownRequest;
        }
        if (it->second.terminalReason.has_value()) {
            return CompletionResult::AlreadyTerminal;
        }

        const auto reason = TerminalReasonForLocked(capture);
        it->second.terminalReason = reason;
        return reason == TerminalReason::Completed
            ? CompletionResult::Accepted
            : CompletionResult::Stale;
    }

    std::optional<TerminalReason> GetTerminalReason(std::uint64_t requestId) const {
        std::scoped_lock lock(mutex_);
        const auto it = requests_.find(requestId);
        if (it == requests_.end()) {
            return std::nullopt;
        }
        return it->second.terminalReason;
    }

    TokenRegistrationState GetTokenState(TokenKind kind) const {
        std::scoped_lock lock(mutex_);
        return tokens_[TokenIndex(kind)];
    }

    std::uint64_t NavigationGeneration() const {
        std::scoped_lock lock(mutex_);
        return navigationGeneration_;
    }

    std::uint64_t HostGeneration() const {
        std::scoped_lock lock(mutex_);
        return hostGeneration_;
    }

    bool IsAlive() const {
        std::scoped_lock lock(mutex_);
        return alive_;
    }

    bool IsClosing() const {
        std::scoped_lock lock(mutex_);
        return closing_;
    }

private:
    static constexpr std::size_t kTokenKindCount =
        static_cast<std::size_t>(TokenKind::Count);

    struct RequestState {
        RequestCapture capture;
        std::optional<TerminalReason> terminalReason;
    };

    static constexpr std::size_t TokenIndex(TokenKind kind) {
        return static_cast<std::size_t>(kind);
    }

    static bool SameCapture(const RequestCapture& left, const RequestCapture& right) {
        return left.requestId == right.requestId
            && left.navigationGeneration == right.navigationGeneration
            && left.hostGeneration == right.hostGeneration;
    }

    bool CanAccessHostLocked() const {
        return alive_ && !closing_;
    }

    bool IsCurrentLocked(const RequestCapture& capture) const {
        return capture.navigationGeneration == navigationGeneration_
            && capture.hostGeneration == hostGeneration_;
    }

    TerminalReason TerminalReasonForLocked(const RequestCapture& capture) const {
        if (!CanAccessHostLocked()) {
            return TerminalReason::HostClosed;
        }
        if (capture.hostGeneration != hostGeneration_) {
            return TerminalReason::HostSuperseded;
        }
        if (capture.navigationGeneration != navigationGeneration_) {
            return TerminalReason::NavigationSuperseded;
        }
        return TerminalReason::Completed;
    }

    bool RemoveTokenLocked(TokenKind kind) {
        auto& token = tokens_[TokenIndex(kind)];
        if (!token.registered) {
            return false;
        }
        token.registered = false;
        ++token.removeCount;
        return true;
    }

    void RemoveAllTokensLocked() {
        for (std::size_t index = 0; index < kTokenKindCount; ++index) {
            auto& token = tokens_[index];
            if (token.registered) {
                token.registered = false;
                ++token.removeCount;
            }
        }
    }

    std::uint64_t ReplaceController() {
        std::scoped_lock lock(mutex_);
        if (closing_) {
            return hostGeneration_;
        }
        RemoveAllTokensLocked();
        alive_ = true;
        return ++hostGeneration_;
    }

    mutable std::mutex mutex_;
    std::uint64_t navigationGeneration_ = 0;
    std::uint64_t hostGeneration_ = 0;
    std::uint64_t nextRequestId_ = 0;
    bool alive_ = false;
    bool closing_ = false;
    std::array<TokenRegistrationState, kTokenKindCount> tokens_{};
    std::unordered_map<std::uint64_t, RequestState> requests_;
};

}  // namespace artwork_request