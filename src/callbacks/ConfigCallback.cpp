#include "pch.h"
#include "callbacks/ConfigCallback.h"
#include "core/WebViewContext.h"

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
            
            std::string eventName;
            if (guid == standard_config_objects::bool_ui_always_on_top) {
                eventName = "window:alwaysOnTopChanged";
            } else if (guid == standard_config_objects::bool_playlist_stop_after_current) {
                eventName = "playback:stopAfterCurrentChanged";
            } else if (guid == standard_config_objects::bool_playback_follows_cursor) {
                eventName = "playback:followCursorChanged";
            } else if (guid == standard_config_objects::bool_cursor_follows_playback) {
                eventName = "playback:cursorFollowChanged";
            }
            
            if (!eventName.empty()) {
                LOG("ConfigCallback:", eventName.c_str(), "=", value ? "true" : "false");
                
                WebViewContext::GetInstance().BroadcastEvent(eventName, {
                    {"enabled", value}
                });
            }
        } catch (...) {}
    }
};

static service_factory_single_t<ConfigCallbackImpl> g_config_callback_factory;
