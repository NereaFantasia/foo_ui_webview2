// MediaMembershipIndex.cpp
#include "pch.h"
#include "utils/MediaMembershipIndex.h"

namespace fb2k_utils {

void MediaMembershipIndex::Rebuild(std::vector<std::string> canonicalPaths) {
    std::unordered_set<std::string> rebuilt;
    rebuilt.reserve(canonicalPaths.size());
    for (auto& path : canonicalPaths) {
        rebuilt.insert(std::move(path));
    }

    std::unique_lock lock(mutex_);
    paths_ = std::move(rebuilt);
    valid_ = true;
    ++generation_;
}

bool MediaMembershipIndex::Contains(const std::string& canonicalPath) const {
    std::shared_lock lock(mutex_);
    if (!valid_) {
        return false;
    }
    return paths_.contains(canonicalPath);
}

std::optional<bool> MediaMembershipIndex::Query(const std::string& canonicalPath) const {
    std::shared_lock lock(mutex_);
    if (!valid_) {
        return std::nullopt;
    }
    return paths_.contains(canonicalPath);
}

void MediaMembershipIndex::Invalidate() {
    std::unique_lock lock(mutex_);
    paths_.clear();
    valid_ = false;
    ++generation_;
}

bool MediaMembershipIndex::IsValid() const {
    std::shared_lock lock(mutex_);
    return valid_;
}

uint64_t MediaMembershipIndex::Generation() const {
    std::shared_lock lock(mutex_);
    return generation_;
}

size_t MediaMembershipIndex::Size() const {
    std::shared_lock lock(mutex_);
    return paths_.size();
}

}  // namespace fb2k_utils
