#pragma once

#include "media/ContainerReader.h"

namespace media::detail {

// 只填配置记录可证明的字段；不识别的配置版本不猜 codecs，截断记录抛 ContainerError。
void ReadCodecConfiguration(api::media::ContainerTrack& track, std::string_view kind, View data);
// 保留 Matroska 的原生 codec 标识，浏览器所需的 codecs 另由 CodecPrivate 推导。
void ReadMatroskaCodec(api::media::ContainerTrack& track, View privateData);

} // namespace media::detail
