#pragma once
/**
 * QueueManagerInternal.h - QueueManager 各编译单元共用的事件与影子列表辅助
 *
 * 只供 QueueManager 的三个编译单元使用。
 */

#include "core/QueueManager.h"
#include "api/EventEmit.h"
#include <vector>

namespace jit_queue_detail {

// metadb handle 的指针值，供归属表比对行；handle 是驻留的，指针相同即同一资源。
inline std::vector<jit_shadow::HandleKey> HandleKeys(metadb_handle_list_cref items) {
    std::vector<jit_shadow::HandleKey> keys;
    keys.reserve(items.get_count());
    for (size_t i = 0; i < items.get_count(); ++i) {
        keys.push_back(reinterpret_cast<jit_shadow::HandleKey>(items[i].get_ptr()));
    }
    return keys;
}

// The JIT events go to the page that started the session; when it is gone, to a page under the
// same top-level window, and then to the main window's page. With no main window (Default UI or
// Columns UI as the interface) that last step reaches nobody. Main thread only, like ToCaller.
template <class E>
void Announce(const JitSessionCaller& to, const typename E::Payload& payload) {
    if (QueueManager::GetInstance().HasProtocolSession()) return;
    api::emit::ToCaller<E>(to.windowId, to.hwnd, payload);
}

}  // namespace jit_queue_detail
