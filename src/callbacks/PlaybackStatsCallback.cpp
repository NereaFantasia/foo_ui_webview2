#include "pch.h"
#include "callbacks/PlaybackStatsCallback.h"
#include "api/EventEmit.h"
#include "api/TrackRow.h"
#include "api/generated/PlaybackSchema.h"

// ============================================
// 播放统计回调实现
// 当曲目被认为"已播放"时广播 playback:itemPlayed 事件
// ============================================

class PlaybackStatsCallbackImpl : public playback_statistics_collector {
public:
    void on_item_played(metadb_handle_ptr item) override {
        try {
            if (!item.is_valid()) return;
            
            LOG("PlaybackStats: Item played");
            api::emit::Broadcast<api::playback::events::ItemPlayed>(BuildTrackRow(item));
        } catch (...) {}
    }
};

static playback_statistics_collector_factory_t<PlaybackStatsCallbackImpl> g_playback_stats_factory;
