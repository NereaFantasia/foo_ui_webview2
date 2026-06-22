#pragma once

#include "BridgeCore.h"

//=============================================================================
// Register all Audio APIs
// 
// APIs:
// - audio.subscribeSpectrum    订阅实时频谱数据
// - audio.unsubscribeSpectrum  取消订阅频谱
// - audio.getSpectrum          获取当前频谱数据
// - audio.getWaveform          获取当前波形数据片段
// - audio.setChannelMode       设置声道模式
// - audio.subscribeStream      订阅音频流捕获
// - audio.unsubscribeStream    取消订阅音频流
// - audio.analyzeBPM           分析曲目 BPM
// - audio.generateWaveform     生成文件波形数据
// - audio.getOutputInfo        获取音频输出信息
// - audio.getStreamInfo        获取当前播放流信息
// - audio.isVisualizationAvailable 检查可视化是否可用
// - audio.getSpectrumDebugState    获取频谱调试状态
// - audio.generateFullWaveform     生成完整文件波形数据
//=============================================================================
/** @brief Register the audio.* API handlers (spectrum / waveform). */
void RegisterAudioApi();

/** @brief Release the spectrum-visualization runtime (timer / stream / subscriptions).
 *  Called from WebViewUI::shutdown() and background_service::Shutdown(). */
void ShutdownAudioVisualizationRuntime();
