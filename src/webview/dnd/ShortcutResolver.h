// ShortcutResolver.h - resolves .lnk shortcut targets for a dropped file list.
//
// A dragged shortcut puts the .lnk path itself in CF_HDROP; foobar2000 cannot
// play that, so the page needs the target as well. Reading a target needs
// IShellLink + IPersistFile, which a page cannot reach.
//
// The result is a parallel array, never a change to the paths list: the index
// correspondence between paths and DataTransfer.files is an established
// contract, so nothing here may add, drop, or reorder an entry.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fb2k_dnd {

// One entry of the parallel array: the shortcut's target, or nothing when the
// path is not a shortcut, the link names a shell namespace object instead of a
// file, the recorded target is too long to come back intact, COM is unavailable,
// or resolution was skipped because the batch ran out of budget.
//
// A broken shortcut is NOT one of those cases: IShellLinkW::GetPath answers
// S_OK with the target the .lnk recorded whether or not that file still exists,
// so a present entry is a path the shortcut points at, not a promise that
// something is there. Verifying existence is left to the caller, which would
// otherwise pay a filesystem hit per entry on the thread the drag is blocked on.
using ResolvedTarget = std::optional<std::wstring>;

// Budget for one whole drop's worth of resolution, spanning every COM call.
//
// This runs on our UI thread inside IDropTarget::DragEnter while the source
// process sits inside DoDragDrop waiting for us to return, so blocking here
// freezes drag and drop desktop-wide, not just this window. A .lnk pointing at
// a dead mapped drive blocks in IPersistFile::Load, which takes no timeout,
// hence a wall-clock budget rather than a per-call one.
constexpr int64_t kShortcutBatchBudgetMs = 1000;

// Budget for a single shortcut. Load cannot be interrupted once entered, so
// this cannot cap one item's cost; it is a detector. An item that overruns is
// evidence the filesystem behind this batch is slow, and the remaining items
// are abandoned rather than paying the same cost again.
constexpr int64_t kShortcutItemBudgetMs = 200;

// Whether a path names a Windows shortcut, by extension only.
//
// Extension matching is what decides which entries cost a COM round trip, so
// it stays cheap and total: no filesystem access, no path normalisation. Only
// .lnk is recognised. .url, .library-ms and virtual search results are
// deliberately out of scope, since each needs a different resolver and a
// separate security argument.
bool IsShortcutPath(const std::wstring& path);

// Targets for paths, one entry per input, in the same order.
//
// Never throws and never reports an error: an unresolvable shortcut yields an
// empty entry, because a drag must not fail over a broken link. Entries that
// are not shortcuts cost no COM call at all.
//
// Requires an initialised COM apartment on the calling thread, which the drop
// target has by construction (RegisterDragDrop demands one). Without it every
// entry comes back empty rather than failing.
std::vector<ResolvedTarget> ResolveShortcutTargets(
    const std::vector<std::wstring>& paths) noexcept;

// Resolves one shortcut, or nothing. The COM half of the work, separated so the
// batch policy above can be exercised without a shell.
ResolvedTarget ResolveOneShortcut(const std::wstring& lnkPath) noexcept;

// The batch policy with its two dependencies injected: how a single shortcut is
// resolved, and how many milliseconds the batch has been running.
//
// Exists so the extension filter, the budgets, and the length invariant can be
// tested against a fake clock and a fake resolver. ResolveShortcutTargets is
// this function wired to the real ones.
//
// elapsedMs is called rather than sampled once because the point of the budget
// is to observe time actually spent inside resolve. Both callables are invoked
// inside a catch-all, so a throwing test double cannot escape.
std::vector<ResolvedTarget> ResolveShortcutTargetsWith(
    const std::vector<std::wstring>& paths,
    const std::function<ResolvedTarget(const std::wstring&)>& resolveOne,
    const std::function<int64_t()>& elapsedMs) noexcept;

}  // namespace fb2k_dnd
