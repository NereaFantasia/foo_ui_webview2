#pragma once
// ContextMenuCheck.h - the checks behind a checked menu.runContextCommandById
// (expectPath + expectTarget). Header-only and free of the foobar2000 SDK so
// GoogleTest drives the same code the handler runs.

#include "api/generated/MenuSchema.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace context_menu_check {

struct TrackIdentity {
    std::string path;  // as metadb reports it
    std::uint32_t subsong = 0;
};

// Fingerprint of the tracks a context menu was built for. Order and repeats
// count. `playlistGuid` is the active playlist's GUID for mode "selection" and
// empty otherwise. FNV-1a is enough: the key only has to tell two targets apart
// within one host process; it guards against drift, not forgery, since the
// unchecked calls already run any command on any track.
inline std::string TargetKey(std::string_view mode, const std::vector<TrackIdentity>& tracks,
                             std::string_view playlistGuid) {
    std::uint64_t hash = 14695981039346656037ull;
    auto mix = [&hash](std::string_view field) {
        for (const unsigned char c : field) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        // 0xFF never occurs in UTF-8, so fields cannot run into each other.
        hash ^= 0xFFu;
        hash *= 1099511628211ull;
    };
    mix(mode);
    mix(playlistGuid);
    for (const auto& track : tracks) {
        mix(track.path);
        mix(std::to_string(track.subsong));
    }
    char text[17] = {};
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
    return text;
}

enum class CommandCheck {
    Ok,
    CommandChanged,  // no command at the id, or it has another path
    AmbiguousPath,   // another node has the same path
    Disabled,        // the command or a parent is disabled or hidden
};

// Checks the node at `id` in a menu rebuilt for the same tracks.
inline CommandCheck CheckCommand(const std::vector<api::menu::MenuTreeNode>& roots, std::int64_t id,
                                 const std::string& expectPath) {
    const api::menu::MenuTreeNode* hit = nullptr;
    bool hitBlocked = false;
    int samePath = 0;

    auto walk = [&](const auto& self, const std::vector<api::menu::MenuTreeNode>& nodes,
                    bool parentBlocked) -> void {
        for (const auto& node : nodes) {
            const bool blocked = parentBlocked || !node.enabled.value_or(true) || node.hidden.value_or(false);
            if (node.path && *node.path == expectPath) ++samePath;
            if (node.type == "command" && node.commandId && *node.commandId == id) {
                hit = &node;
                hitBlocked = blocked;
            }
            if (node.children) self(self, *node.children, blocked);
        }
    };
    walk(walk, roots, false);

    if (!hit || !hit->path || *hit->path != expectPath) return CommandCheck::CommandChanged;
    if (samePath > 1) return CommandCheck::AmbiguousPath;
    if (hitBlocked) return CommandCheck::Disabled;
    return CommandCheck::Ok;
}

}  // namespace context_menu_check
