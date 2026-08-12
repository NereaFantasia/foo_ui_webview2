// test_dnd_hdrop_parse.cpp - CF_HDROP / DROPFILES blob parsing.
//
// Feeds hand-built DROPFILES structures, so no real drag-and-drop, shell
// interaction, or COM apartment is needed.
#include "pch.h"
#include "../src/webview/dnd/HdropReader.h"

#include <objidl.h>

#include <cstring>

using fb2k_dnd::ParseDropFilesBlob;
using fb2k_dnd::ReadHdropPaths;

namespace {

// Builds a DROPFILES blob: header, then NUL-separated paths, then a final NUL.
std::vector<char> MakeWideBlob(const std::vector<std::wstring>& paths) {
    std::vector<wchar_t> chars;
    for (const auto& p : paths) {
        chars.insert(chars.end(), p.begin(), p.end());
        chars.push_back(L'\0');
    }
    chars.push_back(L'\0');  // double-NUL terminator

    std::vector<char> blob(sizeof(DROPFILES) + chars.size() * sizeof(wchar_t));
    auto* df = reinterpret_cast<DROPFILES*>(blob.data());
    df->pFiles = sizeof(DROPFILES);
    df->pt = POINT{0, 0};
    df->fNC = FALSE;
    df->fWide = TRUE;
    std::memcpy(blob.data() + sizeof(DROPFILES), chars.data(),
                chars.size() * sizeof(wchar_t));
    return blob;
}

std::vector<char> MakeAnsiBlob(const std::vector<std::string>& paths) {
    std::vector<char> chars;
    for (const auto& p : paths) {
        chars.insert(chars.end(), p.begin(), p.end());
        chars.push_back('\0');
    }
    chars.push_back('\0');

    std::vector<char> blob(sizeof(DROPFILES) + chars.size());
    auto* df = reinterpret_cast<DROPFILES*>(blob.data());
    df->pFiles = sizeof(DROPFILES);
    df->pt = POINT{0, 0};
    df->fNC = FALSE;
    df->fWide = FALSE;
    std::memcpy(blob.data() + sizeof(DROPFILES), chars.data(), chars.size());
    return blob;
}

}  // namespace

TEST(HdropParse, NullBlobYieldsEmpty) {
    EXPECT_TRUE(ParseDropFilesBlob(nullptr, 0).empty());
}

TEST(HdropParse, ZeroFiles) {
    auto blob = MakeWideBlob({});
    EXPECT_TRUE(ParseDropFilesBlob(blob.data(), blob.size()).empty());
}

TEST(HdropParse, SingleFile) {
    auto blob = MakeWideBlob({L"C:\\music\\a.mp3"});
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"C:\\music\\a.mp3");
}

TEST(HdropParse, MultipleFilesPreserveOrder) {
    // Order matters: the spec relies on CF_HDROP index matching
    // dataTransfer.files index so the page can correlate by subscript.
    auto blob = MakeWideBlob({L"C:\\1.mp3", L"C:\\2.mp3", L"C:\\3.mp3"});
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 3u);
    EXPECT_EQ(paths[0], L"C:\\1.mp3");
    EXPECT_EQ(paths[1], L"C:\\2.mp3");
    EXPECT_EQ(paths[2], L"C:\\3.mp3");
}

TEST(HdropParse, NonAsciiPath) {
    auto blob = MakeWideBlob({L"E:\\OST\\千年幸福論\\01.mp3"});
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"E:\\OST\\千年幸福論\\01.mp3");
}

TEST(HdropParse, LongPathBeyondMaxPath) {
    std::wstring longPath = L"C:\\";
    longPath += std::wstring(400, L'x');
    longPath += L"\\deep.flac";
    ASSERT_GT(longPath.size(), static_cast<size_t>(MAX_PATH));

    auto blob = MakeWideBlob({longPath});
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], longPath);
}

TEST(HdropParse, AnsiVariant) {
    auto blob = MakeAnsiBlob({"C:\\a.mp3", "C:\\b.mp3"});
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], L"C:\\a.mp3");
    EXPECT_EQ(paths[1], L"C:\\b.mp3");
}

TEST(HdropParse, TruncatedBlobDropsIncompleteTail) {
    // "C:\music\a.mp3" is 14 wchar_t; keep only the first 6 so the sole entry is
    // incomplete. A path with no NUL terminator inside the blob must be dropped
    // rather than emitted truncated, otherwise the caller would receive a path
    // pointing at the wrong file.
    auto blob = MakeWideBlob({L"C:\\music\\a.mp3", L"C:\\music\\b.mp3"});
    auto paths = ParseDropFilesBlob(blob.data(), sizeof(DROPFILES) + 6 * sizeof(wchar_t));
    EXPECT_TRUE(paths.empty());
}

TEST(HdropParse, TruncationKeepsFullyTerminatedEntries) {
    // Cut just after the first path's NUL: entry 0 is complete and must survive,
    // entry 1 is unterminated and must be dropped.
    auto blob = MakeWideBlob({L"C:\\a.mp3", L"C:\\b.mp3"});
    const size_t keep = sizeof(DROPFILES) + 9 * sizeof(wchar_t);  // "C:\a.mp3" + NUL
    auto paths = ParseDropFilesBlob(blob.data(), keep);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"C:\\a.mp3");
}

TEST(HdropParse, BlobSmallerThanHeader) {
    char tiny[4] = {};
    EXPECT_TRUE(ParseDropFilesBlob(tiny, sizeof(tiny)).empty());
}

TEST(HdropParse, PFilesOffsetOutOfRange) {
    auto blob = MakeWideBlob({L"C:\\a.mp3"});
    reinterpret_cast<DROPFILES*>(blob.data())->pFiles = 0xFFFFFF;
    EXPECT_TRUE(ParseDropFilesBlob(blob.data(), blob.size()).empty());
}

TEST(HdropParse, MissingDoubleNulTerminatorStillParsesTerminatedEntry) {
    // Only the list terminator is gone; the single path keeps its own NUL, so it
    // must still be returned. The point of this case is that the scan stops at
    // the blob end instead of running off it looking for the double NUL.
    auto blob = MakeWideBlob({L"C:\\a.mp3"});
    blob.resize(blob.size() - sizeof(wchar_t));
    auto paths = ParseDropFilesBlob(blob.data(), blob.size());
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"C:\\a.mp3");
}

// ReadHdropPaths - the production path
// ----------------------------------------------
//
// The cases above exercise ParseDropFilesBlob, which no drop ever reaches.
// ReadHdropPaths is what IDropTarget actually calls, and it hands the medium to
// DragQueryFileW instead of walking the blob itself, so none of the coverage
// above says anything about how it behaves. These cases drive it through a stub
// data object, which is enough because it only ever calls GetData.

namespace {

// Counts its releases so a test can prove ReleaseStgMedium ran. Handed to the
// medium as pUnkForRelease, which also stops ReleaseStgMedium from freeing the
// HGLOBAL, leaving ownership with the test.
class ReleaseCounter final : public IUnknown {
public:
    ULONG releases = 0;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown) {
            *ppv = this;
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override {
        ++releases;
        return 1;
    }
};

// Answers one GetData call with a caller-supplied medium. Only GetData is
// reachable from ReadHdropPaths; the rest of IDataObject would be dead code.
class StubDataObject final : public IDataObject {
public:
    STGMEDIUM answer{};
    HRESULT result = S_OK;
    UINT getDataCalls = 0;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppv = static_cast<IDataObject*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* format, STGMEDIUM* medium) override {
        ++getDataCalls;
        // The real shell honours the requested format; asserting it here keeps a
        // future change to the FORMATETC from silently going untested.
        EXPECT_EQ(format->cfFormat, static_cast<CLIPFORMAT>(CF_HDROP));
        EXPECT_EQ(format->tymed, static_cast<DWORD>(TYMED_HGLOBAL));
        if (FAILED(result)) {
            return result;
        }
        *medium = answer;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return E_NOTIMPL; }
};

// Wraps a DROPFILES blob in an HGLOBAL, which is what an HDROP is. Frees itself,
// since every test here keeps ownership via pUnkForRelease.
class GlobalBlob {
public:
    explicit GlobalBlob(const std::vector<char>& bytes) {
        handle_ = ::GlobalAlloc(GMEM_MOVEABLE, bytes.size());
        if (handle_) {
            if (void* p = ::GlobalLock(handle_)) {
                std::memcpy(p, bytes.data(), bytes.size());
                ::GlobalUnlock(handle_);
            }
        }
    }
    ~GlobalBlob() {
        if (handle_) {
            ::GlobalFree(handle_);
        }
    }
    GlobalBlob(const GlobalBlob&) = delete;
    GlobalBlob& operator=(const GlobalBlob&) = delete;

    HGLOBAL get() const { return handle_; }

private:
    HGLOBAL handle_ = nullptr;
};

}  // namespace

TEST(HdropRead, NullDataObjectYieldsEmptyAndNoHdrop) {
    bool hadHdrop = true;  // seeded true to prove it is written, not just left
    EXPECT_TRUE(ReadHdropPaths(nullptr, &hadHdrop).empty());
    EXPECT_FALSE(hadHdrop);
}

TEST(HdropRead, GetDataFailureMeansNoFileList) {
    StubDataObject data;
    data.result = DV_E_FORMATETC;  // what a drag with no file list answers

    bool hadHdrop = true;
    EXPECT_TRUE(ReadHdropPaths(&data, &hadHdrop).empty());
    EXPECT_FALSE(hadHdrop);
    EXPECT_EQ(data.getDataCalls, 1u);
}

TEST(HdropRead, ForeignTymedIsRejectedWithoutReadingTheUnion) {
    // A hostile or buggy source can answer S_OK with a tymed other than the one
    // requested. hGlobal shares a union with pstm and lpszFileName, so treating
    // this as an HDROP would hand shell32 an IStream pointer to parse as a
    // DROPFILES header. hadHdrop must stay false: a medium that cannot be read is
    // not a file list the page can be promised.
    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_ISTREAM;
    data.answer.pstm = nullptr;
    data.answer.pUnkForRelease = &counter;

    bool hadHdrop = true;
    EXPECT_TRUE(ReadHdropPaths(&data, &hadHdrop).empty());
    EXPECT_FALSE(hadHdrop);
    // Rejected, but still released: the early return must not leak the medium.
    EXPECT_EQ(counter.releases, 1u);
}

TEST(HdropRead, EmptyHdropIsDistinguishedFromNoHdrop) {
    // hadHdrop is what the caller turns into hasFiles, so "CF_HDROP present but
    // empty" has to be reported differently from "no file list offered".
    GlobalBlob blob(MakeWideBlob({}));
    ASSERT_NE(blob.get(), nullptr);

    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_HGLOBAL;
    data.answer.hGlobal = blob.get();
    data.answer.pUnkForRelease = &counter;

    bool hadHdrop = false;
    EXPECT_TRUE(ReadHdropPaths(&data, &hadHdrop).empty());
    EXPECT_TRUE(hadHdrop);
    EXPECT_EQ(counter.releases, 1u);
}

TEST(HdropRead, ReadsPathsInOrderAndReleasesMedium) {
    GlobalBlob blob(MakeWideBlob({L"C:\\1.mp3", L"C:\\2.mp3", L"C:\\3.mp3"}));
    ASSERT_NE(blob.get(), nullptr);

    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_HGLOBAL;
    data.answer.hGlobal = blob.get();
    data.answer.pUnkForRelease = &counter;

    bool hadHdrop = false;
    auto paths = ReadHdropPaths(&data, &hadHdrop);
    EXPECT_TRUE(hadHdrop);
    ASSERT_EQ(paths.size(), 3u);
    EXPECT_EQ(paths[0], L"C:\\1.mp3");
    EXPECT_EQ(paths[1], L"C:\\2.mp3");
    EXPECT_EQ(paths[2], L"C:\\3.mp3");
    EXPECT_EQ(counter.releases, 1u);
}

TEST(HdropRead, PathLongerThanMaxPathIsNotTruncated) {
    // DragQueryFileW is asked for each entry's exact length for this reason: a
    // fixed MAX_PATH buffer would silently cut the path and hand back one that
    // points somewhere else.
    std::wstring longPath = L"C:\\";
    longPath += std::wstring(400, L'x');
    longPath += L"\\deep.flac";
    ASSERT_GT(longPath.size(), static_cast<size_t>(MAX_PATH));

    GlobalBlob blob(MakeWideBlob({longPath}));
    ASSERT_NE(blob.get(), nullptr);

    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_HGLOBAL;
    data.answer.hGlobal = blob.get();
    data.answer.pUnkForRelease = &counter;

    bool hadHdrop = false;
    auto paths = ReadHdropPaths(&data, &hadHdrop);
    EXPECT_TRUE(hadHdrop);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], longPath);
    EXPECT_EQ(paths[0].size(), longPath.size());  // no NUL padding left behind
    EXPECT_EQ(counter.releases, 1u);
}

TEST(HdropRead, NonAsciiPathSurvivesTheRealPath) {
    GlobalBlob blob(MakeWideBlob({L"E:\\OST\\千年幸福論\\01.mp3"}));
    ASSERT_NE(blob.get(), nullptr);

    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_HGLOBAL;
    data.answer.hGlobal = blob.get();
    data.answer.pUnkForRelease = &counter;

    bool hadHdrop = false;
    auto paths = ReadHdropPaths(&data, &hadHdrop);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], L"E:\\OST\\千年幸福論\\01.mp3");
    EXPECT_EQ(counter.releases, 1u);
}

TEST(HdropRead, NullHadHdropIsAccepted) {
    // DragOver passes nullptr when it only wants the paths.
    GlobalBlob blob(MakeWideBlob({L"C:\\a.mp3"}));
    ASSERT_NE(blob.get(), nullptr);

    ReleaseCounter counter;
    StubDataObject data;
    data.answer.tymed = TYMED_HGLOBAL;
    data.answer.hGlobal = blob.get();
    data.answer.pUnkForRelease = &counter;

    auto paths = ReadHdropPaths(&data, nullptr);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(counter.releases, 1u);
}
