#include "pch.h"
#include "core/ContentVersions.h"
#include "utils/GuidUtils.h"

namespace content_version {
namespace {

const std::string& Process() {
    static const std::string process = [] {
        GUID guid{};
        // 生成失败时退回启动时刻：仍能区分绝大多数相邻的两次运行。
        if (FAILED(CoCreateGuid(&guid))) return std::to_string(GetTickCount64());
        return GuidUtils::GuidToString(guid).substr(1, 36);
    }();
    return process;
}

}  // namespace

Counters& Shared() {
    static Counters counters;
    return counters;
}

std::string ListToken(const std::string& guid) { return Format(Process(), Shared().List(guid)); }
std::string QueueToken() { return Format(Process(), Shared().Queue()); }

namespace hooks {

std::string PlaylistContentChanged(std::size_t index) {
    try {
        auto manager = playlist_manager_v5::get();
        if (index >= manager->get_playlist_count()) return {};
        const std::string guid = GuidUtils::GuidToString(manager->playlist_get_guid(index));
        Shared().ListChanged(guid);
        return ListToken(guid);
    } catch (...) {
        return {};
    }
}

void QueueChanged() { Shared().QueueChanged(); }

}  // namespace hooks
}  // namespace content_version
