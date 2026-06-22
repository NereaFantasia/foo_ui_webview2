#pragma once

// ============================================
// 版本号定义 — Single Source of Truth
// ============================================
//
// 本文件由 bump-version.ps1 自动生成/更新
// 手动修改前请确保同步更新 VERSION 文件
//
// 版本号管理规则:
//   1. 唯一真相源: 根目录 VERSION 文件
//   2. bump-version.ps1 负责同步更新:
//      VERSION -> src/version.h -> sdk/package.json
//              -> sdk/package-lock.json -> vitepress config.ts
//   3. 禁止在其他文件中硬编码版本号
//

#define PLUGIN_VERSION_MAJOR  1
#define PLUGIN_VERSION_MINOR  9
#define PLUGIN_VERSION_PATCH  0

#define PLUGIN_VERSION_STR    "1.9.0"
#define PLUGIN_VERSION_WSTR   L"1.9.0"

// SDK 版本与插件版本保持统一
#define SDK_VERSION_STR       PLUGIN_VERSION_STR