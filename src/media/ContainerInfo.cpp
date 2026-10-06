#include "pch.h"
#if __has_include("compat/fb2k_types.h")
#include "compat/fb2k_types.h"
#endif
#include "media/ContainerReader.h"

namespace media {

api::media::GetContainerInfoResult InspectContainer(std::uint64_t size, ContainerRead read) {
    detail::Reader reader(size, std::move(read));
    if (size < 4) return {};
    const auto magic = reader.Read(0, static_cast<std::size_t>(size < 8 ? size : 8));
    if (detail::Unsigned(magic, 0, 4) == 0x1a45dfa3) return detail::ReadMatroska(reader);
    if (magic.size() < 8) return {};
    const auto type = detail::String(detail::View(magic).subspan(4));
    if (type == "ftyp" || type == "moov" || type == "mdat" || type == "wide" || type == "free" || type == "skip") {
        return detail::ReadMp4(reader);
    }
    return {};
}

} // namespace media
