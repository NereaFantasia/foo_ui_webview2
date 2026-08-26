// test_bridge_deferred.cpp — 延迟响应管线（RegisterApiDeferred / DeferredResponder）契约测试
//
// 判据来源：LIBRARY_QUERY_PERF_SPEC §4-B6（每请求恰好一次响应、错误形状保持）、
// §6-T2（崩溃重建竞态 + handler 异常用例）、§7-R4（IsLiveHost 守卫）、
// §7-R7（漏响应 = 页面侧 30s 超时假死）。
//
// 施测层是 BridgeDispatchSimulator：真身三条通道走 fb2k::inMainThread2（主线程调用即
// 同步执行、worker 入队），harness 无主线程队列故一律就地判定；闸门本体（atomic CAS）
// 与三关次序（通知型丢弃 → 恰好一次 → target 存活）与真身逐条一致，不变量本身是真的
// 在测。
#include "pch.h"
#include "harness/BridgeDispatchSimulator.h"

#include <algorithm>
#include <atomic>
#include <optional>
#include <thread>

using json = nlohmann::json;
using Responder = BridgeDispatchSimulator::DeferredResponder;

namespace {
constexpr int kRaceThreads = 8;
}  // namespace

class BridgeDeferredTest : public ::testing::Test {
protected:
    BridgeDispatchSimulator bridge;

    static std::string MakeMessage(const json& id, const std::string& method,
                                   const json& params = json::object()) {
        json msg;
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
        return msg.dump();
    }

    // 通知型调用：无 id 字段（现状同步路径对其不回包）
    static std::string MakeNotification(const std::string& method,
                                        const json& params = json::object()) {
        json msg;
        msg["method"] = method;
        msg["params"] = params;
        return msg.dump();
    }
};

// ============================================
// 信封形状：与同步路径等价
// ============================================

TEST_F(BridgeDeferredTest, SendJson_EnvelopeMatchesSyncPath) {
    json track;
    track["title"] = "曲目";
    const json payload = {{"success", true}, {"tracks", json::array({track})}, {"total", 1}};

    bridge.RegisterApi("test.sync", [payload](const json&) -> json { return payload; });
    bridge.RegisterApiDeferred("test.deferred", [payload](const json&, Responder r) {
        r.SendJson(payload);
    });

    bridge.HandleMessage(MakeMessage(7, "test.sync"));
    bridge.HandleMessage(MakeMessage(7, "test.deferred"));

    ASSERT_EQ(bridge.GetMessageCount(), 2u);
    // 同 id 同结果 → 两条通道的信封必须逐字段相等（type/id/result，无夹带字段）
    EXPECT_EQ(bridge.GetSentMessages()[0], bridge.GetSentMessages()[1]);
    EXPECT_EQ(bridge.GetSentMessages()[1]["type"], "response");
    EXPECT_EQ(bridge.GetSentMessages()[1]["id"].get<int>(), 7);
    EXPECT_EQ(bridge.GetSentMessages()[1]["result"], payload);
}

TEST_F(BridgeDeferredTest, SendRaw_UsesDirectWriteEnvelope) {
    bridge.RegisterApiDeferred("test.raw", [](const json&, Responder r) {
        r.SendRaw(R"({"tracks":[{"absolutePath":"C:\\music\\a.flac"}],"total":1})");
    });

    bridge.HandleMessage(MakeMessage(11, "test.raw"));

    ASSERT_EQ(bridge.GetSentRawMessages().size(), 1u);
    EXPECT_EQ(
        bridge.LastRawMessage(),
        R"({"type":"response","id":11,"result":{"tracks":[{"absolutePath":"C:\\music\\a.flac"}],"total":1}})");
    EXPECT_EQ(bridge.LastMessage()["result"]["total"].get<int>(), 1);
}

TEST_F(BridgeDeferredTest, StringId_PreservedOnDeferredPath) {
    bridge.RegisterApiDeferred("test.strId", [](const json&, Responder r) {
        r.SendJson({{"ok", true}});
    });

    bridge.HandleMessage(MakeMessage("abc-123", "test.strId"));

    EXPECT_EQ(bridge.LastMessage()["id"].get<std::string>(), "abc-123");
}

// ============================================
// 延迟本义：handler 返回时尚未回包
// ============================================

TEST_F(BridgeDeferredTest, DispatchDoesNotAutoRespond_ResponseArrivesLater) {
    std::optional<Responder> saved;
    bridge.RegisterApiDeferred("test.later", [&saved](const json&, Responder r) { saved = r; });

    bridge.HandleMessage(MakeMessage(12, "test.later"));

    // 分发段结束但没有任何包 —— 真身里这段是主线程段，回包要等 worker 完成
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
    ASSERT_TRUE(saved.has_value());

    saved->SendJson({{"success", true}});

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 12);
    EXPECT_TRUE(bridge.LastMessage()["result"]["success"].get<bool>());
}

// ============================================
// 恰好一次不变量（§4-B6）
// ============================================

TEST_F(BridgeDeferredTest, DoubleResponse_SecondDropped) {
    bridge.RegisterApiDeferred("test.twice", [](const json&, Responder r) {
        r.SendJson({{"first", true}});
        r.SendRaw(R"({"second":true})");
    });

    bridge.HandleMessage(MakeMessage(1, "test.twice"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_TRUE(bridge.LastMessage()["result"]["first"].get<bool>());
    EXPECT_EQ(bridge.GetSentRawMessages().size(), 0u);  // 第二包连信封都没拼
    EXPECT_EQ(bridge.GetDuplicateDropCount(), 1u);
}

TEST_F(BridgeDeferredTest, ErrorAfterSuccess_Dropped) {
    bridge.RegisterApiDeferred("test.successThenError", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
        r.SendError("late failure", "INTERNAL_ERROR", "test.successThenError");
    });

    bridge.HandleMessage(MakeMessage(2, "test.successThenError"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_FALSE(bridge.LastMessage().contains("error"));
    EXPECT_EQ(bridge.GetDuplicateDropCount(), 1u);
}

// ============================================
// handler 异常（§6-T2 / §7-R7）
// ============================================

TEST_F(BridgeDeferredTest, HandlerThrows_SingleInternalErrorEnvelope) {
    bridge.RegisterApiDeferred("test.throw", [](const json&, Responder) {
        throw std::runtime_error("main-thread segment failed");
    });

    bridge.HandleMessage(MakeMessage(3, "test.throw"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    const auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "response");
    EXPECT_EQ(msg["id"].get<int>(), 3);
    EXPECT_EQ(msg["code"], "INTERNAL_ERROR");
    EXPECT_EQ(msg["error"], "main-thread segment failed");
}

TEST_F(BridgeDeferredTest, HandlerThrowsNonStd_SingleInternalErrorEnvelope) {
    bridge.RegisterApiDeferred("test.throwNonStd", [](const json&, Responder) { throw 42; });

    bridge.HandleMessage(MakeMessage(4, "test.throwNonStd"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.LastMessage()["code"], "INTERNAL_ERROR");
    EXPECT_EQ(bridge.LastMessage()["error"], "Unknown internal error");
}

TEST_F(BridgeDeferredTest, RespondThenThrow_GateBlocksErrorEnvelope) {
    bridge.RegisterApiDeferred("test.respondThenThrow", [](const json&, Responder r) {
        r.SendJson({{"success", true}, {"stage", "done"}});
        throw std::runtime_error("late failure");
    });

    bridge.HandleMessage(MakeMessage(5, "test.respondThenThrow"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    const auto& msg = bridge.LastMessage();
    EXPECT_FALSE(msg.contains("error"));  // 成功包，不是 INTERNAL_ERROR 信封
    EXPECT_TRUE(msg["result"]["success"].get<bool>());
    EXPECT_EQ(msg["result"]["stage"], "done");
    EXPECT_EQ(bridge.GetDuplicateDropCount(), 1u);
}

// ============================================
// 通知型调用（空 id）
// ============================================

TEST_F(BridgeDeferredTest, NotificationCall_NoResponsePackets) {
    int invoked = 0;
    bridge.RegisterApiDeferred("test.notify", [&invoked](const json&, Responder r) {
        ++invoked;
        r.SendJson({{"success", true}});
        r.SendRaw(R"({"success":true})");
        r.SendError("boom", "INTERNAL_ERROR", "test.notify");
    });

    bridge.HandleMessage(MakeNotification("test.notify"));

    EXPECT_EQ(invoked, 1);  // handler 照常执行，只是回包全丢
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
    EXPECT_EQ(bridge.GetSentRawMessages().size(), 0u);
    EXPECT_EQ(bridge.GetDuplicateDropCount(), 0u);  // 静默丢弃，不计入重复
}

TEST_F(BridgeDeferredTest, NotificationCall_HandlerThrows_NoErrorPacket) {
    bridge.RegisterApiDeferred("test.notifyThrow", [](const json&, Responder) {
        throw std::runtime_error("boom");
    });

    bridge.HandleMessage(MakeNotification("test.notifyThrow"));

    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

// ============================================
// target 失活（§7-R4，崩溃重建竞态）
// ============================================

TEST_F(BridgeDeferredTest, DeadTarget_ResponseDropped) {
    bridge.SetTargetLivePredicate([] { return false; });
    bridge.RegisterApiDeferred("test.dead", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
    });

    bridge.HandleMessage(MakeMessage(6, "test.dead"));

    EXPECT_EQ(bridge.GetMessageCount(), 0u);
    EXPECT_EQ(bridge.GetDeadTargetDropCount(), 1u);
}

TEST_F(BridgeDeferredTest, DeadTarget_ErrorEnvelopeAlsoDropped) {
    bridge.SetTargetLivePredicate([] { return false; });
    bridge.RegisterApiDeferred("test.deadThrow", [](const json&, Responder) {
        throw std::runtime_error("boom");
    });

    bridge.HandleMessage(MakeMessage(7, "test.deadThrow"));

    EXPECT_EQ(bridge.GetMessageCount(), 0u);
    EXPECT_EQ(bridge.GetDeadTargetDropCount(), 1u);
}

TEST_F(BridgeDeferredTest, DeadTarget_ConsumesTheGate) {
    // 闸门在存活守卫之前：因失活被丢的那一次同样把闸门用掉了，重试不会补发
    bridge.SetTargetLivePredicate([] { return false; });
    bridge.RegisterApiDeferred("test.deadRetry", [](const json&, Responder r) {
        r.SendJson({{"attempt", 1}});
        r.SendJson({{"attempt", 2}});
    });

    bridge.HandleMessage(MakeMessage(8, "test.deadRetry"));

    EXPECT_EQ(bridge.GetMessageCount(), 0u);
    EXPECT_EQ(bridge.GetDeadTargetDropCount(), 1u);
    EXPECT_EQ(bridge.GetDuplicateDropCount(), 1u);
}

TEST_F(BridgeDeferredTest, LiveTarget_ResponseDelivered) {
    // 谓词对照组：同一路径在存活时必须照常送达（防守卫写成恒丢弃）
    bridge.SetTargetLivePredicate([] { return true; });
    bridge.RegisterApiDeferred("test.live", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
    });

    bridge.HandleMessage(MakeMessage(9, "test.live"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.GetDeadTargetDropCount(), 0u);
}

// ============================================
// worker 线程回包（闸门的线程安全面）
// ============================================

TEST_F(BridgeDeferredTest, WorkerThreadResponse_ExactlyOnceAndIntact) {
    json track;
    track["absolutePath"] = "C:\\music\\a.flac";
    const json payload = {{"success", true}, {"tracks", json::array({track})}, {"total", 1}};

    bridge.RegisterApiDeferred("test.worker", [payload](const json&, Responder r) {
        // 真身的 worker 段异步返回；此处 join 只为让断言在测试线程可见
        std::thread worker([r, payload]() { r.SendJson(payload); });
        worker.join();
    });

    bridge.HandleMessage(MakeMessage(10, "test.worker"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 10);
    EXPECT_EQ(bridge.LastMessage()["result"], payload);
}

TEST_F(BridgeDeferredTest, ConcurrentResponders_ExactlyOnePacket) {
    bridge.RegisterApiDeferred("test.race", [](const json&, Responder r) {
        std::atomic<bool> start{false};
        std::vector<std::thread> workers;
        workers.reserve(static_cast<size_t>(kRaceThreads));
        for (int i = 0; i < kRaceThreads; ++i) {
            workers.emplace_back([&start, r, i]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                r.SendJson({{"winner", i}});
            });
        }
        start.store(true, std::memory_order_release);
        for (auto& worker : workers) {
            worker.join();
        }
    });

    bridge.HandleMessage(MakeMessage(13, "test.race"));

    // 闸门赢家恰好一个：消息队列只被那一个线程碰过
    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    const int winner = bridge.LastMessage()["result"]["winner"].get<int>();
    EXPECT_GE(winner, 0);
    EXPECT_LT(winner, kRaceThreads);
    EXPECT_EQ(bridge.GetDuplicateDropCount(), static_cast<size_t>(kRaceThreads - 1));
}

// ============================================
// 注册面：deferred 与同步注册在查询/注销面等价可见
// ============================================

TEST_F(BridgeDeferredTest, DeferredRegistration_VisibleToRegistryQueries) {
    bridge.RegisterApiDeferred("library.query", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
    });

    // HasApi 是插件覆盖内建 API 的冲突检查口径；清单是偏好页/PluginRegistry 的数据源
    EXPECT_TRUE(bridge.HasApi("library.query"));
    const auto names = bridge.GetRegisteredApiNames();
    EXPECT_NE(std::find(names.begin(), names.end(), "library.query"), names.end());
}

TEST_F(BridgeDeferredTest, UnregisterApi_RemovesDeferredHandler) {
    bridge.RegisterApiDeferred("library.query", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
    });
    bridge.UnregisterApi("library.query");

    EXPECT_FALSE(bridge.HasApi("library.query"));
    bridge.HandleMessage(MakeMessage(14, "library.query"));
    EXPECT_EQ(bridge.LastMessage()["code"], "METHOD_NOT_FOUND");
}

TEST_F(BridgeDeferredTest, UnknownMethod_StillMethodNotFound_WithDeferredTablePresent) {
    bridge.RegisterApiDeferred("test.present", [](const json&, Responder r) {
        r.SendJson({{"success", true}});
    });

    bridge.HandleMessage(MakeMessage(15, "test.absent"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.LastMessage()["code"], "METHOD_NOT_FOUND");
}

TEST_F(BridgeDeferredTest, SyncRegistrationWins_WhenBothTablesHaveMethod) {
    // 查表顺序：同步表优先。真身在 RegisterApiDeferred 时对该情形记警告但不覆盖。
    bridge.RegisterApi("test.dual", [](const json&) -> json { return {{"path", "sync"}}; });
    bridge.RegisterApiDeferred("test.dual", [](const json&, Responder r) {
        r.SendJson({{"path", "deferred"}});
    });

    bridge.HandleMessage(MakeMessage(16, "test.dual"));

    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    EXPECT_EQ(bridge.LastMessage()["result"]["path"], "sync");
}
