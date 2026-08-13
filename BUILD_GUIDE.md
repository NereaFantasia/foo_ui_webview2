# foo_ui_webview2 构建指南

## 快速开始

### 使用构建脚本（推荐）
```powershell
# Release x64 构建
.\build.ps1

# Debug x64 构建
.\build.ps1 -Config Debug

# 完全重建
.\build.ps1 -Rebuild

# Win32 构建
.\build.ps1 -Platform Win32
```

## 构建要求

### 必需软件
- Visual Studio 2022 (推荐) 或 2019
- Windows SDK 10.0 或更高
- PowerShell 5.1+ (Windows 自带)

### 依赖项
项目使用 NuGet 管理依赖，`build.ps1` 构建前会自动检查并还原缺失的包（优先 `nuget restore`，无 nuget.exe 时回退 msbuild restore + nuget.org 直接下载）：
- Microsoft.Web.WebView2 (1.0.3719.77)
- Microsoft.Windows.ImplementationLibrary (1.0.240803.1)
- Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn (1.8.1.7，单元测试用)

另需第三方 **Columns UI SDK**（`lib/columns_ui-sdk`，不入库）。`build.ps1` 检测到缺失时会自动克隆并修正 include 路径；若网络受限需手动获取：

```powershell
git clone --depth=1 https://github.com/reupen/columns_ui-sdk.git lib/columns_ui-sdk
(Get-Content lib/columns_ui-sdk/ui_extension.h) `
    -replace '\.\./pfc/', '../foobar2000_sdk/pfc/' `
    -replace '\.\./foobar2000/', '../foobar2000_sdk/foobar2000/' |
    Set-Content lib/columns_ui-sdk/ui_extension.h
```

## 预编译头配置

**?? 重要：不要修改预编译头配置！**

当前配置：
- 头文件：`src/pch.h`
- 源文件：`src/pch.cpp`
- 所有 .cpp 文件使用 `Use` 模式
- 只有 pch.cpp 使用 `Create` 模式

## 构建配置

### 支持的配置
- Debug | Win32
- Debug | x64
- Release | Win32
- Release | x64

### 输出路径
```
bin/Debug/foo_ui_webview2.dll          - Debug Win32
bin/Debug_x64/foo_ui_webview2.dll      - Debug x64
bin/Release/foo_ui_webview2.dll        - Release Win32
bin/Release_x64/foo_ui_webview2.dll    - Release x64
```

## 故障排查

### 构建失败
1. 清理中间文件：
```powershell
Remove-Item -Recurse -Force .\obj\
.\build.ps1
```

2. 检查构建日志：
```powershell
Get-Content build_log.txt -Tail 50
```

### 编码警告 C4819
非致命警告，可以安全忽略。源文件包含中文注释使用GBK编码。

如需消除，将源文件转换为UTF-8 with BOM。

### MSBuild 未找到
不要直接调用 `msbuild` 命令，使用 `build.ps1` 脚本会自动查找正确路径。

## 手动构建（高级）

如需手动构建，使用以下步骤：

1. 查找MSBuild：
```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -property installationPath
$msbuild = "$vsPath\MSBuild\Current\Bin\MSBuild.exe"
```

2. 构建：
```powershell
& $msbuild foo_ui_webview2.sln /p:Configuration=Release /p:Platform=x64 /m
```

## 部署

1. 构建 Release x64 配置
2. 输出文件：`bin/Release_x64/foo_ui_webview2.dll`
3. 复制到 foobar2000 的 `components` 文件夹
4. 重启 foobar2000

## 参考文档

- [README.md](README.md) - 项目总览与快速上手
- [docs/vitepress/guide/installation.md](docs/vitepress/guide/installation.md) - 安装与部署
