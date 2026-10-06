// AudioApi.cpp - Audio Analysis API
// Provides spectrum, BPM analysis, waveform generation and audio output info

#include "pch.h"
#include "api/AudioApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/EventEmit.h"
#include "api/LiveWaveform.h"
#include "api/PcmLayout.h"
#include "api/PcmPack.h"
#include "api/PcmRing.h"
#include "api/SpectrumBands.h"
#include "api/SpectrumSchedule.h"
#include "api/TypedApi.h"
#include "api/WaveformAccumulator.h"
#include "api/WaveformTaskQueue.h"
#include "api/generated/AudioSchema.h"
#include "core/WebViewContext.h"
#include "utils/HostTime.h"
#include "utils/SubsongUtils.h"
#include "utils/VisWindowPlan.h"
#include "utils/WaveformCacheKey.h"
#include "webview/SharedPcmBuffer.h"
#include "webview/WebViewHost.h"
#include <foobar2000/SDK/vis.h>
#include <foobar2000/SDK/playback_control.h>
#include <foobar2000/SDK/metadb.h>
#include <foobar2000/SDK/playback_stream_capture.h>
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
    namespace audio = api::audio;

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

    // 单次 get_chunk_absolute 超过约 1 s 就失败（docs/report/audio-timing/CLOCK_MEASUREMENT_REPORT_2026-09-25.md
    // §5.5），更长的窗口拆开取。
    constexpr double kMaxChunkRequestSeconds = 1.0;
    // 这条流还没取到过数据时先按这个采样率规划窗口，取回的块不是这个采样率就按实际的重取。
    constexpr unsigned kAssumedSampleRate = 44100;
    // 拆开取时后面各段前后各多取的帧数，与接缝最多挪动的帧数（实测错一个样本）。起点取早了，段尾
    // 就短一截，所以段尾也要多取；最后一段因此多取到时钟之后。后面各段比规划的长两倍这么多帧，
    // 最多超出 kMaxChunkRequestSeconds 这么多（48 kHz 下 0.7 ms）。
    constexpr size_t kSeamGuardFrames = 16;
    constexpr size_t kSeamMaxShiftFrames = 4;
    // 整窗是零时不取样本，只取这么长一段认出流现在的采样率与声道
    constexpr double kFormatProbeSeconds = 0.001;

    std::string ResolveSpectrumSubscriptionId(const std::optional<std::string>& requested, HWND ownerHwnd,
                                              const std::string& eventName) {
        if (requested) {
            return *requested;
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

    bool IsSupportedSpectrumOutput(const std::string& output) {
        return output == "bands" || output == "bins";
    }

    bool IsSupportedSpectrumChannels(const std::string& channels) {
        return channels == "mix" || channels == "stereo";
    }

    fb2k_spectrum::SpectrumOutput ToScheduleOutput(const std::string& output) {
        return output == "bins" ? fb2k_spectrum::SpectrumOutput::Bins : fb2k_spectrum::SpectrumOutput::Bands;
    }

    fb2k_spectrum::SpectrumChannels ToScheduleChannels(const std::string& channels) {
        return channels == "stereo" ? fb2k_spectrum::SpectrumChannels::Stereo
                                    : fb2k_spectrum::SpectrumChannels::Mix;
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

    // 帧里的 hostTime 与 PCM 头的 hostTimeMs：与 playback 的 hostTime 同一个时钟。
    double HostTimeUnixMs() { return host_time::NowUnixMs(); }

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
        std::string eventName{audio::events::Spectrum::kName};
        int fftSize{1024};
        int fps{30};
        int bands{48};
        std::string scale{"weighted"};
        bool backgroundThrottle{true};
        SpectrumRange range;
        // 'bands' 出对数频带（bands、scale 生效）；'bins' 出线性频点，scale 恒为 'db'，
        // channels 可取 'stereo'（docs/audio-visualization/SPECTRUM_BINS_SPEC.md）。
        std::string output{"bands"};
        std::string channels{"mix"};
        fb2k_spectrum::ScheduleState schedule;
        // 上一帧频谱的流事实，静音帧沿用；没出过帧时为 0。
        unsigned lastSampleRate{0};
        double lastStreamTime{0.0};
        unsigned lastChannelCount{0};
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
        params.output = ToScheduleOutput(subscription.output);
        params.channels = ToScheduleChannels(subscription.channels);
        return params;
    }

    // 订阅实际用的点数：频带输出按带数自动提升，频点输出按请求值。
    int EffectiveFftSizeOf(const SpectrumSubscription& subscription) {
        return fb2k_spectrum::EffectiveFftSize(subscription.fftSize, subscription.bands,
                                               ToScheduleOutput(subscription.output), kMaxSpectrumFftSize);
    }

    // 帧的流事实。频谱帧取自本拍的 chunk；静音帧沿用该订阅上一帧的值，没有则为 0。
    struct SpectrumFrameFacts {
        unsigned sampleRate{0};
        double streamTime{0.0};
        unsigned channelCount{0};
    };

    // 一次 FFT 的产物：频带输出填 bands，频点输出填 bins。
    struct SpectrumComputed {
        std::vector<float> bands;
        fb2k_spectrum::DbBins bins;
        SpectrumFrameFacts facts;
    };

    struct SpectrumDispatchTarget {
        std::string windowId;
        HWND ownerHwnd{nullptr};
        std::string eventName{audio::events::Spectrum::kName};
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
        // 所有订阅中最大的实际 FFT 点数（含按 bands 自动提升），定流保留多长的历史
        std::atomic<int> maxEffectiveFftSize{0};

        // 以下五项只在持有 streamMutex 时读写。
        // 上一次取数时的可视化时钟；读数比它小说明起播、seek 或换曲把时钟复位了
        double lastClock{0.0};
        // 上一次 request_backlog 的秒数；0 表示这条流还没请求过，或者要重新请求
        double requestedBacklog{0.0};
        // 上一次取回的块的格式：下一次按这个采样率规划窗口，整窗是零时按这个格式造零
        unsigned fetchedSampleRate{0};
        unsigned fetchedChannels{0};
        unsigned fetchedChannelConfig{0};

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
            // 仅在流创建成功后应用通道模式。
            if (stream.is_valid()) {
                ApplyChannelModeToStreamLocked();
                // 建流后立即请求历史长度，第一次取数时流里已经攒着
                lastClock = 0.0;
                requestedBacklog = 0.0;
                try {
                    RefreshBacklogLocked(0.0, 0);
                } catch (const std::exception& e) {
                    console::printf("[Spectrum] request_backlog failed: %s", e.what());
                }
            }
        }

        // 流保留的历史 = max(最大窗长, getWaveform 的 duration 上限) + 0.5 s，且不低于 2 s
        // 65536 点实测需要 2 s。fftSize 是本次要算的点数：
        // 不给 subscriptionId 的拉取按请求的 bands 自动提升，可能大于所有订阅。
        double DesiredBacklogLocked(int fftSize) const {
            const unsigned rate = fetchedSampleRate != 0 ? fetchedSampleRate : kAssumedSampleRate;
            const int points = std::max(maxEffectiveFftSize.load(), fftSize);
            const double window = std::max(static_cast<double>(points) / rate, kMaxLiveWaveformDuration);
            return std::max(window + 0.5, 2.0);
        }

        // 先前的请求在起播、seek、换曲后不保留，所以时钟一回退就重新请求，不必另挂 play_callback；
        // 要的比已请求的长时也补一次。两次稀疏的调用之间发生的复位看不出回退，取数失败的调用方
        // 因此把 requestedBacklog 清零，下一次照样重新请求。
        void RefreshBacklogLocked(double clock, int fftSize) {
            const double desired = DesiredBacklogLocked(fftSize);
            if (requestedBacklog <= 0.0 || clock < lastClock || desired > requestedBacklog) {
                stream->request_backlog(desired);
                requestedBacklog = desired;
            }
            lastClock = clock;
        }

        // 取截至可视化时钟 clock 的窗口（规格 T8）：按 vis_window::PlanWindow 分段取数、拼接、
        // 前面补零，结果是按声道交错的 PCM。samplesAt(采样率) 给出窗口的样本数。规划所用的采样率
        // 与取回的块不同时按取回的重取一次。第一段多取的样本从段首丢掉，保证段尾对齐；后面各段前后
        // 各多取 kSeamGuardFrames 帧，按 vis_window::FindSeam 接上。整窗是零时不取样本，只取一小段
        // 认出流现在的格式，否则帧会带上一首或猜的采样率。任何一段取不到、样本不够或几段格式不一致
        // 都返回 false。
        template <typename SamplesAt>
        bool FetchWindowLocked(double clock, SamplesAt samplesAt, audio_chunk_impl& out) {
            unsigned rate = fetchedSampleRate != 0 ? fetchedSampleRate : kAssumedSampleRate;
            for (int attempt = 0; attempt < 2; ++attempt) {
                const uint64_t windowSamples = samplesAt(rate);
                const vis_window::Plan plan =
                    vis_window::PlanWindow(clock, windowSamples, rate, kMaxChunkRequestSeconds);

                std::vector<audio_chunk_impl> pieces(plan.segments.size());
                std::vector<size_t> guards(plan.segments.size(), 0);
                unsigned pieceRate = 0;
                unsigned pieceChannels = 0;
                unsigned pieceChannelConfig = 0;
                if (plan.segments.empty()) {
                    audio_chunk_impl probe;
                    if (stream->get_chunk_absolute(probe, 0.0, kFormatProbeSeconds)) {
                        pieceRate = probe.get_sample_rate();
                        pieceChannels = probe.get_channels();
                        pieceChannelConfig = probe.get_channel_config();
                    }
                }
                // 从最新的一段往回取。先取旧段再取新段时，fb2k 之后对同样远的历史会返回更新的音频：
                // 便携实例上挂着 65536 点订阅时，往回 1.37 s 的请求大多拿到约 0.7 s 前的数据，拼出的
                // 窗口前半段是后半段的复本；倒过来取，拼出的窗口与别处的请求都对。
                for (size_t order = 0; order < plan.segments.size(); ++order) {
                    const size_t i = plan.segments.size() - 1 - order;
                    const vis_window::Segment& segment = plan.segments[i];
                    // 第一段不用接缝；从 0 起的段前面没有可多取的
                    const size_t guard =
                        i == 0 ? 0 : static_cast<size_t>(std::min<uint64_t>(kSeamGuardFrames, segment.start));
                    guards[i] = guard;
                    audio_chunk_impl& piece = pieces[i];
                    if (!stream->get_chunk_absolute(piece, static_cast<double>(segment.start - guard) / rate,
                                                    static_cast<double>(segment.count + 2 * guard) / rate)) {
                        return false;
                    }
                    if (order == 0) {
                        pieceRate = piece.get_sample_rate();
                        pieceChannels = piece.get_channels();
                        pieceChannelConfig = piece.get_channel_config();
                        if (pieceRate == 0 || pieceChannels == 0) return false;
                        if (pieceRate != rate) break;
                    } else if (piece.get_sample_rate() != pieceRate || piece.get_channels() != pieceChannels) {
                        return false;
                    }
                    if (piece.get_sample_count() < segment.count) return false;
                }
                if (pieceRate != 0 && pieceRate != rate) {
                    rate = pieceRate;
                    fetchedSampleRate = pieceRate;
                    continue;
                }

                unsigned channels = pieceChannels != 0 ? pieceChannels : fetchedChannels;
                unsigned channelConfig = pieceChannels != 0 ? pieceChannelConfig : fetchedChannelConfig;
                if (channels == 0) {
                    // 整窗是零、格式又认不出：声道数不影响全零窗口的频谱，按单声道造
                    channels = 1;
                    channelConfig = audio_chunk::g_guess_channel_config(1);
                }

                out.set_data_size(static_cast<size_t>(windowSamples) * channels);
                out.set_sample_count(static_cast<size_t>(windowSamples));
                out.set_channels(channels, channelConfig);
                out.set_srate(rate);
                audio_sample* const begin = out.get_data();
                audio_sample* dst = begin;
                const size_t padValues = static_cast<size_t>(plan.zeroPad) * channels;
                std::fill(dst, dst + padValues, audio_sample(0));
                dst += padValues;
                for (size_t i = 0; i < pieces.size(); ++i) {
                    const auto count = static_cast<size_t>(plan.segments[i].count);
                    const size_t available = pieces[i].get_sample_count();
                    size_t first = available - count;
                    if (i > 0) {
                        const auto written = static_cast<size_t>(dst - begin) / channels;
                        first = vis_window::FindSeam(begin, written, pieces[i].get_data(), available, channels,
                                                     guards[i], kSeamMaxShiftFrames);
                        if (first + count > available) return false;
                    }
                    const audio_sample* src = pieces[i].get_data() + first * channels;
                    dst = std::copy(src, src + count * channels, dst);
                }
                fetchedSampleRate = rate;
                fetchedChannels = channels;
                fetchedChannelConfig = channelConfig;
                return true;
            }
            return false;
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
                maxEffectiveFftSize.store(0);
                eventName = audio::events::Spectrum::kName;
                return;
            }

            int maxFft = 256;
            int maxEffectiveFft = 0;
            int maxFps = 1;
            int maxBands = 8;
            std::string primaryEvent = audio::events::Spectrum::kName;
            for (const auto& [_, subscription] : subscriptions) {
                maxFft = std::max(maxFft, subscription.fftSize);
                maxEffectiveFft = std::max(maxEffectiveFft, EffectiveFftSizeOf(subscription));
                maxFps = std::max(maxFps, subscription.fps);
                maxBands = std::max(maxBands, subscription.bands);
                if (primaryEvent == audio::events::Spectrum::kName && !subscription.eventName.empty()) {
                    primaryEvent = subscription.eventName;
                }
            }

            fftSize.store(maxFft);
            maxEffectiveFftSize.store(maxEffectiveFft);
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

        // 只移除归 canceller 的订阅，归属规则同 unsubscribeStream（fb2k_waveform::JudgeCancel）：
        // 给了 token 只看这一个，没给就是它的全部；登记时没有调用方身份的订阅谁都能移除。
        size_t RemoveSubscriptions(const std::optional<std::string>& token,
                                   const fb2k_waveform::CallerIdentity& canceller) {
            bool shouldRun = false;
            size_t removedCount = 0;
            {
                std::scoped_lock lock(subscriptionsMutex);
                for (auto it = subscriptions.begin(); it != subscriptions.end();) {
                    const SpectrumSubscription& subscription = it->second;
                    const fb2k_waveform::CallerIdentity owner{
                        reinterpret_cast<std::uintptr_t>(subscription.ownerHwnd), subscription.windowId};
                    if ((token && it->first != *token) ||
                        fb2k_waveform::JudgeCancel(owner, canceller) == fb2k_waveform::CancelVerdict::Denied) {
                        ++it;
                        continue;
                    }
                    it = subscriptions.erase(it);
                    ++removedCount;
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
        
        // 算一次频谱：窗口是截至可视化时钟的最近 config.fftSize 个样本，时钟没走满一窗时前面补零
        // 再交给 fb2k 的 FFT。不用 get_spectrum_absolute：它取的
        // 窗口超前一个窗长，还会拨动可视化时钟。config.fftSize 是实际点数（由 EffectiveFftSize 按
        // 输出算好），与 SpectrumBands 的前置条件一致。流无效、流未出数据（停止、暂停、预热期）、
        // 窗口取不全都返回 false；频点输出的区间里没有频点时照样返回 true，数组为空。
        bool ComputeSpectrum(const fb2k_spectrum::ComputeGroup& config, SpectrumComputed& out) {
            if (!active.load()) return false;

            EnsureStream();

            std::scoped_lock lock(streamMutex);
            if (!stream.is_valid()) return false;

            try {
                double absTime = 0;
                if (!stream->get_absolute_time(absTime)) return false;
                RefreshBacklogLocked(absTime, config.fftSize);

                audio_chunk_impl window;
                const auto windowSamples = static_cast<uint64_t>(config.fftSize);
                if (!FetchWindowLocked(absTime, [windowSamples](unsigned) { return windowSamples; }, window)) {
                    requestedBacklog = 0.0;
                    return false;
                }
                // 中心偏移按规格取窗长的一半；临时构建实测它不影响频带值
                audio_chunk_impl chunk;
                stream->chunk_to_spectrum(window, chunk, config.fftSize / 2.0 / window.get_sample_rate());

                out.facts.sampleRate = chunk.get_sample_rate();
                out.facts.channelCount = chunk.get_channels();
                out.facts.streamTime = absTime;
                if (config.output == fb2k_spectrum::SpectrumOutput::Bins) {
                    fb2k_spectrum::ComputeDbBins(chunk.get_data(), chunk.get_sample_count(),
                                                 chunk.get_channels(), out.facts.sampleRate, config.fftSize,
                                                 config.channels, out.bins, config.minFrequency,
                                                 config.maxFrequency);
                } else if (config.scale == fb2k_spectrum::SpectrumScale::Db) {
                    fb2k_spectrum::ComputeDbBands(chunk.get_data(), chunk.get_sample_count(),
                                                  chunk.get_channels(), out.facts.sampleRate, config.fftSize,
                                                  config.bands, out.bands, config.minFrequency,
                                                  config.maxFrequency);
                } else {
                    fb2k_spectrum::ComputeWeightedBands(chunk.get_data(), chunk.get_sample_count(),
                                                        chunk.get_channels(), out.facts.sampleRate,
                                                        config.fftSize, config.bands, out.bands,
                                                        config.minFrequency, config.maxFrequency);
                }
                framesComputed.fetch_add(1);
                return true;
            } catch (...) {
                // Silently ignore — returns false below
            }

            return false;
        }
        
        // getWaveform 的一次取样：截至可视化时钟的最近 duration 秒，时钟没走满 duration 时前面补零
        // （规格 T8）。stereo 为真时填 left / right，否则填 mix；points 为 0 表示不降采样。
        bool GetWaveform(double duration, bool signedOutput, bool stereo, size_t points,
                         LiveWaveformResult& out) {
            if (!active.load()) return false;

            EnsureStream();

            std::scoped_lock lock(streamMutex);
            if (!stream.is_valid()) return false;

            try {
                double absTime = 0;
                if (!stream->get_absolute_time(absTime)) return false;
                RefreshBacklogLocked(absTime, 0);

                audio_chunk_impl chunk;
                const auto samplesAt = [duration](unsigned rate) {
                    return static_cast<uint64_t>(std::llround(duration * rate));
                };
                if (!FetchWindowLocked(absTime, samplesAt, chunk)) {
                    requestedBacklog = 0.0;
                    return false;
                }

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

    // 出帧所需的参数：订阅的参数，或不带 subscriptionId 的拉取按请求填的参数。
    // fftSizeActual 是实际点数（EffectiveFftSize）。
    struct SpectrumFrameSpec {
        int bands{0};
        int fftSizeActual{0};
        std::string scale{"weighted"};
        std::string output{"bands"};
        std::string channels{"mix"};
        SpectrumRange range;
    };

    SpectrumFrameSpec FrameSpecOf(const SpectrumSubscription& subscription) {
        SpectrumFrameSpec spec;
        spec.bands = subscription.bands;
        spec.fftSizeActual = EffectiveFftSizeOf(subscription);
        spec.scale = subscription.scale;
        spec.output = subscription.output;
        spec.channels = subscription.channels;
        spec.range = subscription.range;
        return spec;
    }

    fb2k_spectrum::ComputeGroup ComputeConfigOf(const SpectrumFrameSpec& spec) {
        fb2k_spectrum::ComputeGroup config;
        config.fftSize = spec.fftSizeActual;
        config.bands = spec.bands;
        config.scale = ToScheduleScale(spec.scale);
        config.minFrequency = spec.range.minFrequency;
        config.maxFrequency = spec.range.maxFrequency;
        config.output = ToScheduleOutput(spec.output);
        config.channels = ToScheduleChannels(spec.channels);
        return config;
    }

    // 组一帧，推送与 getSpectrum 共用这一份形状：Frame 是 audio:spectrum 的载荷或 getSpectrum 的应答，
    // 两者由同一个声明生成、字段相同。subscriptionId 为空时不带该字段（getSpectrum 未给
    // subscriptionId 的应答）。computed 为空指针表示静音帧：频带按 scale 填满静音值；频点按 facts 的
    // 采样率推出与上一帧相同的一段频点、填满下限，没有上一帧（采样率为 0）时数组为空。流事实一律取 facts。
    template <class Frame>
    Frame BuildSpectrumFrame(const std::string& subscriptionId, const SpectrumFrameSpec& spec,
                             const SpectrumComputed* computed, fb2k_spectrum::PlaybackState state,
                             const SpectrumFrameFacts& facts) {
        Frame frame;
        auto values = [](const auto& source) { return std::vector<double>(source.begin(), source.end()); };
        if (spec.output == "bins") {
            const bool stereo = spec.channels == "stereo";
            if (computed) {
                frame.firstBin = static_cast<std::int64_t>(computed->bins.firstBin);
                if (stereo) {
                    frame.left = values(computed->bins.left);
                    frame.right = values(computed->bins.right);
                } else {
                    frame.spectrum = values(computed->bins.mix);
                }
            } else {
                fb2k_spectrum::DbBinSpan span;
                if (facts.sampleRate != 0) {
                    span = fb2k_spectrum::FindDbBinSpan(static_cast<size_t>(spec.fftSizeActual / 2),
                                                        facts.sampleRate, spec.fftSizeActual,
                                                        spec.range.minFrequency, spec.range.maxFrequency);
                }
                const std::vector<double> silence(span.count, static_cast<double>(fb2k_spectrum::kDbBandsFloor));
                frame.firstBin = static_cast<std::int64_t>(span.first);
                if (stereo) {
                    frame.left = silence;
                    frame.right = silence;
                } else {
                    frame.spectrum = silence;
                }
            }
            frame.output = "bins";
            frame.channels = spec.channels;
            frame.channelCount = facts.channelCount;
            // 频点恒为 dB 功率
            frame.scale = "db";
        } else {
            if (computed) {
                frame.spectrum = values(computed->bands);
            } else {
                const double silence = spec.scale == "db" ? fb2k_spectrum::kDbBandsFloor : 0.0;
                frame.spectrum = std::vector<double>(static_cast<size_t>(std::max(0, spec.bands)), silence);
            }
            frame.output = "bands";
            frame.bands = spec.bands;
            frame.scale = spec.scale;
        }
        if (!subscriptionId.empty()) {
            frame.subscriptionId = subscriptionId;
        }
        frame.fftSize = spec.fftSizeActual;
        frame.sampleRate = facts.sampleRate;
        frame.minFrequency = spec.range.minFrequency;
        // 缺省上限就是 sampleRate / 2，向下取整；给了上限就报截到 sampleRate / 2 之后的实际值
        if (spec.range.maxFrequency > 0.0) {
            frame.maxFrequency = std::min(spec.range.maxFrequency, facts.sampleRate / 2.0);
        } else {
            frame.maxFrequency = std::floor(facts.sampleRate / 2.0);
        }
        frame.state = PlaybackStateName(state);
        frame.streamTime = facts.streamTime;
        frame.hostTime = HostTimeUnixMs();
        return frame;
    }

    void EmitSpectrumFrame(const SpectrumDispatchTarget& target, const audio::events::Spectrum::Payload& frame) {
        using Spectrum = audio::events::Spectrum;
        if (!target.windowId.empty() &&
            api::emit::SendToNamed<Spectrum>(target.windowId, target.eventName, frame)) {
            return;
        }

        if (!target.ownerHwnd) {
            return;
        }

        auto& context = WebViewContext::GetInstance();
        if (auto* bridge = context.GetBridge(target.ownerHwnd)) {
            api::emit::EmitNamed<Spectrum>(*bridge, target.eventName, frame);
            return;
        }

        // 面板模式 fallback: 同一顶级窗口下的 instance；都找不到就丢弃这一帧
        if (HWND instance = CallerContext::FindInstanceUnderRoot(target.ownerHwnd)) {
            if (auto* bridge = context.GetBridge(instance)) {
                api::emit::EmitNamed<Spectrum>(*bridge, target.eventName, frame);
            }
        }
    }

    // 一拍里某个订阅出帧所需的一切，在订阅锁内抓取，投递在锁外。
    struct SpectrumFrameJob {
        SpectrumDispatchTarget target;
        SpectrumFrameSpec spec;
        SpectrumFrameFacts lastFacts;
    };

    struct SpectrumGroupResult {
        bool ok{false};
        SpectrumComputed computed;
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
                job.spec = FrameSpecOf(subscription);
                job.lastFacts = {subscription.lastSampleRate, subscription.lastStreamTime,
                                 subscription.lastChannelCount};
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
                it->second.lastChannelCount = facts.channelCount;
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
                results[i].ok = state.ComputeSpectrum(plan.groups[i], results[i].computed);
            }

            std::vector<std::pair<std::string, SpectrumFrameFacts>> factsUpdates;
            for (const auto& frame : plan.frames) {
                if (!frame.deliver) continue;
                auto jobIt = jobs.find(frame.subscriptionId);
                if (jobIt == jobs.end()) continue;
                const SpectrumFrameJob& job = jobIt->second;

                if (frame.silent) {
                    EmitSpectrumFrame(job.target, BuildSpectrumFrame<audio::events::Spectrum::Payload>(
                                                      frame.subscriptionId, job.spec, nullptr, frame.state,
                                                      job.lastFacts));
                    continue;
                }
                if (frame.group < 0 || static_cast<size_t>(frame.group) >= results.size()) continue;
                const SpectrumGroupResult& result = results[frame.group];
                if (!result.ok) continue;

                EmitSpectrumFrame(job.target, BuildSpectrumFrame<audio::events::Spectrum::Payload>(
                                                  frame.subscriptionId, job.spec, &result.computed, frame.state,
                                                  result.computed.facts));
                factsUpdates.emplace_back(frame.subscriptionId, result.computed.facts);
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
    // API Handlers
    //=========================================================================
    
    // minFrequency / maxFrequency 的校验，订阅与不带 subscriptionId 的 getSpectrum 共用。生成的解析器
    // 只查了下限，无穷与上下限的关系在这里查；通过时填好 range。上限相对流的 Nyquist 是否有效
    // 要到出帧时才知道（采样率随曲目变），这里只查与下限的关系。
    std::optional<api::Failure> CheckSpectrumRange(double minFrequency, const std::optional<double>& maxFrequency,
                                                   const char* method, SpectrumRange& range) {
        if (!std::isfinite(minFrequency)) {
            FailureHook::LogSync(method, ApiErrorCode::INVALID_PARAMS, "minFrequency out of range", true);
            return api::Fail("minFrequency must be a number of at least 1", ApiErrorCode::INVALID_PARAMS,
                             {{"details", json{{"param", "minFrequency"}, {"value", minFrequency}}}});
        }
        range.minFrequency = minFrequency;
        if (maxFrequency) {
            if (!std::isfinite(*maxFrequency) || *maxFrequency <= minFrequency) {
                FailureHook::LogSync(method, ApiErrorCode::INVALID_PARAMS, "maxFrequency out of range", true);
                return api::Fail("maxFrequency must be a number greater than minFrequency",
                                 ApiErrorCode::INVALID_PARAMS,
                                 {{"details", json{{"param", "maxFrequency"}, {"value", *maxFrequency}}}});
            }
            range.maxFrequency = *maxFrequency;
        }
        return std::nullopt;
    }

    std::optional<double> RequestedMaxFrequency(const SpectrumRange& range) {
        if (range.maxFrequency > 0.0) return range.maxFrequency;
        return std::nullopt;
    }

    api::Failure SpectrumParamError(const char* method, const char* log, const char* message, const char* param,
                                    const json& value) {
        FailureHook::LogSync(method, ApiErrorCode::INVALID_PARAMS, log, true);
        return api::Fail(message, ApiErrorCode::INVALID_PARAMS, {{"details", json{{"param", param}, {"value", value}}}});
    }

    api::Result<audio::SubscribeSpectrumResult> AudioSubscribeSpectrum(const audio::SubscribeSpectrumParams& p,
                                                                        const CallerContext& caller) {
        constexpr const char* kMethod = "audio.subscribeSpectrum";
        // 生成的解析器查过范围与类型，这里只剩 2 的幂和几个参数之间的组合规则
        const auto fftSize = static_cast<int>(p.fftSize);
        if ((fftSize & (fftSize - 1)) != 0) {
            return SpectrumParamError(kMethod, "fftSize out of range",
                                      "fftSize must be a power of 2 between 256 and 65536", "fftSize", fftSize);
        }
        // 分声道只有频点输出支持；频点恒为 dB 功率，显式给了别的 scale 算参数错
        if (p.channels == "stereo" && p.output != "bins") {
            return SpectrumParamError(kMethod, "stereo channels need bin output",
                                      "channels 'stereo' requires output 'bins'", "channels", p.channels);
        }
        std::string scale = p.scale.value_or("weighted");
        if (p.output == "bins") {
            if (p.scale && *p.scale != "db") {
                return SpectrumParamError(kMethod, "bin output only has the db scale",
                                          "scale must be 'db' when output is 'bins'", "scale", *p.scale);
            }
            scale = "db";
        }

        SpectrumRange range;
        if (auto error = CheckSpectrumRange(p.minFrequency, p.maxFrequency, kMethod, range)) {
            return *error;
        }

        auto& state = SpectrumState::Get();
        // on_quit 之后不登记、不建可视化流，否则关停期间还会再起计时线程
        if (state.quitting.load()) {
            FailureHook::LogSync(kMethod, ApiErrorCode::OPERATION_FAILED, "host is shutting down");
            return api::Fail("Host is shutting down", ApiErrorCode::OPERATION_FAILED);
        }
        const std::string subscriptionId =
            ResolveSpectrumSubscriptionId(p.subscriptionId, caller.callerHwnd, p.event);

        SpectrumSubscription subscription;
        subscription.token = subscriptionId;
        subscription.windowId = caller.windowId;
        subscription.ownerHwnd = caller.callerHwnd;
        subscription.eventName = p.event;
        subscription.fftSize = fftSize;
        subscription.fps = static_cast<int>(std::clamp<std::int64_t>(p.fps, 1, 60));
        subscription.bands = static_cast<int>(std::clamp<std::int64_t>(p.bands, 8, fftSize / 2));
        subscription.scale = scale;
        subscription.backgroundThrottle = p.backgroundThrottle;
        subscription.range = range;
        subscription.output = p.output;
        subscription.channels = p.channels;
        // 登记时的播放状态是初始状态，不算一次进入
        subscription.schedule = fb2k_spectrum::MakeScheduleState(ReadPlaybackState());
        state.UpsertSubscription(subscription);

        // Lifecycle log: subscription created/updated
        FB2K_console_print("[SpectrumLifecycle] subscribe id=", subscriptionId.c_str(),
            " fftSize=", fftSize, " bands=", subscription.bands, " fps=", subscription.fps,
            " scale=", scale.c_str(), " backgroundThrottle=", p.backgroundThrottle ? "true" : "false",
            " minFrequency=", range.minFrequency, " maxFrequency=", range.maxFrequency,
            " output=", p.output.c_str(), " channels=", p.channels.c_str());

        // 登记成功就是成功；流是否有效另给 streamReady（停止态下也为 true）
        audio::SubscribeSpectrumResult result;
        result.subscriptionId = subscriptionId;
        result.fftSize = fftSize;
        result.bands = subscription.bands;
        result.fps = subscription.fps;
        result.scale = scale;
        result.backgroundThrottle = p.backgroundThrottle;
        result.minFrequency = range.minFrequency;
        result.maxFrequency = RequestedMaxFrequency(range);
        result.output = p.output;
        result.channels = p.channels;
        result.event = p.event;
        result.streamReady = state.stream.is_valid();
        return result;
    }

    api::Result<audio::UnsubscribeSpectrumResult> AudioUnsubscribeSpectrum(const audio::UnsubscribeSpectrumParams& p,
                                                                            const CallerContext& caller) {
        const fb2k_waveform::CallerIdentity canceller{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                      caller.windowId};
        const size_t removedCount = SpectrumState::Get().RemoveSubscriptions(p.subscriptionId, canceller);
        const std::string subscriptionId = p.subscriptionId.value_or("");

        // Lifecycle log: subscription removed
        FB2K_console_print("[SpectrumLifecycle] unsubscribe id=", subscriptionId.c_str(),
            " removed=", (int)removedCount);

        audio::UnsubscribeSpectrumResult result;
        result.removed = static_cast<std::int64_t>(removedCount);
        result.subscriptionId = subscriptionId;
        return result;
    }

    // 计时线程没在运行时两者都是 null
    std::optional<std::string> BeatSourceValue(const SpectrumState& state) {
        if (!state.beatThreadRunning.load()) return std::nullopt;
        return state.beatSource.load() == kBeatSourceHighResolution ? "high-resolution" : "standard";
    }

    std::optional<double> BeatIntervalValue(const SpectrumState& state) {
        if (!state.beatThreadRunning.load()) return std::nullopt;
        return static_cast<double>(state.beatIntervalNs.load()) / 1e6;
    }

    std::int64_t HwndValue(HWND hwnd) {
        return static_cast<std::int64_t>(reinterpret_cast<std::intptr_t>(hwnd));
    }

    api::Result<audio::GetSpectrumDebugStateResult> AudioGetSpectrumDebugState(
        const audio::GetSpectrumDebugStateParams& /*params*/, const CallerContext& caller) {
        auto& state = SpectrumState::Get();
        HWND callerHwnd = caller.callerHwnd;
        auto& context = WebViewContext::GetInstance();
        auto foregroundInfo = GetForegroundWindowInfo();
        auto dispatchTargets = state.CollectDispatchTargets();

        audio::GetSpectrumDebugStateResult result;
        {
            std::scoped_lock lock(state.subscriptionsMutex);
            for (const auto& [token, subscription] : state.subscriptions) {
                audio::SpectrumDebugSubscription entry;
                entry.token = token;
                entry.windowId = subscription.windowId;
                entry.ownerHwnd = HwndValue(subscription.ownerHwnd);
                entry.event = subscription.eventName;
                entry.fftSize = subscription.fftSize;
                entry.fps = subscription.fps;
                entry.bands = subscription.bands;
                entry.scale = subscription.scale;
                entry.backgroundThrottle = subscription.backgroundThrottle;
                entry.minFrequency = subscription.range.minFrequency;
                entry.maxFrequency = RequestedMaxFrequency(subscription.range);
                entry.output = subscription.output;
                entry.channels = subscription.channels;
                result.subscriptions.push_back(std::move(entry));
                if (callerHwnd && subscription.ownerHwnd == callerHwnd) {
                    result.callerOwnsSubscription = true;
                }
            }
        }

        for (const auto& target : dispatchTargets) {
            audio::SpectrumDebugTarget entry;
            entry.windowId = target.windowId;
            entry.ownerHwnd = HwndValue(target.ownerHwnd);
            entry.event = target.eventName;
            result.dispatchTargets.push_back(std::move(entry));
        }

        result.active = state.active.load();
        result.timerRunning = state.beatThreadRunning.load();
        // 已废弃：旧实现的定时器挂在核心主窗口上，计时线程没有窗口，恒为 0，留给读它的旧脚本
        result.timerHwnd = 0;
        result.beatSource = BeatSourceValue(state);
        result.beatIntervalMs = BeatIntervalValue(state);
        result.beatsCoalesced = static_cast<std::int64_t>(state.beatsCoalesced.load());
        result.effectiveFftSize = state.fftSize.load();
        result.effectiveFps = state.fps.load();
        result.effectiveBands = state.bands.load();
        result.skipFrames = state.skipFrames.load();
        result.framesComputed = static_cast<std::int64_t>(state.framesComputed.load());
        result.streamReady = state.stream.is_valid();
        result.subscriptionCount = static_cast<std::int64_t>(result.subscriptions.size());
        result.dispatchTargetCount = static_cast<std::int64_t>(result.dispatchTargets.size());
        result.instanceCount = static_cast<std::int64_t>(context.GetInstanceCount());
        result.callerHwnd = HwndValue(callerHwnd);
        result.callerWindowId = callerHwnd ? context.GetWindowIdByHwnd(callerHwnd) : "";
        result.foregroundHwnd = HwndValue(foregroundInfo.hwnd);
        result.foregroundPid = foregroundInfo.pid;
        result.foregroundIsExternal = foregroundInfo.isExternal;
        result.foregroundTitle = foregroundInfo.title;
        return result;
    }

    // 拉取一帧。给了 subscriptionId 就用该订阅的参数与上一帧的流事实；
    // 没给则 FFT 点数取所有订阅请求值的最大值再按本次 bands 自动提升（现行语义）。
    api::Result<audio::GetSpectrumResult> AudioGetSpectrum(const audio::GetSpectrumParams& p) {
        constexpr const char* kMethod = "audio.getSpectrum";
        auto& state = SpectrumState::Get();
        std::string subscriptionId;
        SpectrumFrameSpec spec;
        SpectrumFrameFacts lastFacts;

        if (p.subscriptionId) {
            subscriptionId = *p.subscriptionId;
            std::scoped_lock lock(state.subscriptionsMutex);
            auto it = state.subscriptions.find(subscriptionId);
            if (it == state.subscriptions.end()) {
                FailureHook::LogSync(kMethod, ApiErrorCode::NOT_FOUND, "subscription not found", true);
                return api::Fail("subscription not found", ApiErrorCode::NOT_FOUND,
                                 {{"details", json{{"param", "subscriptionId"}, {"value", subscriptionId}}}});
            }
            const SpectrumSubscription& subscription = it->second;
            spec = FrameSpecOf(subscription);
            lastFacts = {subscription.lastSampleRate, subscription.lastStreamTime, subscription.lastChannelCount};
        } else {
            // 这条路径的点数取所有订阅的最大请求值，调用方控制不了分辨率，所以只出频带；
            // 频点与分声道要带 subscriptionId（docs/audio-visualization/SPECTRUM_BINS_SPEC.md B8）
            if (p.output != "bands") {
                return SpectrumParamError(kMethod, "output needs a subscriptionId",
                                          "output must be 'bands' when no subscriptionId is given", "output",
                                          p.output);
            }
            if (p.channels != "mix") {
                return SpectrumParamError(kMethod, "channels need a subscriptionId",
                                          "channels must be 'mix' when no subscriptionId is given", "channels",
                                          p.channels);
            }
            if (auto error = CheckSpectrumRange(p.minFrequency, p.maxFrequency, kMethod, spec.range)) {
                return *error;
            }
            spec.scale = p.scale;
            spec.output = p.output;
            spec.channels = p.channels;
            // 先截到点数上限，免得超大的请求值转成 int 时溢出；最后还要截到 fftSize / 2
            spec.bands = p.bands > 0 ? static_cast<int>(std::min<std::int64_t>(p.bands, kMaxSpectrumFftSize))
                                     : state.bands.load();
            spec.fftSizeActual = fb2k_spectrum::EffectiveFftSize(state.fftSize.load(), spec.bands,
                                                                 kMaxSpectrumFftSize);
            // 频带数不超过频点数：分带函数本来就按频点数截断，这里跟着截，
            // 帧里的 bands 才与 spectrum 长度一致，静音帧也不会按超大的请求值铺开
            spec.bands = std::min(spec.bands, spec.fftSizeActual / 2);
        }

        if (!state.active.load()) {
            return api::Fail("No spectrum data available. Subscribe first or check if audio is playing.",
                             ApiErrorCode::OPERATION_FAILED);
        }

        // 暂停或停止：静音帧、不算 FFT
        const fb2k_spectrum::PlaybackState playback = ReadPlaybackState();
        if (playback != fb2k_spectrum::PlaybackState::Playing) {
            return BuildSpectrumFrame<audio::GetSpectrumResult>(subscriptionId, spec, nullptr, playback, lastFacts);
        }

        // 播放中流未出数据（含预热期）。频点区间里没有频点不算失败，数组为空。
        SpectrumComputed computed;
        if (!state.ComputeSpectrum(ComputeConfigOf(spec), computed)) {
            return api::Fail("No spectrum data available. Subscribe first or check if audio is playing.",
                             ApiErrorCode::OPERATION_FAILED);
        }
        return BuildSpectrumFrame<audio::GetSpectrumResult>(subscriptionId, spec, &computed, playback, computed.facts);
    }

    api::Result<audio::GetWaveformResult> AudioGetWaveform(const audio::GetWaveformParams& p) {
        // 生成的解析器只查了上限，大于 0 与有限在这里查
        if (!std::isfinite(p.duration) || p.duration <= 0.0) {
            return SpectrumParamError("audio.getWaveform", "duration out of range",
                                      "duration must be a number greater than 0 and at most 1", "duration",
                                      p.duration);
        }

        const bool stereo = p.channels == "stereo";
        const size_t points = p.points ? static_cast<size_t>(*p.points) : 0;
        LiveWaveformResult samples;
        if (!SpectrumState::Get().GetWaveform(p.duration, p.signed_, stereo, points, samples)) {
            return api::Fail("No waveform data available", ApiErrorCode::OPERATION_FAILED);
        }

        audio::GetWaveformResult result;
        if (stereo) {
            result.left = std::vector<double>(samples.left.begin(), samples.left.end());
            result.right = std::vector<double>(samples.right.begin(), samples.right.end());
        } else {
            result.waveform = std::vector<double>(samples.mix.begin(), samples.mix.end());
        }
        result.duration = p.duration;
        result.signed_ = p.signed_;
        result.channels = p.channels;
        result.sampleRate = samples.sampleRate;
        result.channelCount = samples.channelCount;
        return result;
    }

    api::Result<audio::SetChannelModeResult> AudioSetChannelMode(const audio::SetChannelModeParams& p) {
        int chMode = 0;
        if (p.mode == "mono") chMode = 1;
        else if (p.mode == "front") chMode = 2;
        else if (p.mode == "back") chMode = 3;

        auto& state = SpectrumState::Get();
        state.channelMode.store(chMode);

        // 通过统一带锁 helper 同步 channelMode 到现有 stream
        state.ApplyChannelModeToStream();

        audio::SetChannelModeResult result;
        result.mode = p.mode;
        return result;
    }

    // 只读 BPM 标签，宿主不做节拍分析。
    api::Result<audio::AnalyzeBPMResult> AudioAnalyzeBPM(const audio::AnalyzeBPMParams& p) {
        try {
            // 先拆出 subsong 再规范化路径，使 CUE 子曲目的 handle 身份与播放列表一致。
            auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(p.path);
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsong);

            if (!handle.is_valid()) {
                return api::Fail("Failed to open file", ApiErrorCode::INVALID_HANDLE);
            }

            metadb_info_container::ptr infoContainer;
            try {
                infoContainer = SubsongUtils::GetInfoOrReadFile(handle);
            } catch (const std::exception& e) {
                return api::Fail(std::string("Failed to read file info: ") + e.what(), ApiErrorCode::NO_INFO);
            }

            const char* bpmStr = infoContainer->info().meta_get("BPM", 0);
            const double bpm = bpmStr ? std::atof(bpmStr) : 0.0;
            if (bpm > 0 && bpm < 500) {
                audio::AnalyzeBPMResult result;
                result.bpm = bpm;
                result.confidence = 1.0;
                result.source = "metadata";
                return result;
            }
            return api::Fail("BPM tag not found", ApiErrorCode::NOT_FOUND);
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    api::Result<audio::GetOutputInfoResult> AudioGetOutputInfo(const audio::GetOutputInfoParams& /*params*/) {
        try {
            const double volume = playback_control::get()->get_volume();
            audio::GetOutputInfoResult result;
            result.volume = volume;
            result.volumePercent = std::pow(10.0, volume / 20.0) * 100.0;
            return result;
        } catch (...) {
            return api::Fail("Failed to get output info", ApiErrorCode::OPERATION_FAILED);
        }
    }

    api::Result<audio::GetStreamInfoResult> AudioGetStreamInfo(const audio::GetStreamInfoParams& /*params*/) {
        try {
            auto pc = playback_control::get();
            audio::GetStreamInfoResult result;
            if (!pc->is_playing()) {
                result.playing = false;
                return result;
            }

            metadb_handle_ptr nowPlaying;
            if (pc->get_now_playing(nowPlaying) && nowPlaying.is_valid()) {
                // Use get_info_ref() to avoid file_info_impl value copy overhead
                metadb_info_container::ptr infoContainer = nowPlaying->get_info_ref();
                if (infoContainer.is_valid()) {
                    const file_info& info = infoContainer->info();
                    const char* codec = info.info_get("codec");
                    result.playing = true;
                    result.sampleRate = info.info_get_int("samplerate");
                    result.channels = info.info_get_int("channels");
                    result.bitrate = info.info_get_int("bitrate");
                    result.codec = codec ? codec : "unknown";
                    result.duration = info.get_length();
                    return result;
                }
            }
        } catch (...) {
            // 与读不到信息同样处理，走下面的失败
        }
        return api::Fail("Failed to get stream info", ApiErrorCode::OPERATION_FAILED);
    }

    api::Result<audio::IsVisualizationAvailableResult> AudioIsVisualizationAvailable(
        const audio::IsVisualizationAvailableParams& /*params*/) {
        audio::IsVisualizationAvailableResult result;
        try {
            result.available = visualisation_manager::get().is_valid();
        } catch (...) {
            // 取服务失败就当没有
        }
        return result;
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
        // 用返回 bool 的重载判断有没有缓存：无参的 get_info_ref() 在没有信息时也返回非空的
        // 占位信息，拿它判断恒为真，下面「缓存与直读都取不到」的分支就永远走不到。
        metadb_info_container::ptr infoContainer;
        const bool hasCachedInfo = handle->get_info_ref(infoContainer);

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

    // 把整轨波形事件路由回发起请求的实例；身份在请求时由 CallerContext 确定。
    // 先按 windowId，再按句柄解析（同一顶级窗口下的 instance，最后是主窗口页面）。
    template <class E>
    void SendToCaller(const fb2k_waveform::CallerIdentity& owner, const typename E::Payload& payload) {
        api::emit::ToCaller<E>(owner.windowId, reinterpret_cast<HWND>(owner.hwnd), payload);
    }

    audio::FullWaveformFailedPayload MakeWaveformFailed(const fb2k_waveform::Waiter& waiter, const char* code,
                                                        const std::string& error) {
        audio::FullWaveformFailedPayload payload;
        payload.taskId = waiter.taskId;
        payload.path = waiter.path;
        payload.error = error;
        payload.code = code;
        return payload;
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
        SendToCaller<audio::events::FullWaveformFailed>(waiter.owner,
                                                        MakeWaveformFailed(waiter, ApiErrorCode::CANCELLED, "Cancelled"));
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
            if (result.outcome == fb2k_waveform::JobOutcome::Succeeded) {
                // maxAmplitude 是归一化前所选序列的最大值（线性满幅）：linear 档页面用
                // waveform[i] * maxAmplitude 还原绝对电平，db 档 dBFS = (v·60 − 60) + 20·log10(maxAmplitude)
                const fb2k_waveform::Rendered rendered = fb2k_waveform::Render(result.raw, waiter.render);
                audio::FullWaveformReadyPayload event;
                event.taskId = waiter.taskId;
                event.path = waiter.path;
                event.waveform.assign(rendered.waveform.begin(), rendered.waveform.end());
                event.maxAmplitude = rendered.maxAmplitude;
                event.duration = result.duration;
                event.sampleRate = result.sampleRate;
                event.channels = result.channels;
                event.resolution = spec.resolution;
                event.method = WaveformMethodName(waiter.render.method);
                event.scale = WaveformScaleName(waiter.render.scale);
                event.signed_ = waiter.render.signedOutput;
                event.cached = false;
                SendToCaller<audio::events::FullWaveformReady>(waiter.owner, event);
            } else if (result.outcome == fb2k_waveform::JobOutcome::Aborted) {
                EmitWaveformCancelled(waiter);
            } else {
                FailureHook::LogAsync(audio::events::FullWaveformFailed::kName, result.code, result.error.c_str(),
                                      waiter.taskId.c_str());
                SendToCaller<audio::events::FullWaveformFailed>(waiter.owner,
                                                                MakeWaveformFailed(waiter, result.code, result.error));
            }
        }
        StartWaveformJobs(finished.start);
    }

    // 整轨波形的应答：请求回显的几项在 ready 与 pending 两种应答里都带
    audio::GenerateFullWaveformResult FullWaveformAnswer(const audio::GenerateFullWaveformParams& p, int resolution) {
        audio::GenerateFullWaveformResult result;
        result.resolution = resolution;
        result.method = p.method;
        result.scale = p.scale;
        result.signed_ = p.signed_;
        result.path = p.path;
        return result;
    }

    api::Result<audio::GenerateFullWaveformResult> AudioGenerateFullWaveform(
        const audio::GenerateFullWaveformParams& p, const CallerContext& caller) {
        try {
            const std::string& path = p.path;
            const int resolution = static_cast<int>(std::clamp<std::int64_t>(p.resolution, 64, 4096));
            const std::string& method = p.method;
            const std::string& scale = p.scale;
            const bool signedOutput = p.signed_;

            // 解析 subsong；cueIndex 优先级高于路径中的 subsong
            auto [filePath, pathSubsong] = ParseSubsongPath(path);
            const t_uint32 subsong = p.cueIndex ? static_cast<t_uint32>(*p.cueIndex) : pathSubsong;

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
                return api::Fail("Invalid path or file not found", ApiErrorCode::INVALID_PATH);
            }

            // 尝试从缓存获取：条目存原始值，按本次请求的 method / signed / scale 现算
            if (p.preferCache) {
                WaveformCacheEntry entry;
                if (WaveformCache::Get().TryGet(canonicalPath, subsong, resolution, fileSize, modifiedTime, entry)) {
                    // Lifecycle log: cache hit → ready (no pending state)
                    FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=ready cached=true path=", canonicalPath.c_str());
                    fb2k_waveform::Rendered rendered = RenderWaveform(entry.raw, method, scale, signedOutput);
                    audio::GenerateFullWaveformResult result = FullWaveformAnswer(p, entry.resolution);
                    result.status = "ready";
                    result.cached = true;
                    result.waveform = std::vector<double>(rendered.waveform.begin(), rendered.waveform.end());
                    result.maxAmplitude = rendered.maxAmplitude;
                    result.duration = entry.duration;
                    result.sampleRate = entry.sampleRate;
                    result.channels = entry.channels;
                    return result;
                }
            }

            // 缓存未命中：交给解码队列。解码键相同的在途任务直接并入，各请求仍拿自己的 taskId
            auto& jobs = GetWaveformJobs();
            if (jobs.shuttingDown) {
                return api::Fail("Host is shutting down", ApiErrorCode::OPERATION_FAILED);
            }
            std::string taskId = GenerateTaskId();

            // caller 是调用时的上下文：事件按它路由回调用者实例，取消时按它核对归属
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

            audio::GenerateFullWaveformResult result = FullWaveformAnswer(p, resolution);
            result.status = "pending";
            result.cached = false;
            result.taskId = taskId;
            return result;

        } catch (const std::exception& e) {
            FailureHook::LogSync("audio.generateFullWaveform",
                                 ApiErrorCode::OPERATION_FAILED, e.what());
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        } catch (...) {
            FailureHook::LogSync("audio.generateFullWaveform",
                                 ApiErrorCode::OPERATION_FAILED, "Unknown error");
            return api::Fail("Unknown error", ApiErrorCode::OPERATION_FAILED);
        }
    }

    // audio.cancelFullWaveform：只认发起请求的调用方。「已结束」「不存在」「不归你」都回
    // cancelled: false、故意不区分，taskId 形如 waveform_N、可以猜
    api::Result<audio::CancelFullWaveformResult> AudioCancelFullWaveform(const audio::CancelFullWaveformParams& p,
                                                                          const CallerContext& caller) {
        const std::string& taskId = p.taskId;
        const fb2k_waveform::CallerIdentity canceller{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                      caller.windowId};
        auto& jobs = GetWaveformJobs();
        const auto cancelled = jobs.queue.Cancel(taskId, canceller);
        audio::CancelFullWaveformResult result;
        if (!cancelled.cancelled) {
            result.cancelled = false;
            return result;
        }
        if (cancelled.verdict == fb2k_waveform::CancelVerdict::Unattributed) {
            FB2K_console_print("[TaskLifecycle] audio.cancelFullWaveform taskId=", taskId.c_str(),
                               " accepted without ownership check: the request carried no caller identity");
        }
        FB2K_console_print("[TaskLifecycle] audio.generateFullWaveform status=cancelled taskId=", taskId.c_str());

        EmitWaveformCancelled(cancelled.waiter);
        AbortWaveformJobs(cancelled.abort);
        ForgetWaveformJobs(cancelled.discarded);
        result.cancelled = true;
        return result;
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

    //=========================================================================
    // 离线 PCM 解码（audio.decodePcm，docs/audio-pcm/SPEC.md §4.2–§4.4）
    //
    // 任务骨架同整轨波形：队列只出决策，请求、取消、收尾、关窗与退出都在主线程，不加锁。
    // 不同之处：结果不进缓存；样本由 worker 直接写进任务启动时建好的共享缓冲；投递前
    // 逐个等待者复核它的页面是不是发起请求的那份文档，事件与缓冲只经那个页面的 host 发。
    //=========================================================================

    // 一次解码的输入。解码键相同的请求并入同一个任务，所以这里只放键决定的东西
    struct PcmJobSpec {
        std::string canonicalPath;
        t_uint32 subsong = 0;
        metadb_handle_ptr handle;
        double start = 0.0;
        std::optional<double> end;
        // 请求没给 sampleRate 时为 0，按解码器实际输出的采样率写
        std::uint32_t sampleRate = 0;
        bool mono = false;
        // 请求时按技术信息估算的缓冲字节数（D8），任务启动时照它建缓冲
        std::uint64_t bufferBytes = 0;
    };

    struct PcmDecodeResult {
        fb2k_waveform::JobOutcome outcome = fb2k_waveform::JobOutcome::Failed;
        const char* code = ApiErrorCode::UNKNOWN_ERROR;
        std::string error;
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        std::uint32_t frames = 0;
        bool truncated = false;
        bool resampled = false;
    };

    // 在 worker 线程里跑：解码、裁到区间、按需重采样与混单声道，样本直接写进 base 起的共享缓冲，
    // 写完样本再填头部（D8）。只碰自己的 spec、中止令牌与这 size 字节；缓冲的 COM 对象留在主线程（D6）
    PcmDecodeResult DecodePcm(const PcmJobSpec& spec, std::uint8_t* base, std::uint64_t size, abort_callback& abort) {
        PcmDecodeResult result;
        auto fail = [&result](const char* code, const char* error) {
            result.outcome = fb2k_waveform::JobOutcome::Failed;
            result.code = code;
            result.error = error;
            return result;
        };
        // seek 越过曲目末尾照样成功、下一次 run 才返回假（P12），所以一帧都没解出就是起点超长
        auto failWithoutFrames = [&spec, &fail]() {
            return spec.start > 0 ? fail(ApiErrorCode::INVALID_PARAMS, "start is at or beyond the end of the track")
                                  : fail(ApiErrorCode::DECODE_FAILED, "The track decoded to no samples");
        };
        try {
            service_ptr_t<input_decoder> decoder;
            input_entry::g_open_for_decoding(decoder, nullptr, spec.canonicalPath.c_str(), abort);
            if (!decoder.is_valid()) {
                return fail(ApiErrorCode::DECODER_FAILED, "Failed to open decoder");
            }
            decoder->initialize(spec.subsong, input_flag_no_looping, abort);

            // 能 seek 就直接跳到起点，不能就从头解、丢掉起点之前的帧
            const bool seeked = spec.start > 0 && decoder->can_seek();
            if (seeked) decoder->seek(spec.start, abort);

            audio_chunk_impl_temporary chunk;
            if (!decoder->run(chunk, abort)) return failWithoutFrames();

            // 格式以解码器实际输出的为准，可能与请求时读到的技术信息不同
            const unsigned sourceRate = chunk.get_srate();
            const unsigned sourceChannels = chunk.get_channels();
            const unsigned channelConfig = chunk.get_channel_config();
            const std::uint32_t outputRate = spec.sampleRate != 0 ? spec.sampleRate : sourceRate;

            service_ptr_t<dsp> resampler;
            if (outputRate != sourceRate && !resampler_entry::g_create(resampler, sourceRate, outputRate, 1.0f)) {
                return fail(ApiErrorCode::NOT_SUPPORTED, "No resampler supports this sample rate conversion");
            }

            // 容量按缓冲实际大小与实际声道数重算（D8）
            const auto capacity = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(fb2k_pcm::CapacityFramesFor(size, spec.mono ? 1u : sourceChannels), UINT32_MAX));
            fb2k_pcm::PlanarPacker<audio_sample> packer(reinterpret_cast<float*>(base + fb2k_pcm::kHeaderBytes),
                                                        capacity, sourceChannels, spec.mono);
            const auto toFrame = [sourceRate](double seconds) {
                return static_cast<std::uint64_t>(std::llround(seconds * sourceRate));
            };
            const std::uint64_t startFrame = toFrame(spec.start);
            fb2k_pcm::RangeClipper clipper(startFrame,
                                           spec.end ? std::optional<std::uint64_t>(toFrame(*spec.end)) : std::nullopt,
                                           seeked ? startFrame : 0);

            // 重采样器按 resampler.h 的约定输出目标采样率；输出别的格式就当不支持
            dsp_chunk_list_impl list;
            bool resamplerMismatch = false;
            auto runResampler = [&](int flags) {
                resampler->run_abortable(&list, spec.handle, flags, abort);
                bool more = true;
                for (t_size i = 0; more && i < list.get_count(); ++i) {
                    const audio_chunk* out = list.get_item(i);
                    if (out->get_srate() != outputRate || out->get_channels() != sourceChannels) {
                        resamplerMismatch = true;
                        more = false;
                    } else {
                        more = packer.Append(out->get_data(), out->get_sample_count());
                    }
                }
                list.remove_all();
                return more;
            };
            // 喂一段源帧；数据区写满或重采样器输出不对时返回假，调用方就此停下
            auto feed = [&](const audio_sample* frames, std::uint64_t count) {
                if (!resampler.is_valid()) return packer.Append(frames, count);
                list.add_item(static_cast<t_size>(count) * sourceChannels)
                    ->set_data(frames, static_cast<t_size>(count), sourceChannels, sourceRate, channelConfig);
                return runResampler(0);
            };

            bool formatChanged = false;
            bool more = true;
            do {
                // 链式 Ogg 一类中途换格式：在变化处停下，按截断交付（D8）
                if (chunk.get_srate() != sourceRate || chunk.get_channels() != sourceChannels) {
                    formatChanged = true;
                    break;
                }
                const fb2k_pcm::RangeClipper::Span span = clipper.Next(chunk.get_sample_count());
                if (span.take > 0) {
                    more = feed(chunk.get_data() + static_cast<t_size>(span.skip) * sourceChannels, span.take);
                }
                if (span.done) break;
            } while (more && decoder->run(chunk, abort));
            if (more && resampler.is_valid()) runResampler(dsp::END_OF_TRACK | dsp::FLUSH);

            if (resamplerMismatch) {
                return fail(ApiErrorCode::NOT_SUPPORTED, "The resampler did not convert to the requested sample rate");
            }
            if (packer.frames() == 0) return failWithoutFrames();

            const bool truncated = formatChanged || packer.truncated();
            fb2k_pcm::Header header;
            header.mode = fb2k_pcm::BufferMode::OneShot;
            header.sampleRate = outputRate;
            header.channels = packer.outputChannels();
            header.capacityFrames = capacity;
            header.seq = 2;
            header.writeFrames = packer.frames();
            // 一次性缓冲交出去以后宿主不再写，ended 恒置位
            header.flags = fb2k_pcm::kFlagEnded | (truncated ? fb2k_pcm::kFlagTruncated : 0u) |
                           (resampler.is_valid() ? fb2k_pcm::kFlagResampled : 0u);
            header.hostTimeMs = HostTimeUnixMs();
            header.startSeconds = spec.start;
            fb2k_pcm::EncodeHeader(header, base);

            result.outcome = fb2k_waveform::JobOutcome::Succeeded;
            result.sampleRate = outputRate;
            result.channels = header.channels;
            result.frames = header.writeFrames;
            result.truncated = truncated;
            result.resampled = resampler.is_valid();
            return result;
        } catch (const exception_aborted&) {
            // exception_aborted 也派生自 std::exception，必须排在它前面，否则取消会被当成解码失败
            result.outcome = fb2k_waveform::JobOutcome::Aborted;
            return result;
        } catch (const std::exception& e) {
            return fail(ApiErrorCode::DECODE_FAILED, e.what());
        } catch (...) {
            return fail(ApiErrorCode::UNKNOWN_ERROR, "Unknown error during PCM decoding");
        }
    }

    // 投递前复核一个等待者要用的：它的页面登记在哪个 hwnd，请求时是哪一份文档（D5）
    struct PcmWaiterDocument {
        HWND hwnd = nullptr;
        WebViewHost::DocumentStamp stamp;
    };

    struct PcmJob {
        PcmJobSpec spec;
        // 按 taskId 记等过这个任务的全部请求，取消或丢弃的也留着，任务结束时一起清掉
        std::unordered_map<std::string, PcmWaiterDocument> documents;
        std::shared_ptr<abort_callback_impl> token;
        // 任务启动时建。有 worker 在写时不能关，只在 worker 返回后的收尾里关（D6）
        std::unique_ptr<SharedPcmBuffer> buffer;
    };

    // 解码任务的全部状态，只在主线程访问。队列是单独的实例、并发 1（D7）
    struct PcmJobs {
        fb2k_waveform::WaveformTaskQueue queue{1};
        std::unordered_map<uint64_t, PcmJob> jobs;
        uint64_t nextTaskNumber = 0;
        bool shuttingDown = false;
    };

    PcmJobs& GetPcmJobs() {
        static PcmJobs state;
        return state;
    }

    WebViewHost* FindPcmPageHost(HWND hwnd) {
        return hwnd ? WebViewContext::GetInstance().GetWebViewHost(hwnd) : nullptr;
    }

    // 页面还是发起请求的那份文档就返回空，否则返回写进日志的原因（D5）
    const char* StalePcmDocumentReason(WebViewHost* host, const WebViewHost::DocumentStamp& stamp) {
        if (!host) return "host-gone";
        if (host->IsCurrentDocument(stamp)) return nullptr;
        const WebViewHost::DocumentStamp now = host->CaptureDocument();
        if (now.hostSerial != stamp.hostSerial) return "host-replaced";
        if (now.hostGeneration != stamp.hostGeneration) return "host-reset";
        if (now.navigationGeneration != stamp.navigationGeneration) return "navigation";
        return "host-closing";
    }

    struct PcmPage {
        WebViewHost* host = nullptr;
        BridgeCore* bridge = nullptr;
    };

    // 找到等待者当时的页面并复核文档戳；复核不过就记日志、返回空，事件与缓冲都不发
    std::optional<PcmPage> ResolvePcmPage(const PcmJob& job, const std::string& taskId) {
        const auto document = job.documents.find(taskId);
        if (document == job.documents.end()) return std::nullopt;
        PcmPage page{FindPcmPageHost(document->second.hwnd), WebViewContext::GetInstance().GetBridge(document->second.hwnd)};
        const char* stale = StalePcmDocumentReason(page.host, document->second.stamp);
        if (!stale && !page.bridge) stale = "bridge-gone";
        if (stale) {
            FB2K_console_print("[TaskLifecycle] audio.decodePcm taskId=", taskId.c_str(), " dropped=", stale);
            return std::nullopt;
        }
        return page;
    }

    // audio:pcmFailed 的唯一发射点，取消、解码失败、来源不再可信与投递失败共用；放在 handler
    // 之外，生成层才不会把事件字段当成取消端点的应答字段
    void EmitPcmFailed(BridgeCore& bridge, const fb2k_waveform::Waiter& waiter, const char* code,
                       const std::string& error) {
        audio::PcmFailedPayload payload;
        payload.taskId = waiter.taskId;
        payload.path = waiter.path;
        payload.error = error;
        payload.code = code;
        api::emit::Emit<audio::events::PcmFailed>(bridge, payload);
    }

    // 给一个等待者交付任务的终态（§4.3）：就绪时先投缓冲再发 audio:pcmReady
    void DeliverPcmResult(const PcmJob& job, const fb2k_waveform::Waiter& waiter, const PcmDecodeResult& result) {
        const std::optional<PcmPage> page = ResolvePcmPage(job, waiter.taskId);
        if (!page) return;
        if (result.outcome == fb2k_waveform::JobOutcome::Aborted) {
            EmitPcmFailed(*page->bridge, waiter, ApiErrorCode::CANCELLED, "Cancelled");
            return;
        }
        auto failWith = [&page, &waiter](const char* code, const std::string& error) {
            FailureHook::LogAsync(audio::events::PcmFailed::kName, code, error.c_str(), waiter.taskId.c_str());
            EmitPcmFailed(*page->bridge, waiter, code, error);
        };
        if (result.outcome == fb2k_waveform::JobOutcome::Failed) {
            failWith(result.code, result.error);
            return;
        }
        if (!page->host->IsCurrentDocumentTrusted()) {
            failWith(ApiErrorCode::ORIGIN_DENIED, "The page origin is no longer trusted");
            return;
        }

        const json additionalData = {
            {"purpose", "audio.decodePcm"},
            {"taskId", waiter.taskId},
            {"sampleRate", result.sampleRate},
            {"channels", result.channels},
            {"frames", result.frames},
            {"headerBytes", fb2k_pcm::kHeaderBytes}
        };
        const std::wstring additionalDataJson = pfc::stringcvt::string_wide_from_utf8(additionalData.dump().c_str()).get_ptr();
        const HRESULT hr = job.buffer ? job.buffer->PostReadOnly(page->host->GetWebView(), additionalDataJson) : RO_E_CLOSED;
        if (FAILED(hr)) {
            FB2K_console_print("[TaskLifecycle] audio.decodePcm taskId=", waiter.taskId.c_str(),
                               " post failed hr=0x", pfc::format_hex(static_cast<std::uint32_t>(hr), 8));
            failWith(ApiErrorCode::OPERATION_FAILED, "Buffer post failed");
            return;
        }

        const double duration = static_cast<double>(result.frames) / result.sampleRate;
        audio::PcmReadyPayload event;
        event.taskId = waiter.taskId;
        event.path = waiter.path;
        event.sampleRate = result.sampleRate;
        event.channels = result.channels;
        event.frames = result.frames;
        event.start = job.spec.start;
        event.end = job.spec.start + duration;
        event.duration = duration;
        event.truncated = result.truncated;
        event.resampled = result.resampled;
        api::emit::Emit<audio::events::PcmReady>(*page->bridge, event);
    }

    // 被取消的等待者恰好收一次 CANCELLED（§4.4），要在它的任务记录被清掉之前发
    void EmitPcmCancelled(const fb2k_waveform::Waiter& waiter) {
        for (const auto& [jobId, job] : GetPcmJobs().jobs) {
            if (!job.documents.contains(waiter.taskId)) continue;
            if (const std::optional<PcmPage> page = ResolvePcmPage(job, waiter.taskId)) {
                EmitPcmFailed(*page->bridge, waiter, ApiErrorCode::CANCELLED, "Cancelled");
            }
            return;
        }
    }

    void OnPcmJobFinished(uint64_t jobId, const PcmDecodeResult& result);

    // 全组件共用一个 WebView2 环境，取任何一个等待者页面上的都行
    ICoreWebView2Environment* FindPcmJobEnvironment(const PcmJob& job) {
        for (const auto& [taskId, document] : job.documents) {
            if (WebViewHost* host = FindPcmPageHost(document.hwnd); host && host->GetEnvironment()) {
                return host->GetEnvironment();
            }
        }
        return nullptr;
    }

    void StartPcmJobs(const std::vector<uint64_t>& jobIds) {
        auto& state = GetPcmJobs();
        for (uint64_t jobId : jobIds) {
            auto it = state.jobs.find(jobId);
            if (it == state.jobs.end()) continue;
            PcmJob& job = it->second;
            HRESULT hr = S_OK;
            job.buffer = SharedPcmBuffer::Create(FindPcmJobEnvironment(job), job.spec.bufferBytes, &hr);
            if (!job.buffer) {
                // 按任务失败收尾（D8）。经 inMainThread 走与 worker 返回相同的路径，免得在收尾里递归启动
                FB2K_console_print("[TaskLifecycle] audio.decodePcm status=failed job=", std::to_string(jobId).c_str(),
                                   " bytes=", std::to_string(job.spec.bufferBytes).c_str(),
                                   " reason=buffer-allocation hr=0x", pfc::format_hex(static_cast<std::uint32_t>(hr), 8));
                PcmDecodeResult failed;
                failed.code = ApiErrorCode::OPERATION_FAILED;
                failed.error = "Buffer allocation failed";
                fb2k::inMainThread([jobId, failed = std::move(failed)]() { OnPcmJobFinished(jobId, failed); });
                continue;
            }
            job.token = std::make_shared<abort_callback_impl>();
            FB2K_console_print("[TaskLifecycle] audio.decodePcm status=decoding job=", std::to_string(jobId).c_str(),
                               " bytes=", std::to_string(job.buffer->Size()).c_str(),
                               " path=", job.spec.canonicalPath.c_str());
            fb2k::inCpuWorkerThread([jobId, spec = job.spec, token = job.token, data = job.buffer->Data(),
                                     size = job.buffer->Size()]() {
                PcmDecodeResult result = DecodePcm(spec, data, size, *token);
                fb2k::inMainThread([jobId, result = std::move(result)]() { OnPcmJobFinished(jobId, result); });
            });
        }
    }

    void AbortPcmJobs(const std::vector<uint64_t>& jobIds) {
        auto& state = GetPcmJobs();
        for (uint64_t jobId : jobIds) {
            auto it = state.jobs.find(jobId);
            if (it != state.jobs.end() && it->second.token) it->second.token->abort();
        }
    }

    // 排队中被丢弃的任务不会再有 worker 回来收尾，记录由丢弃方清掉；它们还没建缓冲
    void ForgetPcmJobs(const std::vector<uint64_t>& jobIds) {
        auto& state = GetPcmJobs();
        for (uint64_t jobId : jobIds) state.jobs.erase(jobId);
    }

    // worker 返回后在主线程收尾：按队列的决策给每个等待者交付，关掉缓冲，再启动排队的任务
    void OnPcmJobFinished(uint64_t jobId, const PcmDecodeResult& result) {
        auto& state = GetPcmJobs();
        auto it = state.jobs.find(jobId);
        if (it == state.jobs.end()) return;
        // 摘下节点而不是移动记录：记录在本函数内一直有效，出作用域时连同缓冲一起释放
        auto node = state.jobs.extract(it);
        PcmJob& job = node.mapped();
        // 退出时等待者已全部丢弃、缓冲已 detach（D15），晚到的 worker 只释放自己的记录
        if (state.shuttingDown) return;

        const auto finished = state.queue.Finish(jobId, result.outcome);
        FB2K_console_print("[TaskLifecycle] audio.decodePcm status=finished job=", std::to_string(jobId).c_str(),
                           " outcome=", JobOutcomeName(result.outcome),
                           " waiters=", std::to_string(finished.notify.size()).c_str());
        // 等待者全部取消、而解码已越过最后一次中止检查时，notify 为空，结果随缓冲一起丢掉
        for (const fb2k_waveform::Waiter& waiter : finished.notify) {
            DeliverPcmResult(job, waiter, result);
        }
        // 先关掉这一块再启动下一个任务，宿主侧同时最多一块解码缓冲（D9）
        job.buffer.reset();
        StartPcmJobs(finished.start);
    }

    // PCM 各方法在请求时就能判定的失败：记一笔失败日志，details 原样放进失败信封
    api::Failure PcmFailure(const char* method, const char* code, const char* message, const json& details) {
        FailureHook::LogSync(method, code, message, !details.is_null());
        nlohmann::json::object_t extra;
        if (!details.is_null()) extra["details"] = details;
        return api::Fail(message, code, std::move(extra));
    }

    api::Result<audio::DecodePcmResult> AudioDecodePcm(const audio::DecodePcmParams& p, const CallerContext& caller) {
        auto reject = [](const char* code, const char* message, const json& details = nullptr) {
            return PcmFailure("audio.decodePcm", code, message, details);
        };
        try {
            auto& state = GetPcmJobs();
            if (state.shuttingDown) {
                return reject(ApiErrorCode::OPERATION_FAILED, "Host is shutting down");
            }

            // 生成的解析器只查了下限；NaN 与无穷在这里拦下
            const double start = p.start.value_or(0.0);
            if (!std::isfinite(start)) {
                return reject(ApiErrorCode::INVALID_PARAMS, "start must be a finite number", json{{"param", "start"}});
            }
            if (p.end && (!std::isfinite(*p.end) || *p.end <= start)) {
                return reject(ApiErrorCode::INVALID_PARAMS, "end must be a finite number greater than start",
                              json{{"param", "end"}, {"value", *p.end}});
            }

            WebViewHost* host = FindPcmPageHost(caller.callerHwnd);
            if (!host) {
                return reject(ApiErrorCode::NOT_SUPPORTED, "Cannot locate the calling page",
                              json("caller window unavailable"));
            }
            if (!SharedPcmBuffer::IsSupported(host->GetEnvironment(), host->GetWebView())) {
                return reject(ApiErrorCode::NOT_SUPPORTED, "This WebView2 runtime cannot share buffers with pages");
            }

            // 子曲目照 generateFullWaveform 解析，cueIndex 优先于路径里的 subsong（D7）
            auto [filePath, pathSubsong] = ParseSubsongPath(p.path);
            const t_uint32 subsong = p.cueIndex ? static_cast<t_uint32>(*p.cueIndex) : pathSubsong;
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(filePath, subsong);
            if (!handle.is_valid()) {
                return reject(ApiErrorCode::INVALID_HANDLE, "Failed to create metadb handle");
            }
            const std::string canonicalPath = handle->get_path();

            // 文件大小与修改时间进解码键，文件改过就不会并入旧任务。远程路径不查，免得在主线程上等网络
            uint64_t fileSize = 0;
            uint64_t modifiedTime = 0;
            if (!filesystem::g_is_remote_or_unrecognized(canonicalPath.c_str())) {
                try {
                    abort_callback_impl abort;
                    t_filestats stats;
                    bool isWriteable = false;
                    filesystem::g_get_stats(canonicalPath.c_str(), stats, isWriteable, abort);
                    fileSize = stats.m_size;
                    modifiedTime = stats.m_timestamp;
                } catch (const std::exception&) {
                    return reject(ApiErrorCode::INVALID_PATH, "Invalid path or file not found");
                }
            }

            // 技术信息在请求时取（D8）：远程或未识别的路径只用缓存，读不了文件抛异常
            AudioTechnicalInfo info;
            bool hasInfo = false;
            try {
                const metadb_info_container::ptr infoRef = SubsongUtils::GetInfoOrReadFile(handle);
                hasInfo = infoRef.is_valid() && TryExtractAudioTechnicalInfo(infoRef->info(), info);
            } catch (const std::exception&) {
                hasInfo = false;
            }
            if (!hasInfo) {
                return reject(ApiErrorCode::NO_INFO, "Failed to get audio info");
            }

            const bool mono = p.mono.value_or(false);
            const auto rate = static_cast<std::uint32_t>(p.sampleRate.value_or(info.sampleRate));
            const auto channels = mono ? 1u : static_cast<std::uint32_t>(info.channels);
            const double end = p.end ? std::min(*p.end, info.duration) : info.duration;
            const double estimatedBytes = fb2k_pcm::OneShotBufferBytes(start, end, rate, channels);
            if (!fb2k_pcm::WithinOneShotLimit(estimatedBytes)) {
                return reject(ApiErrorCode::INVALID_PARAMS, "The decoded range would exceed the buffer size limit",
                              json{{"estimatedBytes", estimatedBytes},
                                   {"limitBytes", fb2k_pcm::kOneShotLimitBytes},
                                   {"suggestion", "Lower sampleRate, set mono, or decode a shorter range"}});
            }

            const std::string taskId = "pcm_" + std::to_string(state.nextTaskNumber++);
            fb2k_waveform::Waiter waiter;
            waiter.taskId = taskId;
            waiter.path = p.path;
            waiter.owner.hwnd = reinterpret_cast<std::uintptr_t>(caller.callerHwnd);
            waiter.owner.windowId = caller.windowId;

            const std::string decodeKey =
                json::array({canonicalPath, subsong, fileSize, modifiedTime, start, p.end ? json(*p.end) : json(),
                             p.sampleRate.value_or(0), mono})
                    .dump();
            const auto submitted = state.queue.Submit(decodeKey, std::move(waiter));
            PcmJob& job = state.jobs[submitted.jobId];
            if (!submitted.merged) {
                job.spec = PcmJobSpec{canonicalPath,
                                      subsong,
                                      handle,
                                      start,
                                      p.end,
                                      static_cast<std::uint32_t>(p.sampleRate.value_or(0)),
                                      mono,
                                      static_cast<std::uint64_t>(estimatedBytes)};
            }
            job.documents[taskId] = PcmWaiterDocument{caller.callerHwnd, host->CaptureDocument()};

            FB2K_console_print("[TaskLifecycle] audio.decodePcm status=pending taskId=", taskId.c_str(),
                               " job=", std::to_string(submitted.jobId).c_str(),
                               submitted.merged ? " merged=true" : " merged=false", " path=", canonicalPath.c_str());
            StartPcmJobs(submitted.start);
            audio::DecodePcmResult result;
            result.taskId = taskId;
            result.status = "pending";
            return result;
        } catch (const std::exception& e) {
            return reject(ApiErrorCode::OPERATION_FAILED, e.what());
        }
    }

    // 只认发起请求的调用方；「已结束」「不存在」「不归你」都回 cancelled: false、故意不区分（§4.4）
    api::Result<audio::CancelDecodePcmResult> AudioCancelDecodePcm(const audio::CancelDecodePcmParams& p,
                                                                    const CallerContext& caller) {
        const fb2k_waveform::CallerIdentity canceller{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                      caller.windowId};
        auto& state = GetPcmJobs();
        const auto cancelled = state.queue.Cancel(p.taskId, canceller);
        audio::CancelDecodePcmResult result;
        if (!cancelled.cancelled) {
            result.cancelled = false;
            return result;
        }
        if (cancelled.verdict == fb2k_waveform::CancelVerdict::Unattributed) {
            FB2K_console_print("[TaskLifecycle] audio.cancelDecodePcm taskId=", p.taskId.c_str(),
                               " accepted without ownership check: the request carried no caller identity");
        }
        FB2K_console_print("[TaskLifecycle] audio.decodePcm status=cancelled taskId=", p.taskId.c_str());

        EmitPcmCancelled(cancelled.waiter);
        AbortPcmJobs(cancelled.abort);
        ForgetPcmJobs(cancelled.discarded);
        result.cancelled = true;
        return result;
    }

    //=========================================================================
    // 实时 PCM 流（audio.subscribeStream / unsubscribeStream，docs/audio-pcm/SPEC.md §4.5–§4.6）
    //
    // 全组件只有一个 playback_stream_capture 回调对象：首个订阅登记时注册，最后一个移除时注销，
    // 各订阅请求的间隔取最小值。每个订阅一块环形共享缓冲，在第一块音频到达时按实际格式建，
    // 采样率或声道数变了就换一代。回调与表都只在主线程上用，不加锁；回调里只结束订阅、
    // 换代与写样本，注册变更延到派发之后（D10）。
    //=========================================================================

    struct PcmStreamSubscription {
        std::string id;
        fb2k_waveform::CallerIdentity owner;
        HWND hwnd = nullptr;
        WebViewHost::DocumentStamp stamp;
        // 向核心请求的回调间隔（秒）；没给就交给核心缺省
        std::optional<double> interval;
        double bufferSeconds = 1.0;
        // 第一块到达时才建，格式变化时换成新的一块；宿主关掉旧的之前先置 ended
        std::unique_ptr<SharedPcmBuffer> buffer;
        // 0 表示还没有缓冲；第一块缓冲是第 1 代，同 id 替换后接着数
        std::uint32_t epoch = 0;
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        std::uint32_t capacityFrames = 0;
    };

    class PcmStreamCapture : public playback_stream_capture_callback {
    public:
        void on_chunk(const audio_chunk& chunk) override;
    };

    struct PcmStreams {
        // 从基类派生、手动注册注销（照 foo_sample）：SDK 的 playback_stream_capture_callback_impl
        // 在构造与析构里注册注销并断言主线程，做成静态对象会在 DLL 加载、卸载时执行
        PcmStreamCapture capture;
        std::vector<std::unique_ptr<PcmStreamSubscription>> subscriptions;
        bool callbackRegistered = false;
        // 当前向核心请求的间隔；nullopt 表示交给核心缺省
        std::optional<double> registeredInterval;
        bool syncScheduled = false;
        std::uint64_t nextId = 0;
        std::uint64_t chunkCount = 0;
        std::uint64_t chunkCycles = 0;
        bool shuttingDown = false;
    };

    PcmStreams& GetPcmStreams() {
        static PcmStreams state;
        return state;
    }

    constexpr size_t kMaxStreamSubscriptionsPerCaller = 8;

    bool SameCaller(const fb2k_waveform::CallerIdentity& a, const fb2k_waveform::CallerIdentity& b) {
        return a.hwnd == b.hwnd && a.windowId == b.windowId;
    }

    // 先置 ended 再关：读端靠 ended 位停止轮询，页面已经拿到的视图不受 Close() 影响
    void ClosePcmStreamBuffer(PcmStreamSubscription& sub) {
        if (!sub.buffer) return;
        if (sub.buffer->Data()) fb2k_pcm::MarkRingEnded(sub.buffer->Data());
        sub.buffer.reset();
    }

    // 让核心那边的注册跟着订阅表走：有订阅就注册，间隔取各订阅请求的最小值（都没给就交给核心
    // 缺省）；最小值变了先注销再注册；没有订阅就注销。只能在主线程、且不在 on_chunk 派发中调用
    void SyncPcmStreamRegistration() {
        auto& state = GetPcmStreams();
        state.syncScheduled = false;
        if (state.shuttingDown) return;
        std::optional<double> wanted;
        for (const auto& sub : state.subscriptions) {
            if (sub->interval) wanted = std::min(wanted.value_or(*sub->interval), *sub->interval);
        }
        auto api = playback_stream_capture_v2::get();
        if (state.subscriptions.empty()) {
            if (state.callbackRegistered) {
                api->remove_callback(&state.capture);
                state.callbackRegistered = false;
                state.registeredInterval.reset();
                FB2K_console_print("[StreamLifecycle] capture callback removed");
            }
            return;
        }
        if (state.callbackRegistered && state.registeredInterval == wanted) return;
        if (state.callbackRegistered) api->remove_callback(&state.capture);
        api->add_callback_v2(&state.capture, wanted.value_or(-1.0));
        state.callbackRegistered = true;
        state.registeredInterval = wanted;
        FB2K_console_print("[StreamLifecycle] capture callback registered interval=",
                           wanted ? std::to_string(*wanted).c_str() : "default");
    }

    // on_chunk 里结束了订阅时用：注销自己不能在派发中做，SDK 没说那样安全
    void SchedulePcmStreamRegistrationSync() {
        auto& state = GetPcmStreams();
        if (state.syncScheduled) return;
        state.syncScheduled = true;
        fb2k::inMainThread([]() { SyncPcmStreamRegistration(); });
    }

    // 结束一个订阅并从表里摘掉。不发事件：§4.5 只在换代时发，其余结束原因调用方自己知道或页面已不在
    void EndPcmStream(size_t index, const char* reason) {
        auto& state = GetPcmStreams();
        PcmStreamSubscription& sub = *state.subscriptions[index];
        FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", sub.id.c_str(), " ended reason=", reason,
                           " epoch=", std::to_string(sub.epoch).c_str());
        ClosePcmStreamBuffer(sub);
        state.subscriptions.erase(state.subscriptions.begin() + static_cast<std::ptrdiff_t>(index));
    }

    // 按这一块的格式建新一代缓冲并投递给订阅的页面；有旧的一代就关掉它并发 audio:stream 换代事件。
    // 成功返回空，失败返回写进日志的原因，订阅由调用方结束
    const char* RotatePcmStreamEpoch(PcmStreamSubscription& sub, WebViewHost& host, std::uint32_t sampleRate,
                                     std::uint32_t channels, double hostTimeMs) {
        if (!host.IsCurrentDocumentTrusted()) return "origin-denied";
        const std::uint32_t capacity = fb2k_pcm::RingCapacityFrames(sub.bufferSeconds, sampleRate, channels);
        HRESULT hr = S_OK;
        std::unique_ptr<SharedPcmBuffer> next =
            SharedPcmBuffer::Create(host.GetEnvironment(), fb2k_pcm::BufferBytes(capacity, channels), &hr);
        if (!next) {
            FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", sub.id.c_str(),
                               " buffer allocation failed hr=0x", pfc::format_hex(static_cast<std::uint32_t>(hr), 8));
            return "buffer-allocation";
        }
        const std::uint32_t epoch = sub.epoch + 1;
        fb2k_pcm::Header header;
        header.mode = fb2k_pcm::BufferMode::Ring;
        header.sampleRate = sampleRate;
        header.channels = channels;
        header.capacityFrames = capacity;
        header.hostTimeMs = hostTimeMs;
        header.epoch = epoch;
        fb2k_pcm::EncodeHeader(header, next->Data());

        const json additionalData = {
            {"purpose", "audio.subscribeStream"},
            {"subscriptionId", sub.id},
            {"epoch", epoch},
            {"sampleRate", sampleRate},
            {"channels", channels},
            {"capacityFrames", capacity},
            {"headerBytes", fb2k_pcm::kHeaderBytes}
        };
        const std::wstring additionalDataJson =
            pfc::stringcvt::string_wide_from_utf8(additionalData.dump().c_str()).get_ptr();
        hr = next->PostReadOnly(host.GetWebView(), additionalDataJson);
        if (FAILED(hr)) {
            FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", sub.id.c_str(),
                               " post failed hr=0x", pfc::format_hex(static_cast<std::uint32_t>(hr), 8));
            return "post-failed";
        }

        const std::uint32_t endedEpoch = sub.epoch;
        ClosePcmStreamBuffer(sub);
        sub.buffer = std::move(next);
        sub.epoch = epoch;
        sub.sampleRate = sampleRate;
        sub.channels = channels;
        sub.capacityFrames = capacity;
        FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", sub.id.c_str(),
                           " epoch=", std::to_string(epoch).c_str(), " format=", std::to_string(sampleRate).c_str(),
                           "/", std::to_string(channels).c_str(), " capacityFrames=", std::to_string(capacity).c_str());
        if (endedEpoch > 0) {
            // 换代不发也能靠新缓冲头知道，但 SDK 要靠它释放旧视图（§4.5）
            if (BridgeCore* bridge = WebViewContext::GetInstance().GetBridge(sub.hwnd)) {
                audio::StreamPayload payload;
                payload.subscriptionId = sub.id;
                payload.type = "ended";
                payload.epoch = endedEpoch;
                payload.reason = "format-change";
                api::emit::Emit<audio::events::Stream>(*bridge, payload);
            }
        }
        return nullptr;
    }

    // 主线程回调。每块：页面不在或文档变了的订阅结束掉；格式与当前缓冲不同就换代；
    // 再把交错样本转成 f32 写进环形区。不分配、不发 JSON（换代除外），注册变更留到派发之后
    void PcmStreamCapture::on_chunk(const audio_chunk& chunk) {
        auto& state = GetPcmStreams();
        if (state.shuttingDown) return;
        ULONG64 cyclesBefore = 0;
        QueryThreadCycleTime(GetCurrentThread(), &cyclesBefore);
        ++state.chunkCount;
        const auto frames = static_cast<std::uint32_t>(chunk.get_sample_count());
        const std::uint32_t sampleRate = chunk.get_srate();
        const std::uint32_t channels = chunk.get_channels();
        const double hostTimeMs = HostTimeUnixMs();
        bool endedAny = false;
        if (frames > 0 && sampleRate > 0 && channels > 0) {
            for (size_t i = 0; i < state.subscriptions.size();) {
                PcmStreamSubscription& sub = *state.subscriptions[i];
                WebViewHost* host = FindPcmPageHost(sub.hwnd);
                if (const char* stale = StalePcmDocumentReason(host, sub.stamp)) {
                    EndPcmStream(i, stale);
                    endedAny = true;
                    continue;
                }
                if (!sub.buffer || sub.sampleRate != sampleRate || sub.channels != channels) {
                    if (const char* failed = RotatePcmStreamEpoch(sub, *host, sampleRate, channels, hostTimeMs)) {
                        EndPcmStream(i, failed);
                        endedAny = true;
                        continue;
                    }
                }
                fb2k_pcm::WriteRingChunk<audio_sample>(sub.buffer->Data(), chunk.get_data(), frames, channels,
                                                       hostTimeMs);
                ++i;
            }
        }
        if (endedAny) SchedulePcmStreamRegistrationSync();
        ULONG64 cyclesAfter = 0;
        QueryThreadCycleTime(GetCurrentThread(), &cyclesAfter);
        state.chunkCycles += cyclesAfter - cyclesBefore;
    }

    api::Result<audio::SubscribeStreamResult> AudioSubscribeStream(const audio::SubscribeStreamParams& p,
                                                                    const CallerContext& caller) {
        auto reject = [](const char* code, const char* message, const json& details = nullptr) {
            return PcmFailure("audio.subscribeStream", code, message, details);
        };
        auto& state = GetPcmStreams();
        if (state.shuttingDown) {
            return reject(ApiErrorCode::OPERATION_FAILED, "Host is shutting down");
        }
        // 生成的解析器只查范围；NaN 与无穷在这里拦下
        if (p.interval && !std::isfinite(*p.interval)) {
            return reject(ApiErrorCode::INVALID_PARAMS, "interval must be a finite number", json{{"param", "interval"}});
        }
        const double bufferSeconds = p.bufferSeconds;
        if (!std::isfinite(bufferSeconds)) {
            return reject(ApiErrorCode::INVALID_PARAMS, "bufferSeconds must be a finite number",
                          json{{"param", "bufferSeconds"}});
        }
        WebViewHost* host = FindPcmPageHost(caller.callerHwnd);
        if (!host) {
            return reject(ApiErrorCode::NOT_SUPPORTED, "Cannot locate the calling page",
                          json("caller window unavailable"));
        }
        if (!SharedPcmBuffer::IsSupported(host->GetEnvironment(), host->GetWebView())) {
            return reject(ApiErrorCode::NOT_SUPPORTED, "This WebView2 runtime cannot share buffers with pages");
        }

        const fb2k_waveform::CallerIdentity owner{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                  caller.windowId};
        std::string id = p.subscriptionId.value_or("");
        PcmStreamSubscription* existing = nullptr;
        size_t owned = 0;
        for (const auto& sub : state.subscriptions) {
            if (!SameCaller(sub->owner, owner)) continue;
            ++owned;
            if (!id.empty() && sub->id == id) existing = sub.get();
        }
        if (id.empty()) id = "pcmstream_" + std::to_string(state.nextId++);

        if (existing) {
            // 同一调用方用同一 id 再订阅：替换。旧缓冲置 ended 后关掉，不发事件（§4.5）
            ClosePcmStreamBuffer(*existing);
            existing->sampleRate = 0;
            existing->channels = 0;
            existing->capacityFrames = 0;
            existing->stamp = host->CaptureDocument();
            existing->interval = p.interval;
            existing->bufferSeconds = bufferSeconds;
            FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", id.c_str(), " replaced");
        } else {
            if (owned >= kMaxStreamSubscriptionsPerCaller) {
                return reject(ApiErrorCode::OPERATION_FAILED, "This page already has 8 stream subscriptions",
                              json("too many stream subscriptions"));
            }
            auto sub = std::make_unique<PcmStreamSubscription>();
            sub->id = id;
            sub->owner = owner;
            sub->hwnd = caller.callerHwnd;
            sub->stamp = host->CaptureDocument();
            sub->interval = p.interval;
            sub->bufferSeconds = bufferSeconds;
            state.subscriptions.push_back(std::move(sub));
            FB2K_console_print("[StreamLifecycle] audio.subscribeStream id=", id.c_str(),
                               " registered windowId=", caller.windowId.c_str(),
                               " bufferSeconds=", std::to_string(bufferSeconds).c_str());
        }
        SyncPcmStreamRegistration();

        audio::SubscribeStreamResult result;
        result.subscriptionId = id;
        result.interval = p.interval;
        result.bufferSeconds = bufferSeconds;
        return result;
    }

    // 只移除归本调用方的订阅（归属规则同 §4.4）；不发事件
    api::Result<audio::UnsubscribeStreamResult> AudioUnsubscribeStream(const audio::UnsubscribeStreamParams& p,
                                                                        const CallerContext& caller) {
        auto& state = GetPcmStreams();
        const fb2k_waveform::CallerIdentity canceller{reinterpret_cast<std::uintptr_t>(caller.callerHwnd),
                                                      caller.windowId};
        size_t removed = 0;
        for (size_t i = 0; i < state.subscriptions.size();) {
            const PcmStreamSubscription& sub = *state.subscriptions[i];
            const bool idMatches = !p.subscriptionId || sub.id == *p.subscriptionId;
            if (!idMatches ||
                fb2k_waveform::JudgeCancel(sub.owner, canceller) == fb2k_waveform::CancelVerdict::Denied) {
                ++i;
                continue;
            }
            EndPcmStream(i, "unsubscribe");
            ++removed;
        }
        if (removed > 0) SyncPcmStreamRegistration();
        audio::UnsubscribeStreamResult result;
        result.removed = static_cast<std::int64_t>(removed);
        return result;
    }

    // 弹窗关闭：该窗口的订阅直接结束，不发事件。可能从任何主线程调用点进来，注册变更延后做
    void EndPcmStreamsForWindow(const std::string& windowId) {
        auto& state = GetPcmStreams();
        bool endedAny = false;
        for (size_t i = 0; i < state.subscriptions.size();) {
            if (state.subscriptions[i]->owner.windowId != windowId) {
                ++i;
                continue;
            }
            EndPcmStream(i, "window-closed");
            endedAny = true;
        }
        if (endedAny) SchedulePcmStreamRegistrationSync();
    }

    // 退出：结束全部订阅（置 ended、Close()）并注销回调。没有 worker 写环形缓冲，可以当场关（D15）
    void ShutdownPcmStreams() {
        auto& state = GetPcmStreams();
        state.shuttingDown = true;
        for (auto& sub : state.subscriptions) ClosePcmStreamBuffer(*sub);
        state.subscriptions.clear();
        if (state.callbackRegistered) {
            playback_stream_capture_v2::get()->remove_callback(&state.capture);
            state.callbackRegistered = false;
        }
    }

    api::Result<audio::GetPcmDebugStateResult> AudioGetPcmDebugState(const audio::GetPcmDebugStateParams& /*params*/,
                                                                      const CallerContext& caller) {
        std::string version;
        wil::unique_cotaskmem_string rawVersion;
        if (SUCCEEDED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &rawVersion)) && rawVersion) {
            version = pfc::stringcvt::string_utf8_from_wide(rawVersion.get()).get_ptr();
        }
        bool environment12 = false;
        bool webview17 = false;
        if (WebViewHost* host = FindPcmPageHost(caller.callerHwnd)) {
            wil::com_ptr<ICoreWebView2Environment12> environment;
            wil::com_ptr<ICoreWebView2_17> webview;
            environment12 = host->GetEnvironment() &&
                            SUCCEEDED(host->GetEnvironment()->QueryInterface(IID_PPV_ARGS(&environment)));
            webview17 = host->GetWebView() && SUCCEEDED(host->GetWebView()->QueryInterface(IID_PPV_ARGS(&webview)));
        }

        const auto& state = GetPcmJobs();
        uint64_t openBufferBytes = 0;
        for (const auto& [jobId, job] : state.jobs) {
            if (job.buffer && job.buffer->Data()) openBufferBytes += job.buffer->Size();
        }

        const auto& streams = GetPcmStreams();
        audio::GetPcmDebugStateResult result;
        for (const auto& sub : streams.subscriptions) {
            // 写指针只有主线程改，这里读头部不需要 seqlock
            const std::uint32_t writeFrames = sub->buffer && sub->buffer->Data()
                ? fb2k_pcm::LoadU32(sub->buffer->Data(), fb2k_pcm::offsets::kWriteFrames)
                : 0;
            audio::PcmStreamDebugEntry entry;
            entry.subscriptionId = sub->id;
            entry.windowId = sub->owner.windowId;
            entry.epoch = sub->epoch;
            entry.capacityFrames = sub->capacityFrames;
            entry.writeFrames = writeFrames;
            result.stream.subscriptions.push_back(std::move(entry));
        }
        result.stream.callbackRegistered = streams.callbackRegistered;
        result.stream.interval = streams.registeredInterval;
        result.stream.chunkCount = static_cast<std::int64_t>(streams.chunkCount);
        result.stream.chunkCycles = static_cast<std::int64_t>(streams.chunkCycles);
        result.runtime.version = version;
        result.runtime.environment12 = environment12;
        result.runtime.webview17 = webview17;
        result.decode.active = static_cast<std::int64_t>(state.queue.ActiveCount());
        result.decode.queued = static_cast<std::int64_t>(state.queue.QueuedCount());
        result.decode.openBufferBytes = static_cast<std::int64_t>(openBufferBytes);
        return result;
    }

    // 退出时结束全部实时订阅并注销回调，再丢弃全部解码任务，做法同 WaveformShutdownInitQuit。
    // 还有记录的解码任务都已启动、收尾还没轮到：worker 可能还在写，缓冲不能 Close()，detach 后
    // 随进程丢掉。这样 DLL 卸载时静态表里不剩 COM 引用（D15）
    class PcmShutdownInitQuit : public initquit {
    public:
        void on_quit() override {
            // 从 on_quit 抛出会打断宿主的关停序列，自己兜住
            try {
                ShutdownPcmStreams();
            } catch (...) {
            }
            try {
                auto& state = GetPcmJobs();
                state.shuttingDown = true;
                const auto dropped = state.queue.DropAll();
                AbortPcmJobs(dropped.abort);
                ForgetPcmJobs(dropped.discarded);
                size_t detached = 0;
                for (auto& [jobId, job] : state.jobs) {
                    if (!job.buffer) continue;
                    job.buffer->Detach();
                    ++detached;
                }
                if (dropped.removedWaiters > 0 || detached > 0) {
                    console::printf("audio.decodePcm: cancelled %u waiter(s) and detached %u buffer(s) on quit",
                                    static_cast<unsigned>(dropped.removedWaiters), static_cast<unsigned>(detached));
                }
            } catch (...) {
            }
        }
    };

    static initquit_factory_t<PcmShutdownInitQuit> g_pcm_shutdown_initquit;

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
// 取消指定窗口发起的 decodePcm 请求并结束它的流订阅（popup 关闭时调用）
//
// 同整轨波形：只移除该窗口的等待者、不发事件，任务没人等了才中止。解码中的缓冲
// 留给 worker 返回后的收尾去关；流订阅的缓冲当场置 ended 并关掉（docs/audio-pcm/SPEC.md D6、D15）。
//=============================================================================
void CancelAllPcmForWindow(const std::string& windowId) {
    try {
        EndPcmStreamsForWindow(windowId);
    } catch (...) {
    }
    try {
        auto& state = GetPcmJobs();
        const auto dropped = state.queue.DropWindow(windowId);
        AbortPcmJobs(dropped.abort);
        ForgetPcmJobs(dropped.discarded);
        if (dropped.removedWaiters > 0) {
            console::printf("audio.decodePcm: dropped %u waiter(s) for a closing window",
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
    // Spectrum APIs
    api::RegisterApi("audio.subscribeSpectrum", AudioSubscribeSpectrum);
    api::RegisterApi("audio.unsubscribeSpectrum", AudioUnsubscribeSpectrum);
    api::RegisterApi("audio.getSpectrum", AudioGetSpectrum);
    api::RegisterApi("audio.getSpectrumDebugState", AudioGetSpectrumDebugState);
    api::RegisterApi("audio.getWaveform", AudioGetWaveform);
    api::RegisterApi("audio.setChannelMode", AudioSetChannelMode);

    // Analysis APIs
    api::RegisterApi("audio.analyzeBPM", AudioAnalyzeBPM);
    api::RegisterApi("audio.generateFullWaveform", AudioGenerateFullWaveform);
    api::RegisterApi("audio.cancelFullWaveform", AudioCancelFullWaveform);

    // Info APIs
    api::RegisterApi("audio.getOutputInfo", AudioGetOutputInfo);
    api::RegisterApi("audio.getStreamInfo", AudioGetStreamInfo);
    api::RegisterApi("audio.isVisualizationAvailable", AudioIsVisualizationAvailable);

    // PCM primitives (docs/audio-pcm/SPEC.md)
    api::RegisterApi("audio.decodePcm", AudioDecodePcm);
    api::RegisterApi("audio.cancelDecodePcm", AudioCancelDecodePcm);
    api::RegisterApi("audio.subscribeStream", AudioSubscribeStream);
    api::RegisterApi("audio.unsubscribeStream", AudioUnsubscribeStream);
    api::RegisterApi("audio.getPcmDebugState", AudioGetPcmDebugState);

    LOG("Audio API registered (17 APIs)");
}
