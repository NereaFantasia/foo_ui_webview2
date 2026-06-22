// FileApi.cpp - File System API
// Provides safe file read/write operations within allowed directories

#include "pch.h"
#include "api/FileApi.h"
#include "api/BridgeCore.h"
#include "utils/PathExpansion.h"
// PathSecurity validation is now handled by BridgeCore decorator
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ShlObj.h>

namespace fs = std::filesystem;

namespace {
    using json = nlohmann::json;
    
    //==========================================================================
    // Path helpers — 委托给共享 PathExpansion 模块
    //==========================================================================

    std::wstring GetProfileDirectory()   { return PathExpansion::GetProfileDirectory(); }
    std::wstring GetComponentDirectory() { return PathExpansion::GetComponentDirectory(); }
    std::wstring GetMusicDirectory()     { return PathExpansion::GetMusicDirectory(); }
    std::wstring GetAppDataDirectory()   { return PathExpansion::GetAppDataDirectory(); }
    std::wstring GetTempDirectory()      { return PathExpansion::GetTempDirectory(); }

    std::wstring ExpandPathVariables(const std::string& pathUtf8) {
        return PathExpansion::Expand(pathUtf8);
    }
    
    // Check if path is within allowed directories
    bool IsPathAllowed(const std::wstring& path) {
        try {
            fs::path normalizedPath = fs::weakly_canonical(path);
            std::wstring normalizedStr = normalizedPath.wstring();
            
            // Convert to lowercase for comparison
            std::wstring lowerPath = normalizedStr;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
            
            // Allowed directories
            std::wstring profileDir = GetProfileDirectory();
            std::wstring componentDir = GetComponentDirectory();
            std::wstring musicDir = GetMusicDirectory();
            std::wstring appDataDir = GetAppDataDirectory();
            std::wstring tempDir = GetTempDirectory();
            
            // Normalize and lowercase allowed directories
            auto normalizeDir = [](const std::wstring& dir) -> std::wstring {
                if (dir.empty()) return L"";
                std::wstring result = fs::weakly_canonical(dir).wstring();
                std::transform(result.begin(), result.end(), result.begin(), ::towlower);
                return result;
            };
            
            std::wstring lowerProfile = normalizeDir(profileDir);
            std::wstring lowerComponent = normalizeDir(componentDir);
            std::wstring lowerMusic = normalizeDir(musicDir);
            std::wstring lowerAppData = normalizeDir(appDataDir);
            std::wstring lowerTemp = normalizeDir(tempDir);
            
            // Check if path starts with any allowed directory
            if (!lowerProfile.empty() && lowerPath.starts_with(lowerProfile)) return true;
            if (!lowerComponent.empty() && lowerPath.starts_with(lowerComponent)) return true;
            if (!lowerMusic.empty() && lowerPath.starts_with(lowerMusic)) return true;
            if (!lowerAppData.empty() && lowerPath.starts_with(lowerAppData)) return true;
            if (!lowerTemp.empty() && lowerPath.starts_with(lowerTemp)) return true;
            
            // Default deny: only explicitly allowed directories above return true
            return false;
        } catch (...) {
            return false;
        }
    }
    
    //==========================================================================
    // file.read - Read file content
    // 安全: 使用统一 PathSecurity 验证
    //==========================================================================
    json FileRead(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string encoding = params.value("encoding", "utf-8");
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
            if (!fs::exists(path)) {
                return {{"success", false}, {"error", "File not found"}};
            }
            
            if (!fs::is_regular_file(path)) {
                return {{"success", false}, {"error", "Path is not a file"}};
            }
            
            // Read file
            std::ifstream file;
            if (encoding == "binary") {
                file.open(path, std::ios::binary);
            } else {
                file.open(path, std::ios::in);
            }
            
            if (!file.is_open()) {
                return {{"success", false}, {"error", "Failed to open file"}};
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
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
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
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
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
                return {{"success", false}, {"error", "Failed to open file for writing"}};
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
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    //==========================================================================
    // file.exists - Check if file or directory exists
    //==========================================================================
    json FileExists(const json& params) {
        std::string pathStr = params.value("path", "");
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
            bool exists = fs::exists(path);
            bool isFile = exists && fs::is_regular_file(path);
            bool isDirectory = exists && fs::is_directory(path);
            
            return {
                {"exists", exists},
                {"isFile", isFile},
                {"isDirectory", isDirectory}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
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
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
            if (!fs::exists(path)) {
                return {{"success", false}, {"error", "Directory not found"}};
            }
            
            if (!fs::is_directory(path)) {
                return {{"success", false}, {"error", "Path is not a directory"}};
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
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    //==========================================================================
    // file.delete - Delete a file
    //==========================================================================
    json FileDelete(const json& params) {
        std::string pathStr = params.value("path", "");
        bool moveToTrash = params.value("moveToTrash", true);
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
            if (!fs::exists(path)) {
                return {{"success", false}, {"error", "File not found"}};
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
                    return {{"success", false}, {"error", "Failed to move to recycle bin"}};
                }
            } else {
                fs::remove(path);
            }
            
            return {{"success", true}};
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    //==========================================================================
    // file.mkdir - Create directory
    //==========================================================================
    json FileMkdir(const json& params) {
        std::string pathStr = params.value("path", "");
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = ExpandPathVariables(pathStr);
        
        try {
            if (fs::exists(path)) {
                if (fs::is_directory(path)) {
                    return {{"success", true}, {"created", false}, {"message", "Directory already exists"}};
                } else {
                    return {{"success", false}, {"error", "Path exists but is not a directory"}};
                }
            }
            
            bool created = fs::create_directories(path);
            
            return {
                {"success", true},
                {"created", created}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
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
            return {{"success", false}, {"error", "source is required"}};
        }
        if (destStr.empty()) {
            return {{"success", false}, {"error", "destination is required"}};
        }

        std::wstring srcPath = ExpandPathVariables(srcStr);
        std::wstring destPath = ExpandPathVariables(destStr);

        try {
            if (!fs::exists(srcPath)) {
                return {{"success", false}, {"error", "Source does not exist"}};
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
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // file.move - Move file or directory
    //==========================================================================
    json FileMove(const json& params) {
        std::string srcStr = params.value("source", "");
        std::string destStr = params.value("destination", "");

        if (srcStr.empty()) {
            return {{"success", false}, {"error", "source is required"}};
        }
        if (destStr.empty()) {
            return {{"success", false}, {"error", "destination is required"}};
        }

        std::wstring srcPath = ExpandPathVariables(srcStr);
        std::wstring destPath = ExpandPathVariables(destStr);

        try {
            if (!fs::exists(srcPath)) {
                return {{"success", false}, {"error", "Source does not exist"}};
            }

            fs::rename(srcPath, destPath);

            return {
                {"success", true},
                {"source", srcStr},
                {"destination", destStr}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // file.rename - Rename a file or directory
    //==========================================================================
    json FileRename(const json& params) {
        std::string pathStr = params.value("path", "");
        std::string newName = params.value("newName", "");

        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        if (newName.empty()) {
            return {{"success", false}, {"error", "newName is required"}};
        }

        // Validate newName doesn't contain path separators
        if (newName.find('/') != std::string::npos || newName.find('\\') != std::string::npos) {
            return {{"success", false}, {"error", "newName cannot contain path separators"}};
        }

        std::wstring srcPath = ExpandPathVariables(pathStr);

        try {
            if (!fs::exists(srcPath)) {
                return {{"success", false}, {"error", "Path does not exist"}};
            }

            fs::path src(srcPath);
            fs::path dest = src.parent_path() / Utf8ToWide(newName);

            if (fs::exists(dest)) {
                return {{"success", false}, {"error", "A file with the new name already exists"}};
            }

            fs::rename(src, dest);

            return {
                {"success", true},
                {"oldPath", pathStr},
                {"newPath", WideToUtf8(dest.wstring())}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // file.getInfo - Get file information
    //==========================================================================
    json FileGetInfo(const json& params) {
        std::string pathStr = params.value("path", "");

        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }

        std::wstring path = ExpandPathVariables(pathStr);

        try {
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
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
} // anonymous namespace

//==========================================================================
// Register File API
//==========================================================================
void RegisterFileApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // file.read - Read file content
    bridge.RegisterApi("file.read", FileRead, {{"path", SecurityLevel::Read}});
    
    // file.write - Write content to file
    bridge.RegisterApi("file.write", FileWrite, {{"path", SecurityLevel::MediaWrite}});
    
    // file.exists - Check if file/directory exists
    bridge.RegisterApi("file.exists", FileExists, {{"path", SecurityLevel::Read}});
    
    // file.list - List directory contents
    bridge.RegisterApi("file.list", FileList, {{"path", SecurityLevel::Read}});
    
    // file.delete - Delete a file
    bridge.RegisterApi("file.delete", FileDelete, {{"path", SecurityLevel::MediaWrite}});
    
    // file.mkdir - Create directory
    bridge.RegisterApi("file.mkdir", FileMkdir, {{"path", SecurityLevel::MediaWrite}});

    // file.copy - Copy file or directory
    bridge.RegisterApi("file.copy", FileCopy, {
        {"source", SecurityLevel::Read},
        {"destination", SecurityLevel::MediaWrite}
    });

    // file.move - Move file or directory
    bridge.RegisterApi("file.move", FileMove, {
        {"source", SecurityLevel::MediaWrite},
        {"destination", SecurityLevel::MediaWrite}
    });

    // file.rename - Rename a file or directory
    bridge.RegisterApi("file.rename", FileRename, {{"path", SecurityLevel::MediaWrite}});

    // file.getInfo - Get file information
    bridge.RegisterApi("file.getInfo", FileGetInfo, {{"path", SecurityLevel::Read}});
    
    LOG("File API registered (10 APIs)");
}
