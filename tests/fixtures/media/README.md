# 媒体容器测试样本

样本于 2026-10-01 使用本机 FFmpeg 8.1.2 合成：0.4 秒的 32×24 蓝色视频、25 fps、48 kHz 双声道静音 AAC，以及一条人工字幕。没有用户素材。

- `av-subtitle.mp4`：H.264、AAC、mov_text，音轨语言为 jpn。
- `av-subtitle.mkv`：从 MP4 复制音视频，字幕转 SubRip，并附加 `caption.srt`，附件名称为 fixture.txt、MIME 为 text/plain。

- `chapters.m4a`、`chapters.mka`：1.5 秒的 48 kHz 单声道静音 AAC，按 `chapters.ffmeta` 写入三章（0–0.5、0.5–1、1–1.5 秒，标题 Alpha、Beta、第三章）。FFmpeg 给 MP4 同时写 QuickTime 章节轨与 Nero `chpl`；MKA 从 M4A 复制音频与章节。

用 ffprobe 同版本核对：两文件均有三条媒体轨，MKV 另有一份附件；MP4 时长 0.400 秒，MKV 为 0.421 秒（保留 AAC 时间信息）。`test_media_fixtures.cpp` 固定这些预期值。产品与容器单元测试运行时均不调用 FFmpeg。

`mcp/tests/e2e-media-read.mjs` 另用 FFmpeg 在临时目录生成大于 2 MiB 的可播放 MP4，验证浏览器分段与 seek。该实机套件需要包含 libx264 的 FFmpeg，可用环境变量 `FB2K_E2E_FFMPEG` 指定可执行文件路径；未设置时从 PATH 查找。生成文件随本次测试清理，不接触用户媒体。

生成命令：

```powershell
ffmpeg -nostdin -y -f lavfi -i 'color=c=blue:s=32x24:r=25:d=0.4' -f lavfi -i 'anullsrc=r=48000:cl=stereo' -i caption.srt -map 0:v -map 1:a -map 2:s -t 0.4 -c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 32k -c:s mov_text -metadata:s:a:0 language=jpn -movflags +faststart av-subtitle.mp4
ffmpeg -nostdin -y -i av-subtitle.mp4 -map 0 -c:v copy -c:a copy -c:s srt -attach caption.srt -metadata:s:t mimetype=text/plain -metadata:s:t filename=fixture.txt av-subtitle.mkv
ffprobe -v error -show_streams -show_format -of json av-subtitle.mp4
ffprobe -v error -show_streams -show_format -of json av-subtitle.mkv
ffmpeg -nostdin -y -f lavfi -i 'anullsrc=r=48000:cl=mono' -f ffmetadata -i chapters.ffmeta -map 0:a -map_metadata 1 -map_chapters 1 -t 1.5 -c:a aac -b:a 32k chapters.m4a
ffmpeg -nostdin -y -i chapters.m4a -map 0:a -map_chapters 0 -c:a copy -write_crc32 0 chapters.mka
ffprobe -v error -show_chapters -show_streams -of compact chapters.m4a
ffprobe -v error -show_chapters -show_streams -of compact chapters.mka
```

两份章节样本的 ffprobe 结果：三章起止与标题如上，M4A 另有一条 data 类型的章节文本轨；`test_media_fixtures.cpp` 固定章节起止、标题与轨道类型。
