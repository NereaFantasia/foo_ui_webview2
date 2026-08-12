// PathCanonicalCache.cpp
#include "pch.h"
#include "utils/PathCanonicalCache.h"

namespace fb2k_utils {

PathCanonicalCache::PathCanonicalCache(size_t capacity) : capacity_(capacity) {}

std::optional<std::wstring> PathCanonicalCache::Lookup(const std::wstring& key, uint64_t parentStamp) {
    std::lock_guard lock(mutex_);

    auto it = entries_.find(key);
    if (it == entries_.end()) {
        ++misses_;
        return std::nullopt;
    }

    if (it->second.parentStamp != parentStamp) {
        // The directory entry changed underneath us, so the recorded resolution
        // may no longer describe the same file. Drop it rather than keep a
        // second chance around: a stale hit would be a wrong security decision.
        recency_.erase(it->second.recencyPos);
        entries_.erase(it);
        ++misses_;
        return std::nullopt;
    }

    recency_.splice(recency_.begin(), recency_, it->second.recencyPos);
    it->second.recencyPos = recency_.begin();
    ++hits_;
    return it->second.resolvedPath;
}

void PathCanonicalCache::Store(const std::wstring& key, std::wstring resolvedPath, uint64_t parentStamp) {
    if (capacity_ == 0) {
        return;
    }

    std::lock_guard lock(mutex_);

    auto it = entries_.find(key);
    if (it != entries_.end()) {
        it->second.resolvedPath = std::move(resolvedPath);
        it->second.parentStamp = parentStamp;
        recency_.splice(recency_.begin(), recency_, it->second.recencyPos);
        it->second.recencyPos = recency_.begin();
        return;
    }

    recency_.push_front(key);
    Entry entry;
    entry.resolvedPath = std::move(resolvedPath);
    entry.parentStamp = parentStamp;
    entry.recencyPos = recency_.begin();
    entries_.emplace(key, std::move(entry));

    while (entries_.size() > capacity_) {
        const std::wstring& victim = recency_.back();
        entries_.erase(victim);
        recency_.pop_back();
        ++evictions_;
    }
}

void PathCanonicalCache::Clear() {
    std::lock_guard lock(mutex_);
    entries_.clear();
    recency_.clear();
}

size_t PathCanonicalCache::Size() const {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

size_t PathCanonicalCache::Capacity() const {
    std::lock_guard lock(mutex_);
    return capacity_;
}

uint64_t PathCanonicalCache::HitCount() const {
    std::lock_guard lock(mutex_);
    return hits_;
}

uint64_t PathCanonicalCache::MissCount() const {
    std::lock_guard lock(mutex_);
    return misses_;
}

uint64_t PathCanonicalCache::EvictionCount() const {
    std::lock_guard lock(mutex_);
    return evictions_;
}

}  // namespace fb2k_utils
