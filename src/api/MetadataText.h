#pragma once

#include "api/generated/MetadataSchema.h"
#include "utils/StringUtils.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace metadata_text {

enum class MetadataNameCase {
    Preserve,
    UpperAscii,
};

inline std::string MetadataFieldName(const char* name, MetadataNameCase nameCase) {
    std::string result = name ? name : "";
    if (nameCase == MetadataNameCase::UpperAscii) {
        for (char& value : result) {
            if (value >= 'a' && value <= 'z') {
                value = static_cast<char>(value - 'a' + 'A');
            }
        }
    }
    return result;
}

template <typename Info>
std::map<std::string, nlohmann::json> CollectMetadataTags(
    const Info& info, MetadataNameCase nameCase) {
    std::map<std::string, nlohmann::json> tags;
    for (size_t i = 0; i < info.meta_get_count(); ++i) {
        const char* rawName = info.meta_enum_name(i);
        // 替换坏名字会让不同字段变成同一个 JSON 键，因此省略整项。
        if (!StringUtils::IsValidUtf8(rawName)) {
            continue;
        }
        const auto name = MetadataFieldName(rawName, nameCase);
        const size_t valueCount = info.meta_enum_value_count(i);
        if (valueCount == 1) {
            tags[name] = StringUtils::SafeUtf8(info.meta_enum_value(i, 0));
        } else {
            auto values = nlohmann::json::array();
            for (size_t j = 0; j < valueCount; ++j) {
                values.push_back(StringUtils::SafeUtf8(info.meta_enum_value(i, j)));
            }
            tags[name] = std::move(values);
        }
    }
    return tags;
}

template <typename Info>
std::map<std::string, nlohmann::json> CollectTechnicalFields(
    const Info& info, MetadataNameCase nameCase) {
    std::map<std::string, nlohmann::json> fields;
    for (size_t i = 0; i < info.info_get_count(); ++i) {
        const char* rawName = info.info_enum_name(i);
        if (!StringUtils::IsValidUtf8(rawName)) {
            continue;
        }
        const auto name = MetadataFieldName(rawName, nameCase);
        fields[name] = StringUtils::SafeUtf8(info.info_enum_value(i));
    }
    return fields;
}

template <typename Info>
std::map<std::string, nlohmann::json> CollectFlatMetadata(
    const Info& info, double duration, std::optional<std::uint64_t> fileSize) {
    auto fields = CollectMetadataTags(info, MetadataNameCase::UpperAscii);
    for (auto& [name, value] : CollectTechnicalFields(info, MetadataNameCase::UpperAscii)) {
        fields.insert_or_assign(name, std::move(value));
    }

    char durationText[32];
    std::snprintf(durationText, sizeof(durationText), "%.3f", duration);
    fields["DURATION"] = durationText;
    if (fileSize) {
        fields["FILESIZE"] = std::to_string(*fileSize);
    }
    return fields;
}

inline api::metadata::TrackTechnicalInfo MakeTechnicalInfo(
    double duration, std::int64_t bitrate, std::int64_t sampleRate,
    std::int64_t channels, const char* codec) {
    return {duration, bitrate, sampleRate, channels, StringUtils::SafeUtf8(codec)};
}

} // namespace metadata_text
