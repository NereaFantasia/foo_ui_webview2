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
// SendResponseRaw format (直写通道)
//
// result 段是调用方已序列化好的 UTF-8 JSON 串，信封靠字符串拼接。断言分两层：
// 字节层看 id 二态与 result 原样嵌入，语义层与 SendResponse 的 DOM 产物对拍
// （键序不作要求：SendResponse 走 nlohmann dump，其对象键序按 std::map 排序）。
// ============================================

TEST_F(SendFormatTest, Raw_EnvelopeIsThreeKeyResponse) {
    bridge.SendResponseRaw("1", R"({"ok":true})");
    EXPECT_EQ(bridge.LastRawMessage(), R"({"type":"response","id":1,"result":{"ok":true}})");

    auto& msg = bridge.LastMessage();
    EXPECT_EQ(msg["type"], "response");
    EXPECT_EQ(msg.size(), 3u);  // 只有 type/id/result，不夹带 error/code
    EXPECT_FALSE(msg.contains("error"));
}

TEST_F(SendFormatTest, Raw_NumericIdEmittedAsJsonNumber) {
    bridge.SendResponseRaw("1", R"({"ok":true})");
    // 字节层：不带引号（带引号即页面侧 _callbacks Map miss → 30s 超时假死）
    EXPECT_NE(bridge.LastRawMessage().find(R"("id":1,)"), std::string::npos);
    EXPECT_EQ(bridge.LastRawMessage().find(R"("id":"1")"), std::string::npos);
    EXPECT_TRUE(bridge.LastMessage()["id"].is_number());
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 1);
}

TEST_F(SendFormatTest, Raw_StringIdEmittedAsQuotedJsonString) {
    bridge.SendResponseRaw("abc-def", R"({"ok":true})");
    EXPECT_NE(bridge.LastRawMessage().find(R"("id":"abc-def",)"), std::string::npos);
    EXPECT_TRUE(bridge.LastMessage()["id"].is_string());
    EXPECT_EQ(bridge.LastMessage()["id"].get<std::string>(), "abc-def");
}

TEST_F(SendFormatTest, Raw_StringIdIsJsonEscaped) {
    // 非数值 id 必须过 JSON 转义，否则引号/反斜杠会把信封拼坏
    bridge.SendResponseRaw("a\"b\\c\nd", R"({"ok":true})");
    EXPECT_NE(bridge.LastRawMessage().find(R"("id":"a\"b\\c\nd",)"), std::string::npos);
    EXPECT_EQ(bridge.LastMessage()["id"].get<std::string>(), "a\"b\\c\nd");
}

TEST_F(SendFormatTest, Raw_OddIdFormsMatchSendResponse) {
    // stoi 的前缀语义是既有 wire 行为，两条通道必须同口径：
    // "007" -> 7（前导零丢弃）、"12abc" -> 12（尾部垃圾丢弃）、"" -> 字符串 ""
    for (const char* id : {"1", "42", "007", "12abc", "abc-def", "", "0", "-5"}) {
        bridge.ClearMessages();
        const json result = {{"ok", true}};
        bridge.SendResponse(id, result);
        bridge.SendResponseRaw(id, result.dump());

        ASSERT_EQ(bridge.GetMessageCount(), 2u) << "id=" << id;
        // 语义等价（含 id 的 number/string 二态与其取值）
        EXPECT_EQ(bridge.GetSentMessages()[0], bridge.GetSentMessages()[1]) << "id=" << id;
    }
}

TEST_F(SendFormatTest, Raw_OddIdNumericValues) {
    bridge.SendResponseRaw("007", "{}");
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 7);
    EXPECT_NE(bridge.LastRawMessage().find(R"("id":7,)"), std::string::npos);

    bridge.SendResponseRaw("12abc", "{}");
    EXPECT_EQ(bridge.LastMessage()["id"].get<int>(), 12);
    EXPECT_NE(bridge.LastRawMessage().find(R"("id":12,)"), std::string::npos);
}

TEST_F(SendFormatTest, Raw_ResultEmbeddedVerbatim) {
    // result 段原样拼入：不得二次转义（转义一次即页面侧拿到字符串而非对象）
    const std::string payload =
        R"({"tracks":[{"title":"播放 \"引号\"","path":"C:\\music\\a.flac"}],"total":1})";
    bridge.SendResponseRaw("5", payload);

    EXPECT_NE(bridge.LastRawMessage().find(payload), std::string::npos);
    EXPECT_EQ(bridge.LastRawMessage().find(R"(\\\")"), std::string::npos);

    auto& msg = bridge.LastMessage();
    EXPECT_TRUE(msg["result"]["tracks"].is_array());
    EXPECT_EQ(msg["result"]["tracks"][0]["title"], "播放 \"引号\"");
    EXPECT_EQ(msg["result"]["tracks"][0]["path"], "C:\\music\\a.flac");
    EXPECT_EQ(msg["result"]["total"].get<int>(), 1);
}

TEST_F(SendFormatTest, Raw_ResultNonObjectShapes) {
    // result 段的形状由调用方决定，通道不假设是对象
    bridge.SendResponseRaw("1", "[1,2,3]");
    EXPECT_TRUE(bridge.LastMessage()["result"].is_array());
    EXPECT_EQ(bridge.LastMessage()["result"].size(), 3u);

    bridge.SendResponseRaw("2", "null");
    EXPECT_TRUE(bridge.LastMessage()["result"].is_null());

    bridge.SendResponseRaw("3", R"("hello")");
    EXPECT_EQ(bridge.LastMessage()["result"], "hello");
}

TEST_F(SendFormatTest, Raw_EmptyResultObject) {
    bridge.SendResponseRaw("1", "{}");
    EXPECT_EQ(bridge.LastRawMessage(), R"({"type":"response","id":1,"result":{}})");
    EXPECT_TRUE(bridge.LastMessage()["result"].is_object());
    EXPECT_TRUE(bridge.LastMessage()["result"].empty());
}

TEST_F(SendFormatTest, Raw_OrderingSharedWithDomChannel) {
    bridge.EmitEvent("ev1", {});
    bridge.SendResponse("1", {{"a", 1}});
    bridge.SendResponseRaw("2", R"({"b":2})");
    ASSERT_EQ(bridge.GetMessageCount(), 3u);
    EXPECT_EQ(bridge.GetSentMessages()[0]["event"], "ev1");
    EXPECT_EQ(bridge.GetSentMessages()[1]["result"]["a"].get<int>(), 1);
    EXPECT_EQ(bridge.GetSentMessages()[2]["result"]["b"].get<int>(), 2);
    EXPECT_EQ(bridge.GetSentRawMessages().size(), 1u);  // 只有直写通道入 raw 队列
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
