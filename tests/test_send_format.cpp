// test_send_format.cpp - Response/Error/Event envelope format tests
// Validates that the wire protocol matches JS SDK expectations.
#include "pch.h"
#include "harness/BridgeDispatchSimulator.h"

using json = nlohmann::json;

class SendFormatTest : public ::testing::Test {
protected:
    BridgeDispatchSimulator bridge;
};

// ============================================
// SendResponse format
// ============================================

TEST_F(SendFormatTest, Response_HasTypeField) {
    bridge.SendResponse("1", {{"success", true}});
    EXPECT_EQ(bridge.LastMessage()["type"], "response");
}

TEST_F(SendFormatTest, Response_NumericIdConversion) {
    bridge.SendResponse("42", {{"ok", true}});
    auto& msg = bridge.LastMessage();
    EXPECT_TRUE(msg["id"].is_number());
    EXPECT_EQ(msg["id"].get<int>(), 42);
}

TEST_F(SendFormatTest, Response_StringIdKept) {
    bridge.SendResponse("abc-def", {{"ok", true}});
    auto& msg = bridge.LastMessage();
    EXPECT_TRUE(msg["id"].is_string());
    EXPECT_EQ(msg["id"].get<std::string>(), "abc-def");
}

TEST_F(SendFormatTest, Response_ResultPassedThrough) {
    json result = {{"state", "playing"}, {"volume", 0.8}};
    bridge.SendResponse("1", result);
    EXPECT_EQ(bridge.LastMessage()["result"]["state"], "playing");
    EXPECT_DOUBLE_EQ(bridge.LastMessage()["result"]["volume"].get<double>(), 0.8);
}

TEST_F(SendFormatTest, Response_EmptyResult) {
    bridge.SendResponse("1", json::object());
    EXPECT_TRUE(bridge.LastMessage()["result"].is_object());
    EXPECT_TRUE(bridge.LastMessage()["result"].empty());
}

// ============================================
// SendError format
// ============================================

TEST_F(SendFormatTest, Error_HasTypeAndErrorFields) {
    bridge.SendError("1", -32601, "Method not found", "METHOD_NOT_FOUND");
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "response");
    EXPECT_EQ(msg["error"], "Method not found");
    EXPECT_EQ(msg["code"], "METHOD_NOT_FOUND");
}

TEST_F(SendFormatTest, Error_WithoutErrorCode) {
    bridge.SendError("1", -1, "unknown error");
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["error"], "unknown error");
    EXPECT_FALSE(msg.contains("code"));
}

TEST_F(SendFormatTest, Error_EmptyErrorCode_Omitted) {
    bridge.SendError("1", -1, "err", "");
    EXPECT_FALSE(bridge.LastMessage().contains("code"));
}

TEST_F(SendFormatTest, Error_NumericIdConversion) {
    bridge.SendError("99", -1, "err", "ERR");
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 99);
}

// ============================================
// EmitEvent format
// ============================================

TEST_F(SendFormatTest, Event_HasTypeAndEventFields) {
    bridge.EmitEvent("playback:stateChanged", {{"state", "paused"}});
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "event");
    EXPECT_EQ(msg["event"], "playback:stateChanged");
    EXPECT_EQ(msg["data"]["state"], "paused");
}

TEST_F(SendFormatTest, Event_EmptyData) {
    bridge.EmitEvent("audio:spectrum", json::object());
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "event");
    EXPECT_TRUE(msg["data"].is_object());
    EXPECT_TRUE(msg["data"].empty());
}

TEST_F(SendFormatTest, Event_ComplexData) {
    json data = {
        {"bins", json::array({0.1, 0.5, 0.8, 0.3})},
        {"fftSize", 1024}
    };
    bridge.EmitEvent("audio:spectrum", data);
    EXPECT_EQ(bridge.LastMessage()["data"]["fftSize"].get<int>(), 1024);
    EXPECT_EQ(bridge.LastMessage()["data"]["bins"].size(), 4u);
}

TEST_F(SendFormatTest, Event_ColonFormatNaming) {
    // Verify event names use colon format per project convention
    bridge.EmitEvent("playback:trackChanged", {{"title", "Song"}});
    std::string eventName = bridge.LastMessage()["event"].get<std::string>();
    EXPECT_NE(eventName.find(':'), std::string::npos);  // must contain colon
    EXPECT_EQ(eventName.find('.'), std::string::npos);   // must NOT contain dot
}

TEST_F(SendFormatTest, Event_EmptyEventName) {
    bridge.EmitEvent("", {{"x", 1}});
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "event");
    EXPECT_EQ(msg["event"], "");
}

TEST_F(SendFormatTest, Event_DataAsArray) {
    json arr = json::array({1, 2, 3});
    bridge.EmitEvent("test:array", arr);
    auto& msg = bridge.LastMessage();
    EXPECT_TRUE(msg["data"].is_array());
    EXPECT_EQ(msg["data"].size(), 3u);
}

TEST_F(SendFormatTest, Event_DataAsNull) {
    bridge.EmitEvent("test:null", nullptr);
    auto& msg = bridge.LastMessage();
    EXPECT_TRUE(msg["data"].is_null());
}

TEST_F(SendFormatTest, Event_DataAsString) {
    bridge.EmitEvent("test:string", json("hello"));
    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["data"], "hello");
}

// ============================================
// Message ordering
// ============================================

TEST_F(SendFormatTest, MultipleMessages_OrderPreserved) {
    bridge.EmitEvent("ev1", {});
    bridge.EmitEvent("ev2", {});
    bridge.SendResponse("1", {});
    ASSERT_EQ(bridge.GetMessageCount(), 3u);
    EXPECT_EQ(bridge.GetSentMessages()[0]["event"], "ev1");
    EXPECT_EQ(bridge.GetSentMessages()[1]["event"], "ev2");
    EXPECT_EQ(bridge.GetSentMessages()[2]["type"], "response");
}

TEST_F(SendFormatTest, ClearMessages) {
    bridge.EmitEvent("ev1", {});
    EXPECT_EQ(bridge.GetMessageCount(), 1u);
    bridge.ClearMessages();
    EXPECT_EQ(bridge.GetMessageCount(), 0u);
}
