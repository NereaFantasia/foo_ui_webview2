# live-e2e 实机测试

在运行中的 foobar2000 实例上运行 `mcp/tests/` 中的端到端套件：部署新构建的 DLL，运行套件，结束后恢复实例状态。handler 的失败码、副作用以及事件送达的窗口，静态检查无法确认，须通过实机测试验证。

## 准备

- 构建组件：`.\build.ps1 -Config Release -Platform x64`。默认部署 `bin/Release_x64/foo_ui_webview2.dll`，可通过 `--dll` 指定其他文件。
- 指定实例：将 `FB2K_LIVE_DIR` 设为 `foobar2000.exe` 所在的文件夹，或在每次运行时传入 `--instance`。组件目录默认按便携版布局取 `profile/user-components-x64/foo_ui_webview2`；安装版须通过 `FB2K_LIVE_COMPONENT_DIR` 指定。
- 实例须以 `--remote-debugging-port` 启用 CDP，端口取自 `FB2K_CDP_PORT`，默认为 9222。
- 在 `mcp/` 中执行 `npm ci`，套件依赖其中的 `chrome-remote-interface`。
- 测试过程中实例会被重启，请勿使用日常播放所用的实例。

## 用法

```powershell
$env:FB2K_LIVE_DIR = 'D:\foobar2000-test'
node scripts/live-e2e/live.mjs run
node scripts/live-e2e/live.mjs run e2e-api-surface e2e-playlist-ops
node scripts/live-e2e/live.mjs run --no-deploy e2e-bridge-infra
node scripts/live-e2e/live.mjs deploy
node scripts/live-e2e/live.mjs show
```

- `run` 部署 DLL 后运行指定的套件；不指定套件时运行全部套件。
- `run --no-deploy` 不部署 DLL，直接运行套件。
- `deploy` 只部署 DLL。
- `show` 显示当前的播放状态。

`run` 依次执行以下步骤：

1. 保存播放状态。
2. 停止播放，并将音量调低 6 dB（即用户音量的一半）。
3. 部署 DLL。
4. 运行套件。
5. 恢复播放状态。套件失败时同样恢复。

退出码沿用 `mcp/tests/run-e2e.mjs`：全部套件通过时为 0；有套件失败或受阻（实例不可用）时为 1。套件运行前的步骤失败时同样为 1。每次运行的状态快照与各套件的 JSON 结果保存在临时目录 `fb2k-live-*` 中，运行时会输出其路径。

## 部署过程

1. 通过 `misc.exit` 请求实例退出。30 秒后仍未退出时，仅结束从该文件夹启动的 `foobar2000.exe`，不影响其他实例，并删除残留的 `profile/running`，否则下次启动会显示崩溃对话框，页面无法加载。
2. 将原 DLL 备份为 `foo_ui_webview2.dll.bak-<时间>`，复制新 DLL 并核对 MD5。
3. 通过 `explorer.exe` 启动实例，使其不属于当前 shell 的进程树，并等待桥接层响应，最长两分钟。

## 注意事项

- 恢复时只重新播放原来的曲目。播放列表中原位置的曲目已变化时不重新播放，只恢复音量、播放顺序与活动播放列表。套件新建或删除的播放列表会列在输出中，不会自动清理。
- 同一时间只运行一组实机测试。音频相关的套件对时序敏感，请勿同时运行占用大量 CPU 的任务。
