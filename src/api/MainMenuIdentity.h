#pragma once

#include "api/MenuNodeContract.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <stdexcept>

namespace main_menu {

template <class Equal = decltype(&menu_node::SegmentsEqual)>
bool MatchesQuery(const std::string& name, const std::string& display,
                  const std::string& path, bool hierarchyComplete, const std::string& query,
                  Equal equal = &menu_node::SegmentsEqual) {
    const auto parts = menu_node::SplitPath(query);
    if (parts.empty()) return false;
    if (parts.size() == 1) return equal(name, parts.front()) || equal(display, parts.front());
    if (!hierarchyComplete) return false;
    const auto actual = menu_node::SplitPath(path);
    return actual.size() == parts.size() && std::equal(actual.begin(), actual.end(), parts.begin(), equal);
}

struct Group {
    std::string guid;
    std::string parent;
    std::string label;
    bool popup = false;
    bool complete = true;
};

struct GroupPath {
    std::string path;
    std::vector<std::string> ancestors;
    bool complete = false;
};

// Ordinary and inline groups retain identity without adding a displayed path segment.
inline GroupPath ResolveGroupPath(const std::vector<Group>& groups, std::string parent,
                                 const std::unordered_map<std::string, std::string>& roots) {
    GroupPath result;
    std::vector<std::string> labels;
    std::unordered_set<std::string> seen;
    while (!parent.empty() && seen.insert(parent).second) {
        result.ancestors.push_back(parent);
        if (const auto root = roots.find(parent); root != roots.end()) {
            labels.push_back(root->second);
            std::reverse(labels.begin(), labels.end());
            result.path = menu_node::JoinPath(labels);
            result.complete = true;
            return result;
        }
        const Group* match = nullptr;
        for (const auto& group : groups) {
            if (group.guid != parent) continue;
            if (match) return result;
            match = &group;
        }
        if (!match || !match->complete) return result;
        if (match->popup && !match->label.empty()) labels.push_back(match->label);
        parent = match->parent;
    }
    return result;
}

struct Entry {
    std::string name;
    std::string displayText;
    std::string path;
    std::string guid;
    std::string subGuid;
    std::string groupGuid;
    std::vector<std::string> groupAncestors;
    std::uintptr_t provider = 0;
    std::uint32_t slot = 0;
    std::uintptr_t node = 0;
    bool isDynamic = false;
    bool isDynamicParent = false;
    bool hierarchyComplete = true;
    menu_node::State state;
};

template <class Construct, class Read, class Valid>
auto ReadConstructedTree(Construct construct, Read read, Valid valid)
    -> std::optional<decltype(read(construct()))> {
    decltype(construct()) tree;
    try { tree = construct(); } catch (...) { return std::nullopt; }
    if (!valid(tree)) return std::nullopt;
    return read(tree);
}

// The adapter supplies retained native nodes. Enumeration never reinstantiates a subtree.
template <class Node, class Adapter, class Emit>
void VisitDynamic(const Node& node, const std::string& prefix, int depth,
                  Adapter& adapter, Emit& emit) {
    if (!adapter.valid(node) || menu_node::DepthExceeded(depth)) throw std::runtime_error("incompleteHierarchy");
    const auto kind = adapter.kind(node);
    if (kind == menu_node::Kind::Separator) return;
    const auto display = adapter.display(node);
    const std::string path = prefix.empty() ? display.first
        : (display.first.empty() ? prefix : prefix + "/" + display.first);
    if (kind == menu_node::Kind::Command) {
        emit(node, path, display.first, display.second, adapter.guid(node));
        return;
    }
    const auto count = adapter.count(node);
    if (menu_node::ChildrenExceeded(count)) throw std::runtime_error("incompleteHierarchy");
    for (std::size_t i = 0; i < count; ++i) VisitDynamic(adapter.child(node, i), path, depth + 1, adapter, emit);
}

inline bool SameIdentity(const Entry& left, const Entry& right) {
    return left.provider == right.provider && left.slot == right.slot && left.node == right.node &&
           left.isDynamicParent == right.isDynamicParent && left.isDynamic == right.isDynamic &&
           left.guid == right.guid && left.subGuid == right.subGuid;
}

template <class T, class Resolve>
std::vector<T> UniqueTreeMatches(const std::vector<T>& matches, Resolve resolve) {
    std::vector<T> result;
    std::vector<const Entry*> identities;
    for (const auto& match : matches) {
        const Entry* identity = resolve(match);
        const bool duplicate = identity && std::any_of(identities.begin(), identities.end(), [&](const Entry* seen) {
            return seen && SameIdentity(*identity, *seen);
        });
        if (duplicate) continue;
        identities.push_back(identity);
        result.push_back(match);
    }
    return result;
}

template <class T, class Predicate>
std::vector<const T*> Collect(const std::vector<T>& entries, Predicate predicate) {
    std::vector<const T*> result;
    for (const auto& entry : entries) {
        if (!predicate(entry)) continue;
        if (std::none_of(result.begin(), result.end(), [&](const T* other) { return SameIdentity(entry, *other); })) {
            result.push_back(&entry);
        }
    }
    return result;
}

template <class T, class Equal = decltype(&menu_node::SegmentsEqual)>
std::vector<const T*> FindCandidates(const std::vector<T>& entries, const std::string& query,
                                    Equal equal = &menu_node::SegmentsEqual) {
    return Collect(entries, [&](const T& entry) {
        return !entry.isDynamicParent && MatchesQuery(entry.name, entry.displayText, entry.path,
                                                      entry.hierarchyComplete, query, equal);
    });
}

template <class T>
std::vector<const T*> FindAddress(const std::vector<T>& entries, const std::string& guid,
                                 const std::string& subGuid) {
    return Collect(entries, [&](const T& entry) {
        return entry.guid == guid && entry.subGuid == subGuid &&
               !(subGuid.empty() && entry.isDynamic && !entry.isDynamicParent);
    });
}

// Address validity never uses enabled state to choose between owners or leaves.
template <class T>
std::string AddressReason(const T& entry, const std::vector<T>& entries) {
    if (entry.guid.empty()) return "noStableIdentifier";
    for (const auto& other : entries) {
        if (other.guid == entry.guid && (other.provider != entry.provider || other.slot != entry.slot)) {
            return "ambiguousMatch";
        }
    }
    if (entry.isDynamicParent) return "dynamicParent";
    if (entry.isDynamic && entry.subGuid.empty()) return "missingDynamicIdentifier";
    if (!entry.hierarchyComplete) return "incompleteHierarchy";
    const auto matches = FindAddress(entries, entry.guid, entry.subGuid);
    return matches.size() == 1 ? "" : "ambiguousMatch";
}

// Empty means that the selected retained leaf was invoked. Callback exceptions propagate.
template <class T, class ReadEnabled, class Execute>
std::string ExecuteUnique(const std::vector<T>& entries, const std::vector<const T*>& matches,
                          ReadEnabled readEnabled, Execute execute) {
    if (matches.empty()) return "notFound";
    if (matches.size() != 1) return "ambiguousMatch";
    const auto& entry = *matches.front();
    if (const auto reason = AddressReason(entry, entries); !reason.empty()) return reason;
    if (!readEnabled(entry)) return "disabled";
    execute(entry);
    return {};
}

}  // namespace main_menu
