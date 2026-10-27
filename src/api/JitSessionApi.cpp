#include "pch.h"
#include "api/JitSessionApi.h"
#include "api/TypedApi.h"
#include "api/generated/JitSessionSchema.h"
#include "core/QueueManager.h"

namespace {
namespace js = api::jitSession;

void GetCapabilities(const js::GetCapabilitiesParams&, const DeferredResponder& responder) {
    api::Send(responder, api::Result<js::GetCapabilitiesResult>{js::GetCapabilitiesResult{1, true, false, true, true, "afterTrackStart"}});
}
void Create(const js::CreateParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionCreate(params, caller));
}
void Discover(const js::DiscoverParams&, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionDiscover(caller));
}
void GetState(const js::GetStateParams& params, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionGetState(params));
}
void ClaimControl(const js::ClaimControlParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionClaim(params, caller));
}
void Play(const js::PlayParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionPlay(params, caller));
}
void SetNext(const js::SetNextParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionSetNext(params, caller));
}
void Skip(const js::SkipParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionSkip(params, caller));
}
void Stop(const js::StopParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionStop(params, caller));
}
void GetOperation(const js::GetOperationParams& params, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionGetOperation(params));
}
void Release(const js::ReleaseParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionRelease(params, caller));
}
void PrepareAdoption(const js::PrepareAdoptionParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionPrepareAdoption(params, caller));
}
void GetNativeContext(const js::GetNativeContextParams& params, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionGetNativeContext(params));
}
void AdoptNative(const js::AdoptNativeParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionAdoptNative(params, caller));
}
void PlayNative(const js::PlayNativeParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionPlayNative(params, caller));
}
void SetNativeNext(const js::SetNativeNextParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionSetNativeNext(params, caller));
}
void SetNativeQueue(const js::SetNativeQueueParams& params, const CallerContext& caller, const DeferredResponder& responder) {
    api::Send(responder, g_QueueManager.SessionSetNativeQueue(params, caller));
}
}

// 分发前后都核对发起文档，避免导航期间排队的请求取得新文档的身份。
void RegisterJitSessionApi() {
    api::RegisterApiDeferred("jitSession.getCapabilities", GetCapabilities);
    api::RegisterApiDeferred("jitSession.create", Create);
    api::RegisterApiDeferred("jitSession.discover", Discover);
    api::RegisterApiDeferred("jitSession.getState", GetState);
    api::RegisterApiDeferred("jitSession.claimControl", ClaimControl);
    api::RegisterApiDeferred("jitSession.play", Play);
    api::RegisterApiDeferred("jitSession.setNext", SetNext);
    api::RegisterApiDeferred("jitSession.skip", Skip);
    api::RegisterApiDeferred("jitSession.stop", Stop);
    api::RegisterApiDeferred("jitSession.getOperation", GetOperation);
    api::RegisterApiDeferred("jitSession.release", Release);
    api::RegisterApiDeferred("jitSession.prepareAdoption", PrepareAdoption);
    api::RegisterApiDeferred("jitSession.getNativeContext", GetNativeContext);
    api::RegisterApiDeferred("jitSession.adoptNative", AdoptNative);
    api::RegisterApiDeferred("jitSession.playNative", PlayNative);
    api::RegisterApiDeferred("jitSession.setNativeNext", SetNativeNext);
    api::RegisterApiDeferred("jitSession.setNativeQueue", SetNativeQueue);
}
