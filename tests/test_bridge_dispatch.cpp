// test_bridge_dispatch.cpp - BridgeCore dispatch contract tests
// Uses BridgeDispatchSimulator to test the message routing logic
// without foobar2000 SDK or WebView2 dependencies.
#include "pch.h"
#include "harness/BridgeDispatchSimulator.h"

using json = nlohmann::json;

class BridgeDispatchTest : public ::testing::Test {
protected:
    BridgeDispatchSimulator bridge;

    void SetUp() override {
        // Register a simple echo handler
        bridge.RegisterApi("test.echo", [](const json& params) -> json {
            return {{"success", true}, {"echo", params}};
        });
        // Register a handler that always fails
        bridge.RegisterApi("test.fail", [](const json&) -> json {
            throw std::runtime_error("intentional failure");
        });
        // Register a handler that throws a non-std exception
        bridge.RegisterApi("test.failNonStd", [](const json&) -> json {
            throw 42;
        });
        // Register a handler that returns data
        bridge.RegisterApi("playback.getState", [](const json&) -> json {
            return {{"success", true}, {"state", "playing"}, {"position", 42.5}};
        });
    }

    std::string MakeMessage(const std::string& id, const std::string& method,
                            const json& params = json::object()) {
        json msg;
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
        return msg.dump();
    }

    std::string MakeMessageNumId(int id, const std::string& method,
                                  const json& params = json::object()) {
        json msg;
        msg["id"] = id;
        msg["method"] = method;
        msg["params"] = params;
        return msg.dump();
    }
};

// ============================================
// Handler routing
// ============================================

TEST_F(BridgeDispatchTest, ValidMessage_RoutesToHandler) {
    bridge.HandleMessage(MakeMessage("1", "test.echo", {{"key", "value"}}));
    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["type"], "response");
    EXPECT_TRUE(resp["result"]["success"].get<bool>());
    EXPECT_EQ(resp["result"]["echo"]["key"], "value");
}

TEST_F(BridgeDispatchTest, HandlerResultWrappedInResponse) {
    bridge.HandleMessage(MakeMessage("r1", "playback.getState"));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["result"]["state"], "playing");
    EXPECT_DOUBLE_EQ(resp["result"]["position"].get<double>(), 42.5);
}

TEST_F(BridgeDispatchTest, UnknownMethod_MethodNotFound) {
    bridge.HandleMessage(MakeMessage("2", "nonexistent.method"));
    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["type"], "response");
    EXPECT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["code"], "METHOD_NOT_FOUND");
}

TEST_F(BridgeDispatchTest, CaseSensitive_MethodRouting) {
    bridge.HandleMessage(MakeMessage("3", "Test.Echo"));  // wrong case
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["code"], "METHOD_NOT_FOUND");
}

// ============================================
// Message parsing
// ============================================

TEST_F(BridgeDispatchTest, EmptyMethod_InvalidRequest) {
    bridge.HandleMessage(MakeMessage("4", ""));
    ASSERT_EQ(bridge.GetMessageCount(), 1u);
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["code"], "INVALID_REQUEST");
}

TEST_F(BridgeDispatchTest, WhitespaceOnlyMethod_InvalidRequest) {
    bridge.HandleMessage(MakeMessage("5", "   "));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["code"], "INVALID_REQUEST");
}

TEST_F(BridgeDispatchTest, MethodWithWhitespace_Trimmed) {
    bridge.HandleMessage(MakeMessage("6", "  test.echo  "));
    auto& resp = bridge.LastMessage();
    EXPECT_TRUE(resp["result"]["success"].get<bool>());
}

TEST_F(BridgeDispatchTest, InvalidJson_NoCrash) {
    EXPECT_NO_THROW(bridge.HandleMessage("{invalid json!!!"));
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

TEST_F(BridgeDispatchTest, EmptyString_NoCrash) {
    EXPECT_NO_THROW(bridge.HandleMessage(""));
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

TEST_F(BridgeDispatchTest, MissingParams_DefaultsToEmptyObject) {
    json msg = {{"id", "7"}, {"method", "test.echo"}};  // no "params"
    bridge.HandleMessage(msg.dump());
    auto& resp = bridge.LastMessage();
    EXPECT_TRUE(resp["result"]["echo"].is_object());
    EXPECT_TRUE(resp["result"]["echo"].empty());
}

// ============================================
// ID handling
// ============================================

TEST_F(BridgeDispatchTest, NumericId_PreservedInResponse) {
    bridge.HandleMessage(MakeMessageNumId(42, "test.echo"));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["id"].get<int>(), 42);
}

TEST_F(BridgeDispatchTest, StringId_PreservedInResponse) {
    bridge.HandleMessage(MakeMessage("abc-123", "test.echo"));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["id"].get<std::string>(), "abc-123");
}

TEST_F(BridgeDispatchTest, NoId_NoResponseSent) {
    json msg = {{"method", "test.echo"}, {"params", json::object()}};
    bridge.HandleMessage(msg.dump());
    EXPECT_EQ(bridge.GetMessageCount(), 0u);  // fire-and-forget
}

TEST_F(BridgeDispatchTest, NoId_NoMethod_NoResponse) {
    json msg = {{"params", json::object()}};
    bridge.HandleMessage(msg.dump());
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

// ============================================
// Error handling
// ============================================

TEST_F(BridgeDispatchTest, HandlerThrows_InternalError) {
    bridge.HandleMessage(MakeMessage("8", "test.fail"));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["code"], "INTERNAL_ERROR");
    EXPECT_EQ(resp["error"], "intentional failure");
}

TEST_F(BridgeDispatchTest, HandlerThrows_NoId_NoErrorSent) {
    json msg = {{"method", "test.fail"}};
    bridge.HandleMessage(msg.dump());
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

TEST_F(BridgeDispatchTest, HandlerThrowsNonStd_InternalError) {
    bridge.HandleMessage(MakeMessage("10", "test.failNonStd"));
    auto& resp = bridge.LastMessage();
    EXPECT_EQ(resp["code"], "INTERNAL_ERROR");
    EXPECT_EQ(resp["error"], "Unknown internal error");
}

TEST_F(BridgeDispatchTest, HandlerThrowsNonStd_NoId_NoErrorSent) {
    json msg = {{"method", "test.failNonStd"}};
    bridge.HandleMessage(msg.dump());
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}

// ============================================
// Registration
// ============================================

TEST_F(BridgeDispatchTest, HasApi_True) {
    EXPECT_TRUE(bridge.HasApi("test.echo"));
}

TEST_F(BridgeDispatchTest, HasApi_False) {
    EXPECT_FALSE(bridge.HasApi("nonexistent"));
}

TEST_F(BridgeDispatchTest, UnregisterApi_RemovesHandler) {
    bridge.UnregisterApi("test.echo");
    EXPECT_FALSE(bridge.HasApi("test.echo"));
    bridge.HandleMessage(MakeMessage("9", "test.echo"));
    EXPECT_EQ(bridge.LastMessage()["code"], "METHOD_NOT_FOUND");
}

TEST_F(BridgeDispatchTest, GetRegisteredApiNames) {
    auto names = bridge.GetRegisteredApiNames();
    EXPECT_EQ(names.size(), 4u);
    // Check all registered names are present (order not guaranteed)
    auto hasName = [&](const std::string& name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };
    EXPECT_TRUE(hasName("test.echo"));
    EXPECT_TRUE(hasName("test.fail"));
    EXPECT_TRUE(hasName("test.failNonStd"));
    EXPECT_TRUE(hasName("playback.getState"));
}
