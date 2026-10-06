#include "pch.h"
#include "api/SpectrumSchedule.h"

#include <algorithm>

namespace fb2k_spectrum {

namespace {

// "Due at the next tick" marker; see ScheduleState::nextDue.
constexpr Clock::time_point kDueNow{};

Clock::duration FrameInterval(int fps) {
    const int clamped = std::max(1, fps);
    return std::chrono::duration_cast<Clock::duration>(std::chrono::microseconds(1000000 / clamped));
}

// Reset instead of catching up when more than one interval behind: bursting
// several frames after a stall would only clog the bridge.
void AdvanceDeadline(ScheduleState& state, Clock::time_point now, Clock::duration interval) {
    if (state.nextDue == kDueNow || now - state.nextDue > interval) {
        state.nextDue = now + interval;
    } else {
        state.nextDue += interval;
    }
}

// bins 不看 bands 与 scale：SDK 与旧调用方照样会带 bands，拿它分组会让同一配置白算几遍
ComputeGroup MakeGroup(int fftSize, const ScheduleParams& params) {
    if (params.output == SpectrumOutput::Bins) {
        return {fftSize, 0, SpectrumScale::Db, params.minFrequency, params.maxFrequency,
                SpectrumOutput::Bins, params.channels};
    }
    return {fftSize, params.bands, params.scale, params.minFrequency, params.maxFrequency,
            SpectrumOutput::Bands, SpectrumChannels::Mix};
}

int FindOrAddGroup(std::vector<ComputeGroup>& groups, int fftSize, const ScheduleParams& params) {
    const ComputeGroup wanted = MakeGroup(fftSize, params);
    for (size_t i = 0; i < groups.size(); ++i) {
        const ComputeGroup& g = groups[i];
        if (g.fftSize == wanted.fftSize && g.bands == wanted.bands && g.scale == wanted.scale &&
            g.minFrequency == wanted.minFrequency && g.maxFrequency == wanted.maxFrequency &&
            g.output == wanted.output && g.channels == wanted.channels) {
            return static_cast<int>(i);
        }
    }
    groups.push_back(wanted);
    return static_cast<int>(groups.size() - 1);
}

}  // namespace

ScheduleState MakeScheduleState(PlaybackState atRegistration) {
    ScheduleState state;
    state.nextDue = kDueNow;
    state.lastState = atRegistration;
    return state;
}

int EffectiveFftSize(int requested, int bands, int maxFftSize) {
    int minFft = requested;
    if (bands >= 64) {
        minFft = 8192;
    } else if (bands >= 32) {
        minFft = 4096;
    }
    return std::min(std::max(requested, minFft), maxFftSize);
}

int EffectiveFftSize(int requested, int bands, SpectrumOutput output, int maxFftSize) {
    if (output == SpectrumOutput::Bins) return std::min(requested, maxFftSize);
    return EffectiveFftSize(requested, bands, maxFftSize);
}

TickPlan PlanTick(const TickInput& in, const std::vector<ScheduleEntry>& entries) {
    TickPlan plan;

    for (const ScheduleEntry& entry : entries) {
        if (!entry.params || !entry.state) continue;
        const ScheduleParams& params = *entry.params;
        ScheduleState& state = *entry.state;

        // 1. State machine, before anything can bail out.
        const PlaybackState previous = state.lastState;
        const bool entered = previous != in.playback;
        state.lastState = in.playback;

        if (in.playback != PlaybackState::Playing) {
            if (entered) {
                plan.frames.push_back({params.subscriptionId, true, -1, in.playback, entry.visible});
            }
            continue;
        }
        if (entered) {
            state.nextDue = kDueNow;
        }

        // 2. Due?
        if (state.nextDue != kDueNow && in.now < state.nextDue - in.halfBeat) continue;

        // 3. Withheld frames leave the deadline untouched so the subscription is
        //    still due on the next tick that is allowed to compute.
        const bool throttledOut = in.throttled && params.backgroundThrottle;
        if (in.skipFft || throttledOut) continue;

        AdvanceDeadline(state, in.now, FrameInterval(params.fps));

        if (!entry.visible) {
            plan.frames.push_back({params.subscriptionId, false, -1, in.playback, false});
            continue;
        }

        const int fftSize = EffectiveFftSize(params.fftSize, params.bands, params.output, in.maxFftSize);
        const int group = FindOrAddGroup(plan.groups, fftSize, params);
        plan.frames.push_back({params.subscriptionId, false, group, in.playback, true});
    }

    return plan;
}

Clock::time_point NextBeat(Clock::time_point deadline, Clock::duration beat, Clock::time_point now) {
    if (now - deadline > beat) return now + beat;
    return deadline + beat;
}

}  // namespace fb2k_spectrum
