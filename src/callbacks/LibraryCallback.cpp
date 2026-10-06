/**
 * LibraryCallback Implementation
 * 
 * Monitors media library changes and invalidates cache accordingly.
 */

#include "pch.h"
#include "callbacks/LibraryCallback.h"
#include "domain/library/LibraryCache.h"
#include "domain/library/LibraryTreeIndex.h"
#include "api/EventEmit.h"
#include "api/generated/LibrarySchema.h"

// ============================================
// LibraryCallback Implementation
// ============================================

namespace {

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// The three item events share the { count, timestamp } payload.
template <class E>
void AnnounceItems(metadb_handle_list_cref items) {
    typename E::Payload payload;
    payload.count = static_cast<std::int64_t>(items.get_count());
    payload.timestamp = NowMs();
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
            
            AnnounceItems<api::library::events::ItemsAdded>(p_data);
            
            FB2K_console_print("[LibraryCallback] Items added: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when items are removed from the library
    void on_items_removed(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            
            AnnounceItems<api::library::events::ItemsRemoved>(p_data);
            
            FB2K_console_print("[LibraryCallback] Items removed: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when item metadata is modified
    void on_items_modified(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            
            AnnounceItems<api::library::events::ItemsModified>(p_data);
            
            FB2K_console_print("[LibraryCallback] Items modified: ", p_data.get_count());
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
