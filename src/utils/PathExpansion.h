#pragma once
// PathExpansion.h — 共享路径变量展开工具
// 统一 BridgeCore(框架层) / FileApi / HttpApi 的路径展开逻辑

#include <string>

namespace PathExpansion {

// 目录获取（带尾 '\\'，结果缓存于 static 局部变量，线程安全由 C++11 magic static 保证）
std::wstring GetProfileDirectory();
std::wstring GetComponentDirectory();
std::wstring GetMusicDirectory();
std::wstring GetAppDataDirectory();
std::wstring GetTempDirectory();

// 路径变量展开 — 支持 %profile%, %component%, %music%, %APPDATA%, %TEMP%
// wide 版本（框架层使用）
std::wstring Expand(const std::wstring& path);
// UTF-8 便捷版本（API handler 使用）
std::wstring Expand(const std::string& pathUtf8);

} // namespace PathExpansion
