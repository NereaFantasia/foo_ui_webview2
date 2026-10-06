// FrontendDirectoryPolicy.cpp
#include "pch.h"
#include "core/FrontendDirectoryPolicy.h"

#include <array>
#include <utility>

namespace frontend_directory_policy {

bool HasIndexHtml(const std::wstring& directory) {
    if (directory.empty()) return false;
    const std::wstring indexPath = directory + L"\\index.html";
    const DWORD attrs = GetFileAttributesW(indexPath.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

Resolution Resolve(const Candidates& candidates) {
    const std::array<std::pair<Source, const std::wstring*>, 4> order{{
        {Source::PanelTemplate, &candidates.panelTemplate},
        {Source::ActiveTemplate, &candidates.activeTemplate},
        {Source::Bundled, &candidates.bundled},
        {Source::DefaultTemplate, &candidates.defaultTemplate},
    }};
    for (const auto& [source, directory] : order) {
        if (HasIndexHtml(*directory)) return {source, *directory};
    }
    return {};
}

}  // namespace frontend_directory_policy
