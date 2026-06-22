/**
 * QueueManager.h - JIT Queue Manager for Streaming Media
 * 
 * Implements a dual-layer queue architecture:
 * - Frontend maintains the logical queue (full metadata, play modes)
 * - Backend maintains a shadow playlist (only current + next track)
 * 
 * Key features:
 * - JIT (Just-In-Time) URL resolution for streaming media
 * - Automatic buffer management (keeps only 2-3 tracks)
 * - Event-driven prefetching via jitQueue:needNext
 */

#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <optional>

class QueueManager {
public:
    static QueueManager& GetInstance();
    
    // Non-copyable
    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;
    
    //==========================================================================
    // State Machine
    //==========================================================================
    
    enum class State {
        Idle,           // Not active
        Active,         // Playing from shadow playlist
        WaitingNext,    // needNext emitted, waiting for enqueueNext
        Exhausted,      // No more tracks, waiting for PlayNow or clear
    };
    
    // Get current state machine state
    State GetState() const { return m_state.load(); }
    
    // Get state as string for diagnostics
    static const char* StateToString(State s) {
        switch (s) {
            case State::Idle: return "Idle";
            case State::Active: return "Active";
            case State::WaitingNext: return "WaitingNext";
            case State::Exhausted: return "Exhausted";
            default: return "Unknown";
        }
    }
    
    //==========================================================================
    // Initialization
    //==========================================================================
    
    // Initialize: create or find shadow playlist
    void Initialize();
    
    // Shutdown: cleanup resources
    void Shutdown();
    
    //==========================================================================
    // Core API Methods
    //==========================================================================
    
    /**
     * PlayNow - Immediately play a track
     * Clears the buffer and starts playing the specified URL
     * 
     * @param trackId   Frontend's unique track identifier
     * @param title     Track title for display
     * @param url       Resolved streaming URL
     * @return true if operation was accepted
     */
    bool PlayNow(const std::string& trackId, const std::string& title, const std::string& url);
    
    /**
     * EnqueueNext - Preload the next track
     * Appends to shadow playlist for gapless playback
     * 
     * @param trackId   Frontend's unique track identifier
     * @param title     Track title for display
     * @param url       Resolved streaming URL
     * @return true if operation was accepted
     */
    bool EnqueueNext(const std::string& trackId, const std::string& title, const std::string& url);
    
    /**
     * PreloadBatch - Bulk-insert tracks into shadow playlist using handle_create.
     * Bypasses the needNext/enqueueNext round-trip for finite playlists.
     * 
     * @param urls        List of resolved URLs (HTTP or local)
     * @param startIndex  Which track to start playback from (0-based)
     * @param replace     If true, clear existing shadow playlist first
     * @return {success, tracksAdded}
     */
    struct PreloadResult {
        bool success = false;
        size_t tracksAdded = 0;
        std::string error;
    };
    PreloadResult PreloadBatch(const std::vector<std::string>& urls, size_t startIndex, bool replace);
    
    /**
     * Skip - Skip to next track in buffer
     * @return true if there was a next track to skip to
     */
    bool Skip();
    
    /**
     * Stop - Stop playback and optionally clear buffer
     * @param clearBuffer Whether to clear the shadow playlist
     */
    void Stop(bool clearBuffer = true);
    
    /**
     * Clear - Clear the shadow playlist buffer
     */
    void Clear();
    
    //==========================================================================
    // State Queries
    //==========================================================================
    
    // Get current playing track ID (frontend ID)
    std::string GetCurrentTrackId() const;
    
    // Get next track ID in buffer
    std::string GetNextTrackId() const;
    
    // Get buffer size (tracks in shadow playlist)
    size_t GetBufferSize() const;
    
    // Get shadow playlist index
    size_t GetShadowPlaylistIndex() const;
    
    // Check if JIT queue is active (playing from shadow playlist)
    bool IsActive() const;
    
    // Notify frontend of list exhaustion (exposed for QueueApi)
    void NotifyListExhausted();
    
    //==========================================================================
    // Playback Callbacks (called from PlaybackCallback)
    //==========================================================================
    
    // Called when fb2k switches to a new track
    void OnPlaybackNewTrack(const metadb_handle_ptr& track);
    
    // Called when playback stops
    void OnPlaybackStop(play_control::t_stop_reason reason);
    
    // Called when playback reaches near end (for prefetching)
    void OnPlaybackTime(double time, double duration);

    //==========================================================================
    // Playlist Lock Callbacks (called by ShadowPlaylistLock)
    //==========================================================================

    // Called when shadow playlist index changes due to external playlist reorder/add/remove
    void OnShadowPlaylistIndexChanged(size_t newIndex);

    // Called when shadow playlist is removed externally
    void OnShadowPlaylistRemoved();

private:
    QueueManager() = default;
    
    //==========================================================================
    // Constants
    //==========================================================================
    
    // Shadow playlist name (hidden from user, prefixed with __)
    static constexpr const char* SHADOW_PLAYLIST_NAME = "__webview_buffer__";
    
    // Prefetch threshold: request next track when this many seconds remain
    static constexpr double PREFETCH_THRESHOLD_SECONDS = 30.0;
    
    //==========================================================================
    // State
    //==========================================================================
    
    std::atomic<State> m_state{State::Idle};
    std::atomic<size_t> m_shadowPlaylistIndex{pfc::infinite_size};  // atomic for thread safety
    
    // Saved user context (restored on Stop/Clear)
    int m_savedPlaybackOrder = -1;       // save/restore playbackOrder
    size_t m_savedActivePlaylist = 0;    // save/restore active playlist
    
    // Track IDs from frontend
    std::string m_currentTrackId;
    std::string m_nextTrackId;
    
    // Track titles for display
    std::string m_currentTitle;
    std::string m_nextTitle;
    
    // Track URLs for JIT detection (used to verify playing from JIT queue)
    std::string m_currentJitUrl;
    std::string m_nextJitUrl;
    
    // Playback state
    std::atomic<bool> m_pendingOperation{false};
    std::chrono::steady_clock::time_point m_pendingStartTime{};  // deadlock timeout
    static constexpr int PENDING_TIMEOUT_SECONDS = 30;
    std::atomic<bool> m_needNextRequested{false};
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    //==========================================================================
    // Internal Methods
    //==========================================================================
    
    // Get or create the shadow playlist
    size_t GetOrCreateShadowPlaylist();
    
    // Check if currently playing from shadow playlist
    bool IsPlayingFromShadowPlaylist() const;
    
    // Clean up already-played tracks from shadow playlist, pinning the currently
    // playing track to index 0. Takes the authoritative now-playing handle because
    // get_playing_item_location() can report a stale index during on_playback_new_track.
    void CleanupPlayedTracks(const metadb_handle_ptr& playingTrack);
    
    // Remove every shadow-playlist item positioned after the current track (located
    // by m_currentJitUrl). Used when a re-enqueued "next" must replace a previously
    // buffered next instead of appending after it; otherwise the stale next becomes
    // an orphan between current and the new next that native prev/next can step onto,
    // escaping the [current, next] URL window and deactivating the JIT queue.
    void RemoveBufferedNextFromShadow(size_t playlistIndex);
    
    // Request next track from frontend
    void RequestNextTrack();
    
    // Add URL to shadow playlist (async operation). When replaceBufferedNext is true,
    // any previously buffered next is physically removed before the new item is
    // appended, keeping the shadow playlist equal to the [current, next] window.
    void AddUrlToPlaylistAsync(const std::string& url, const std::string& trackId, 
                                const std::string& title, bool clearFirst, bool startPlayback,
                                bool replaceBufferedNext = false);
    
    // Add local file to shadow playlist (different path for local files). See
    // AddUrlToPlaylistAsync for replaceBufferedNext semantics.
    void AddLocalFileToPlaylistAsync(const std::string& path, const std::string& trackId,
                                      const std::string& title, bool clearFirst, bool startPlayback,
                                      bool replaceBufferedNext = false);
    
    // Check if a URL is a local file path (not HTTP/HTTPS)
    static bool IsLocalFilePath(const std::string& url);
    
    // Restore user's playbackOrder and active playlist on JIT deactivation
    void RestoreUserContext();

    // Install playlist lock on shadow playlist for index tracking & protection
    void InstallPlaylistLock(size_t playlistIndex);

    // Parse "path|subsong:N" format -> (path, subsongIndex)
    static std::pair<pfc::string8, t_uint32> ParseSubsongUrl(const std::string& url);

    // Bulk-convert URL strings to metadb handles (used by PreloadBatch)
    metadb_handle_list BuildHandlesFromUrls(const std::vector<std::string>& urls);

    // Callback for process_locations_notify (extracted from AddUrlToPlaylistAsync).
    // When replaceBufferedNext is true, removes any stale buffered next before insert.
    void HandleResolvedItems(metadb_handle_list_cref items, const std::string& trackId,
                             const std::string& sourceUrl, size_t playlistIndex, bool startPlayback,
                             bool replaceBufferedNext = false);

    // Start playback on shadow playlist (shared by AddUrl/AddLocalFile/PreloadBatch)
    void ActivatePlaybackOnShadow(size_t playlistIndex);

    // Playlist lock instance (tracks index changes, blocks rename/delete)
    service_ptr_t<playlist_lock> m_playlistLock;
};

// Convenience macro for accessing singleton
#define g_QueueManager QueueManager::GetInstance()
