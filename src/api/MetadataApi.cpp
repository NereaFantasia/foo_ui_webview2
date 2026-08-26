// MetadataApi.cpp - Metadata Editing API
// Provides tag editing capabilities for audio files

#include "pch.h"
#include "api/MetadataApi.h"
#include "api/AsyncOperationRegistry.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "utils/PathSecurity.h"
#include "utils/SubsongUtils.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/metadb_index.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <set>
#include <type_traits>
#include <vector>
#include "core/WebViewContext.h"

namespace {
using json = nlohmann::json;

//==========================================================================
// Base64 Decoding Helper
//==========================================================================
static const int base64_decode_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

std::vector<uint8_t> Base64Decode(const std::string &input) {
  std::vector<uint8_t> result;
  if (input.empty())
    return result;

  result.reserve((input.size() * 3) / 4);

  int val = 0, bits = -8;
  for (unsigned char c : input) {
    if (base64_decode_table[c] == -1) {
      if (c == '=')
        break;
      continue; // Skip whitespace
    }
    val = (val << 6) + base64_decode_table[c];
    bits += 6;
    if (bits >= 0) {
      result.push_back((val >> bits) & 0xFF);
      bits -= 8;
    }
  }
  return result;
}

// Convert art type string to GUID
static GUID StringToArtType(const std::string &type) {
  if (type == "front" || type == "cover_front")
    return album_art_ids::cover_front;
  if (type == "back" || type == "cover_back")
    return album_art_ids::cover_back;
  if (type == "disc")
    return album_art_ids::disc;
  if (type == "icon")
    return album_art_ids::icon;
  if (type == "artist")
    return album_art_ids::artist;
  return album_art_ids::cover_front; // Default
}

//==========================================================================
// Artwork sidecar helpers — used by metadata.embedArtwork target=="file"
//==========================================================================

// Detect image format from leading magic bytes; returns extension with leading dot.
// Falls back to ".jpg" when format cannot be identified.
static std::wstring DetectImageExtension(const std::vector<uint8_t>& bytes) {
  if (bytes.size() >= 3 &&
      bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
    return L".jpg";
  }
  if (bytes.size() >= 8 &&
      bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
      bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
    return L".png";
  }
  if (bytes.size() >= 12 &&
      bytes[0] == 0x52 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x46 &&
      bytes[8] == 0x57 && bytes[9] == 0x45 && bytes[10] == 0x42 && bytes[11] == 0x50) {
    return L".webp";
  }
  if (bytes.size() >= 4 &&
      bytes[0] == 0x47 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x38) {
    return L".gif";
  }
  if (bytes.size() >= 2 && bytes[0] == 0x42 && bytes[1] == 0x4D) {
    return L".bmp";
  }
  return L".jpg"; // safe default — JPEG is the most common embedded format
}

// Resolve base filename (without extension) for the artwork sidecar by type.
// type == "front"  -> "cover" (matches fb2k's default external artwork pattern)
// type == "back"   -> "back"
// type == "disc"   -> "disc"
// type == "icon"   -> "icon"
// type == "artist" -> "artist"
static std::wstring ArtworkBaseName(const std::string& type) {
  if (type == "front" || type == "cover_front" || type.empty()) {
    return L"cover";
  }
  if (type == "back" || type == "cover_back") {
    return L"back";
  }
  if (type == "disc") return L"disc";
  if (type == "icon") return L"icon";
  if (type == "artist") return L"artist";
  return L"cover"; // unknown type falls back to cover for safety
}

// Reject filenames that contain path separators or traversal sequences —
// the input must be a plain file name, not a relative or absolute path.
static bool ContainsFilenameTraversal(const std::string& filename) {
  return filename.find(".\\") != std::string::npos ||
         filename.find("./") != std::string::npos ||
         filename.find("..") != std::string::npos ||
         filename.find('/') != std::string::npos ||
         filename.find('\\') != std::string::npos;
}

// Resolve the artwork output path: parent directory of the audio file +
// explicit filename or generated `<base><ext>`.
// audioPath may carry a "|subsong:N" suffix; it is stripped before resolving the directory.
// fb2k's external artwork lookup is per-directory, so we deliberately ignore subsong index
// — CUE multi-track containers share one sidecar (later writes overwrite earlier ones).
static std::wstring ResolveArtworkOutputPath(const std::string& audioPath,
                                             const std::string& filename,
                                             const std::string& type,
                                             const std::vector<uint8_t>& bytes) {
  auto parsed = SubsongUtils::ParseSubsongPath(audioPath);
  const std::string& filePath = parsed.first;
  std::filesystem::path dir = std::filesystem::path(Utf8ToWide(filePath)).parent_path();

  if (!filename.empty()) {
    return (dir / Utf8ToWide(filename)).wstring();
  }

  std::wstring base = ArtworkBaseName(type);
  std::wstring ext = DetectImageExtension(bytes);
  return (dir / (base + ext)).wstring();
}

// Embed decoded artwork bytes into the file via album_art_editor (legacy path).
// For containers album_art_editor does not support (e.g. CUE referencing external audio),
// returns a structured failure envelope without throwing.
static json EmbedArtworkInternal(const std::string& path,
                                 const std::vector<uint8_t>& bytes,
                                 const std::string& type) {
  try {
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    if (!album_art_editor::g_is_supported_path(canonicalPath.c_str())) {
      return {{"success", false},
              {"error", "Album art editing not supported for this file format"},
              {"path", path},
              {"type", type}};
    }

    GUID artType = StringToArtType(type);

    // Acquire write lock before opening file (required for files in use, e.g. during playback).
    // SDK: "If you want to write tags using album_art_editor APIs, obtain a write lock first."
    abort_callback_dummy abort;
    auto lockMgr = file_lock_manager::get();
    file_lock_ptr writeLock = lockMgr->acquire_write(canonicalPath.c_str(), abort);

    album_art_editor_instance_ptr instance =
        album_art_editor::g_open(nullptr, canonicalPath.c_str(), abort);

    if (!instance.is_valid()) {
      return {{"success", false},
              {"error", "Failed to open album art editor for this file"},
              {"path", path}};
    }

    album_art_data_ptr artData =
        album_art_data_impl::g_create(bytes.data(), bytes.size());

    instance->set(artType, artData, abort);
    instance->commit(abort);
    // writeLock released when going out of scope

    LOG("metadata.embedArtwork: Embedded %zu bytes of %s art into %s",
        bytes.size(), type.c_str(), path.c_str());

    return {{"success", true},
            {"path", path},
            {"type", type},
            {"size", bytes.size()}};
  } catch (const pfc::exception &e) {
    return {{"success", false}, {"error", e.what()}, {"path", path}};
  } catch (...) {
    return {{"success", false},
            {"error", "Unknown error embedding artwork"},
            {"path", path}};
  }
}

// Save decoded artwork bytes to a sidecar file next to the audio file.
// Naming: "<base><ext>" where base = "cover"/"back"/... (per type) and
// ext is detected from magic bytes (".jpg"/".png"/".webp"/".gif"/".bmp").
// CUE/subsong paths fall back to the underlying audio file's directory and share
// a single sidecar — this matches fb2k's per-directory external artwork model.
static json SaveArtworkToDirectory(const std::string& audioPath,
                                   const std::vector<uint8_t>& bytes,
                                   const std::string& type,
                                   const std::string& filename) {
  if (!filename.empty() && ContainsFilenameTraversal(filename)) {
    return {{"success", false},
            {"error", "Invalid filename: path separators and traversal sequences not allowed"}};
  }

  std::wstring outputPath = ResolveArtworkOutputPath(audioPath, filename, type, bytes);
  if (outputPath.empty()) {
    return {{"success", false}, {"error", "Failed to resolve output path"}};
  }

  std::wstring pathError;
  if (!PathSecurity::Instance().ValidateMediaWriteAccess(outputPath, pathError, Utf8ToWide(audioPath))) {
    return {{"success", false}, {"error", "Write access denied: " + WideToUtf8(pathError)}};
  }

  try {
    std::ofstream out(outputPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      return {{"success", false}, {"error", "Failed to create artwork file"}};
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();

    LOG("metadata.embedArtwork: Saved %zu bytes of %s art to %s",
        bytes.size(), type.c_str(), WideToUtf8(outputPath).c_str());

    return {{"success", true},
            {"path", audioPath},
            {"type", type},
            {"size", bytes.size()},
            {"savedTo", WideToUtf8(outputPath)}};
  } catch (const std::exception& e) {
    return {{"success", false}, {"error", e.what()}};
  } catch (...) {
    return {{"success", false}, {"error", "Unknown error saving artwork file"}};
  }
}

static bool HasEssentialMetadataFields(const file_info &info) {
  return info.meta_find("title") != pfc::infinite_size ||
         info.meta_find("TITLE") != pfc::infinite_size ||
         info.meta_find("Title") != pfc::infinite_size;
}

static bool TryReadInfoDirect(const char *canonicalPath, t_uint32 subsong,
                              file_info_impl &info) {
  try {
    input_info_reader::ptr reader;
    input_entry::g_open_for_info_read(reader, nullptr, canonicalPath,
                                      fb2k::noAbort);
    if (!reader.is_valid()) {
      return false;
    }

    reader->get_info(subsong, info, fb2k::noAbort);
    return true;
  } catch (...) {
    return false;
  }
}

static bool ReadMetadataInfoWithFallback(const metadb_handle_ptr& handle,
                                         const char *canonicalPath,
                                         t_uint32 subsong,
                                         file_info_impl &info) {
  bool gotInfo = handle->get_info(info);

  // The direct reader must receive the same subsong the handle was built with;
  // multi-subsong containers (ISO / CUE / multi-track FLAC) would otherwise
  // fall back to track 0 and report the wrong track's tags.
  if ((!gotInfo || !HasEssentialMetadataFields(info)) && TryReadInfoDirect(canonicalPath, subsong, info)) {
    gotInfo = true;
  }

  return gotInfo;
}

//==========================================================================
// Refactored helpers — reduce nesting depth (S134) & cognitive complexity
//==========================================================================

// 从 file_info 收集所有元数据和技术信息到扁平 JSON 对象（大写 key）
//
// fileSize 由调用方给出而不是在这里向 handle 查询：worker 线程上只有
// metadb_info_container 的不可变快照（其 stats() 任意线程可读），
// metadb_handle::get_filestats() 没有这个保证。主线程调用方传
// handle->get_filesize() 即为原行为。
static json CollectFlatMetadataFromInfo(const file_info &info, t_filesize fileSize) {
  json tags = json::object();

  for (t_size i = 0; i < info.meta_get_count(); i++) {
    const char *name = info.meta_enum_name(i);
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    t_size valueCount = info.meta_enum_value_count(i);
    if (valueCount == 1) {
      tags[upperName] = info.meta_enum_value(i, 0);
    } else {
      json values = json::array();
      for (t_size j = 0; j < valueCount; j++) {
        values.push_back(info.meta_enum_value(i, j));
      }
      tags[upperName] = values;
    }
  }

  for (t_size i = 0; i < info.info_get_count(); i++) {
    const char *name = info.info_enum_name(i);
    const char *value = info.info_enum_value(i);
    std::string upperName = name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    tags[upperName] = value;
  }

  char durationStr[32];
  snprintf(durationStr, sizeof(durationStr), "%.3f", info.get_length());
  tags["DURATION"] = durationStr;

  if (fileSize != filesize_invalid) {
    tags["FILESIZE"] = std::to_string(fileSize);
  }

  return tags;
}

// 供 MetadataReadBatch 和 MetadataReadByPath 共享（主线程调用者）
static json CollectFlatMetadata(const file_info &info, const metadb_handle_ptr& handle) {
  return CollectFlatMetadataFromInfo(info, handle->get_filesize());
}

// 从路径解析 CUE subsong 索引
// 支持三种格式: cueIndex 参数 > |subsong:N > #N
struct SubsongParseResult {
  std::string cleanPath;
  int subsongIndex = 0;
};

static SubsongParseResult ParseSubsongIndex(const std::string &path,
                                            int explicitCueIndex) {
  // Always strip |subsong:N from cleanPath first
  // (g_get_canonical_path must never receive a path with |subsong:N)
  std::string cleanPath = path;
  int pathSubsong = 0;

  size_t pipePos = path.find("|subsong:");
  if (pipePos != std::string::npos) {
    cleanPath = path.substr(0, pipePos);
    try {
      pathSubsong = std::stoi(path.substr(pipePos + 9));
    } catch (...) {
      pathSubsong = 0;
    }
  }

  if (explicitCueIndex >= 0) {
    return {cleanPath, explicitCueIndex};
  }

  if (pipePos != std::string::npos) {
    return {cleanPath, pathSubsong};
  }

  // #N 格式 (向后兼容)
  size_t hashPos = path.rfind('#');
  if (hashPos == std::string::npos || hashPos >= path.length() - 1) {
    return {path, 0};
  }

  std::string indexStr = path.substr(hashPos + 1);
  if (indexStr.empty() ||
      !std::all_of(indexStr.begin(), indexStr.end(), ::isdigit)) {
    return {path, 0};
  }

  try {
    return {path.substr(0, hashPos), std::stoi(indexStr)};
  } catch (...) {
    return {path, 0};
  }
}

// 标签值类型分发 — 按 JSON 类型分类到 set/remove
static void ClassifyTagValue(const std::string &upperKey, const json &value,
                             std::map<std::string, std::string> &tagsToSet,
                             std::vector<std::string> &tagsToRemove,
                             json &appliedTags) {
  if (value.is_null() ||
      (value.is_string() && value.get<std::string>().empty())) {
    tagsToRemove.push_back(upperKey);
    appliedTags[upperKey] = nullptr;
    console::printf("metadata.write: Will remove [%s]", upperKey.c_str());
    return;
  }
  if (value.is_string()) {
    std::string strValue = value.get<std::string>();
    tagsToSet[upperKey] = strValue;
    appliedTags[upperKey] = strValue;
    console::printf("metadata.write: Will set [%s] = [%s]", upperKey.c_str(),
                    strValue.c_str());
    return;
  }
  if (value.is_number_integer()) {
    std::string strValue = std::to_string(value.get<int>());
    tagsToSet[upperKey] = strValue;
    appliedTags[upperKey] = value.get<int>();
    return;
  }
  if (value.is_number_float()) {
    std::string strValue = std::to_string(value.get<double>());
    tagsToSet[upperKey] = strValue;
    appliedTags[upperKey] = value.get<double>();
  }
}

static int ParseTrackNumberToken(const std::string &value) {
  if (value.empty() || value.length() > 3 ||
      !std::all_of(value.begin(), value.end(), ::isdigit)) {
    return 0;
  }

  try {
    return std::stoi(value);
  } catch (...) {
    return 0;
  }
}

static int ExtractLeadingTrackNumber(const std::string &filename) {
  // isdigit 的合法域是 [-1, 255]：CJK 文件名的 UTF-8 字节以 signed char 直传为负值，
  // Debug CRT 直接断言（isctype.cpp c >= -1 && c <= 255），必须先转 unsigned char。
  if (filename.length() <= 2 || !std::isdigit(static_cast<unsigned char>(filename[0]))) {
    return 0;
  }

  size_t endPos = 0;
  while (endPos < filename.length() &&
         std::isdigit(static_cast<unsigned char>(filename[endPos]))) {
    endPos++;
  }

  if (endPos == 0 || endPos > 3 || endPos >= filename.length()) {
    return 0;
  }

  const char separator = filename[endPos];
  if (separator != '.' && separator != ' ' && separator != '-' &&
      separator != '_') {
    return 0;
  }

  return ParseTrackNumberToken(filename.substr(0, endPos));
}

static int ExtractWrappedTrackNumber(const std::string &filename) {
  if (filename.length() <= 3 ||
      (filename[0] != '(' && filename[0] != '[')) {
    return 0;
  }

  const char closeChar = filename[0] == '(' ? ')' : ']';
  const size_t closePos = filename.find(closeChar);
  if (closePos == std::string::npos || closePos > 4) {
    return 0;
  }

  return ParseTrackNumberToken(filename.substr(1, closePos - 1));
}

// 从文件名提取轨道号 — 支持 "01. Song.flac" / "(01) Song" / "[01] Song"
static int ExtractTrackNumberFromFilename(const std::string &filename) {
  if (int trackNumber = ExtractLeadingTrackNumber(filename); trackNumber > 0) {
    return trackNumber;
  }

  return ExtractWrappedTrackNumber(filename);
}

static contextmenu_node *FindNamedPopupChild(contextmenu_node *parent,
                                             const char *utf8Fragment,
                                             const char *asciiFragment,
                                             std::string &matchedName) {
  if (!parent) {
    return nullptr;
  }

  for (t_size index = 0; index < parent->get_num_children(); index++) {
    contextmenu_node *child = parent->get_child(index);
    if (!child || !child->get_name() ||
        child->get_type() != contextmenu_item_node::TYPE_POPUP) {
      continue;
    }

    std::string childName = child->get_name();
    const bool matchedUtf8 = utf8Fragment && childName.find(utf8Fragment) != std::string::npos;
    const bool matchedAscii = asciiFragment &&
                              (childName == asciiFragment ||
                               childName.find(asciiFragment) != std::string::npos);
    if (!matchedUtf8 && !matchedAscii) {
      continue;
    }

    matchedName = childName;
    return child;
  }

  return nullptr;
}

static std::optional<json> ExecuteRatingMenuNode(contextmenu_node *ratingMenu,
                                                 const std::string &statsName,
                                                 const std::string &ratingName,
                                                 int rating,
                                                 const std::string &displayPath) {
  if (!ratingMenu) {
    return std::nullopt;
  }

  const t_size targetIndex = rating == 0 ? 0 : static_cast<t_size>(rating);
  if (targetIndex >= ratingMenu->get_num_children()) {
    return std::nullopt;
  }

  contextmenu_node *targetItem = ratingMenu->get_child(targetIndex);
  if (!targetItem ||
      targetItem->get_type() != contextmenu_item_node::TYPE_COMMAND) {
    return std::nullopt;
  }

  try {
    targetItem->execute();
    std::string foundPath = statsName + "/" + ratingName + "/" +
                            (targetItem->get_name() ? targetItem->get_name()
                                                    : std::to_string(rating));
    return json{{"success", true},
                {"path", displayPath},
                {"rating", rating},
                {"storage", "stats"},
                {"menuPath", foundPath}};
  } catch (...) {
    return std::nullopt;
  }
}

// 通过 UTF-8 字节模式直接搜索上下文菜单中的评级项
// 绕过源码编码与运行时编码的错配问题
static std::optional<json> TryRatingViaUtf8Fallback(
    contextmenu_node *root, int rating, const std::string &displayPath) {
  if (!root || root->get_type() != contextmenu_item_node::TYPE_POPUP) {
    return std::nullopt;
  }

  // UTF-8: "播放统计" = E6 92 AD E6 94 BE E7 BB 9F E8 AE A1
  //        "等级"     = E7 AD 89 E7 BA A7
  const char *utf8_stats =
      "\xE6\x92\xAD\xE6\x94\xBE\xE7\xBB\x9F\xE8\xAE\xA1";
  const char *utf8_rating_menu = "\xE7\xAD\x89\xE7\xBA\xA7";

  std::string statsName;
  contextmenu_node *statsMenu =
      FindNamedPopupChild(root, utf8_stats, "Playback Statist", statsName);
  if (!statsMenu) {
    return std::nullopt;
  }

  std::string ratingName;
  contextmenu_node *ratingMenu =
      FindNamedPopupChild(statsMenu, utf8_rating_menu, "Rating", ratingName);
  return ExecuteRatingMenuNode(ratingMenu, statsName, ratingName, rating,
                               displayPath);
}

// 通过 titleformat API 读取 foo_playcount 评级
static std::optional<int> TryReadRatingFromPlaycount(
    const metadb_handle_ptr& handle) {
  try {
    static_api_ptr_t<titleformat_compiler> compiler;
    titleformat_object::ptr script;
    if (!compiler->compile(script, "%rating%")) return std::nullopt;

    pfc::string8 result;
    handle->format_title(nullptr, result, script, nullptr);
    if (result.get_length() == 0 || result[0] == '?') return std::nullopt;

    int rating = atoi(result.get_ptr());
    if (rating > 0 && rating <= 5) return rating;
  } catch (...) {
    // Fall through
  }
  return std::nullopt;
}

//==========================================================================
// metadata.write - Write metadata to a single file
// Uses metadb_io_v2::update_info_async for actual file writing
//==========================================================================

// Custom file_info_filter for tag updates
class TagUpdateFilter : public file_info_filter {
public:
  TagUpdateFilter(const std::map<std::string, std::string> &tags,
                  const std::vector<std::string> &tagsToRemove)
      : m_tags(tags), m_tagsToRemove(tagsToRemove) {}

  bool apply_filter(metadb_handle_ptr p_location, t_filestats p_stats,
                    file_info &p_info) override {
    // Remove specified tags
    for (const auto &tagName : m_tagsToRemove) {
      p_info.meta_remove_field(tagName.c_str());
    }

    // Apply new tag values
    for (const auto &[key, value] : m_tags) {
      p_info.meta_remove_field(key.c_str());
      if (!value.empty()) {
        p_info.meta_add(key.c_str(), value.c_str());
      }
    }

    return true; // Always apply changes
  }

private:
  std::map<std::string, std::string> m_tags;
  std::vector<std::string> m_tagsToRemove;
};

// Async completion_notify wrapper (revised after crash analysis)
//
// DESIGN: We CANNOT block or pump messages on the main thread because:
//   - Blocking: deadlocks (on_completion also fires on main thread)
//   - Message pump: causes reentrant crashes (window activation, metadb indexing)
//
// Instead: fire-and-dispatch. The handler returns immediately with dispatched=true.
// When update_info_async completes, on_completion fires a bridge event
// "metadata:writeComplete" so JS can observe the outcome.
class AsyncWriteNotify : public completion_notify {
public:
  AsyncWriteNotify(std::string path, int subsong, std::string operation)
      : m_path(std::move(path)), m_subsong(subsong),
        m_operation(std::move(operation)) {}

  void on_completion(unsigned code) override {
    const char* status = (code == 0) ? "success" :
                         (code == 1) ? "aborted" : "error";

    console::printf("metadata.%s: completion code=%u (%s) path=%s subsong=%d",
                    m_operation.c_str(), code, status,
                    m_path.c_str(), m_subsong);

    // Fire bridge event so JS can react
    try {
      json eventData = {
          {"operation", m_operation},
          {"path", m_path},
          {"subsong", m_subsong},
          {"code", code},
          {"success", code == 0},
          {"status", status}};
      WebViewContext::GetInstance().BroadcastEvent(
          "metadata:writeComplete", eventData);
    } catch (...) {
      // Best-effort event broadcast; don't crash on failure
    }
  }

private:
  std::string m_path;
  int m_subsong;
  std::string m_operation;
};

json MetadataWrite(const json &params) {
  std::string path = params.value("path", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  if (!params.contains("tags") || !params["tags"].is_object()) {
    return {{"success", false}, {"error", "tags object is required"}};
  }

  try {
    // Parse subsong from path before canonicalization
    int explicitCueIndex = params.value("cueIndex", -1);
    auto parsed = ParseSubsongIndex(path, explicitCueIndex);

    // Convert cleaned path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);
    
    console::printf("metadata.write: path=%s, cleanPath=%s, subsong=%d",
                    path.c_str(), parsed.cleanPath.c_str(), parsed.subsongIndex);
    console::printf("metadata.write: Canonical path = %s", canonicalPath.c_str());

    metadb_handle_ptr handle;
    
    // Use parsed.subsongIndex instead of hardcoded 0
    auto mdb = metadb::get();
    handle = mdb->handle_create(canonicalPath.c_str(), parsed.subsongIndex);
    console::printf("metadata.write: Created handle via handle_create (subsong=%d)",
                    parsed.subsongIndex);

    if (!handle.is_valid()) {
      console::printf("metadata.write: Failed to get handle for %s", canonicalPath.c_str());
      return {{"success", false}, 
              {"error", "Failed to open file"}, 
              {"path", path},
              {"canonicalPath", canonicalPath.c_str()}};
    }
    
    console::printf("metadata.write: Got valid handle, path = %s, subsong_index = %u",
                    handle->get_path(), handle->get_subsong_index());

    // Prepare tag updates
    std::map<std::string, std::string> tagsToSet;
    std::vector<std::string> tagsToRemove;
    json appliedTags = json::object();

    for (auto &[key, value] : params["tags"].items()) {
      std::string upperKey = key;
      std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(),
                     ::toupper);
      ClassifyTagValue(upperKey, value, tagsToSet, tagsToRemove, appliedTags);
    }

    if (tagsToSet.empty() && tagsToRemove.empty()) {
      return {{"success", true}, {"path", path}, {"note", "No tags to update"}};
    }

    // Create filter and handle list
    service_ptr_t<file_info_filter> filter =
        fb2k::service_new<TagUpdateFilter>(tagsToSet, tagsToRemove);

    metadb_handle_list handles;
    handles.add_item(handle);

    console::printf("metadata.write: Calling update_info_async (%u set, %u remove)",
                    (unsigned)tagsToSet.size(), (unsigned)tagsToRemove.size());

    // Async completion with event notification
    // Cannot block main thread (deadlock) or pump messages (reentrant crash).
    // Instead: dispatch + notify JS via metadata:writeComplete event.
    service_ptr_t<AsyncWriteNotify> notify =
        fb2k::service_new<AsyncWriteNotify>(path, parsed.subsongIndex, "write");

    auto io = metadb_io_v2::get();
    // 静默写入：op_flag_silent (fb2k 2.0+) 完全抑制 UI；
    // op_flag_delay_ui (fb2k 1.x fallback) 让短操作不弹进度对话框；
    // op_flag_no_errors 抑制错误对话框。
    io->update_info_async(handles, filter, core_api::get_main_window(),
                          metadb_io_v2::op_flag_no_errors |
                              metadb_io_v2::op_flag_delay_ui |
                              metadb_io_v2::op_flag_silent,
                          notify);

    console::printf("metadata.write: update_info_async dispatched");

    return {
        {"success", true},
        {"dispatched", true},
        {"path", path},
        {"handlePath", handle->get_path()},
        {"subsong", parsed.subsongIndex},
        {"tagsApplied", appliedTags},
        {"tagsSet", static_cast<int>(tagsToSet.size())},
        {"tagsRemoved", static_cast<int>(tagsToRemove.size())},
        {"note", "Write dispatched. Listen for metadata:writeComplete event for final result."},
    };
  } catch (const std::exception &e) {
    console::printf("metadata.write: Exception: %s", e.what());
    return {{"success", false}, {"error", e.what()}};
  } catch (...) {
    console::printf("metadata.write: Unknown exception");
    return {{"success", false}, {"error", "Unknown error"}};
  }
}

//==========================================================================
// metadata.writeBatch - Write metadata to multiple files
//==========================================================================
json MetadataWriteBatch(const json &params) {
  if (!params.contains("items") || !params["items"].is_array()) {
    return {{"success", false}, {"error", "items array is required"}};
  }

  int successCount = 0;
  int failCount = 0;
  json errors = json::array();

  for (const auto &item : params["items"]) {
    std::string path = item.value("path", "");
    if (path.empty()) {
      failCount++;
      errors.push_back({{"path", ""}, {"error", "Missing path"}});
      continue;
    }

    if (!item.contains("tags") || !item["tags"].is_object()) {
      failCount++;
      errors.push_back({{"path", path}, {"error", "Missing tags"}});
      continue;
    }

    // Create params for single write
    json singleParams = {{"path", path}, {"tags", item["tags"]}};
    json result = MetadataWrite(singleParams);

    if (result.value("success", false)) {
      successCount++;
    } else {
      failCount++;
      errors.push_back(
          {{"path", path}, {"error", result.value("error", "Unknown error")}});
    }
  }

  return {{"success", failCount == 0},
          {"successCount", successCount},
          {"failCount", failCount},
          {"errors", errors}};
}

//==========================================================================
// metadata.embedArtwork - Write artwork to a file or its sibling sidecar
// Supports: front, back, disc, icon, artist
// Targets:
//   "embedded" (default) — write via album_art_editor (legacy behavior)
//   "file"               — write a sidecar image next to the audio file
//                          (cover.jpg / back.jpg / disc.jpg / ... — fb2k auto-recognized)
//   "all"                — run both targets and collect results
//   array of the above   — run the listed targets
// CUE / subsong paths fall back to the underlying audio file's directory for the
// sidecar; all subsongs in one container share the same external artwork (matches
// fb2k's per-directory external artwork lookup model).
//==========================================================================
json MetadataEmbedArtwork(const json &params) {
  std::string path = params.value("path", "");
  std::string imageData = params.value("imageData", "");
  std::string type = params.value("type", "front");
  std::string filename = params.value("filename", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  if (imageData.empty()) {
    return {{"success", false},
            {"error", "imageData is required (Base64 encoded)"}};
  }

  // Decode once — shared by every target branch below.
  std::vector<uint8_t> decoded = Base64Decode(imageData);
  if (decoded.empty()) {
    return {{"success", false},
            {"error", "Failed to decode Base64 image data"}};
  }

  // Parse target — accepts string, "all" alias, or string[] array.
  std::set<std::string> targets;
  if (params.contains("target") && params["target"].is_array()) {
    for (const auto& t : params["target"]) {
      if (t.is_string()) targets.insert(t.get<std::string>());
    }
  } else {
    std::string t = params.value("target", "embedded");
    if (t == "all") {
      targets = {"embedded", "file"};
    } else {
      targets.insert(t);
    }
  }

  if (targets.empty()) {
    targets.insert("embedded");
  }

  static const std::set<std::string> validTargets = {"embedded", "file"};
  for (const auto& t : targets) {
    if (validTargets.find(t) == validTargets.end()) {
      return {{"success", false}, {"error", "Invalid target: " + t}};
    }
  }

  // Single target: backward-compatible flat response envelope.
  if (targets.size() == 1) {
    const std::string& t = *targets.begin();
    if (t == "embedded") {
      return EmbedArtworkInternal(path, decoded, type);
    }
    // t == "file"
    return SaveArtworkToDirectory(path, decoded, type, filename);
  }

  // Multiple targets: aggregate into a `results` map. Top-level success is true
  // when any target succeeded — mirrors lyrics.save behavior for consistency.
  json results = json::object();
  bool anySuccess = false;

  if (targets.count("embedded")) {
    results["embedded"] = EmbedArtworkInternal(path, decoded, type);
    if (results["embedded"].value("success", false)) anySuccess = true;
  }

  if (targets.count("file")) {
    results["file"] = SaveArtworkToDirectory(path, decoded, type, filename);
    if (results["file"].value("success", false)) anySuccess = true;
  }

  return {{"success", anySuccess},
          {"path", path},
          {"type", type},
          {"results", results}};
}

//==========================================================================
// metadata.removeEmbeddedArt - Remove embedded artwork from file
//==========================================================================
json MetadataRemoveEmbeddedArt(const json &params) {
  std::string path = params.value("path", "");
  std::string type = params.value("type", ""); // Empty = remove all
  bool removeAll = params.value("removeAll", false);

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  try {
    // Convert path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    // Check if album_art_editor supports this file format
    if (!album_art_editor::g_is_supported_path(canonicalPath.c_str())) {
      return {{"success", false},
              {"error", "Album art editing not supported for this file format"},
              {"path", path}};
    }

    // Acquire write lock before opening file (required for files in use, e.g. during playback)
    abort_callback_dummy abort;
    auto lockMgr = file_lock_manager::get();
    file_lock_ptr writeLock = lockMgr->acquire_write(canonicalPath.c_str(), abort);

    // Open album art editor for the file (now safe, write lock held)
    album_art_editor_instance_ptr instance =
        album_art_editor::g_open(nullptr, canonicalPath.c_str(), abort);

    if (!instance.is_valid()) {
      return {{"success", false},
              {"error", "Failed to open album art editor for this file"},
              {"path", path}};
    }

    json removedTypes = json::array();

    if (removeAll || type.empty()) {
      // Try to get v2 instance for remove_all()
      album_art_editor_instance_v2::ptr v2;
      if (instance->service_query_t(v2) && v2.is_valid()) {
        v2->remove_all();
        removedTypes.push_back("all");
      } else {
        // Fallback: remove common art types individually
        static const GUID artTypes[] = {
            album_art_ids::cover_front, album_art_ids::cover_back,
            album_art_ids::disc, album_art_ids::icon, album_art_ids::artist};
        static const char *artNames[] = {"front", "back", "disc", "icon",
                                         "artist"};

        for (int i = 0; i < 5; i++) {
          try {
            instance->remove(artTypes[i]);
            removedTypes.push_back(artNames[i]);
          } catch (...) {
            // Ignore if specific type doesn't exist
          }
        }
      }
    } else {
      // Remove specific art type
      GUID artType = StringToArtType(type);
      instance->remove(artType);
      removedTypes.push_back(type);
    }

    // Commit changes
    instance->commit(abort);

    LOG("metadata.removeEmbeddedArt: Removed art from %s", path.c_str());

    return {{"success", true}, {"path", path}, {"removedTypes", removedTypes}};
  } catch (const pfc::exception &e) {
    return {{"success", false}, {"error", e.what()}, {"path", path}};
  } catch (...) {
    return {{"success", false},
            {"error", "Unknown error removing artwork"},
            {"path", path}};
  }
}

//==========================================================================
// metadata.removeTag - Remove specific tags from file
//==========================================================================
json MetadataRemoveTag(const json &params) {
  std::string path = params.value("path", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  if (!params.contains("tags") || !params["tags"].is_array()) {
    return {{"success", false}, {"error", "tags array is required"}};
  }

  try {
    // Parse subsong from path (same pattern as MetadataWrite)
    int explicitCueIndex = params.value("cueIndex", -1);
    auto parsed = ParseSubsongIndex(path, explicitCueIndex);

    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);

    console::printf("metadata.removeTag: path=%s, cleanPath=%s, subsong=%d",
                    path.c_str(), parsed.cleanPath.c_str(), parsed.subsongIndex);

    auto mdb = metadb::get();
    metadb_handle_ptr handle;
    handle = mdb->handle_create(canonicalPath.c_str(), parsed.subsongIndex);

    if (!handle.is_valid()) {
      return {
          {"success", false}, {"error", "Failed to open file"}, {"path", path}};
    }

    // Collect tags to remove
    std::vector<std::string> tagsToRemove;
    json removedTags = json::array();

    for (const auto &tag : params["tags"]) {
      if (tag.is_string()) {
        std::string tagName = tag.get<std::string>();
        std::transform(tagName.begin(), tagName.end(), tagName.begin(),
                       ::toupper);
        tagsToRemove.push_back(tagName);
        removedTags.push_back(tagName);
      }
    }

    if (tagsToRemove.empty()) {
      return {{"success", true},
              {"path", path},
              {"removedTags", removedTags},
              {"removedCount", 0}};
    }

    // Use TagUpdateFilter to remove tags (empty tagsToSet, only tagsToRemove)
    std::map<std::string, std::string> emptyTags;
    service_ptr_t<file_info_filter> filter =
        fb2k::service_new<TagUpdateFilter>(emptyTags, tagsToRemove);

    metadb_handle_list handles;
    handles.add_item(handle);

    // Async completion with event notification
    service_ptr_t<AsyncWriteNotify> notify =
        fb2k::service_new<AsyncWriteNotify>(path, parsed.subsongIndex, "removeTag");

    auto io = metadb_io_v2::get();
    // 静默：在原有 delay_ui 基础上叠加 silent (fb2k 2.0+ 完全抑制 UI)。
    io->update_info_async(handles, filter, core_api::get_main_window(),
                          metadb_io_v2::op_flag_delay_ui |
                              metadb_io_v2::op_flag_no_errors |
                              metadb_io_v2::op_flag_silent,
                          notify);

    console::printf("metadata.removeTag: update_info_async dispatched");

    return {
        {"success", true},
        {"dispatched", true},
        {"path", path},
        {"subsong", parsed.subsongIndex},
        {"removedTags", removedTags},
        {"removedCount", static_cast<int>(removedTags.size())},
        {"note", "Remove dispatched. Listen for metadata:writeComplete event for final result."},
    };
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// metadata.read - Read all metadata from a file
//==========================================================================
json MetadataRead(const json &params) {
  std::string path = params.value("path", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  try {
    // Resolve the subsong so multi-track containers (CUE sheets, ISO images)
    // report the requested track instead of always falling back to track 0.
    int explicitCueIndex = params.value("cueIndex", -1);
    auto parsed = ParseSubsongIndex(path, explicitCueIndex);
    const auto subsong = static_cast<t_uint32>(parsed.subsongIndex);

    // Convert path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr handle;
    handle = mdb->handle_create(canonicalPath.c_str(), subsong);

    if (!handle.is_valid()) {
      return {
          {"success", false}, {"error", "Failed to open file"}, {"path", path}};
    }

    file_info_impl info;
    if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), subsong,
                                     info)) {
      return {{"success", false}, {"error", "Failed to get track info"}};
    }

    json tags = json::object();

    // Read all metadata fields
    for (t_size i = 0; i < info.meta_get_count(); i++) {
      const char *name = info.meta_enum_name(i);
      t_size valueCount = info.meta_enum_value_count(i);

      if (valueCount == 1) {
        tags[name] = info.meta_enum_value(i, 0);
      } else {
        json values = json::array();
        for (t_size j = 0; j < valueCount; j++) {
          values.push_back(info.meta_enum_value(i, j));
        }
        tags[name] = values;
      }
    }

    // Technical info
    json techInfo = {
        {"duration", info.get_length()},
        {"bitrate", info.info_get_int("bitrate")},
        {"sampleRate", info.info_get_int("samplerate")},
        {"channels", info.info_get_int("channels")},
        {"codec", info.info_get("codec") ? info.info_get("codec") : ""}};

    return {
        {"success", true}, {"path", path}, {"tags", tags}, {"info", techInfo}};
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// metadata.readBatch - Batch read metadata from multiple files
// params: { paths: string[] }
// Returns: { success: true, results: [ { path, success, tags?, error? }, ... ] }
//==========================================================================
json MetadataReadBatch(const json &params) {
  if (!params.contains("paths") || !params["paths"].is_array()) {
    return {{"success", false}, {"error", "paths array is required"}};
  }

  const auto &paths = params["paths"];
  json results = json::array();
  int successCount = 0;
  int errorCount = 0;

  auto mdb = metadb::get();

  for (const auto &pathItem : paths) {
    if (!pathItem.is_string()) {
      results.push_back({{"success", false}, {"error", "invalid path type"}});
      errorCount++;
      continue;
    }

    std::string path = pathItem.get<std::string>();
    
    try {
      // Per-entry subsong: batch paths may mix plain files and
      // "container|subsong:N" references.
      auto parsed = ParseSubsongIndex(path, -1);
      const auto subsong = static_cast<t_uint32>(parsed.subsongIndex);

      pfc::string8 canonicalPath;
      filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);

      metadb_handle_ptr handle;
      handle = mdb->handle_create(canonicalPath.c_str(), subsong);

      if (!handle.is_valid()) {
        results.push_back({{"path", path}, {"success", false}, {"error", "Failed to open file"}});
        errorCount++;
        continue;
      }

      file_info_impl info;
      if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), subsong,
                                       info)) {
        results.push_back({{"path", path}, {"success", false}, {"error", "Failed to get track info"}});
        errorCount++;
        continue;
      }

      results.push_back({{"path", path}, {"success", true}, {"tags", CollectFlatMetadata(info, handle)}});
      successCount++;

    } catch (const std::exception &e) {
      results.push_back({{"path", path}, {"success", false}, {"error", e.what()}});
      errorCount++;
    }
  }

  return {
      {"success", true},
      {"total", paths.size()},
      {"successCount", successCount},
      {"errorCount", errorCount},
      {"results", results}};
}

//==========================================================================
// metadata.readRaw - Read metadata directly from file, bypassing metadb cache
// params: { path: string, cueIndex?: number }
// Returns structured format identical to metadata.read + "source": "file"
//==========================================================================
json MetadataReadRaw(const json &params) {
  std::string path = params.value("path", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  try {
    int explicitCueIndex = params.value("cueIndex", -1);
    auto parsed = ParseSubsongIndex(path, explicitCueIndex);

    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);

    file_info_impl info;
    if (!TryReadInfoDirect(canonicalPath.c_str(),
                           static_cast<t_uint32>(parsed.subsongIndex), info)) {
      return {{"success", false},
              {"error", "Failed to read file directly"},
              {"path", path}};
    }

    // 收集 tags（保留原始 key 大小写，与 metadata.read 一致）
    json tags = json::object();
    for (t_size i = 0; i < info.meta_get_count(); i++) {
      const char *name = info.meta_enum_name(i);
      t_size valueCount = info.meta_enum_value_count(i);
      if (valueCount == 1) {
        tags[name] = info.meta_enum_value(i, 0);
      } else {
        json values = json::array();
        for (t_size j = 0; j < valueCount; j++) {
          values.push_back(info.meta_enum_value(i, j));
        }
        tags[name] = values;
      }
    }

    // 技术信息
    json techInfo = {
        {"duration", info.get_length()},
        {"bitrate", info.info_get_int("bitrate")},
        {"sampleRate", info.info_get_int("samplerate")},
        {"channels", info.info_get_int("channels")},
        {"codec", info.info_get("codec") ? info.info_get("codec") : ""}};

    return {{"success", true},
            {"path", path},
            {"tags", tags},
            {"info", techInfo},
            {"source", "file"}};
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// metadata.readByPath - Read all metadata from a file (flat format)
// Returns all tags and technical info in a single flat object
//==========================================================================
json MetadataReadByPath(const json &params) {
  std::string path = params.value("path", "");

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  try {
    // Multi-subsong containers (CUE / ISO / multi-track files) address a single
    // track as `path|subsong:N`. The suffix must be stripped before the path is
    // canonicalized, and the index has to reach both handle_create() and the
    // direct-read fallback, otherwise track N reports track 0's tags.
    int explicitCueIndex = params.value("cueIndex", -1);
    auto parsed = ParseSubsongIndex(path, explicitCueIndex);
    const auto subsong = static_cast<t_uint32>(parsed.subsongIndex);

    // Convert path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(parsed.cleanPath.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr handle;
    handle = mdb->handle_create(canonicalPath.c_str(), subsong);

    if (!handle.is_valid()) {
      return {{"success", false},
              {"error", "Failed to open file"},
              {"path", path},
              {"canonicalPath", canonicalPath.c_str()}};
    }

    file_info_impl info;
    if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), subsong,
                                      info)) {
      return {{"success", false}, {"error", "Failed to get track info"}};
    }

    json result = CollectFlatMetadata(info, handle);
    result["success"] = true;
    result["path"] = path;

    // Fallback: Extract TRACKNUMBER from filename if not present in tags
    if (!result.contains("TRACKNUMBER")) {
      std::string filename = path;
      size_t lastSlash = filename.find_last_of("/\\");
      if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1);
      }
      int trackNum = ExtractTrackNumberFromFilename(filename);
      if (trackNum > 0 && trackNum <= 999) {
        result["TRACKNUMBER"] = std::to_string(trackNum);
      }
    }

    return result;
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// rating.set - Set rating for a track
// Primary: Uses foo_playcount via context menu (recommended)
// Fallback: Writes to file RATING tag
// NOTE: foo_playcount plugin is required for full functionality
// CUE 支持: 从路径解析 subsong index
//   - 优先: |subsong:N 格式 (与 playback/playlist API 统一)
//   - 后备: #N 格式 (向后兼容)
//   - 参数: cueIndex 参数 (最高优先级)
//==========================================================================
json RatingSet(const json &params) {
  std::string trackPath = params.value("path", "");
  std::string originalPath = trackPath;  // 保存原始路径用于返回（含 |subsong:N）
  int rating = params.value("rating", -1);
  
  // 支持显式指定 cueIndex 参数 (优先级最高)
  int cueIndex = params.value("cueIndex", -1);

  if (rating < 0 || rating > 5) {
    return {{"success", false}, {"error", "rating must be 0-5 (0 = unrated)"}};
  }

  // Get target track(s)
  metadb_handle_list items;

  if (!trackPath.empty()) {
    auto [cleanPath, subsongIndex] = ParseSubsongIndex(trackPath, cueIndex);
    trackPath = cleanPath;

    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(trackPath.c_str(), canonicalPath);
    auto mdb = metadb::get();
    metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), subsongIndex);
    if (handle.is_valid()) {
      items.add_item(handle);
    }
  }

  if (items.get_count() == 0) {
    auto pc = playback_control::get();
    metadb_handle_ptr nowPlaying;
    if (pc->get_now_playing(nowPlaying)) {
      items.add_item(nowPlaying);
    } else {
      auto pm = playlist_manager::get();
      pm->activeplaylist_get_selected_items(items);
    }
  }

  if (items.get_count() == 0) {
    return {{"success", false}, {"error", "No track selected or playing"}};
  }

  // Build rating menu path - supports multiple menu structures
  // foo_playcount may place Rating menu at different locations depending on version:
  // - Direct: "等级/X" or "Rating/X" 
  // - Nested: "播放统计信息/等级/X" or "Playback Statistics/Rating/X"
  std::vector<std::string> pathVariants;
  if (rating == 0) {
    // Multiple variants for "clear rating" / "unset" menu item
    // Based on screenshot: 播放统计信息 > 等级 > <未设置>
    pathVariants = {
        // Nested paths (confirmed from user's screenshot)
        "播放统计信息/等级/<未设置>",
        "播放统计信息/等级/<not set>",
        "Playback Statistics/Rating/<not set>",
        "Playback Statistics/Rating/<未设置>",
        // Direct paths (fallback for some versions)
        "等级/<未设置>",
        "等级/<not set>",
        "Rating/<not set>",
    };
  } else {
    pathVariants = {
        // Direct paths
        std::string("等级/") + std::to_string(rating),
        std::string("Rating/") + std::to_string(rating),
        // Nested paths
        std::string("播放统计信息/等级/") + std::to_string(rating),
        std::string("Playback Statistics/Rating/") + std::to_string(rating),
    };
  }

  // Create context menu manager
  auto mgr = contextmenu_manager::g_create();
  mgr->init_context(items, contextmenu_manager::flag_view_full);

  contextmenu_node *root = mgr->get_root();
  if (!root) {
    // Fallback to file tag
    json tags;
    if (rating == 0) {
      tags["RATING"] = nullptr;
    } else {
      tags["RATING"] = std::to_string(rating);
    }
    json writeParams = {{"path", trackPath}, {"tags", tags}};
    json result = MetadataWrite(writeParams);
    if (result.value("success", false)) {
      return {{"success", true},
              {"path", originalPath},  // 返回原始路径（含 |subsong:N）
              {"rating", rating},
              {"storage", "file"},
              {"note", "foo_playcount not available, written to file tag"}};
    }
    return result;
  }

  // Helper lambda to split path
  auto splitPath = [](const std::string &path) -> std::vector<std::string> {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
      if (!segment.empty()) {
        parts.push_back(segment);
      }
    }
    return parts;
  };

  // Helper lambda to find node by path
  std::function<contextmenu_node *(contextmenu_node *,
                                   const std::vector<std::string> &, size_t)>
      findNode;
  findNode = [&findNode](contextmenu_node *node,
                         const std::vector<std::string> &pathParts,
                         size_t index) -> contextmenu_node * {
    if (!node || index >= pathParts.size())
      return nullptr;

    if (node->get_type() != contextmenu_item_node::TYPE_POPUP) {
      return nullptr;
    }

    t_size childCount = node->get_num_children();
    for (t_size i = 0; i < childCount; i++) {
      contextmenu_node *child = node->get_child(i);
      if (!child)
        continue;

      const char *childName = child->get_name();
      if (!childName)
        continue;

      if (std::string(childName) != pathParts[index])
        continue;

      if (index == pathParts.size() - 1) {
        // 最后一段路径：接受 COMMAND 或 POPUP
        if (child->get_type() == contextmenu_item_node::TYPE_COMMAND ||
            child->get_type() == contextmenu_item_node::TYPE_POPUP)
          return child;
      } else if (child->get_type() == contextmenu_item_node::TYPE_POPUP) {
        contextmenu_node *found = findNode(child, pathParts, index + 1);
        if (found)
          return found;
      }
    }
    return nullptr;
  };

  // Try each path variant
  for (const auto &menuPath : pathVariants) {
    std::vector<std::string> pathParts = splitPath(menuPath);
    if (pathParts.empty())
      continue;

    if (root->get_type() != contextmenu_item_node::TYPE_POPUP)
      continue;

    contextmenu_node *targetNode = nullptr;
    t_size childCount = root->get_num_children();
    for (t_size i = 0; i < childCount && !targetNode; i++) {
      contextmenu_node *child = root->get_child(i);
      if (!child)
        continue;
      const char *childName = child->get_name();
      if (!childName)
        continue;
      if (std::string(childName) != pathParts[0])
        continue;

      if (pathParts.size() == 1 &&
          child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
        targetNode = child;
      } else {
        targetNode = findNode(child, pathParts, 1);
      }
    }

    if (targetNode) {
      try {
        targetNode->execute();
        return {{"success", true},
                {"path", originalPath.empty() ? "(current)" : originalPath},  // 返回原始路径
                {"rating", rating},
                {"storage", "stats"},
                {"menuPath", menuPath}};
      } catch (...) {
        continue; // Try next variant
      }
    }
  }

  // Fallback: directly search menu by UTF-8 byte patterns
  auto utf8Result = TryRatingViaUtf8Fallback(
      root, rating, originalPath.empty() ? "(current)" : originalPath);
  if (utf8Result) return *utf8Result;

  json tags;
  if (rating == 0) {
    tags["RATING"] = nullptr;
  } else {
    tags["RATING"] = std::to_string(rating);
  }
  json writeParams = {{"path", trackPath}, {"tags", tags}};
  json result = MetadataWrite(writeParams);
  if (result.value("success", false)) {
    return {{"success", true},
            {"path", originalPath},  // 返回原始路径（含 |subsong:N）
            {"rating", rating},
            {"storage", "file"},
            {"note", "foo_playcount menu not found, written to file tag"}};
  }
  return result;
}

//==========================================================================
// rating.get - Get rating from playback statistics
// CUE 支持: 从路径解析 subsong index
//   - 优先: |subsong:N 格式 (与 playback/playlist API 统一)
//   - 后备: #N 格式 (向后兼容)
//   - 参数: cueIndex 参数 (最高优先级)
//==========================================================================
json RatingGet(const json &params) {
  std::string path = params.value("path", "");
  std::string originalPath = path;  // 保存原始路径用于返回（含 |subsong:N）
  int cueIndex = params.value("cueIndex", -1);

  if (path.empty()) {
    return {{"success", false}, {"error", "path is required"}};
  }

  try {
    auto [cleanPath, subsongIndex] = ParseSubsongIndex(path, cueIndex);
    path = cleanPath;

    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), subsongIndex);

    if (!handle.is_valid()) {
      return {{"success", false}, {"error", "Failed to open file"}};
    }

    // Use titleformat API to read %rating% via foo_playcount
    auto statsRating = TryReadRatingFromPlaycount(handle);
    if (statsRating.has_value()) {
      return {
          {"success", true},
          {"path", originalPath},
          {"rating", *statsRating},
          {"storage", "stats"},
      };
    }

    // Fallback: Read from file tag using get_info_ref() (cached, more reliable)
    int rating = 0;
    metadb_info_container::ptr infoContainer = handle->get_info_ref();
    if (infoContainer.is_valid()) {
      const file_info &info = infoContainer->info();
      const char *ratingStr = info.meta_get("RATING", 0);
      if (ratingStr) {
        rating = atoi(ratingStr);
        if (rating < 0)
          rating = 0;
        if (rating > 5)
          rating = 5;
      }
    }
    // Note: If infoContainer is invalid, we return rating=0 (unrated)
    // This is better than failing completely

    return {
        {"success", true},
        {"path", originalPath},  // 返回原始路径（含 |subsong:N）
        {"rating", rating},
        {"storage", "file"},
    };
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// metadata.probeBatchAsync / metadata.cancelProbe
//
// 既有四个 read API 已经能读未入库文件的 duration/bitrate/samplerate
// （降级链 ReadMetadataInfoWithFallback）。本组新面只补它们做不到的三件事：
// 不阻塞主线程、中途可取消、失败原因分类。四个 read API 的行为不变。
//
// 已知残留缺口（本任务不修）：降级链判据 HasEssentialMetadataFields 只看
// title 标签，「缓存里有 title 但缺 bitrate」不会重新读盘。本组新面改用
// metadb_info_container::isInfoPartial() 绕开了它，但那四个 API 仍是旧判据；
// 改判据会同时影响它们，属独立议题。
//==========================================================================

// 取消令牌注册表。abort_callback_impl 可外部触发（abort_callback.h:71）
// 但不可拷贝（:82-83），注册表与 worker 必须共享同一个对象，故用 shared_ptr。
using ProbeAbortRegistry = fb2k_api::AsyncOperationRegistry<abort_callback_impl>;

static ProbeAbortRegistry& GetProbeRegistry() {
  static ProbeAbortRegistry registry;
  return registry;
}

// 关掉「已注册但还没派工」这段窗口的泄漏。Register 成功之后到
// fb2k::inCpuWorkerThread 返回之前还有好几处会抛：params.value("includeTags",
// true) 碰到非 bool 会抛 nlohmann type_error，callerSeed 赋值与 std::function
// 的捕获拷贝会抛 bad_alloc，线程池在关停期派工本身也会失败。这些异常都会被
// handler 末尾的 catch(const std::exception&) 接住并返回错误信封，但注册表条目
// 会永久留下 —— abort token 不释放，而且此后每次 cancelProbe 对这个
// operationId 都返回 cancelled:true，对页面是纯假信号（它以为取消了一个
// 从来没跑起来的操作）。
//
// 选 RAII 而不是「把派工单独 try 起来」：作用域天然覆盖整个窗口，以后有人在
// Register 与派工之间插代码也自动被保护；而单独 try 需要把 totalCount 挪到
// try 之外才能给 return 用，是一次无谓的结构改动。
//
// operationId 由调用者持有：这里存引用而不是拷贝，构造就不会抛，否则「守卫自己
// 构造失败」又是同一个泄漏。要求该字符串的生存期覆盖本对象 —— 调用点是同一个
// 作用域内紧邻的局部变量。
class ProbeRegistrationGuard {
public:
  explicit ProbeRegistrationGuard(const std::string& operationId) noexcept
      : operationId_(operationId) {}

  ProbeRegistrationGuard(const ProbeRegistrationGuard&) = delete;
  ProbeRegistrationGuard& operator=(const ProbeRegistrationGuard&) = delete;

  // 派工成功后调用：摘除责任移交给 worker 自己的最外层。
  void Dismiss() noexcept { armed_ = false; }

  ~ProbeRegistrationGuard() {
    if (!armed_) {
      return;
    }
    // 析构可能跑在异常展开中，抛出去就是 std::terminate。
    try {
      GetProbeRegistry().Remove(operationId_);
    } catch (...) {
    }
  }

private:
  const std::string& operationId_;
  bool armed_ = true;
};

// 高位随机是为了让页面无法靠观察序号推出别人的 operationId。
// 只在主线程的 handler 里调用，所以 mt19937_64 不需要加锁。
static std::string NextProbeOperationId() {
  static std::mt19937_64 rng(std::random_device{}());
  static std::atomic<uint64_t> counter{0};
  return fb2k_api::FormatAsyncOperationId("probe", counter.fetch_add(1) + 1, rng());
}

static int64_t ProbeNowMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// 一条路径的探测目标。handle 在主线程建好；earlyFailure 非空表示主线程
// 解析阶段就失败了，worker 直接回报，不必碰磁盘。
struct ProbeTarget {
  std::string path;          // 原样回显（含 |subsong: 后缀）
  metadb_handle_ptr handle;
  const char* earlyFailure = nullptr;
};

// 一条路径的探测产物。infoSource / failure 用 API 契约定死的字符串。
struct ProbeItemOutcome {
  const char* infoSource = "none";  // cached | direct | none
  const char* failure = nullptr;    // nullptr = 成功
  bool aborted = false;
  json info = json::object();
  json tags = json::object();
  bool hasTags = false;
};

// 读一条：先看缓存快照，未命中或 partial 才落盘。
//
// 判据用 metadb_info_container::isInfoPartial()（metadb_handle.h:16）而不是
// 本文件的 HasEssentialMetadataFields()（只看 title 标签，缺 bitrate 也算齐）。
// 落盘走 SDK 原语 metadb_handle::get_full_info_ref(abort_callback&)
// （metadb_handle.cpp:101-117），它原生接受 abort_callback，取消能真正打断读盘；
// 而本文件的 TryReadInfoDirect 用 fb2k::noAbort，打不断。
//
// worker 线程安全性：get_info_ref() 返回不可变快照，SDK 明写「任意上下文可调、
// 不涉锁语义」（metadb_handle.h:108-111）；get_full_info_ref 内部只用它加
// g_open_for_info_read。反之 metadb_handle::get_info()（:69）文档说明会临时锁
// metadb 且缓存状态只在主线程变化 —— 所以这里不用它。
//
// catch 顺序即语义，不得调换。pfc::exception 就是 std::exception
// （pfc/primitives.h:201），exception_aborted 派生自它（abort_callback.h:5），
// 所以 catch(const std::exception&) 提前会把「取消」吞成 read-error。
// 继承链（exception_io.h，PFC_DECLARE_EXCEPTION 为 public 继承，
// primitives.h:35）：exception_io_unsupported_format(:20) ⊂
// exception_io_data(:16) ⊂ exception_io(:7) ⊂ std::exception；
// exception_io_not_found(:9) 与 exception_io_data 是兄弟，必须排在
// exception_io 之前，否则永不命中。
// exception_io_data_truncation(:18) 与 exception_io_bad_subsong_index(:22)
// 也在 exception_io_data 之下，被同一条归成 read-error。
//
// 下面这组 static_assert 把上面每一条继承关系钉在编译期。
//
// 为什么单测里构造不出这六类真异常 —— 不是「缺 SDK lib」：MSVC 分支的
// PFC_DECLARE_EXCEPTION（pfc/primitives.h:34-42）展开出的 ctor 全是 inline，
// 基类 std::exception(const char*, int) 由 MSVC CRT 提供，所以这些类型本身
// header-only 就能构造，不需要任何 SDK lib。真正的障碍是这两个头不自含：
// exception_io.h 与 abort_callback.h 都在 #pragma once 之后直接用
// PFC_DECLARE_EXCEPTION（分别是 :7 起与 :5），零 include，要引它们就得把整条
// pfc/SDK 头链拽进来；而 tests/pch.h 与 tests/compat/fb2k_types.h 是刻意不引
// SDK 头的（后者只给 t_size 一类 typedef 加一个 console 桩）。
//
// 分工因此是：真实类型的继承事实由这里的 static_assert 保证，catch 排序逻辑由
// tests/test_async_operation_registry.cpp 的镜像层级保证。SDK 升级把
// exception_io_not_found 挪到 exception_io_data 之下，就会在这里编译失败，
// 而不是在运行时静默失去分类。
static_assert(std::is_same_v<std::exception, pfc::exception>,
              "pfc::exception IS std::exception (a typedef, not a subclass). "
              "If this ever becomes a distinct type, re-derive the catch order "
              "instead of assuming the chain below still holds");
static_assert(std::is_base_of_v<std::exception, exception_aborted>,
              "exception_aborted derives from std::exception, so it MUST be "
              "caught before catch(const std::exception&)");
static_assert(std::is_base_of_v<exception_io, exception_io_not_found>);
static_assert(std::is_base_of_v<exception_io, exception_io_data>);
static_assert(std::is_base_of_v<exception_io_data, exception_io_unsupported_format>,
              "unsupported-format must be caught before exception_io_data, "
              "otherwise that classification never appears");
static_assert(std::is_base_of_v<exception_io_data, exception_io_data_truncation>);
static_assert(std::is_base_of_v<exception_io_data, exception_io_bad_subsong_index>);
static_assert(!std::is_base_of_v<exception_io_data, exception_io_not_found>,
              "not-found is a sibling of exception_io_data, not a child; the "
              "catch order relies on it only having to precede exception_io");

static ProbeItemOutcome ProbeOneTrack(const metadb_handle_ptr& handle,
                                      bool includeTags,
                                      abort_callback& abort) {
  ProbeItemOutcome out;
  try {
    // 缓存全命中的批次也要能被及时取消，所以每条都先 check 一次。
    abort.check();

    metadb_info_container::ptr container;
    if (handle->get_info_ref(container) && container.is_valid() &&
        !container->isInfoPartial()) {
      out.infoSource = "cached";
    } else {
      container = handle->get_full_info_ref(abort);
      // 赋值放在调用之后：抛出时 infoSource 应留在 "none"。
      out.infoSource = "direct";
    }

    if (!container.is_valid()) {
      out.infoSource = "none";
      out.failure = "read-error";
      return out;
    }

    const file_info& info = container->info();
    // 与 metadata.read 的 techInfo 同形（本文件 MetadataRead）。
    json techInfo = {
        {"duration", info.get_length()},
        {"bitrate", info.info_get_int("bitrate")},
        {"sampleRate", info.info_get_int("samplerate")},
        {"channels", info.info_get_int("channels")},
        {"codec", info.info_get("codec") ? info.info_get("codec") : ""}};
    out.info = std::move(techInfo);

    if (includeTags) {
      // stats() 取自同一个不可变快照，任意线程可读；
      // handle->get_filesize() 没有这个保证，worker 上不能用。
      out.tags = CollectFlatMetadataFromInfo(info, container->stats().m_size);
      out.hasTags = true;
    }
    return out;
  } catch (const exception_aborted&) {
    // 取消不是 failure：调用方要的是 cancelled，不是 read-error。
    out.aborted = true;
    out.infoSource = "none";
    return out;
  } catch (const exception_io_not_found&) {
    out.infoSource = "none";
    out.failure = "not-found";
    return out;
  } catch (const exception_io_unsupported_format&) {
    out.infoSource = "none";
    out.failure = "unsupported-format";
    return out;
  } catch (const exception_io_data&) {
    out.infoSource = "none";
    out.failure = "read-error";
    return out;
  } catch (const exception_io&) {
    out.infoSource = "none";
    out.failure = "read-error";
    return out;
  } catch (const std::exception&) {
    out.infoSource = "none";
    out.failure = "read-error";
    return out;
  }
}

// 事件载荷由具名函数构造，而不是在 EmitEvent 调用点摊成 init-list。
// 这样 Graph 的 cpp-parser 把 payload_schema 记为空（对照
// playback:trackChanged 的 payload_pattern = local-var+helper-call），
// 生成层由 sdk/src/types/overrides/events.ts 的 @codegen-override 供型，
// results[] 的元素形状不是 extractor 能推出来的。
static json BuildProbeProgressPayload(const std::string& operationId, size_t done,
                                      size_t total, const json& results) {
  return {{"operationId", operationId},
          {"done", done},
          {"total", total},
          {"results", results}};
}

static json BuildProbeCompletePayload(const std::string& operationId, size_t total,
                                      size_t successCount, size_t failureCount,
                                      bool cancelled) {
  return {{"operationId", operationId},
          {"total", total},
          {"successCount", successCount},
          {"failureCount", failureCount},
          {"cancelled", cancelled}};
}

//==========================================================================
// metadata.probeBatchAsync - 异步批量探测，可取消
// params: { paths: string[], includeTags?: boolean }
// Returns: { success: true, operationId, totalCount }
// Events: metadata:probeProgress / metadata:probeComplete
//==========================================================================
json MetadataProbeBatchAsync(const json &params) {
  // paths 的逐项 MediaRead 校验由三参 RegisterApi 的 wrapper 在本函数之前
  // 跑完（BridgeCore.cpp:63-91）。skipInvalid 保持默认 false，所以数组模式
  // 是 fail-fast 整批拒绝（ValidateArrayParam，BridgeCore.cpp:438-469）：
  // 任一条不过本函数就不执行，不产生 operationId。整批拒绝的 code 按失败
  // 类型分派 —— paths 非数组、元素非字符串这类形状错误是 INVALID_PARAMS，
  // 路径安全拒绝才是 PERMISSION_DENIED；两者都是整批 fail-fast，不逐项。
  // 逐项 invalid-path 在这个校验架构下做不出来，是显式取舍。
  // 形状错误按 ErrorEnvelope.h:11 的 {success, error, code} 契约带 code 返回，
  // 否则页面只能去匹配 error 文案。文案本身不动：已被文档引用。
  if (!params.contains("paths") || !params["paths"].is_array()) {
    return ApiEnvelope::MakeError("paths array is required",
                                  ApiErrorCode::INVALID_PARAMS);
  }

  const auto &paths = params["paths"];
  if (paths.empty()) {
    return ApiEnvelope::MakeError("paths array must not be empty",
                                  ApiErrorCode::INVALID_PARAMS);
  }

  try {
    // 分相：handle_create 是 metadb 服务调用，留在主线程；落盘读交给 worker。
    auto targets = std::make_shared<std::vector<ProbeTarget>>();
    targets->reserve(paths.size());

    auto mdb = metadb::get();

    for (const auto &pathItem : paths) {
      ProbeTarget target;
      target.path = pathItem.get<std::string>();

      try {
        // 只识别 |subsong:，不用本文件的 ParseSubsongIndex。后者在
        // path.rfind('#') 之后全为数字时切分（#N 向后兼容分支），于是
        // 无扩展名且以 #<数字> 结尾的文件名会被误切成 "Track " + subsong 2。
        // T1 的输入是用户拖进来的任意文件名，必须避开这条兼容分支。
        // SubsongUtils::ParseSubsongPath 只认 |subsong:，语义由
        // tests/test_subsong_utils.cpp 覆盖。
        auto [cleanPath, subsong] = SubsongUtils::ParseSubsongPath(target.path);

        pfc::string8 canonicalPath;
        filesystem::g_get_canonical_path(cleanPath.c_str(), canonicalPath);
        target.handle = mdb->handle_create(canonicalPath.c_str(), subsong);
      } catch (const exception_aborted&) {
        target.earlyFailure = "read-error";
      } catch (const exception_io_not_found&) {
        target.earlyFailure = "not-found";
      } catch (const exception_io_unsupported_format&) {
        target.earlyFailure = "unsupported-format";
      } catch (const exception_io&) {
        target.earlyFailure = "read-error";
      } catch (const std::exception&) {
        target.earlyFailure = "read-error";
      }

      if (!target.handle.is_valid() && !target.earlyFailure) {
        target.earlyFailure = "read-error";
      }
      targets->push_back(std::move(target));
    }

    const std::string operationId = NextProbeOperationId();
    auto abortToken = std::make_shared<abort_callback_impl>();
    // 窗口维度在主线程于派工前解析：popup 关闭时 PopupWindow::OnDestroy 按
    // windowId 一次取消该窗口发起的全部未完成探测。没有这一步，worker 会把整个
    // 队列跑完，结果发往一个已经不存在的窗口。
    // 归属口径与 file.*Async 同源（同一个 CallerContext.windowId），完整说明
    // 见 AsyncOperationRegistry.h 的 Register 注释。主窗口发起的探测拿到的是
    // "main" 而不是空串，它不被窗口级取消触及的原因是 CancelAllForWindow 的
    // 唯一调用点只传 popup 的 windowId，而非因为归属为空。
    const std::string callerWindowId = CallerContext::FromParams(params).windowId;
    if (!GetProbeRegistry().Register(operationId, abortToken, callerWindowId)) {
      // operationId 撞了就不派工：拿不到取消能力的异步操作不该存在。
      return {{"success", false}, {"error", "Failed to register probe operation"}};
    }
    // 注册已成立但 worker 还不存在。此刻起到派工成功之间的任何抛出都必须把条目
    // 摘掉，否则注册表泄漏 + cancelProbe 假信号，详见 ProbeRegistrationGuard。
    ProbeRegistrationGuard registration(operationId);

    const bool includeTags = params.value("includeTags", true);
    const size_t totalCount = targets->size();

    // 事件路由上下文在主线程取，但只带 _callerHwnd 的值过去：
    // CallerContext 持的是裸 BridgeCore*，面板销毁后跨线程持有会悬垂，
    // 所以在发射前于主线程重新解析（同 AudioApi.cpp:1302-1320 的做法）。
    json callerSeed = json::object();
    if (params.contains("_callerHwnd")) {
      callerSeed["_callerHwnd"] = params["_callerHwnd"];
    }

    // 读盘在 worker。参考 AudioApi.cpp:1299 / LibraryApi.cpp:1712。
    //
    // 整个 worker 体标 noexcept 并自己兜住所有异常：抛出去会落进 fb2k 的
    // 线程池，等于 std::terminate。范式同 HttpApi.cpp:146-194 的外层守卫。
    //
    // 「兜住所有异常」必须连最外层 catch 自己的 handler 体一起兜：C++ 规定
    // handler 体内抛出的异常不由同一个 try 的其他 handler 处理，最外层 handler
    // 一抛就直接冲出 noexcept。所以下面最外层 catch 的体内还嵌了一层 try。
    // 内层那两个 catch handler（探测循环的 std::exception / ... ）不需要这层：
    // 它们位于最外层 try 的体内，抛出会被最外层 handler 接住。
    fb2k::inCpuWorkerThread([targets, abortToken, operationId, includeTags,
                             totalCount, callerSeed]() noexcept {
      try {
        // 每次发射都必须包 fb2k::inMainThread：EmitEvent 最终落到 WebView2
        // COM 对象（STA / UI 线程绑定），从 worker 直接调是跨 apartment 调用。
        // 范例 AudioApi.cpp:1333 / LibraryApi.cpp:1754。
        //
        // 两个事件各自写一个 lambda、事件名在 EmitEvent 调用点写成字面量，
        // 不合并成「事件名当参数」的单个发射器：Graph 的 cpp-parser 按调用点
        // 的 callee 名加字面量参数识别事件，事件名一变成变量就只能靠
        // scripts/graph/data/event-emit-manifest.json 手工登记
        // （http:response 正是这么进去的）。
        auto emitProgress = [callerSeed](json payload) {
          fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
            try {
              auto caller = CallerContext::FromParams(callerSeed);
              caller.EmitEvent("metadata:probeProgress", payload);
            } catch (...) {
              // best-effort：抛进 main-thread callback runner 会 terminate
            }
          });
        };
        auto emitComplete = [callerSeed](json payload) {
          fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
            try {
              auto caller = CallerContext::FromParams(callerSeed);
              caller.EmitEvent("metadata:probeComplete", payload);
            } catch (...) {
              // 同上
            }
          });
        };

        size_t done = 0;
        size_t successCount = 0;
        size_t failureCount = 0;
        bool cancelled = false;

        try {
          // 分批发射，不逐条：单次拖放几十上百条逐条发射就是 IPC 洪泛。
          fb2k_api::BatchEmitScheduler scheduler;
          scheduler.Start(ProbeNowMillis());

          json pending = json::array();

          for (const auto &target : *targets) {
            if (abortToken->is_aborting()) {
              cancelled = true;
              break;
            }

            json item = json::object();
            item["path"] = target.path;

            if (target.earlyFailure) {
              item["success"] = false;
              item["infoSource"] = "none";
              item["failure"] = target.earlyFailure;
              ++failureCount;
            } else {
              ProbeItemOutcome outcome =
                  ProbeOneTrack(target.handle, includeTags, *abortToken);
              if (outcome.aborted) {
                // 取消掉的那一条不入结果：它既不是成功也不是失败原因，
                // 报成 read-error 会让「取消」和「读坏了」分不开。
                cancelled = true;
                break;
              }
              item["success"] = (outcome.failure == nullptr);
              item["infoSource"] = outcome.infoSource;
              if (outcome.failure) {
                item["failure"] = outcome.failure;
                ++failureCount;
              } else {
                item["info"] = std::move(outcome.info);
                if (outcome.hasTags) {
                  item["tags"] = std::move(outcome.tags);
                }
                ++successCount;
              }
            }

            pending.push_back(std::move(item));
            ++done;

            const int64_t now = ProbeNowMillis();
            if (scheduler.ShouldFlush(pending.size(), now)) {
              emitProgress(
                  BuildProbeProgressPayload(operationId, done, totalCount, pending));
              pending = json::array();
              scheduler.MarkFlushed(now);
            }
          }

          // 最后一批不得丢。fb2k::inMainThread 保证 FIFO
          // （threadsLite.h:18-20），所以这一批一定排在 probeComplete 之前。
          if (!pending.empty()) {
            emitProgress(
                BuildProbeProgressPayload(operationId, done, totalCount, pending));
          }
        } catch (const std::exception &e) {
          // 探测循环失败：仍要发 probeComplete，否则页面永远等不到收尾。
          console::printf("metadata.probeBatchAsync: worker failed (%s) op=%s",
                          e.what(), operationId.c_str());
        } catch (...) {
          console::printf("metadata.probeBatchAsync: worker failed (unknown) op=%s",
                          operationId.c_str());
        }

        emitComplete(BuildProbeCompletePayload(operationId, totalCount, successCount,
                                               failureCount, cancelled));
      } catch (...) {
        // 外层守卫。走到这里意味着连 probeComplete 都发不出去（基本只有
        // OOM），页面会等不到收尾事件 —— 记录下来，不要 terminate 宿主。
        //
        // handler 体自己再包一层 try：这里已是最外层 handler，体内抛出的异常
        // 不会被同一个 try 的任何 handler 接住，会直接冲出这个 noexcept lambda。
        // console::printf（SDK/console.h:16）没有 noexcept，写日志失败就把宿主
        // 拖去 terminate 是本末倒置。
        try {
          console::printf("metadata.probeBatchAsync: outer guard, no probeComplete "
                          "emitted for op=%s", operationId.c_str());
        } catch (...) {
        }
      }

      // 摘除必须发生：留下条目会让 abort token 一直存活。所以放在最外层、
      // 与发射路径的成败无关。
      try {
        GetProbeRegistry().Remove(operationId);
      } catch (...) {
      }
    });

    // 派工成立，worker 的最外层负责摘除，守卫不再需要动手。
    registration.Dismiss();

    return {{"success", true},
            {"operationId", operationId},
            {"totalCount", totalCount}};
  } catch (const std::exception &e) {
    return {{"success", false}, {"error", e.what()}};
  }
}

//==========================================================================
// metadata.cancelProbe - 取消一个进行中的探测
// params: { operationId: string }
// Returns: { success: true, cancelled: boolean }
//==========================================================================
json MetadataCancelProbe(const json &params) {
  std::string operationId = params.value("operationId", "");
  if (operationId.empty()) {
    return {{"success", false}, {"error", "operationId is required"}};
  }

  // cancelled=false 表示该 operationId 已结束或不存在。两者对调用方故意
  // 不可区分：一个页面无法分辨自己是差了一微秒还是差了一分钟。
  const bool cancelled = GetProbeRegistry().Cancel(operationId);
  return {{"success", true}, {"cancelled", cancelled}};
}

//==========================================================================
// 退出时取消所有未完成探测
//==========================================================================
//
// 挂 initquit::on_quit 而不是 background_service::Shutdown()：后者被
// g_initialized 门挡着（BackgroundService.cpp:119-121），只有后台模式初始化
// 成功过才往下走；WebView2 UI 当主界面时它第一行就 return，探测照跑不误。
// on_quit 与运行模式无关，按 SDK 约定发生在主窗口销毁之前（initquit.h:2），
// 此时服务系统仍可用；探测 worker 跑在 cpuThreadPool 上，线程池收工属于 core
// 卸载的更后期，所以这里发出的 abort 还来得及被 worker 看见。多个 initquit
// 之间的先后不确定，但这里不碰任何其他服务，与顺序无关。
//
// 反过来，这里也不要去碰 WebView 或 UI：独立 UI 模式下 user_interface::
// shutdown() 已经先跑过（见 WebViewEnvironment.cpp:322-326 的说明）。
//
// 只 abort 不摘条目，摘除仍归 worker（理由见 AsyncOperationRegistry.h 的
// CancelAll 注释）。取消也不是硬中断：worker 每条之间查一次 token，所以退出
// 延迟收敛到「当前这一条读完」，而不是整个剩余队列。
// 实测 10000 条：退出前不取消 5188ms，退出前先取消 107ms，空闲基线 175ms。
class ProbeShutdownInitQuit : public initquit {
public:
  void on_quit() override {
    // 从 on_quit 抛出会打断宿主的关停序列，自己兜住。
    try {
      const size_t cancelledCount = GetProbeRegistry().CancelAll();
      if (cancelledCount > 0) {
        console::printf("metadata.probeBatchAsync: cancelled %u in-flight "
                        "probe(s) on quit",
                        static_cast<unsigned>(cancelledCount));
      }
    } catch (...) {
    }
  }
};

static initquit_factory_t<ProbeShutdownInitQuit> g_probe_shutdown_initquit;

} // anonymous namespace

// Public wrapper for sibling APIs (e.g., LyricsApi embedded tag writing)
nlohmann::json MetadataWriteTags(const nlohmann::json& params) {
    return MetadataWrite(params);
}

//==========================================================================
// 取消指定窗口发起的所有未完成探测（popup 关闭时调用）
//
// 只 abort 不摘条目，摘除仍归 worker —— 与退出取消同一套理由，见
// AsyncOperationRegistry.h 的 CancelAll 注释。
//==========================================================================
void CancelAllProbesForWindow(const std::string& windowId) {
  try {
    const size_t cancelledCount = GetProbeRegistry().CancelAllForWindow(windowId);
    if (cancelledCount > 0) {
      console::printf("metadata.probeBatchAsync: cancelled %u in-flight probe(s) "
                      "for a closing window",
                      static_cast<unsigned>(cancelledCount));
    }
  } catch (...) {
  }
}

//==========================================================================
// Register Metadata API
//==========================================================================
void RegisterMetadataApi() {
  auto &bridge = BridgeCore::GetInstance();

  // metadata.read - Read all tags from a file (structured format)
  bridge.RegisterApi("metadata.read", MetadataRead, {{"path", SecurityLevel::MediaRead}});

  // metadata.readByPath - Read all tags from a file (flat format)
  bridge.RegisterApi("metadata.readByPath", MetadataReadByPath, {{"path", SecurityLevel::MediaRead}});

  // metadata.readRaw - Read tags directly from file, bypassing metadb cache
  bridge.RegisterApi("metadata.readRaw", MetadataReadRaw, {{"path", SecurityLevel::MediaRead}});

  // metadata.readBatch - Batch read metadata from multiple files
  bridge.RegisterApi("metadata.readBatch", MetadataReadBatch, {{"paths", SecurityLevel::MediaRead, true}});

  // metadata.probeBatchAsync - Cancellable async batch probe (worker-thread disk reads)
  // skipInvalid 保持默认 false → 数组模式 fail-fast 整批拒绝
  bridge.RegisterApi("metadata.probeBatchAsync", MetadataProbeBatchAsync,
                     {{"paths", SecurityLevel::MediaRead, true}});

  // metadata.cancelProbe - Cancel an in-flight probe (no path params)
  bridge.RegisterApi("metadata.cancelProbe", MetadataCancelProbe);

  // metadata.write - Write tags to a file
  bridge.RegisterApi("metadata.write", MetadataWrite, {{"path", SecurityLevel::MediaWrite}});

  // metadata.writeBatch - Write tags to multiple files
  bridge.RegisterApi("metadata.writeBatch", MetadataWriteBatch, {{"items", SecurityLevel::MediaWrite, true, "path"}});

  // metadata.embedArtwork - Embed artwork into file
  bridge.RegisterApi("metadata.embedArtwork", MetadataEmbedArtwork, {{"path", SecurityLevel::MediaWrite}});

  // metadata.removeEmbeddedArt - Remove embedded artwork from file
  bridge.RegisterApi("metadata.removeEmbeddedArt", MetadataRemoveEmbeddedArt, {{"path", SecurityLevel::MediaWrite}});

  // metadata.removeTag - Remove tags from file
  bridge.RegisterApi("metadata.removeTag", MetadataRemoveTag, {{"path", SecurityLevel::MediaWrite}});
  
  // metadata.removeField - Alias for removeTag (for compatibility)
  bridge.RegisterApi("metadata.removeField", MetadataRemoveTag, {{"path", SecurityLevel::MediaWrite}});

  // rating.set - Set track rating (0-5)
  bridge.RegisterApi("rating.set", RatingSet, {{"path", SecurityLevel::MediaWrite}});

  // rating.get - Get track rating
  bridge.RegisterApi("rating.get", RatingGet, {{"path", SecurityLevel::MediaRead}});

  LOG("Metadata API registered (14 APIs)");
}
