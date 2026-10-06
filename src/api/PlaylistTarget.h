#pragma once
// PlaylistTarget.h - 把参数里按序号或 GUID 指定的目标播放列表换成它现在的序号。
//
// playlist.* 的按列表操作的方法，以及 library.addToPlaylist、queue.add、queue.addPaths、
// queue.setContents 与 queue.insertNext 按 GUID 给出的行、artwork.getByPlaylistItem 都经这里
// 解析，各种失败码因此一致。
//
// GUID 找不到时直接失败，不退回序号或活动列表：页面按 GUID 调用时，目标可能刚被删掉，
// 退回别的列表就会把曲目写进页面没点名的列表。
#include "api/ApiResult.h"
#include "api/ErrorEnvelope.h"
#include "interfaces/IPlaylistService.h"
#include "utils/GuidUtils.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace api {

// 序号与 GUID 都没给时的处理：多数方法取活动列表，playlist.setActive 与 rename 必须点名。
enum class PlaylistOmitted { UseActive, Refuse };

// 成功时把当前序号写进 out。失败码：两者都给或 GUID 格式不对为 INVALID_PARAMS，
// GUID 找不到为 NOT_FOUND，序号越界为 INVALID_INDEX（报错文字取 invalidMessage，
// 沿用各方法迁移前的写法），省略且没有活动列表为 NO_ACTIVE_ITEM。负序号已由声明的
// @minimum 0 挡掉；artwork.getByPlaylistItem 的负数表示活动列表，由调用方换成 std::nullopt。
inline std::optional<Failure> ResolvePlaylistTarget(const IPlaylistService& svc,
                                                    const std::optional<std::int64_t>& index,
                                                    const std::optional<std::string>& guid,
                                                    size_t& out,
                                                    PlaylistOmitted omitted = PlaylistOmitted::UseActive,
                                                    const char* invalidMessage = "Invalid playlist index") {
    if (guid.has_value()) {
        if (index.has_value()) {
            return Fail("Give playlist or playlistGuid, not both", ApiErrorCode::INVALID_PARAMS);
        }
        GUID parsed{};
        if (!GuidUtils::StringToGuid(*guid, parsed)) {
            return Fail("playlistGuid is not a GUID", ApiErrorCode::INVALID_PARAMS,
                        {{"details", nlohmann::json{{"playlistGuid", *guid}}}});
        }
        out = svc.find_playlist_by_guid(parsed);
        // SDK 没写找不到时返回什么，不小于列表个数一律当作没有。
        if (out >= svc.get_playlist_count()) {
            return Fail("No playlist has this GUID", ApiErrorCode::NOT_FOUND,
                        {{"details", nlohmann::json{{"playlistGuid", *guid}}}});
        }
        return std::nullopt;
    }

    if (index.has_value()) {
        out = static_cast<size_t>(*index);
    } else if (omitted == PlaylistOmitted::Refuse) {
        return Fail("playlist or playlistGuid is required", ApiErrorCode::INVALID_PARAMS);
    } else {
        out = svc.get_active_playlist();
        if (out == SIZE_MAX) return Fail("No active playlist", ApiErrorCode::NO_ACTIVE_ITEM);
    }
    if (out >= svc.get_playlist_count()) return Fail(invalidMessage, ApiErrorCode::INVALID_INDEX);
    return std::nullopt;
}

// playlist.reorderPlaylists 的 newOrderGuids：第 i 项换成那张列表现在的序号，写进 order[i]，
// 结果与按序号给出的 newOrder 同义。页面读完 getAll 之后清单又变了，序号会静默排错列表，
// GUID 不会。失败码：长度不等于列表个数、GUID 格式不对、同一张列表出现两次为 INVALID_PARAMS，
// GUID 找不到为 NOT_FOUND；逐项的失败在 details 里带出下标与 GUID。
inline std::optional<Failure> ResolvePlaylistGuidOrder(const IPlaylistService& svc,
                                                       const std::vector<std::string>& guids,
                                                       std::vector<size_t>& order) {
    const size_t count = svc.get_playlist_count();
    if (guids.size() != count) {
        return Fail("newOrderGuids length mismatch", ApiErrorCode::INVALID_PARAMS,
                    {{"expected", count}, {"got", guids.size()}});
    }
    order.assign(count, 0);
    std::vector<bool> seen(count, false);
    for (size_t i = 0; i < count; i++) {
        const nlohmann::json details{{"index", i}, {"playlistGuid", guids[i]}};
        GUID parsed{};
        if (!GuidUtils::StringToGuid(guids[i], parsed)) {
            return Fail("newOrderGuids[" + std::to_string(i) + "] is not a GUID",
                        ApiErrorCode::INVALID_PARAMS, {{"details", details}});
        }
        const size_t index = svc.find_playlist_by_guid(parsed);
        if (index >= count) {
            return Fail("newOrderGuids[" + std::to_string(i) + "]: no playlist has this GUID",
                        ApiErrorCode::NOT_FOUND, {{"details", details}});
        }
        if (seen[index]) {
            return Fail("newOrderGuids[" + std::to_string(i) + "] names a playlist listed before",
                        ApiErrorCode::INVALID_PARAMS, {{"details", details}});
        }
        seen[index] = true;
        order[i] = index;
    }
    return std::nullopt;
}

// 结果里的 guid 字段，写法与 playlistGuid 接受的一致，原样传回即可。
inline std::string PlaylistGuidOf(const IPlaylistService& svc, size_t index) {
    return GuidUtils::GuidToString(svc.get_playlist_guid(index));
}

// 按路径加曲目时，解析前得到的序号到写入时可能已指向另一张列表：process_locations 开着
// 模态进度框跑自己的消息循环，addPathsAsync 在后台展开，这期间用户可以增删、拖动播放列表。
// 解析前用序号构造，写入前用 Locate 换回同一张列表现在的序号。
class PinnedPlaylist {
public:
    PinnedPlaylist(const IPlaylistService& svc, size_t index) : m_guid(svc.get_playlist_guid(index)) {}

    // 列表已被删掉时返回 std::nullopt；调用方不能退回原序号，原序号现在可能是另一张列表。
    std::optional<size_t> Locate(const IPlaylistService& svc) const {
        const size_t index = svc.find_playlist_by_guid(m_guid);
        if (index >= svc.get_playlist_count()) return std::nullopt;
        return index;
    }

private:
    GUID m_guid;
};

}  // namespace api
