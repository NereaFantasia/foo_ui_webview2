#pragma once

// ============================================
// JitSessionModel.h - jitSession 会话的写凭据、重试去重、操作记录与尝试结局
//
// 只做决策与记录，不依赖 foobar2000 SDK：令牌与 ID 由调用方生成后传入，文档是否仍有效、
// 播放停止原因等事实也由调用方读出后传入。会话层（JitSession）持有这些对象并负责加锁。
// ============================================

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace jit_session {

// 写命令的凭据校验结果。NotOwner：令牌不是当前控制者的；Conflict：版本号已过期。
enum class WriteCheck { Ok, NotOwner, Conflict };

inline WriteCheck CheckWrite(const std::string& currentToken, std::uint64_t currentRevision,
                             const std::string& token, std::uint64_t expectedRevision) {
    if (currentToken.empty() || token != currentToken) return WriteCheck::NotOwner;
    if (expectedRevision != currentRevision) return WriteCheck::Conflict;
    return WriteCheck::Ok;
}

enum class ClaimMode { Resume, Handover };

// 认领控制的判定。Resume 只给同一窗口里重载后的新文档：原控制文档已失效，且来自同一
// windowId。Handover 给任何窗口，但必须说中当前的 ownerEpoch，不能凭 sessionId 静默抢占。
enum class ClaimCheck { Ok, NotOwner, Conflict };

inline ClaimCheck CheckClaim(ClaimMode mode, bool sameWindow, bool ownerDocumentAlive,
                             bool hasExpectedEpoch, std::uint64_t expectedEpoch, std::uint64_t ownerEpoch) {
    if (mode == ClaimMode::Resume) {
        return (sameWindow && !ownerDocumentAlive) ? ClaimCheck::Ok : ClaimCheck::NotOwner;
    }
    if (!hasExpectedEpoch || expectedEpoch != ownerEpoch) return ClaimCheck::Conflict;
    return ClaimCheck::Ok;
}

// 同一 requestId 的重试返回第一次的应答；换了载荷则冲突。最近 kRecent 条留完整应答，更早的
// 只留 requestId 的哈希，重用时报冲突而不是当新命令执行。
class RequestLog {
public:
    static constexpr std::size_t kRecent = 1024;

    enum class Hit { New, Replay, Conflict };

    // digest 是调用方对载荷（不含凭据）算的摘要；Replay 时 *response 为第一次的应答。
    Hit Check(const std::string& requestId, std::uint64_t digest, std::string* response) const {
        for (const Entry& entry : m_recent) {
            if (entry.requestId != requestId) continue;
            if (entry.digest != digest) return Hit::Conflict;
            if (response) *response = entry.response;
            return Hit::Replay;
        }
        if (m_retired.count(std::hash<std::string>{}(requestId)) != 0) return Hit::Conflict;
        return Hit::New;
    }

    void Record(const std::string& requestId, std::uint64_t digest, std::string response) {
        if (m_recent.size() == kRecent) {
            // ponytail: retired ids are kept as hashes for the session's lifetime; a session would need
            // millions of commands before this set matters
            m_retired.insert(std::hash<std::string>{}(m_recent.front().requestId));
            m_recent.pop_front();
        }
        m_recent.push_back({requestId, digest, std::move(response)});
    }

private:
    struct Entry {
        std::string requestId;
        std::uint64_t digest = 0;
        std::string response;
    };
    std::deque<Entry> m_recent;
    std::unordered_set<std::size_t> m_retired;
};

enum class OpKind { Play, SetNext, Skip };
enum class OpStatus { Pending, Completed, Failed, Superseded };

struct Operation {
    std::string id;
    OpKind kind = OpKind::Play;
    std::string entryId;
    std::string attemptId;
    OpStatus status = OpStatus::Pending;
    std::string reason;
};

// 操作记录：每个操作恰好一个终态。未结束的全部保留，已结束的保留最近 kFinished 个，更早的
// 查不到。
class OperationLog {
public:
    static constexpr std::size_t kFinished = 64;

    void Begin(Operation op) {
        op.status = OpStatus::Pending;
        m_ops.push_back(std::move(op));
    }

    // 把未结束的操作落到终态；已结束或查不到时返回 false，不改动记录。
    bool Finish(const std::string& id, OpStatus status, std::string reason = {}) {
        if (status == OpStatus::Pending) return false;
        Operation* op = FindMutable(id);
        if (!op || op->status != OpStatus::Pending) return false;
        op->status = status;
        op->reason = std::move(reason);
        // 完成顺序可能不同于受理顺序；把刚完成的记录移到末尾，保留未结束操作的相对顺序。
        const auto finished = std::find_if(m_ops.begin(), m_ops.end(),
                                           [op](const Operation& item) { return &item == op; });
        std::rotate(finished, std::next(finished), m_ops.end());
        Trim();
        return true;
    }

    const Operation* Find(const std::string& id) const {
        for (const Operation& op : m_ops) {
            if (op.id == id) return &op;
        }
        return nullptr;
    }

    // 未结束操作的 ID，按受理顺序
    std::deque<std::string> PendingIds() const {
        std::deque<std::string> ids;
        for (const Operation& op : m_ops) {
            if (op.status == OpStatus::Pending) ids.push_back(op.id);
        }
        return ids;
    }

private:
    Operation* FindMutable(const std::string& id) {
        for (Operation& op : m_ops) {
            if (op.id == id) return &op;
        }
        return nullptr;
    }

    void Trim() {
        std::size_t finished = 0;
        for (const Operation& op : m_ops) {
            if (op.status != OpStatus::Pending) ++finished;
        }
        for (auto it = m_ops.begin(); finished > kFinished && it != m_ops.end();) {
            if (it->status != OpStatus::Pending) {
                it = m_ops.erase(it);
                --finished;
            } else {
                ++it;
            }
        }
    }

    std::deque<Operation> m_ops;
};

// 一次尝试的结局。Superseded 与 Released 由会话层按发生的命令判定，不来自停止原因。
enum class Outcome { Completed, Stopped, Superseded, Released, Unknown };

enum class StopKind { EndOfFile, User, Other };

// eof 时最后读数离时长不超过 1 秒才算完成，否则报 Unknown，不推断失败原因。
// 位置按整秒回报，所以取闭区间。
inline Outcome ClassifyStop(StopKind stop, double lastPosition, double duration) {
    if (stop == StopKind::User) return Outcome::Stopped;
    if (stop == StopKind::EndOfFile && duration > 0 && duration - lastPosition <= 1.0) return Outcome::Completed;
    return Outcome::Unknown;
}

// 失败复查只针对以 unknown 结束且没播满这么久的尝试：再往后的失败是中途断流或解码出错，
// 重开资源复现不了。
constexpr double kRecheckBefore = 1.0;

// fb2k 记录播放失败时写两行：本地化的说明，以 "):" 结尾，下一行是带引号的资源路径。只认这个
// 形状并取出引号里的路径，不解析说明文字；形状不符时为空。
inline std::optional<std::string> EngineFailurePath(std::string_view text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.remove_suffix(1);
    const std::size_t newline = text.rfind('\n');
    if (newline == std::string_view::npos) return std::nullopt;
    std::string_view head = text.substr(0, newline);
    if (!head.empty() && head.back() == '\r') head.remove_suffix(1);
    const std::string_view tail = text.substr(newline + 1);
    if (head.size() < 2 || head.substr(head.size() - 2) != "):") return std::nullopt;
    if (tail.size() < 3 || tail.front() != '"' || tail.back() != '"') return std::nullopt;
    return std::string(tail.substr(1, tail.size() - 2));
}

// 控制台与 handle 写同一资源的形式不同：控制台没有 file:// 前缀，分轨带 |subsong:N。比对前两边
// 都去掉这两处，并忽略 ASCII 大小写。
inline std::string ComparableResourcePath(std::string_view path) {
    constexpr std::string_view kFile = "file://";
    if (path.substr(0, kFile.size()) == kFile) path.remove_prefix(kFile.size());
    if (const std::size_t marker = path.rfind("|subsong:"); marker != std::string_view::npos) path = path.substr(0, marker);
    std::string out(path);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

}  // namespace jit_session
