#pragma once

#include "media/MediaTokens.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>

namespace media {
namespace detail {
inline std::atomic<bool> ioStopping{false};
}

class FileError : public std::runtime_error {
public:
    FileError(std::string reason, unsigned long code);
    const std::string reason;
    const unsigned long code;
};

// 元数据和正文使用同一句柄；Read 会移动文件指针，同一对象不能并发读取。
// 允许其他进程写入和替换文件，因此调用方须在读取前后核对 Identity。
class MediaFile {
public:
    explicit MediaFile(const std::wstring& path);
    ~MediaFile();
    MediaFile(const MediaFile&) = delete;
    MediaFile& operator=(const MediaFile&) = delete;
    FileIdentity Identity() const;
    std::wstring FinalPath() const;
    std::vector<std::uint8_t> Read(std::uint64_t offset, std::size_t length) const;
    // 仅探测有限的文件头；不能识别时返回 application/octet-stream，不保证浏览器可解码。
    std::string MimeType() const;

private:
    struct Handle;
    std::unique_ptr<Handle> handle_;
};

// 在主线程提交。task 在 worker 执行；cancelled 在主线程处理排队任务被取消或 task 抛异常。
// 返回 false 表示任务未入队，此时两个回调都不会执行，由调用方完成响应。
// 分配或线程创建失败仍可抛异常；cancelled 的再次投递也可能失败，不能保证错误响应必达。
bool QueueMediaIo(std::function<void()> task, std::function<void()> cancelled = {});
// 在主线程停止入队并等待 worker；底层驱动的同步 IO 没有可保证的退出时限。
void StopMediaIo();

}  // namespace media
