#include "pch.h"
#include "callbacks/QueueCallback.h"
#include "api/EventEmit.h"
#include "api/generated/PlaybackSchema.h"
#include "core/WebViewContext.h"

#include <cassert>

// ============================================
// 播放队列回调实现
// 当播放队列变化时广播 playback:queueChanged 事件
// ============================================

namespace {
    // 抑制标志与"抑制期最后一次 origin"记录：SDK 规定 playlist_manager 所有方法仅
    // 主线程有效，简单静态变量即可，无需加锁。
    bool g_queueBroadcastSuppressed = false;
    std::string g_lastSuppressedOrigin = "unknown";
}  // namespace

// 单一 emit helper：全库唯一的
// playback:queueChanged 发射点。count 现场读
// queue_get_count()，让订阅方免于逐次回查 queue.get。
void EmitQueueChanged(const std::string& origin) {
    try {
        auto plm = playlist_manager::get();
        size_t count = plm->queue_get_count();

        LOG("QueueCallback: Queue changed, origin=", origin.c_str());

        api::playback::QueueChangedPayload payload;
        payload.origin = origin;
        payload.count = static_cast<std::int64_t>(count);
        api::emit::Broadcast<api::playback::events::QueueChanged>(payload);
    } catch (...) {}
}

QueueBroadcastSuppressor::QueueBroadcastSuppressor(const char* overrideOrigin)
    : hasOverrideOrigin_(overrideOrigin != nullptr)
    , overrideOrigin_(overrideOrigin != nullptr ? overrideOrigin : "") {
    // 嵌套禁令：调用 RebuildQueue 的 handler 不得在外层再包一层。
    // 简单 bool 标志下，内层析构清位广播后，外层作用域内的后续变更会再
    // 次广播，产生额外事件——本断言用于在开发期捕获这类误用。
    assert(!g_queueBroadcastSuppressed &&
           "QueueBroadcastSuppressor: nested suppression scopes are forbidden");
    g_queueBroadcastSuppressed = true;
    g_lastSuppressedOrigin = "unknown";
}

QueueBroadcastSuppressor::~QueueBroadcastSuppressor() {
    // 先清位再广播：即便 EmitQueueChanged 内部出现意外，也不会让抑制标
    // 志卡在置位状态。异常安全风格对齐既有 on_changed 的 catch(...)。
    g_queueBroadcastSuppressed = false;
    try {
        EmitQueueChanged(hasOverrideOrigin_ ? overrideOrigin_ : g_lastSuppressedOrigin);
    } catch (...) {}
}

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

            // 抑制期间只记录最后一次 origin，不广播——由包裹本次
            // 写入段的 QueueBroadcastSuppressor 在析构时经 EmitQueueChanged
            // 统一广播一次。未置位时每次变化都各自广播。
            if (g_queueBroadcastSuppressed) {
                g_lastSuppressedOrigin = originStr;
                return;
            }

            EmitQueueChanged(originStr);
        } catch (...) {}
    }
};

static service_factory_single_t<QueueCallbackImpl> g_queue_callback_factory;
