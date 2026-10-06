#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace media {

struct DocumentOwner {
    // 三项共同区分窗口重建、文档导航和 WebView 重建；仅凭 HWND 无法隔离迟到请求。
    std::uint64_t hostSerial = 0;
    std::uint64_t navigationGeneration = 0;
    std::uint64_t hostGeneration = 0;
    bool operator==(const DocumentOwner&) const = default;
};

struct FileIdentity {
    std::uint64_t size = 0;  // 字节数。
    std::uint64_t mtime = 0;  // Windows FILETIME 原值，仅用于相等比较，不是 Unix 时间。
    bool operator==(const FileIdentity&) const = default;
};

struct MediaToken {
    DocumentOwner owner;
    std::string origin;
    std::wstring path;
    FileIdentity identity;
    std::string mimeType;
    std::uint64_t lastUsed = 0;
    std::uint64_t ordinal = 0;
};

// 仅在主线程访问，worker 只取副本。令牌不按时间过期；文档失效、文件变化或 LRU 淘汰时撤销。
class MediaTokens {
public:
    using TokenSource = std::function<std::string()>;
    using Clock = std::function<std::uint64_t()>;

    MediaTokens(TokenSource source, Clock clock, std::size_t perDocumentLimit = 64)
        : source_(std::move(source)), clock_(std::move(clock)), limit_(perDocumentLimit) {}

    std::optional<std::string> Mint(DocumentOwner owner, std::string origin, std::wstring path,
                                    FileIdentity identity, std::string mimeType) {
        if (limit_ == 0) return std::nullopt;
        std::string token;
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            token = source_();
            if (IsToken(token) && !entries_.contains(token)) break;
            token.clear();
        }
        if (token.empty()) return std::nullopt;
        std::size_t count = 0;
        auto oldest = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            const auto& entry = it->second;
            if (entry.owner != owner) continue;
            ++count;
            if (oldest == entries_.end() || entry.lastUsed < oldest->second.lastUsed ||
                (entry.lastUsed == oldest->second.lastUsed && entry.ordinal < oldest->second.ordinal)) oldest = it;
        }
        // 同一时钟刻度内仍用序号区分先后，且只淘汰当前文档的条目。
        if (count >= limit_ && oldest != entries_.end()) entries_.erase(oldest);
        ++ordinal_;
        entries_.try_emplace(token, owner, std::move(origin), std::move(path), identity,
            std::move(mimeType), clock_(), ordinal_);
        return token;
    }

    std::optional<MediaToken> Find(const std::string& token, DocumentOwner owner) const {
        const auto found = entries_.find(token);
        if (found == entries_.end() || found->second.owner != owner) return std::nullopt;
        return found->second;
    }

    void Touch(const std::string& token) {
        if (auto found = entries_.find(token); found != entries_.end()) {
            found->second.lastUsed = clock_();
            found->second.ordinal = ++ordinal_;
        }
    }
    void Revoke(const std::string& token) { entries_.erase(token); }
    void InvalidateOwner(DocumentOwner owner) {
        std::erase_if(entries_, [owner](const auto& pair) { return pair.second.owner == owner; });
    }
    void InvalidateHost(std::uint64_t serial) {
        std::erase_if(entries_, [serial](const auto& pair) { return pair.second.owner.hostSerial == serial; });
    }
    void ClearAll() { entries_.clear(); }

private:
    static bool IsToken(const std::string& token) {
        return token.size() == 32 && std::ranges::all_of(token, [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    }
    TokenSource source_;
    Clock clock_;
    std::size_t limit_;
    std::uint64_t ordinal_ = 0;
    std::unordered_map<std::string, MediaToken> entries_;
};

}  // namespace media
