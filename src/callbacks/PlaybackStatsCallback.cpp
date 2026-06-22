#include "pch.h"
#include "callbacks/PlaybackStatsCallback.h"
#include "core/WebViewContext.h"
#include "api/PlaybackApi.h"  // For GetTrackInfo()

// ============================================
// 播放统计回调实现
// 当曲目被认为"已播放"时广播 playback:itemPlayed 事件
// ============================================

class PlaybackStatsCallbackImpl : public playback_statistics_collector {
public:
    void on_item_played(metadb_handle_ptr item) override {
        try {
            if (!item.is_valid()) return;
            
            json trackInfo = GetTrackInfo(item);
            
            LOG("PlaybackStats: Item played");
            WebViewContext::GetInstance().BroadcastEvent("playback:itemPlayed", trackInfo);
        } catch (...) {}
    }
};

static playback_statistics_collector_factory_t<PlaybackStatsCallbackImpl> g_playback_stats_factory;
