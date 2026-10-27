// test_jit_session_model.cpp - jitSession 的写凭据、认领、重试去重、操作记录与尝试结局
//
// 直接链接生产头 core/JitSessionModel.h。
#include "pch.h"
#include "core/JitSessionModel.h"

#include <string>

using jit_session::CheckClaim;
using jit_session::CheckWrite;
using jit_session::ClaimCheck;
using jit_session::ClaimMode;
using jit_session::ClassifyStop;
using jit_session::ComparableResourcePath;
using jit_session::EngineFailurePath;
using jit_session::Operation;
using jit_session::OperationLog;
using jit_session::OpKind;
using jit_session::OpStatus;
using jit_session::Outcome;
using jit_session::RequestLog;
using jit_session::StopKind;
using jit_session::WriteCheck;

// ---------------------------------------------------------------------------
// 写凭据与认领
// ---------------------------------------------------------------------------

TEST(JitSessionWrite, OwnerAndRevisionAreCheckedInThatOrder) {
    EXPECT_EQ(CheckWrite("tok", 3, "tok", 3), WriteCheck::Ok);
    EXPECT_EQ(CheckWrite("tok", 3, "tok", 2), WriteCheck::Conflict);
    // 令牌不对时不透露版本号是否也过期
    EXPECT_EQ(CheckWrite("tok", 3, "other", 2), WriteCheck::NotOwner);
    // 会话没有控制者（令牌已撤销）时任何令牌都不算数
    EXPECT_EQ(CheckWrite("", 3, "", 3), WriteCheck::NotOwner);
}

TEST(JitSessionClaim, ResumeOnlyForTheSameWindowAfterItsDocumentIsGone) {
    EXPECT_EQ(CheckClaim(ClaimMode::Resume, true, false, false, 0, 4), ClaimCheck::Ok);
    EXPECT_EQ(CheckClaim(ClaimMode::Resume, true, true, false, 0, 4), ClaimCheck::NotOwner);
    EXPECT_EQ(CheckClaim(ClaimMode::Resume, false, false, false, 0, 4), ClaimCheck::NotOwner);
}

TEST(JitSessionClaim, HandoverMustNameTheCurrentEpoch) {
    EXPECT_EQ(CheckClaim(ClaimMode::Handover, false, true, true, 4, 4), ClaimCheck::Ok);
    EXPECT_EQ(CheckClaim(ClaimMode::Handover, false, true, true, 3, 4), ClaimCheck::Conflict);
    EXPECT_EQ(CheckClaim(ClaimMode::Handover, false, true, false, 0, 4), ClaimCheck::Conflict);
}

// ---------------------------------------------------------------------------
// 重试去重
// ---------------------------------------------------------------------------

TEST(JitSessionRequestLog, RetryReplaysTheFirstResponse) {
    RequestLog log;
    std::string response;
    EXPECT_EQ(log.Check("r1", 7, &response), RequestLog::Hit::New);
    log.Record("r1", 7, "{\"operationId\":\"o1\"}");
    EXPECT_EQ(log.Check("r1", 7, &response), RequestLog::Hit::Replay);
    EXPECT_EQ(response, "{\"operationId\":\"o1\"}");
    // 同一 requestId 换了载荷
    EXPECT_EQ(log.Check("r1", 8, &response), RequestLog::Hit::Conflict);
}

TEST(JitSessionRequestLog, AnEvictedRequestIdIsNeverRunAgain) {
    RequestLog log;
    log.Record("first", 1, "a");
    for (std::size_t i = 0; i < RequestLog::kRecent; ++i) {
        log.Record("r" + std::to_string(i), 1, "x");
    }
    std::string response;
    EXPECT_EQ(log.Check("first", 1, &response), RequestLog::Hit::Conflict);
    EXPECT_EQ(log.Check("r0", 1, &response), RequestLog::Hit::Replay);
}

// ---------------------------------------------------------------------------
// 操作记录
// ---------------------------------------------------------------------------

namespace {

Operation Op(const std::string& id) {
    Operation op;
    op.id = id;
    op.kind = OpKind::Play;
    return op;
}

}  // namespace

TEST(JitSessionOperationLog, EachOperationEndsExactlyOnce) {
    OperationLog log;
    log.Begin(Op("o1"));
    EXPECT_EQ(log.Find("o1")->status, OpStatus::Pending);
    EXPECT_TRUE(log.Finish("o1", OpStatus::Superseded));
    EXPECT_FALSE(log.Finish("o1", OpStatus::Completed));
    EXPECT_EQ(log.Find("o1")->status, OpStatus::Superseded);
    EXPECT_FALSE(log.Finish("missing", OpStatus::Completed));
    EXPECT_FALSE(log.Finish("o1", OpStatus::Pending));
}

TEST(JitSessionOperationLog, KeepsPendingAndTheLatestFinished) {
    OperationLog log;
    log.Begin(Op("pending"));
    for (std::size_t i = 0; i <= OperationLog::kFinished; ++i) {
        const std::string id = "o" + std::to_string(i);
        log.Begin(Op(id));
        log.Finish(id, OpStatus::Completed);
    }
    EXPECT_EQ(log.Find("o0"), nullptr);
    EXPECT_NE(log.Find("o1"), nullptr);
    ASSERT_NE(log.Find("pending"), nullptr);
    EXPECT_EQ(log.PendingIds().size(), 1u);
}

TEST(JitSessionOperationLog, ALongRunningOperationRemainsReadableAfterItFinishes) {
    OperationLog log;
    log.Begin(Op("slow"));
    for (std::size_t i = 0; i < OperationLog::kFinished; ++i) {
        const std::string id = "fast" + std::to_string(i);
        log.Begin(Op(id));
        ASSERT_TRUE(log.Finish(id, OpStatus::Completed));
    }

    ASSERT_TRUE(log.Finish("slow", OpStatus::Failed, "resolutionFailed"));
    ASSERT_NE(log.Find("slow"), nullptr);
    EXPECT_EQ(log.Find("slow")->status, OpStatus::Failed);
    EXPECT_EQ(log.Find("fast0"), nullptr);
    EXPECT_NE(log.Find("fast1"), nullptr);
}

TEST(JitSessionOperationLog, FinishingOutOfOrderPreservesThePendingAcceptanceOrder) {
    OperationLog log;
    log.Begin(Op("first"));
    log.Begin(Op("second"));
    log.Begin(Op("third"));
    ASSERT_TRUE(log.Finish("second", OpStatus::Superseded));
    EXPECT_EQ(log.PendingIds(), (std::deque<std::string>{"first", "third"}));
}

// ---------------------------------------------------------------------------
// 尝试结局
// ---------------------------------------------------------------------------

TEST(JitSessionOutcome, CompletedOnlyWhenPlaybackReachedTheEnd) {
    EXPECT_EQ(ClassifyStop(StopKind::EndOfFile, 179.4, 180.0), Outcome::Completed);
    // 整秒长的曲目：最后一次整秒回报恰好离结尾 1 秒
    EXPECT_EQ(ClassifyStop(StopKind::EndOfFile, 3.0, 4.0), Outcome::Completed);
    EXPECT_EQ(ClassifyStop(StopKind::EndOfFile, 12.0, 180.0), Outcome::Unknown);
    // 时长为 0 的 eof 可能是解码失败，不判为完成
    EXPECT_EQ(ClassifyStop(StopKind::EndOfFile, 0.0, 0.0), Outcome::Unknown);
    EXPECT_EQ(ClassifyStop(StopKind::User, 30.0, 180.0), Outcome::Stopped);
    EXPECT_EQ(ClassifyStop(StopKind::Other, 179.9, 180.0), Outcome::Unknown);
}

// ---------------------------------------------------------------------------

TEST(JitSessionRecheck, EngineFailureLineYieldsOnlyTheQuotedPath) {
    // 实测的控制台行形状：说明文字随界面语言变化，路径行不变。
    EXPECT_EQ(EngineFailurePath("\xE6\x97\xA0\xE6\xB3\x95(Forbidden (403)):\r\n\"http://127.0.0.1:61382/denied.mp3\""),
              std::optional<std::string>("http://127.0.0.1:61382/denied.mp3"));
    EXPECT_EQ(EngineFailurePath("Unable to open item for playback (Unsupported format):\n\"E:\\a\\b.cue|subsong:2\"\n"),
              std::optional<std::string>("E:\\a\\b.cue|subsong:2"));
    EXPECT_EQ(EngineFailurePath("Opening track: \"E:\\a.flac\""), std::nullopt);
    EXPECT_EQ(EngineFailurePath("Something happened.\n\"E:\\a.flac\""), std::nullopt);
    EXPECT_EQ(EngineFailurePath("Failed (x):\nE:\\a.flac"), std::nullopt);
}

TEST(JitSessionRecheck, ConsoleAndHandlePathsCompareEqual) {
    EXPECT_EQ(ComparableResourcePath("file://E:\\Music\\A.cue"), ComparableResourcePath("e:\\music\\a.cue|subsong:2"));
    EXPECT_EQ(ComparableResourcePath("http://Host/x.mp3"), "http://host/x.mp3");
    EXPECT_NE(ComparableResourcePath("E:\\a.flac"), ComparableResourcePath("E:\\b.flac"));
}
