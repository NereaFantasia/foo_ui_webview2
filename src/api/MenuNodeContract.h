#pragma once
// MenuNodeContract.h - unified menu node representation
// (docs/menu-subsystem/SPEC.md §5). Header-only and free of the foobar2000
// SDK, WebView2 and Win32 so GoogleTest drives the SAME production code the
// API layer runs - not a copy. This mirrors the established pattern in
// MenuTokenTable.h / MenuResourceLimits.h / TaskbarTrayContracts.h.
//
// Scope of this file: everything that is pure decision logic.
//   - flag -> boolean state normalization for all node sources
//   - addressability classification
//   - label / path normalization and exact segment matching
//   - shared traversal limits
// SDK interaction lives in MenuNodeCollector; execution in MenuExecutionGuard.
//
// GUIDs are carried as already-formatted strings so this header needs no
// Win32 GUID type. The collector performs GUID<->string conversion.

#include <cstdint>
#include <string>
#include <vector>

namespace menu_node {

// ---------------------------------------------------------------------------
// Node identity
// ---------------------------------------------------------------------------

enum class Kind {
    Command,    // invokable leaf
    Submenu,    // container
    Separator,  // visual gap; only `source` is meaningful
};

// Where a node came from. Retained in the response so callers can reason about
// capability differences between tiers (SPEC §2.1) instead of guessing.
enum class Source {
    MainMenuStatic,      // mainmenu_commands slot
    MainMenuDynamic,     // mainmenu_node from dynamic_instantiate()
    ContextMenuStatic,   // contextmenu_item slot
    ContextMenuDynamic,  // contextmenu_item_node subtree
    HmenuFallback,       // walked from a generated Win32 HMENU
};

// ---------------------------------------------------------------------------
// Raw SDK flag vocabularies
//
// The two menu families use DIFFERENT bit assignments for the same concepts.
// Mixing them up silently mislabels state, so both are spelled out here and
// converted through dedicated normalizers. Values mirror the vendored SDK:
//   mainmenu : lib/foobar2000_sdk/foobar2000/SDK/menu.h  (mainmenu_commands)
//   context  : lib/foobar2000_sdk/foobar2000/SDK/contextmenu.h
//              (contextmenu_item_node::t_flags)
// ---------------------------------------------------------------------------

namespace mainmenu_flag {
inline constexpr std::uint32_t kDisabled      = 1u << 0;
inline constexpr std::uint32_t kChecked       = 1u << 1;
inline constexpr std::uint32_t kRadioChecked  = 1u << 2;
// "hidden by default, reachable while holding Shift"
inline constexpr std::uint32_t kDefaultHidden = 1u << 3;
}  // namespace mainmenu_flag

namespace contextmenu_flag {
inline constexpr std::uint32_t kChecked      = 1u;
inline constexpr std::uint32_t kDisabled     = 2u;
inline constexpr std::uint32_t kGrayed       = 4u;
inline constexpr std::uint32_t kRadioChecked = 8u;
}  // namespace contextmenu_flag

// Win32 menu item states used by the HMENU fallback tier. Declared locally so
// this header stays Win32-free; values match winuser.h:
//   MF_GRAYED 0x1, MF_DISABLED 0x2, MF_CHECKED 0x8
//   MFS_GRAYED == MFS_DISABLED == MF_GRAYED|MF_DISABLED == 0x3
// Both single bits are listed because MFS_DISABLED is the *union*, so testing
// against it alone would also report a merely-grayed item as disabled.
namespace hmenu_state {
inline constexpr std::uint32_t kGrayed   = 0x00000001u;  // MF_GRAYED
inline constexpr std::uint32_t kDisabled = 0x00000002u;  // MF_DISABLED
inline constexpr std::uint32_t kChecked  = 0x00000008u;  // MF_CHECKED
}  // namespace hmenu_state

// contextmenu_item::t_enabled_state. FORCE_OFF means "keyboard shortcut list
// only" per the SDK, which is stronger than DEFAULT_OFF's "hidden by default".
enum class ContextEnabledState {
    ForceOff = 0,
    DefaultOff = 1,
    DefaultOn = 2,
};

// Execution pre-flight for a context-menu command.
//
// The SDK treats FORCE_OFF as "this command belongs in the keyboard-shortcut
// list only" — it is never drawn in the real menu for the current selection.
// Dispatching it anyway is what let callers "successfully" run commands the user
// could not have clicked. DEFAULT_OFF is deliberately NOT refused: it only means
// "hidden unless Shift is held", so it remains reachable and invocable.
//
// `force` exists because a caller holding a GUID it obtained deliberately may
// still want the old unchecked behaviour; it must be opt-in, never the default.
inline bool ShouldRefuseExecution(ContextEnabledState state, bool force) {
    if (force) return false;
    return state == ContextEnabledState::ForceOff;
}

// ---------------------------------------------------------------------------
// Normalized state
// ---------------------------------------------------------------------------

// One vocabulary for every source. `flags` keeps the untranslated bits so
// advanced callers can still distinguish e.g. defaultHidden from FORCE_OFF,
// which both normalize to hidden == true.
//
// `stateKnown` records whether enabled/checked were actually observed. The
// context-menu tier can only report them when a track selection exists, because
// `item_get_display_data_root()` takes a `metadb_handle_list`. Reporting a
// cheerful `enabled: true` for an unqueried item would be indistinguishable
// from a genuinely enabled one, so the distinction is carried explicitly rather
// than papered over (SPEC §5.1 fail-loud).
struct State {
    bool enabled = true;
    bool checked = false;
    bool radioChecked = false;
    bool hidden = false;
    bool stateKnown = true;
    std::uint32_t flags = 0;
};

// Main menu, static slots and dynamic nodes alike.
//
// `displayReturnedTrue` is the bool result of get_display(); returning false is
// the SDK's documented way to make a command shortcut-only, so it must be
// treated as hidden rather than dropped on the floor (SPEC D1).
inline State NormalizeMainMenu(std::uint32_t flags, bool displayReturnedTrue) {
    State s;
    s.flags = flags;
    s.enabled = (flags & mainmenu_flag::kDisabled) == 0;
    s.radioChecked = (flags & mainmenu_flag::kRadioChecked) != 0;
    s.checked = s.radioChecked || (flags & mainmenu_flag::kChecked) != 0;
    s.hidden = !displayReturnedTrue || (flags & mainmenu_flag::kDefaultHidden) != 0;
    return s;
}

// Context menu, static items and dynamic nodes alike.
//
// Disabled covers DISABLED and GRAYED independently: some components set only
// one of the pair even though the SDK defines DISABLED_GRAYED as their union.
inline State NormalizeContextMenu(std::uint32_t flags,
                                  bool displayReturnedTrue,
                                  ContextEnabledState enabledState) {
    State s;
    s.flags = flags;
    s.enabled = (flags & (contextmenu_flag::kDisabled | contextmenu_flag::kGrayed)) == 0;
    s.radioChecked = (flags & contextmenu_flag::kRadioChecked) != 0;
    s.checked = s.radioChecked || (flags & contextmenu_flag::kChecked) != 0;
    s.hidden = !displayReturnedTrue || enabledState == ContextEnabledState::ForceOff;
    s.stateKnown = true;
    return s;
}

// Context menu when no track selection is available.
//
// `item_get_display_data_root()` requires a `metadb_handle_list`, so with an
// empty selection there are no display flags to read at all. `get_enabled_state()`
// takes only an index and stays callable, which is enough to decide `hidden`
// (FORCE_OFF is a constant property of the item per the SDK, not a per-selection
// one). Everything else is genuinely unknown and is reported as such instead of
// defaulting to a value the caller cannot tell apart from a real observation.
inline State NormalizeContextMenuStateUnknown(ContextEnabledState enabledState) {
    State s;
    s.flags = 0;
    s.enabled = true;   // meaningless while stateKnown == false
    s.checked = false;
    s.radioChecked = false;
    s.hidden = enabledState == ContextEnabledState::ForceOff;
    s.stateKnown = false;
    return s;
}

// HMENU fallback tier. A node absent from the generated menu simply is not
// walked, so there is no hidden state to recover here.
inline State NormalizeHmenu(std::uint32_t fState) {
    State s;
    s.flags = fState;
    s.enabled = (fState & (hmenu_state::kDisabled | hmenu_state::kGrayed)) == 0;
    s.checked = (fState & hmenu_state::kChecked) != 0;
    s.radioChecked = false;  // not expressible through MENUITEMINFO state bits
    s.hidden = false;
    return s;
}

// ---------------------------------------------------------------------------
// Addressing
// ---------------------------------------------------------------------------

// Why a node cannot be executed. Emitted alongside a null address so callers
// never receive a listed-but-unusable entry with no explanation (SPEC D11).
enum class Unaddressable {
    None = 0,
    Separator,           // nothing to invoke
    DynamicParent,       // container slot; executing it is UB in the SDK
    NoStableIdentifier,  // tier exposes no GUID (e.g. HMENU without resolve)
    EmptyNode,           // degenerate registration: no name, no children
};

// The only stable way to reach a command. Path and label are display-only
// (SPEC §5.5): name addressing was measured at 22% unreachable and cannot be
// fixed by extending an alias table.
struct Address {
    std::string guid;     // owning command GUID, required
    std::string subGuid;  // dynamic child node GUID; empty when not dynamic

    bool hasSubGuid() const { return !subGuid.empty(); }
    bool valid() const { return !guid.empty(); }
};

// A dynamic container slot must never be handed to the SDK's execute(): for
// mainmenu_commands_v2 dynamic slots the behaviour is undefined and was
// observed to be able to bugcheck the host.
inline Unaddressable ClassifyAddressability(Kind kind,
                                            bool isDynamicParent,
                                            bool hasName,
                                            bool hasStableGuid) {
    if (kind == Kind::Separator) return Unaddressable::Separator;
    if (isDynamicParent) return Unaddressable::DynamicParent;
    if (!hasStableGuid) return Unaddressable::NoStableIdentifier;
    if (!hasName) return Unaddressable::EmptyNode;
    return Unaddressable::None;
}

inline bool IsExecutable(Unaddressable reason) {
    return reason == Unaddressable::None;
}

// Stable wire tokens. Kept next to the enum so response encoding and error
// reporting cannot drift apart.
inline const char* ToString(Unaddressable reason) {
    switch (reason) {
        case Unaddressable::None:                return "";
        case Unaddressable::Separator:           return "separator";
        case Unaddressable::DynamicParent:       return "dynamicParent";
        case Unaddressable::NoStableIdentifier:  return "noStableIdentifier";
        case Unaddressable::EmptyNode:           return "emptyNode";
    }
    return "";
}

inline const char* ToString(Kind kind) {
    switch (kind) {
        case Kind::Command:   return "command";
        case Kind::Submenu:   return "submenu";
        case Kind::Separator: return "separator";
    }
    return "";
}

inline const char* ToString(Source source) {
    switch (source) {
        case Source::MainMenuStatic:     return "mainmenu_static";
        case Source::MainMenuDynamic:    return "mainmenu_dynamic";
        case Source::ContextMenuStatic:  return "contextmenu_static";
        case Source::ContextMenuDynamic: return "contextmenu_dynamic";
        case Source::HmenuFallback:      return "hmenu_fallback";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Label and path normalization
//
// Paths are DISPLAY-ONLY for addressing purposes (see Address above), but they
// still have to be normalized consistently for two jobs: rendering a stable
// `path` field, and resolving a caller-supplied path well enough to report a
// precise error. Matching here is EXACT per segment.
//
// Why exact: the pre-refactor matcher accepted a substring hit in EITHER
// direction and took the first winner, so "Rating/1" could resolve to
// "Rating/10" and silently execute the wrong command.
// ---------------------------------------------------------------------------

// ASCII-only case folding. Bytes >= 0x80 are passed through untouched, so a
// UTF-8 multi-byte sequence can never be corrupted mid-character and no
// negative char is ever handed to a <cctype> function (that is UB, and the
// pre-refactor code did exactly that).
inline char FoldAsciiByte(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u >= 'A' && u <= 'Z') return static_cast<char>(u - 'A' + 'a');
    return c;
}

inline bool IsAsciiSpace(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u == ' ' || u == '\t' || u == '\n' || u == '\r' || u == '\v' || u == '\f';
}

inline std::string TrimAscii(const std::string& in) {
    std::size_t begin = 0;
    while (begin < in.size() && IsAsciiSpace(in[begin])) ++begin;
    std::size_t end = in.size();
    while (end > begin && IsAsciiSpace(in[end - 1])) --end;
    return in.substr(begin, end - begin);
}

// Canonical form of a single menu label:
//   - drop everything from the first tab (Win32 accelerator text, e.g.
//     "Open...\tCtrl+O")
//   - drop '&' mnemonic markers
//   - drop one trailing "..." (a dialog hint, not part of the identity)
//   - trim, then fold ASCII case
inline std::string NormalizeLabel(const std::string& raw) {
    std::string s = raw;

    const std::size_t tab = s.find('\t');
    if (tab != std::string::npos) s.erase(tab);

    std::string noMnemonic;
    noMnemonic.reserve(s.size());
    for (const char c : s) {
        if (c != '&') noMnemonic.push_back(c);
    }
    s = TrimAscii(noMnemonic);

    if (s.size() >= 3 && s.compare(s.size() - 3, 3, "...") == 0) {
        s.erase(s.size() - 3);
    }
    s = TrimAscii(s);

    for (char& c : s) c = FoldAsciiByte(c);
    return s;
}

// Split on '/' or '\', dropping empty segments so "a//b" and "a/b" agree.
inline std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

// Exact match after normalization. Never a substring test.
inline bool SegmentsEqual(const std::string& a, const std::string& b) {
    return NormalizeLabel(a) == NormalizeLabel(b);
}

inline std::string JoinPath(const std::vector<std::string>& parts) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out.push_back('/');
        out += parts[i];
    }
    return out;
}

// Result of resolving a caller-supplied path. `Ambiguous` exists so a genuinely
// ambiguous request is reported as such instead of silently resolving to
// whichever node happened to be enumerated first.
enum class MatchKind {
    NotFound,
    Unique,
    Ambiguous,
};

inline const char* ToString(MatchKind kind) {
    switch (kind) {
        case MatchKind::NotFound:  return "notFound";
        case MatchKind::Unique:    return "unique";
        case MatchKind::Ambiguous: return "ambiguous";
    }
    return "";
}

// Classify how many candidates a lookup produced. Callers pair this with the
// candidate list so the response can carry disambiguation hints.
inline MatchKind ClassifyMatch(std::size_t candidateCount) {
    if (candidateCount == 0) return MatchKind::NotFound;
    if (candidateCount == 1) return MatchKind::Unique;
    return MatchKind::Ambiguous;
}

// ---------------------------------------------------------------------------
// Traversal limits
//
// A single depth cap for every menu walk. The pre-refactor code used three
// different values (16 / 10 / unbounded), so the same tree was visible to
// different extents depending on which endpoint you asked (SPEC D13).
// ---------------------------------------------------------------------------

inline constexpr int kMaxMenuTreeDepth = 16;
inline constexpr int kMaxChildrenPerNode = 512;

// Truncation must be reported, never silent: the pre-refactor tree dump capped
// children at 50 while still reporting the true childCount (SPEC D12).
struct Truncation {
    bool depthExceeded = false;
    bool childrenExceeded = false;

    bool any() const { return depthExceeded || childrenExceeded; }
};

inline bool DepthExceeded(int depth) { return depth > kMaxMenuTreeDepth; }

inline bool ChildrenExceeded(std::size_t childCount) {
    return childCount > static_cast<std::size_t>(kMaxChildrenPerNode);
}

}  // namespace menu_node
