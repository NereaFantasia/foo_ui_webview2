#include "pch.h"
#include "callbacks/PlaybackCallback.h"
#include "core/WebViewContext.h"
#include "api/PlaybackApi.h"
#include "core/QueueManager.h"
#include "window/TaskbarIntegration.h"

// ============================================
// PlaybackCallback Implementation
// ============================================
// Handles foobar2000 playback events and sends them to JavaScript via Bridge
// Also integrates with JIT Queue Manager for streaming media support

// Use play_callback_static for static registration with service factory
class PlaybackCallbackImpl : public play_callback_static {
public:
    PlaybackCallbackImpl() {
        // Single instance via service_factory_single_t; route timer ticks here.
        s_instance = this;
    }
    
    ~PlaybackCallbackImpl() {
        StopHighResTimer();
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }
    
    // Return flags for all events we want to receive
    unsigned get_flags() override {
        return flag_on_playback_all | flag_on_volume_change;
    }
    
    // Called when a new track starts playing
    void on_playback_new_track(metadb_handle_ptr track) override {
        try {
            // Reset time throttle for immediate update on new track
            ResetTimeThrottle();
            
            json trackInfo = GetTrackInfo(track);
            WebViewContext::GetInstance().BroadcastEvent("playback:trackChanged", trackInfo);
            
            // Also emit stateChanged with "playing" state
            EmitStateChanged("playing");
            
            // Drive high-resolution position updates via the independent ~100ms
            // timer, and push one immediate accurate sample so the frontend
            // interpolation does not start ~1s late (on_playback_time is ~1Hz).
            // If playback starts paused, on_playback_starting handles the timer.
            if (!playback_control::get()->is_paused()) {
                StartHighResTimer();
            }
            OnHighResTick();
            
            // Notify JIT Queue Manager
            g_QueueManager.OnPlaybackNewTrack(track);
        } catch (...) {}
    }
    
    // Called when playback stops
    void on_playback_stop(play_control::t_stop_reason reason) override {
        try {
            std::string reasonStr;
            switch (reason) {
                case play_control::stop_reason_user:
                    reasonStr = "user";
                    break;
                case play_control::stop_reason_eof:
                    reasonStr = "eof";
                    break;
                case play_control::stop_reason_starting_another:
                    reasonStr = "starting_another";
                    break;
                case play_control::stop_reason_shutting_down:
                    reasonStr = "shutting_down";
                    break;
                default:
                    reasonStr = "unknown";
                    break;
            }
            
            FB2K_console_print("[Playback] on_playback_stop, reason: ", reasonStr.c_str());
            
            // Stop high-resolution position polling; playback is no longer active.
            StopHighResTimer();
            
            WebViewContext::GetInstance().BroadcastEvent("playback:stopped", {
                {"reason", reasonStr},
            });
            
            // Emit stateChanged for user stop and EOF (not for starting_another)
            if (reason == play_control::stop_reason_user || reason == play_control::stop_reason_eof) {
                EmitStateChanged("stopped");
            }
            
            // Notify JIT Queue Manager
            g_QueueManager.OnPlaybackStop(reason);
        } catch (...) {}
    }
    
    // Called when pause state changes
    void on_playback_pause(bool state) override {
        try {
            // Pause the high-res timer while paused; resume when unpaused.
            // Position does not advance while paused, so polling is wasteful and
            // would keep re-broadcasting the same value.
            if (state) {
                StopHighResTimer();
            } else {
                StartHighResTimer();
                OnHighResTick();  // immediate accurate sample on resume
            }
            
            WebViewContext::GetInstance().BroadcastEvent("playback:paused", {
                {"paused", state},
            });
            
            // Emit stateChanged
            EmitStateChanged(state ? "paused" : "playing");
        } catch (...) {}
    }
    
    // Called when user seeks
    void on_playback_seek(double time) override {
        try {
            // Reset time throttle for immediate update after seek
            ResetTimeThrottle();
            
            WebViewContext::GetInstance().BroadcastEvent("playback:seeked", {
                {"position", time},
            });
            
            // Push an immediate high-res sample so the progress bar / lyrics
            // snap to the seek target without waiting for the next tick.
            OnHighResTick();
        } catch (...) {}
    }
    
    // Called when volume changes
    void on_volume_change(float newVal) override {
        try {
            // dB → 0-100 线性百分比（对数逆转换，与 PlaybackApi.cpp getVolume 一致）
            float volume;
            if (newVal <= -100.0f) {
                volume = 0.0f;
            } else {
                volume = 100.0f * std::pow(10.0f, newVal / 20.0f);
                volume = std::max(0.0f, std::min(100.0f, volume));
            }
            bool muted = playback_control::get()->is_muted();
            
            WebViewContext::GetInstance().BroadcastEvent("playback:volumeChanged", {
                {"volume", volume},
                {"volumeDb", newVal},
                {"muted", muted},
                {"isMuted", muted},  // alias: match playback.getVolume response
            });
        } catch (...) {}
    }
    
    // Called once per second by foobar2000 (SDK: "Called every second, for time
    // display"). The value is integer-second granularity, so this is only used
    // for the coarse 500ms-class UI event and JIT prefetch timing. High-res
    // sub-second position is delivered by the independent timer (OnHighResTick).
    void on_playback_time(double time) override {
        try {
            // Self-healing: if playback is active but the high-res timer is not
            // running (e.g. the component loaded mid-playback and missed the
            // starting/new_track events), arm it here. This 1Hz callback is the
            // reliable "is still playing" heartbeat.
            if (m_highResTimerId == 0) {
                auto pcCheck = playback_control::get();
                if (pcCheck->is_playing() && !pcCheck->is_paused()) {
                    StartHighResTimer();
                }
            }
            
            // Standard precision: ~1Hz - suitable for coarse UI display.
            // Throttle is effectively a pass-through here since the callback
            // itself is ~1Hz, but kept for resilience against SDK changes.
            if (m_lastTime < 0 || std::abs(time - m_lastTime) >= 0.5) {
                WebViewContext::GetInstance().BroadcastEvent("playback:time", {
                    {"position", time},
                });
                m_lastTime = time;
                
                // Notify JIT Queue Manager for prefetch timing
                // Get track duration from playback control
                auto pc = playback_control::get();
                double duration = pc->playback_get_length();
                if (duration > 0) {
                    g_QueueManager.OnPlaybackTime(time, duration);
                }
            }
        } catch (...) {}
    }
    
    // Called when dynamic track info updates (e.g., streaming metadata)
    void on_playback_dynamic_info(const file_info& info) override {
        try {
            // Extract dynamic info like bitrate, streaming title, etc.
            json dynamicInfo;
            
            // Get bitrate
            dynamicInfo["bitrate"] = static_cast<int>(info.info_get_bitrate());
            
            // Get streaming title if available
            const char* streamTitle = info.meta_get("TITLE", 0);
            if (streamTitle) {
                dynamicInfo["streamTitle"] = std::string(streamTitle);
            }
            
            WebViewContext::GetInstance().BroadcastEvent("playback:dynamicInfo", dynamicInfo);
        } catch (...) {}
    }
    
    // Called when dynamic track info updates during track transition
    void on_playback_dynamic_info_track(const file_info& info) override {
        try {
            // Similar to on_playback_dynamic_info but for track-level changes
            json dynamicInfo;
            
            const char* artist = info.meta_get("ARTIST", 0);
            const char* title = info.meta_get("TITLE", 0);
            
            if (artist) dynamicInfo["artist"] = std::string(artist);
            if (title) dynamicInfo["title"] = std::string(title);
            
            if (!dynamicInfo.empty()) {
                WebViewContext::GetInstance().BroadcastEvent("playback:dynamicInfoTrack", dynamicInfo);
            }
        } catch (...) {}
    }
    
    // Called when playback is starting (after on_playback_new_track)
    void on_playback_starting(play_control::t_track_command cmd, bool paused) override {
        try {
            std::string command;
            switch (cmd) {
                case play_control::track_command_play:
                    command = "play";
                    break;
                case play_control::track_command_next:
                    command = "next";
                    break;
                case play_control::track_command_prev:
                    command = "previous";
                    break;
                case play_control::track_command_rand:
                    command = "random";
                    break;
                default:
                    command = "unknown";
                    break;
            }
            
            WebViewContext::GetInstance().BroadcastEvent("playback:starting", {
                {"command", command},
                {"paused", paused},
            });
            
            // Emit stateChanged - playing or paused depending on initial state
            EmitStateChanged(paused ? "paused" : "playing");
            
            // Start high-res polling only when actually playing.
            if (!paused) {
                StartHighResTimer();
            }
        } catch (...) {}
    }
    
private:
    // Time throttle state
    double m_lastTime = -1.0;         // For standard precision (~1Hz callback)
    
    // High-resolution position timer state.
    // foobar2000's on_playback_time callback fires only ~1Hz with integer-second
    // values, so it cannot drive the documented ~100ms sub-second timeHighRes
    // event. Instead we run an independent main-thread timer that actively polls
    // playback_control::playback_get_position() (which returns true fractional
    // seconds) and broadcasts playback:timeHighRes.
    static constexpr UINT kHighResIntervalMs = 100;
    UINT_PTR m_highResTimerId = 0;
    static PlaybackCallbackImpl* s_instance;
    
    // Reset coarse time throttle for immediate update
    void ResetTimeThrottle() {
        m_lastTime = -1.0;
    }
    
    // Timer callback (NULL-window timer): WinAPI dispatches this on the thread
    // that called SetTimer, which is foobar2000's main thread (all play_callback
    // methods run on the main thread). That satisfies playback_control's
    // main-thread-only requirement.
    static void CALLBACK HighResTimerProc(HWND, UINT, UINT_PTR, DWORD) {
        if (s_instance) {
            s_instance->OnHighResTick();
        }
    }
    
    // Start the ~100ms high-resolution polling timer (idempotent).
    void StartHighResTimer() {
        if (m_highResTimerId != 0) return;
        // NULL hwnd => timer bound to the calling thread's message queue;
        // TimerProc is dispatched via DispatchMessage on the main thread.
        m_highResTimerId = ::SetTimer(nullptr, 0, kHighResIntervalMs, &HighResTimerProc);
    }
    
    // Stop the high-resolution polling timer (idempotent).
    void StopHighResTimer() {
        if (m_highResTimerId != 0) {
            ::KillTimer(nullptr, m_highResTimerId);
            m_highResTimerId = 0;
        }
    }
    
    // Poll the true fractional playback position and broadcast it.
    // Runs on the main thread (see HighResTimerProc).
    void OnHighResTick() {
        try {
            auto pc = playback_control::get();
            // Guard against stray ticks after playback ended.
            if (!pc->is_playing()) {
                return;
            }
            double position = pc->playback_get_position();
            WebViewContext::GetInstance().BroadcastEvent("playback:timeHighRes", {
                {"position", position},
            });
        } catch (...) {}
    }
    
    // Helper function to emit unified playback:stateChanged event
    void EmitStateChanged(const char* state) {
        try {
            auto pc = playback_control::get();
            double position = pc->playback_get_position();
            double duration = pc->playback_get_length();
            
            FB2K_console_print("[Playback] Emitting playback:stateChanged, state: ", state);
            
            WebViewContext::GetInstance().BroadcastEvent("playback:stateChanged", {
                {"state", state},
                {"position", position},
                {"duration", duration}
            });
            TaskbarIntegration::GetInstance().OnPlaybackStateChanged(state);
        } catch (...) {}
    }
    
    // Called when playback order changes
    void on_playback_edited(metadb_handle_ptr track) override {
        try {
            // Track metadata was edited during playback
            json trackInfo = GetTrackInfo(track);
            WebViewContext::GetInstance().BroadcastEvent("playback:edited", trackInfo);
        } catch (...) {}
    }
};

// Static instance pointer for routing main-thread timer ticks.
PlaybackCallbackImpl* PlaybackCallbackImpl::s_instance = nullptr;

// Static factory for automatic registration
static play_callback_static_factory_t<PlaybackCallbackImpl> g_playbackCallback;

void InitPlaybackCallbacks() {
    // The callback is automatically registered by the static factory
    // This function can be used for any additional initialization if needed
    console::print("[WebView2 UI] Playback callbacks initialized");
}

