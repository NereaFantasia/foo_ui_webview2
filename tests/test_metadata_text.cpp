#include "pch.h"
#include "compat/fb2k_types.h"
#include "api/MetadataText.h"

#include <map>
#include <optional>
#include <utility>

namespace {
using nlohmann::json;
namespace md = api::metadata;
using metadata_text::MetadataNameCase;

const std::string kReplacement = "\xEF\xBF\xBD";

struct FakeInfo {
    std::vector<std::pair<std::string, std::vector<std::string>>> tags;
    std::vector<std::pair<std::string, std::string>> fields;

    size_t meta_get_count() const { return tags.size(); }
    const char* meta_enum_name(size_t index) const { return tags.at(index).first.c_str(); }
    size_t meta_enum_value_count(size_t index) const { return tags.at(index).second.size(); }
    const char* meta_enum_value(size_t index, size_t valueIndex) const {
        return tags.at(index).second.at(valueIndex).c_str();
    }
    size_t info_get_count() const { return fields.size(); }
    const char* info_enum_name(size_t index) const { return fields.at(index).first.c_str(); }
    const char* info_enum_value(size_t index) const { return fields.at(index).second.c_str(); }
};

TEST(MetadataText, PreservesTagNamesAndValueShapes) {
    const FakeInfo info{{
        {"Title", {"曲目"}},
        {"EMPTY", {""}},
        {"ZERO", {}},
        {"Artist", {"乙", "", "乙", "甲", " ", "A;B,C"}},
    }, {}};
    const auto tags = metadata_text::CollectMetadataTags(info, MetadataNameCase::Preserve);

    EXPECT_EQ(tags.at("Title").get<std::string>(), "曲目");
    EXPECT_EQ(tags.at("EMPTY").get<std::string>(), "");
    EXPECT_TRUE(tags.at("ZERO").is_array());
    EXPECT_TRUE(tags.at("ZERO").empty());
    EXPECT_EQ(tags.at("Artist").get<std::vector<std::string>>(),
              (std::vector<std::string>{"乙", "", "乙", "甲", " ", "A;B,C"}));
    EXPECT_NO_THROW(json(tags).dump());
}

TEST(MetadataText, OmitsInvalidTagNamesWithoutOverwritingValidReplacementName) {
    const FakeInfo info{{
        {kReplacement, {"valid name"}},
        {"\xB9", {"first invalid name"}},
        {"\xBA", {"second invalid name"}},
        {"TITLE", {"normal"}},
    }, {}};

    for (const auto mode : {MetadataNameCase::Preserve, MetadataNameCase::UpperAscii}) {
        const auto tags = metadata_text::CollectMetadataTags(info, mode);
        EXPECT_EQ(tags.size(), 2u);
        EXPECT_EQ(tags.at(kReplacement).get<std::string>(), "valid name");
        EXPECT_EQ(tags.at("TITLE").get<std::string>(), "normal");
        EXPECT_EQ(tags.count("\xB9"), 0u);
        EXPECT_EQ(tags.count("\xBA"), 0u);
        EXPECT_NO_THROW(json(tags).dump());
    }
}

TEST(MetadataText, SanitizesEachTagValueWithoutChangingOrderOrDuplicates) {
    const FakeInfo info{{
        {"Title", {"曲目\xB9"}},
        {"Artist", {"乙", "\xBA", "", "\xBA", kReplacement}},
    }, {}};

    const auto tags = metadata_text::CollectMetadataTags(info, MetadataNameCase::Preserve);
    EXPECT_EQ(tags.at("Title").get<std::string>(), "曲目" + kReplacement);
    EXPECT_EQ(tags.at("Artist").get<std::vector<std::string>>(),
              (std::vector<std::string>{"乙", kReplacement, "", kReplacement, kReplacement}));
    EXPECT_NO_THROW(json(tags).dump());
}

TEST(MetadataText, FlatNamesUppercaseOnlyAsciiLetters) {
    const FakeInfo info{{{"自定义titleé", {"合法"}}}, {{"技术codecé", "FLAC"}}};
    const auto tags = metadata_text::CollectMetadataTags(info, MetadataNameCase::UpperAscii);
    const auto fields = metadata_text::CollectTechnicalFields(info, MetadataNameCase::UpperAscii);

    EXPECT_EQ(tags.at("自定义TITLEé").get<std::string>(), "合法");
    EXPECT_EQ(fields.at("技术CODECé").get<std::string>(), "FLAC");
    EXPECT_NO_THROW(json(tags).dump());
    EXPECT_NO_THROW(json(fields).dump());
}

TEST(MetadataText, OmitsInvalidTechnicalNamesWithoutOverwritingValidReplacementName) {
    const FakeInfo info{{}, {
        {kReplacement, "valid name"},
        {"\xB9", "first invalid name"},
        {"\xBA", "second invalid name"},
        {"codec", "FLAC\xB9"},
    }};

    for (const auto mode : {MetadataNameCase::Preserve, MetadataNameCase::UpperAscii}) {
        const auto fields = metadata_text::CollectTechnicalFields(info, mode);
        EXPECT_EQ(fields.size(), 2u);
        EXPECT_EQ(fields.at(kReplacement).get<std::string>(), "valid name");
        EXPECT_EQ(fields.at(mode == MetadataNameCase::Preserve ? "codec" : "CODEC").get<std::string>(),
                  "FLAC" + kReplacement);
        EXPECT_EQ(fields.count("\xB9"), 0u);
        EXPECT_EQ(fields.count("\xBA"), 0u);
        EXPECT_NO_THROW(json(fields).dump());
    }
}

TEST(MetadataText, FlatMetadataPreservesTechnicalOverridesAndFormattedFields) {
    const FakeInfo info{{
        {"codec", {"tag codec"}}, {"DURATION", {"tag duration"}}, {"Artist", {"甲", "乙"}},
    }, {{"codec", "FLAC"}, {"DURATION", "technical duration"}}};
    const auto fields = metadata_text::CollectFlatMetadata(info, 215.4, 1024u);

    EXPECT_EQ(fields.at("CODEC").get<std::string>(), "FLAC");
    EXPECT_EQ(fields.at("DURATION").get<std::string>(), "215.400");
    EXPECT_EQ(fields.at("FILESIZE").get<std::string>(), "1024");
    EXPECT_EQ(fields.at("ARTIST").get<std::vector<std::string>>(),
              (std::vector<std::string>{"甲", "乙"}));
    EXPECT_EQ(metadata_text::CollectFlatMetadata(info, 0, std::nullopt).count("FILESIZE"), 0u);
}

TEST(MetadataText, TechnicalInfoSanitizesCodecAndPreservesNumericValues) {
    const auto info = metadata_text::MakeTechnicalInfo(215.4, 1000, 44100, 2, "FLAC\xB9");

    EXPECT_EQ(info.duration, 215.4);
    EXPECT_EQ(info.bitrate, 1000);
    EXPECT_EQ(info.sampleRate, 44100);
    EXPECT_EQ(info.channels, 2);
    EXPECT_EQ(info.codec, "FLAC" + kReplacement);
    EXPECT_NO_THROW(md::ToJson(info).dump());
    EXPECT_EQ(metadata_text::MakeTechnicalInfo(0, 0, 0, 0, nullptr).codec, "");
}

TEST(MetadataText, StructuredReadAndReadRawSerializeCollectedValues) {
    const FakeInfo info{{{"Title", {"\xB9"}}, {"Artist", {"甲", "\xBA"}}}, {}};
    md::ReadResult read;
    read.path = "C:\\music\\sample.flac";
    read.tags = metadata_text::CollectMetadataTags(info, MetadataNameCase::Preserve);
    read.info = metadata_text::MakeTechnicalInfo(30, 1000, 44100, 2, "FLAC\xB9");
    md::ReadRawResult raw;
    raw.path = read.path;
    raw.tags = read.tags;
    raw.info = read.info;
    raw.source = "file";

    EXPECT_NO_THROW(md::ToJson(read).dump());
    EXPECT_NO_THROW(md::ToJson(raw).dump());
    EXPECT_EQ(md::ToJson(read).at("tags").at("Title").get<std::string>(), kReplacement);
    EXPECT_EQ(md::ToJson(raw).at("source").get<std::string>(), "file");
}

TEST(MetadataText, MixedReadBatchKeepsNormalAndDamagedRowsSerializable) {
    const FakeInfo normal{{{"Title", {"正常曲目"}}}, {{"codec", "FLAC"}}};
    const FakeInfo damaged{{{"Title", {"\xB9"}}, {"\xBA", {"omitted"}}}, {{"codec", "FLAC"}}};
    md::MetadataReadBatchItem normalRow;
    normalRow.path = "C:\\music\\normal.flac";
    normalRow.success = true;
    normalRow.tags = metadata_text::CollectFlatMetadata(normal, 10, std::nullopt);
    md::MetadataReadBatchItem damagedRow;
    damagedRow.path = "C:\\music\\damaged.flac";
    damagedRow.success = true;
    damagedRow.tags = metadata_text::CollectFlatMetadata(damaged, 20, std::nullopt);
    md::MetadataReadBatchItem missingRow;
    missingRow.path = "C:\\music\\missing.flac";
    missingRow.error = "Failed to get track info";
    md::ReadBatchResult batch;
    batch.total = 3;
    batch.successCount = 2;
    batch.errorCount = 1;
    batch.results = {normalRow, damagedRow, missingRow};

    const auto payload = md::ToJson(batch);
    EXPECT_NO_THROW(payload.dump());
    EXPECT_EQ(payload.at("results").at(0).at("tags").at("TITLE").get<std::string>(), "正常曲目");
    EXPECT_EQ(payload.at("results").at(1).at("tags").at("TITLE").get<std::string>(), kReplacement);
    EXPECT_FALSE(payload.at("results").at(2).at("success").get<bool>());
    EXPECT_EQ(payload.at("successCount").get<std::int64_t>(), 2);
    EXPECT_EQ(payload.at("errorCount").get<std::int64_t>(), 1);
}

TEST(MetadataText, MixedProbeProgressSerializesBeforeMainThreadDelivery) {
    const FakeInfo normal{{{"Title", {"正常曲目"}}}, {}};
    const FakeInfo damaged{{{"Title", {"\xB9"}}, {"\xBA", {"omitted"}}}, {
        {"codec", "FLAC\xB9"}, {"\xBA", "omitted"},
    }};
    md::MetadataProbeResultItem normalRow;
    normalRow.path = "C:\\music\\normal.flac";
    normalRow.success = true;
    normalRow.infoSource = "cached";
    normalRow.info = metadata_text::MakeTechnicalInfo(10, 1000, 44100, 2, "FLAC");
    normalRow.tags = metadata_text::CollectFlatMetadata(normal, 10, std::nullopt);
    md::MetadataProbeResultItem damagedRow;
    damagedRow.path = "C:\\music\\damaged.cue|subsong:2";
    damagedRow.success = true;
    damagedRow.infoSource = "direct";
    damagedRow.info = metadata_text::MakeTechnicalInfo(20, 1000, 44100, 2, "FLAC\xB9");
    damagedRow.tags = metadata_text::CollectFlatMetadata(damaged, 20, std::nullopt);
    md::MetadataProbeResultItem missingRow;
    missingRow.path = "C:\\music\\missing.flac";
    missingRow.infoSource = "none";
    missingRow.failure = "not-found";
    md::ProbeProgressPayload progress;
    progress.operationId = "probe_utf8";
    progress.done = 3;
    progress.total = 3;
    progress.results = {normalRow, damagedRow, missingRow};

    const auto payload = md::ToJson(progress);
    EXPECT_NO_THROW(payload.dump());
    ASSERT_EQ(payload.at("results").size(), 3u);
    EXPECT_EQ(payload.at("results").at(0).at("tags").at("TITLE").get<std::string>(), "正常曲目");
    EXPECT_EQ(payload.at("results").at(1).at("info").at("codec").get<std::string>(),
              "FLAC" + kReplacement);
    EXPECT_EQ(payload.at("results").at(1).at("tags").count("\xBA"), 0u);
    EXPECT_EQ(payload.at("results").at(2).at("failure").get<std::string>(), "not-found");
    EXPECT_EQ(payload.at("done").get<std::int64_t>(), payload.at("total").get<std::int64_t>());
}

TEST(MetadataText, ProbeWithoutTagsStillSerializesDamagedCodec) {
    md::MetadataProbeResultItem row;
    row.path = "C:\\music\\damaged.flac";
    row.success = true;
    row.infoSource = "direct";
    row.info = metadata_text::MakeTechnicalInfo(20, 1000, 44100, 2, "\xB9");

    const auto payload = md::ToJson(row);
    EXPECT_NO_THROW(payload.dump());
    EXPECT_EQ(payload.at("info").at("codec").get<std::string>(), kReplacement);
    EXPECT_FALSE(payload.contains("tags"));
}

} // namespace
