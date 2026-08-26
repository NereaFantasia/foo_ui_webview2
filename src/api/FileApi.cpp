// FileApi.cpp - File System API
// Provides safe file read/write operations within allowed directories

#include "pch.h"
#include "api/FileApi.h"
#include "api/AsyncOperationRegistry.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "utils/PathExpansion.h"
// PathSecurity validation is now handled by BridgeCore decorator
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <filesystem>
#include <system_error>
#include <ShlObj.h>

namespace fs = std::filesystem;

namespace {
    using json = nlohmann::json;
    
    //==========================================================================
    // Path helpers — 委托给共享 PathExpansion 模块
    //==========================================================================

    std::wstring ExpandPathVariables(const std::string& pathUtf8) {
        return PathExpansion::Expand(pathUtf8);
    }
    
    //==========================================================================
    // Filesystem error helpers
    //==========================================================================

    // filesystem_error 的可外传明细：只给原始 Win32 错误号。
    //
    // 不外传 e.code().message()：那是给程序员看的英文描述，随 MUI 语言包变化，
    // 页面无法拿它做分支。数值码是稳定契约，开发者查表即可。
    // e.what() 拼了 path1/path2，任何情况下都不可外传。
    json FsErrorDetails(const fs::filesystem_error& e) {
        return {{"value", e.code().value()}};
    }

    //==========================================================================
    // file.read - Read file content
    // 安全: 使用统一 PathSecurity 验证
    //==========================================================================
    json FileRead(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string encoding = params.value("encoding", "utf-8");
        
        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            if (!fs::exists(path)) {
                return ApiEnvelope::MakeError("File not found", ApiErrorCode::NOT_FOUND);
            }

            if (!fs::is_regular_file(path)) {
                return ApiEnvelope::MakeError("Path is not a file", ApiErrorCode::INVALID_PATH);
            }

            // Read file
            std::ifstream file;
            if (encoding == "binary") {
                file.open(path, std::ios::binary);
            } else {
                file.open(path, std::ios::in);
            }
            
            if (!file.is_open()) {
                return ApiEnvelope::MakeError("Failed to open file", ApiErrorCode::OPERATION_FAILED);
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            // Get file size
            auto fileSize = fs::file_size(path);
            
            if (encoding == "binary") {
                // Return base64 encoded for binary
                static const char* base64_chars = 
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                
                std::string base64;
                int i = 0;
                unsigned char char_array_3[3];
                unsigned char char_array_4[4];
                const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(content.data());
                size_t in_len = content.size();
                
                while (in_len--) {
                    char_array_3[i++] = *(bytes_to_encode++);
                    if (i == 3) {
                        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                        char_array_4[3] = char_array_3[2] & 0x3f;
                        for (i = 0; i < 4; i++)
                            base64 += base64_chars[char_array_4[i]];
                        i = 0;
                    }
                }
                
                if (i) {
                    for (int j = i; j < 3; j++)
                        char_array_3[j] = '\0';
                    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                    for (int j = 0; j < i + 1; j++)
                        base64 += base64_chars[char_array_4[j]];
                    while (i++ < 3)
                        base64 += '=';
                }
                
                return {
                    {"success", true},
                    {"content", base64},
                    {"size", fileSize},
                    {"encoding", "base64"}
                };
            }
            
            return {
                {"success", true},
                {"content", content},
                {"size", fileSize}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("read failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("read failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.write - Write content to file
    // 安全: 使用更严格的写入权限验证
    //==========================================================================
    json FileWrite(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string content = params.value("content", "");
        std::string encoding = params.value("encoding", "utf-8");
        bool append = params.value("append", false);
        
        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            // Ensure parent directory exists
            fs::path filePath(path);
            fs::path parentDir = filePath.parent_path();
            if (!parentDir.empty() && !fs::exists(parentDir)) {
                fs::create_directories(parentDir);
            }
            
            // Open file
            std::ios_base::openmode mode = std::ios::out;
            if (encoding == "binary") {
                mode |= std::ios::binary;
            }
            if (append) {
                mode |= std::ios::app;
            } else {
                mode |= std::ios::trunc;
            }
            
            std::ofstream file(path, mode);
            if (!file.is_open()) {
                return ApiEnvelope::MakeError("Failed to open file for writing",
                                              ApiErrorCode::OPERATION_FAILED);
            }
            
            // Write content
            if (encoding == "binary" && content.starts_with("base64:")) {
                // Decode base64
                std::string base64 = content.substr(7);
                static const std::string base64_chars = 
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                
                auto is_base64 = [](unsigned char c) -> bool {
                    return (isalnum(c) || (c == '+') || (c == '/'));
                };
                
                std::string decoded;
                int i = 0;
                unsigned char char_array_4[4], char_array_3[3];
                
                for (char c : base64) {
                    if (c == '=') break;
                    if (!is_base64(c)) continue;
                    
                    char_array_4[i++] = c;
                    if (i == 4) {
                        for (i = 0; i < 4; i++)
                            char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
                        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
                        for (i = 0; i < 3; i++)
                            decoded += char_array_3[i];
                        i = 0;
                    }
                }
                
                if (i) {
                    for (int j = i; j < 4; j++)
                        char_array_4[j] = 0;
                    for (int j = 0; j < 4; j++)
                        char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
                    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                    for (int j = 0; j < i - 1; j++)
                        decoded += char_array_3[j];
                }
                
                file.write(decoded.data(), decoded.size());
            } else {
                file << content;
            }
            
            file.close();
            
            // Get written size
            auto writtenSize = fs::file_size(path);
            
            return {
                {"success", true},
                {"bytesWritten", writtenSize}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("write failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("write failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.exists - Check if file or directory exists
    //==========================================================================
    json FileExists(const json& params) {
        std::string pathStr = params.value("path", "");

        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            bool exists = fs::exists(path);
            bool isFile = exists && fs::is_regular_file(path);
            bool isDirectory = exists && fs::is_directory(path);

            return {
                {"exists", exists},
                {"isFile", isFile},
                {"isDirectory", isDirectory}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("exists check failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("exists check failed", ApiErrorCode::OPERATION_FAILED);
        }
    }
    
    //==========================================================================
    // file.list - List directory contents
    //==========================================================================
    json FileList(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string pattern = params.value("pattern", "*");
        bool recursive = params.value("recursive", false);
        
        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            if (!fs::exists(path)) {
                return ApiEnvelope::MakeError("Directory not found", ApiErrorCode::NOT_FOUND);
            }

            if (!fs::is_directory(path)) {
                return ApiEnvelope::MakeError("Path is not a directory", ApiErrorCode::INVALID_PATH);
            }

            json files = json::array();
            json directories = json::array();
            
            // Convert pattern to wstring for matching
            std::wstring wpattern = Utf8ToWide(pattern);
            
            // Simple wildcard matching
            auto matchPattern = [&wpattern](const std::wstring& name) -> bool {
                if (wpattern == L"*" || wpattern == L"*.*") return true;
                
                // Simple extension matching like *.txt
                if (wpattern.length() > 2 && wpattern[0] == L'*' && wpattern[1] == L'.') {
                    std::wstring ext = wpattern.substr(1);
                    size_t dotPos = name.rfind(L'.');
                    if (dotPos != std::wstring::npos) {
                        std::wstring nameExt = name.substr(dotPos);
                        // Case-insensitive comparison
                        std::wstring lowerExt = ext;
                        std::wstring lowerNameExt = nameExt;
                        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::towlower);
                        std::transform(lowerNameExt.begin(), lowerNameExt.end(), lowerNameExt.begin(), ::towlower);
                        return lowerExt == lowerNameExt;
                    }
                    return false;
                }
                
                return true;
            };
            
            if (recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    std::wstring name = entry.path().filename().wstring();
                    if (entry.is_regular_file() && matchPattern(name)) {
                        files.push_back(WideToUtf8(entry.path().wstring()));
                    } else if (entry.is_directory()) {
                        directories.push_back(WideToUtf8(entry.path().wstring()));
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(path)) {
                    std::wstring name = entry.path().filename().wstring();
                    if (entry.is_regular_file() && matchPattern(name)) {
                        files.push_back(WideToUtf8(name));
                    } else if (entry.is_directory()) {
                        directories.push_back(WideToUtf8(name));
                    }
                }
            }
            
            return {
                {"success", true},
                {"files", files},
                {"directories", directories},
                {"items", files}  // alias for test compatibility
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("list failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("list failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.delete - Delete a file
    //==========================================================================
    json FileDelete(const json& params) {
        std::string pathStr = params.value("path", "");
        bool moveToTrash = params.value("moveToTrash", true);

        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            if (!fs::exists(path)) {
                return ApiEnvelope::MakeError("File not found", ApiErrorCode::NOT_FOUND);
            }

            if (moveToTrash) {
                // Use SHFileOperation to move to recycle bin
                std::wstring pathDoubleNull = path + L'\0';  // Double null terminated
                SHFILEOPSTRUCTW fileOp = {};
                fileOp.wFunc = FO_DELETE;
                fileOp.pFrom = pathDoubleNull.c_str();
                fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
                
                int result = SHFileOperationW(&fileOp);
                if (result != 0) {
                    return ApiEnvelope::MakeError("Failed to move to recycle bin",
                                                  ApiErrorCode::OPERATION_FAILED);
                }
            } else {
                fs::remove(path);
            }

            return {{"success", true}};
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("delete failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("delete failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.mkdir - Create directory
    //==========================================================================
    json FileMkdir(const json& params) {
        std::string pathStr = params.value("path", "");

        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            if (fs::exists(path)) {
                if (fs::is_directory(path)) {
                    return {{"success", true}, {"created", false}, {"message", "Directory already exists"}};
                } else {
                    return ApiEnvelope::MakeError("Path exists but is not a directory",
                                                  ApiErrorCode::INVALID_PATH);
                }
            }

            bool created = fs::create_directories(path);

            return {
                {"success", true},
                {"created", created}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("mkdir failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("mkdir failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.copy - Copy file or directory
    //==========================================================================
    json FileCopy(const json& params) {
        std::string srcStr = params.value("source", "");
        std::string destStr = params.value("destination", "");
        bool overwrite = params.value("overwrite", false);

        if (srcStr.empty()) {
            return ApiEnvelope::MakeError("source is required", ApiErrorCode::REQUIRED_PARAM);
        }
        if (destStr.empty()) {
            return ApiEnvelope::MakeError("destination is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring srcPath = ExpandPathVariables(srcStr);
            std::wstring destPath = ExpandPathVariables(destStr);

            if (!fs::exists(srcPath)) {
                return ApiEnvelope::MakeError("Source does not exist", ApiErrorCode::NOT_FOUND);
            }

            auto copyOptions = fs::copy_options::recursive;
            if (overwrite) {
                copyOptions |= fs::copy_options::overwrite_existing;
            } else {
                copyOptions |= fs::copy_options::skip_existing;
            }

            fs::copy(srcPath, destPath, copyOptions);

            return {
                {"success", true},
                {"source", srcStr},
                {"destination", destStr}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("copy failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("copy failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.move - Move file or directory
    //==========================================================================
    json FileMove(const json& params) {
        std::string srcStr = params.value("source", "");
        std::string destStr = params.value("destination", "");

        if (srcStr.empty()) {
            return ApiEnvelope::MakeError("source is required", ApiErrorCode::REQUIRED_PARAM);
        }
        if (destStr.empty()) {
            return ApiEnvelope::MakeError("destination is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring srcPath = ExpandPathVariables(srcStr);
            std::wstring destPath = ExpandPathVariables(destStr);

            if (!fs::exists(srcPath)) {
                return ApiEnvelope::MakeError("Source does not exist", ApiErrorCode::NOT_FOUND);
            }

            fs::rename(srcPath, destPath);

            return {
                {"success", true},
                {"source", srcStr},
                {"destination", destStr}
            };
        } catch (const fs::filesystem_error& e) {
            // Windows 的 ERROR_NOT_SAME_DEVICE 经 MSVC STL 的 _Winerror_map 映射为
            // cross_device_link；跨卷回退（复制后删除）由异步族另行立项。
            // 文件跨卷由 fs::rename 底层的 MOVEFILE_COPY_ALLOWED 静默降级为复制，
            // 不进此分支，所以这里能拿到的必然是目录。
            if (e.code() == std::errc::cross_device_link) {
                return ApiEnvelope::MakeError(
                    "move failed: cross-volume directory move is not supported",
                    ApiErrorCode::NOT_SUPPORTED,
                    {{"reason", "cross-volume"}});
            }
            return ApiEnvelope::MakeError("move failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("move failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.rename - Rename a file or directory
    //==========================================================================
    json FileRename(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string newName = params.value("newName", "");

        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }
        if (newName.empty()) {
            return ApiEnvelope::MakeError("newName is required", ApiErrorCode::REQUIRED_PARAM);
        }

        // Validate newName doesn't contain path separators
        if (newName.find('/') != std::string::npos || newName.find('\\') != std::string::npos) {
            return ApiEnvelope::MakeError("newName cannot contain path separators",
                                          ApiErrorCode::INVALID_PARAMS);
        }

        try {
            std::wstring srcPath = ExpandPathVariables(pathStr);

            if (!fs::exists(srcPath)) {
                return ApiEnvelope::MakeError("Path does not exist", ApiErrorCode::NOT_FOUND);
            }

            fs::path src(srcPath);
            fs::path dest = src.parent_path() / Utf8ToWide(newName);

            if (fs::exists(dest)) {
                return ApiEnvelope::MakeError("A file with the new name already exists",
                                              ApiErrorCode::OPERATION_FAILED);
            }

            fs::rename(src, dest);

            return {
                {"success", true},
                {"oldPath", pathStr},
                {"newPath", WideToUtf8(dest.wstring())}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("rename failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("rename failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.getInfo - Get file information
    //==========================================================================
    json FileGetInfo(const json& params) {
        std::string pathStr = params.value("path", "");

        if (pathStr.empty()) {
            return ApiEnvelope::MakeError("path is required", ApiErrorCode::REQUIRED_PARAM);
        }

        try {
            std::wstring path = ExpandPathVariables(pathStr);

            if (!fs::exists(path)) {
                return {
                    {"success", true},
                    {"exists", false}
                };
            }

            bool isDir = fs::is_directory(path);
            uintmax_t size = isDir ? 0 : fs::file_size(path);
            
            // Get last write time
            auto ftime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            auto time_t_val = std::chrono::system_clock::to_time_t(sctp);

            fs::path p(path);

            return {
                {"success", true},
                {"exists", true},
                {"isDirectory", isDir},
                {"isFile", fs::is_regular_file(path)},
                {"size", size},
                {"modified", time_t_val * 1000}, // JavaScript timestamp (ms)
                {"name", WideToUtf8(p.filename().wstring())},
                {"extension", WideToUtf8(p.extension().wstring())},
                {"parent", WideToUtf8(p.parent_path().wstring())}
            };
        } catch (const fs::filesystem_error& e) {
            return ApiEnvelope::MakeError("getInfo failed", ApiErrorCode::OPERATION_FAILED,
                                          FsErrorDetails(e));
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("getInfo failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // 异步批量文件操作族
    // file.copyAsync / file.moveAsync / file.deleteAsync / file.cancelOp
    //
    // 同步的 file.copy / move / delete 一律阻塞主线程直到整棵目录树处理完,
    // 且中途不可取消。本组补的正是这三件: 不阻塞、可取消、逐条结果分类。
    // 同步四个端点的行为一个字不动。
    //
    // 与 metadata.probeBatchAsync 的差异只有一处: 那边把活派给
    // fb2k::inCpuWorkerThread, 这边每个操作起一条自己的 std::thread。
    // 文件 IO 是长阻塞而非计算, 占着 CPU worker 池会把探测之类的活饿死;
    // 取的是 HttpApi 的形态 (HttpApi.cpp:146-195 的 detach 款)。
    //==========================================================================

    // 同时进行的操作数上限。页面可以无限次调用这三个方法, 没有上限就是
    // "一次点击起一条线程"的资源耗尽面。取值与 HttpApi 的并发闸门同数量级
    // (HttpApi.cpp:93 是 10)。
    constexpr size_t kMaxConcurrentFileOps = 8;

    // 回收站删除每批的条数上限。见 RunTrashItems 的说明。
    constexpr size_t kTrashBatchSize = 16;

    // 逐条 result 的 reason 取值全集, 事件契约定死六个, 不得新增字面量。
    // 下面所有比较都是指针比较: reason 只允许从这六个常量赋值, 每个常量都是
    // 同一个对象, 所以比较的是"来自哪一个常量"而不是字符串内容。
    constexpr const char* kReasonAlreadyExists = "already-exists";
    constexpr const char* kReasonNotFound      = "not-found";
    constexpr const char* kReasonPermission    = "permission";
    constexpr const char* kReasonCrossVolume   = "cross-volume";
    constexpr const char* kReasonIoError       = "io-error";
    constexpr const char* kReasonCancelled     = "cancelled";

    enum class FileOpKind { Copy, Move, Delete };

    // 逐条 result 的 status。用枚举而不是字符串常量: status 参与计数分支,
    // 写错一个字面量就会让 complete 事件的三个计数对不上总数。
    enum class FileOpStatus { Ok, Skipped, Failed };

    const char* FileOpKindName(FileOpKind kind) {
        switch (kind) {
            case FileOpKind::Copy:   return "copy";
            case FileOpKind::Move:   return "move";
            case FileOpKind::Delete: return "delete";
        }
        return "copy";
    }

    const char* FileOpStatusName(FileOpStatus status) {
        switch (status) {
            case FileOpStatus::Ok:      return "ok";
            case FileOpStatus::Skipped: return "skipped";
            case FileOpStatus::Failed:  return "failed";
        }
        return "failed";
    }

    // 一条待处理条目。expanded 版供实际落盘用, echo 版原样回显给页面
    // (与 probe 回显 target.path 同款: 页面传什么就看到什么, 不必反推
    // %music% 展开后的绝对路径)。
    struct FileOpItem {
        std::string sourceEcho;
        std::string destEcho;
        std::wstring source;
        std::wstring destination;
    };

    struct FileOpItemResult {
        std::string source;
        std::string destination;       // delete 分支为空, 不进 payload
        FileOpStatus status = FileOpStatus::Ok;
        const char* reason = nullptr;  // nullptr = 不带 reason 字段
    };

    struct FileOpTally {
        size_t successCount = 0;
        size_t skippedCount = 0;
        size_t failureCount = 0;
        bool cancelled = false;
    };

    // 一次操作的全部输入。worker 只读, 所以整份按值搬进线程。
    struct FileOpRequest {
        FileOpKind kind = FileOpKind::Copy;
        std::string operationId;
        std::vector<FileOpItem> items;
        bool overwrite = false;
        bool moveToTrash = true;
        json callerSeed = json::object();
    };

    int64_t FileOpNowMillis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    //==========================================================================
    // 失败分类
    //==========================================================================

    const char* ReasonFromWin32(DWORD err) {
        switch (err) {
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_DRIVE:
                return kReasonNotFound;
            case ERROR_ACCESS_DENIED:
            case ERROR_WRITE_PROTECT:
            case ERROR_PRIVILEGE_NOT_HELD:
                return kReasonPermission;
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                return kReasonAlreadyExists;
            // 进度回调返回 PROGRESS_CANCEL 之后 CopyFileExW 就是这个码。
            case ERROR_REQUEST_ABORTED:
                return kReasonCancelled;
            case ERROR_NOT_SAME_DEVICE:
                return kReasonCrossVolume;
            default:
                return kReasonIoError;
        }
    }

    // std::filesystem 给出的 error_code 分两种来源: Windows 上多数是
    // system_category 的原始 Win32 错误号, 但标准库也会给 generic_category
    // 的 errc。两种都要认, 否则一半的失败会落进 io-error 兜底。
    const char* ReasonFromErrorCode(const std::error_code& ec) {
        if (ec.category() == std::system_category()) {
            return ReasonFromWin32(static_cast<DWORD>(ec.value()));
        }
        if (ec == std::errc::no_such_file_or_directory) return kReasonNotFound;
        if (ec == std::errc::permission_denied)         return kReasonPermission;
        if (ec == std::errc::file_exists)               return kReasonAlreadyExists;
        if (ec == std::errc::cross_device_link)         return kReasonCrossVolume;
        return kReasonIoError;
    }

    // SHFileOperationW 的返回码是 shell 自有的 DE_* 体系, 不是 Win32 错误号,
    // 数值也不与之对齐, 所以单独一张表。DE_* 常量 shellapi.h 未导出, 按
    // 文档值写死: 0x75 = DE_OPCANCELLED, 0x78 = DE_ACCESSDENIEDSRC。
    // 它也可能直接回 Win32 码, 故两类都列。
    const char* ReasonFromShellError(int code) {
        switch (code) {
            case 0x78:
            case ERROR_ACCESS_DENIED:
                return kReasonPermission;
            case 0x75:
                return kReasonCancelled;
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                return kReasonNotFound;
            default:
                return kReasonIoError;
        }
    }

    // reason 到 status 的唯一映射点。
    // already-exists 与 cancelled 记 skipped 而不是 failed: 这两种是"没做",
    // 报成 failed 会让用户自己按的取消在界面上显示成一堆错误。
    // cross-volume 根本不是失败, 它只说明这一条走的是复制回退而不是改名。
    void ApplyReason(FileOpItemResult& out, const char* reason) {
        out.reason = reason;
        if (!reason || reason == kReasonCrossVolume) {
            out.status = FileOpStatus::Ok;
            return;
        }
        out.status = (reason == kReasonAlreadyExists || reason == kReasonCancelled)
                         ? FileOpStatus::Skipped
                         : FileOpStatus::Failed;
    }

    void TallyResult(FileOpTally& tally, const FileOpItemResult& result) {
        switch (result.status) {
            case FileOpStatus::Ok:      ++tally.successCount; break;
            case FileOpStatus::Skipped: ++tally.skippedCount; break;
            case FileOpStatus::Failed:  ++tally.failureCount; break;
        }
        if (result.reason == kReasonCancelled) {
            tally.cancelled = true;
        }
    }

    //==========================================================================
    // 事件载荷
    //==========================================================================

    // 载荷由具名函数构造, 不在 EmitEvent 调用点摊成 init-list: Graph 的
    // cpp-parser 对这种形状把 payload_schema 记空, 由 sdk/src/types/overrides/
    // events.ts 的 @codegen-override 供型 —— results[] 的元素形状不是
    // extractor 能推出来的。同 MetadataApi.cpp 的 BuildProbeProgressPayload。
    //
    // results 里带路径是允许的: 它是功能数据, 且经 CallerContext 单窗口投递。
    // 错误信封与 console 日志里仍然一个路径都不许出现。
    json FileOpResultToJson(const FileOpItemResult& result) {
        json item = {{"source", result.source},
                     {"status", FileOpStatusName(result.status)}};
        if (!result.destination.empty()) {
            item["destination"] = result.destination;
        }
        if (result.reason) {
            item["reason"] = result.reason;
        }
        return item;
    }

    json BuildFileOpProgressPayload(const std::string& operationId, const char* op,
                                    size_t done, size_t total, const json& results) {
        return {{"operationId", operationId},
                {"op", op},
                {"done", done},
                {"total", total},
                {"results", results}};
    }

    json BuildFileOpCompletePayload(const std::string& operationId, const char* op,
                                    size_t total, size_t successCount,
                                    size_t skippedCount, size_t failureCount,
                                    bool cancelled) {
        return {{"operationId", operationId},
                {"op", op},
                {"total", total},
                {"successCount", successCount},
                {"skippedCount", skippedCount},
                {"failureCount", failureCount},
                {"cancelled", cancelled}};
    }

    //==========================================================================
    // 取消令牌 / 注册表 / 退出闸门
    //==========================================================================

    // 注册表要求令牌类型提供 abort()。这里不用 abort_callback_impl:
    // 本组不走 SDK filesystem 入口, 没有任何要收 abort_callback& 的形参,
    // 换来的只是一个 SDK 依赖 (AsyncOperationRegistry.h:10-23 的选型说明)。
    class FileOpAbortToken {
    public:
        FileOpAbortToken() = default;
        FileOpAbortToken(const FileOpAbortToken&) = delete;
        FileOpAbortToken& operator=(const FileOpAbortToken&) = delete;

        void abort() { aborted_.store(true); }
        bool is_aborting() const { return aborted_.load(); }

    private:
        std::atomic<bool> aborted_{false};
    };

    using FileOpRegistry = fb2k_api::AsyncOperationRegistry<FileOpAbortToken>;

    FileOpRegistry& GetFileOpRegistry() {
        static FileOpRegistry registry;
        return registry;
    }

    // 退出闸门。on_quit 里先落这个再 CancelAll, worker 被唤醒后据此不再碰
    // 任何 fb2k 服务 (发事件 / 写 console)。理由: 这些 worker 是 detach 出去
    // 的, 没人等它们收工, 而 on_quit 之后服务系统随时可能拆掉 —— 那时候
    // main_thread_callback_manager::get() 与 console::printf 都不再安全。
    // 纯标准库的收尾 (注册表 Remove) 不受此闸门影响, 照常执行。
    std::atomic<bool> g_fileOpShuttingDown{false};

    bool FileOpShuttingDown() { return g_fileOpShuttingDown.load(); }

    //==========================================================================
    // 事件发射
    //==========================================================================

    // 每次发射都必须包 fb2k::inMainThread: EmitEvent 最终落到 WebView2 COM
    // 对象 (STA / UI 线程绑定), 从 worker 直接调是跨 apartment 调用。
    // 两个事件各写一个函数、事件名在 EmitEvent 调用点写字面量, 不合并成
    // "事件名当参数"的单个发射器: Graph 的 cpp-parser 按调用点的字面量参数
    // 识别事件, 名字一变成变量就只能靠 event-emit-manifest.json 手工登记。
    void EmitFileOpProgress(const json& callerSeed, json payload) noexcept {
        if (FileOpShuttingDown()) {
            return;
        }
        try {
            fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
                try {
                    auto caller = CallerContext::FromParams(callerSeed);
                    caller.EmitEvent("file:opProgress", payload);
                } catch (...) {
                    // best-effort: 抛进 main-thread callback runner 会 terminate
                }
            });
        } catch (...) {
        }
    }

    void EmitFileOpComplete(const json& callerSeed, json payload) noexcept {
        if (FileOpShuttingDown()) {
            return;
        }
        try {
            fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
                try {
                    auto caller = CallerContext::FromParams(callerSeed);
                    caller.EmitEvent("file:opComplete", payload);
                } catch (...) {
                    // 同上
                }
            });
        } catch (...) {
        }
    }

    //==========================================================================
    // 复制 / 移动 / 删除原语
    //==========================================================================

    // CopyFileExW 的进度回调只做一件事: 查取消。它跑在发起复制的线程
    // (即本组的 worker) 上, 读的是同一个 worker 持有的 token, 不涉跨线程写。
    // 返回 PROGRESS_CANCEL 而不是 PROGRESS_STOP: 前者让系统删掉已写入的目标
    // 残片, 后者会把半个文件留在盘上。
    DWORD CALLBACK FileOpCopyProgress(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER,
                                      LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE,
                                      LPVOID lpData) {
        const auto* token = static_cast<const FileOpAbortToken*>(lpData);
        return (token && token->is_aborting()) ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
    }

    // 目标文件的父目录不存在时先建出来, 与同文件 FileWrite 建父目录的行为
    // 一致。父目录必然位于已通过 FileWrite 校验的 destination 之上同一分支,
    // 不构成越权写。
    void EnsureParentDirectory(const std::wstring& target) {
        std::error_code ec;
        const fs::path parent = fs::path(target).parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
        }
    }

    // 复制一个文件。返回该条的 reason, nullptr 表示成功。
    const char* CopyOneFile(const std::wstring& from, const std::wstring& to,
                            bool overwrite, FileOpAbortToken& token) {
        const DWORD flags = overwrite ? 0u : static_cast<DWORD>(COPY_FILE_FAIL_IF_EXISTS);
        if (CopyFileExW(from.c_str(), to.c_str(), &FileOpCopyProgress, &token,
                        nullptr, flags)) {
            return nullptr;
        }
        return ReasonFromWin32(GetLastError());
    }

    // destination 指向一个已存在的目录时, 文件复制/移动进该目录。
    // 照字面把目录当目标文件名的话, CopyFileExW 只会回 ERROR_ACCESS_DENIED,
    // 页面拿到的 reason 是 permission, 与真实原因无关; 而 std::filesystem
    // 的 copy 本来就是这个语义 (同步 file.copy 走的正是它)。
    std::wstring ResolveFileTarget(const std::wstring& source, const std::wstring& destination) {
        std::error_code ec;
        if (fs::is_directory(destination, ec)) {
            return (fs::path(destination) / fs::path(source).filename()).wstring();
        }
        return destination;
    }

    // 把一个目录条目展开成文件清单逐个复制, 而不是交给 fs::copy。
    // fs::copy 递归大目录期间不可中断, 取消要等它整棵树复制完才生效, 与
    // "取消后不再产生新写入"直接冲突。展开之后取消粒度落到单个文件, 加上
    // CopyFileExW 的进度回调, 文件内部也能停。
    //
    // 目录内已存在的文件按"跳过"处理而不上报: 条目粒度只有一个 result,
    // 用其中一个文件的 already-exists 代表整个目录会掩盖真正的失败。这与
    // 同步 file.copy 的 skip_existing 选项同义。
    const char* CopyDirectoryTree(const std::wstring& from, const std::wstring& to,
                                  bool overwrite, FileOpAbortToken& token) {
        try {
            std::error_code ec;
            fs::create_directories(to, ec);
            if (ec && !fs::is_directory(to, ec)) {
                return ReasonFromErrorCode(ec);
            }
            const char* firstFailure = nullptr;
            // 默认不跟随目录符号链接 (directory_options 未开
            // follow_directory_symlink), 所以源目录里的 junction 无法把递归
            // 引到校验范围之外。
            const auto options = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::recursive_directory_iterator(from, options)) {
                if (token.is_aborting()) {
                    return kReasonCancelled;
                }
                const fs::path target = fs::path(to) / entry.path().lexically_relative(from);
                if (entry.is_directory(ec)) {
                    fs::create_directories(target, ec);
                    continue;
                }
                const char* reason =
                    CopyOneFile(entry.path().wstring(), target.wstring(), overwrite, token);
                if (reason == kReasonCancelled) {
                    return kReasonCancelled;
                }
                if (reason && reason != kReasonAlreadyExists && !firstFailure) {
                    firstFailure = reason;
                }
            }
            return firstFailure;
        } catch (const fs::filesystem_error& e) {
            return ReasonFromErrorCode(e.code());
        } catch (const std::exception&) {
            return kReasonIoError;
        }
    }

    FileOpItemResult RunCopyItem(const FileOpItem& item, bool overwrite,
                                 FileOpAbortToken& token) {
        FileOpItemResult out{item.sourceEcho, item.destEcho};
        std::error_code ec;
        if (!fs::exists(item.source, ec)) {
            ApplyReason(out, kReasonNotFound);
            return out;
        }
        if (fs::is_directory(item.source, ec)) {
            ApplyReason(out, CopyDirectoryTree(item.source, item.destination, overwrite, token));
            return out;
        }
        const std::wstring target = ResolveFileTarget(item.source, item.destination);
        if (!overwrite && fs::exists(target, ec)) {
            ApplyReason(out, kReasonAlreadyExists);
            return out;
        }
        EnsureParentDirectory(target);
        ApplyReason(out, CopyOneFile(item.source, target, overwrite, token));
        return out;
    }

    // 跨卷移动的回退: 复制过去, 全部成功之后再删源。
    // 该条仍算成功, reason 记 cross-volume 只为让页面知道这一条的代价是一次
    // 完整复制 (耗时与可中断性都与同卷改名不同)。
    // 复制成功但源删不掉时报 io-error: 源还在, 那就不是一次移动。
    const char* MoveAcrossVolumes(const std::wstring& source, const std::wstring& target,
                                  bool sourceIsDir, bool overwrite, FileOpAbortToken& token) {
        const char* reason = sourceIsDir
                                 ? CopyDirectoryTree(source, target, overwrite, token)
                                 : CopyOneFile(source, target, overwrite, token);
        if (reason) {
            return reason;
        }
        std::error_code ec;
        if (sourceIsDir) {
            fs::remove_all(source, ec);
        } else {
            fs::remove(source, ec);
        }
        return ec ? kReasonIoError : kReasonCrossVolume;
    }

    // 移动一条。先试 MoveFileExW: 同卷时它是一次改名, 无论目录多大都不落盘
    // 复制, 这是异步移动相对复制的全部价值所在。
    //
    // 不带 MOVEFILE_COPY_ALLOWED: 那个标志会让系统在跨卷时自己做一次复制,
    // 而那次复制既不可中断也不给进度, 等于把 worker 钉死在一个系统调用里;
    // 它对目录也无效。跨卷一律走自己的回退。
    // MOVEFILE_REPLACE_EXISTING 只在 overwrite 为真时给: 同步 file.move
    // 底层的 fs::rename 是无条件覆盖, 本组把它改成显式选项。
    FileOpItemResult RunMoveItem(const FileOpItem& item, bool overwrite,
                                 FileOpAbortToken& token) {
        FileOpItemResult out{item.sourceEcho, item.destEcho};
        std::error_code ec;
        if (!fs::exists(item.source, ec)) {
            ApplyReason(out, kReasonNotFound);
            return out;
        }
        const bool sourceIsDir = fs::is_directory(item.source, ec);
        const std::wstring target =
            sourceIsDir ? item.destination : ResolveFileTarget(item.source, item.destination);
        if (!overwrite && fs::exists(target, ec)) {
            ApplyReason(out, kReasonAlreadyExists);
            return out;
        }
        EnsureParentDirectory(target);
        const DWORD flags = overwrite ? static_cast<DWORD>(MOVEFILE_REPLACE_EXISTING) : 0u;
        if (MoveFileExW(item.source.c_str(), target.c_str(), flags)) {
            ApplyReason(out, nullptr);
            return out;
        }
        const DWORD err = GetLastError();
        if (err != ERROR_NOT_SAME_DEVICE) {
            ApplyReason(out, ReasonFromWin32(err));
            return out;
        }
        ApplyReason(out, MoveAcrossVolumes(item.source, target, sourceIsDir, overwrite, token));
        return out;
    }

    // 永久删除分支。用 remove_all 而不是同步版的 fs::remove: 后者删非空目录
    // 直接失败, 而同一个 API 的回收站分支能删非空目录 —— 两分支目录语义不
    // 一致是同步版的既有缺陷, 本组统一为 remove_all, 同步版按既定口径不动。
    FileOpItemResult RunDeletePermanent(const FileOpItem& item) {
        FileOpItemResult out{item.sourceEcho, std::string()};
        std::error_code ec;
        if (!fs::exists(item.source, ec)) {
            ApplyReason(out, kReasonNotFound);
            return out;
        }
        fs::remove_all(item.source, ec);
        ApplyReason(out, ec ? ReasonFromErrorCode(ec) : nullptr);
        return out;
    }

    // 单条走一次 SHFileOperationW, 不把整批塞进一个 pFrom 列表:
    // 列表形式整批只回一个返回码, 逐条 result 就无从分类。
    const char* DeleteOneToRecycleBin(const std::wstring& path) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            return kReasonNotFound;
        }
        // pFrom 要求双 null 结尾; c_str() 自带一个, 所以只需自己补一个。
        std::wstring pathDoubleNull = path;
        pathDoubleNull.push_back(L'\0');

        SHFILEOPSTRUCTW fileOp = {};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = pathDoubleNull.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

        const int result = SHFileOperationW(&fileOp);
        if (fileOp.fAnyOperationsAborted) {
            return kReasonCancelled;
        }
        return result == 0 ? nullptr : ReasonFromShellError(result);
    }

    //==========================================================================
    // 回收站删除: 主线程分批
    //==========================================================================

    // 一批的执行状态。results 只在 done 为 true 之后由 worker 读一次。
    struct TrashBatchState {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        std::vector<FileOpItemResult> results;
    };

    // 回收站删除必须在主线程: SHFileOperationW 走 shell COM, 是 STA 绑定的,
    // 从 worker 线程调的代价已由拖出 spike 实测过。
    // 每条之前查一次 token: 取消之后这一批剩下的条目不再产生新的删除。
    void RunTrashBatchOnMainThread(const std::vector<FileOpItem>& batch,
                                   const std::shared_ptr<FileOpAbortToken>& token,
                                   const std::shared_ptr<TrashBatchState>& state) noexcept {
        std::vector<FileOpItemResult> results;
        try {
            results.reserve(batch.size());
            for (const auto& item : batch) {
                FileOpItemResult out{item.sourceEcho, std::string()};
                ApplyReason(out, token->is_aborting() ? kReasonCancelled
                                                      : DeleteOneToRecycleBin(item.source));
                results.push_back(std::move(out));
            }
        } catch (...) {
            // 兜住之后 results 与 batch 不等长, worker 据此把这一批记 cancelled
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->results = std::move(results);
            state->done = true;
        }
        state->cv.notify_all();
    }

    // 等主线程那一批跑完。
    // 等待有上限而不是无限: 退出序列里主线程可能再也不抽 main_thread_callback
    // 队列, 无限等会把这条 detach 出去的线程永久挂在那里。回收站删除本身是
    // 秒级, 上限取得宽只为兜住死锁。退出闸门一落就立刻放弃等待。
    bool WaitForTrashBatch(TrashBatchState& state) {
        constexpr int kWaitSliceMs = 50;
        constexpr int kMaxWaitSlices = 1200;  // 50ms * 1200 = 60s
        std::unique_lock<std::mutex> lock(state.mutex);
        for (int slice = 0; slice < kMaxWaitSlices; ++slice) {
            if (state.cv.wait_for(lock, std::chrono::milliseconds(kWaitSliceMs),
                                  [&state] { return state.done; })) {
                return true;
            }
            if (FileOpShuttingDown()) {
                return false;
            }
        }
        return false;
    }

    std::vector<FileOpItemResult> CancelledResultsFor(const std::vector<FileOpItem>& batch) {
        std::vector<FileOpItemResult> results;
        results.reserve(batch.size());
        for (const auto& item : batch) {
            FileOpItemResult out{item.sourceEcho, std::string()};
            ApplyReason(out, kReasonCancelled);
            results.push_back(std::move(out));
        }
        return results;
    }

    // 排一批到主线程并等它跑完。返回空表示这一批没能确认完成 (排不进队列、
    // 退出闸门已落, 或超过等待上限), 由调用方按 cancelled 记账。
    //
    // 排一批等一批, 而不是一口气把所有批都排进去: 后者队列会被主线程一次
    // 抽干, 等于把整批 SHFileOperationW 摊在一个回调里, 分批就白分了。
    std::vector<FileOpItemResult> RunOneTrashBatch(const std::vector<FileOpItem>& batch,
                                                   const std::shared_ptr<FileOpAbortToken>& token) {
        auto state = std::make_shared<TrashBatchState>();
        auto batchCopy = std::make_shared<std::vector<FileOpItem>>(batch);
        try {
            fb2k::inMainThread([batchCopy, token, state]() noexcept {
                RunTrashBatchOnMainThread(*batchCopy, token, state);
            });
        } catch (...) {
            return {};
        }
        if (!WaitForTrashBatch(*state)) {
            // 放弃等待之后不再碰 state->results: 那份状态还被主线程回调按
            // shared_ptr 持着, 它可能仍在往里写。
            return {};
        }
        std::lock_guard<std::mutex> lock(state->mutex);
        return std::move(state->results);
    }

    //==========================================================================
    // 进度分批发射
    //==========================================================================

    // 分批而不是逐条: 一次上百条逐条发射就是 IPC 洪泛, 与 probe 用同一个
    // BatchEmitScheduler 策略 (64 条 / 100ms 先到者)。
    class FileOpProgressEmitter {
    public:
        explicit FileOpProgressEmitter(const FileOpRequest& request) : request_(request) {
            scheduler_.Start(FileOpNowMillis());
        }

        void Add(const FileOpItemResult& result) {
            pending_.push_back(FileOpResultToJson(result));
            ++done_;
            const int64_t now = FileOpNowMillis();
            if (scheduler_.ShouldFlush(pending_.size(), now)) {
                Flush();
                scheduler_.MarkFlushed(now);
            }
        }

        // 残余批不得丢。fb2k::inMainThread 保证 FIFO (threadsLite.h:18-20),
        // 所以收尾时先 Flush 再发 complete, 页面看到的顺序就是这个顺序。
        void Flush() {
            if (pending_.empty()) {
                return;
            }
            EmitFileOpProgress(request_.callerSeed,
                               BuildFileOpProgressPayload(request_.operationId,
                                                          FileOpKindName(request_.kind), done_,
                                                          request_.items.size(), pending_));
            pending_ = json::array();
        }

    private:
        const FileOpRequest& request_;
        fb2k_api::BatchEmitScheduler scheduler_;
        json pending_ = json::array();
        size_t done_ = 0;
    };

    //==========================================================================
    // worker
    //==========================================================================

    FileOpItemResult RunOneItem(const FileOpRequest& request, const FileOpItem& item,
                                FileOpAbortToken& token) {
        switch (request.kind) {
            case FileOpKind::Copy:   return RunCopyItem(item, request.overwrite, token);
            case FileOpKind::Move:   return RunMoveItem(item, request.overwrite, token);
            case FileOpKind::Delete: return RunDeletePermanent(item);
        }
        FileOpItemResult out{item.sourceEcho, item.destEcho};
        ApplyReason(out, kReasonIoError);
        return out;
    }

    // 单条失败只记 result 继续下一条, 不整批中断; 取消之后剩下的条目全部记
    // cancelled, 页面才分得清"没做"与"做失败了"。
    void RunWorkerItems(const FileOpRequest& request, const std::shared_ptr<FileOpAbortToken>& token,
                        FileOpTally& tally, FileOpProgressEmitter& emitter) {
        const bool hasDestination = (request.kind != FileOpKind::Delete);
        for (const auto& item : request.items) {
            FileOpItemResult out;
            if (token->is_aborting()) {
                out = FileOpItemResult{item.sourceEcho,
                                       hasDestination ? item.destEcho : std::string()};
                ApplyReason(out, kReasonCancelled);
            } else {
                out = RunOneItem(request, item, *token);
            }
            TallyResult(tally, out);
            emitter.Add(out);
        }
    }

    void AbsorbTrashResults(const std::vector<FileOpItemResult>& results, FileOpTally& tally,
                            FileOpProgressEmitter& emitter) {
        for (const auto& out : results) {
            TallyResult(tally, out);
            emitter.Add(out);
        }
    }

    void RunTrashItems(const FileOpRequest& request, const std::shared_ptr<FileOpAbortToken>& token,
                       FileOpTally& tally, FileOpProgressEmitter& emitter) {
        const size_t total = request.items.size();
        for (size_t start = 0; start < total; start += kTrashBatchSize) {
            const size_t end = std::min(start + kTrashBatchSize, total);
            const std::vector<FileOpItem> batch(
                request.items.begin() + static_cast<std::ptrdiff_t>(start),
                request.items.begin() + static_cast<std::ptrdiff_t>(end));
            if (token->is_aborting()) {
                // 已取消就不必再往主线程排一趟: 那一趟的结果也全是 cancelled。
                AbsorbTrashResults(CancelledResultsFor(batch), tally, emitter);
                continue;
            }
            std::vector<FileOpItemResult> results = RunOneTrashBatch(batch, token);
            if (results.size() != batch.size()) {
                results = CancelledResultsFor(batch);
            }
            AbsorbTrashResults(results, tally, emitter);
        }
    }

    // 执行循环自己兜住异常, 已累计的 tally 照常返回: 调用方据此仍会发出
    // opComplete, 否则页面永远等不到收尾。这里一个字都不记日志 ——
    // filesystem_error::what() 里拼着 path1/path2, 路径不得进日志。
    FileOpTally ExecuteFileOpItems(const FileOpRequest& request,
                                   const std::shared_ptr<FileOpAbortToken>& token) {
        FileOpTally tally;
        FileOpProgressEmitter emitter(request);
        try {
            if (request.kind == FileOpKind::Delete && request.moveToTrash) {
                RunTrashItems(request, token, tally, emitter);
            } else {
                RunWorkerItems(request, token, tally, emitter);
            }
        } catch (...) {
        }
        try {
            emitter.Flush();
        } catch (...) {
        }
        return tally;
    }

    // worker 线程体。整体标 noexcept 并自己兜住所有异常: 抛出 std::thread 的
    // 线程函数就是 std::terminate, 范式同 HttpApi.cpp:146-194 的外层守卫。
    // 最外层 catch 的 handler 体内再包一层 try: C++ 规定 handler 体内抛出的
    // 异常不由同一个 try 的其他 handler 处理, 一抛就直接冲出 noexcept。
    //
    // 注册表摘除放在最外层, 与发射路径的成败无关: 留下条目会让 abort token
    // 一直存活, 而且此后 cancelOp 对这个 id 永远回 cancelled:true 的假信号。
    void RunFileOpWorker(FileOpRequest request, std::shared_ptr<FileOpAbortToken> token) noexcept {
        try {
            const FileOpTally tally = ExecuteFileOpItems(request, token);
            EmitFileOpComplete(request.callerSeed,
                               BuildFileOpCompletePayload(
                                   request.operationId, FileOpKindName(request.kind),
                                   request.items.size(), tally.successCount, tally.skippedCount,
                                   tally.failureCount, tally.cancelled));
        } catch (...) {
            if (!FileOpShuttingDown()) {
                try {
                    console::printf("file.*Async: outer guard, no opComplete emitted for op=%s",
                                    request.operationId.c_str());
                } catch (...) {
                }
            }
        }
        try {
            GetFileOpRegistry().Remove(request.operationId);
        } catch (...) {
        }
    }

    //==========================================================================
    // 派工
    //==========================================================================

    // 高位随机是为了让页面无法靠观察序号推出别人的 operationId。
    // 只在主线程的 handler 里调用, 所以 mt19937_64 不需要加锁。
    std::string NextFileOpOperationId() {
        static std::mt19937_64 rng(std::random_device{}());
        static std::atomic<uint64_t> counter{0};
        return fb2k_api::FormatAsyncOperationId("fileop", counter.fetch_add(1) + 1, rng());
    }

    // 关掉"已注册但还没派工"这段窗口的泄漏, 同 MetadataApi.cpp 的
    // ProbeRegistrationGuard: Register 成功之后到 std::thread 构造返回之前
    // 还有会抛的语句 (lambda 捕获拷贝的 bad_alloc、线程资源不足), 那些异常
    // 会被 handler 的 catch 接住并返回错误信封, 但注册表条目会永久留下。
    //
    // operationId 存引用不存拷贝: 构造就不会抛, 否则"守卫自己构造失败"又是
    // 同一个泄漏。要求该字符串的生存期覆盖本对象, 调用点是同作用域的局部量。
    class FileOpRegistrationGuard {
    public:
        explicit FileOpRegistrationGuard(const std::string& operationId) noexcept
            : operationId_(operationId) {}

        FileOpRegistrationGuard(const FileOpRegistrationGuard&) = delete;
        FileOpRegistrationGuard& operator=(const FileOpRegistrationGuard&) = delete;

        void Dismiss() noexcept { armed_ = false; }

        ~FileOpRegistrationGuard() {
            if (!armed_) {
                return;
            }
            // 析构可能跑在异常展开中, 抛出去就是 std::terminate。
            try {
                GetFileOpRegistry().Remove(operationId_);
            } catch (...) {
            }
        }

    private:
        const std::string& operationId_;
        bool armed_ = true;
    };

    // 可选 bool 参数: key 缺省时取 fallback, key 在但类型不对时返回 nullopt。
    // 不直接用 params.value("overwrite", false) 是因为它在类型不符时抛
    // json type_error, 那会被 handler 的兜底 catch 变成 OPERATION_FAILED,
    // 而形状错该回 INVALID_PARAMS。
    std::optional<bool> ReadBoolFlag(const json& params, const char* key, bool fallback) {
        if (!params.contains(key)) {
            return fallback;
        }
        if (!params[key].is_boolean()) {
            return std::nullopt;
        }
        return params[key].get<bool>();
    }

    // items 的逐条路径校验由三参 RegisterApi 的 wrapper 在 handler 之前跑完
    // (BridgeCore.cpp:63-91)。copyAsync 在同一个 paramKey 上挂了两条 spec,
    // wrapper 的 spec 循环对每条各调一次 ValidatePathParam, 各自完整走一遍
    // items 数组 (BridgeCore.cpp:495-512 -> ValidateNestedArrayParam), 互不
    // 干扰: 缺 source 与缺 destination 各由自己那条 spec 判成形状错。
    // 这里只补校验器故意跳过的一类 —— 数组元素本身不是对象
    // (BridgeCore.cpp:409-412 的 continue)。
    //
    // 返回值非空即为应当直接回给页面的错误信封。
    std::optional<json> CollectCopyMoveItems(const json& params, std::vector<FileOpItem>& out) {
        if (!params.contains("items") || !params["items"].is_array()) {
            return ApiEnvelope::MakeError("items array is required", ApiErrorCode::INVALID_PARAMS);
        }
        const auto& items = params["items"];
        if (items.empty()) {
            return ApiEnvelope::MakeError("items array must not be empty",
                                          ApiErrorCode::INVALID_PARAMS);
        }
        out.reserve(items.size());
        for (const auto& entry : items) {
            if (!entry.is_object() || !entry.contains("source") || !entry.contains("destination") ||
                !entry["source"].is_string() || !entry["destination"].is_string()) {
                return ApiEnvelope::MakeError("items[] requires string source and destination",
                                              ApiErrorCode::INVALID_PARAMS);
            }
            FileOpItem item;
            item.sourceEcho = entry["source"].get<std::string>();
            item.destEcho = entry["destination"].get<std::string>();
            if (item.sourceEcho.empty() || item.destEcho.empty()) {
                return ApiEnvelope::MakeError("items[] source and destination must not be empty",
                                              ApiErrorCode::INVALID_PARAMS);
            }
            item.source = ExpandPathVariables(item.sourceEcho);
            item.destination = ExpandPathVariables(item.destEcho);
            out.push_back(std::move(item));
        }
        return std::nullopt;
    }

    std::optional<json> CollectDeletePaths(const json& params, std::vector<FileOpItem>& out) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return ApiEnvelope::MakeError("paths array is required", ApiErrorCode::INVALID_PARAMS);
        }
        const auto& paths = params["paths"];
        if (paths.empty()) {
            return ApiEnvelope::MakeError("paths array must not be empty",
                                          ApiErrorCode::INVALID_PARAMS);
        }
        out.reserve(paths.size());
        for (const auto& entry : paths) {
            if (!entry.is_string() || entry.get<std::string>().empty()) {
                return ApiEnvelope::MakeError("paths[] must be non-empty strings",
                                              ApiErrorCode::INVALID_PARAMS);
            }
            FileOpItem item;
            item.sourceEcho = entry.get<std::string>();
            item.source = ExpandPathVariables(item.sourceEcho);
            out.push_back(std::move(item));
        }
        return std::nullopt;
    }

    // 三个方法共同的尾段: 起 id、注册、派线程、回收据。
    json DispatchFileOp(FileOpKind kind, const json& params, std::vector<FileOpItem> items,
                        bool overwrite, bool moveToTrash) {
        // 页面可以无限次调这三个方法, 没有闸门就是"一次点击起一条线程"。
        if (GetFileOpRegistry().Size() >= kMaxConcurrentFileOps) {
            return ApiEnvelope::MakeError("too many concurrent file operations",
                                          ApiErrorCode::OPERATION_FAILED);
        }
        try {
            FileOpRequest request;
            request.kind = kind;
            request.items = std::move(items);
            request.overwrite = overwrite;
            request.moveToTrash = moveToTrash;
            // 事件路由上下文在主线程取, 但只把 _callerHwnd 的值带过去:
            // CallerContext 持的是裸 BridgeCore*, 面板销毁后跨线程持有会悬垂,
            // 所以发射前在主线程重新解析 (同 MetadataApi.cpp 的
            // MetadataProbeBatchAsync 取 callerSeed 的做法)。
            if (params.contains("_callerHwnd")) {
                request.callerSeed["_callerHwnd"] = params["_callerHwnd"];
            }

            const std::string operationId = NextFileOpOperationId();
            const size_t totalCount = request.items.size();
            request.operationId = operationId;

            auto token = std::make_shared<FileOpAbortToken>();
            // 窗口维度也在主线程解析: popup 关闭时按 windowId 一次取消它发起
            // 的全部未完成操作, 否则 worker 会把整队跑完, 事件发往已销毁的窗口。
            const std::string windowId = CallerContext::FromParams(params).windowId;
            if (!GetFileOpRegistry().Register(operationId, token, windowId)) {
                // id 撞了就不派工: 拿不到取消能力的异步操作不该存在。
                return ApiEnvelope::MakeError("failed to register file operation",
                                              ApiErrorCode::OPERATION_FAILED);
            }
            FileOpRegistrationGuard registration(operationId);

            std::thread(RunFileOpWorker, std::move(request), token).detach();
            registration.Dismiss();

            return {{"success", true}, {"operationId", operationId}, {"totalCount", totalCount}};
        } catch (const std::exception&) {
            return ApiEnvelope::MakeError("failed to start file operation",
                                          ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.copyAsync - 异步批量复制, 可取消
    // params: { items: [{source, destination}], overwrite?: false }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    json FileCopyAsync(const json& params) {
        std::vector<FileOpItem> items;
        if (auto error = CollectCopyMoveItems(params, items)) {
            return *error;
        }
        const std::optional<bool> overwrite = ReadBoolFlag(params, "overwrite", false);
        if (!overwrite.has_value()) {
            return ApiEnvelope::MakeError("overwrite must be a boolean",
                                          ApiErrorCode::INVALID_PARAMS);
        }
        return DispatchFileOp(FileOpKind::Copy, params, std::move(items), *overwrite, true);
    }

    //==========================================================================
    // file.moveAsync - 异步批量移动, 可取消, 跨卷自动回退成复制加删源
    // params: { items: [{source, destination}], overwrite?: false }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    json FileMoveAsync(const json& params) {
        std::vector<FileOpItem> items;
        if (auto error = CollectCopyMoveItems(params, items)) {
            return *error;
        }
        const std::optional<bool> overwrite = ReadBoolFlag(params, "overwrite", false);
        if (!overwrite.has_value()) {
            return ApiEnvelope::MakeError("overwrite must be a boolean",
                                          ApiErrorCode::INVALID_PARAMS);
        }
        return DispatchFileOp(FileOpKind::Move, params, std::move(items), *overwrite, true);
    }

    //==========================================================================
    // file.deleteAsync - 异步批量删除, 可取消
    // params: { paths: string[], moveToTrash?: true }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    json FileDeleteAsync(const json& params) {
        std::vector<FileOpItem> items;
        if (auto error = CollectDeletePaths(params, items)) {
            return *error;
        }
        const std::optional<bool> moveToTrash = ReadBoolFlag(params, "moveToTrash", true);
        if (!moveToTrash.has_value()) {
            return ApiEnvelope::MakeError("moveToTrash must be a boolean",
                                          ApiErrorCode::INVALID_PARAMS);
        }
        return DispatchFileOp(FileOpKind::Delete, params, std::move(items), false, *moveToTrash);
    }

    //==========================================================================
    // file.cancelOp - 取消一个进行中的异步文件操作
    // params: { operationId: string }
    // Returns: { success: true, cancelled: boolean }
    //==========================================================================
    json FileCancelOp(const json& params) {
        if (!params.contains("operationId") || !params["operationId"].is_string()) {
            return ApiEnvelope::MakeError("operationId is required", ApiErrorCode::INVALID_PARAMS);
        }
        const std::string operationId = params["operationId"].get<std::string>();
        if (operationId.empty()) {
            return ApiEnvelope::MakeError("operationId is required", ApiErrorCode::INVALID_PARAMS);
        }
        // cancelled=false 表示该 operationId 已结束或不存在。两者对调用方故意
        // 不可区分: 一个页面无法分辨自己是差了一微秒还是差了一分钟。
        return {{"success", true}, {"cancelled", GetFileOpRegistry().Cancel(operationId)}};
    }

    //==========================================================================
    // 退出时取消所有未完成操作
    //==========================================================================
    //
    // 挂 initquit::on_quit 而不是 background_service::Shutdown(): 后者被
    // g_initialized 门挡着, 只有后台模式初始化成功过才往下走。理由与
    // MetadataApi.cpp 的 ProbeShutdownInitQuit 完全相同。
    //
    // 与那条并列而不是合并成一条: 两个注册表分居两个编译单元, 合并要把
    // CancelAll 反向暴露给对方模块, 换来的只是少一个 initquit 实例。
    //
    // 顺序要紧 —— 先落退出闸门再 CancelAll, 否则被唤醒的 worker 会赶在闸门
    // 之前去发事件。只 abort 不 join: 取消不是硬中断, worker 每条之间查一次
    // token, 退出延迟收敛到"当前这一条做完"而不是整个剩余队列。
    class FileOpShutdownInitQuit : public initquit {
    public:
        void on_quit() override {
            // 从 on_quit 抛出会打断宿主的关停序列, 自己兜住。
            try {
                g_fileOpShuttingDown.store(true);
                const size_t cancelledCount = GetFileOpRegistry().CancelAll();
                if (cancelledCount > 0) {
                    console::printf("file.*Async: cancelled %u in-flight file operation(s) on quit",
                                    static_cast<unsigned>(cancelledCount));
                }
            } catch (...) {
            }
        }
    };

    static initquit_factory_t<FileOpShutdownInitQuit> g_fileop_shutdown_initquit;

} // anonymous namespace

//==========================================================================
// 取消指定窗口发起的所有未完成文件操作 (popup 关闭时调用)
//==========================================================================
void CancelAllFileOpsForWindow(const std::string& windowId) {
    try {
        const size_t cancelledCount = GetFileOpRegistry().CancelAllForWindow(windowId);
        if (cancelledCount > 0) {
            console::printf("file.*Async: cancelled %u in-flight file operation(s) for a "
                            "closing window",
                            static_cast<unsigned>(cancelledCount));
        }
    } catch (...) {
    }
}

//==========================================================================
// Register File API
//==========================================================================
void RegisterFileApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // file.read - Read file content
    bridge.RegisterApi("file.read", FileRead, {{"path", SecurityLevel::Read}});
    
    // file.write - Write content to file
    //
    // 以下六个写端点使用 FileWrite 而非 MediaWrite: 它们操作的是任意文件,
    // 不是媒体上下文中的文件。此前挂在 MediaWrite 上, 使"非系统盘直通"这一
    // 通用文件写策略混进了媒体写入语义。统一权限架构 §7.2 要求它们最终归入
    // Write (profile/temp), 该改归属破坏性变更, 单独决策。
    bridge.RegisterApi("file.write", FileWrite, {{"path", SecurityLevel::FileWrite}});
    
    // file.exists - Check if file/directory exists
    bridge.RegisterApi("file.exists", FileExists, {{"path", SecurityLevel::Read}});
    
    // file.list - List directory contents
    bridge.RegisterApi("file.list", FileList, {{"path", SecurityLevel::Read}});
    
    // file.delete - Delete a file
    bridge.RegisterApi("file.delete", FileDelete, {{"path", SecurityLevel::FileWrite}});
    
    // file.mkdir - Create directory
    bridge.RegisterApi("file.mkdir", FileMkdir, {{"path", SecurityLevel::FileWrite}});

    // file.copy - Copy file or directory
    bridge.RegisterApi("file.copy", FileCopy, {
        {"source", SecurityLevel::Read},
        {"destination", SecurityLevel::FileWrite}
    });

    // file.move - Move file or directory
    bridge.RegisterApi("file.move", FileMove, {
        {"source", SecurityLevel::FileWrite},
        {"destination", SecurityLevel::FileWrite}
    });

    // file.rename - Rename a file or directory
    bridge.RegisterApi("file.rename", FileRename, {{"path", SecurityLevel::FileWrite}});

    // file.getInfo - Get file information
    bridge.RegisterApi("file.getInfo", FileGetInfo, {{"path", SecurityLevel::Read}});

    // file.copyAsync - 异步批量复制 (worker 线程, 可取消)
    //
    // 同一个 paramKey 上挂两条 spec: wrapper 的 spec 循环对每条各调一次
    // ValidatePathParam (BridgeCore.cpp:69-79), 每次完整走一遍 items 数组并
    // 只看自己那个 nestedKey, 两条互不干扰。档位与同步版 file.copy 一致 ——
    // source 读、destination 写。
    bridge.RegisterApi("file.copyAsync", FileCopyAsync, {
        {"items", SecurityLevel::Read, true, "source"},
        {"items", SecurityLevel::FileWrite, true, "destination"}
    });

    // file.moveAsync - 异步批量移动 (跨卷自动回退成复制加删源, 可取消)
    // 两端都是写: 移动会删掉 source。与同步版 file.move 同档。
    bridge.RegisterApi("file.moveAsync", FileMoveAsync, {
        {"items", SecurityLevel::FileWrite, true, "source"},
        {"items", SecurityLevel::FileWrite, true, "destination"}
    });

    // file.deleteAsync - 异步批量删除 (可取消)
    bridge.RegisterApi("file.deleteAsync", FileDeleteAsync,
                       {{"paths", SecurityLevel::FileWrite, true}});

    // file.cancelOp - 取消一个进行中的异步文件操作 (无路径参数)
    bridge.RegisterApi("file.cancelOp", FileCancelOp);

    LOG("File API registered (14 APIs)");
}
