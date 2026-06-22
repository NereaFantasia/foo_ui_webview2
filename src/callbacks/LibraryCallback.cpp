/**
 * LibraryCallback Implementation
 * 
 * Monitors media library changes and invalidates cache accordingly.
 */

#include "pch.h"
#include "callbacks/LibraryCallback.h"
#include "core/LibraryCache.h"
#include "core/LibraryTreeIndex.h"
#include "core/WebViewContext.h"

// ============================================
// LibraryCallback Implementation
// ============================================

class LibraryCallbackImpl : public library_callback_v2 {
public:
    // Called when items are added to the library
    void on_items_added(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            
            WebViewContext::GetInstance().BroadcastEvent("library:itemsAdded", {
                {"count", p_data.get_count()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()},
            });
            
            FB2K_console_print("[LibraryCallback] Items added: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when items are removed from the library
    void on_items_removed(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            
            WebViewContext::GetInstance().BroadcastEvent("library:itemsRemoved", {
                {"count", p_data.get_count()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()},
            });
            
            FB2K_console_print("[LibraryCallback] Items removed: ", p_data.get_count());
        } catch (...) {}
    }
    
    // Called when item metadata is modified
    void on_items_modified(metadb_handle_list_cref p_data) override {
        try {
            g_LibraryCache.Invalidate();
            g_LibraryTreeIndex.Invalidate();
            
            WebViewContext::GetInstance().BroadcastEvent("library:itemsModified", {
                {"count", p_data.get_count()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()},
            });
            
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
            
            WebViewContext::GetInstance().BroadcastEvent("library:initialized", {
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count()},
            });
            
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
