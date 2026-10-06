#pragma once
#include <algorithm>
#include <cmath>

// foobar2000 的音量是 dB（-100 为静音、0 为满），页面用 0-100 的线性百分比。
// playback.getVolume 与 playback:volumeChanged 事件共用这一个换算，两边读数才一致。
namespace volume_scale {

// dB 转换为 0-100 线性百分比（对数逆转换）
// volume = 100 * 10^(dB/20)
inline float PercentFromDb(float db) {
    float volume;
    if (db <= -100.0f) {
        volume = 0.0f;
    } else {
        volume = 100.0f * std::pow(10.0f, db / 20.0f);
        volume = std::max(0.0f, std::min(100.0f, volume));
    }
    return volume;
}

}  // namespace volume_scale
