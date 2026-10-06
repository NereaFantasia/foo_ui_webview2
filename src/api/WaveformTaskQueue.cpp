#include "pch.h"
#include "api/WaveformTaskQueue.h"

#include <algorithm>
#include <utility>

namespace fb2k_waveform {

CancelVerdict JudgeCancel(const CallerIdentity& registered, const CallerIdentity& canceller) {
    if (registered.hwnd == 0 && registered.windowId.empty()) return CancelVerdict::Unattributed;
    // Either identity matching is enough. The handle is the calling instance's
    // own window (a panel's, not its top-level parent's), so two panels never
    // share it; the window id can be shared, because CallerContext's last
    // fallback hands different callers the first instance's id.
    if (registered.hwnd != 0 && registered.hwnd == canceller.hwnd) return CancelVerdict::Owner;
    if (!registered.windowId.empty() && registered.windowId == canceller.windowId) {
        return CancelVerdict::Owner;
    }
    return CancelVerdict::Denied;
}

WaveformTaskQueue::WaveformTaskQueue(std::size_t maxConcurrent)
    : maxConcurrent_(maxConcurrent == 0 ? 1 : maxConcurrent) {}

WaveformTaskQueue::SubmitResult WaveformTaskQueue::Submit(const std::string& decodeKey, Waiter waiter) {
    SubmitResult result;
    const std::string taskId = waiter.taskId;
    auto open = openByKey_.find(decodeKey);
    if (open != openByKey_.end()) {
        result.jobId = open->second;
        result.merged = true;
        jobs_[result.jobId].waiters.push_back(std::move(waiter));
        jobByTask_[taskId] = result.jobId;
        return result;
    }

    result.jobId = nextJobId_++;
    Job job;
    job.key = decodeKey;
    job.waiters.push_back(std::move(waiter));
    jobs_.emplace(result.jobId, std::move(job));
    openByKey_[decodeKey] = result.jobId;
    jobByTask_[taskId] = result.jobId;
    queued_.push_back(result.jobId);
    result.start = Pump();
    return result;
}

WaveformTaskQueue::CancelResult WaveformTaskQueue::Cancel(const std::string& taskId,
                                                          const CallerIdentity& canceller) {
    CancelResult result;
    auto owner = jobByTask_.find(taskId);
    if (owner == jobByTask_.end()) return result;
    const std::uint64_t jobId = owner->second;
    Job& job = jobs_[jobId];
    auto it = std::find_if(job.waiters.begin(), job.waiters.end(),
                           [&](const Waiter& w) { return w.taskId == taskId; });
    if (it == job.waiters.end()) return result;

    result.verdict = JudgeCancel(it->owner, canceller);
    if (result.verdict == CancelVerdict::Denied) return result;

    result.cancelled = true;
    result.waiter = *it;
    RemoveWaiter(jobId, static_cast<std::size_t>(it - job.waiters.begin()), result.abort, result.discarded);
    return result;
}

WaveformTaskQueue::FinishResult WaveformTaskQueue::Finish(std::uint64_t jobId, JobOutcome outcome) {
    FinishResult result;
    auto it = jobs_.find(jobId);
    if (it == jobs_.end() || it->second.state == JobState::Queued) return result;

    Job job = std::move(it->second);
    jobs_.erase(it);
    auto open = openByKey_.find(job.key);
    if (open != openByKey_.end() && open->second == jobId) openByKey_.erase(open);
    for (const Waiter& w : job.waiters) jobByTask_.erase(w.taskId);

    result.writeCache = outcome == JobOutcome::Succeeded;
    result.notify = std::move(job.waiters);
    result.start = Pump();
    return result;
}

WaveformTaskQueue::DropResult WaveformTaskQueue::DropWindow(const std::string& windowId) {
    DropResult result;
    if (windowId.empty()) return result;

    std::vector<std::uint64_t> ids;
    ids.reserve(jobs_.size());
    for (const auto& [id, job] : jobs_) ids.push_back(id);
    for (std::uint64_t id : ids) {
        auto it = jobs_.find(id);
        if (it == jobs_.end()) continue;
        for (std::size_t i = it->second.waiters.size(); i-- > 0;) {
            if (it->second.waiters[i].owner.windowId != windowId) continue;
            ++result.removedWaiters;
            RemoveWaiter(id, i, result.abort, result.discarded);
            it = jobs_.find(id);
            if (it == jobs_.end()) break;
        }
    }
    return result;
}

WaveformTaskQueue::DropResult WaveformTaskQueue::DropAll() {
    DropResult result;
    closed_ = true;
    for (auto& [id, job] : jobs_) {
        result.removedWaiters += job.waiters.size();
        job.waiters.clear();
        if (job.state == JobState::Decoding) {
            job.state = JobState::Aborting;
            result.abort.push_back(id);
        }
    }
    for (std::uint64_t id : queued_) {
        jobs_.erase(id);
        result.discarded.push_back(id);
    }
    queued_.clear();
    openByKey_.clear();
    jobByTask_.clear();
    std::sort(result.abort.begin(), result.abort.end());
    return result;
}

std::size_t WaveformTaskQueue::ActiveCount() const {
    return static_cast<std::size_t>(std::count_if(jobs_.begin(), jobs_.end(), [](const auto& entry) {
        return entry.second.state != JobState::Queued;
    }));
}

std::size_t WaveformTaskQueue::QueuedCount() const { return queued_.size(); }

bool WaveformTaskQueue::HasJob(std::uint64_t jobId) const { return jobs_.contains(jobId); }

JobState WaveformTaskQueue::StateOf(std::uint64_t jobId) const {
    auto it = jobs_.find(jobId);
    return it == jobs_.end() ? JobState::Queued : it->second.state;
}

std::size_t WaveformTaskQueue::WaiterCount(std::uint64_t jobId) const {
    auto it = jobs_.find(jobId);
    return it == jobs_.end() ? 0 : it->second.waiters.size();
}

void WaveformTaskQueue::RemoveWaiter(std::uint64_t jobId, std::size_t index,
                                     std::vector<std::uint64_t>& abort,
                                     std::vector<std::uint64_t>& discarded) {
    Job& job = jobs_[jobId];
    jobByTask_.erase(job.waiters[index].taskId);
    job.waiters.erase(job.waiters.begin() + static_cast<std::ptrdiff_t>(index));
    if (!job.waiters.empty()) return;

    auto open = openByKey_.find(job.key);
    if (open != openByKey_.end() && open->second == jobId) openByKey_.erase(open);
    if (job.state == JobState::Queued) {
        queued_.erase(std::remove(queued_.begin(), queued_.end(), jobId), queued_.end());
        jobs_.erase(jobId);
        discarded.push_back(jobId);
        return;
    }
    if (job.state == JobState::Decoding) {
        job.state = JobState::Aborting;
        abort.push_back(jobId);
    }
}

std::vector<std::uint64_t> WaveformTaskQueue::Pump() {
    std::vector<std::uint64_t> start;
    if (closed_) return start;
    std::size_t active = ActiveCount();
    while (active < maxConcurrent_ && !queued_.empty()) {
        const std::uint64_t id = queued_.front();
        queued_.pop_front();
        jobs_[id].state = JobState::Decoding;
        start.push_back(id);
        ++active;
    }
    return start;
}

}  // namespace fb2k_waveform
