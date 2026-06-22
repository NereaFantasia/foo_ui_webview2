// MetadataApi.cpp - Metadata Editing API
// Provides tag editing capabilities for audio files

#include "pch.h"
#include "api/MetadataApi.h"
#include "api/BridgeCore.h"
#include "utils/PathSecurity.h"
#include "utils/SubsongUtils.h"
#include <foobar2000/SDK/album_art.h>
#include <foobar2000/SDK/metadb_index.h>
#include <filesystem>
#include <fstream>
#include <set>
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
                                         file_info_impl &info) {
  bool gotInfo = handle->get_info(info);

  if ((!gotInfo || !HasEssentialMetadataFields(info)) && TryReadInfoDirect(canonicalPath, 0, info)) {
    gotInfo = true;
  }

  return gotInfo;
}

//==========================================================================
// Refactored helpers — reduce nesting depth (S134) & cognitive complexity
//==========================================================================

// 从 file_info 收集所有元数据和技术信息到扁平 JSON 对象（大写 key）
// 供 MetadataReadBatch 和 MetadataReadByPath 共享
static json CollectFlatMetadata(const file_info &info, const metadb_handle_ptr& handle) {
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

  t_filesize fileSize = handle->get_filesize();
  if (fileSize != filesize_invalid) {
    tags["FILESIZE"] = std::to_string(fileSize);
  }

  return tags;
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
  if (filename.length() <= 2 || !std::isdigit(filename[0])) {
    return 0;
  }

  size_t endPos = 0;
  while (endPos < filename.length() && std::isdigit(filename[endPos])) {
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
    // Convert path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr handle;
    handle = mdb->handle_create(canonicalPath.c_str(), 0);

    if (!handle.is_valid()) {
      return {
          {"success", false}, {"error", "Failed to open file"}, {"path", path}};
    }

    file_info_impl info;
    if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), info)) {
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
      pfc::string8 canonicalPath;
      filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

      metadb_handle_ptr handle;
      handle = mdb->handle_create(canonicalPath.c_str(), 0);

      if (!handle.is_valid()) {
        results.push_back({{"path", path}, {"success", false}, {"error", "Failed to open file"}});
        errorCount++;
        continue;
      }

      file_info_impl info;
      if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), info)) {
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
    // Convert path to canonical form (essential for Unicode paths)
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

    auto mdb = metadb::get();
    metadb_handle_ptr handle;
    handle = mdb->handle_create(canonicalPath.c_str(), 0);

    if (!handle.is_valid()) {
      return {{"success", false},
              {"error", "Failed to open file"},
              {"path", path},
              {"canonicalPath", canonicalPath.c_str()}};
    }

    file_info_impl info;
    if (!ReadMetadataInfoWithFallback(handle, canonicalPath.c_str(), info)) {
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

} // anonymous namespace

// Public wrapper for sibling APIs (e.g., LyricsApi embedded tag writing)
nlohmann::json MetadataWriteTags(const nlohmann::json& params) {
    return MetadataWrite(params);
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

  LOG("Metadata API registered (12 APIs)");
}
