#pragma once

// ============================================
// JitShadowRows.h - JIT 影子列表的行归属、归属判定与添加代次
//
// 回调里的播放位置会滞后，同一资源的两行 handle 相同，所以按“哪一行属于哪个条目”
// 判定归属，不按路径或 handle。这里只做决策，不依赖 foobar2000 SDK：handle 以不透明键
// 表示，由 QueueManager 传入指针值；读写影子列表、发事件都在 QueueManager。
// ============================================

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace jit_shadow {

// 条目序号由 QueueManager 递增分配；0 表示无主。
using EntrySerial = std::uint64_t;
// metadb handle 的指针值，只做相等比较。
using HandleKey = std::uintptr_t;

inline constexpr std::size_t kNoRow = std::numeric_limits<std::size_t>::max();

enum class EntryKind : std::uint8_t {
    // playNow 或 enqueueNext 的一次调用；解析展开出的多行同属一个条目
    Single,
    // preloadBatch 插入的行，没有调用方标识
    Batch,
};

struct Row {
    EntrySerial entry = 0;
    EntryKind kind = EntryKind::Single;
    HandleKey handle = 0;
};

// 与影子列表逐行对齐的归属表。QueueManager 每次改列表都同步更新它；比对出外部修改后
// 作废，作废期间不再用于判定和删行，直到列表被清空重建。
class RowTable {
public:
    void Reset() {
        m_rows.clear();
        m_stale = false;
    }

    // 现有各行全部改记为一个批量条目。批量起播后，影子列表的任一行都算本会话的曲目。
    void ResetAsBatch(const std::vector<HandleKey>& actual, EntrySerial entry) {
        m_rows.clear();
        m_rows.reserve(actual.size());
        for (HandleKey handle : actual) {
            m_rows.push_back({entry, EntryKind::Batch, handle});
        }
        m_stale = false;
    }

    void MarkStale() { m_stale = true; }
    bool IsStale() const { return m_stale; }
    std::size_t Size() const { return m_rows.size(); }
    const Row& At(std::size_t index) const { return m_rows[index]; }

    // 与列表实际内容逐行比对，返回比对后是否可用。已登记的行一致、只在末尾多出行时，多出的行
    // 登记为无主行：外部往列表末尾追加曲目不结束会话。行数变少或任一已登记行不同就作废。
    bool Verify(const std::vector<HandleKey>& actual) {
        if (m_stale) return false;
        if (actual.size() < m_rows.size()) {
            m_stale = true;
            return false;
        }
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            if (actual[i] != m_rows[i].handle) {
                m_stale = true;
                return false;
            }
        }
        for (std::size_t i = m_rows.size(); i < actual.size(); ++i) {
            m_rows.push_back({0, EntryKind::Single, actual[i]});
        }
        return true;
    }

    void Insert(std::size_t at, EntrySerial entry, EntryKind kind, const std::vector<HandleKey>& handles) {
        if (m_stale) return;
        if (at > m_rows.size()) at = m_rows.size();
        std::vector<Row> added;
        added.reserve(handles.size());
        for (HandleKey handle : handles) {
            added.push_back({entry, kind, handle});
        }
        m_rows.insert(m_rows.begin() + static_cast<std::ptrdiff_t>(at), added.begin(), added.end());
    }

    // 删除 [first, first + count)，越界部分忽略。
    void Remove(std::size_t first, std::size_t count) {
        if (m_stale || first >= m_rows.size()) return;
        const std::size_t last = (count > m_rows.size() - first) ? m_rows.size() : first + count;
        m_rows.erase(m_rows.begin() + static_cast<std::ptrdiff_t>(first),
                     m_rows.begin() + static_cast<std::ptrdiff_t>(last));
    }

    std::size_t FirstRowOf(EntrySerial entry) const {
        if (m_stale || entry == 0) return kNoRow;
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].entry == entry) return i;
        }
        return kNoRow;
    }

    std::size_t LastRowOf(EntrySerial entry) const {
        if (m_stale || entry == 0) return kNoRow;
        for (std::size_t i = m_rows.size(); i > 0; --i) {
            if (m_rows[i - 1].entry == entry) return i - 1;
        }
        return kNoRow;
    }

    // 当前条目的行插在下一条目的第一行之前，没有下一条目时追加到末尾。rowCount 是列表的
    // 实际行数：作废后本表的行数不可信。
    std::size_t CurrentInsertPosition(EntrySerial nextEntry, std::size_t rowCount) const {
        const std::size_t first = FirstRowOf(nextEntry);
        return first == kNoRow ? rowCount : first;
    }

private:
    std::vector<Row> m_rows;
    bool m_stale = false;
};

struct PlayingLocation {
    // get_playing_item_location 成功
    bool located = false;
    // 播放所在的列表就是影子列表
    bool inShadow = false;
    std::size_t row = kNoRow;
};

enum class Attribution {
    // 不是本会话的曲目：会话不在 Idle 时让出控制
    Release,
    // 进入下一条目：下一条转为当前
    Advance,
    // 仍是当前条目：重播，或在同一条目展开出的行之间推进
    SameEntry,
    // 批量插入的行：两个 trackId 不变
    BatchRow,
};

// 判定新曲目的归属。table 须先经 Verify。sessionIdle 时批量行不重新激活会话，单条条目
// 的行会：停止但未清缓冲后，在影子列表里重播仍回到原会话。
inline Attribution Attribute(const RowTable& table, const PlayingLocation& location, EntrySerial currentEntry,
                             EntrySerial nextEntry, bool sessionIdle) {
    if (!location.located || !location.inShadow || table.IsStale()) return Attribution::Release;
    if (location.row >= table.Size()) return Attribution::Release;
    const Row& row = table.At(location.row);
    if (row.entry == 0) return Attribution::Release;
    if (row.kind == EntryKind::Batch) return sessionIdle ? Attribution::Release : Attribution::BatchRow;
    if (nextEntry != 0 && row.entry == nextEntry) return Attribution::Advance;
    if (currentEntry != 0 && row.entry == currentEntry) return Attribution::SameEntry;
    return Attribution::Release;
}

enum class Slot : std::uint8_t { Current, Next };

// 一次添加受理时取得的票据，提交前用 Generations::IsLive 复验。
struct Ticket {
    std::uint64_t session = 0;
    Slot slot = Slot::Current;
    std::uint64_t next = 0;
};

// 会话代次在会话结束或被替换时推进，使全部未完成的添加失效；下一条代次在每次
// enqueueNext 时推进，只使上一次的下一条失效，不影响当前条目。换曲序号在每首新曲目开始时
// 推进，只有最新一首的归属步骤生效；会话结束时也推进，此前开始的曲目交给新会话处理。
class Generations {
public:
    void EndSession() {
        ++m_session;
        ++m_track;
    }

    std::uint64_t TrackStarted() { return ++m_track; }
    bool IsLatestTrack(std::uint64_t track) const { return track == m_track; }

    Ticket TakeCurrent() const { return {m_session, Slot::Current, 0}; }

    Ticket TakeNext() {
        ++m_next;
        return {m_session, Slot::Next, m_next};
    }

    bool IsLive(const Ticket& ticket) const {
        if (ticket.session != m_session) return false;
        return ticket.slot == Slot::Current || ticket.next == m_next;
    }

private:
    std::uint64_t m_session = 0;
    std::uint64_t m_next = 0;
    std::uint64_t m_track = 0;
};

}  // namespace jit_shadow
