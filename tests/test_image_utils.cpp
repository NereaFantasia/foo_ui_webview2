#include "pch.h"

#include "../src/utils/ImageUtils.h"

#include <fstream>
#include <iterator>
#include <filesystem>
#include <atomic>
#include <thread>

namespace {

std::vector<uint8_t> ReadAllBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<std::string> CollectJpegMarkers(const std::vector<uint8_t>& data) {
    std::vector<std::string> markers;
    if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return markers;
    }

    size_t pos = 2;
    while (pos + 4 <= data.size()) {
        if (data[pos] != 0xFF) {
            ++pos;
            continue;
        }
        while (pos < data.size() && data[pos] == 0xFF) {
            ++pos;
        }
        if (pos >= data.size()) break;

        uint8_t marker = data[pos++];
        if (marker == 0xD9) {
            markers.emplace_back("EOI");
            break;
        }
        if (marker == 0xDA) {
            markers.emplace_back("SOS");
            break;
        }
        if (pos + 2 > data.size()) break;

        uint16_t segLen = static_cast<uint16_t>((data[pos] << 8) | data[pos + 1]);
        std::string name;
        if (marker >= 0xE0 && marker <= 0xEF) {
            name = "APP" + std::to_string(marker - 0xE0);
        } else if (marker == 0xC0) {
            name = "SOF0";
        } else if (marker == 0xC2) {
            name = "SOF2";
        } else {
            name = "0x" + std::to_string(marker);
        }
        markers.push_back(name);

        pos += segLen;
    }
    return markers;
}

} // namespace

TEST(ImageUtils, ResizeProblematicProgressiveJpegProducesCleanBaselineJpeg) {
    const std::filesystem::path inputPath =
        LR"(E:\OST\Diverse System\[DVSP-0068] AD：TRANCE (C80)\cover.jpg)";

    if (!std::filesystem::exists(inputPath)) {
        return;
    }

    std::vector<uint8_t> input = ReadAllBytes(inputPath);
    ASSERT_FALSE(input.empty());

    int outWidth = 0;
    int outHeight = 0;
    const char* outMimeType = nullptr;

    std::vector<uint8_t> output = ResizeImageWIC(
        input.data(),
        input.size(),
        300,
        outWidth,
        outHeight,
        outMimeType
    );

    ASSERT_FALSE(output.empty());
    EXPECT_EQ(outWidth, 300);
    EXPECT_EQ(outHeight, 300);
    ASSERT_NE(outMimeType, nullptr);
    EXPECT_STREQ(outMimeType, "image/jpeg");

    const std::vector<std::string> markers = CollectJpegMarkers(output);
    EXPECT_FALSE(markers.empty());
    EXPECT_EQ(markers.front(), "APP0");
    EXPECT_NE(std::find(markers.begin(), markers.end(), "SOF0"), markers.end());
    EXPECT_EQ(std::find(markers.begin(), markers.end(), "SOF2"), markers.end())
        << "Resized JPEG should not keep the original progressive scan layout";
    EXPECT_EQ(std::find(markers.begin(), markers.end(), "APP12"), markers.end())
        << "Resized JPEG should not carry Ducky metadata back out";
    EXPECT_EQ(std::find(markers.begin(), markers.end(), "APP14"), markers.end())
        << "Resized JPEG should not carry Adobe metadata back out";
}

TEST(ImageUtils, ResizeProblematicProgressiveJpegStress) {
    const std::filesystem::path inputPath =
        LR"(E:\OST\Diverse System\[DVSP-0068] AD：TRANCE (C80)\cover.jpg)";

    if (!std::filesystem::exists(inputPath)) {
        return;
    }

    std::vector<uint8_t> input = ReadAllBytes(inputPath);
    ASSERT_FALSE(input.empty());

    constexpr int kThreadCount = 8;
    constexpr int kIterationsPerThread = 25;
    std::atomic<int> failures = 0;

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);
    for (int t = 0; t < kThreadCount; ++t) {
        workers.emplace_back([&]() {
            for (int i = 0; i < kIterationsPerThread; ++i) {
                int outWidth = 0;
                int outHeight = 0;
                const char* outMimeType = nullptr;
                std::vector<uint8_t> output = ResizeImageWIC(
                    input.data(),
                    input.size(),
                    300,
                    outWidth,
                    outHeight,
                    outMimeType
                );
                if (output.empty() || outWidth != 300 || outHeight != 300 ||
                    outMimeType == nullptr || std::string(outMimeType) != "image/jpeg") {
                    ++failures;
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(failures.load(), 0);
}
