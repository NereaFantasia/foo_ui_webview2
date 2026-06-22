/**
 * MetadbCallback Implementation
 *
 * Monitors all metadata changes in foobar2000 and emits events to WebView.
 * This is the key callback for real-time rating/tag updates.
 *
 * Uses metadb_io_callback_dynamic_impl_base for dynamic registration.
 */

#include "pch.h"
#include "callbacks/MetadbCallback.h"
#include "api/BridgeCore.h"
#include "core/WebViewContext.h"


// ============================================
// Helper Functions for Rating and Path
// ============================================
namespace {

/**
 * Read rating from foo_playcount using titleformat API
 * This is the documented interface exposed by foo_playcount: %rating%
 * Returns 0 if not found or on error
 */
int GetRatingFromPlaycount(metadb_handle_ptr handle) {
  try {
    // Use titleformat to read %rating% from foo_playcount
    static_api_ptr_t<titleformat_compiler> compiler;
    titleformat_object::ptr script;

    if (!compiler->compile(script, "%rating%")) {
      return 0;
    }

    pfc::string8 result;
    handle->format_title(nullptr, result, script, nullptr);

    if (result.get_length() > 0 && result[0] != '?') {
      int rating = atoi(result.get_ptr());
      if (rating >= 0 && rating <= 5) {
        return rating;
      }
    }
  } catch (...) {
    // Ignore errors, fall back to 0
  }
  return 0;
}

/**
 * Convert foobar2000 internal path to absolute Windows path
 */
std::string GetAbsolutePath(metadb_handle_ptr handle) {
  try {
    pfc::string8 internalPath = handle->get_path();

    // Use filesystem API to get native path
    pfc::string8 nativePath;
    filesystem::g_get_native_path(internalPath.get_ptr(), nativePath);

    if (nativePath.get_length() > 0) {
      return std::string(nativePath.get_ptr());
    }

    // Fallback: return internal path as-is
    return std::string(internalPath.get_ptr());
  } catch (...) {
    return std::string(handle->get_path());
  }
}
} // namespace

// ============================================
// MetadbCallback Implementation
// ============================================

// Use the dynamic implementation base class for easier registration
class MetadbCallbackImpl : public metadb_io_callback_dynamic_impl_base {
public:
  /**
   * Called when metadata changes for any tracks (sorted, deduplicated)
   * This is the main callback for detecting rating changes from foo_playcount
   */
  void on_changed_sorted(metadb_handle_list_cref p_items_sorted,
                         bool p_fromhook) override {
    const t_size count = p_items_sorted.get_count();

    // Skip if no items
    if (count == 0)
      return;

    // Build track list with metadata
    json tracks = json::array();
    const t_size maxItems =
        (count > 50) ? 50 : count; // Limit to 50 for performance

    for (t_size i = 0; i < maxItems; i++) {
      auto handle = p_items_sorted[i];
      if (!handle.is_valid())
        continue;

      json track;
      track["path"] = GetAbsolutePath(handle);

      // Try to get current metadata
      metadb_info_container::ptr info;
      if (handle->get_info_ref(info)) {
        const file_info &fi = info->info();

        // Get rating from foo_playcount first, then fallback to file tags
        int rating = GetRatingFromPlaycount(handle);
        if (rating == 0) {
          // Fallback to file tag
          const char *ratingStr = fi.meta_get("RATING", 0);
          if (ratingStr)
            rating = atoi(ratingStr);
        }
        track["rating"] = rating;

        // Get play count if available
        const char *playCount = fi.meta_get("PLAY_COUNT", 0);
        if (playCount)
          track["playCount"] = atoi(playCount);

        // Get basic info
        const char *title = fi.meta_get("TITLE", 0);
        if (title)
          track["title"] = title;

        const char *artist = fi.meta_get("ARTIST", 0);
        if (artist)
          track["artist"] = artist;
      }

      tracks.push_back(track);
    }

    // Emit event to WebView
    json eventData;
    eventData["tracks"] = tracks;
    eventData["count"] = count;
    eventData["fromHook"] =
        p_fromhook; // true = internal modification (like foo_playcount)
    eventData["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Broadcast to all WebView instances
    // metadb:changed is a global event — all windows need to know about tag updates
    WebViewContext::GetInstance().BroadcastEvent("metadb:changed", eventData);

    // Debug log
    FB2K_console_print("[MetadbCallback] Metadata changed: ", count, " items",
                       p_fromhook ? " (from hook)" : "");
  }
};

// Lazy initialization - avoid constructing during DLL load
static MetadbCallbackImpl *g_MetadbCallback = nullptr;

// ============================================
// Initialization
// ============================================

void InitMetadbCallbacks() {
  // Create instance on first call (after foobar2000 services are ready)
  if (!g_MetadbCallback) {
    g_MetadbCallback = new MetadbCallbackImpl();
    FB2K_console_print(
        "[MetadbCallback] Initialized - listening for metadata changes");
  }
}
