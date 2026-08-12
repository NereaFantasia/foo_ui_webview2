// HdropReader.cpp
#include "pch.h"
#include "webview/dnd/HdropReader.h"

#include <objidl.h>
#include <shellapi.h>
#include <ShlObj_core.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace fb2k_dnd {
namespace {

// Scans a NUL-separated, double-NUL-terminated list of wide characters.
//
// count is the number of whole wchar_t that actually fit inside the blob, so
// the scan can never read past its end. An entry whose NUL lies outside the
// blob is dropped instead of being emitted truncated: a shortened path would
// silently name a different file.
std::vector<std::wstring> ScanWideList(const wchar_t* chars, size_t count) {
    std::vector<std::wstring> paths;
    size_t start = 0;
    while (start < count) {
        if (chars[start] == L'\0') {
            break;  // list terminator
        }
        size_t end = start;
        while (end < count && chars[end] != L'\0') {
            ++end;
        }
        if (end == count) {
            break;  // unterminated tail entry, discard
        }
        paths.emplace_back(chars + start, end - start);
        start = end + 1;
    }
    return paths;
}

// ANSI counterpart. Each entry is converted with the active code page; an entry
// that fails conversion is skipped rather than replaced by a partial path.
std::vector<std::wstring> ScanAnsiList(const char* chars, size_t count) {
    std::vector<std::wstring> paths;
    size_t start = 0;
    while (start < count) {
        if (chars[start] == '\0') {
            break;
        }
        size_t end = start;
        while (end < count && chars[end] != '\0') {
            ++end;
        }
        if (end == count) {
            break;
        }
        const int byteLen = static_cast<int>(end - start);
        const int wideLen =
            ::MultiByteToWideChar(CP_ACP, 0, chars + start, byteLen, nullptr, 0);
        if (wideLen > 0) {
            std::wstring wide(static_cast<size_t>(wideLen), L'\0');
            const int written = ::MultiByteToWideChar(CP_ACP, 0, chars + start, byteLen,
                                                      wide.data(), wideLen);
            if (written > 0) {
                wide.resize(static_cast<size_t>(written));
                paths.push_back(std::move(wide));
            }
        }
        start = end + 1;
    }
    return paths;
}

// Upper bound for the speculative reserve. Well past any plausible hand-made
// selection, while a bogus count cannot allocate more than a few MB of pointers.
constexpr size_t kReserveCap = 4096;

// Releases a medium obtained from GetData on every exit, an exception included.
struct StgMediumGuard {
    STGMEDIUM* medium;
    ~StgMediumGuard() { ::ReleaseStgMedium(medium); }
};

}  // namespace

std::vector<std::wstring> ParseDropFilesBlob(const void* blob, size_t size) {
    if (!blob || size < sizeof(DROPFILES)) {
        return {};
    }

    const auto* bytes = static_cast<const unsigned char*>(blob);
    const auto* header = reinterpret_cast<const DROPFILES*>(bytes);

    // pFiles is a byte offset from the start of the structure to the path list.
    // Reject anything that would point into the header or past the blob.
    const size_t offset = static_cast<size_t>(header->pFiles);
    if (offset < sizeof(DROPFILES) || offset >= size) {
        return {};
    }

    const size_t available = size - offset;
    const void* list = bytes + offset;

    if (header->fWide) {
        // Only whole characters are usable; a trailing odd byte is ignored.
        return ScanWideList(static_cast<const wchar_t*>(list), available / sizeof(wchar_t));
    }
    return ScanAnsiList(static_cast<const char*>(list), available);
}

std::vector<std::wstring> ReadHdropPaths(IDataObject* dataObject, bool* hadHdrop) noexcept {
    if (hadHdrop) {
        *hadHdrop = false;
    }
    std::vector<std::wstring> paths;
    if (!dataObject) {
        return paths;
    }

    FORMATETC format = {};
    format.cfFormat = CF_HDROP;
    format.ptd = nullptr;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;

    // The whole COM region sits inside the try so that no allocation failure can
    // escape into IDropTarget::DragEnter, which reports drags through an HRESULT.
    try {
        STGMEDIUM medium = {};
        if (FAILED(dataObject->GetData(&format, &medium))) {
            return paths;  // drag carries no file list
        }

        // Release only the medium this function obtained, on every path out
        // including a throw. The data object is borrowed from the drag source, so
        // its lifetime is not ours to end, and DragFinish must not be called on an
        // HDROP we did not take ownership of.
        StgMediumGuard release{&medium};

        // A source may answer S_OK with a tymed other than the one asked for, and
        // hGlobal shares a union with pstm and lpszFileName, so reading it as an
        // HDROP would hand shell32 an IStream or a string to parse as a DROPFILES
        // header. hadHdrop stays false here because the caller turns it into
        // hasFiles: a medium that cannot be read is not a file list the page can
        // be promised, and claiming otherwise would offer a drop we cannot honour.
        if (medium.tymed != TYMED_HGLOBAL) {
            return paths;
        }
        if (hadHdrop) {
            *hadHdrop = true;
        }

        if (auto hdrop = static_cast<HDROP>(medium.hGlobal)) {
            const UINT count = ::DragQueryFileW(hdrop, 0xFFFFFFFFu, nullptr, 0);
            // count comes from the untrusted HDROP, so it only guides the
            // reservation; the loop bound is what actually limits the work.
            paths.reserve(count < kReserveCap ? count : kReserveCap);
            for (UINT i = 0; i < count; ++i) {
                // Query the exact length per entry: paths can exceed MAX_PATH, so a
                // fixed-size buffer would silently truncate them.
                const UINT length = ::DragQueryFileW(hdrop, i, nullptr, 0);
                if (length == 0) {
                    continue;
                }
                std::wstring path(length, L'\0');
                const UINT written = ::DragQueryFileW(hdrop, i, path.data(), length + 1);
                if (written == 0) {
                    continue;
                }
                path.resize(written);
                paths.push_back(std::move(path));
            }
        }
    } catch (...) {
        // A partial list is worse than none: it would drop files without saying
        // so. Cleared instead, which makes the caller's hasFiles false.
        paths.clear();
    }
    return paths;
}

}  // namespace fb2k_dnd
