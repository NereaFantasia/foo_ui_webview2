#pragma once
// ============================================
// TrackWireSnapshot.h - library.query / library.search 曲目 JSON 的纯值层
// ============================================
//
// 热路径序列化拆成两截：fb2k 侧（LibraryApi.cpp）把每曲抽成全 plain 值的快照
// —— 无 SDK 类型、无服务指针、无线程约束；本文件只负责把快照写成 UTF-8 JSON。
// 分层的用处是字段名与取值语义能在无 fb2k 宿主的单测里对着 nlohmann DOM 对拍，
// SDK 调用与线程放置的约束全留在提取侧。
//
// 字段集与取值语义逐字段对齐 DOM 版本 BuildTrackJsonFromSnapshot：info 容器
// 有效时 19 键，无效时退到 8 键（index/title/artist/album/duration/path/
// absolutePath/rating）。键序与 DOM 产物不同（nlohmann 对象按键名排序输出），
// 契约上只承诺 parse 回来语义相等，不承诺字节等价。
//
// 调用方传 fields 时走投影写法（WriteTrackJsonProjected）：每行恰好输出请求的那
// 几键，损坏条目也照样出全部请求键。省略 fields 的全字段路径走 WriteTrackJson，
// 形状（含 8 键 fallback）不受投影面影响 —— 两支的 19/8 键形状是既有契约。
//
// 入参字符串必须已是合法 UTF-8（JsonWriter.h 的约定）：标签值、codec 等来源
// 不可信的串由提取侧先过 StringUtils::SafeUtf8，否则坏字节会直接进 wire。

#include "utils/JsonWriter.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// 单曲的线路形态快照。字段名与 JSON 键同名，便于并排核对。
struct TrackWireSnapshot {
    size_t index = 0;              // 输出数组内的位置（query = 0 起，search = offset 起）
    std::string title;
    std::string artist;
    std::string album;
    std::string albumArtist;
    std::string genre;
    std::string date;
    int trackNumber = 0;
    int discNumber = 0;
    double duration = 0.0;         // 秒；info.get_length()
    std::string path;              // fb2k 逻辑路径
    std::string absolutePath;      // 原生文件系统路径
    int64_t fileSize = 0;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;
    std::string codec;
    uint32_t subsong = 0;
    int rating = 0;                // 0-5
    bool hasInfo = true;           // false = info 容器无效，只出 8 键
};

// ============================================
// 字段投影面（library.query / library.search 的 fields 参数）
// ============================================
//
// 掩码而非字段名集合：每行序列化要对 19 个字段各判一次"要不要出"，位测试是常数
// 时间且无分配；名字只在解析入口出现一次。位序 = 输出键序 = WriteTrackJson 的
// 声明序，三者同源于下面这张表，加字段只需改一处。

namespace TrackField {

constexpr uint32_t kIndex        = 1u << 0;
constexpr uint32_t kTitle        = 1u << 1;
constexpr uint32_t kArtist       = 1u << 2;
constexpr uint32_t kAlbum        = 1u << 3;
constexpr uint32_t kAlbumArtist  = 1u << 4;
constexpr uint32_t kGenre        = 1u << 5;
constexpr uint32_t kDate         = 1u << 6;
constexpr uint32_t kTrackNumber  = 1u << 7;
constexpr uint32_t kDiscNumber   = 1u << 8;
constexpr uint32_t kDuration     = 1u << 9;
constexpr uint32_t kPath         = 1u << 10;
constexpr uint32_t kAbsolutePath = 1u << 11;
constexpr uint32_t kFileSize     = 1u << 12;
constexpr uint32_t kBitrate      = 1u << 13;
constexpr uint32_t kSampleRate   = 1u << 14;
constexpr uint32_t kChannels     = 1u << 15;
constexpr uint32_t kCodec        = 1u << 16;
constexpr uint32_t kSubsong      = 1u << 17;
constexpr uint32_t kRating       = 1u << 18;

struct Entry {
    const char* name;
    uint32_t bit;
};

// 白名单：名字精确匹配、大小写敏感（拼错不静默忽略，走 fields 校验的未知名分支）。
inline constexpr Entry kTable[] = {
    {"index",        kIndex},
    {"title",        kTitle},
    {"artist",       kArtist},
    {"album",        kAlbum},
    {"albumArtist",  kAlbumArtist},
    {"genre",        kGenre},
    {"date",         kDate},
    {"trackNumber",  kTrackNumber},
    {"discNumber",   kDiscNumber},
    {"duration",     kDuration},
    {"path",         kPath},
    {"absolutePath", kAbsolutePath},
    {"fileSize",     kFileSize},
    {"bitrate",      kBitrate},
    {"sampleRate",   kSampleRate},
    {"channels",     kChannels},
    {"codec",        kCodec},
    {"subsong",      kSubsong},
    {"rating",       kRating},
};

constexpr uint32_t ComputeAllMask() {
    uint32_t mask = 0;
    for (const auto& entry : kTable) {
        mask |= entry.bit;
    }
    return mask;
}

// 全字段集 = 省略 fields 时的掩码。从表算出而非写死，加字段时不会漏更新。
inline constexpr uint32_t kAll = ComputeAllMask();

// 名字 → 位；白名单外返回 0。
inline uint32_t Lookup(std::string_view name) {
    for (const auto& entry : kTable) {
        if (name == entry.name) {
            return entry.bit;
        }
    }
    return 0;
}

// 掩码里的字段数。序列化侧用它按比例预留缓冲。
inline size_t Count(uint32_t mask) {
    size_t count = 0;
    for (const auto& entry : kTable) {
        if (mask & entry.bit) {
            ++count;
        }
    }
    return count;
}

}  // namespace TrackField

// fields 参数的解析结果。valid=false 时 errorMessage 恒非空，unknownFields 仅在
// "白名单外字段名"这一类填充（其余类别为空）。
struct TrackFieldSelection {
    uint32_t mask = TrackField::kAll;
    bool projected = false;  // true = 调用方显式传了 fields，走投影写法
    bool valid = true;
    std::string errorMessage;
    std::vector<std::string> unknownFields;
};

// 从 params 解析 fields。fail-closed：形状不对一律判负，不猜调用方意图。
//
// 语义（contract）：
//   · 缺 fields 键 = 全字段现状（projected=false，mask=kAll）
//   · 显式 null / 非数组 / 空数组 / 含非字符串元素 → valid=false
//   · 含白名单外名字 → valid=false，unknownFields 列出全部未知名（扫完再报，
//     调用方一次就能改对，不用逐个试）
//   · 重复名字去重（同一位 OR 两次即幂等）
inline TrackFieldSelection ParseTrackFieldSelection(const nlohmann::json& params) {
    TrackFieldSelection selection;
    if (!params.contains("fields")) {
        return selection;
    }

    const nlohmann::json& raw = params.at("fields");
    // 显式 null 也落这一支：null 不是数组
    if (!raw.is_array()) {
        selection.valid = false;
        selection.errorMessage = "fields must be an array of field names";
        return selection;
    }
    if (raw.empty()) {
        selection.valid = false;
        selection.errorMessage = "fields must not be an empty array";
        return selection;
    }

    uint32_t mask = 0;
    for (const nlohmann::json& entry : raw) {
        if (!entry.is_string()) {
            selection.valid = false;
            selection.errorMessage = "fields must contain only field name strings";
            selection.unknownFields.clear();
            return selection;
        }
        const std::string& name = entry.get_ref<const std::string&>();
        const uint32_t bit = TrackField::Lookup(name);
        if (bit == 0) {
            if (std::find(selection.unknownFields.begin(), selection.unknownFields.end(), name) ==
                selection.unknownFields.end()) {
                selection.unknownFields.push_back(name);
            }
            continue;
        }
        mask |= bit;
    }

    if (!selection.unknownFields.empty()) {
        selection.valid = false;
        selection.errorMessage = "fields contains unknown field names";
        return selection;
    }

    selection.mask = mask;
    selection.projected = true;
    return selection;
}

// 把一条快照追加成 JSON 对象文本（含首尾花括号）。不清空调用方缓冲。
inline void WriteTrackJson(std::string& out, const TrackWireSnapshot& snap) {
    out.append("{\"index\":");
    JsonWriter::AppendJsonInt(out, static_cast<int64_t>(snap.index));

    if (!snap.hasInfo) {
        // info 容器无效的损坏条目：键集与 DOM 版本的 fallback 分支一致（8 键）。
        out.append(",\"title\":");
        JsonWriter::AppendJsonString(out, snap.title);
        out.append(",\"artist\":");
        JsonWriter::AppendJsonString(out, snap.artist);
        out.append(",\"album\":");
        JsonWriter::AppendJsonString(out, snap.album);
        out.append(",\"duration\":");
        JsonWriter::AppendJsonNumber(out, snap.duration);
        out.append(",\"path\":");
        JsonWriter::AppendJsonString(out, snap.path);
        out.append(",\"absolutePath\":");
        JsonWriter::AppendJsonString(out, snap.absolutePath);
        out.append(",\"rating\":");
        JsonWriter::AppendJsonInt(out, snap.rating);
        out.push_back('}');
        return;
    }

    out.append(",\"title\":");
    JsonWriter::AppendJsonString(out, snap.title);
    out.append(",\"artist\":");
    JsonWriter::AppendJsonString(out, snap.artist);
    out.append(",\"album\":");
    JsonWriter::AppendJsonString(out, snap.album);
    out.append(",\"albumArtist\":");
    JsonWriter::AppendJsonString(out, snap.albumArtist);
    out.append(",\"genre\":");
    JsonWriter::AppendJsonString(out, snap.genre);
    out.append(",\"date\":");
    JsonWriter::AppendJsonString(out, snap.date);
    out.append(",\"trackNumber\":");
    JsonWriter::AppendJsonInt(out, snap.trackNumber);
    out.append(",\"discNumber\":");
    JsonWriter::AppendJsonInt(out, snap.discNumber);
    out.append(",\"duration\":");
    JsonWriter::AppendJsonNumber(out, snap.duration);
    out.append(",\"path\":");
    JsonWriter::AppendJsonString(out, snap.path);
    out.append(",\"absolutePath\":");
    JsonWriter::AppendJsonString(out, snap.absolutePath);
    out.append(",\"fileSize\":");
    JsonWriter::AppendJsonInt(out, snap.fileSize);
    out.append(",\"bitrate\":");
    JsonWriter::AppendJsonInt(out, snap.bitrate);
    out.append(",\"sampleRate\":");
    JsonWriter::AppendJsonInt(out, snap.sampleRate);
    out.append(",\"channels\":");
    JsonWriter::AppendJsonInt(out, snap.channels);
    out.append(",\"codec\":");
    JsonWriter::AppendJsonString(out, snap.codec);
    out.append(",\"subsong\":");
    JsonWriter::AppendJsonInt(out, snap.subsong);
    out.append(",\"rating\":");
    JsonWriter::AppendJsonInt(out, snap.rating);
    out.push_back('}');
}

// 把 8 键 fallback 不产出的那 11 个字段归零到类型默认（字符串 ""、整型 0）。
//
// 只投影模式需要：损坏条目在投影下同样要出全部请求键，值取类型默认。序列化侧的
// 快照对象在行循环外复用（省分配），不归零就会把上一行的 genre/codec 等带进这一
// 行，成为无法从输出反查的串值。index/path/absolutePath/rating 是 fallback 自己
// 产出的真值，不在此列。
inline void ResetFieldsAbsentFromFallback(TrackWireSnapshot& snap) {
    snap.albumArtist.clear();
    snap.genre.clear();
    snap.date.clear();
    snap.trackNumber = 0;
    snap.discNumber = 0;
    snap.fileSize = 0;
    snap.bitrate = 0;
    snap.sampleRate = 0;
    snap.channels = 0;
    snap.codec.clear();
    snap.subsong = 0;
}

// 投影写法：只输出掩码选中的键，键序与 WriteTrackJson 的声明序一致。
//
// 与全字段路径的两点差别：
//   · 不看 snap.hasInfo —— 损坏条目也出全部请求键（契约是"请求键必在"），调用方
//     负责先把 fallback 不产出的字段归零（ResetFieldsAbsentFromFallback）；
//   · mask == TrackField::kAll 时产物与 WriteTrackJson 的全字段支逐字节相同（同序
//     同原语），这是两支不漂移的最省事保证，不是对外承诺的契约。
inline void WriteTrackJsonProjected(std::string& out, const TrackWireSnapshot& snap,
                                    uint32_t mask) {
    out.push_back('{');

    bool first = true;
    // 键名全是 ASCII 标识符，不过转义原语
    auto appendKey = [&out, &first](const char* name) {
        if (!first) {
            out.push_back(',');
        }
        first = false;
        out.push_back('"');
        out.append(name);
        out.append("\":", 2);
    };

    if (mask & TrackField::kIndex) {
        appendKey("index");
        JsonWriter::AppendJsonInt(out, static_cast<int64_t>(snap.index));
    }
    if (mask & TrackField::kTitle) {
        appendKey("title");
        JsonWriter::AppendJsonString(out, snap.title);
    }
    if (mask & TrackField::kArtist) {
        appendKey("artist");
        JsonWriter::AppendJsonString(out, snap.artist);
    }
    if (mask & TrackField::kAlbum) {
        appendKey("album");
        JsonWriter::AppendJsonString(out, snap.album);
    }
    if (mask & TrackField::kAlbumArtist) {
        appendKey("albumArtist");
        JsonWriter::AppendJsonString(out, snap.albumArtist);
    }
    if (mask & TrackField::kGenre) {
        appendKey("genre");
        JsonWriter::AppendJsonString(out, snap.genre);
    }
    if (mask & TrackField::kDate) {
        appendKey("date");
        JsonWriter::AppendJsonString(out, snap.date);
    }
    if (mask & TrackField::kTrackNumber) {
        appendKey("trackNumber");
        JsonWriter::AppendJsonInt(out, snap.trackNumber);
    }
    if (mask & TrackField::kDiscNumber) {
        appendKey("discNumber");
        JsonWriter::AppendJsonInt(out, snap.discNumber);
    }
    if (mask & TrackField::kDuration) {
        appendKey("duration");
        JsonWriter::AppendJsonNumber(out, snap.duration);
    }
    if (mask & TrackField::kPath) {
        appendKey("path");
        JsonWriter::AppendJsonString(out, snap.path);
    }
    if (mask & TrackField::kAbsolutePath) {
        appendKey("absolutePath");
        JsonWriter::AppendJsonString(out, snap.absolutePath);
    }
    if (mask & TrackField::kFileSize) {
        appendKey("fileSize");
        JsonWriter::AppendJsonInt(out, snap.fileSize);
    }
    if (mask & TrackField::kBitrate) {
        appendKey("bitrate");
        JsonWriter::AppendJsonInt(out, snap.bitrate);
    }
    if (mask & TrackField::kSampleRate) {
        appendKey("sampleRate");
        JsonWriter::AppendJsonInt(out, snap.sampleRate);
    }
    if (mask & TrackField::kChannels) {
        appendKey("channels");
        JsonWriter::AppendJsonInt(out, snap.channels);
    }
    if (mask & TrackField::kCodec) {
        appendKey("codec");
        JsonWriter::AppendJsonString(out, snap.codec);
    }
    if (mask & TrackField::kSubsong) {
        appendKey("subsong");
        JsonWriter::AppendJsonInt(out, snap.subsong);
    }
    if (mask & TrackField::kRating) {
        appendKey("rating");
        JsonWriter::AppendJsonInt(out, snap.rating);
    }

    out.push_back('}');
}
