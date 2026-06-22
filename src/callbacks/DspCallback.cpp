#include "pch.h"
#include "callbacks/DspCallback.h"
#include "core/WebViewContext.h"

// ============================================
// DSP 配置回调实现
// 当 DSP 预设变化时广播 audio:dspPresetChanged 事件
// ============================================

class DspCallbackImpl : public dsp_config_callback {
public:
    void on_core_settings_change(const dsp_chain_config& cfg) override {
        try {
            LOG("DspCallback: DSP preset changed");
            
            WebViewContext::GetInstance().BroadcastEvent("audio:dspPresetChanged", {});
        } catch (...) {}
    }
};

static service_factory_single_t<DspCallbackImpl> g_dsp_callback_factory;
