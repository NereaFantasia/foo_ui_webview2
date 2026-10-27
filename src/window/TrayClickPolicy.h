#pragma once
#include <cstdint>

namespace tray_click {
enum class Action { Event, ToggleMainWindow };
enum class Input { Down, Up, DoubleClick };
struct Timer {
    std::uintptr_t id = 0;
    std::uint64_t generation = 0;
    std::uint64_t deadline = 0;
};
class Policy {
public:
    void SetAction(Action action) { ResetGesture(); action_ = action; }
    Action GetAction() const { return action_; }
    void Created() { created_ = true; }
    void Destroyed() { ResetGesture(); created_ = false; action_ = Action::Event; }
    void Rebuilding() { ResetGesture(); created_ = false; }
    unsigned Handle(Input input, std::uint64_t now, unsigned interval) {
        if (!created_) {
            if (input == Input::Up) { down_ = false; residualUp_ = false; }
            return 0;
        }
        if (action_ == Action::Event) {
            down_ = input != Input::Up;
            if (input == Input::Up) residualUp_ = false;
            return 0;
        }
        if (residualUp_) {
            if (input == Input::Up) residualUp_ = false;
            return 0;
        }
        if (input == Input::Down) { down_ = true; return 0; }
        if (input == Input::DoubleClick) {
            down_ = false;
            residualUp_ = true;
            if (!pending_.id) return 0;
            CancelPending();
            return 1;
        }
        if (!down_) return 0;
        down_ = false;
        const unsigned previous = pending_.id ? 1 : 0;
        // Each arm gets a different native timer id, including within one mode generation.
        if (++nextTimer_ == 0) ++nextTimer_;
        pending_ = {nextTimer_, generation_, now + interval};
        return previous;
    }
    unsigned Expire(Timer timer, std::uint64_t now) {
        if (!created_ || action_ != Action::ToggleMainWindow || !timer.id ||
            timer.id != pending_.id || timer.generation != generation_ || now < pending_.deadline) return 0;
        CancelPending();
        return 1;
    }
    Timer Pending() const { return pending_; }
    void CancelPending() { pending_ = {}; }
private:
    void ResetGesture() {
        residualUp_ = residualUp_ || down_;
        down_ = false;
        ++generation_;
        CancelPending();
    }
    Action action_ = Action::Event;
    bool created_ = false;
    bool down_ = false;
    bool residualUp_ = false;
    std::uint64_t generation_ = 1;
    std::uintptr_t nextTimer_ = 0;
    Timer pending_;
};
}
