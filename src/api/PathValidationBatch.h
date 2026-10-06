#pragma once

#include "api/SkippedPathFilter.h"
#include <optional>
#include <utility>

namespace api {

struct PathValidationBatch {
    struct Failure {
        std::string message;
        bool shapeError = false;
    };
    std::optional<Failure> error;
    std::vector<SkippedEntries> skipped;
    std::size_t skippedCount = 0;
};

// The validator runs before dispatch. Both synchronous and deferred calls use
// this aggregation, including removal of every entry rejected by skipInvalid.
template <class Specs, class Validator>
PathValidationBatch CheckPathSpecs(const nlohmann::json& params, const Specs& specs, Validator validate) {
    PathValidationBatch checked;
    for (const auto& spec : specs) {
        auto verdict = validate(params, spec);
        if (!verdict.success) {
            checked.error = PathValidationBatch::Failure{verdict.errorMsg, verdict.shapeError};
            return checked;
        }
        if (!verdict.skippedIndices.empty()) {
            checked.skippedCount += verdict.skippedIndices.size();
            checked.skipped.push_back({spec.paramKey, std::move(verdict.skippedIndices)});
        }
    }
    return checked;
}

inline void AddSkippedPathCount(nlohmann::json& result, std::size_t count) {
    if (count > 0 && result.is_object() && result.value("success", false)) result["skippedPaths"] = count;
}

}  // namespace api
