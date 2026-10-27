#pragma once

// ============================================
// LibraryChangeLog.h - 媒体库修订号与本次运行的变化表
//
// library.getChanges 的纯逻辑，不依赖 SDK，只在主线程使用。落盘与回调接入见 LibrarySync.cpp。
//
// 载入完成之前的回调不记。fb2k 每次启动都先从自己的数据库载入整库，用 on_items_added
// 报一遍，然后才调 on_library_initialized；关机期间的变化在这之后才到达。
// 修订号从启动时的值（floor）起，每次回调加一，同一曲目只留最后一次变化。变化表不落盘，
// 早于 floor 的修订号一律答 resync。
// ============================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace library_sync {

class ChangeLog {
public:
    explicit ChangeLog(std::uint64_t floor = 0) : m_floor(floor), m_revision(floor) {}

    void MarkLoaded() { m_loaded = true; }
    bool Loaded() const { return m_loaded; }

    // 记下一次回调的曲目。载入完成之前不记。
    void Record(const std::vector<std::string>& handles, bool removed) {
        if (!m_loaded || handles.empty()) return;
        ++m_revision;
        for (const auto& handle : handles) m_entries[handle] = Entry{m_revision, removed};
    }

    std::uint64_t Floor() const { return m_floor; }
    std::uint64_t Revision() const { return m_revision; }

    // 填出 since 之后变化的曲目，顺序不固定。since 不在 [floor, revision] 内时返回 false，
    // 调用方答 resync。
    // ponytail: 每次扫全表，条目数以本次运行变化过的曲目为上限；调用频繁再按修订号建索引
    bool Since(std::uint64_t since, std::vector<std::string>& changed,
               std::vector<std::string>& removed) const {
        if (since < m_floor || since > m_revision) return false;
        for (const auto& [handle, entry] : m_entries) {
            if (entry.revision <= since) continue;
            (entry.removed ? removed : changed).push_back(handle);
        }
        return true;
    }

private:
    struct Entry {
        std::uint64_t revision;
        bool removed;
    };
    std::unordered_map<std::string, Entry> m_entries;
    std::uint64_t m_floor;
    std::uint64_t m_revision;
    bool m_loaded = false;
};

// 落盘的三项。sessionOpen 在开始记录时写为真，正常退出时写为假。
struct Stored {
    std::string epoch;
    std::uint64_t revision = 0;
    bool sessionOpen = false;
};

// 从没记过 epoch，或上次没有正常退出时，换新 epoch、修订号归 0：上次的变化可能没写回修订号。
inline Stored Resume(const Stored& stored, const std::string& freshEpoch) {
    if (stored.epoch.empty() || stored.sessionOpen) return {freshEpoch, 0, true};
    return {stored.epoch, stored.revision, true};
}

}  // namespace library_sync
