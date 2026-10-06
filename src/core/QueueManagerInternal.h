#pragma once
/**
 * QueueManagerInternal.h - QueueManager 各编译单元共用的事件辅助
 *
 * 只供 QueueManager.cpp 与 QueueSources.cpp 使用。
 */

#include "core/QueueManager.h"
#include "api/EventEmit.h"

namespace jit_queue_detail {

// The JIT events go to the page that started the session; when it is gone, to a page under the
// same top-level window, and then to the main window's page. With no main window (Default UI or
// Columns UI as the interface) that last step reaches nobody. Main thread only, like ToCaller.
template <class E>
void Announce(const JitSessionCaller& to, const typename E::Payload& payload) {
    api::emit::ToCaller<E>(to.windowId, to.hwnd, payload);
}

}  // namespace jit_queue_detail
