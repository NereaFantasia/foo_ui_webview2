#include "pch.h"
#include "selection/SelectionWatcher.h"
#include "core/WebViewContext.h"
#include "core/WebViewPanel.h"
#include "api/PlaybackApi.h"  // for GetTrackInfo()

// ============================================
// SelectionWatcher Implementation
// ============================================
// 
// Monitors foobar2000 global selection changes and broadcasts selection:changed events.
// Uses ui_selection_callback_impl_base for automatic register/unregister of callbacks.
//
// Key points:
// 1. 50ms throttle to avoid flooding during rapid selection changes
// 2. Callbacks only fire when panels are registered
// 3. Events include count, type, handles fields

// ============================================
// Singleton
// ============================================

SelectionWatcher& SelectionWatcher::GetInstance() {
    // Note: this is a Meyer's singleton (lazy static initialization)
    // foobar2000 requires ui_selection_callback to not be a static instance
    static SelectionWatcher instance;
    return instance;
}

// ============================================
// Construction / Destruction
// ============================================

SelectionWatcher::SelectionWatcher() 
    : ui_selection_callback_impl_base(true)  // auto register
    , lastEmitTime_(std::chrono::steady_clock::now())
{
    LOG("SelectionWatcher: Initialized");
}

SelectionWatcher::~SelectionWatcher() {
    LOG("SelectionWatcher: Destructed");
}

void SelectionWatcher::Shutdown() {
    // Safely deregister callback while service infrastructure is still available
    // After this call, base class destructor will not attempt to access ui_selection_manager
    ui_selection_callback_activate(false);
    LOG("SelectionWatcher: Shutdown complete");
}

// ============================================
// Panel management
// ============================================

void SelectionWatcher::RegisterPanel(WebViewPanel* panel) {
    if (!panel) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto result = panels_.insert(panel);
    if (result.second) {
        LOG("SelectionWatcher: Panel registered, count:", panels_.size());
    }
}

void SelectionWatcher::UnregisterPanel(WebViewPanel* panel) {
    if (!panel) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t removed = panels_.erase(panel);
    if (removed > 0) {
        LOG("SelectionWatcher: Panel unregistered, remaining:", panels_.size());
    }
}

size_t SelectionWatcher::GetPanelCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return panels_.size();
}

// ============================================
// ui_selection_callback interface implementation
// ============================================

void SelectionWatcher::on_selection_changed(metadb_handle_list_cref selection) {
    // Called on the main thread, no thread dispatch needed
    
    // ========== Throttle logic ==========
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEmitTime_).count();
    
    if (elapsed < THROTTLE_MS) {
        // Within throttle window -- drop this selection event
        // Simple implementation: just skip and wait for the next change
        // A more sophisticated approach could use a timer to fire at interval end
        return;
    }
    
    lastEmitTime_ = now;
    
    // Emit the event
    EmitSelectionChanged(selection);
}

// ============================================
// Event broadcast
// ============================================

void SelectionWatcher::EmitSelectionChanged(metadb_handle_list_cref selection) {
    // Get selection type
    auto selMgr = ui_selection_manager::get();
    GUID type = selMgr->get_selection_type();
    
    // Map type to string
    std::string typeStr = "unknown";
    if (type == contextmenu_item::caller_now_playing) {
        typeStr = "now_playing";
    } else if (type == contextmenu_item::caller_active_playlist_selection) {
        typeStr = "active_playlist_selection";
    } else if (type == contextmenu_item::caller_active_playlist) {
        typeStr = "active_playlist";
    } else if (type == contextmenu_item::caller_playlist_manager) {
        typeStr = "playlist_manager";
    } else if (type == contextmenu_item::caller_media_library_viewer) {
        typeStr = "media_library_viewer";
    }
    
    size_t count = selection.get_count();
    
    // Build handles array (cap at 100 to avoid excessive payload)
    json handles = json::array();
    size_t maxHandles = std::min(count, static_cast<size_t>(100));
    
    for (size_t i = 0; i < maxHandles; i++) {
        metadb_handle_ptr h = selection[i];
        pfc::string8 nativePath;
        filesystem::g_get_native_path(h->get_path(), nativePath);
        std::string handlePath = nativePath.get_ptr();
        t_uint32 subsong = h->get_subsong_index();
        if (subsong > 0) {
            handlePath += "|subsong:" + std::to_string(subsong);
        }
        handles.push_back(handlePath);
    }
    
    // Build event data
    json data = {
        {"count", count},
        {"type", typeStr},
        {"handles", handles},
        {"truncated", count > 100}
    };
    
    // If only one track is selected, include full track info
    if (count == 1) {
        data["track"] = GetTrackInfo(selection[0]);
    }
    
    // Broadcast to all WebView instances

    // Add currently playing track info
    metadb_handle_ptr nowPlaying;
    if (playback_control::get()->get_now_playing(nowPlaying) && nowPlaying.is_valid()) {
        data["nowPlaying"] = GetTrackInfo(nowPlaying);
    }

    WebViewContext::GetInstance().BroadcastEvent("selection:changed", data);
    
    LOG("SelectionWatcher: Broadcast selection:changed, count=", count, ", type=", typeStr.c_str());
}

// ============================================
// initquit service - ensure SelectionWatcher shuts down safely before process exit
// ============================================

class SelectionWatcherShutdown : public initquit {
public:
    void on_init() override { /* No initialization needed — only on_quit is used */ }
    void on_quit() override {
        SelectionWatcher::GetInstance().Shutdown();
    }
};

static initquit_factory_t<SelectionWatcherShutdown> g_selection_watcher_shutdown;