#include "pch.h"
#include "utils/PlaylistFormatUtils.h"

namespace {

const PlaylistFormatUtils::ExtensionSet& GetPlaylistWrapperExtensions() {
    static const PlaylistFormatUtils::ExtensionSet extensions = [] {
        PlaylistFormatUtils::ExtensionSet result;
        for (const std::string_view builtIn : PlaylistFormatUtils::kBuiltInExtensions) {
            result.emplace(builtIn);
        }

        for (const auto& loader : playlist_loader::enumerate()) {
            const char* extension = loader->get_extension();
            if (extension == nullptr || *extension == '\0') {
                continue;
            }

            std::string normalized = PlaylistFormatUtils::NormalizeExtension(extension);
            if (normalized.size() > 1) {
                result.emplace(std::move(normalized));
            }
        }
        return result;
    }();
    return extensions;
}

} // namespace

namespace PlaylistFormatUtils {

bool LooksLikePlaylistWrapper(std::string_view location) {
    return LooksLikePlaylistWrapper(location, GetPlaylistWrapperExtensions());
}

} // namespace PlaylistFormatUtils
