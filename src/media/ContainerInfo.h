#pragma once

#include "api/generated/MediaSchema.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace media {

// offset 为文件起点后的字节偏移；回调须读满指定长度或抛异常，解析器不接受短读。
using ContainerRead = std::function<std::vector<std::uint8_t>(std::uint64_t, std::size_t)>;

class ContainerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 同步解析整份容器的元数据，不解码媒体；调用方负责后台调度和文件身份复查。
// 未识别返回 recognized:false；读取错误直接传播，已识别结构损坏或超预算抛 ContainerError。
api::media::GetContainerInfoResult InspectContainer(std::uint64_t size, ContainerRead read);

} // namespace media
