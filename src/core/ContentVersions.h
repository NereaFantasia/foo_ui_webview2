#pragma once

// ============================================
// ContentVersions.h - 播放列表各行与队列的内容版本
//
// 宿主里唯一的内容计数，供公开 API、事件与 JIT 接管共用。列表按 GUID 计：插入、删除、
// 重排、替换与整表删除都算，行数不变也算；选择、焦点、改名、上锁与标签修改不算。
// 队列的任何变化都算，包括播放取走队首。
//
// Counters 不依赖 foobar2000 SDK，只在主线程使用。回调接入与对外写法见 ContentVersions.cpp。
// ============================================

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace content_version {

class Counters {
public:
    void ListChanged(const std::string& guid) { ++m_lists[guid]; }
    void QueueChanged() { ++m_queue; }

    std::uint64_t List(const std::string& guid) const {
        const auto found = m_lists.find(guid);
        return found == m_lists.end() ? 0 : found->second;
    }
    std::uint64_t Queue() const { return m_queue; }

private:
    // 删除的列表也保留计数：清掉会让版本退回 0，与删除前读到的初值相等。
    // ponytail: one counter per playlist GUID that ever changed, never pruned; a profile has at most
    // a few hundred playlists over a run
    std::unordered_map<std::string, std::uint64_t> m_lists;
    std::uint64_t m_queue = 0;
};

// 对外的不透明版本：进程标识加计数。标识每次运行不同，上一次运行的版本因此永不相等。
inline std::string Format(const std::string& process, std::uint64_t value) {
    return process + "-" + std::to_string(value);
}

// 本进程的计数；只在主线程访问。
Counters& Shared();

std::string ListToken(const std::string& guid);
std::string QueueToken();

// 回调接入点：只推进计数，不读写列表内容。
namespace hooks {
// 第 index 个播放列表的内容变了；在列表仍存在时调用，包括即将删除的列表。
// 返回变化后的版本，取不到列表 GUID 时为空。
std::string PlaylistContentChanged(std::size_t index);
void QueueChanged();
}  // namespace hooks

}  // namespace content_version
