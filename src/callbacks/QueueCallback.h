#pragma once
#include "pch.h"

// ============================================
// 播放队列回调
// 当播放队列变化时广播 playback:queueChanged 事件
// ============================================

// 回调通过 service_factory_single_t 自动注册
// 无需手动初始化

// 批量操作期间的广播抑制。
//
// 背景：queue_flush() + N 次 queue_add_item* 会让内核连续发出 N+1 次
// on_changed，若逐次广播会在前端订阅方触发回查风暴（实测 256 项重排=257
// 次广播，紧随其后的下一次 invoke 排队 23.5 秒）。本类用 RAII 把一段连续
// 写入期间的多次广播合并为析构时的 1 次。
//
// 落点分两类：
//   (a) 单一重建单元 RebuildQueue 内部自带：覆盖 setContents / insertNext
//       重建分支 / playNow(index>0) / moveToTop 四个入口。析构广播的
//       origin 一律传 "unknown"（重建语义是移动/重排，不得透传抑制期内
//       核给出的 user_added，那对这四个入口都是误导性描述）。
//   (b) 不走重建单元的独立写入段各自安放：insertNext 追加快路径段、
//       queue.add 的 tracks 循环、queue.addPaths 的入队循环——三处各由
//       handler 在循环外包一层，析构广播透传抑制期内最后一次 origin
//       （均为 user_added，语义正确）。
//
// 嵌套禁令：调用 RebuildQueue 的 handler 不得在外层再包一层——本类构造
// 时断言标志未置位，用于在开发期捕获误用（简单 bool 标志下，内层析构清
// 位广播后，外层作用域内的后续变更会再次广播，产生额外事件）。
//
// 误吞风险：抑制窗口内若有非本组件来源的队列变更（fb2k 原生 UI、其他组
// 件、menu.runContextCommand 触发的上下文命令）恰好在同一次主线程调用
// 内发生 on_changed，会被一并计入"抑制期最后一次 origin"、其自身的那次
// 广播被吞掉，只剩本类析构时那一次广播。抑制窗口是同一次主线程调用内的
// 连续区间，实际触发面很窄，但不得视为不存在。
//
// 线程约束：playlist_manager 所有方法仅主线程有效，抑制标志与"抑制期最
// 后一次 origin"记录用简单静态变量即可，无需加锁。
class QueueBroadcastSuppressor {
public:
    // overrideOrigin 非空时，析构广播固定使用该值（落点 (a)）；为 nullptr
    // 时透传抑制期内记录到的最后一次 origin（落点 (b)）。
    explicit QueueBroadcastSuppressor(const char* overrideOrigin = nullptr);
    ~QueueBroadcastSuppressor();

    QueueBroadcastSuppressor(const QueueBroadcastSuppressor&) = delete;
    QueueBroadcastSuppressor& operator=(const QueueBroadcastSuppressor&) = delete;

private:
    bool hasOverrideOrigin_;
    std::string overrideOrigin_;
};

// 全库唯一的 BroadcastEvent("playback:queueChanged") 站点（单一
// emit helper）。payload 固定为 {origin, count}，count 现场读
// playlist_manager::get()->queue_get_count()，供订阅方免于逐次回查
// queue.get。
void EmitQueueChanged(const std::string& origin);
