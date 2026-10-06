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
// 全字段的键集就是声明的媒体库曲目行 LibraryTrack（src/api/schema/library.ts）的
// 全部键，单测拿写出的键集与生成的 kFields 核对。取不到 info 容器的行照样出全部键，
// 取自 info 的字段由提取侧归零到类型默认（ResetInfoFields），与 BuildTrackRow 对这种
// 句柄的处理一致。键序不进契约，只承诺 parse 回来语义相等。
//
// artists 是 artist 的原子值数组并列形态：artists.join(", ") 与 artist 逐字节
// 相等（取值侧由 MetaValuesRaw + JoinMetaValues 保证）；albumArtists 与 albumArtist 同理。
//
// 调用方传 fields 时走投影写法（WriteTrackJsonProjected）：每行恰好输出请求的那
// 几键。省略 fields 等于请求全部键。
//
// 入参字符串必须已是合法 UTF-8（JsonWriter.h 的约定）：标签值、codec 等来源
// 不可信的串由提取侧先过 StringUtils::SafeUtf8，否则坏字节会直接进 wire。

#include "utils/JsonWriter.h"
#include "api/ApiResult.h"
#include "api/ErrorEnvelope.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 单曲的线路形态快照。字段名与 JSON 键同名，便于并排核对。
struct TrackWireSnapshot {
    size_t index = 0;              // 输出数组内的位置（query = 0 起，search = offset 起）
    std::string handle;            // absolutePath，子曲目不为 0 时带 "|subsong:N"
    std::string title;
    std::string artist;
    std::vector<std::string> artists;  // artist 的原子值数组；join(", ") 即 artist
    std::string album;
    std::string albumArtist;
    std::vector<std::string> albumArtists;  // album artist 的原子值数组；join(", ") 即 albumArtist
    std::string genre;
    std::string date;
    int trackNumber = 0;
    int discNumber = 0;
    double duration = 0.0;         // 秒；info.get_length()
    std::string path;              // fb2k 逻辑路径
    std::string absolutePath;      // 原生文件系统路径；没有原生形式时为存储路径
    int64_t fileSize = 0;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;
    std::string codec;
    uint32_t subsong = 0;
    int rating = 0;                // 0-5
};

// ============================================
// 字段投影面（library.query / library.search 的 fields 参数）
// ============================================
//
// 掩码而非字段名集合：每行序列化要对每个字段各判一次"要不要出"，位测试是常数
// 时间且无分配；名字只在解析入口出现一次。位序 = 输出键序 = 下面这张表的顺序，
// 加字段只需改一处。
//
// 位值不落盘、不上线路（每次都从 fields 名字重新解析），因此在中间插字段、让后面
// 的位顺延没有外部影响 —— 表序要跟着输出键序走，不迁就历史位号。

namespace TrackField {

constexpr uint32_t kIndex        = 1u << 0;
constexpr uint32_t kHandle       = 1u << 1;
constexpr uint32_t kTitle        = 1u << 2;
constexpr uint32_t kArtist       = 1u << 3;
constexpr uint32_t kArtists      = 1u << 4;
constexpr uint32_t kAlbum        = 1u << 5;
constexpr uint32_t kAlbumArtist  = 1u << 6;
constexpr uint32_t kAlbumArtists = 1u << 7;
constexpr uint32_t kGenre        = 1u << 8;
constexpr uint32_t kDate         = 1u << 9;
constexpr uint32_t kTrackNumber  = 1u << 10;
constexpr uint32_t kDiscNumber   = 1u << 11;
constexpr uint32_t kDuration     = 1u << 12;
constexpr uint32_t kPath         = 1u << 13;
constexpr uint32_t kAbsolutePath = 1u << 14;
constexpr uint32_t kFileSize     = 1u << 15;
constexpr uint32_t kBitrate      = 1u << 16;
constexpr uint32_t kSampleRate   = 1u << 17;
constexpr uint32_t kChannels     = 1u << 18;
constexpr uint32_t kCodec        = 1u << 19;
constexpr uint32_t kSubsong      = 1u << 20;
constexpr uint32_t kRating       = 1u << 21;

struct Entry {
    const char* name;
    uint32_t bit;
};

// 白名单：名字精确匹配、大小写敏感（拼错不静默忽略，走 fields 校验的未知名分支）。
inline constexpr Entry kTable[] = {
    {"index",        kIndex},
    {"handle",       kHandle},
    {"title",        kTitle},
    {"artist",       kArtist},
    {"artists",      kArtists},
    {"album",        kAlbum},
    {"albumArtist",  kAlbumArtist},
    {"albumArtists", kAlbumArtists},
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

// 已由声明的参数读取器校验过形状的 fields（字符串数组、至少一项；省略与 null 都是
// nullopt）：这里只查名字。
//
// 语义（contract）：
//   · nullopt = 全字段（projected=false，mask=kAll）
//   · 含白名单外名字 → valid=false，unknownFields 列出全部未知名（扫完再报，
//     调用方一次就能改对，不用逐个试）
//   · 重复名字去重（同一位 OR 两次即幂等）
inline TrackFieldSelection ParseTrackFieldSelection(
    const std::optional<std::vector<std::string>>& fields) {
    TrackFieldSelection selection;
    if (!fields) {
        return selection;
    }

    uint32_t mask = 0;
    for (const std::string& name : *fields) {
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

// fields 校验失败：INVALID_PARAMS，有未知名时全部列在 details.unknownFields，拼写错误
// 不静默丢字段，也不用逐个试。library.query / library.search / playlist.getTracks 共用
// 这一份；放在本头是因为白名单与解析器都在这里，ApiResult.h 与 ErrorEnvelope.h 也是
// fb2k-free，本头仍是纯值层。
inline api::Failure TrackFieldsFailure(const TrackFieldSelection& fields) {
    json::object_t extra;
    if (!fields.unknownFields.empty()) {
        extra["details"] = json{{"unknownFields", fields.unknownFields}};
    }
    return api::Fail(fields.errorMessage, ApiErrorCode::INVALID_PARAMS, std::move(extra));
}

// 同一失败的响应体，给自己回包的 deferred handler（library.query / library.search）用。
// 它是 resolve 出的 success:false 正常响应体。不用 DeferredResponder::SendError：那是框架
// 错误信封通道（BridgeCore.h 中 SendError 的注释明文只给框架兜底用），页面侧收到 error
// 字段会把 Promise reject 掉（webview/BridgeBootstrapScript.inl 注入脚本的 _handleResponse），而本仓库所有
// 参数校验失败都是 resolve 出 success:false。
inline json MakeTrackFieldsErrorBody(const TrackFieldSelection& fields) {
    return api::results::FailureToJson(TrackFieldsFailure(fields));
}

// 把取自 info 容器的字段归零到类型默认（字符串 ""、空数组、数值 0）。
//
// 取不到 info 的行要用它：序列化侧的快照对象在行循环外复用（省分配），不归零就会把
// 上一行的 genre、codec 等带进这一行，成为无法从输出反查的串值。index、handle、
// path、absolutePath、fileSize、subsong 取自句柄，rating 由调用方给出，都不在此列。
inline void ResetInfoFields(TrackWireSnapshot& snap) {
    snap.title.clear();
    snap.artist.clear();
    snap.artists.clear();
    snap.album.clear();
    snap.albumArtist.clear();
    snap.albumArtists.clear();
    snap.genre.clear();
    snap.date.clear();
    snap.trackNumber = 0;
    snap.discNumber = 0;
    snap.duration = 0.0;
    snap.bitrate = 0;
    snap.sampleRate = 0;
    snap.channels = 0;
    snap.codec.clear();
}

// 投影写法：只输出掩码选中的键，键序是 TrackField::kTable 的顺序。
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
    if (mask & TrackField::kHandle) {
        appendKey("handle");
        JsonWriter::AppendJsonString(out, snap.handle);
    }
    if (mask & TrackField::kTitle) {
        appendKey("title");
        JsonWriter::AppendJsonString(out, snap.title);
    }
    if (mask & TrackField::kArtist) {
        appendKey("artist");
        JsonWriter::AppendJsonString(out, snap.artist);
    }
    if (mask & TrackField::kArtists) {
        appendKey("artists");
        JsonWriter::AppendJsonStringArray(out, snap.artists);
    }
    if (mask & TrackField::kAlbum) {
        appendKey("album");
        JsonWriter::AppendJsonString(out, snap.album);
    }
    if (mask & TrackField::kAlbumArtist) {
        appendKey("albumArtist");
        JsonWriter::AppendJsonString(out, snap.albumArtist);
    }
    if (mask & TrackField::kAlbumArtists) {
        appendKey("albumArtists");
        JsonWriter::AppendJsonStringArray(out, snap.albumArtists);
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

// 全字段写法：一行完整的媒体库曲目行。
inline void WriteTrackJson(std::string& out, const TrackWireSnapshot& snap) {
    WriteTrackJsonProjected(out, snap, TrackField::kAll);
}
