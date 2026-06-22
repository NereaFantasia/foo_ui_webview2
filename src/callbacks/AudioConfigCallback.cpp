#include "pch.h"
#include "callbacks/AudioConfigCallback.h"
#include "core/WebViewContext.h"

// ============================================
// 音频配置回调实现
// 监听输出设备和 ReplayGain 设置变化
// ============================================

// Output device change callback
class OutputChangeCallback : public output_config_change_callback {
public:
    void outputConfigChanged() override {
        try {
            LOG("AudioConfig: Output device changed");
            WebViewContext::GetInstance().BroadcastEvent("audio:outputDeviceChanged", json::object());
        } catch (...) {}
    }
};

// ReplayGain settings change callback
class ReplayGainChangeCallback : public replaygain_core_settings_notify {
public:
    void on_changed(t_replaygain_config const& cfg) override {
        try {
            LOG("AudioConfig: ReplayGain mode changed");
            WebViewContext::GetInstance().BroadcastEvent("audio:replaygainModeChanged", {
                {"mode", static_cast<int>(cfg.m_source_mode)}
            });
        } catch (...) {}
    }
};

// Singleton to manage dynamic callbacks
class AudioConfigCallbackManager {
public:
    static AudioConfigCallbackManager& GetInstance() {
        static AudioConfigCallbackManager instance;
        return instance;
    }
    
    void Initialize() {
        // Register output callback
        auto outputMgr = output_manager_v2::tryGet();
        if (outputMgr.is_valid()) {
            outputMgr->addCallback(&outputCallback_);
            LOG("AudioConfig: Output callback registered");
        }
        
        // Register replaygain callback
        auto rgMgr = replaygain_manager_v2::tryGet();
        if (rgMgr.is_valid()) {
            rgMgr->add_notify(&replaygainCallback_);
            LOG("AudioConfig: ReplayGain callback registered");
        }
    }
    
    void Shutdown() {
        auto outputMgr = output_manager_v2::tryGet();
        if (outputMgr.is_valid()) {
            outputMgr->removeCallback(&outputCallback_);
        }
        
        auto rgMgr = replaygain_manager_v2::tryGet();
        if (rgMgr.is_valid()) {
            rgMgr->remove_notify(&replaygainCallback_);
        }
    }
    
private:
    OutputChangeCallback outputCallback_;
    ReplayGainChangeCallback replaygainCallback_;
};

// initquit service to register callbacks
class AudioConfigInitQuit : public initquit {
public:
    void on_init() override {
        AudioConfigCallbackManager::GetInstance().Initialize();
    }
    
    void on_quit() override {
        AudioConfigCallbackManager::GetInstance().Shutdown();
    }
};

static initquit_factory_t<AudioConfigInitQuit> g_audio_config_initquit;
