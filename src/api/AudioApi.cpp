// AudioApi.cpp - Audio Analysis API
// Provides spectrum, BPM analysis, waveform generation and audio output info

#include "pch.h"
#include "api/AudioApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/LiveWaveform.h"
#include "api/SpectrumBands.h"
#include "api/SpectrumSchedule.h"
#include "api/WaveformAccumulator.h"
#include "api/WaveformTaskQueue.h"
#include "core/WebViewContext.h"
#include "utils/SubsongUtils.h"
#include "utils/WaveformCacheKey.h"
#include "webview/WebViewHost.h"
#include <foobar2000/SDK/vis.h>
#include <foobar2000/SDK/playback_control.h>
#include <foobar2000/SDK/metadb.h>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {
    using json = nlohmann::json;

    // 提取原始 _callerHwnd（不提升到顶级窗口）
    // 面板模式下 _callerHwnd 是 panel 的 hwnd_，与 WebViewContext 注册的 key 一致
    HWND GetRawCallerHwnd(const json& params) {
        if (params.contains("_callerHwnd")) {
            auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
            if (hwnd && IsWindow(hwnd)) {
                return hwnd;
            }
        }
        return nullptr;
    }

    std::string MakeSpectrumLegacyToken(HWND ownerHwnd, const std::string& eventName) {
        const auto hwndValue = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ownerHwnd));
        if (hwndValue != 0) {
            return "legacy:" + std::to_string(hwndValue) + ":" + eventName;
        }
        return "legacy:main:" + eventName;
    }

    // foobar2000 visualisation_stream only requires a power of two.
    // 65536 is our own ceiling: at 44.1 kHz that is ~0.67 Hz/bin and
    // ~1.5 s of PCM per frame, which is already past what a bar visualizer needs.
    constexpr int kMinSpectrumFftSize = 256;
    constexpr int kMaxSpectrumFftSize = 65536;

    // getWaveform 的上限只为限制应答体积：192 kHz 双声道一秒约 38 万个浮点数。
    constexpr double kMaxLiveWaveformDuration = 1.0;
    constexpr int kMinLiveWaveformPoints = 2;
    constexpr int kMaxLiveWaveformPoints = 65536;

    std::string ResolveSpectrumSubscriptionId(const json& params, HWND ownerHwnd, const std::string& eventName) {
        std::string subscriptionId = params.value("subscriptionId", "");
        if (!subscriptionId.empty()) {
            return subscriptionId;
        }
        return MakeSpectrumLegacyToken(ownerHwnd, eventName);
    }

    struct ForegroundWindowInfo {
        HWND hwnd{nullptr};
        DWORD pid{0};
        bool isExternal{false};
        std::string title;
    };

    ForegroundWindowInfo GetForegroundWindowInfo() {
        ForegroundWindowInfo info;
        info.hwnd = GetForegroundWindow();
        if (!info.hwnd) return info;

        GetWindowThreadProcessId(info.hwnd, &info.pid);
        info.isExternal = info.pid != 0 && info.pid != GetCurrentProcessId();

        wchar_t titleBuffer[256] = {};
        int copied = GetWindowTextW(info.hwnd, titleBuffer, static_cast<int>(std::size(titleBuffer)));
        if (copied > 0) {
            info.title = WideToUtf8(std::wstring(titleBuffer, copied));
        }
        return info;
    }

    // 面板模式 fallback: 在同一顶级窗口下查找 bridge 实例
    BridgeCore* FindBridgeByTopLevelAncestor(WebViewContext& wvc, HWND hwnd) {
        HWND top = ::GetAncestor(hwnd, GA_ROOT);
        if (!top) return nullptr;
        for (auto ih : wvc.GetAllInstances()) {
            if (::GetAncestor(ih, GA_ROOT) == top) {
                if (auto* bridge = wvc.GetBridge(ih)) {
                    return bridge;
                }
            }
        }
        return nullptr;
    }

    //=========================================================================
    // SpectrumState - Internal state for spectrum subscription
    //=========================================================================
    // 计时线程的定时源，getSpectrumDebugState.beatSource
    constexpr int kBeatSourceNone = 0;
    constexpr int kBeatSourceHighResolution = 1;
    constexpr int kBeatSourceStandard = 2;

    void BeatThreadMain(HANDLE timer, HANDLE stopEvent, HANDLE configEvent);

    bool IsSupportedSpectrumScale(const std::string& scale) {
        return scale == "weighted" || scale == "db";
    }

    fb2k_spectrum::SpectrumScale ToScheduleScale(const std::string& scale) {
        return scale == "db" ? fb2k_spectrum::SpectrumScale::Db : fb2k_spectrum::SpectrumScale::Weighted;
    }

    const char* PlaybackStateName(fb2k_spectrum::PlaybackState state) {
        switch (state) {
            case fb2k_spectrum::PlaybackState::Playing: return "playing";
            case fb2k_spectrum::PlaybackState::Paused: return "paused";
            default: return "stopped";
        }
    }

    // playback_control 只能在主线程调用；订阅登记与推帧的每一拍都在主线程。
    fb2k_spectrum::PlaybackState ReadPlaybackState() {
        auto pc = playback_control::get();
        if (!pc->is_playing()) return fb2k_spectrum::PlaybackState::Stopped;
        return pc->is_paused() ? fb2k_spectrum::PlaybackState::Paused : fb2k_spectrum::PlaybackState::Playing;
    }

    // 帧里的 hostTime：Unix 纪元毫秒、带小数。FILETIME 从 1601 年起按 100 ns 计。
    double HostTimeUnixMs() {
        FILETIME ft{};
        GetSystemTimePreciseAsFileTime(&ft);
        const uint64_t ticks = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        constexpr uint64_t kUnixEpochTicks = 116444736000000000ULL;
        return static_cast<double>(ticks - kUnixEpochTicks) / 10000.0;
    }

    // 频带的频率范围，Hz。maxFrequency 为 0 表示跟随可视化流的 sampleRate / 2；
    // 实际上限怎么取见 SpectrumBands.h。
    struct SpectrumRange {
        double minFrequency{fb2k_spectrum::kDefaultMinFrequency};
        double maxFrequency{0.0};
    };

    struct SpectrumSubscription {
        std::string token;
        std::string windowId;
        HWND ownerHwnd{nullptr};
        std::string eventName{"audio:spectrum"};
        int fftSize{1024};
        int fps{30};
        int bands{48};
        std::string scale{"weighted"};
        bool backgroundThrottle{true};
        SpectrumRange range;
        fb2k_spectrum::ScheduleState schedule;
        // 上一帧频谱的流事实，静音帧沿用；没出过帧时为 0。
        unsigned lastSampleRate{0};
        double lastStreamTime{0.0};
    };

    fb2k_spectrum::ScheduleParams ToScheduleParams(const SpectrumSubscription& subscription) {
        fb2k_spectrum::ScheduleParams params;
        params.subscriptionId = subscription.token;
        params.fftSize = subscription.fftSize;
        params.bands = subscription.bands;
        params.fps = subscription.fps;
        params.scale = ToScheduleScale(subscription.scale);
        params.backgroundThrottle = subscription.backgroundThrottle;
        params.minFrequency = subscription.range.minFrequency;
        params.maxFrequency = subscription.range.maxFrequency;
        return params;
    }

    struct SpectrumDispatchTarget {
        std::string windowId;
        HWND ownerHwnd{nullptr};
        std::string eventName{"audio:spectrum"};
    };

    struct LiveWaveformResult {
        std::vector<float> mix;
        std::vector<float> left;
        std::vector<float> right;
        unsigned sampleRate{0};
        unsigned channelCount{0};
    };
    
    struct SpectrumState {
        std::atomic<bool> active{false};
        std::atomic<int> fftSize{1024};
        std::atomic<int> fps{30};
        std::atomic<int> channelMode{0};
        std::atomic<int> bands{48};
        std::atomic<int> skipFrames{0};
        std::atomic<DWORD> lastBackgroundDispatchTick{0};
        std::atomic<DWORD> lastDiagnosticLogTick{0};
        // 累计成功算出的 FFT 次数，getSpectrumDebugState.framesComputed。
        std::atomic<uint64_t> framesComputed{0};
        std::string eventName;
        visualisation_stream_v3::ptr stream;
        std::mutex streamMutex;
        std::mutex subscriptionsMutex;
        std::unordered_map<std::string, SpectrumSubscription> subscriptions;

        // 推帧计时线程。beatThread 与 beatTimer 只在主线程上启停和读取；计时线程只读
        // beatIntervalNs，写 tickArmed、beatsCoalesced 与 beatThreadRunning。两个事件自动复位、
        // 随本对象建一次。
        std::thread beatThread;
        // 线程因异常或等待失败自行退出后 beatThread 仍可 join，「在不在跑」要看这个标志
        std::atomic<bool> beatThreadRunning{false};
        HANDLE beatTimer{nullptr};
        HANDLE beatStopEvent{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
        HANDLE beatConfigEvent{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
        // 拍长 = 1 s / 所有订阅的最大 fps，纳秒，不取整到毫秒；初值对应 fps 的初值 30
        std::atomic<int64_t> beatIntervalNs{1000000000LL / 30};
        std::atomic<int> beatSource{kBeatSourceNone};
        std::atomic<uint64_t> beatsCoalesced{0};
        fb2k_spectrum::BeatGate tickArmed;
        // 进程退出闩，只由 initquit::on_quit 置位；ShutdownRuntime 是界面关闭链路，不置它
        std::atomic<bool> quitting{false};

        static SpectrumState& Get() {
            static SpectrumState instance;
            return instance;
        }

        void ApplyChannelModeToStreamLocked() {
            if (!stream.is_valid()) return;

            try {
                stream->set_channel_mode(static_cast<t_uint32>(channelMode.load()));
            } catch (const std::exception& e) {
                console::printf("[Spectrum] set_channel_mode failed: %s", e.what());
            }
        }

        void ApplyChannelModeToStream() {
            std::scoped_lock lock(streamMutex);
            ApplyChannelModeToStreamLocked();
        }
        
        void EnsureStream() {
            std::scoped_lock lock(streamMutex);
            if (stream.is_valid()) return;
            
            try {
                auto visManager = visualisation_manager::get();
                if (!visManager.is_valid()) {
                    console::print("[Spectrum] visualisation_manager not available!");
                    return;
                }
                visManager->create_stream(stream, visualisation_manager::KStreamFlagNewFFT);
                console::printf("[Spectrum] Stream created: %s", stream.is_valid() ? "OK" : "FAILED");
            } catch (const std::exception& e) {
                console::printf("[Spectrum] EnsureStream exception: %s", e.what());
                stream.release();
            } catch (...) {
                console::print("[Spectrum] EnsureStream unknown exception");
                stream.release();
            }
            // Apply channelMode after creation (moved outside try to resolve S1141 nested-try)
            if (stream.is_valid()) {
                ApplyChannelModeToStreamLocked();
            }
        }
        
        void DestroyStream() {
            std::scoped_lock lock(streamMutex);
            stream.release();
        }
        
        // 线程已在跑就不重建：拍长变化经 RecalculateConfigLocked 的配置变更事件生效，相位不动。
        // 两种可等待定时器都建不出来时不推帧，拉取（getSpectrum）不受影响。
        void StartBeatThread() {
            if (quitting.load()) return;
            if (beatThread.joinable()) {
                if (beatThreadRunning.load()) return;
                // 已自行退出的线程先回收，再起新的
                StopBeatThread();
            }
            if (!beatStopEvent || !beatConfigEvent) {
                console::print("[Spectrum] beat thread not started: events unavailable, push frames disabled");
                return;
            }

            // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 从 Windows 10 1803 起才支持
            HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                  TIMER_ALL_ACCESS);
            int source = kBeatSourceHighResolution;
            if (!timer) {
                timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
                source = kBeatSourceStandard;
            }
            if (!timer) {
                console::printf("[Spectrum] beat timer unavailable (error %lu), push frames disabled",
                                static_cast<unsigned long>(GetLastError()));
                return;
            }

            // 线程没跑时置下的配置变更信号不能留给新线程；停止事件同理
            ResetEvent(beatStopEvent);
            ResetEvent(beatConfigEvent);
            tickArmed.Reset();
            beatThreadRunning.store(true);
            try {
                beatThread = std::thread(BeatThreadMain, timer, beatStopEvent, beatConfigEvent);
            } catch (const std::exception& e) {
                beatThreadRunning.store(false);
                CloseHandle(timer);
                console::printf("[Spectrum] beat thread not started: %s", e.what());
                return;
            }
            beatTimer = timer;
            beatSource.store(source);
            console::printf("[Spectrum] beat thread started: %s timer, interval=%.3fms",
                            source == kBeatSourceHighResolution ? "high-resolution" : "standard",
                            static_cast<double>(beatIntervalNs.load()) / 1e6);
        }

        // 计时线程在等待与投递之间不持锁、也不等主线程，所以在主线程上 join 不会互等。
        void StopBeatThread() {
            if (!beatThread.joinable()) return;
            SetEvent(beatStopEvent);
            beatThread.join();
            CloseHandle(beatTimer);
            beatTimer = nullptr;
            beatSource.store(kBeatSourceNone);
        }

        void RecalculateConfigLocked() {
            if (subscriptions.empty()) {
                active.store(false);
                fftSize.store(1024);
                fps.store(30);
                bands.store(48);
                eventName = "audio:spectrum";
                return;
            }

            int maxFft = 256;
            int maxFps = 1;
            int maxBands = 8;
            std::string primaryEvent = "audio:spectrum";
            for (const auto& [_, subscription] : subscriptions) {
                maxFft = std::max(maxFft, subscription.fftSize);
                maxFps = std::max(maxFps, subscription.fps);
                maxBands = std::max(maxBands, subscription.bands);
                if (primaryEvent == "audio:spectrum" && !subscription.eventName.empty()) {
                    primaryEvent = subscription.eventName;
                }
            }

            fftSize.store(maxFft);
            fps.store(maxFps);
            bands.store(maxBands);
            eventName = primaryEvent;
            active.store(true);

            // 拍长写在这里而不是 RefreshRuntime：两处剪枝订阅后只调本函数，订阅没剪光时不经
            // RefreshRuntime。拍长变了才唤醒线程，唤醒后立即出一拍、再按新拍长走。
            const int64_t beatNs = 1000000000LL / maxFps;
            if (beatIntervalNs.exchange(beatNs) != beatNs && beatThreadRunning.load()) {
                SetEvent(beatConfigEvent);
            }
        }

        // 退出闩置位后只停不起，也不建可视化流。
        void RefreshRuntime(bool shouldRun) {
            skipFrames.store(0);
            if (shouldRun && !quitting.load()) {
                active.store(true);
                EnsureStream();
                StartBeatThread();
            } else {
                active.store(false);
                StopBeatThread();
                DestroyStream();
            }
        }

        // 显式清理频谱运行时（用于 shutdown 链路）
        void ShutdownRuntime() {
            {
                std::scoped_lock lock(subscriptionsMutex);
                subscriptions.clear();
                RecalculateConfigLocked();
            }
            StopBeatThread();
            DestroyStream();
            skipFrames.store(0);
            lastBackgroundDispatchTick.store(0);
            lastDiagnosticLogTick.store(0);
            console::print("[Spectrum] Runtime shut down");
        }

        void UpsertSubscription(const SpectrumSubscription& subscription) {
            bool shouldRun = false;
            {
                std::scoped_lock lock(subscriptionsMutex);
                subscriptions[subscription.token] = subscription;
                RecalculateConfigLocked();
                shouldRun = !subscriptions.empty();
            }
            RefreshRuntime(shouldRun);
        }

        size_t RemoveSubscription(const std::string& token, HWND ownerHwnd) {
            bool shouldRun = false;
            size_t removedCount = 0;
            {
                std::scoped_lock lock(subscriptionsMutex);
                if (!token.empty()) {
                    removedCount = subscriptions.erase(token);
                } else if (ownerHwnd) {
                    for (auto it = subscriptions.begin(); it != subscriptions.end();) {
                        if (it->second.ownerHwnd == ownerHwnd) {
                            it = subscriptions.erase(it);
                            ++removedCount;
                        } else {
                            ++it;
                        }
                    }
                } else {
                    removedCount = subscriptions.size();
                    subscriptions.clear();
                }

                RecalculateConfigLocked();
                shouldRun = !subscriptions.empty();
            }
            RefreshRuntime(shouldRun);
            return removedCount;
        }

        std::vector<SpectrumDispatchTarget> CollectDispatchTargets() {
            std::vector<SpectrumDispatchTarget> targets;
            bool pruned = false;
            bool shouldRun = false;
            {
                std::scoped_lock lock(subscriptionsMutex);
                auto& context = WebViewContext::GetInstance();
                std::unordered_set<std::string> emittedTargets;
                for (auto it = subscriptions.begin(); it != subscriptions.end();) {
                    const bool hasOwnerBridge = it->second.ownerHwnd && context.GetBridge(it->second.ownerHwnd);
                    const bool hasWindowBridge = !it->second.windowId.empty() && context.GetBridgeByWindowId(it->second.windowId);
                    const bool isAlive = hasOwnerBridge || hasWindowBridge;

                    if (!isAlive) {
                        it = subscriptions.erase(it);
                        pruned = true;
                        continue;
                    }

                    // 可见性门控：hidden 窗口从分发目标中剔除（订阅保留，
                    // 恢复可见后下一拍自然出帧，无需重订阅/重启动作）。
                    // 全部目标 hidden 时本函数返回空。
                    WebViewHost* targetHost = nullptr;
                    if (!it->second.windowId.empty()) {
                        if (HWND windowHwnd = context.GetHwndByWindowId(it->second.windowId)) {
                            targetHost = context.GetWebViewHost(windowHwnd);
                        }
                    }
                    if (!targetHost && it->second.ownerHwnd) {
                        targetHost = context.GetHostByHwnd(it->second.ownerHwnd);
                    }
                    if (targetHost && targetHost->IsPageHidden()) {
                        ++it;
                        continue;
                    }

                    const auto hwndValue = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(it->second.ownerHwnd));
                    const std::string dedupeKey = it->second.windowId + "|" + std::to_string(hwndValue) + "|" + it->second.eventName;
                    if (emittedTargets.insert(dedupeKey).second) {
                        targets.push_back({
                            it->second.windowId,
                            it->second.ownerHwnd,
                            it->second.eventName
                        });
                    }
                    ++it;
                }

                if (pruned) {
                    RecalculateConfigLocked();
                }
                shouldRun = !subscriptions.empty();
            }

            if (!shouldRun) {
                RefreshRuntime(false);
            }

            return targets;
        }
        
        // 算一次频谱。fftSizeActual 是实际点数（含自动提升，由 EffectiveFftSize 算好），
        // 与 SpectrumBands 的前置条件一致。流无效、流未出数据（停止、暂停、预热期）都返回 false。
        bool ComputeSpectrum(int fftSizeActual, int outBands, fb2k_spectrum::SpectrumScale scale,
                             const SpectrumRange& range, std::vector<float>& outData,
                             unsigned& sampleRate, double& streamTime) {
            if (!active.load()) return false;

            EnsureStream();

            std::scoped_lock lock(streamMutex);
            if (!stream.is_valid()) return false;

            try {
                double absTime = 0;
                if (!stream->get_absolute_time(absTime)) return false;

                audio_chunk_impl chunk;
                if (!stream->get_spectrum_absolute(chunk, absTime, fftSizeActual)) return false;

                sampleRate = chunk.get_sample_rate();
                streamTime = absTime;
                if (scale == fb2k_spectrum::SpectrumScale::Db) {
                    fb2k_spectrum::ComputeDbBands(chunk.get_data(), chunk.get_sample_count(),
                                                  chunk.get_channels(), sampleRate, fftSizeActual,
                                                  outBands, outData, range.minFrequency,
                                                  range.maxFrequency);
                } else {
                    fb2k_spectrum::ComputeWeightedBands(chunk.get_data(), chunk.get_sample_count(),
                                                        chunk.get_channels(), sampleRate, fftSizeActual,
                                                        outBands, outData, range.minFrequency,
                                                        range.maxFrequency);
                }
                framesComputed.fetch_add(1);
                return true;
            } catch (...) {
                // Silently ignore — returns false below
            }

            return false;
        }
        
        // getWaveform 的一次取样。stereo 为真时填 left / right，否则填 mix；
        // points 为 0 表示不降采样。
        bool GetWaveform(double duration, bool signedOutput, bool stereo, size_t points,
                         LiveWaveformResult& out) {
            if (!active.load()) return false;

            EnsureStream();

            std::scoped_lock lock(streamMutex);
            if (!stream.is_valid()) return false;

            try {
                double absTime = 0;
                if (!stream->get_absolute_time(absTime)) return false;

                audio_chunk_impl chunk;
                if (!stream->get_chunk_absolute(chunk, absTime, duration)) return false;

                const audio_sample* data = chunk.get_data();
                const size_t count = chunk.get_sample_count();
                const unsigned channels = chunk.get_channels();
                out.sampleRate = chunk.get_sample_rate();
                out.channelCount = channels;

                if (stereo) {
                    fb2k_waveform::SplitStereoWindow(data, count, channels, signedOutput, out.left, out.right);
                    fb2k_waveform::PickEvenly(out.left, points);
                    fb2k_waveform::PickEvenly(out.right, points);
                    return !out.left.empty();
                }
                fb2k_waveform::MixWindow(data, count, channels, signedOutput, out.mix);
                fb2k_waveform::PickEvenly(out.mix, points);
                return !out.mix.empty();
            } catch (...) {
                // Silently ignore — returns false below
            }
            
            return false;
        }
    };

    //=========================================================================
    // 频谱推帧 — 计时线程定拍，主线程读取频谱并推送到 JS
    //=========================================================================
    bool ShouldEmitSpectrumDiagnostic(SpectrumState& state, DWORD nowTick, DWORD minIntervalMs = 2000) {
        DWORD lastTick = state.lastDiagnosticLogTick.load();
        if (lastTick != 0 && (nowTick - lastTick) < minIntervalMs) {
            return false;
        }
        state.lastDiagnosticLogTick.store(nowTick);
        return true;
    }

    bool ShouldThrottleSpectrumForExternalForeground(SpectrumState& state, DWORD nowTick) {
        HWND foregroundHwnd = GetForegroundWindow();
        if (!foregroundHwnd) {
            state.lastBackgroundDispatchTick.store(0);
            return false;
        }

        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(foregroundHwnd, &foregroundPid);
        if (foregroundPid == 0 || foregroundPid == GetCurrentProcessId()) {
            state.lastBackgroundDispatchTick.store(0);
            return false;
        }

        if (ShouldEmitSpectrumDiagnostic(state, nowTick)) {
            console::printf("[Spectrum] External foreground window detected (pid=%lu), throttling visualization dispatch to reduce cross-app input lag.",
                static_cast<unsigned long>(foregroundPid));
        }

        constexpr DWORD kBackgroundMinIntervalMs = 1000 / 12;
        DWORD lastTick = state.lastBackgroundDispatchTick.load();
        if (lastTick != 0 && (nowTick - lastTick) < kBackgroundMinIntervalMs) {
            return true;
        }

        state.lastBackgroundDispatchTick.store(nowTick);
        return false;
    }

    struct ScopedSpectrumTimerGuard {
        explicit ScopedSpectrumTimerGuard(std::atomic_bool& inProgress)
            : inProgress_(inProgress), acquired_(!inProgress_.exchange(true)) {}

        ~ScopedSpectrumTimerGuard() {
            if (acquired_) {
                inProgress_.store(false);
            }
        }

        explicit operator bool() const { return acquired_; }

    private:
        std::atomic_bool& inProgress_;
        bool acquired_{false};
    };

    // 帧的流事实。频谱帧取自本拍的 chunk；静音帧沿用该订阅上一帧的值，没有则为 0。
    struct SpectrumFrameFacts {
        unsigned sampleRate{0};
        double streamTime{0.0};
    };

    // 组一帧。subscriptionId 为空时不带该字段（getSpectrum 未给 subscriptionId 的应答）。
    // spectrum 为空表示静音帧：按 scale 填满静音值。
    json BuildSpectrumFrame(const std::string& subscriptionId, const std::vector<float>& spectrum,
                            int bands, int fftSizeActual, const std::string& scale,
                            const SpectrumRange& range, fb2k_spectrum::PlaybackState state,
                            const SpectrumFrameFacts& facts) {
        json data;
        if (spectrum.empty()) {
            const float silence = scale == "db" ? fb2k_spectrum::kDbBandsFloor : 0.0f;
            data["spectrum"] = std::vector<float>(static_cast<size_t>(std::max(0, bands)), silence);
        } else {
            data["spectrum"] = spectrum;
        }
        if (!subscriptionId.empty()) {
            data["subscriptionId"] = subscriptionId;
        }
        data["bands"] = bands;
        data["fftSize"] = fftSizeActual;
        data["scale"] = scale;
        data["sampleRate"] = facts.sampleRate;
        data["minFrequency"] = range.minFrequency;
        // 缺省上限就是 sampleRate / 2，按整数报；给了上限就报截到 sampleRate / 2 之后的实际值
        if (range.maxFrequency > 0.0) {
            data["maxFrequency"] = std::min(range.maxFrequency, facts.sampleRate / 2.0);
        } else {
            data["maxFrequency"] = facts.sampleRate / 2;
        }
        data["state"] = PlaybackStateName(state);
        data["streamTime"] = facts.streamTime;
        data["hostTime"] = HostTimeUnixMs();
        return data;
    }

    void EmitSpectrumFrame(const SpectrumDispatchTarget& target, const json& data) {
        auto& context = WebViewContext::GetInstance();
        if (!target.windowId.empty() &&
            context.SendEventTo(target.windowId, target.eventName, data)) {
            return;
        }

        if (!target.ownerHwnd) {
            return;
        }

        if (auto* bridge = context.GetBridge(target.ownerHwnd)) {
            bridge->EmitEvent(target.eventName, data);
            return;
        }

        if (auto* bridge = FindBridgeByTopLevelAncestor(context, target.ownerHwnd)) {
            bridge->EmitEvent(target.eventName, data);
        }
    }

    // 一拍里某个订阅出帧所需的一切，在订阅锁内抓取，投递在锁外。
    struct SpectrumFrameJob {
        SpectrumDispatchTarget target;
        int bands{0};
        int fftSizeActual{0};
        std::string scale;
        SpectrumRange range;
        SpectrumFrameFacts lastFacts;
    };

    struct SpectrumGroupResult {
        bool ok{false};
        std::vector<float> spectrum;
        SpectrumFrameFacts facts;
    };

    // 本拍的调度：清掉已失效的订阅、判可见性、推进每个订阅的状态机（固定顺序：
    // 先推进状态，再定出帧，最后定投递）。返回 false 表示已无订阅、运行时已停。
    bool PlanSpectrumTick(SpectrumState& state, const fb2k_spectrum::TickInput& input,
                          fb2k_spectrum::TickPlan& plan,
                          std::unordered_map<std::string, SpectrumFrameJob>& jobs) {
        bool pruned = false;
        bool shouldRun = false;
        {
            std::scoped_lock lock(state.subscriptionsMutex);
            auto& context = WebViewContext::GetInstance();

            for (auto it = state.subscriptions.begin(); it != state.subscriptions.end();) {
                const bool hasOwnerBridge = it->second.ownerHwnd && context.GetBridge(it->second.ownerHwnd);
                const bool hasWindowBridge = !it->second.windowId.empty() && context.GetBridgeByWindowId(it->second.windowId);
                if (!hasOwnerBridge && !hasWindowBridge) {
                    it = state.subscriptions.erase(it);
                    pruned = true;
                    continue;
                }
                ++it;
            }

            std::vector<fb2k_spectrum::ScheduleParams> params;
            std::vector<fb2k_spectrum::ScheduleEntry> entries;
            params.reserve(state.subscriptions.size());
            entries.reserve(state.subscriptions.size());
            for (auto& [token, subscription] : state.subscriptions) {
                WebViewHost* targetHost = nullptr;
                if (!subscription.windowId.empty()) {
                    if (HWND windowHwnd = context.GetHwndByWindowId(subscription.windowId)) {
                        targetHost = context.GetWebViewHost(windowHwnd);
                    }
                }
                if (!targetHost && subscription.ownerHwnd) {
                    targetHost = context.GetHostByHwnd(subscription.ownerHwnd);
                }
                const bool visible = !(targetHost && targetHost->IsPageHidden());

                params.push_back(ToScheduleParams(subscription));
                entries.push_back({&params.back(), &subscription.schedule, visible});

                SpectrumFrameJob job;
                job.target = {subscription.windowId, subscription.ownerHwnd, subscription.eventName};
                job.bands = subscription.bands;
                job.fftSizeActual = fb2k_spectrum::EffectiveFftSize(subscription.fftSize, subscription.bands,
                                                                    kMaxSpectrumFftSize);
                job.scale = subscription.scale;
                job.range = subscription.range;
                job.lastFacts = {subscription.lastSampleRate, subscription.lastStreamTime};
                jobs.emplace(token, std::move(job));
            }

            plan = fb2k_spectrum::PlanTick(input, entries);

            if (pruned) {
                state.RecalculateConfigLocked();
            }
            shouldRun = !state.subscriptions.empty();
        }

        if (!shouldRun) {
            state.RefreshRuntime(false);
        }
        return shouldRun;
    }

    // 记下本拍投递过频谱帧的订阅的流事实，供之后的静音帧沿用。
    void RememberSpectrumFacts(SpectrumState& state,
                               const std::vector<std::pair<std::string, SpectrumFrameFacts>>& updates) {
        if (updates.empty()) return;
        std::scoped_lock lock(state.subscriptionsMutex);
        for (const auto& [token, facts] : updates) {
            auto it = state.subscriptions.find(token);
            if (it != state.subscriptions.end()) {
                it->second.lastSampleRate = facts.sampleRate;
                it->second.lastStreamTime = facts.streamTime;
            }
        }
    }

    void HandleSpectrumFrameBudget(SpectrumState& state, DWORD startedAt) {
        const DWORD elapsedMs = GetTickCount() - startedAt;
        const int targetFps = std::max(1, state.fps.load());
        const int frameInterval = std::max(16, 1000 / targetFps);
        const DWORD slowFrameThresholdMs = static_cast<DWORD>(frameInterval) * 2;

        if (elapsedMs > slowFrameThresholdMs &&
            ShouldEmitSpectrumDiagnostic(state, GetTickCount())) {
            console::printf("[Spectrum] Slow visualization frame detected: %lums (target interval=%dms, configured fps=%d).",
                static_cast<unsigned long>(elapsedMs), frameInterval, targetFps);
        }

        if (elapsedMs > static_cast<DWORD>(frameInterval) && targetFps > 20) {
            state.skipFrames.store(1);
        }
    }

    // 推帧的一拍，由计时线程经 fb2k::inMainThread 投到主线程执行。
    void RunSpectrumTick() {
        auto& state = SpectrumState::Get();
        // 合并闸必须最先清掉，排在任何检查与早退之前：先查 active 再清的话，拍体里剪枝停掉
        // 线程之后闸就再也清不掉，推送从此静默（拉取不受影响）。
        state.tickArmed.Disarm();

        // 作用域重入护卫——避免任何提前 return 或异常漏清理状态
        static std::atomic_bool s_inProgress{false};
        const ScopedSpectrumTimerGuard guard(s_inProgress);
        if (!guard) {
            return;
        }

        try {
            if (state.quitting.load() || !state.active.load()) {
                return;
            }

            // 每拍固定顺序：读一次播放状态、判一次限流、取一次跳帧标志，
            // 全部交给 PlanTick 推进状态机；隐藏、限流、跳帧只能少算 FFT 或丢投递，
            // 不能再像从前那样在读状态之前早退，否则状态冻结、恢复后补发旧转移。
            const DWORD startedAt = GetTickCount();
            fb2k_spectrum::TickInput input;
            input.now = fb2k_spectrum::Clock::now();
            input.playback = ReadPlaybackState();
            input.throttled = ShouldThrottleSpectrumForExternalForeground(state, startedAt);
            input.maxFftSize = kMaxSpectrumFftSize;
            input.halfBeat = std::chrono::nanoseconds(state.beatIntervalNs.load() / 2);
            // 过载保护：上一帧超预算时跳过本拍的 FFT 给主线程让路（跳过是全局的，不按订阅区分）
            const int pendingSkip = state.skipFrames.load();
            if (pendingSkip > 0) {
                state.skipFrames.store(std::max(0, pendingSkip - 1));
                input.skipFft = true;
            }

            fb2k_spectrum::TickPlan plan;
            std::unordered_map<std::string, SpectrumFrameJob> jobs;
            if (!PlanSpectrumTick(state, input, plan, jobs)) {
                return;
            }

            // 相同配置只算一次
            std::vector<SpectrumGroupResult> results(plan.groups.size());
            for (size_t i = 0; i < plan.groups.size(); ++i) {
                const auto& group = plan.groups[i];
                auto& result = results[i];
                result.ok = state.ComputeSpectrum(group.fftSize, group.bands, group.scale,
                                                  {group.minFrequency, group.maxFrequency}, result.spectrum,
                                                  result.facts.sampleRate, result.facts.streamTime);
            }

            std::vector<std::pair<std::string, SpectrumFrameFacts>> factsUpdates;
            for (const auto& frame : plan.frames) {
                if (!frame.deliver) continue;
                auto jobIt = jobs.find(frame.subscriptionId);
                if (jobIt == jobs.end()) continue;
                const SpectrumFrameJob& job = jobIt->second;

                if (frame.silent) {
                    EmitSpectrumFrame(job.target,
                                      BuildSpectrumFrame(frame.subscriptionId, {}, job.bands, job.fftSizeActual,
                                                         job.scale, job.range, frame.state, job.lastFacts));
                    continue;
                }
                if (frame.group < 0 || static_cast<size_t>(frame.group) >= results.size()) continue;
                const SpectrumGroupResult& result = results[frame.group];
                if (!result.ok || result.spectrum.empty()) continue;

                EmitSpectrumFrame(job.target,
                                  BuildSpectrumFrame(frame.subscriptionId, result.spectrum, job.bands,
                                                     job.fftSizeActual, job.scale, job.range, frame.state,
                                                     result.facts));
                factsUpdates.emplace_back(frame.subscriptionId, result.facts);
            }
            RememberSpectrumFacts(state, factsUpdates);

            HandleSpectrumFrameBudget(state, startedAt);
        } catch (const std::exception& e) {
            console::printf("[Spectrum] Tick exception: %s", e.what());
        } catch (...) {
            console::print("[Spectrum] Tick unknown exception");
        }
    }

    // 置位与投递之间不能有别的分支，否则闸可能永远是置位的。
    void PostSpectrumBeat(SpectrumState& state) {
        if (!state.tickArmed.TryArm()) {
            state.beatsCoalesced.fetch_add(1);
            return;
        }
        fb2k::inMainThread([] { RunSpectrumTick(); });
    }

    // 计时线程：只等可等待定时器与两个事件、只经 inMainThread 投递，FFT 与播放状态都在主线程。
    // 异常逃出线程函数就是 std::terminate，整体兜住。
    void BeatThreadMain(HANDLE timer, HANDLE stopEvent, HANDLE configEvent) {
        auto& state = SpectrumState::Get();
        struct RunningFlag {
            std::atomic<bool>& flag;
            ~RunningFlag() { flag.store(false); }
        } const running{state.beatThreadRunning};

        try {
            fb2k_spectrum::Clock::time_point deadline = fb2k_spectrum::Clock::now();
            PostSpectrumBeat(state);

            // 同时触发时 WaitForMultipleObjects 报下标最小的，停止事件排第一即停止优先
            const HANDLE handles[] = {stopEvent, configEvent, timer};
            using HundredNs = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
            for (;;) {
                const fb2k_spectrum::Clock::duration beat = std::chrono::nanoseconds(state.beatIntervalNs.load());
                const fb2k_spectrum::Clock::time_point now = fb2k_spectrum::Clock::now();
                deadline = fb2k_spectrum::NextBeat(deadline, beat, now);

                // 正值是 UTC 绝对时间，系统校时会让它跳；按 steady_clock 换成负值的相对时间
                LARGE_INTEGER due{};
                due.QuadPart = -std::max<int64_t>(1, std::chrono::duration_cast<HundredNs>(deadline - now).count());
                if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                    console::printf("[Spectrum] SetWaitableTimer failed (error %lu), push frames stopped",
                                    static_cast<unsigned long>(GetLastError()));
                    return;
                }

                const DWORD signaled = WaitForMultipleObjects(static_cast<DWORD>(std::size(handles)), handles,
                                                              FALSE, INFINITE);
                if (signaled == WAIT_OBJECT_0) return;
                if (signaled == WAIT_OBJECT_0 + 1) {
                    // 拍长变了：立即出一拍，再从当前时刻按新拍长走
                    deadline = fb2k_spectrum::Clock::now();
                } else if (signaled != WAIT_OBJECT_0 + 2) {
                    console::printf("[Spectrum] beat wait returned %lu (error %lu), push frames stopped",
                                    static_cast<unsigned long>(signaled), static_cast<unsigned long>(GetLastError()));
                    return;
                }
                PostSpectrumBeat(state);
            }
        } catch (const std::exception& e) {
            console::printf("[Spectrum] beat thread stopped by exception: %s", e.what());
        } catch (...) {
            console::print("[Spectrum] beat thread stopped by unknown exception");
        }
    }

    //=========================================================================
    // BPM Utilities
    //=========================================================================
    
    double EstimateBPMFromGenre(const std::string& genre) {
        std::string g = genre;
        std::transform(g.begin(), g.end(), g.begin(), ::tolower);
        
        if (g.find("drum") != std::string::npos && g.find("bass") != std::string::npos) return 174;
        if (g.find("dubstep") != std::string::npos) return 140;
        if (g.find("house") != std::string::npos) return 128;
        if (g.find("techno") != std::string::npos) return 130;
        if (g.find("trance") != std::string::npos) return 138;
        if (g.find("hardcore") != std::string::npos) return 170;
        if (g.find("hip") != std::string::npos && g.find("hop") != std::string::npos) return 90;
        if (g.find("rap") != std::string::npos) return 90;
        if (g.find("rock") != std::string::npos) return 120;
        if (g.find("metal") != std::string::npos) return 130;
        if (g.find("pop") != std::string::npos) return 120;
        if (g.find("jazz") != std::string::npos) return 110;
        if (g.find("classical") != std::string::npos) return 80;
        if (g.find("ambient") != std::string::npos) return 80;
        if (g.find("chill") != std::string::npos) return 100;
        
        return 0;
    }

    //=========================================================================
    // API Handlers
    //=========================================================================
    
    // minFrequency / maxFrequency 的校验，订阅与不带 subscriptionId 的 getSpectrum 共用。
    // 参数错时返回错误信封，否则填好 range 返回空。上限相对流的 Nyquist 是否有效
    // 要到出帧时才知道（采样率随曲目变），这里只查与下限的关系。
    std::optional<json> ReadSpectrumRange(const json& params, const char* method, SpectrumRange& range) {
        if (params.contains("minFrequency")) {
            const json& raw = params["minFrequency"];
            if (!raw.is_number() || !std::isfinite(raw.get<double>()) || raw.get<double>() < 1.0) {
                FailureHook::LogSync(method, ApiErrorCode::INVALID_PARAMS, "minFrequency out of range", true);
                return ApiEnvelope::MakeError("minFrequency must be a number of at least 1",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "minFrequency"}, {"value", raw}});
            }
            range.minFrequency = raw.get<double>();
        }
        if (params.contains("maxFrequency")) {
            const json& raw = params["maxFrequency"];
            if (!raw.is_number() || !std::isfinite(raw.get<double>()) ||
                raw.get<double>() <= range.minFrequency) {
                FailureHook::LogSync(method, ApiErrorCode::INVALID_PARAMS, "maxFrequency out of range", true);
                return ApiEnvelope::MakeError("maxFrequency must be a number greater than minFrequency",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "maxFrequency"}, {"value", raw}});
            }
            range.maxFrequency = raw.get<double>();
        }
        return std::nullopt;
    }

    json MaxFrequencyValue(const SpectrumRange& range) {
        return range.maxFrequency > 0.0 ? json(range.maxFrequency) : json(nullptr);
    }

    json AudioSubscribeSpectrum(const json& params) {
        int fftSize = params.value("fftSize", 1024);
        std::string eventName = params.value("event", "audio:spectrum");
        int fps = params.value("fps", 30);
        auto caller = CallerContext::FromParams(params);
        std::string windowId = caller.windowId;
        HWND ownerHwnd = caller.callerHwnd;
        std::string subscriptionId = ResolveSpectrumSubscriptionId(params, ownerHwnd, eventName);

        // 参数错走统一错误信封、不登记。新参数先判类型再取值：params.value() 遇到
        // 类型不符会抛 type_error 被折成 INTERNAL_ERROR，既有参数保持这一语义不改。
        // 校验后直接对 params 取下标再 get<T>()：Graph 的 cpp-parser 只从这个形状推出
        // 参数类型，改成经别名取值，生成层的类型会退成 unknown。
        if (fftSize < kMinSpectrumFftSize || fftSize > kMaxSpectrumFftSize || (fftSize & (fftSize - 1)) != 0) {
            FailureHook::LogSync("audio.subscribeSpectrum", ApiErrorCode::INVALID_PARAMS,
                                 "fftSize out of range", true);
            return ApiEnvelope::MakeError("fftSize must be a power of 2 between 256 and 65536",
                                          ApiErrorCode::INVALID_PARAMS,
                                          json{{"param", "fftSize"}, {"value", fftSize}});
        }

        std::string scale = "weighted";
        if (params.contains("scale")) {
            const json& raw = params["scale"];
            if (!raw.is_string() || !IsSupportedSpectrumScale(raw.get<std::string>())) {
                FailureHook::LogSync("audio.subscribeSpectrum", ApiErrorCode::INVALID_PARAMS,
                                     "scale not supported", true);
                return ApiEnvelope::MakeError("scale must be 'weighted' or 'db'", ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "scale"}, {"value", raw}});
            }
            scale = params["scale"].get<std::string>();
        }

        bool backgroundThrottle = true;
        if (params.contains("backgroundThrottle")) {
            const json& raw = params["backgroundThrottle"];
            if (!raw.is_boolean()) {
                FailureHook::LogSync("audio.subscribeSpectrum", ApiErrorCode::INVALID_PARAMS,
                                     "backgroundThrottle must be boolean", true);
                return ApiEnvelope::MakeError("backgroundThrottle must be a boolean",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "backgroundThrottle"}, {"value", raw}});
            }
            backgroundThrottle = params["backgroundThrottle"].get<bool>();
        }

        SpectrumRange range;
        if (auto error = ReadSpectrumRange(params, "audio.subscribeSpectrum", range)) {
            return *error;
        }

        int numBands = params.value("bands", 48);

        auto& state = SpectrumState::Get();
        // on_quit 之后不登记、不建可视化流，否则关停期间还会再起计时线程
        if (state.quitting.load()) {
            FailureHook::LogSync("audio.subscribeSpectrum", ApiErrorCode::OPERATION_FAILED,
                                 "host is shutting down");
            return ApiEnvelope::MakeError("Host is shutting down", ApiErrorCode::OPERATION_FAILED);
        }
        SpectrumSubscription subscription;
        subscription.token = subscriptionId;
        subscription.windowId = windowId;
        subscription.ownerHwnd = ownerHwnd;
        subscription.eventName = eventName;
        subscription.fftSize = fftSize;
        subscription.fps = std::max(1, std::min(fps, 60));
        subscription.bands = std::max(8, std::min(numBands, fftSize / 2));
        subscription.scale = scale;
        subscription.backgroundThrottle = backgroundThrottle;
        subscription.range = range;
        // 登记时的播放状态是初始状态，不算一次进入
        subscription.schedule = fb2k_spectrum::MakeScheduleState(ReadPlaybackState());
        state.UpsertSubscription(subscription);

        // Lifecycle log: subscription created/updated
        FB2K_console_print("[SpectrumLifecycle] subscribe id=", subscriptionId.c_str(),
            " fftSize=", fftSize, " bands=", subscription.bands, " fps=", subscription.fps,
            " scale=", scale.c_str(), " backgroundThrottle=", backgroundThrottle ? "true" : "false",
            " minFrequency=", range.minFrequency, " maxFrequency=", range.maxFrequency);

        // 登记成功 success 恒真；流是否有效另给 streamReady（停止态下也为 true）
        return {
            {"success", true},
            {"subscriptionId", subscriptionId},
            {"fftSize", fftSize},
            {"bands", subscription.bands},
            {"fps", subscription.fps},
            {"scale", scale},
            {"backgroundThrottle", backgroundThrottle},
            {"minFrequency", range.minFrequency},
            {"maxFrequency", MaxFrequencyValue(range)},
            {"event", eventName},
            {"streamReady", state.stream.is_valid()}
        };
    }
    
    json AudioUnsubscribeSpectrum(const json& params) {
        std::string subscriptionId = params.value("subscriptionId", "");
        HWND ownerHwnd = GetRawCallerHwnd(params);
        auto& state = SpectrumState::Get();
        size_t removedCount = state.RemoveSubscription(subscriptionId, ownerHwnd);

        // Lifecycle log: subscription removed
        FB2K_console_print("[SpectrumLifecycle] unsubscribe id=", subscriptionId.c_str(),
            " removed=", (int)removedCount);

        return {
            {"success", true},
            {"removed", removedCount},
            {"subscriptionId", subscriptionId}
        };
    }

    // 计时线程没在运行时两者都是 null
    json BeatSourceValue(const SpectrumState& state) {
        if (!state.beatThreadRunning.load()) return nullptr;
        return state.beatSource.load() == kBeatSourceHighResolution ? "high-resolution" : "standard";
    }

    json BeatIntervalValue(const SpectrumState& state) {
        if (!state.beatThreadRunning.load()) return nullptr;
        return static_cast<double>(state.beatIntervalNs.load()) / 1e6;
    }

    json AudioGetSpectrumDebugState(const json& params) {
        auto& state = SpectrumState::Get();
        auto caller = CallerContext::FromParams(params);
        HWND callerHwnd = caller.callerHwnd;
        auto& context = WebViewContext::GetInstance();
        auto foregroundInfo = GetForegroundWindowInfo();
        auto dispatchTargets = state.CollectDispatchTargets();

        json subscriptions = json::array();
        bool callerOwnsSubscription = false;
        {
            std::scoped_lock lock(state.subscriptionsMutex);
            for (const auto& [token, subscription] : state.subscriptions) {
                subscriptions.push_back({
                    {"token", token},
                    {"windowId", subscription.windowId},
                    {"ownerHwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(subscription.ownerHwnd))},
                    {"event", subscription.eventName},
                    {"fftSize", subscription.fftSize},
                    {"fps", subscription.fps},
                    {"bands", subscription.bands},
                    {"scale", subscription.scale},
                    {"backgroundThrottle", subscription.backgroundThrottle},
                    {"minFrequency", subscription.range.minFrequency},
                    {"maxFrequency", MaxFrequencyValue(subscription.range)}
                });
                if (callerHwnd && subscription.ownerHwnd == callerHwnd) {
                    callerOwnsSubscription = true;
                }
            }
        }

        json targets = json::array();
        for (const auto& target : dispatchTargets) {
            targets.push_back({
                {"windowId", target.windowId},
                {"ownerHwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(target.ownerHwnd))},
                {"event", target.eventName}
            });
        }

        return {
            {"success", true},
            {"active", state.active.load()},
            {"timerRunning", state.beatThreadRunning.load()},
            // 已废弃：旧实现的定时器挂在核心主窗口上，计时线程没有窗口，恒为 0，留给读它的旧脚本
            {"timerHwnd", 0},
            {"beatSource", BeatSourceValue(state)},
            {"beatIntervalMs", BeatIntervalValue(state)},
            {"beatsCoalesced", state.beatsCoalesced.load()},
            {"effectiveFftSize", state.fftSize.load()},
            {"effectiveFps", state.fps.load()},
            {"effectiveBands", state.bands.load()},
            {"skipFrames", state.skipFrames.load()},
            {"framesComputed", state.framesComputed.load()},
            {"streamReady", state.stream.is_valid()},
            {"subscriptionCount", subscriptions.size()},
            {"dispatchTargetCount", targets.size()},
            {"subscriptions", subscriptions},
            {"dispatchTargets", targets},
            {"instanceCount", context.GetInstanceCount()},
            {"callerHwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(callerHwnd))},
            {"callerWindowId", callerHwnd ? context.GetWindowIdByHwnd(callerHwnd) : ""},
            {"callerOwnsSubscription", callerOwnsSubscription},
            {"foregroundHwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(foregroundInfo.hwnd))},
            {"foregroundPid", foregroundInfo.pid},
            {"foregroundIsExternal", foregroundInfo.isExternal},
            {"foregroundTitle", foregroundInfo.title}
        };
    }
    
    // 拉取一帧。给了 subscriptionId 就用该订阅的参数与上一帧的流事实；
    // 没给则 FFT 点数取所有订阅请求值的最大值再按本次 bands 自动提升（现行语义）。
    json AudioGetSpectrum(const json& params) {
        int requestedBands = params.value("bands", 0);
        if (requestedBands < 0) {
            return {
                {"success", false},
                {"error", "bands must be >= 0"}
            };
        }

        auto& state = SpectrumState::Get();
        std::string subscriptionId;
        std::string scale = "weighted";
        SpectrumRange range;
        int fftSizeActual = 0;
        int bands = 0;
        SpectrumFrameFacts lastFacts;

        if (params.contains("subscriptionId")) {
            // 校验后直接对 params 取下标再 get<T>()，理由同 AudioSubscribeSpectrum
            const json& raw = params["subscriptionId"];
            if (!raw.is_string() || raw.get<std::string>().empty()) {
                FailureHook::LogSync("audio.getSpectrum", ApiErrorCode::INVALID_PARAMS,
                                     "subscriptionId must be a non-empty string", true);
                return ApiEnvelope::MakeError("subscriptionId must be a non-empty string",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "subscriptionId"}, {"value", raw}});
            }
            subscriptionId = params["subscriptionId"].get<std::string>();
            std::scoped_lock lock(state.subscriptionsMutex);
            auto it = state.subscriptions.find(subscriptionId);
            if (it == state.subscriptions.end()) {
                FailureHook::LogSync("audio.getSpectrum", ApiErrorCode::NOT_FOUND, "subscription not found", true);
                return ApiEnvelope::MakeError("subscription not found", ApiErrorCode::NOT_FOUND,
                                              json{{"param", "subscriptionId"}, {"value", subscriptionId}});
            }
            const SpectrumSubscription& subscription = it->second;
            scale = subscription.scale;
            range = subscription.range;
            bands = subscription.bands;
            fftSizeActual = fb2k_spectrum::EffectiveFftSize(subscription.fftSize, subscription.bands,
                                                            kMaxSpectrumFftSize);
            lastFacts = {subscription.lastSampleRate, subscription.lastStreamTime};
        } else {
            if (params.contains("scale")) {
                const json& raw = params["scale"];
                if (!raw.is_string() || !IsSupportedSpectrumScale(raw.get<std::string>())) {
                    FailureHook::LogSync("audio.getSpectrum", ApiErrorCode::INVALID_PARAMS,
                                         "scale not supported", true);
                    return ApiEnvelope::MakeError("scale must be 'weighted' or 'db'", ApiErrorCode::INVALID_PARAMS,
                                                  json{{"param", "scale"}, {"value", raw}});
                }
                scale = params["scale"].get<std::string>();
            }
            if (auto error = ReadSpectrumRange(params, "audio.getSpectrum", range)) {
                return *error;
            }
            bands = requestedBands > 0 ? requestedBands : state.bands.load();
            fftSizeActual = fb2k_spectrum::EffectiveFftSize(state.fftSize.load(), bands, kMaxSpectrumFftSize);
            // 频带数不超过频点数：分带函数本来就按频点数截断，这里跟着截，
            // 帧里的 bands 才与 spectrum 长度一致，静音帧也不会按超大的请求值铺开
            bands = std::min(bands, fftSizeActual / 2);
        }

        // 无订阅时的失败应答不变
        if (!state.active.load()) {
            return {
                {"success", false},
                {"error", "No spectrum data available. Subscribe first or check if audio is playing."}
            };
        }

        // 暂停或停止：静音帧、不算 FFT
        const fb2k_spectrum::PlaybackState playback = ReadPlaybackState();
        if (playback != fb2k_spectrum::PlaybackState::Playing) {
            json frame = BuildSpectrumFrame(subscriptionId, {}, bands, fftSizeActual, scale, range, playback,
                                            lastFacts);
            frame["success"] = true;
            return frame;
        }

        std::vector<float> spectrum;
        SpectrumFrameFacts facts;
        const bool ok = state.ComputeSpectrum(fftSizeActual, bands, ToScheduleScale(scale), range, spectrum,
                                              facts.sampleRate, facts.streamTime);
        if (!ok || spectrum.empty()) {
            // 播放中流未出数据（含预热期）：失败应答不变
            return {
                {"success", false},
                {"error", "No spectrum data available. Subscribe first or check if audio is playing."}
            };
        }

        json frame = BuildSpectrumFrame(subscriptionId, spectrum, bands, fftSizeActual, scale, range, playback,
                                        facts);
        frame["success"] = true;
        return frame;
    }
    
    json AudioGetWaveform(const json& params) {
        // duration 改为严格校验，channels / points 是新参数。写法同 AudioSubscribeSpectrum：
        // 先判类型，校验后对 params 取下标再 get<T>()，生成层才推得出参数类型。
        double duration = 0.05;
        if (params.contains("duration")) {
            const json& raw = params["duration"];
            const bool valid = raw.is_number() && std::isfinite(raw.get<double>()) &&
                               raw.get<double>() > 0.0 && raw.get<double>() <= kMaxLiveWaveformDuration;
            if (!valid) {
                FailureHook::LogSync("audio.getWaveform", ApiErrorCode::INVALID_PARAMS,
                                     "duration out of range", true);
                return ApiEnvelope::MakeError("duration must be a number greater than 0 and at most 1",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "duration"}, {"value", raw}});
            }
            duration = params["duration"].get<double>();
        }

        bool signedOutput = params.value("signed", false);

        std::string channels = "mix";
        if (params.contains("channels")) {
            const json& raw = params["channels"];
            if (!raw.is_string() || (raw.get<std::string>() != "mix" && raw.get<std::string>() != "stereo")) {
                FailureHook::LogSync("audio.getWaveform", ApiErrorCode::INVALID_PARAMS,
                                     "channels not supported", true);
                return ApiEnvelope::MakeError("channels must be 'mix' or 'stereo'", ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "channels"}, {"value", raw}});
            }
            channels = params["channels"].get<std::string>();
        }

        int points = 0;
        if (params.contains("points")) {
            const json& raw = params["points"];
            if (!raw.is_number_integer() || raw.get<int64_t>() < kMinLiveWaveformPoints ||
                raw.get<int64_t>() > kMaxLiveWaveformPoints) {
                FailureHook::LogSync("audio.getWaveform", ApiErrorCode::INVALID_PARAMS,
                                     "points out of range", true);
                return ApiEnvelope::MakeError("points must be an integer between 2 and 65536",
                                              ApiErrorCode::INVALID_PARAMS,
                                              json{{"param", "points"}, {"value", raw}});
            }
            points = params["points"].get<int>();
        }

        const bool stereo = channels == "stereo";
        LiveWaveformResult result;
        const bool ok = SpectrumState::Get().GetWaveform(duration, signedOutput, stereo,
                                                         static_cast<size_t>(points), result);
        if (!ok) {
            return {
                {"success", false},
                {"error", "No waveform data available"}
            };
        }

        if (stereo) {
            return {
                {"success", true},
                {"left", result.left},
                {"right", result.right},
                {"duration", duration},
                {"signed", signedOutput},
                {"channels", channels},
                {"sampleRate", result.sampleRate},
                {"channelCount", result.channelCount}
            };
        }
        return {
            {"success", true},
            {"waveform", result.mix},
            {"duration", duration},
            {"signed", signedOutput},
            {"channels", channels},
            {"sampleRate", result.sampleRate},
            {"channelCount", result.channelCount}
        };
    }
    
    json AudioSetChannelMode(const json& params) {
        std::string mode = params.value("mode", "default");
        
        int chMode = 0;
        if (mode == "mono") chMode = 1;
        else if (mode == "front") chMode = 2;
        else if (mode == "back") chMode = 3;
        else mode = "default"; // 非法值规范化为 default
        
        auto& state = SpectrumState::Get();
        state.channelMode.store(chMode);

        // 通过统一带锁 helper 同步 channelMode 到现有 stream
        state.ApplyChannelModeToStream();
        
        return {{"success", true}, {"mode", mode}};
    }
    
    json AudioSubscribeStream(const json& params) {
        // Stream capture requires more complex setup
        // For now, return a stub indicating the capability
        std::string eventName = params.value("event", "audio:stream");
        double interval = params.value("interval", 0.05);
        
        return {
            {"success", false},
            {"error", "Stream capture requires playback_stream_capture integration"},
            {"event", eventName},
            {"interval", interval}
        };
    }
    
    json AudioUnsubscribeStream(const json& /*params*/) {
        return {{"success", true}};
    }
    
    json AudioAnalyzeBPM(const json& params) {
        std::string path = params.value("path", "");
        bool force = params.value("forceAnalysis", false);
        
        if (path.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        try {
            // 先拆出 subsong 再规范化路径，使 CUE 子曲目的 handle 身份与播放列表一致。
            auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(path);
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsong);
            
            if (!handle.is_valid()) {
                return {{"success", false}, {"error", "Failed to open file"}};
            }
            
            // Use get_info_ref() to avoid file_info_impl value copy overhead
            metadb_info_container::ptr infoContainer = handle->get_info_ref();
            if (!infoContainer.is_valid()) {
                return {
                    {"success", false},
                    {"error", "BPM not found in metadata and audio analysis not available"}
                };
            }

            const file_info& info = infoContainer->info();
            // Check existing BPM metadata
            if (!force) {
                const char* bpmStr = info.meta_get("BPM", 0);
                double bpm = bpmStr ? std::atof(bpmStr) : 0.0;
                if (bpm > 0 && bpm < 500) {
                    return {
                        {"success", true},
                        {"bpm", bpm},
                        {"confidence", 1.0},
                        {"source", "metadata"}
                    };
                }
            }
            
            // Try genre-based estimation
            const char* genre = info.meta_get("GENRE", 0);
            if (genre) {
                double estimatedBpm = EstimateBPMFromGenre(genre);
                if (estimatedBpm > 0) {
                    return {
                        {"success", true},
                        {"bpm", estimatedBpm},
                        {"confidence", 0.3},
                        {"source", "estimate"}
                    };
                }
            }
            
            return {
                {"success", false},
                {"error", "BPM not found in metadata and audio analysis not available"}
            };
            
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    json AudioGenerateWaveform(const json& params) {
        std::string path = params.value("path", "");
        int resolution = params.value("resolution", 800);
        
        if (path.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        resolution = std::max(50, std::min(resolution, 4000));
        
        try {
            // 先拆出 subsong 再规范化路径，使 CUE 子曲目的 handle 身份与播放列表一致。
            auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(path);
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsong);
            
            if (!handle.is_valid()) {
                return {{"success", false}, {"error", "Failed to open file"}};
            }
            
            // Use get_info_ref() to avoid file_info_impl value copy overhead
            metadb_info_container::ptr infoContainer = handle->get_info_ref();
            double duration = 0;
            int sampleRate = 0;
            int channels = 0;
            
            if (infoContainer.is_valid()) {
                const file_info& info = infoContainer->info();
                duration = info.get_length();
                sampleRate = static_cast<int>(info.info_get_int("samplerate"));
                channels = static_cast<int>(info.info_get_int("channels"));
            }
            
            if (duration <= 0) {
                return {{"success", false}, {"error", "Could not determine track duration"}};
            }
            
            // Waveform generation requires audio decoding
            // Return file info for now
            return {
                {"success", false},
                {"error", "Waveform generation requires audio decoding (not yet implemented)"},
                {"duration", duration},
                {"sampleRate", sampleRate},
                {"channels", channels},
                {"requestedResolution", resolution}
            };
            
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    json AudioGetOutputInfo(const json& /*params*/) {
        try {
            auto pc = playback_control::get();
            double volume = pc->get_volume();
            
            return {
                {"success", true},
                {"volume", volume},
                {"volumePercent", pow(10.0, volume / 20.0) * 100.0}
            };
        } catch (...) {
            return {{"success", false}, {"error", "Failed to get output info"}};
        }
    }
    
    json AudioGetStreamInfo(const json& /*params*/) {
        try {
            auto pc = playback_control::get();
            
            if (!pc->is_playing()) {
                return {
                    {"success", true},
                    {"playing", false}
                };
            }
            
            metadb_handle_ptr nowPlaying;
            if (pc->get_now_playing(nowPlaying) && nowPlaying.is_valid()) {
                // Use get_info_ref() to avoid file_info_impl value copy overhead
                metadb_info_container::ptr infoContainer = nowPlaying->get_info_ref();
                if (infoContainer.is_valid()) {
                    const file_info& info = infoContainer->info();
                    const char* codec = info.info_get("codec");
                    return {
                        {"success", true},
                        {"playing", true},
                        {"sampleRate", info.info_get_int("samplerate")},
                        {"channels", info.info_get_int("channels")},
                        {"bitrate", info.info_get_int("bitrate")},
                        {"codec", codec ? codec : "unknown"},
                        {"duration", info.get_length()}
                    };
                }
            }
        } catch (...) {
            // Silently ignore — returns error response below
        }
        
        return {{"success", false}, {"error", "Failed to get stream info"}};
    }
    
    json AudioIsVisualizationAvailable(const json& /*params*/) {
        bool available = false;

        try {
            auto visManager = visualisation_manager::get();
            available = visManager.is_valid();
        } catch (...) {
            // Silently ignore — available stays false
        }

        return {
            {"success", true},
            {"available", available}
        };
    }

    //=========================================================================
    // Full Waveform Generation - Cache & Task Management
    //=========================================================================

    using SubsongUtils::ParseSubsongPath;

    struct AudioTechnicalInfo {
        double duration{0};
        int sampleRate{0};
        int channels{0};
    };

    static bool TryExtractAudioTechnicalInfo(const file_info& info, AudioTechnicalInfo& audioInfo) {
        audioInfo.duration = info.get_length();
        audioInfo.sampleRate = static_cast<int>(info.info_get_int("samplerate"));
        audioInfo.channels = static_cast<int>(info.info_get_int("channels"));

        return audioInfo.duration > 0 && audioInfo.sampleRate > 0 && audioInfo.channels > 0;
    }

    static bool TryReadDirectAudioInfo(const std::string& canonicalPath, t_uint32 subsong,
                                       file_info_impl& info) {
        try {
            abort_callback_impl abort;
            input_info_reader::ptr reader;
            input_entry::g_open_for_info_read(reader, nullptr, canonicalPath.c_str(), abort);
            if (!reader.is_valid()) {
                return false;
            }

            reader->get_info(subsong, info, abort);
            return true;
        } catch (...) {
            return false;
        }
    }

    enum class AudioTechnicalInfoStatus {
        Valid,
        MissingInfo,
        InvalidParams
    };

    AudioTechnicalInfoStatus ResolveAudioTechnicalInfo(const metadb_handle_ptr& handle,
                                                              const std::string& canonicalPath,
                                                              t_uint32 subsong,
                                                              AudioTechnicalInfo& audioInfo) {
        metadb_info_container::ptr infoContainer = handle->get_info_ref();
        const bool hasCachedInfo = infoContainer.is_valid();

        if (hasCachedInfo && TryExtractAudioTechnicalInfo(infoContainer->info(), audioInfo)) {
            return AudioTechnicalInfoStatus::Valid;
        }

        file_info_impl directInfo;
        const bool hasDirectInfo = TryReadDirectAudioInfo(canonicalPath, subsong, directInfo);
        if (hasDirectInfo && TryExtractAudioTechnicalInfo(directInfo, audioInfo)) {
            return AudioTechnicalInfoStatus::Valid;
        }

        if (!hasCachedInfo && !hasDirectInfo) {
            return AudioTechnicalInfoStatus::MissingInfo;
        }

        return AudioTechnicalInfoStatus::InvalidParams;
    }

    // 缓存条目：存一次解码得到的三组原始窗口值，method / signed / scale 在应答时现算，
    // 所以同一曲目、同一 resolution 的任何请求形态共用一个条目。
    struct WaveformCacheEntry {
        fb2k_waveform::RawWindows raw;
        double duration;
        int sampleRate;
        int channels;
        int resolution;
        std::string path;
        uint64_t fileSize;
        uint64_t modifiedTime;
        std::chrono::steady_clock::time_point lastAccess;
    };

    fb2k_waveform::Method ToWaveformMethod(const std::string& method) {
        return method == "peak" ? fb2k_waveform::Method::Peak : fb2k_waveform::Method::Rms;
    }

    fb2k_waveform::Scale ToWaveformScale(const std::string& scale) {
        return scale == "db" ? fb2k_waveform::Scale::Db : fb2k_waveform::Scale::Linear;
    }

    fb2k_waveform::Rendered RenderWaveform(const fb2k_waveform::RawWindows& raw, const std::string& method,
                                           const std::string& scale, bool signedOutput) {
        fb2k_waveform::RenderOptions options;
        options.method = ToWaveformMethod(method);
        options.scale = ToWaveformScale(scale);
        options.signedOutput = signedOutput;
        return fb2k_waveform::Render(raw, options);
    }

    // 缓存管理器
    class WaveformCache {
    private:
        std::mutex mutex_;
        std::map<std::string, WaveformCacheEntry> cache_;
        static constexpr size_t MAX_ENTRIES = 50;

        std::string MakeCacheKey(const std::string& canonicalPath, t_uint32 subsong,
                                 int resolution, uint64_t fileSize, uint64_t modifiedTime) {
            return waveform_cache::MakeKey(canonicalPath, subsong, resolution, fileSize, modifiedTime);
        }

        void EvictLRU() {
            if (cache_.size() < MAX_ENTRIES) return;

            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.lastAccess < oldest->second.lastAccess) {
                    oldest = it;
                }
            }
            cache_.erase(oldest);
        }

    public:
        static WaveformCache& Get() {
            static WaveformCache instance;
            return instance;
        }

        bool TryGet(const std::string& canonicalPath, t_uint32 subsong, int resolution,
                    uint64_t fileSize, uint64_t modifiedTime, WaveformCacheEntry& outEntry) {
            std::scoped_lock lock(mutex_);
            std::string key = MakeCacheKey(canonicalPath, subsong, resolution, fileSize, modifiedTime);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                it->second.lastAccess = std::chrono::steady_clock::now();
                outEntry = it->second;
                return true;
            }
            return false;
        }

        void Put(const std::string& canonicalPath, t_uint32 subsong, int resolution,
                 uint64_t fileSize, uint64_t modifiedTime, const WaveformCacheEntry& entry) {
            std::scoped_lock lock(mutex_);
            std::string key = MakeCacheKey(canonicalPath, subsong, resolution, fileSize, modifiedTime);
            // 同键覆盖不增加条目，不该为它挤掉别的曲目
            if (!cache_.contains(key)) EvictLRU();
            cache_[key] = entry;
            cache_[key].lastAccess = std::chrono::steady_clock::now();
        }
    };

    // 任务 ID 生成器
    std::atomic<int> g_waveformTaskIdCounter{0};

    std::string GenerateTaskId() {
        int id = g_waveformTaskIdCounter.fetch_add(1);
        char buf[64];
        sprintf_s(buf, "waveform_%d", id);
        return buf;
    }

    std::string WaveformMethodName(fb2k_waveform::Method method) {
        return method == fb2k_waveform::Method::Peak ? "peak" : "rms";
    }

    std::string WaveformScaleName(fb2k_waveform::Scale scale) {
        return scale == fb2k_waveform::Scale::Db ? "db" : "linear";
    }

    // 把整轨波形事件路由回发起请求的实例；身份在请求时由 CallerContext 确定
    void SendToCaller(const fb2k_waveform::CallerIdentity& owner, const std::string& event, const json& data) {
        auto& wvc = WebViewContext::GetInstance();
        // 优先 windowId
        if (!owner.windowId.empty() && wvc.SendEventTo(owner.windowId, event, data)) return;
        // 其次直接 hwnd
        auto callerHwnd = reinterpret_cast<HWND>(owner.hwnd);
        if (callerHwnd) {
            if (auto* bridge = wvc.GetBridge(callerHwnd)) {
                bridge->EmitEvent(event, data);
                return;
            }
            // 面板 fallback: 同一顶级窗口下的 instance
            if (auto* bridge = FindBridgeByTopLevelAncestor(wvc, callerHwnd)) {
                bridge->EmitEvent(event, data);
                return;
            }
        }
        // 最终 fallback
        BridgeCore::GetInstance().EmitEvent(event, data);
    }

    // 一次整轨解码的输入：解码键的五项，加上首个请求的原始路径（写进缓存条目）
    struct WaveformJobSpec {
        std::string path;
        std::string canonicalPath;
        t_uint32 subsong = 0;
        int resolution = 0;
        uint64_t fileSize = 0;
        uint64_t modifiedTime = 0;
    };

    struct WaveformDecodeResult {
        fb2k_waveform::JobOutcome outcome = fb2k_waveform::JobOutcome::Failed;
        const char* code = ApiErrorCode::UNKNOWN_ERROR;
        std::string error;
        fb2k_waveform::RawWindows raw;
        double duration = 0.0;
        int sampleRate = 0;
        int channels = 0;
    };

    // 在 worker 线程里跑：打开解码器、一次累加三组窗口值。不碰队列、缓存与事件，结果交回主线程
    WaveformDecodeResult DecodeWaveform(const WaveformJobSpec& spec, abort_callback& abort) {
        WaveformDecodeResult result;
        auto fail = [&result](const char* code, const char* error) {
            result.outcome = fb2k_waveform::JobOutcome::Failed;
            result.code = code;
            result.error = error;
            return result;
        };
        try {
            auto mdb = metadb::get();
            pfc::string8 fb2kPath(spec.canonicalPath.c_str());
            metadb_handle_ptr handle = mdb->handle_create(fb2kPath, spec.subsong);
            if (!handle.is_valid()) {
                return fail(ApiErrorCode::INVALID_HANDLE, "Failed to create metadb handle");
            }

            AudioTechnicalInfo audioInfo;
            AudioTechnicalInfoStatus infoStatus = ResolveAudioTechnicalInfo(handle, spec.canonicalPath,
                                                                            spec.subsong, audioInfo);
            if (infoStatus == AudioTechnicalInfoStatus::MissingInfo) {
                return fail(ApiErrorCode::NO_INFO, "Failed to get file info");
            }
            if (infoStatus == AudioTechnicalInfoStatus::InvalidParams) {
                return fail(ApiErrorCode::INVALID_PARAMS, "Invalid audio parameters");
            }

            service_ptr_t<input_decoder> decoder;
            input_entry::g_open_for_decoding(decoder, nullptr, fb2kPath, abort);
            if (!decoder.is_valid()) {
                return fail(ApiErrorCode::DECODER_FAILED, "Failed to open decoder");
            }
            decoder->initialize(spec.subsong, input_flag_no_looping, abort);

            // 窗口长按 duration 估算；曲目比 resolution 还短时每窗至少一帧
            uint64_t totalSamples = static_cast<uint64_t>(audioInfo.duration * audioInfo.sampleRate);
            uint64_t samplesPerWindow = totalSamples / spec.resolution;
            if (samplesPerWindow == 0) samplesPerWindow = 1;

            fb2k_waveform::Accumulator<audio_sample> accumulator(spec.resolution, samplesPerWindow);
            audio_chunk_impl_temporary chunk;
            while (decoder->run(chunk, abort)) {
                accumulator.Feed(chunk.get_data(), chunk.get_sample_count(), chunk.get_channels());
            }
            result.outcome = fb2k_waveform::JobOutcome::Succeeded;
            result.raw = accumulator.Finish();
            result.duration = audioInfo.duration;
            result.sampleRate = audioInfo.sampleRate;
            result.channels = audioInfo.channels;
            return result;
        } catch (const exception_aborted&) {
            // exception_aborted 也派生自 std::exception，必须排在它前面，否则取消会被当成解码失败
            result.outcome = fb2k_waveform::JobOutcome::Aborted;
            return result;
        } catch (const std::exception& e) {
            return fail(ApiErrorCode::DECODE_FAILED, e.what());
        } catch (...) {
            return fail(ApiErrorCode::UNKNOWN_ERROR, "Unknown error during waveform generation");
        }
    }

    // 整轨解码任务的全部状态。请求、取消、worker 完成回调（经 inMainThread）、弹窗关闭与退出
    // 都在主线程执行，所以不加锁；worker 只持有自己那份 spec 与中止令牌
    struct WaveformJobs {
        fb2k_waveform::WaveformTaskQueue queue;
        std::unordered_map<uint64_t, WaveformJobSpec> specs;
        std::unordered_map<uint64_t, std::shared_ptr<abort_callback_impl>> tokens;
        bool shuttingDown = false;
    };

    WaveformJobs& GetWaveformJobs() {
        static WaveformJobs jobs;
        return jobs;
    }

    void OnWaveformJobFinished(uint64_t jobId, const WaveformDecodeResult& result);

    void StartWaveformJobs(const std::vector<uint64_t>& jobIds) {
        auto& jobs = GetWaveformJobs();
        for (uint64_t jobId : jobIds) {
            auto spec = jobs.specs.find(jobId);
            if (spec == jobs.specs.end()) continue;
            auto token = std::make_shared<abort_callback_impl>();
            jobs.tokens[jobId] = token;
            FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=decoding job=",
                               std::to_string(jobId).c_str(), " path=", spec->second.canonicalPath.c_str());
            fb2k::inCpuWorkerThread([jobId, spec = spec->second, token]() {
                WaveformDecodeResult result = DecodeWaveform(spec, *token);
                fb2k::inMainThread([jobId, result = std::move(result)]() { OnWaveformJobFinished(jobId, result); });
            });
        }
    }

    void AbortWaveformJobs(const std::vector<uint64_t>& jobIds) {
        auto& jobs = GetWaveformJobs();
        for (uint64_t jobId : jobIds) {
            auto token = jobs.tokens.find(jobId);
            if (token != jobs.tokens.end()) token->second->abort();
        }
    }

    // 排队中被丢弃的任务不会再有 worker 回来收尾，它的 spec 由丢弃方清掉
    void ForgetWaveformJobs(const std::vector<uint64_t>& jobIds) {
        auto& jobs = GetWaveformJobs();
        for (uint64_t jobId : jobIds) jobs.specs.erase(jobId);
    }

    // 给一个被取消的等待者发它唯一的终态事件。取消端点与中止收尾共用；放在 handler 之外，
    // 生成层才不会把事件字段当成取消端点的应答字段
    void EmitWaveformCancelled(const fb2k_waveform::Waiter& waiter) {
        auto emitToCaller = [&waiter](const std::string& event, const json& data) {
            SendToCaller(waiter.owner, event, data);
        };
        json event = ApiEnvelope::MakeFailureEvent("Cancelled", ApiErrorCode::CANCELLED, waiter.taskId, waiter.path);
        emitToCaller("audio:fullWaveformFailed", event);
    }

    const char* JobOutcomeName(fb2k_waveform::JobOutcome outcome) {
        switch (outcome) {
            case fb2k_waveform::JobOutcome::Succeeded: return "succeeded";
            case fb2k_waveform::JobOutcome::Aborted: return "aborted";
            default: return "failed";
        }
    }

    // worker 返回后在主线程收尾：按队列的决策写缓存、给每个等待者发终态事件、启动排队的任务
    void OnWaveformJobFinished(uint64_t jobId, const WaveformDecodeResult& result) {
        auto& jobs = GetWaveformJobs();
        jobs.tokens.erase(jobId);
        auto specIt = jobs.specs.find(jobId);
        if (specIt == jobs.specs.end()) return;
        const WaveformJobSpec spec = std::move(specIt->second);
        jobs.specs.erase(specIt);
        // 退出时等待者已全部丢弃，页面也在销毁；晚到的 worker 只释放自己的记录
        if (jobs.shuttingDown) return;

        const auto finished = jobs.queue.Finish(jobId, result.outcome);
        FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=finished job=",
                           std::to_string(jobId).c_str(), " outcome=", JobOutcomeName(result.outcome),
                           " waiters=", std::to_string(finished.notify.size()).c_str());

        // 等待者全部取消、而解码已越过最后一次中止检查时，结果照写缓存，只是没有人收事件
        if (finished.writeCache) {
            WaveformCacheEntry entry;
            entry.raw = result.raw;
            entry.duration = result.duration;
            entry.sampleRate = result.sampleRate;
            entry.channels = result.channels;
            entry.resolution = spec.resolution;
            entry.path = spec.path;
            entry.fileSize = spec.fileSize;
            entry.modifiedTime = spec.modifiedTime;
            WaveformCache::Get().Put(spec.canonicalPath, spec.subsong, spec.resolution, spec.fileSize,
                                     spec.modifiedTime, entry);
        }

        for (const fb2k_waveform::Waiter& waiter : finished.notify) {
            auto emitToCaller = [&waiter](const std::string& event, const json& data) {
                SendToCaller(waiter.owner, event, data);
            };
            if (result.outcome == fb2k_waveform::JobOutcome::Succeeded) {
                // maxAmplitude 是归一化前所选序列的最大值（线性满幅）：linear 档页面用
                // waveform[i] * maxAmplitude 还原绝对电平，db 档 dBFS = (v·60 − 60) + 20·log10(maxAmplitude)
                const fb2k_waveform::Rendered rendered = fb2k_waveform::Render(result.raw, waiter.render);
                json event = {
                    {"taskId", waiter.taskId},
                    {"path", waiter.path},
                    {"waveform", rendered.waveform},
                    {"maxAmplitude", rendered.maxAmplitude},
                    {"duration", result.duration},
                    {"sampleRate", result.sampleRate},
                    {"channels", result.channels},
                    {"resolution", spec.resolution},
                    {"method", WaveformMethodName(waiter.render.method)},
                    {"scale", WaveformScaleName(waiter.render.scale)},
                    {"signed", waiter.render.signedOutput},
                    {"cached", false}
                };
                emitToCaller("audio:fullWaveformReady", event);
            } else if (result.outcome == fb2k_waveform::JobOutcome::Aborted) {
                EmitWaveformCancelled(waiter);
            } else {
                json event = ApiEnvelope::MakeFailureEvent(result.error, result.code, waiter.taskId, waiter.path);
                FailureHook::LogAsync("audio:fullWaveformFailed", result.code, result.error.c_str(),
                                      waiter.taskId.c_str());
                emitToCaller("audio:fullWaveformFailed", event);
            }
        }
        StartWaveformJobs(finished.start);
    }

    json AudioGenerateFullWaveform(const json& params) {
        try {
            // 解析参数
            std::string path = params.value("path", "");
            if (path.empty()) {
                FailureHook::LogSync("audio.generateFullWaveform",
                                     ApiErrorCode::MISSING_PATH, "path is required");
                return ApiEnvelope::MakeError("path is required", ApiErrorCode::MISSING_PATH);
            }

            int resolution = params.value("resolution", 256);
            resolution = std::max(64, std::min(resolution, 4096));

            std::string method = params.value("method", "rms");
            if (method != "peak" && method != "rms") {
                method = "rms";
            }

            std::string scale = params.value("scale", "linear");
            if (scale != "linear" && scale != "db") {
                scale = "linear";
            }

            bool signedOutput = params.value("signed", false);

            bool preferCache = params.value("preferCache", true);

            // 解析 subsong
            auto [filePath, pathSubsong] = ParseSubsongPath(path);

            // cueIndex 优先级高于路径中的 subsong
            t_uint32 subsong = pathSubsong;
            if (params.contains("cueIndex")) {
                int cueIndex = params.value("cueIndex", -1);
                if (cueIndex >= 0) {
                    subsong = static_cast<t_uint32>(cueIndex);
                }
            }

            // 获取规范化路径，供 handle_create / decoder / cache 共用
            pfc::string8 canonicalPathValue;
            filesystem::g_get_canonical_path(filePath.c_str(), canonicalPathValue);
            std::string canonicalPath = canonicalPathValue.get_ptr();
            if (canonicalPath.empty()) {
                canonicalPath = filePath;
            }

            // Trace log: record input → canonical path transformation
            if (canonicalPath != filePath) {
                FB2K_console_print("[PathTransform] audio.generateFullWaveform input=", filePath.c_str(),
                    " output=", canonicalPath.c_str(), " transform=g_get_canonical_path fallback=false");
            }

            // 获取文件状态
            uint64_t fileSize = 0;
            uint64_t modifiedTime = 0;
            try {
                abort_callback_impl abort;
                t_filestats stats;
                bool isWriteable = false;
                filesystem::g_get_stats(filePath.c_str(), stats, isWriteable, abort);
                fileSize = stats.m_size;
                modifiedTime = stats.m_timestamp;
            } catch (...) {
                // 文件状态获取失败，说明路径无效
                FailureHook::LogSync("audio.generateFullWaveform",
                                     ApiErrorCode::INVALID_PATH,
                                     "Invalid path or file not found");
                return ApiEnvelope::MakeError("Invalid path or file not found",
                                              ApiErrorCode::INVALID_PATH);
            }

            // 尝试从缓存获取：条目存原始值，按本次请求的 method / signed / scale 现算
            if (preferCache) {
                WaveformCacheEntry entry;
                if (WaveformCache::Get().TryGet(canonicalPath, subsong, resolution, fileSize, modifiedTime, entry)) {
                    // Lifecycle log: cache hit → ready (no pending state)
                    FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=ready cached=true path=", canonicalPath.c_str());
                    fb2k_waveform::Rendered rendered = RenderWaveform(entry.raw, method, scale, signedOutput);
                    return {
                        {"success", true},
                        {"status", "ready"},
                        {"cached", true},
                        {"waveform", rendered.waveform},
                        {"maxAmplitude", rendered.maxAmplitude},
                        {"duration", entry.duration},
                        {"sampleRate", entry.sampleRate},
                        {"channels", entry.channels},
                        {"resolution", entry.resolution},
                        {"method", method},
                        {"scale", scale},
                        {"signed", signedOutput},
                        {"path", path}
                    };
                }
            }

            // 缓存未命中：交给解码队列。解码键相同的在途任务直接并入，各请求仍拿自己的 taskId
            auto& jobs = GetWaveformJobs();
            if (jobs.shuttingDown) {
                return ApiEnvelope::MakeError("Host is shutting down", ApiErrorCode::OPERATION_FAILED);
            }
            std::string taskId = GenerateTaskId();

            // 在调用时捕获 caller context：事件按它路由回调用者实例，取消时按它核对归属
            auto caller = CallerContext::FromParams(params);
            fb2k_waveform::Waiter waiter;
            waiter.taskId = taskId;
            waiter.path = path;
            waiter.render.method = ToWaveformMethod(method);
            waiter.render.scale = ToWaveformScale(scale);
            waiter.render.signedOutput = signedOutput;
            waiter.owner.hwnd = reinterpret_cast<std::uintptr_t>(caller.callerHwnd);
            waiter.owner.windowId = caller.windowId;

            const std::string decodeKey =
                waveform_cache::MakeKey(canonicalPath, subsong, resolution, fileSize, modifiedTime);
            const auto submitted = jobs.queue.Submit(decodeKey, std::move(waiter));
            if (!submitted.merged) {
                jobs.specs[submitted.jobId] =
                    WaveformJobSpec{path, canonicalPath, subsong, resolution, fileSize, modifiedTime};
            }

            // Lifecycle log: pending → queued (or merged into an in-flight job)
            FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=pending taskId=", taskId.c_str(),
                               " job=", std::to_string(submitted.jobId).c_str(),
                               submitted.merged ? " merged=true" : " merged=false", " path=", canonicalPath.c_str());
            StartWaveformJobs(submitted.start);

            return {
                {"success", true},
                {"status", "pending"},
                {"cached", false},
                {"taskId", taskId},
                {"resolution", resolution},
                {"method", method},
                {"scale", scale},
                {"signed", signedOutput},
                {"path", path}
            };

        } catch (const std::exception& e) {
            FailureHook::LogSync("audio.generateFullWaveform",
                                 ApiErrorCode::EXCEPTION, e.what());
            return ApiEnvelope::MakeError(e.what(), ApiErrorCode::EXCEPTION);
        } catch (...) {
            FailureHook::LogSync("audio.generateFullWaveform",
                                 ApiErrorCode::UNKNOWN_ERROR, "Unknown error");
            return ApiEnvelope::MakeError("Unknown error", ApiErrorCode::UNKNOWN_ERROR);
        }
    }

    // audio.cancelFullWaveform：只认发起请求的调用方。「已结束」「不存在」「不归你」都回
    // cancelled: false、故意不区分，taskId 形如 waveform_N、可以猜
    json AudioCancelFullWaveform(const json& params) {
        if (!params.contains("taskId")) {
            FailureHook::LogSync("audio.cancelFullWaveform", ApiErrorCode::REQUIRED_PARAM, "taskId is required", true);
            return ApiEnvelope::MakeError("taskId is required", ApiErrorCode::REQUIRED_PARAM,
                                          json{{"param", "taskId"}});
        }
        // 校验后直接对 params 取下标再 get<T>()，理由同 AudioSubscribeSpectrum
        const json& raw = params["taskId"];
        if (!raw.is_string() || raw.get<std::string>().empty()) {
            FailureHook::LogSync("audio.cancelFullWaveform", ApiErrorCode::INVALID_PARAMS,
                                 "taskId must be a non-empty string", true);
            return ApiEnvelope::MakeError("taskId must be a non-empty string", ApiErrorCode::INVALID_PARAMS,
                                          json{{"param", "taskId"}, {"value", raw}});
        }
        const std::string taskId = params["taskId"].get<std::string>();

        auto caller = CallerContext::FromParams(params);
        const fb2k_waveform::CallerIdentity canceller{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                      caller.windowId};
        auto& jobs = GetWaveformJobs();
        const auto cancelled = jobs.queue.Cancel(taskId, canceller);
        if (!cancelled.cancelled) {
            return {{"success", true}, {"cancelled", false}};
        }
        if (cancelled.verdict == fb2k_waveform::CancelVerdict::Unattributed) {
            FB2K_console_print("[TaskLifecycle] audio.cancelFullWaveform taskId=", taskId.c_str(),
                               " accepted without ownership check: the request carried no caller identity");
        }
        FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=cancelled taskId=", taskId.c_str());

        EmitWaveformCancelled(cancelled.waiter);
        AbortWaveformJobs(cancelled.abort);
        ForgetWaveformJobs(cancelled.discarded);
        return {{"success", true}, {"cancelled", true}};
    }

    // 退出时取消全部整轨任务：丢弃等待者与排队任务，中止解码中的任务，此后完成的 worker
    // 不写缓存、不发事件。挂 initquit::on_quit 而不是 ShutdownAudioVisualizationRuntime，
    // 理由同 MetadataApi.cpp 的 ProbeShutdownInitQuit：on_quit 与运行模式无关，且发生在
    // 线程池收工之前，worker 还来得及看到中止。
    class WaveformShutdownInitQuit : public initquit {
    public:
        void on_quit() override {
            // 从 on_quit 抛出会打断宿主的关停序列，自己兜住
            try {
                auto& jobs = GetWaveformJobs();
                jobs.shuttingDown = true;
                const auto dropped = jobs.queue.DropAll();
                AbortWaveformJobs(dropped.abort);
                ForgetWaveformJobs(dropped.discarded);
                if (!dropped.abort.empty() || dropped.removedWaiters > 0) {
                    console::printf("audio.generateFullWaveform: cancelled %u waiter(s) and aborted %u decode(s) on quit",
                                    static_cast<unsigned>(dropped.removedWaiters),
                                    static_cast<unsigned>(dropped.abort.size()));
                }
            } catch (...) {
            }
        }
    };

    static initquit_factory_t<WaveformShutdownInitQuit> g_waveform_shutdown_initquit;

    // 置退出闩并清空订阅、停计时线程。面板模式没有任何路径调 ShutdownRuntime，面板与页面在
    // on_quit 之后才销毁；这段时间里排着的订阅 handler 若还能起线程，DLL 卸载时会留下可 join
    // 的 std::thread（std::terminate）。旧实现的定时器挂在核心主窗口上，随窗口销毁自然停下。
    // 析构函数里不能 join：DLL 卸载时持有 loader lock。
    class SpectrumShutdownInitQuit : public initquit {
    public:
        void on_quit() override {
            // 从 on_quit 抛出会打断宿主的关停序列，自己兜住
            try {
                auto& state = SpectrumState::Get();
                state.quitting.store(true);
                state.ShutdownRuntime();
            } catch (...) {
            }
        }
    };

    static initquit_factory_t<SpectrumShutdownInitQuit> g_spectrum_shutdown_initquit;

} // anonymous namespace

//=============================================================================
// 取消指定窗口发起的整轨波形请求（popup 关闭时调用）
//
// 只移除该窗口的等待者、不发事件：页面正在销毁，事件的最终 fallback 会落到主窗口。
// 任务还有别的窗口在等就照常解码，没人等了才中止。
//=============================================================================
void CancelAllWaveformTasksForWindow(const std::string& windowId) {
    try {
        auto& jobs = GetWaveformJobs();
        const auto dropped = jobs.queue.DropWindow(windowId);
        AbortWaveformJobs(dropped.abort);
        ForgetWaveformJobs(dropped.discarded);
        if (dropped.removedWaiters > 0) {
            console::printf("audio.generateFullWaveform: dropped %u waiter(s) for a closing window",
                            static_cast<unsigned>(dropped.removedWaiters));
        }
    } catch (...) {
    }
}

//=============================================================================
// 公开窄接口 — 显式释放频谱可视化运行时
//=============================================================================
void ShutdownAudioVisualizationRuntime() {
    SpectrumState::Get().ShutdownRuntime();
}

//=============================================================================
// Register Audio Analysis API
//=============================================================================
void RegisterAudioApi() {
    auto& bridge = BridgeCore::GetInstance();

    // Spectrum APIs
    bridge.RegisterApi("audio.subscribeSpectrum", AudioSubscribeSpectrum);
    bridge.RegisterApi("audio.unsubscribeSpectrum", AudioUnsubscribeSpectrum);
    bridge.RegisterApi("audio.getSpectrum", AudioGetSpectrum);
    bridge.RegisterApi("audio.getSpectrumDebugState", AudioGetSpectrumDebugState);
    bridge.RegisterApi("audio.getWaveform", AudioGetWaveform);
    bridge.RegisterApi("audio.setChannelMode", AudioSetChannelMode);

    // Stream capture APIs
    bridge.RegisterApi("audio.subscribeStream", AudioSubscribeStream);
    bridge.RegisterApi("audio.unsubscribeStream", AudioUnsubscribeStream);

    // Analysis APIs
    bridge.RegisterApi("audio.analyzeBPM", AudioAnalyzeBPM, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("audio.generateWaveform", AudioGenerateWaveform, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("audio.generateFullWaveform", AudioGenerateFullWaveform, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("audio.cancelFullWaveform", AudioCancelFullWaveform);

    // Info APIs
    bridge.RegisterApi("audio.getOutputInfo", AudioGetOutputInfo);
    bridge.RegisterApi("audio.getStreamInfo", AudioGetStreamInfo);
    bridge.RegisterApi("audio.isVisualizationAvailable", AudioIsVisualizationAvailable);

    LOG("Audio API registered (15 APIs)");
}
