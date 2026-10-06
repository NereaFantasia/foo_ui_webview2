// FileApi.cpp - File System API
// Provides safe file read/write operations within allowed directories
//
// Shapes are declared in src/api/schema/file.ts; the parameter structs, the
// parameter reader and the result structs come from the generated
// FileSchema.h, and the path security levels from its kPathParams.

#include "pch.h"
#include "api/FileApi.h"
#include "api/AsyncOperationRegistry.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/FileSchema.h"
#include "utils/PathExpansion.h"
// PathSecurity validation is now handled by BridgeCore decorator
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <filesystem>
#include <format>
#include <string_view>
#include <system_error>
#include <variant>
#include <ShlObj.h>

namespace fs = std::filesystem;

namespace {
    using json = nlohmann::json;
    // 别名不叫 file / fs：foobar2000 SDK 的 foobar2000_io::file 经 using 进了全局，
    // fs 已是 std::filesystem。
    namespace fileapi = api::file;
    
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

    // foobar2000.exe 没有声明长路径支持，本进程的 Win32 文件调用受 MAX_PATH 限制，完整路径
    // 最多 259 个字符。超出时系统报 ERROR_PATH_NOT_FOUND，与真的不存在分不开，fs::exists
    // 还会把存在的文件答成不存在，所以先按完整路径的长度拦下，明细给 ERROR_FILENAME_EXCED_RANGE。
    // 新建文件夹的上限是 247 个字符，那里系统自己就报 ERROR_FILENAME_EXCED_RANGE，不另拦。
    constexpr size_t kMaxPathChars = MAX_PATH - 1;

    bool ExceedsMaxPath(const fs::path& path) {
        std::error_code ec;
        const fs::path full = fs::absolute(path, ec);
        return (ec ? path : full).native().size() > kMaxPathChars;
    }

    std::optional<api::Failure> PathTooLong(const fs::path& path, std::string_view what = "path") {
        if (!ExceedsMaxPath(path)) return std::nullopt;
        return api::Fail(std::format("{} is longer than {} characters", what, kMaxPathChars),
                         ApiErrorCode::OPERATION_FAILED,
                         {{"details", json{{"value", static_cast<int>(ERROR_FILENAME_EXCED_RANGE)}}}});
    }

    // 文件流打不开时，CRT 把 CreateFileW 的错误号留在 _doserrno。调用方打开前清零、失败后
    // 读出；非零才放进明细，与 FsErrorDetails 同形。
    api::Failure OpenFailure(const char* message, unsigned long osError) {
        if (osError == 0) return api::Fail(message, ApiErrorCode::OPERATION_FAILED);
        return api::Fail(message, ApiErrorCode::OPERATION_FAILED,
                         {{"details", json{{"value", static_cast<int>(osError)}}}});
    }

    //==========================================================================
    // file.read - Read file content
    // 安全: 使用统一 PathSecurity 验证
    //==========================================================================
    api::Result<fileapi::ReadResult> FileRead(const fileapi::ReadParams& p) {
        const std::string& encoding = p.encoding;

        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            if (!fs::exists(path)) {
                return api::Fail("File not found", ApiErrorCode::NOT_FOUND);
            }

            if (!fs::is_regular_file(path)) {
                return api::Fail("Path is not a file", ApiErrorCode::INVALID_PATH);
            }

            // Read file
            std::ifstream file;
            _doserrno = 0;
            if (encoding == "binary") {
                file.open(path, std::ios::binary);
            } else {
                file.open(path, std::ios::in);
            }

            if (!file.is_open()) {
                return OpenFailure("Failed to open file", _doserrno);
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            // Get file size
            auto fileSize = fs::file_size(path);

            fileapi::ReadResult result;
            result.size = static_cast<std::int64_t>(fileSize);
            
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
                
                result.content = std::move(base64);
                result.encoding = "base64";
                return result;
            }
            
            result.content = std::move(content);
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("read failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("read failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.write - Write content to file
    // 安全: 使用更严格的写入权限验证
    //==========================================================================

    // 解码 file.write 的 `base64:` 载荷：字母表以外的字符跳过，遇到第一个 `=` 停止。
    std::string DecodeWireBase64(const std::string& base64) {
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
        return decoded;
    }

    // 普通写入与原子写入共用：两者写出的字节必须一致，原子写入只改变落盘方式。
    void WriteContent(std::ofstream& file, const std::string& content, const std::string& encoding) {
        if (encoding == "binary" && content.starts_with("base64:")) {
            const std::string decoded = DecodeWireBase64(content.substr(7));
            file.write(decoded.data(), static_cast<std::streamsize>(decoded.size()));
        } else {
            file << content;
        }
    }

    // 原子写入的临时文件与目标同目录，替换才是同一卷上的一次改名。名字只由进程号与序号
    // 组成，不带目标文件名：进程没有声明长路径支持，整条路径受 MAX_PATH 限制，临时名
    // 不随目标文件名变长，目标路径能写的，临时路径通常也写得下。
    fs::path AtomicTempPathFor(const fs::path& target) {
        static std::atomic<std::uint32_t> sequence{0};
        const std::wstring name = L".~" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                  std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed)) + L".tmp";
        return target.parent_path() / name;
    }

    // ofstream 关闭只把数据交给系统缓存。替换前不先落盘的话，断电后可能留下
    // 已经改了名、内容却不完整的目标。返回 Win32 错误号，成功时为 ERROR_SUCCESS。
    DWORD FlushToDisk(const fs::path& file) {
        HANDLE handle = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return GetLastError();
        const DWORD error = FlushFileBuffers(handle) ? ERROR_SUCCESS : GetLastError();
        CloseHandle(handle);
        return error;
    }

    // 写临时文件、落盘、一次改名替换 target。任何一步失败都删掉临时文件，
    // target 保持原样；失败明细与 FsErrorDetails 同形，只给 Win32 错误号。
    std::optional<api::Failure> WriteAtomically(const fs::path& target, std::ios_base::openmode mode,
                                                const std::string& content, const std::string& encoding) {
        // target 是解析掉链接之后的位置，可能比页面传来的路径长，两条都要量。
        if (auto tooLong = PathTooLong(target)) return tooLong;
        const fs::path temp = AtomicTempPathFor(target);
        if (auto tooLong = PathTooLong(temp, "temporary file path")) return tooLong;
        const auto failWith = [&temp](DWORD error) {
            DeleteFileW(temp.c_str());
            return api::Fail("write failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", json{{"value", static_cast<int>(error)}}}});
        };

        {
            _doserrno = 0;
            std::ofstream file(temp, mode);
            if (!file.is_open()) {
                return OpenFailure("Failed to open file for writing", _doserrno);
            }
            WriteContent(file, content, encoding);
            file.close();
            if (file.fail()) {
                DeleteFileW(temp.c_str());
                return api::Fail("write failed", ApiErrorCode::OPERATION_FAILED);
            }
        }

        if (const DWORD error = FlushToDisk(temp); error != ERROR_SUCCESS) return failWith(error);
        if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return failWith(GetLastError());
        }
        return std::nullopt;
    }

    api::Result<fileapi::WriteResult> FileWrite(const fileapi::WriteParams& p) {
        const std::string& content = p.content;
        const std::string& encoding = p.encoding;
        const bool append = p.append;

        if (p.atomic && append) {
            return api::Fail("atomic cannot be combined with append", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

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
            
            if (p.atomic) {
                // 路径校验（PathSecurity::ValidateFileWriteAccess）按解析掉符号链接与目录联接之后的
                // 位置放行，临时文件和替换也要落在那里，不能落在链接所在的目录。
                const fs::path target = fs::weakly_canonical(filePath);
                if (auto failure = WriteAtomically(target, mode, content, encoding)) return std::move(*failure);
            } else {
                _doserrno = 0;
                std::ofstream file(path, mode);
                if (!file.is_open()) {
                    return OpenFailure("Failed to open file for writing", _doserrno);
                }
                WriteContent(file, content, encoding);
                file.close();
            }
            
            // Get written size
            auto writtenSize = fs::file_size(path);
            
            fileapi::WriteResult result;
            result.bytesWritten = static_cast<std::int64_t>(writtenSize);
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("write failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("write failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.exists - Check if file or directory exists
    //==========================================================================
    api::Result<fileapi::ExistsResult> FileExists(const fileapi::ExistsParams& p) {
        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            bool exists = fs::exists(path);
            bool isFile = exists && fs::is_regular_file(path);
            bool isDirectory = exists && fs::is_directory(path);

            fileapi::ExistsResult result;
            result.exists = exists;
            result.isFile = isFile;
            result.isDirectory = isDirectory;
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("exists check failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("exists check failed", ApiErrorCode::OPERATION_FAILED);
        }
    }
    
    // Simple wildcard matching for file.list. `*` and `*.*` match everything,
    // `*.ext` compares extensions case-insensitively (no match without a dot),
    // and every other pattern matches everything too; that last rule is a
    // pinned behaviour (e2e A-09), not an oversight to tighten here.
    bool MatchListPattern(const std::wstring& wpattern, const std::wstring& name) {
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
    }

    //==========================================================================
    // file.list - List directory contents
    //==========================================================================
    api::Result<fileapi::ListResult> FileList(const fileapi::ListParams& p) {
        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            if (!fs::exists(path)) {
                return api::Fail("Directory not found", ApiErrorCode::NOT_FOUND);
            }

            if (!fs::is_directory(path)) {
                return api::Fail("Path is not a directory", ApiErrorCode::INVALID_PATH);
            }

            fileapi::ListResult result;
            
            // Convert pattern to wstring for matching
            const std::wstring wpattern = Utf8ToWide(p.pattern);

            if (p.recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(path)) {
                    std::wstring name = entry.path().filename().wstring();
                    if (entry.is_regular_file() && MatchListPattern(wpattern, name)) {
                        result.files.push_back(WideToUtf8(entry.path().wstring()));
                    } else if (entry.is_directory()) {
                        result.directories.push_back(WideToUtf8(entry.path().wstring()));
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(path)) {
                    std::wstring name = entry.path().filename().wstring();
                    if (entry.is_regular_file() && MatchListPattern(wpattern, name)) {
                        result.files.push_back(WideToUtf8(name));
                    } else if (entry.is_directory()) {
                        result.directories.push_back(WideToUtf8(name));
                    }
                }
            }
            
            // items is kept as an alias of files: the SMP compatibility layer
            // (Glob / ListFiles) reads it.
            result.items = result.files;
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("list failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("list failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.delete - Delete a file
    //==========================================================================
    api::Result<void> FileDelete(const fileapi::DeleteParams& p) {
        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            if (!fs::exists(path)) {
                return api::Fail("File not found", ApiErrorCode::NOT_FOUND);
            }

            if (p.moveToTrash) {
                // Use SHFileOperation to move to recycle bin
                std::wstring pathDoubleNull = path + L'\0';  // Double null terminated
                SHFILEOPSTRUCTW fileOp = {};
                fileOp.wFunc = FO_DELETE;
                fileOp.pFrom = pathDoubleNull.c_str();
                fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
                
                int result = SHFileOperationW(&fileOp);
                if (result != 0) {
                    return api::Fail("Failed to move to recycle bin", ApiErrorCode::OPERATION_FAILED);
                }
            } else {
                fs::remove(path);
            }

            return api::Ok();
        } catch (const fs::filesystem_error& e) {
            return api::Fail("delete failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("delete failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.mkdir - Create directory
    //==========================================================================
    api::Result<fileapi::MkdirResult> FileMkdir(const fileapi::MkdirParams& p) {
        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            fileapi::MkdirResult result;
            if (fs::exists(path)) {
                if (fs::is_directory(path)) {
                    result.created = false;
                    result.message = "Directory already exists";
                    return result;
                } else {
                    return api::Fail("Path exists but is not a directory", ApiErrorCode::INVALID_PATH);
                }
            }

            result.created = fs::create_directories(path);
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("mkdir failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("mkdir failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.copy - Copy file or directory
    //==========================================================================
    api::Result<fileapi::CopyResult> FileCopy(const fileapi::CopyParams& p) {
        try {
            std::wstring srcPath = ExpandPathVariables(p.source);
            std::wstring destPath = ExpandPathVariables(p.destination);
            if (auto tooLong = PathTooLong(srcPath)) return std::move(*tooLong);
            if (auto tooLong = PathTooLong(destPath)) return std::move(*tooLong);

            if (!fs::exists(srcPath)) {
                return api::Fail("Source does not exist", ApiErrorCode::NOT_FOUND);
            }

            auto copyOptions = fs::copy_options::recursive;
            if (p.overwrite) {
                copyOptions |= fs::copy_options::overwrite_existing;
            } else {
                copyOptions |= fs::copy_options::skip_existing;
            }

            fs::copy(srcPath, destPath, copyOptions);

            fileapi::CopyResult result;
            result.source = p.source;
            result.destination = p.destination;
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("copy failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("copy failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.move - Move file or directory
    //==========================================================================
    api::Result<fileapi::MoveResult> FileMove(const fileapi::MoveParams& p) {
        try {
            std::wstring srcPath = ExpandPathVariables(p.source);
            std::wstring destPath = ExpandPathVariables(p.destination);
            if (auto tooLong = PathTooLong(srcPath)) return std::move(*tooLong);
            if (auto tooLong = PathTooLong(destPath)) return std::move(*tooLong);

            if (!fs::exists(srcPath)) {
                return api::Fail("Source does not exist", ApiErrorCode::NOT_FOUND);
            }

            fs::rename(srcPath, destPath);

            fileapi::MoveResult result;
            result.source = p.source;
            result.destination = p.destination;
            return result;
        } catch (const fs::filesystem_error& e) {
            // Windows 的 ERROR_NOT_SAME_DEVICE 经 MSVC STL 的 _Winerror_map 映射为
            // cross_device_link；跨卷回退（复制后删除）由异步族另行立项。
            // 文件跨卷由 fs::rename 底层的 MOVEFILE_COPY_ALLOWED 静默降级为复制，
            // 不进此分支，所以这里能拿到的必然是目录。
            if (e.code() == std::errc::cross_device_link) {
                return api::Fail("move failed: cross-volume directory move is not supported",
                                 ApiErrorCode::NOT_SUPPORTED,
                                 {{"details", json{{"reason", "cross-volume"}}}});
            }
            return api::Fail("move failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("move failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.rename - Rename a file or directory
    //==========================================================================
    api::Result<fileapi::RenameResult> FileRename(const fileapi::RenameParams& p) {
        const std::string& newName = p.newName;

        // Validate newName doesn't contain path separators
        if (newName.find('/') != std::string::npos || newName.find('\\') != std::string::npos) {
            return api::Fail("newName cannot contain path separators", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            std::wstring srcPath = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(srcPath)) return std::move(*tooLong);

            if (!fs::exists(srcPath)) {
                return api::Fail("Path does not exist", ApiErrorCode::NOT_FOUND);
            }

            fs::path src(srcPath);
            fs::path dest = src.parent_path() / Utf8ToWide(newName);
            if (auto tooLong = PathTooLong(dest)) return std::move(*tooLong);

            if (fs::exists(dest)) {
                return api::Fail("A file with the new name already exists", ApiErrorCode::OPERATION_FAILED);
            }

            fs::rename(src, dest);

            fileapi::RenameResult result;
            result.oldPath = p.path;
            result.newPath = WideToUtf8(dest.wstring());
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("rename failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("rename failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // file.getInfo - Get file information
    //==========================================================================
    api::Result<fileapi::GetInfoResult> FileGetInfo(const fileapi::GetInfoParams& p) {
        try {
            std::wstring path = ExpandPathVariables(p.path);
            if (auto tooLong = PathTooLong(path)) return std::move(*tooLong);

            // A missing path answers { success, exists: false } and nothing
            // else: every other field of the result stays unset.
            fileapi::GetInfoResult result;
            if (!fs::exists(path)) {
                result.exists = false;
                return result;
            }

            bool isDir = fs::is_directory(path);
            uintmax_t size = isDir ? 0 : fs::file_size(path);
            
            // Get last write time
            auto ftime = fs::last_write_time(path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            auto time_t_val = std::chrono::system_clock::to_time_t(sctp);

            fs::path target(path);

            result.exists = true;
            result.isDirectory = isDir;
            result.isFile = fs::is_regular_file(path);
            result.size = static_cast<std::int64_t>(size);
            result.modified = static_cast<std::int64_t>(time_t_val) * 1000; // JavaScript timestamp (ms)
            result.name = WideToUtf8(target.filename().wstring());
            result.extension = WideToUtf8(target.extension().wstring());
            result.parent = WideToUtf8(target.parent_path().wstring());
            return result;
        } catch (const fs::filesystem_error& e) {
            return api::Fail("getInfo failed", ApiErrorCode::OPERATION_FAILED,
                             {{"details", FsErrorDetails(e)}});
        } catch (const std::exception&) {
            return api::Fail("getInfo failed", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // 异步批量文件操作族
    // file.copyAsync / file.moveAsync / file.deleteAsync / file.cancelOp
    //
    // 同步的 file.copy / move / delete 一律阻塞主线程直到整棵目录树处理完,
    // 且中途不可取消。本组补的正是这三件: 不阻塞、可取消、逐条结果分类。
    //
    // 与 metadata.probeBatchAsync 的差异只有一处: 那边把活派给
    // fb2k::inCpuWorkerThread, 这边每个操作起一条自己的 std::thread。
    // 文件 IO 是长阻塞而非计算, 占着 CPU worker 池会把探测之类的活饿死;
    // 取的是 HttpApi 异步请求的形态 (detach 的 std::thread)。
    //==========================================================================

    // 同时进行的操作数上限。页面可以无限次调用这三个方法, 没有上限就是
    // "一次点击起一条线程"的资源耗尽面。取值与 HttpApi 的并发闸门同数量级
    // (HttpApi 的 MAX_CONCURRENT_REQUESTS 是 10)。
    constexpr size_t kMaxConcurrentFileOps = 8;

    // 回收站删除每批的条数上限。见 RunTrashItems 的说明。
    constexpr size_t kTrashBatchSize = 16;

    // 逐条 result 的 reason 取值全集, 与 schema/file.ts 声明的七个一一对应, 不得新增字面量。
    // 下面所有比较都是指针比较: reason 只允许从这六个常量赋值, 每个常量都是
    // 同一个对象, 所以比较的是"来自哪一个常量"而不是字符串内容。
    constexpr const char* kReasonAlreadyExists = "already-exists";
    constexpr const char* kReasonNotFound      = "not-found";
    constexpr const char* kReasonPermission    = "permission";
    constexpr const char* kReasonCrossVolume   = "cross-volume";
    constexpr const char* kReasonPathTooLong   = "path-too-long";
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
            // 新建超过 247 个字符的文件夹时系统报这个码；超过 259 个字符的文件路径报的是
            // ERROR_PATH_NOT_FOUND，那一种由各原语事先量长度拦下。
            case ERROR_FILENAME_EXCED_RANGE:
                return kReasonPathTooLong;
            default:
                return kReasonIoError;
        }
    }

    // 条目本身或展开出来的路径超过 MAX_PATH 时记 path-too-long。系统对这种路径报
    // ERROR_PATH_NOT_FOUND，fs::exists 也答不存在，不先量就会被归成 not-found。
    const char* PathLengthReason(const std::wstring& path) {
        return ExceedsMaxPath(path) ? kReasonPathTooLong : nullptr;
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

    // 载荷形状由 src/api/schema/file.ts 的 Events 声明。
    //
    // results 里带路径是允许的: 它是功能数据, 且经 CallerContext 单窗口投递。
    // 错误信封与 console 日志里仍然一个路径都不许出现。
    fileapi::FileOpResultItem FileOpResultItemOf(const FileOpItemResult& result) {
        fileapi::FileOpResultItem item;
        item.source = result.source;
        item.status = FileOpStatusName(result.status);
        if (!result.destination.empty()) {
            item.destination = result.destination;
        }
        if (result.reason) {
            item.reason = result.reason;
        }
        return item;
    }

    fileapi::OpProgressPayload BuildFileOpProgressPayload(const std::string& operationId, const char* op,
                                                          size_t done, size_t total,
                                                          std::vector<fileapi::FileOpResultItem> results) {
        fileapi::OpProgressPayload payload;
        payload.operationId = operationId;
        payload.op = op;
        payload.done = static_cast<std::int64_t>(done);
        payload.total = static_cast<std::int64_t>(total);
        payload.results = std::move(results);
        return payload;
    }

    fileapi::OpCompletePayload BuildFileOpCompletePayload(const std::string& operationId, const char* op,
                                                          size_t total, size_t successCount,
                                                          size_t skippedCount, size_t failureCount,
                                                          bool cancelled) {
        fileapi::OpCompletePayload payload;
        payload.operationId = operationId;
        payload.op = op;
        payload.total = static_cast<std::int64_t>(total);
        payload.successCount = static_cast<std::int64_t>(successCount);
        payload.skippedCount = static_cast<std::int64_t>(skippedCount);
        payload.failureCount = static_cast<std::int64_t>(failureCount);
        payload.cancelled = cancelled;
        return payload;
    }

    //==========================================================================
    // 取消令牌 / 注册表 / 退出闸门
    //==========================================================================

    // 注册表要求令牌类型提供 abort()。这里不用 abort_callback_impl:
    // 本组不走 SDK filesystem 入口, 没有任何要收 abort_callback& 的形参,
    // 换来的只是一个 SDK 依赖 (选型说明见 AsyncOperationRegistry.h 文件头)。
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
    void EmitFileOpProgress(const json& callerSeed, fileapi::OpProgressPayload payload) noexcept {
        if (FileOpShuttingDown()) {
            return;
        }
        try {
            fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
                try {
                    auto caller = CallerContext::FromParams(callerSeed);
                    api::emit::EmitTo<fileapi::events::OpProgress>(caller, payload);
                } catch (...) {
                    // best-effort: 抛进 main-thread callback runner 会 terminate
                }
            });
        } catch (...) {
        }
    }

    void EmitFileOpComplete(const json& callerSeed, fileapi::OpCompletePayload payload) noexcept {
        if (FileOpShuttingDown()) {
            return;
        }
        try {
            fb2k::inMainThread([callerSeed, payload = std::move(payload)]() noexcept {
                try {
                    auto caller = CallerContext::FromParams(callerSeed);
                    api::emit::EmitTo<fileapi::events::OpComplete>(caller, payload);
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
    // 建不出来时只报 path-too-long：其他失败留给随后的复制或移动报出真实原因，而文件夹
    // 超长时随后的调用只会报 ERROR_PATH_NOT_FOUND。
    const char* EnsureParentDirectory(const std::wstring& target) {
        std::error_code ec;
        const fs::path parent = fs::path(target).parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
        }
        return (ec && ReasonFromErrorCode(ec) == kReasonPathTooLong) ? kReasonPathTooLong : nullptr;
    }

    // 复制一个文件。返回该条的 reason, nullptr 表示成功。
    const char* CopyOneFile(const std::wstring& from, const std::wstring& to,
                            bool overwrite, FileOpAbortToken& token) {
        if (const char* reason = PathLengthReason(from)) return reason;
        if (const char* reason = PathLengthReason(to)) return reason;
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
                    if (ec && ReasonFromErrorCode(ec) == kReasonPathTooLong && !firstFailure) {
                        firstFailure = kReasonPathTooLong;
                    }
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
        if (const char* reason = EnsureParentDirectory(target)) {
            ApplyReason(out, reason);
            return out;
        }
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
    // 底层的 fs::rename 是无条件覆盖, 异步版的覆盖由 overwrite 显式控制。
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
        if (const char* reason = PathLengthReason(target)) {
            ApplyReason(out, reason);
            return out;
        }
        if (!overwrite && fs::exists(target, ec)) {
            ApplyReason(out, kReasonAlreadyExists);
            return out;
        }
        if (const char* reason = EnsureParentDirectory(target)) {
            ApplyReason(out, reason);
            return out;
        }
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
                if (token->is_aborting()) {
                    ApplyReason(out, kReasonCancelled);
                } else if (const char* tooLong = PathLengthReason(item.source)) {
                    ApplyReason(out, tooLong);
                } else {
                    ApplyReason(out, DeleteOneToRecycleBin(item.source));
                }
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
            pending_.push_back(FileOpResultItemOf(result));
            ++done_;
            const int64_t now = FileOpNowMillis();
            if (scheduler_.ShouldFlush(pending_.size(), now)) {
                Flush();
                scheduler_.MarkFlushed(now);
            }
        }

        // 残余批不得丢。fb2k::inMainThread 保证 FIFO (SDK 在其声明处写明),
        // 所以收尾时先 Flush 再发 complete, 页面看到的顺序就是这个顺序。
        void Flush() {
            if (pending_.empty()) {
                return;
            }
            EmitFileOpProgress(request_.callerSeed,
                               BuildFileOpProgressPayload(request_.operationId,
                                                          FileOpKindName(request_.kind), done_,
                                                          request_.items.size(), std::move(pending_)));
            pending_.clear();
        }

    private:
        const FileOpRequest& request_;
        fb2k_api::BatchEmitScheduler scheduler_;
        std::vector<fileapi::FileOpResultItem> pending_;
        size_t done_ = 0;
    };

    //==========================================================================
    // worker
    //==========================================================================

    FileOpItemResult RunOneItem(const FileOpRequest& request, const FileOpItem& item,
                                FileOpAbortToken& token) {
        const bool hasDestination = (request.kind != FileOpKind::Delete);
        const char* tooLong = PathLengthReason(item.source);
        if (!tooLong && hasDestination) tooLong = PathLengthReason(item.destination);
        if (tooLong) {
            FileOpItemResult out{item.sourceEcho, hasDestination ? item.destEcho : std::string()};
            ApplyReason(out, tooLong);
            return out;
        }
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
    // 线程函数就是 std::terminate, 范式同 HttpApi 异步请求线程的外层守卫。
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

    // 条目形状 (items 是非空数组、每条是对象、带非空字符串 source / destination、
    // 没有多余键; overwrite / moveToTrash 是布尔) 由生成的参数读取器在进入
    // handler 之前查完。逐条路径校验更早, 由桥接层按声明里的 @pathKey 跑:
    // source 那条在前、destination 那条在后, 各自完整走一遍 items
    // (ValidateNestedArrayParam), 缺哪个成员就由哪条判成形状错; 空串在那里
    // 就被判成 PERMISSION_DENIED。这里只做展开, 并把原值留作回显。
    std::vector<FileOpItem> CollectCopyMoveItems(const std::vector<fileapi::FileOpEntry>& entries) {
        std::vector<FileOpItem> out;
        out.reserve(entries.size());
        for (const auto& entry : entries) {
            FileOpItem item;
            item.sourceEcho = entry.source;
            item.destEcho = entry.destination;
            item.source = ExpandPathVariables(item.sourceEcho);
            item.destination = ExpandPathVariables(item.destEcho);
            out.push_back(std::move(item));
        }
        return out;
    }

    // paths 同理: 数组非空由读取器查, 元素是字符串与路径安全由桥接层的
    // ValidateArrayParam 查。
    std::vector<FileOpItem> CollectDeletePaths(const std::vector<std::string>& paths) {
        std::vector<FileOpItem> out;
        out.reserve(paths.size());
        for (const auto& entry : paths) {
            FileOpItem item;
            item.sourceEcho = entry;
            item.source = ExpandPathVariables(item.sourceEcho);
            out.push_back(std::move(item));
        }
        return out;
    }

    // 派工成功的收据。三个方法各有自己的 <Method>Result 类型 (声明里各自一个
    // 接口), 所以 DispatchFileOp 只交出这份中间值, 由 ToReceipt 转成各自的结果。
    struct DispatchOutcome {
        std::string operationId;
        size_t totalCount = 0;
    };

    using DispatchResult = std::variant<DispatchOutcome, api::Failure>;

    // 三个方法共同的尾段: 起 id、注册、派线程、回收据。
    DispatchResult DispatchFileOp(FileOpKind kind, const CallerContext& caller,
                                  std::vector<FileOpItem> items, bool overwrite,
                                  bool moveToTrash) {
        // 页面可以无限次调这三个方法, 没有闸门就是"一次点击起一条线程"。
        if (GetFileOpRegistry().Size() >= kMaxConcurrentFileOps) {
            return api::Fail("too many concurrent file operations", ApiErrorCode::OPERATION_FAILED);
        }
        try {
            FileOpRequest request;
            request.kind = kind;
            request.items = std::move(items);
            request.overwrite = overwrite;
            request.moveToTrash = moveToTrash;
            // 事件路由上下文在主线程取, 但只把 hwnd 的值带过去: CallerContext 持的
            // 是裸 BridgeCore*, 面板销毁后跨线程持有会悬垂, 所以不把 caller 存进
            // 线程, 发射前在主线程用这份 seed 重新 FromParams (同 MetadataApi.cpp
            // 的 MetadataProbeBatchAsync 取 callerSeed 的做法)。caller 由注册包装层
            // 用原始参数 FromParams 得到; 那时 _callerHwnd 缺失、为 0 或已不是窗口
            // 都不设 callerHwnd, 这里也就不写 seed, 发射时同样回落到单例桥。
            if (caller.callerHwnd) {
                request.callerSeed["_callerHwnd"] = reinterpret_cast<intptr_t>(caller.callerHwnd);
            }

            const std::string operationId = NextFileOpOperationId();
            const size_t totalCount = request.items.size();
            request.operationId = operationId;

            auto token = std::make_shared<FileOpAbortToken>();
            // 窗口维度也在主线程解析: popup 关闭时按 windowId 一次取消它发起
            // 的全部未完成操作, 否则 worker 会把整队跑完, 事件发往已销毁的窗口。
            const std::string windowId = caller.windowId;
            if (!GetFileOpRegistry().Register(operationId, token, windowId)) {
                // id 撞了就不派工: 拿不到取消能力的异步操作不该存在。
                return api::Fail("failed to register file operation", ApiErrorCode::OPERATION_FAILED);
            }
            FileOpRegistrationGuard registration(operationId);

            std::thread(RunFileOpWorker, std::move(request), token).detach();
            registration.Dismiss();

            return DispatchOutcome{operationId, totalCount};
        } catch (const std::exception&) {
            return api::Fail("failed to start file operation", ApiErrorCode::OPERATION_FAILED);
        }
    }

    // 把派工结果转成某个方法自己的结果类型 (CopyAsyncResult / MoveAsyncResult /
    // DeleteAsyncResult, 三者字段相同)。
    template <class R>
    api::Result<R> ToReceipt(DispatchResult dispatched) {
        if (auto* failure = std::get_if<api::Failure>(&dispatched)) {
            return std::move(*failure);
        }
        const DispatchOutcome& outcome = std::get<DispatchOutcome>(dispatched);
        R result;
        result.operationId = outcome.operationId;
        result.totalCount = static_cast<std::int64_t>(outcome.totalCount);
        return result;
    }

    //==========================================================================
    // file.copyAsync - 异步批量复制, 可取消
    // params: { items: [{source, destination}], overwrite?: false }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    api::Result<fileapi::CopyAsyncResult> FileCopyAsync(const fileapi::CopyAsyncParams& p,
                                                        const CallerContext& caller) {
        return ToReceipt<fileapi::CopyAsyncResult>(DispatchFileOp(
            FileOpKind::Copy, caller, CollectCopyMoveItems(p.items), p.overwrite, true));
    }

    //==========================================================================
    // file.moveAsync - 异步批量移动, 可取消, 跨卷自动回退成复制加删源
    // params: { items: [{source, destination}], overwrite?: false }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    api::Result<fileapi::MoveAsyncResult> FileMoveAsync(const fileapi::MoveAsyncParams& p,
                                                        const CallerContext& caller) {
        return ToReceipt<fileapi::MoveAsyncResult>(DispatchFileOp(
            FileOpKind::Move, caller, CollectCopyMoveItems(p.items), p.overwrite, true));
    }

    //==========================================================================
    // file.deleteAsync - 异步批量删除, 可取消
    // params: { paths: string[], moveToTrash?: true }
    // Returns: { success: true, operationId, totalCount }
    // Events: file:opProgress / file:opComplete
    //==========================================================================
    api::Result<fileapi::DeleteAsyncResult> FileDeleteAsync(const fileapi::DeleteAsyncParams& p,
                                                            const CallerContext& caller) {
        return ToReceipt<fileapi::DeleteAsyncResult>(DispatchFileOp(
            FileOpKind::Delete, caller, CollectDeletePaths(p.paths), false, p.moveToTrash));
    }

    //==========================================================================
    // file.cancelOp - 取消一个进行中的异步文件操作
    // params: { operationId: string }
    // Returns: { success: true, cancelled: boolean }
    //==========================================================================
    api::Result<fileapi::CancelOpResult> FileCancelOp(const fileapi::CancelOpParams& p) {
        // cancelled=false 表示该 operationId 已结束或不存在。两者对调用方故意
        // 不可区分: 一个页面无法分辨自己是差了一微秒还是差了一分钟。
        fileapi::CancelOpResult result;
        result.cancelled = GetFileOpRegistry().Cancel(p.operationId);
        return result;
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

    // items 上由 @pathKey 生成的一条路径规格是否为 items[].<member>、按 access 校验。
    // RegisterFileApi 用它把校验顺序与档位钉在编译期。
    constexpr bool IsItemsPathParam(const api::params::PathParam& param, std::string_view member,
                                    api::params::PathAccess access) {
        return std::string_view(param.key) == "items" && param.isArray && !param.skipInvalid &&
               param.nestedKey != nullptr && std::string_view(param.nestedKey) == member &&
               param.access == access;
    }

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
    // 路径参数的安全级别来自声明 (生成头里的 kPathParams), 不再在这里登记。
    
    // file.read - Read file content
    api::RegisterApi("file.read", FileRead);
    
    // file.write - Write content to file
    //
    // 以下六个写端点在声明里用 FileWrite 而非 MediaWrite: 它们操作的是任意文件,
    // 不是媒体上下文中的文件, "非系统盘直通"这一通用文件写策略不应混进媒体写入语义。
    // 目标态是归入 Write (profile/temp) 严格写白名单; 那会拒绝非系统盘任意路径的写入,
    // 属破坏性变更, 须单独决策。
    api::RegisterApi("file.write", FileWrite);
    
    // file.exists - Check if file/directory exists
    api::RegisterApi("file.exists", FileExists);
    
    // file.list - List directory contents
    api::RegisterApi("file.list", FileList);
    
    // file.delete - Delete a file
    api::RegisterApi("file.delete", FileDelete);
    
    // file.mkdir - Create directory
    api::RegisterApi("file.mkdir", FileMkdir);

    // file.copy - Copy file or directory (source Read, destination FileWrite)
    api::RegisterApi("file.copy", FileCopy);

    // file.move - Move file or directory (both ends FileWrite)
    api::RegisterApi("file.move", FileMove);

    // file.rename - Rename a file or directory
    api::RegisterApi("file.rename", FileRename);

    // file.getInfo - Get file information
    api::RegisterApi("file.getInfo", FileGetInfo);

    // file.copyAsync - 异步批量复制 (worker 线程, 可取消)
    //
    // 声明在 items 上写 @pathKey source=Read destination=FileWrite, 同一个
    // paramKey 生成两条规格: 包装层的循环对每条各调一次 ValidatePathParam, 每次
    // 完整走一遍 items 数组并只看自己那个成员, 两条互不干扰。档位与同步版
    // file.copy 一致 —— source 读、destination 写。顺序也要紧: 缺成员与空串都
    // 报第一条失败的规格, e2e FO-15 按「source 先报」断言。
    static_assert(fileapi::CopyAsyncParams::kPathParams.size() == 2 &&
                      IsItemsPathParam(fileapi::CopyAsyncParams::kPathParams[0], "source",
                                       api::params::PathAccess::Read) &&
                      IsItemsPathParam(fileapi::CopyAsyncParams::kPathParams[1], "destination",
                                       api::params::PathAccess::FileWrite),
                  "file.copyAsync checks items[].source as Read, then items[].destination as FileWrite");
    api::RegisterApi("file.copyAsync", FileCopyAsync);

    // file.moveAsync - 异步批量移动 (跨卷自动回退成复制加删源, 可取消)
    // 两端都是写: 移动会删掉 source。与同步版 file.move 同档。source 这条若被
    // 写丢, 被删的一端就不再过 FileWrite 校验, 而没有一条 e2e 能发现, 所以同样
    // 在编译期钉住。
    static_assert(fileapi::MoveAsyncParams::kPathParams.size() == 2 &&
                      IsItemsPathParam(fileapi::MoveAsyncParams::kPathParams[0], "source",
                                       api::params::PathAccess::FileWrite) &&
                      IsItemsPathParam(fileapi::MoveAsyncParams::kPathParams[1], "destination",
                                       api::params::PathAccess::FileWrite),
                  "file.moveAsync checks items[].source, then items[].destination, both as FileWrite");
    api::RegisterApi("file.moveAsync", FileMoveAsync);

    // file.deleteAsync - 异步批量删除 (可取消)
    api::RegisterApi("file.deleteAsync", FileDeleteAsync);

    // file.cancelOp - 取消一个进行中的异步文件操作 (无路径参数)
    api::RegisterApi("file.cancelOp", FileCancelOp);

    LOG("File API registered (14 APIs)");
}
