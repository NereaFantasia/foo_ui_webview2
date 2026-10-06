// SpectrumSchedule.h - per-tick scheduling of spectrum subscriptions
//
/* No foobar2000 SDK or Win32 dependency. The host's timer callback reads the
 * playback state and the external-foreground throttle verdict once per tick,
 * then hands them in together with every subscription; this unit decides which
 * subscriptions are due, which FFT configurations have to be computed, and
 * which frames go out. Nothing here touches the visualisation stream or the
 * bridge, so the state machine and the timing rules run in tests without a host.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace fb2k_spectrum {

using Clock = std::chrono::steady_clock;

enum class PlaybackState { Stopped, Paused, Playing };

enum class SpectrumScale { Weighted, Db };

// Per-subscription parameters as accepted by audio.subscribeSpectrum. fftSize
// is the requested value; the size actually used follows EffectiveFftSize().
struct ScheduleParams {
    std::string subscriptionId;
    int fftSize = 1024;
    int bands = 48;
    int fps = 30;
    SpectrumScale scale = SpectrumScale::Weighted;
    bool backgroundThrottle = true;
    // Band range in Hz as requested; maxFrequency 0 follows the stream's
    // sampleRate / 2. See SpectrumBands.h for how the upper edge is derived.
    double minFrequency = 20.0;
    double maxFrequency = 0.0;
};

// Mutable per-subscription state owned by the host and advanced by PlanTick().
// nextDue at the clock epoch means "due at the next tick"; that is the value
// MakeScheduleState() starts with and the value a return to Playing resets to.
struct ScheduleState {
    Clock::time_point nextDue{};
    PlaybackState lastState = PlaybackState::Stopped;
};

// The state a subscription starts in. The playback state at registration is the
// initial state and does not count as a transition: a subscription created
// while paused receives no silence frame until the state changes again.
ScheduleState MakeScheduleState(PlaybackState atRegistration);

struct ScheduleEntry {
    const ScheduleParams* params = nullptr;
    ScheduleState* state = nullptr;
    // false while the owning page is hidden: frames for it are decided and its
    // state advances, but delivery is dropped.
    bool visible = true;
};

struct TickInput {
    Clock::time_point now{};
    PlaybackState playback = PlaybackState::Stopped;
    // External-foreground throttle verdict for this tick, decided once by the
    // host and shared by every subscription with backgroundThrottle == true.
    bool throttled = false;
    // Overload protection: no FFT is computed this tick. Silence frames and
    // state transitions are unaffected.
    bool skipFft = false;
    // Upper bound for EffectiveFftSize(); the host passes kMaxSpectrumFftSize.
    int maxFftSize = 65536;
    // Half of the host's beat, not of a subscription's interval. A subscription
    // counts as due once now >= nextDue - halfBeat, so a beat that jitters a
    // little ahead of a deadline equal to the beat length still emits instead of
    // slipping a whole beat. Deadlines still advance from the previous deadline,
    // so the long-term frame rate does not rise. Zero is the strict comparison.
    Clock::duration halfBeat{};
};

// One FFT configuration to compute this tick. Subscriptions whose effective
// fftSize, bands, scale and requested frequency range coincide share one group.
struct ComputeGroup {
    int fftSize = 0;
    int bands = 0;
    SpectrumScale scale = SpectrumScale::Weighted;
    double minFrequency = 20.0;
    double maxFrequency = 0.0;
};

struct FrameDecision {
    std::string subscriptionId;
    // true: a silence frame announcing `state`, no FFT involved.
    bool silent = false;
    // Index into TickPlan::groups for spectrum frames; -1 for silence frames
    // and for frames that are not delivered.
    int group = -1;
    // Playback state to stamp into the frame.
    PlaybackState state = PlaybackState::Stopped;
    // false when the page is hidden: nothing is sent and nothing is re-sent
    // later; the subscription's state has already moved on.
    bool deliver = true;
};

struct TickPlan {
    std::vector<ComputeGroup> groups;
    std::vector<FrameDecision> frames;
};

// The FFT size actually used for a request: bands >= 64 need at least 8192
// points and bands >= 32 at least 4096 so that the low bands do not collapse
// onto a handful of bins (the rule the host applied before scheduling existed);
// the result never exceeds maxFftSize. `requested` is expected to be a power of
// two already validated by the caller.
int EffectiveFftSize(int requested, int bands, int maxFftSize);

// Advance every entry by one tick and decide the tick's work. Order is fixed:
// the state machine moves first for every entry, independent of
// visibility, throttling and skipFft; then due subscriptions are picked; then
// delivery is decided.
//
// - Entering Paused or Stopped from any other state yields one silence frame,
//   also while throttled or skipping FFT. Entering Playing resets the deadline
//   so the first spectrum frame goes out on this tick.
// - While not Playing no spectrum frame is planned and deadlines hold.
// - A due spectrum frame that is withheld because skipFft is set, or because
//   the tick is throttled and the subscription has backgroundThrottle, does not
//   consume the deadline: the subscription stays due for the next tick. This is
//   what keeps two throttled subscriptions at the throttle cadence instead of
//   half of it.
// - Deadlines advance by 1 s / fps from the previous deadline; when the
//   subscription has fallen behind by more than one interval the deadline is
//   reset to now + interval and no frame is made up.
// - Hidden entries get FrameDecision::deliver == false and never create a
//   ComputeGroup; their deadlines and states advance like everybody else's.
TickPlan PlanTick(const TickInput& in, const std::vector<ScheduleEntry>& entries);

// The beat thread's next deadline: deadline + beat, or now + beat when more
// than one beat behind, so a stall does not produce a burst of beats (the same
// rule as the per-subscription deadlines). A change of beat length does not go
// through here; the thread restarts its deadline from the current time.
Clock::time_point NextBeat(Clock::time_point deadline, Clock::duration beat, Clock::time_point now);

// Coalesces beats posted to the main thread: at most one tick is queued or
// running at a time. The beat thread posts only when TryArm() succeeds; the
// tick clears the gate before any check or early return. Reset() runs when a
// beat thread starts, so a tick that was posted by a stopped thread and never
// ran cannot keep the gate closed for the new one.
class BeatGate {
public:
    // true when the gate was open and is now armed; false means a tick is
    // still pending and this beat is dropped.
    bool TryArm() { return !armed_.exchange(true); }
    void Disarm() { armed_.store(false); }
    void Reset() { armed_.store(false); }
    bool IsArmed() const { return armed_.load(); }

private:
    std::atomic<bool> armed_{false};
};

}  // namespace fb2k_spectrum
