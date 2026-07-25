#pragma once
// MenuTokenTable.h - per-show opaque token index for the self-drawn menu overlay
// (DESIGN 8.3). Extracted from MenuOverlayHost as a header-only, window-free
// component so the token state machine (assignment, collision/failure handling,
// select/value resolution, single-use invalidation) is unit-testable against the
// SAME production code the overlay runs — not a copy.
//
// The token generator is injected (default: a Windows GUID) so tests can force
// generator failure and collisions deterministically.

#include "window/MenuActionContract.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class MenuTokenTable {
public:
    struct Entry {
        menu_action::ResolvedAction action;  // publicId + trusted origin/builtin
        std::string kind;                    // item type
        bool enabled = true;
        bool canSelect = false;              // selectable leaf (normal / nowplaying)
        int sliderMin = 0;
        int sliderMax = 0;
        std::vector<char> segmentEnabled;    // segmented: per-index enabled (1/0)
    };

    // Returns a fresh token string, or std::nullopt on failure. Must be
    // unpredictable and single-use in production (GUID); tests may inject a
    // failing / colliding generator.
    using TokenGen = std::function<std::optional<std::string>()>;

    // Assign a token per actionable node (skips separators) and (re)build the
    // index, mutating `items` in place: each node gains a `_token`, the internal
    // `_origin` / `_builtinAction` are stripped. Recurses into submenus. Private
    // action fields are untrusted by default; only the tray owner path opts in
    // after it has stamped the payload in C++.
    //
    // TRANSACTIONAL: if the generator fails or a unique token cannot be produced
    // within the retry budget, the table is left EMPTY and false is returned —
    // the caller must NOT show a partial token set. On success the table holds
    // exactly the actionable nodes and true is returned.
    bool Rebuild(nlohmann::json& items, const TokenGen& gen,
                 bool trustInternalActionFields = false) {
        index_.clear();
        if (!BuildRecursive(items, gen, trustInternalActionFields)) {
            index_.clear();   // no partial token set escapes
            return false;
        }
        return true;
    }

    // TrayZones model: `zones` is an array of `{ id, items: [...] }`. Walks every
    // zone's items array with the same transactional rebuild semantics as Rebuild.
    // Private action fields remain untrusted unless the tray owner explicitly opts in.
    bool RebuildZones(nlohmann::json& zones, const TokenGen& gen,
                      bool trustInternalActionFields = false) {
        index_.clear();
        if (!zones.is_array()) return true;
        for (auto& z : zones) {
            if (!z.is_object() || !z.contains("items")) continue;
            if (!BuildRecursive(z["items"], gen, trustInternalActionFields)) {
                index_.clear();
                return false;
            }
        }
        return true;
    }

    // Resolve a select: valid only for a known token whose entry is enabled AND
    // selectable. Unknown / expired (cleared) token, disabled item, submenu
    // parent or value control -> nullopt.
    std::optional<menu_action::ResolvedAction> ResolveSelect(const std::string& token) const {
        auto it = index_.find(token);
        if (it == index_.end()) return std::nullopt;
        if (!it->second.enabled || !it->second.canSelect) return std::nullopt;
        return it->second.action;
    }

    // Resolve a value change: valid only for a known, enabled value control whose
    // value is in range (rating 0..5 / slider [min,max] / segmented existing +
    // enabled index). Returns the item's public id to report, else nullopt.
    std::optional<std::string> ResolveValue(const std::string& token, int value) const {
        auto it = index_.find(token);
        if (it == index_.end()) return std::nullopt;
        if (!it->second.enabled) return std::nullopt;
        if (!menu_action::RichValueInRange(it->second.kind, value, it->second.sliderMin,
                                           it->second.sliderMax, it->second.segmentEnabled)) {
            return std::nullopt;
        }
        return it->second.action.publicId;
    }

    // Single-use invalidation: the first accepted select clears the whole set so
    // a rapid repeat (e.g. during an exit animation) resolves to nothing.
    void Clear() { index_.clear(); }
    bool Empty() const { return index_.empty(); }
    std::size_t Size() const { return index_.size(); }

    static constexpr int kMaxTokenRetries = 8;

private:
    bool BuildRecursive(nlohmann::json& items, const TokenGen& gen,
                        bool trustInternalActionFields) {
        if (!items.is_array()) return true;
        for (auto& it : items) {
            if (!it.is_object()) continue;
            std::string type = it.value("type", std::string("normal"));
            if (type == "separator") continue;   // separators carry no token
            const bool hasSub = it.contains("submenu") && it["submenu"].is_array()
                                && !it["submenu"].empty();

            Entry entry;
            entry.action.publicId = it.value("id", std::string());
            std::optional<std::string> origin;
            std::optional<std::string> builtin;
            if (trustInternalActionFields && it.contains("_origin") && it["_origin"].is_string()) {
                origin = it["_origin"].get<std::string>();
            }
            if (trustInternalActionFields && it.contains("_builtinAction") &&
                it["_builtinAction"].is_string()) {
                builtin = it["_builtinAction"].get<std::string>();
            }
            entry.action = menu_action::ResolveInternalActionFields(
                entry.action.publicId, origin, builtin);
            // Tray owner-mode inputs explicitly opt into trusted internal
            // stamps. Public menu.show always opts out, so private JSON fields
            // and exact reserved ids remain ordinary user actions there.
            entry.kind = type;
            entry.enabled = it.value("enabled", true);
            entry.canSelect = menu_action::CanSelectLeaf(type, entry.enabled, hasSub);
            if (type == "slider") {
                int mn = it.value("min", 0);
                int mx = it.value("max", 100);
                if (mx < mn) std::swap(mn, mx);   // normalize for range validation
                entry.sliderMin = mn;
                entry.sliderMax = mx;
                // Constant slider (min==max): never accept value changes.
                // ResolveValue rejects via RichValueInRange when
                // sliderMin==sliderMax after the constant gate below.
            } else if (type == "segmented" && it.contains("segments") && it["segments"].is_array()) {
                for (const auto& s : it["segments"]) {
                    entry.segmentEnabled.push_back(
                        (s.is_object() && s.value("enabled", true)) ? 1 : 0);
                }
            }

            // Unique token with finite retry. Generator failure or an
            // unresolvable collision aborts the whole build (transactional).
            std::string token;
            bool got = false;
            for (int attempt = 0; attempt < kMaxTokenRetries; ++attempt) {
                std::optional<std::string> candidate = gen();
                if (!candidate.has_value() || candidate->empty()) return false;  // generator failure
                if (index_.find(*candidate) == index_.end()) {
                    token = std::move(*candidate);
                    got = true;
                    break;
                }
                // collision -> retry
            }
            if (!got) return false;   // could not obtain a unique token

            index_[token] = std::move(entry);
            it["_token"] = token;
            it.erase("_origin");        // internal routing fields never reach the renderer
            it.erase("_builtinAction");

            if (hasSub && !BuildRecursive(it["submenu"], gen, trustInternalActionFields)) return false;
        }
        return true;
    }

    std::unordered_map<std::string, Entry> index_;
};
