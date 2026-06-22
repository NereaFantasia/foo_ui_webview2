#include "pch.h"
#include "callbacks/QueueCallback.h"
#include "core/WebViewContext.h"

// ============================================
// 播放队列回调实现
// 当播放队列变化时广播 playback:queueChanged 事件
// ============================================

class QueueCallbackImpl : public playback_queue_callback {
public:
    void on_changed(t_change_origin origin) override {
        try {
            std::string originStr;
            switch (origin) {
                case changed_user_added: 
                    originStr = "user_added"; 
                    break;
                case changed_user_removed: 
                    originStr = "user_removed"; 
                    break;
                case changed_playback_advance: 
                    originStr = "playback_advance"; 
                    break;
                default: 
                    originStr = "unknown"; 
                    break;
            }
            
            LOG("QueueCallback: Queue changed, origin=", originStr.c_str());
            
            WebViewContext::GetInstance().BroadcastEvent("playback:queueChanged", {
                {"origin", originStr}
            });
        } catch (...) {}
    }
};

static service_factory_single_t<QueueCallbackImpl> g_queue_callback_factory;
