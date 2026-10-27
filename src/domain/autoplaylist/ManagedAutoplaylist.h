#pragma once
#include "AutoplaylistConfig.h"
#include <functional>

namespace managed_autoplaylist {

using ChangeCallback = std::function<void(const std::string& playlistGuid, const std::string& revision)>;

struct Operation {
    std::string playlistGuid;
    Outcome result;
    std::optional<std::string> cleanup;
};

/** Identify this provider by its private service interface and provider GUID. Main thread only. */
bool IsManaged(size_t index);
/** Queue an identity-pinned observation of native flags; never mutate from a playlist callback. */
void ScheduleFlagsObservation(size_t index);
/** Install the main-thread notification sink for confirmed configuration changes. */
void SetChangeCallback(ChangeCallback callback);
/** Compile before creating a playlist, and report the outcome of identity-checked cleanup. */
Operation Create(const std::string& name, Config config);
/** Read configuration from the current provider of the specified canonical playlist GUID. */
Operation Read(const std::string& playlistGuid);
/** Apply a revision-checked partial update on the main thread. */
Operation Update(const std::string& playlistGuid, const Patch& patch, const std::string& expectedRevision);

} // namespace managed_autoplaylist
