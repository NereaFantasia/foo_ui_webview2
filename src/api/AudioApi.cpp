// AudioApi.cpp - Audio Analysis API
// Provides spectrum, BPM analysis, waveform generation and audio output info

#include "pch.h"
#include "api/AudioApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "core/WebViewContext.h"
#include "utils/SubsongUtils.h"
#include <foobar2000/SDK/vis.h>
#include <foobar2000/SDK/playback_control.h>
#include <foobar2000/SDK/metadb.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <mutex>
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
    // 定时器 ID 和回调前置声明
    constexpr UINT_PTR TIMER_ID_SPECTRUM = 64207;  // 0xFACF
    void CALLBACK SpectrumTimerProc(HWND hwnd, UINT, UINT_PTR id, DWORD);

    struct SpectrumSubscription {
        std::string token;
        std::string windowId;
        HWND ownerHwnd{nullptr};
        std::string eventName{"audio:spectrum"};
        int fftSize{1024};
        int fps{30};
        int bands{48};
    };

    struct SpectrumDispatchTarget {
        std::string windowId;
        HWND ownerHwnd{nullptr};
        std::string eventName{"audio:spectrum"};
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
        std::string eventName;
        visualisation_stream_v3::ptr stream;
        std::mutex streamMutex;
        std::mutex subscriptionsMutex;
        std::unordered_map<std::string, SpectrumSubscription> subscriptions;
        HWND timerHwnd{nullptr};
        
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
        
        void StartTimer() {
            StopTimer();
            timerHwnd = core_api::get_main_window();
            if (!timerHwnd) {
                console::print("[Spectrum] StartTimer: no main window!");
                return;
            }
            int interval = std::max(16, 1000 / std::max(1, fps.load()));
            UINT_PTR result = SetTimer(timerHwnd, TIMER_ID_SPECTRUM, interval, SpectrumTimerProc);
            console::printf("[Spectrum] StartTimer: hwnd=0x%p, interval=%dms, result=%llu",
                (void*)timerHwnd, interval, (unsigned long long)result);
        }
        
        void StopTimer() {
            if (timerHwnd) {
                KillTimer(timerHwnd, TIMER_ID_SPECTRUM);
                timerHwnd = nullptr;
            }
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
        }

        void RefreshRuntime(bool shouldRun) {
            skipFrames.store(0);
            if (shouldRun) {
                active.store(true);
                EnsureStream();
                StartTimer();
            } else {
                active.store(false);
                StopTimer();
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
            StopTimer();
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
        
        bool GetSpectrum(std::vector<float>& outData, int requestedBands = 0) {
            if (!active.load()) return false;
            
            EnsureStream();
            
            std::scoped_lock lock(streamMutex);
            if (!stream.is_valid()) return false;
            
            try {
                double absTime = 0;
                if (!stream->get_absolute_time(absTime)) return false;
                
                audio_chunk_impl chunk;
                int fft = fftSize.load();
                
                // 自动提升 FFT 大小以确保低频分辨率
                // 256 bands log-spaced 在 FFT=4096 时, 20-100Hz 仅有 8 个 bin
                // 导致 ~70 个低频 band 共享相同 bin → 视觉上黏在一起
                // FFT=8192 时 binWidth≈5.4Hz，可在性能和分辨率之间取得更稳妥平衡
                int reqBands = requestedBands > 0 ? requestedBands : bands.load();
                int minFft = reqBands >= 64 ? 8192 : (reqBands >= 32 ? 4096 : fft);
                if (fft < minFft) {
                    fft = minFft;
                    // 确保是 2 的幂
                    fft = (int)pow(2, ceil(log2((double)fft)));
                }
                
                if (!stream->get_spectrum_absolute(chunk, absTime, fft)) return false;

                {
                    const audio_sample* data = chunk.get_data();
                    size_t count = chunk.get_sample_count();
                    unsigned channels = chunk.get_channels();
                    unsigned sampleRate = chunk.get_sample_rate();
                    if (sampleRate == 0) sampleRate = 44100;
                    
                    int numBands = requestedBands > 0 ? requestedBands : bands.load();
                    if (numBands <= 0 || numBands > (int)count) numBands = (int)count;
                    
                    outData.resize(numBands);
                    
                    // --- 处理流水线 ---
                    // 1. 对数频率映射 (log-spaced bands)
                    // 2. Sub-bin 插值 + 三角滤波器 RMS
                    // 3. 频率倾斜补偿 (乘法域)
                    // 4. dB 归一化 + gamma
                    
                    float minFreq = 20.0f;
                    float maxFreq = (float)sampleRate / 2.0f;
                    float binWidth = (float)sampleRate / (float)fft;
                    float logMin = log10f(minFreq);
                    float logMax = log10f(maxFreq);
                    
                    // 辅助: 对 FFT bin 做线性插值, 支持小数位置
                    // 让共享相邻 bin 的频段有不同的插值结果
                    auto interpBin = [&](float binPos) -> float {
                        binPos = std::max(0.0f, std::min(binPos, (float)(count - 1)));
                        int b0 = (int)floorf(binPos);
                        int b1 = std::min(b0 + 1, (int)(count - 1));
                        float frac = binPos - (float)b0;
                        float v0 = 0, v1 = 0;
                        for (unsigned ch = 0; ch < channels; ++ch) {
                            v0 += static_cast<float>(data[b0 * channels + ch]);
                            v1 += static_cast<float>(data[b1 * channels + ch]);
                        }
                        v0 /= channels; v1 /= channels;
                        return v0 + (v1 - v0) * frac;
                    };
                    
                    // dB 归一化参数
                    constexpr float dbFloor = -50.0f;
                    constexpr float dbCeil  =   0.0f;
                    constexpr float dbRange = dbCeil - dbFloor;
                    
                    // 频率倾斜补偿: 只提升中高频, 不衰减低频
                    constexpr float slopeAmount = 1.5f;
                    constexpr float slopeOffset = 20.0f;
                    constexpr float slopeExp = slopeAmount / 6.0f;
                    
                    // Bass shelf: 衰减 200Hz 以下频段，避免 bass 能量饱和导致视觉粘连
                    // 20Hz→0.55, 100Hz→~0.78, 200Hz→1.0  (smoothstep S 曲线)
                    constexpr float bassShelfFloor = 0.55f;   // 20Hz 处衰减保留比
                    constexpr float bassShelfFreq  = 200.0f;  // shelf 拐点频率
                    
                    constexpr float gamma = 0.8f;
                    
                    // 宽频带三角滤波器 RMS 辅助（消除 4-5 层嵌套）
                    auto computeWideBandRMS = [&](float fMinBin, float fMidBin, float fMaxBin) -> float {
                        int loIdx = std::max(1, (int)floorf(fMinBin));
                        int hiIdx = std::min((int)count, (int)ceilf(fMaxBin));
                        // 先收敛到合法范围，再保证 hiIdx > loIdx
                        if (loIdx >= (int)count) loIdx = (int)count - 1;
                        if (hiIdx > (int)count) hiIdx = (int)count;
                        if (hiIdx <= loIdx) hiIdx = loIdx + 1;
                        if (hiIdx > (int)count) hiIdx = (int)count;
                        
                        float sumSq = 0.0f;
                        for (int i = loIdx; i < hiIdx; ++i) {
                            float fi = (float)i;
                            float weight = (fi <= fMidBin)
                                ? ((fMidBin > fMinBin) ? (fi - fMinBin) / (fMidBin - fMinBin) : 1.0f)
                                : ((fMaxBin > fMidBin) ? (fMaxBin - fi) / (fMaxBin - fMidBin) : 1.0f);
                            weight = std::max(weight, 0.05f);
                            
                            float mag = 0.0f;
                            for (unsigned ch = 0; ch < channels; ++ch)
                                mag += static_cast<float>(data[i * channels + ch]);
                            mag /= channels;
                            float wm = mag * weight;
                            sumSq += wm * wm;
                        }
                        return sqrtf(sumSq);
                    };
                    
                    for (int b = 0; b < numBands; ++b) {
                        float f0 = powf(10.0f, logMin + (logMax - logMin) * b / numBands);
                        float f1 = powf(10.0f, logMin + (logMax - logMin) * (b + 1) / numBands);
                        float fCenter = sqrtf(f0 * f1);
                        
                        float fMinBin = f0 / binWidth;
                        float fMidBin = fCenter / binWidth;
                        float fMaxBin = f1 / binWidth;
                        
                        float bandWidthBins = fMaxBin - fMinBin;
                        float rmsVal = (bandWidthBins < 2.0f)
                            ? interpBin(fMidBin)
                            : computeWideBandRMS(fMinBin, fMidBin, fMaxBin);
                        
                        // 频率倾斜补偿
                        float tilt = powf(fCenter / slopeOffset, slopeExp);
                        
                        // Bass shelf 衰减: smoothstep 从 20Hz (0.55) 平滑过渡到 200Hz (1.0)
                        float bassShelf = 1.0f;
                        if (fCenter < bassShelfFreq) {
                            float t = (fCenter - 20.0f) / (bassShelfFreq - 20.0f);
                            t = std::max(0.0f, std::min(1.0f, t));
                            bassShelf = bassShelfFloor + (1.0f - bassShelfFloor) * t * t * (3.0f - 2.0f * t);
                        }
                        rmsVal *= tilt * bassShelf;
                        
                        // dB + 归一化 + gamma
                        float db = 20.0f * log10f(std::max(rmsVal, 1e-10f));
                        float normalized = (db - dbFloor) / dbRange;
                        normalized = std::max(0.0f, std::min(1.0f, normalized));
                        outData[b] = powf(normalized, gamma);
                    }
                    return true;
                }
            } catch (...) {
                // Silently ignore — returns false below
            }
            
            return false;
        }
        
        bool GetWaveform(std::vector<float>& outData, double duration, bool signedOutput = false) {
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
                size_t count = chunk.get_sample_count();
                unsigned channels = chunk.get_channels();
                
                outData.resize(count);
                for (size_t i = 0; i < count; ++i) {
                    float sum = 0;
                    for (unsigned ch = 0; ch < channels; ++ch) {
                        sum += static_cast<float>(data[i * channels + ch]);
                    }
                    float linear = sum / channels;
                    if (signedOutput) {
                        // 保留正负极性，clamp 到 [-1, 1]
                        outData[i] = std::max(-1.0f, std::min(1.0f, linear));
                    } else {
                        // 原始行为: 绝对值 → dB 归一化 → [0, 1]
                        float db = 20.0f * log10f(std::max(fabsf(linear), 1e-10f));
                        float normalized = (db + 70.0f) / 70.0f;
                        outData[i] = std::max(0.0f, std::min(1.0f, normalized));
                    }
                }
                return true;
            } catch (...) {
                // Silently ignore — returns false below
            }
            
            return false;
        }
    };

    //=========================================================================
    // 频谱定时器回调 — 定期读取频谱数据并推送到 JS
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

    void EmitSpectrumToTargets(const std::vector<SpectrumDispatchTarget>& targets, const std::vector<float>& spectrum) {
        if (targets.empty() || spectrum.empty()) {
            return;
        }

        json data;
        data["spectrum"] = spectrum;

        auto& context = WebViewContext::GetInstance();
        for (const auto& target : targets) {
            if (!target.windowId.empty() &&
                context.SendEventTo(target.windowId, target.eventName, data)) {
                continue;
            }

            if (!target.ownerHwnd) {
                continue;
            }

            if (auto* bridge = context.GetBridge(target.ownerHwnd)) {
                bridge->EmitEvent(target.eventName, data);
                continue;
            }

            if (auto* bridge = FindBridgeByTopLevelAncestor(context, target.ownerHwnd)) {
                bridge->EmitEvent(target.eventName, data);
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

    void CALLBACK SpectrumTimerProc(HWND, UINT, UINT_PTR, DWORD) {
        // 作用域重入护卫——避免任何提前 return 或异常漏清理状态
        static std::atomic_bool s_inProgress{false};
        const ScopedSpectrumTimerGuard guard(s_inProgress);
        if (!guard) {
            return;
        }

        try {
            auto& state = SpectrumState::Get();
            if (!state.active.load()) {
                return;
            }

            auto targets = state.CollectDispatchTargets();
            if (targets.empty()) {
                return;
            }

            // 过载保护：上一帧超预算时，主动跳过一帧给主线程让路
            int pendingSkip = state.skipFrames.load();
            if (pendingSkip > 0) {
                state.skipFrames.store(std::max(0, pendingSkip - 1));
                return;
            }

            const DWORD startedAt = GetTickCount();
            if (ShouldThrottleSpectrumForExternalForeground(state, startedAt)) {
                return;
            }

            std::vector<float> spectrum;
            if (state.GetSpectrum(spectrum) && !spectrum.empty()) {
                EmitSpectrumToTargets(targets, spectrum);
            }

            HandleSpectrumFrameBudget(state, startedAt);
        } catch (const std::exception& e) {
            console::printf("[Spectrum] Timer exception: %s", e.what());
        } catch (...) {
            console::print("[Spectrum] Timer unknown exception");
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
    
    json AudioSubscribeSpectrum(const json& params) {
        int fftSize = params.value("fftSize", 1024);
        std::string eventName = params.value("event", "audio:spectrum");
        int fps = params.value("fps", 30);
        auto caller = CallerContext::FromParams(params);
        std::string windowId = caller.windowId;
        HWND ownerHwnd = caller.callerHwnd;
        std::string subscriptionId = ResolveSpectrumSubscriptionId(params, ownerHwnd, eventName);
        
        if (fftSize < 256 || fftSize > 16384 || (fftSize & (fftSize - 1)) != 0) {
            return {
                {"success", false},
                {"error", "fftSize must be a power of 2 between 256 and 16384"}
            };
        }
        
        int numBands = params.value("bands", 48);
        
        auto& state = SpectrumState::Get();
        SpectrumSubscription subscription;
        subscription.token = subscriptionId;
        subscription.windowId = windowId;
        subscription.ownerHwnd = ownerHwnd;
        subscription.eventName = eventName;
        subscription.fftSize = fftSize;
        subscription.fps = std::max(1, std::min(fps, 60));
        subscription.bands = std::max(8, std::min(numBands, fftSize / 2));
        state.UpsertSubscription(subscription);
        
        // Lifecycle log: subscription created/updated
        FB2K_console_print("[SpectrumLifecycle] subscribe id=", subscriptionId.c_str(),
            " fftSize=", fftSize, " bands=", subscription.bands, " fps=", subscription.fps);

        return {
            {"success", state.stream.is_valid()},
            {"subscriptionId", subscriptionId},
            {"fftSize", fftSize},
            {"bands", subscription.bands},
            {"fps", subscription.fps},
            {"event", eventName}
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
                    {"bands", subscription.bands}
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
            {"timerRunning", state.timerHwnd != nullptr},
            {"timerHwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(state.timerHwnd))},
            {"effectiveFftSize", state.fftSize.load()},
            {"effectiveFps", state.fps.load()},
            {"effectiveBands", state.bands.load()},
            {"skipFrames", state.skipFrames.load()},
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
    
    json AudioGetSpectrum(const json& params) {
        int requestedBands = params.value("bands", 0);
        if (requestedBands < 0) {
            return {
                {"success", false},
                {"error", "bands must be >= 0"}
            };
        }

        std::vector<float> spectrum;
        bool ok = SpectrumState::Get().GetSpectrum(spectrum, requestedBands);
        
        if (!ok || spectrum.empty()) {
            return {
                {"success", false},
                {"error", "No spectrum data available. Subscribe first or check if audio is playing."}
            };
        }
        
        return {
            {"success", true},
            {"spectrum", spectrum},
            {"fftSize", SpectrumState::Get().fftSize.load()},
            {"bands", spectrum.size()}
        };
    }
    
    json AudioGetWaveform(const json& params) {
        double duration = params.value("duration", 0.05);
        bool signedOutput = params.value("signed", false);
        
        std::vector<float> waveform;
        bool ok = SpectrumState::Get().GetWaveform(waveform, duration, signedOutput);
        
        if (!ok || waveform.empty()) {
            return {
                {"success", false},
                {"error", "No waveform data available"}
            };
        }
        
        return {
            {"success", true},
            {"waveform", waveform},
            {"duration", duration},
            {"signed", signedOutput}
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
            auto mdb = metadb::get();
            metadb_handle_ptr handle;
            
            pfc::string8 fb2kPath(path.c_str());
            handle = mdb->handle_create(fb2kPath, 0);
            
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
            auto mdb = metadb::get();
            metadb_handle_ptr handle;
            
            pfc::string8 fb2kPath(path.c_str());
            handle = mdb->handle_create(fb2kPath, 0);
            
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

    // 缓存条目
    struct WaveformCacheEntry {
        std::vector<float> waveform;
        double duration;
        int sampleRate;
        int channels;
        int resolution;
        std::string method;
        std::string scale;
        std::string path;
        uint64_t fileSize;
        uint64_t modifiedTime;
        std::chrono::steady_clock::time_point lastAccess;
    };

    // 缓存管理器
    class WaveformCache {
    private:
        std::mutex mutex_;
        std::map<std::string, WaveformCacheEntry> cache_;
        static constexpr size_t MAX_ENTRIES = 50;

        std::string MakeCacheKey(const std::string& canonicalPath, t_uint32 subsong,
                                 int resolution, const std::string& method,
                                 const std::string& scale,
                                 uint64_t fileSize, uint64_t modifiedTime) {
            char buf[512];
            sprintf_s(buf, "%s|%u|%d|%s|%s|%llu|%llu",
                     canonicalPath.c_str(), subsong, resolution, method.c_str(),
                     scale.c_str(), fileSize, modifiedTime);
            return buf;
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

        bool TryGet(const std::string& canonicalPath, t_uint32 subsong,
                   int resolution, const std::string& method,
                   const std::string& scale,
                   uint64_t fileSize, uint64_t modifiedTime,
                   WaveformCacheEntry& outEntry) {
            std::scoped_lock lock(mutex_);
            std::string key = MakeCacheKey(canonicalPath, subsong, resolution, method, scale, fileSize, modifiedTime);
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                it->second.lastAccess = std::chrono::steady_clock::now();
                outEntry = it->second;
                return true;
            }
            return false;
        }

        void Put(const std::string& canonicalPath, t_uint32 subsong,
                int resolution, const std::string& method,
                const std::string& scale,
                uint64_t fileSize, uint64_t modifiedTime,
                const WaveformCacheEntry& entry) {
            std::scoped_lock lock(mutex_);
            EvictLRU();
            std::string key = MakeCacheKey(canonicalPath, subsong, resolution, method, scale, fileSize, modifiedTime);
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

    // 后台任务执行
    // callerHwnd / callerWindowId 在调用时捕获，用于把事件路由回正确的实例
    void ExecuteWaveformGeneration(std::string taskId, std::string path, std::string canonicalPath,
                                   t_uint32 subsong, int resolution, std::string method,
                                   std::string scale, bool signedOutput,
                                   uint64_t fileSize, uint64_t modifiedTime,
                                   HWND callerHwnd, std::string callerWindowId) {

        fb2k::inCpuWorkerThread([taskId = std::move(taskId), path = std::move(path), canonicalPath = std::move(canonicalPath), subsong, resolution, method = std::move(method), scale = std::move(scale), signedOutput, fileSize, modifiedTime, callerHwnd, callerWindowId = std::move(callerWindowId)]() {
            // 路由事件到调用者实例的辅助 lambda
            // callerHwnd / callerWindowId 在调用方 FromParams 时已确定
            auto emitToCaller = [callerHwnd, callerWindowId](const std::string& event, const json& data) {
                auto& wvc = WebViewContext::GetInstance();
                // 优先 windowId
                if (!callerWindowId.empty() && wvc.SendEventTo(callerWindowId, event, data)) return;
                // 其次直接 hwnd
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
            };

            try {
                abort_callback_impl abort;

                // 创建 metadb handle
                auto mdb = metadb::get();
                metadb_handle_ptr handle;

                pfc::string8 fb2kPath(canonicalPath.c_str());
                handle = mdb->handle_create(fb2kPath, subsong);

                if (!handle.is_valid()) {
                    fb2k::inMainThread([emitToCaller, taskId, path]() {
                        json event = ApiEnvelope::MakeFailureEvent(
                            "Failed to create metadb handle",
                            ApiErrorCode::INVALID_HANDLE, taskId, path);
                        FailureHook::LogAsync("audio:fullWaveformFailed",
                            ApiErrorCode::INVALID_HANDLE,
                            "Failed to create metadb handle", taskId.c_str());
                        emitToCaller("audio:fullWaveformFailed", event);
                    });
                    return;
                }

                AudioTechnicalInfo audioInfo;
                AudioTechnicalInfoStatus infoStatus = ResolveAudioTechnicalInfo(handle, canonicalPath,
                                                                                subsong, audioInfo);
                if (infoStatus == AudioTechnicalInfoStatus::MissingInfo) {
                    fb2k::inMainThread([emitToCaller, taskId, path]() {
                        json event = ApiEnvelope::MakeFailureEvent(
                            "Failed to get file info",
                            ApiErrorCode::NO_INFO, taskId, path);
                        FailureHook::LogAsync("audio:fullWaveformFailed",
                            ApiErrorCode::NO_INFO,
                            "Failed to get file info", taskId.c_str());
                        emitToCaller("audio:fullWaveformFailed", event);
                    });
                    return;
                }

                if (infoStatus == AudioTechnicalInfoStatus::InvalidParams) {
                    fb2k::inMainThread([emitToCaller, taskId, path]() {
                        json event = ApiEnvelope::MakeFailureEvent(
                            "Invalid audio parameters",
                            ApiErrorCode::INVALID_PARAMS, taskId, path);
                        FailureHook::LogAsync("audio:fullWaveformFailed",
                            ApiErrorCode::INVALID_PARAMS,
                            "Invalid audio parameters", taskId.c_str());
                        emitToCaller("audio:fullWaveformFailed", event);
                    });
                    return;
                }

                double duration = audioInfo.duration;
                int sampleRate = audioInfo.sampleRate;
                int channels = audioInfo.channels;

                // 打开音频输入
                service_ptr_t<input_decoder> decoder;
                input_entry::g_open_for_decoding(decoder, nullptr, fb2kPath, abort);

                if (!decoder.is_valid()) {
                    fb2k::inMainThread([emitToCaller, taskId, path]() {
                        json event = ApiEnvelope::MakeFailureEvent(
                            "Failed to open decoder",
                            ApiErrorCode::DECODER_FAILED, taskId, path);
                        FailureHook::LogAsync("audio:fullWaveformFailed",
                            ApiErrorCode::DECODER_FAILED,
                            "Failed to open decoder", taskId.c_str());
                        emitToCaller("audio:fullWaveformFailed", event);
                    });
                    return;
                }

                // 初始化解码器
                decoder->initialize(subsong, input_flag_no_looping, abort);

                // 计算窗口大小
                uint64_t totalSamples = static_cast<uint64_t>(duration * sampleRate);
                uint64_t samplesPerWindow = totalSamples / resolution;
                if (samplesPerWindow == 0) samplesPerWindow = 1;

                std::vector<float> waveform(resolution, 0.0f);
                std::vector<float> windowBuffer;
                int currentWindow = 0;
                float maxValue = 0.0f;
                uint64_t totalProcessedSamples = 0;  // 全局已处理样本帧计数

                bool isPeak = (method == "peak");

                // 辅助: 计算单样本帧的多声道值（消除 if(isPeak) > for(ch) 嵌套）
                auto computeSampleValue = [signedOutput](const audio_sample* samples, size_t i,
                                             unsigned chCount, bool peak) -> float {
                    if (signedOutput) {
                        // signed 模式: 多声道平均，保留正负极性
                        float sum = 0.0f;
                        for (unsigned ch = 0; ch < chCount; ch++) {
                            sum += static_cast<float>(samples[i * chCount + ch]);
                        }
                        return sum / chCount;
                    }
                    if (peak) {
                        float value = 0.0f;
                        for (unsigned ch = 0; ch < chCount; ch++) {
                            float s = static_cast<float>(samples[i * chCount + ch]);
                            value = std::max(value, std::abs(s));
                        }
                        return value;
                    }
                    float sumSq = 0.0f;
                    for (unsigned ch = 0; ch < chCount; ch++) {
                        float s = static_cast<float>(samples[i * chCount + ch]);
                        sumSq += s * s;
                    }
                    return sumSq / chCount;
                };

                // 辅助: 窗口聚合（消除 if(isPeak)/else 嵌套）
                auto finalizeWindow = [signedOutput](const std::vector<float>& buf, bool peak) -> float {
                    if (signedOutput) {
                        // signed 模式: 选取窗口内绝对值最大的样本（保留符号）
                        float best = 0.0f;
                        for (float v : buf) {
                            if (std::abs(v) > std::abs(best)) best = v;
                        }
                        return best;
                    }
                    if (peak)
                        return *std::max_element(buf.begin(), buf.end());
                    float sum = std::accumulate(buf.begin(), buf.end(), 0.0f);
                    return std::sqrt(sum / buf.size());
                };

                // 解码并处理音频数据
                audio_chunk_impl_temporary chunk;
                while (decoder->run(chunk, abort)) {
                    const audio_sample* samples = chunk.get_data();
                    size_t sampleCount = chunk.get_sample_count();
                    unsigned chCount = chunk.get_channels();

                    for (size_t i = 0; i < sampleCount; i++) {
                        uint64_t globalSampleIndex = totalProcessedSamples + i;
                        int windowIndex = static_cast<int>(globalSampleIndex / samplesPerWindow);
                        if (windowIndex >= resolution) break;

                        float value = computeSampleValue(samples, i, chCount, isPeak);

                        if (windowIndex == currentWindow) {
                            windowBuffer.push_back(value);
                            continue;
                        }
                        // 窗口边界 — 完成当前窗口
                        if (!windowBuffer.empty()) {
                            float windowValue = finalizeWindow(windowBuffer, isPeak);
                            waveform[currentWindow] = windowValue;
                            maxValue = std::max(maxValue, windowValue);
                        }
                        currentWindow = windowIndex;
                        windowBuffer.clear();
                        windowBuffer.push_back(value);
                    }

                    // 更新全局已处理样本计数
                    totalProcessedSamples += sampleCount;
                }

                // 处理最后一个窗口
                if (!windowBuffer.empty() && currentWindow < resolution) {
                    float windowValue = finalizeWindow(windowBuffer, isPeak);
                    waveform[currentWindow] = windowValue;
                    maxValue = std::max(maxValue, windowValue);
                }

                // 归一化：简单 max 归一化（JS 层负责视觉动态范围拉伸）
                if (signedOutput) {
                    // signed 模式: 找最大绝对值，归一化到 [-1, 1]
                    float absMax = 0.0f;
                    for (float v : waveform) {
                        absMax = std::max(absMax, std::abs(v));
                    }
                    if (absMax > 0.0f) {
                        for (float& v : waveform) {
                            v /= absMax;
                        }
                    }
                } else {
                    if (maxValue > 0.0f) {
                        for (float& v : waveform) {
                            v /= maxValue;
                        }
                    }
                }

                // dB scale mapping (only when scale == "db", not applicable in signed mode)
                if (scale == "db" && !signedOutput) {
                    const float dbFloor = -60.0f;
                    for (float& v : waveform) {
                        if (v <= 0.0f) { v = 0.0f; continue; }
                        float db = 20.0f * std::log10f(v);
                        v = std::max(0.0f, (db - dbFloor) / (-dbFloor));
                    }
                }

                // 保存到缓存
                WaveformCacheEntry entry;
                entry.waveform = waveform;
                entry.duration = duration;
                entry.sampleRate = sampleRate;
                entry.channels = channels;
                entry.resolution = resolution;
                entry.method = method;
                entry.scale = scale;
                entry.path = path;
                entry.fileSize = fileSize;
                entry.modifiedTime = modifiedTime;

                WaveformCache::Get().Put(canonicalPath, subsong, resolution, method,
                                         scale, fileSize, modifiedTime, entry);

                // 切回主线程发送成功事件
                fb2k::inMainThread([emitToCaller, taskId, path, waveform, duration, sampleRate, channels, resolution, method, scale, signedOutput]() {
                    json event = {
                        {"taskId", taskId},
                        {"path", path},
                        {"waveform", waveform},
                        {"duration", duration},
                        {"sampleRate", sampleRate},
                        {"channels", channels},
                        {"resolution", resolution},
                        {"method", method},
                        {"scale", scale},
                        {"signed", signedOutput},
                        {"cached", false}
                    };
                    emitToCaller("audio:fullWaveformReady", event);
                });

            } catch (const std::exception& e) {
                std::string errMsg = e.what();
                fb2k::inMainThread([emitToCaller, errMsg, taskId, path]() {
                    json event = ApiEnvelope::MakeFailureEvent(
                        errMsg, ApiErrorCode::DECODE_FAILED, taskId, path);
                    FailureHook::LogAsync("audio:fullWaveformFailed",
                        ApiErrorCode::DECODE_FAILED,
                        errMsg.c_str(), taskId.c_str());
                    emitToCaller("audio:fullWaveformFailed", event);
                });
            } catch (...) {
                fb2k::inMainThread([emitToCaller, taskId, path]() {
                    json event = ApiEnvelope::MakeFailureEvent(
                        "Unknown error during waveform generation",
                        ApiErrorCode::UNKNOWN_ERROR, taskId, path);
                    FailureHook::LogAsync("audio:fullWaveformFailed",
                        ApiErrorCode::UNKNOWN_ERROR,
                        "Unknown error during waveform generation", taskId.c_str());
                    emitToCaller("audio:fullWaveformFailed", event);
                });
            }
        });
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

            // 尝试从缓存获取
            if (preferCache) {
                WaveformCacheEntry entry;
                if (WaveformCache::Get().TryGet(canonicalPath, subsong, resolution, method,
                                                scale, fileSize, modifiedTime, entry)) {
                    // Lifecycle log: cache hit → ready (no pending state)
                    FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=ready cached=true path=", canonicalPath.c_str());
                    return {
                        {"success", true},
                        {"status", "ready"},
                        {"cached", true},
                        {"waveform", entry.waveform},
                        {"duration", entry.duration},
                        {"sampleRate", entry.sampleRate},
                        {"channels", entry.channels},
                        {"resolution", entry.resolution},
                        {"method", entry.method},
                        {"scale", entry.scale},
                        {"path", path}
                    };
                }
            }

            // 缓存未命中，创建后台任务
            std::string taskId = GenerateTaskId();

            // 在调用时捕获 caller context，用于把异步事件路由回调用者实例
            auto caller = CallerContext::FromParams(params);

            // Lifecycle log: pending → background task created
            FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=pending taskId=", taskId.c_str(), " path=", canonicalPath.c_str());

            // 派发后台任务
            ExecuteWaveformGeneration(taskId, path, canonicalPath, subsong,
                                     resolution, method, scale, signedOutput,
                                     fileSize, modifiedTime,
                                     caller.callerHwnd, caller.windowId);

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

} // anonymous namespace

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

    // Info APIs
    bridge.RegisterApi("audio.getOutputInfo", AudioGetOutputInfo);
    bridge.RegisterApi("audio.getStreamInfo", AudioGetStreamInfo);
    bridge.RegisterApi("audio.isVisualizationAvailable", AudioIsVisualizationAvailable);

    LOG("Audio API registered (14 APIs)");
}
