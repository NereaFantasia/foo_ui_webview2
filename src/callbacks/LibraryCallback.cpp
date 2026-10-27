/**
 * LibraryCallback Implementation
 * 
 * Monitors media library changes and invalidates cache accordingly.
 */

#include "pch.h"
#include "callbacks/LibraryCallback.h"
#include "domain/library/LibraryCache.h"
#include "domain/library/LibrarySync.h"
#include "domain/library/LibraryTreeIndex.h"
#include "api/EventEmit.h"
#include "api/TrackRow.h"
#include "api/generated/LibrarySchema.h"

// ============================================
// LibraryCallback Implementation
// ============================================

namespace {

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Fills the fields every item event carries: count, handles, truncated and timestamp.
template <class P>
void FillItemsPayload(P& payload, metadb_handle_list_cref items) {
    payload.count = static_cast<std::int64_t>(items.get_count());
    payload.truncated = FillChangedHandles(items, payload.handles);
    payload.timestamp = NowMs();
}

// Advances the library.getChanges revision. Skipped until the library has loaded:
// those callbacks list the whole library as foobar2000 reads its database.
void RecordForSync(metadb_handle_list_cref items, bool removed) {
    if (!library_sync::Recording()) return;
    std::vector<std::string> handles;
    handles.reserve(items.get_count());
    for (size_t i = 0; i < items.get_count(); ++i) {
        if (items[i].is_valid()) handles.push_back(TrackIdentityOf(items[i]).handle);
    }
    library_sync::OnItems(handles, removed);
}

template <class E>
void AnnounceItems(metadb_handle_list_cref items) {
    typename E::Payload payload;
    FillItemsPayload(payload, items);
    api::emit::Broadcast<E>(payload);
}

}  // namespace

class LibraryCallbackImpl : public library_callback_v2 {
public:
    // Called when items are added to the library
    void on_items_added(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            RecordForSync(p_data, false);

            AnnounceItems<api::library::events::ItemsAdded>(p_data);
            
            FB2K_console_print("[LibraryCallback] Items added: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when items are removed from the library
    void on_items_removed(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            RecordForSync(p_data, true);

            AnnounceItems<api::library::events::ItemsRemoved>(p_data);
            
            FB2K_console_print("[LibraryCallback] Items removed: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when item metadata is modified
    void on_items_modified(metadb_handle_list_cref p_data) override {
        try {
            // A modified track keeps its path (a moved file is removed and
            // added), so the tree index stays. A hook change (foo_playcount and
            // the like) leaves the tags as they were too, and with them the
            // tag-built results.
            const bool fromHook = library_callback::is_modified_from_hook();
            if (fromHook) {
                g_LibraryCache.InvalidateHookFields();
            } else {
                g_LibraryCache.Invalidate();
            }
            // Hook changes count too: library rows carry the rating.
            RecordForSync(p_data, false);

            api::library::events::ItemsModified::Payload payload;
            FillItemsPayload(payload, p_data);
            payload.fromHook = fromHook;
            api::emit::Broadcast<api::library::events::ItemsModified>(payload);
            
            FB2K_console_print("[LibraryCallback] Items modified: ", p_data.get_count(),
                               fromHook ? " (from hook)" : "");
        } catch (...) {}
    }
    
    // Called when item metadata is modified - extended version
    void on_items_modified_v2(metadb_handle_list_cref p_data, metadb_io_callback_v2_data& p_data2) override {
        // Delegate to on_items_modified
        on_items_modified(p_data);
    }
    
    // Called when library scan completes
    void on_library_initialized() override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            library_sync::OnLibraryLoaded();

            api::library::events::Initialized::Payload payload;
            payload.timestamp = NowMs();
            api::emit::Broadcast<api::library::events::Initialized>(payload);
            
            FB2K_console_print("[LibraryCallback] Library initialized");
        } catch (...) {}
    }
};

// Static registration - creates instance automatically
static service_factory_single_t<LibraryCallbackImpl> g_LibraryCallbackFactory;

// ============================================
// Initialization
// ============================================

void InitLibraryCallbacks() {
    // Factory already handles registration
    FB2K_console_print("[LibraryCallback] Initialized");
}
