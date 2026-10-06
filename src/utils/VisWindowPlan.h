// VisWindowPlan.h - 可视化窗口的取数规划
//
/* 频谱与 getWaveform 的窗口是截至可视化时钟「现在」的最近 N 个样本（docs/audio-timing/SPEC.md
 * T8）。这里只算取哪几段、各多少样本、前面补几个零；取数与拼接在 AudioApi.cpp。
 *
 * 可视化时钟在起播、seek、手动换曲后从 0 重新计时，头 N / 采样率 秒里流中只有 [0, now)，缺的
 * 部分在前面补零，与 AnalyserNode 起播前历史为零一致。单次 get_chunk_absolute 超过约 1 s 就失败
 * （docs/report/audio-timing/CLOCK_MEASUREMENT_REPORT_2026-09-25.md §5.5），更长的窗口拆成
 * 几段等长的请求：临时构建验证过的正是 65536 点拆成两段各 0.68 s。
 *
 * get_chunk_absolute 把秒数换成样本位置时每次请求各自取整，相邻两段会重一个或缺一个样本：便携
 * 实例上把 1 s 窗口拆成 0.1 s 一段时，接缝处出现一个样本的跳变。FindSeam 按样本内容找出真正的
 * 接缝。
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace vis_window {

// 一次 get_chunk_absolute 的范围。以样本计，0 是可视化时钟的零点。
struct Segment {
    uint64_t start{0};
    uint64_t count{0};
};

struct Plan {
    // 按时间先后，后一段紧接前一段；各段样本数之和加上 zeroPad 等于窗口长度
    std::vector<Segment> segments;
    // 放在取回的样本前面的零，按每声道的样本数计
    uint64_t zeroPad{0};
};

// now 是可视化时钟的秒数，windowSamples 是窗口长度，maxRequestSeconds 是单次请求的上限（不大于 0
// 时不拆）。采样率或窗口为 0、now 不大于 0（含 NaN）时整窗都是零。
inline Plan PlanWindow(double now, uint64_t windowSamples, unsigned sampleRate, double maxRequestSeconds) {
    Plan plan;
    plan.zeroPad = windowSamples;
    if (windowSamples == 0 || sampleRate == 0 || !(now > 0.0)) return plan;

    const auto end = static_cast<uint64_t>(std::llround(now * sampleRate));
    const uint64_t available = std::min(end, windowSamples);
    if (available == 0) return plan;
    plan.zeroPad = windowSamples - available;

    uint64_t maxRequest = available;
    if (maxRequestSeconds > 0.0) {
        const double limit = std::floor(maxRequestSeconds * sampleRate);
        if (limit < static_cast<double>(available)) {
            maxRequest = std::max<uint64_t>(1, static_cast<uint64_t>(limit));
        }
    }
    const uint64_t pieces = (available + maxRequest - 1) / maxRequest;
    const uint64_t base = available / pieces;
    // 除不尽的样本分给最早的几段，每段多一个
    const uint64_t extra = available % pieces;
    plan.segments.reserve(static_cast<size_t>(pieces));
    uint64_t start = end - available;
    for (uint64_t i = 0; i < pieces; ++i) {
        const uint64_t count = base + (i < extra ? 1 : 0);
        plan.segments.push_back({start, count});
        start += count;
    }
    return plan;
}

// 后一段往前多取 guard 帧之后，找出它里面接续已拼好部分的第一帧。tail 是已拼好的最后 tailFrames
// 帧，piece 是后一段的 pieceFrames 帧（含前面多取的帧），都按 channels 交错。在 guard ± maxShift 里
// 取与 tail 末尾逐帧差值之和最小的位置，一样小时取离 guard 近的：数字静音处处一样，就是 guard；
// 周期不大于挪动范围的纯音会有几个位置一样好，挪一整个周期接出来的样本相同。guard 不大于
// maxShift、tail 为空或声道数为 0 时比不了，返回 guard。
template <typename T>
size_t FindSeam(const T* tail, size_t tailFrames, const T* piece, size_t pieceFrames, unsigned channels,
                size_t guard, size_t maxShift) {
    if (guard <= maxShift || channels == 0) return guard;
    const size_t compare = std::min(tailFrames, guard - maxShift);
    if (compare == 0) return guard;
    const auto distance = [guard](size_t k) { return k > guard ? k - guard : guard - k; };
    size_t best = guard;
    double bestError = std::numeric_limits<double>::infinity();
    for (size_t k = guard - maxShift; k <= guard + maxShift && k <= pieceFrames; ++k) {
        double error = 0.0;
        const T* a = tail + (tailFrames - compare) * channels;
        const T* b = piece + (k - compare) * channels;
        for (size_t i = 0; i < compare * channels; ++i) {
            error += std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
        }
        if (error < bestError || (error == bestError && distance(k) < distance(best))) {
            best = k;
            bestError = error;
        }
    }
    return best;
}

}  // namespace vis_window
