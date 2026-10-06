#pragma once
#include <string>

// UTF-16 与 UTF-8 互转（Win32 WideCharToMultiByte / MultiByteToWideChar）。空串或转换失败返回空串。
std::string WideToUtf8(const std::wstring& wide);
std::wstring Utf8ToWide(const std::string& utf8);
