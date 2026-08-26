// ShortcutResolver.cpp
#include "pch.h"
#include "webview/dnd/ShortcutResolver.h"

#include <objbase.h>
#include <objidl.h>
#include <ShlObj_core.h>
#include <wrl/client.h>

#include <chrono>
#include <cwchar>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace fb2k_dnd {
namespace {

constexpr wchar_t kLnkExtension[] = L".lnk";
constexpr size_t kLnkExtensionLength = 4;

// Milliseconds Resolve may spend before it gives up, packed into the high word
// of its flags as IShellLink::Resolve documents. A zero high word would mean
// the 3000 ms default, which is three times the whole batch budget.
constexpr DWORD kResolveTimeoutMs = 200;

int64_t SteadyNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// GetPath with the flags this component is allowed to use, or nothing.
//
// fFlags is 0 because the page needs a path it can hand to playlist or library
// calls. SLGP_RAWPATH is documented as possibly naming something that does not
// exist and as possibly containing unexpanded environment variables, so it
// would put strings like %USERPROFILE%\... on the wire, where the path security
// layer would expand them a second time. SLGP_UNCPRIORITY is marked
// unsupported.
//
// S_FALSE is a success code and means the link has no filesystem path at all,
// which is what a shortcut to a namespace object such as the recycle bin
// returns; treating it as success would publish an empty string where the
// contract says null. Hence the exact S_OK test plus a non-empty check, since
// the buffer is only guaranteed to be written on S_OK.
//
// A buffer filled to capacity is rejected too. cch is documented as counting the
// terminating null, so at most ARRAYSIZE(buf) - 1 characters can come back, and a
// longer target is truncated in silence: the call still reports S_OK with a
// non-empty buffer, and the shortened string names a different filesystem object
// - measured against a 304-character target, whose 259-character remainder was a
// directory rather than the file. Nothing recovers the rest: the interface has no
// length above MAX_PATH and no out parameter for the untruncated size. So "every
// writable character was used" counts as untrustworthy and reports no target at
// all, which is the choice HdropReader.cpp:18-20 already makes for the paths
// array. A real target of exactly ARRAYSIZE(buf) - 1 characters is
// indistinguishable from a truncated one and is refused with it: null costs the
// page a shortcut it could have followed, while a truncated path would have it
// act on the wrong file.
ResolvedTarget ReadLinkPath(IShellLinkW* link) {
    wchar_t buf[MAX_PATH] = {};
    constexpr int kCch = static_cast<int>(ARRAYSIZE(buf));
    const HRESULT hr = link->GetPath(buf, kCch, nullptr, 0);
    if (hr != S_OK || !buf[0]) {
        return std::nullopt;
    }
    // Bounded so an unterminated buffer cannot run off the end; that case yields
    // kCch and is refused by the same test.
    const size_t length = ::wcsnlen(buf, static_cast<size_t>(kCch));
    if (length + 1 >= static_cast<size_t>(kCch)) {
        return std::nullopt;
    }
    return std::wstring(buf, length);
}

}  // namespace

bool IsShortcutPath(const std::wstring& path) {
    if (path.size() <= kLnkExtensionLength) {
        return false;  // ".lnk" alone is an extension, not a shortcut named ".lnk"
    }
    const size_t start = path.size() - kLnkExtensionLength;
    // Shell extensions are case-insensitive, and CompareStringOrdinal keeps that
    // comparison ASCII-exact rather than locale-dependent: a Turkish locale
    // would otherwise decide "I" and "i" differ.
    return ::CompareStringOrdinal(path.c_str() + start, static_cast<int>(kLnkExtensionLength),
                                  kLnkExtension, static_cast<int>(kLnkExtensionLength),
                                  TRUE) == CSTR_EQUAL;
}

ResolvedTarget ResolveOneShortcut(const std::wstring& lnkPath) noexcept {
    try {
        Microsoft::WRL::ComPtr<IShellLinkW> link;
        // CLSCTX_INPROC_SERVER keeps this inside shell32 in our own process; an
        // out-of-process activation would add a second way to block. A failure
        // here includes CO_E_NOTINITIALIZED, so a caller on a thread without an
        // apartment degrades to empty entries instead of misbehaving.
        if (FAILED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&link)))) {
            return std::nullopt;
        }
        Microsoft::WRL::ComPtr<IPersistFile> file;
        if (FAILED(link.As(&file))) {
            return std::nullopt;
        }
        // The blocking step, and the one no timeout reaches: a .lnk on an
        // unreachable network share stalls here. The batch budget in
        // ResolveShortcutTargetsWith is the only bound on it.
        if (FAILED(file->Load(lnkPath.c_str(), STGM_READ))) {
            return std::nullopt;
        }

        // Asked for before Resolve because a link whose target is present needs
        // no repair, and Resolve is what goes looking on the filesystem.
        if (ResolvedTarget direct = ReadLinkPath(link.Get())) {
            return direct;
        }

        // SLR_NO_UI: never show the "problem with this shortcut" dialog, which
        //   would block the drag on a modal window the user did not ask for.
        // SLR_NOSEARCH: do not go hunting for a moved target.
        // SLR_NOTRACK: do not use the distributed link tracking service, which
        //   can reach the network.
        // SLR_NOUPDATE is omitted: without SLR_UPDATE it is close to a no-op,
        //   and this component has no business rewriting the user's .lnk file.
        const DWORD flags = static_cast<DWORD>(SLR_NO_UI | SLR_NOSEARCH | SLR_NOTRACK) |
                            (kResolveTimeoutMs << 16);
        if (FAILED(link->Resolve(nullptr, flags))) {
            return std::nullopt;
        }
        return ReadLinkPath(link.Get());
    } catch (...) {
        // Absorbed at this boundary so the noexcept contract of the drop target
        // path holds all the way down. A broken shortcut is not a drag failure.
        return std::nullopt;
    }
}

std::vector<ResolvedTarget> ResolveShortcutTargetsWith(
    const std::vector<std::wstring>& paths,
    const std::function<ResolvedTarget(const std::wstring&)>& resolveOne,
    const std::function<int64_t()>& elapsedMs) noexcept {
    std::vector<ResolvedTarget> resolved;
    try {
        // Sized up front so every later step only assigns: the array is
        // published alongside paths and a short one would break the index
        // correspondence the contract rests on.
        resolved.assign(paths.size(), std::nullopt);
        if (!resolveOne || !elapsedMs) {
            return resolved;
        }

        // Set once the batch decides it can no longer afford COM. The remaining
        // entries stay empty and cost nothing, which is the only real bound on
        // total damage when a single Load can outlast the whole budget.
        bool abandoned = false;

        for (size_t i = 0; i < paths.size(); ++i) {
            if (abandoned || !IsShortcutPath(paths[i])) {
                continue;
            }
            if (elapsedMs() >= kShortcutBatchBudgetMs) {
                abandoned = true;
                continue;
            }
            const int64_t startedAt = elapsedMs();
            resolved[i] = resolveOne(paths[i]);
            // The overrunning item keeps its answer; it arrived, just late. What
            // its cost buys is the knowledge not to try the rest.
            if (elapsedMs() - startedAt > kShortcutItemBudgetMs) {
                abandoned = true;
            }
        }
    } catch (...) {
        // Only an allocation failure or a throwing injected callable reaches
        // here. Whatever was resolved so far is kept: entries are assigned in
        // place, so a partial pass still lines up by index, and the serializer
        // pads to the length of paths.
    }
    return resolved;
}

std::vector<ResolvedTarget> ResolveShortcutTargets(
    const std::vector<std::wstring>& paths) noexcept {
    try {
        // Zero shortcuts is the common case and must not pay for a clock read
        // or for wrapping the two callables below.
        bool anyShortcut = false;
        for (const std::wstring& path : paths) {
            if (IsShortcutPath(path)) {
                anyShortcut = true;
                break;
            }
        }
        if (!anyShortcut) {
            return std::vector<ResolvedTarget>(paths.size());
        }

        const int64_t startedAt = SteadyNowMs();
        return ResolveShortcutTargetsWith(
            paths, [](const std::wstring& lnkPath) { return ResolveOneShortcut(lnkPath); },
            [startedAt]() { return SteadyNowMs() - startedAt; });
    } catch (...) {
        // Building the two std::function wrappers is the only allocation this
        // frame owns. An empty array here is safe: the serializer pads to the
        // length of paths, so the published arrays still match.
        return {};
    }
}

}  // namespace fb2k_dnd
