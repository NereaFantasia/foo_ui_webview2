#include "pch.h"
#include "callbacks/PlaybackCallback.h"
#include "api/EventEmit.h"
#include "api/generated/PlaybackSchema.h"
#include "core/WebViewContext.h"
#include "api/MetaAccess.h"
#include "api/PlaybackApi.h"
#include "api/TrackRow.h"
#include "api/VolumeScale.h"
#include "core/QueueManager.h"
#include "utils/HostTime.h"
#include "utils/StringUtils.h"
#include "window/TaskbarIntegration.h"

// ============================================
// PlaybackCallback Implementation
// ============================================
// Handles foobar2000 playback events and sends them to JavaScript via Bridge
// Also integrates with JIT Queue Manager for streaming media support

namespace pb = api::playback;

// Use play_callback_static for static registration with service factory
class PlaybackCallbackImpl: public play_callback_static {
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
            
            api::emit::Broadcast<pb::events::TrackChanged>(BuildTrackRow(track));
            
            // This also runs when playback starts paused, so the state is read
            // rather than assumed; "playing" here would overwrite the "paused"
            // that on_playback_starting has just sent.
            const bool paused = playback_control::get()->is_paused();
            EmitStateChanged(paused ? "paused" : "playing");
            
            // Drive high-resolution position updates via the independent ~100ms
            // timer, and push one immediate accurate sample so the frontend
            // interpolation does not start ~1s late (on_playback_time is ~1Hz).
            // If playback starts paused, on_playback_starting handles the timer.
            if (!paused) {
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
            
            pb::StoppedPayload stopped;
            stopped.reason = reasonStr;
            api::emit::Broadcast<pb::events::Stopped>(stopped);
            
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
            
            pb::PausedPayload paused;
            paused.paused = state;
            api::emit::Broadcast<pb::events::Paused>(paused);
            
            // Emit stateChanged
            EmitStateChanged(state ? "paused" : "playing");
        } catch (...) {}
    }
    
    // Called when user seeks
    void on_playback_seek(double time) override {
        try {
            // Reset time throttle for immediate update after seek
            ResetTimeThrottle();
            
            pb::SeekedPayload seeked;
            seeked.position = time;  // 跳转目标，不是读出来的位置
            seeked.hostTime = host_time::NowUnixMs();
            api::emit::Broadcast<pb::events::Seeked>(seeked);
            
            // Push an immediate high-res sample so the progress bar / lyrics
            // snap to the seek target without waiting for the next tick.
            OnHighResTick();
        } catch (...) {}
    }
    
    // Called when volume changes
    void on_volume_change(float newVal) override {
        try {
            const float volume = volume_scale::PercentFromDb(newVal);
            bool muted = playback_control::get()->is_muted();
            
            pb::VolumeChangedPayload changed;
            changed.volume = volume;
            changed.volumeDb = newVal;
            changed.muted = muted;
            changed.isMuted = muted;  // alias: match playback.getVolume response
            api::emit::Broadcast<pb::events::VolumeChanged>(changed);
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
                // 生产侧可见性门控只覆盖事件投递：本事件是可再生流（~1Hz 全量
                // 重发），全部窗口 hidden 时跳过无损。下方 JIT 预取**不可**一并
                // 跳过——它驱动播放接续时机，扣发即断播（同理 jitQueue:needNext
                // 属直通类事件）。节流记账 m_lastTime 也必须无条件推进，
                // 否则恢复后节流状态与真实时间脱节。
                if (WebViewContext::GetInstance().HasVisibleInstance()) {
                    pb::TimePayload tick;
                    tick.position = time;
                    api::emit::Broadcast<pb::events::Time>(tick);
                }
                m_lastTime = time;
                
                // Notify JIT Queue Manager for prefetch timing
                // Get track duration from playback control
                auto pc = playback_control::get();
                double duration = pc->playback_get_length();
                if (duration > 0) {
                    g_QueueManager.OnPlaybackTime(time, duration);
                }
                // 任务栏进度条每秒跟一次已播比例；状态以 playback_control 为准，不假定本回调只在播放中触发。
                TaskbarIntegration::GetInstance().OnPlaybackProgress(pc->is_paused() ? "paused" : "playing", time, duration);
            }
        } catch (...) {}
    }
    
    // Called when dynamic track info updates (e.g., streaming metadata)
    void on_playback_dynamic_info(const file_info& info) override {
        try {
            // Extract dynamic info like bitrate, streaming title, etc.
            pb::DynamicInfoPayload dynamicInfo;

            // Get bitrate
            dynamicInfo.bitrate = static_cast<std::int64_t>(info.info_get_bitrate());

            // Get streaming title if available. 流媒体标题来自电台推送的元数据，编码不受控；
            // 非法 UTF-8 会让 EmitEvent 的 dump 抛异常、整条事件发不出去，所以先过 SafeUtf8。
            const char* streamTitle = info.meta_get("TITLE", 0);
            if (streamTitle) {
                dynamicInfo.streamTitle = StringUtils::SafeUtf8(streamTitle);
            }

            api::emit::Broadcast<pb::events::DynamicInfo>(dynamicInfo);
        } catch (...) {}
    }
    
    // Called when dynamic track info updates during track transition
    void on_playback_dynamic_info_track(const file_info& info) override {
        try {
            // Similar to on_playback_dynamic_info but for track-level changes
            pb::DynamicInfoTrackPayload dynamicInfo;

            const char* title = info.meta_get("TITLE", 0);

            // artist 经 MetaJoined 已是合法 UTF-8，title 同理要先过 SafeUtf8。
            if (MetaPresent(info, "ARTIST")) dynamicInfo.artist = MetaJoined(info, "ARTIST");
            if (title) dynamicInfo.title = StringUtils::SafeUtf8(title);

            if (dynamicInfo.artist || dynamicInfo.title) {
                api::emit::Broadcast<pb::events::DynamicInfoTrack>(dynamicInfo);
            }
        } catch (...) {}
    }
    
    // Called when playback is being initialized, before the track is opened;
    // on_playback_new_track follows once the first track opens for decoding.
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
            
            pb::StartingPayload starting;
            starting.command = command;
            starting.paused = paused;
            api::emit::Broadcast<pb::events::Starting>(starting);
            
            // Emit stateChanged - playing or paused depending on initial state.
            // No track is open yet, so its canSeek can be false; the event sent
            // from on_playback_new_track carries the opened track's value.
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
            // 生产侧可见性门控（与频谱 CollectDispatchTargets 同模型）：
            // 本事件是可再生流——每拍重发全量新位置，跳过无损，恢复后下一拍
            // 自然到达。全部窗口 hidden 时在位置查询与 JSON 构造之前早退，
            // 不做无用功；定时器保持运行，恢复无需重启动作。
            // 注意与 on_playback_time 的区别：那条回调还驱动 JIT 预取时机，
            // 不可在此模型下早退。
            if (!WebViewContext::GetInstance().HasVisibleInstance()) {
                return;
            }
            pb::TimeHighResPayload tick;
            tick.position = pc->playback_get_position();
            tick.hostTime = host_time::NowUnixMs();
            api::emit::Broadcast<pb::events::TimeHighRes>(tick);
        } catch (...) {}
    }
    
    // Helper function to emit unified playback:stateChanged event
    void EmitStateChanged(const char* state) {
        try {
            auto pc = playback_control::get();
            double position = pc->playback_get_position();
            const double positionHostTime = host_time::NowUnixMs();
            double duration = pc->playback_get_length();
            
            FB2K_console_print("[Playback] Emitting ", pb::events::StateChanged::kName, ", state: ", state);
            
            pb::StateChangedPayload changed;
            changed.state = state;
            changed.position = position;
            changed.hostTime = positionHostTime;
            changed.duration = duration;
            // Same source as playback.getState. "stopped" is pinned to false:
            // the SDK does not say whether the track is already closed when
            // on_playback_stop runs.
            changed.canSeek = std::string_view(state) != "stopped" && pc->playback_can_seek();
            api::emit::Broadcast<pb::events::StateChanged>(changed);
            TaskbarIntegration::GetInstance().OnPlaybackStateChanged(state);
            TaskbarIntegration::GetInstance().OnPlaybackProgress(state, position, duration);
        } catch (...) {}
    }
    
    // Called when playback order changes
    void on_playback_edited(metadb_handle_ptr track) override {
        try {
            // Track metadata was edited during playback
            api::emit::Broadcast<pb::events::Edited>(BuildTrackRow(track));
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

