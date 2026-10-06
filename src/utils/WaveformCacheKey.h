// WaveformCacheKey.h - 整轨波形缓存的键拼接
//
/* 键长随规范化路径变化，不能用定长缓冲：若用 sprintf_s 拼进 512 字节的栈数组，
 * 路径的 UTF-8 超过约 466 字节（约 150 个汉字）就装不下——按 UCRT 行为，未装
 * 无效参数处理器时直接 fast-fail 终止进程；装了且处理器返回，则键退化成
 * 空串，所有超长路径共用一个键、互相串波形。
 *
 * 键只在进程内的 std::map 里做查找，不落盘、不跨版本，格式可以改。键里没有
 * method / scale / signed：缓存存的是一次解码得到的三组原始窗口值，任何一种
 * 请求形态都由同一条目现算，所以这些参数不区分条目。
 */
#pragma once

#include <cstdint>
#include <string>

namespace waveform_cache {

// 文件大小与修改时间进键，是为了让文件被改写后的请求不命中旧波形。
inline std::string MakeKey(const std::string& canonicalPath, uint32_t subsong, int resolution,
                           uint64_t fileSize, uint64_t modifiedTime) {
    std::string key;
    // 四个数字字段最长 10 + 11 + 20 + 20 字节，加四个分隔符共 65 字节；留 72 让拼接途中不扩容。
    key.reserve(canonicalPath.size() + 72);
    key += canonicalPath;
    key += '|';
    key += std::to_string(subsong);
    key += '|';
    key += std::to_string(resolution);
    key += '|';
    key += std::to_string(fileSize);
    key += '|';
    key += std::to_string(modifiedTime);
    return key;
}

}  // namespace waveform_cache
