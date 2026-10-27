#pragma once
#include <cstdint>
#include <algorithm>
#include <map>
#include <vector>

namespace window_activation {
struct Target {
    std::uintptr_t handle = 0;
    std::uint64_t instance = 0;
    bool operator==(const Target&) const = default;
};
struct Reference { Target target; std::uint32_t process = 0, thread = 0; };
struct Request { Target target; std::uint64_t epoch = 0; };
struct Interaction { Target source; std::uint64_t epoch = 0, serial = 0, tick = 0; };
struct Undo { Interaction interaction; Target target; };
struct Return { Target surface; std::uint64_t epoch = 0, serial = 0; };
class Policy {
public:
    Target Register(std::uintptr_t handle) {
        if (!handle) return {};
        auto [it, inserted] = instances_.try_emplace(handle, 0);
        if (inserted) it->second = ++nextInstance_;
        return {handle, it->second};
    }
    Target Find(std::uintptr_t handle) const {
        const auto it = instances_.find(handle);
        return it == instances_.end() ? Target{} : Target{handle, it->second};
    }
    void Forget(Target target) { if (IsLive(target)) instances_.erase(target.handle); }
    bool IsLive(Target target) const { return target.instance && Find(target.handle) == target; }
    const std::map<std::uintptr_t, std::uint64_t>& Targets() const { return instances_; }
    void Clear() { instances_.clear(); active_.clear(); returns_.clear(); ++epoch_; }
    Request BeginExplicit(Target target) {
        return IsLive(target) ? Request{target, ++epoch_} : Request{};
    }
    std::uint64_t Epoch() const { return epoch_; }
    bool IsCurrent(Request request) const { return IsLive(request.target) && request.epoch == epoch_; }
    void Enter(Request request) { if (IsCurrent(request)) active_.push_back(request); }
    void Leave(Request request) {
        const auto it = std::find_if(active_.rbegin(), active_.rend(), [&](const Request& entry) {
            return entry.target == request.target && entry.epoch == request.epoch;
        });
        if (it != active_.rend()) active_.erase(std::next(it).base());
    }
    bool IsExplicit(Target target) const {
        return IsLive(target) && !active_.empty() && active_.back().target == target;
    }
    Interaction Interact(Target source, std::uint64_t tick) {
        if (!IsLive(source) || IsExplicit(source)) return {};
        return {source, epoch_, ++interactionSerial_, tick};
    }
    bool IsCurrent(Interaction interaction) const {
        return interaction.serial && interaction.serial == interactionSerial_ &&
            interaction.epoch == epoch_ && IsLive(interaction.source);
    }
    bool IsRecent(Interaction interaction, Target target, std::uint64_t now, unsigned window) const {
        return IsCurrent(interaction) && IsLive(target) && interaction.source != target &&
            !IsExplicit(target) && now >= interaction.tick && now - interaction.tick <= window;
    }
    Undo CaptureUndo(Interaction interaction, Target target) const {
        return IsCurrent(interaction) && IsLive(target) && !IsExplicit(target)
            ? Undo{interaction, target} : Undo{};
    }
    bool CanUndo(const Undo& undo, Target foreground) const {
        return CanContinue(undo) && undo.target == foreground;
    }
    bool CanContinue(const Undo& undo) const {
        return IsCurrent(undo.interaction) && IsLive(undo.target) && !IsExplicit(undo.target);
    }
    bool CanContinue(const Undo& undo, Target foreground, Target intermediary) const {
        return CanContinue(undo) && (foreground == undo.target ||
            (IsLive(intermediary) && foreground == intermediary));
    }
    Return CaptureReturn(Target surface) {
        const auto serial = ++nextReturn_;
        returns_[surface.handle] = serial;
        return {surface, epoch_, serial};
    }
    bool CanReturn(Return ticket) const {
        const auto it = returns_.find(ticket.surface.handle);
        return ticket.epoch == epoch_ && IsLive(ticket.surface) && it != returns_.end() && it->second == ticket.serial;
    }
private:
    std::map<std::uintptr_t, std::uint64_t> instances_;
    std::map<std::uintptr_t, std::uint64_t> returns_;
    std::vector<Request> active_;
    std::uint64_t nextInstance_ = 0, epoch_ = 1, interactionSerial_ = 0;
    std::uint64_t nextReturn_ = 0;
};

class Scope {
public:
    Scope() = default;
    Scope(Policy& policy, Request request) : policy_(policy.IsCurrent(request) ? &policy : nullptr), request_(request) {
        if (policy_) policy_->Enter(request_);
    }
    ~Scope() { if (policy_) policy_->Leave(request_); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&& other) noexcept : policy_(other.policy_), request_(other.request_) { other.policy_ = nullptr; }
    explicit operator bool() const { return policy_ && policy_->IsLive(request_.target); }
private:
    Policy* policy_ = nullptr;
    Request request_;
};
}
