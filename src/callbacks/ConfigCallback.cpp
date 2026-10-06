#include "pch.h"
#include "callbacks/ConfigCallback.h"
#include "core/WebViewContext.h"
#include "api/EventEmit.h"
#include "api/generated/PlaybackSchema.h"
#include "api/generated/WindowSchema.h"

// ============================================
// 配置变化回调实现
// 监听 foobar2000 配置变化并广播事件
// ============================================

class ConfigCallbackImpl : public config_object_notify {
public:
    t_size get_watched_object_count() override {
        return 4;
    }
    
    GUID get_watched_object(t_size index) override {
        static const GUID guids[] = {
            standard_config_objects::bool_ui_always_on_top,
            standard_config_objects::bool_playlist_stop_after_current,
            standard_config_objects::bool_playback_follows_cursor,
            standard_config_objects::bool_cursor_follows_playback
        };
        return guids[index];
    }
    
    void on_watched_object_changed(const service_ptr_t<config_object>& obj) override {
        try {
            GUID guid = obj->get_guid();
            bool value = false;
            obj->get_data_bool(value);
            
            if (guid == standard_config_objects::bool_ui_always_on_top) {
                Announce<api::window::events::AlwaysOnTopChanged>(value);
            } else if (guid == standard_config_objects::bool_playlist_stop_after_current) {
                Announce<api::playback::events::StopAfterCurrentChanged>(value);
            } else if (guid == standard_config_objects::bool_playback_follows_cursor) {
                Announce<api::playback::events::FollowCursorChanged>(value);
            } else if (guid == standard_config_objects::bool_cursor_follows_playback) {
                Announce<api::playback::events::CursorFollowChanged>(value);
            }
        } catch (...) {}
    }

private:
    // 监听的四个选项都以 { enabled } 通告新值。
    template <class E>
    static void Announce(bool value) {
        LOG("ConfigCallback:", E::kName, "=", value ? "true" : "false");
        typename E::Payload payload;
        payload.enabled = value;
        api::emit::Broadcast<E>(payload);
    }
};

static service_factory_single_t<ConfigCallbackImpl> g_config_callback_factory;
