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
// - audio.subscribeStream      订阅正在播放的音频，样本经共享环形缓冲交给页面
// - audio.unsubscribeStream    移除本页面的流订阅
// - audio.analyzeBPM           读取曲目的 BPM 标签
// - audio.getOutputInfo        获取音频输出信息
// - audio.getStreamInfo        获取当前播放流信息
// - audio.isVisualizationAvailable 检查可视化是否可用
// - audio.getSpectrumDebugState    获取频谱调试状态
// - audio.generateFullWaveform     生成完整文件波形数据
// - audio.cancelFullWaveform       取消整轨波形请求
// - audio.decodePcm                把曲目或区间解成 PCM，经共享缓冲交给页面
// - audio.cancelDecodePcm          取消 decodePcm 任务
// - audio.getPcmDebugState         共享缓冲支持情况与解码队列（调试用）
//=============================================================================
/** @brief Register the audio.* API handlers (spectrum / waveform). */
void RegisterAudioApi();

/** @brief Release the spectrum-visualization runtime (timer / stream / subscriptions).
 *  Called from WebViewUI::shutdown() and background_service::Shutdown(). */
void ShutdownAudioVisualizationRuntime();

/** @brief Drop the full-track waveform requests a closing window made; decodes nobody
 *  else waits for are aborted. Called from PopupWindow teardown. */
void CancelAllWaveformTasksForWindow(const std::string& windowId);

/** @brief Drop the decodePcm requests a closing window made, without events; decodes nobody
 *  else waits for are aborted. Called from PopupWindow teardown. */
void CancelAllPcmForWindow(const std::string& windowId);
