#pragma once
// 按生成的参数结构体注册 API。参数结构、路径安全规格都取自 src/api/schema/<ns>.ts 生成的类型，
// 参数解析失败统一返回 INVALID_PARAMS，handler 只拿到已经校验过的结构体。
// handler 只能返回 Result<R>（ApiResult.h），R 取自同一方法的声明，「返回值符合声明」由编译器保证。
//
// 第一个实参是方法名字面量，第二个是 handler；registrations.mjs、MCP 门禁等按这个字面量找注册点。
// 本注释刻意不写出调用示例：那些工具按文本匹配，连注释里的示例也会当成注册点。
// BridgeCore 的原始注册是私有的，只经下面的 detail::Registrar 调用，已声明的方法因此绕不开生成的解析器。
// 方法名必须等于生成类型的 kMethod，写错直接编译失败，见 MethodName。
#include "pch.h"
#include "api/ApiParams.h"
#include "api/ApiResult.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace api {

namespace detail {
// BridgeCore 的友元：api::RegisterApi / RegisterApiDeferred 经它调用私有的原始注册。
struct Registrar {
    static void Register(const std::string& method, ApiHandler handler, std::vector<PathSecuritySpec> specs) {
        BridgeCore::GetInstance().RegisterApi(method, std::move(handler), std::move(specs));
    }
    static void RegisterDeferred(const std::string& method, DeferredApiHandler handler,
                                 std::vector<PathSecuritySpec> specs) {
        BridgeCore::GetInstance().RegisterApiDeferred(method, std::move(handler), std::move(specs));
    }
};
}  // namespace detail

static_assert(static_cast<int>(params::PathAccess::Read) == static_cast<int>(SecurityLevel::Read));
static_assert(static_cast<int>(params::PathAccess::Write) == static_cast<int>(SecurityLevel::Write));
static_assert(static_cast<int>(params::PathAccess::MediaRead) == static_cast<int>(SecurityLevel::MediaRead));
static_assert(static_cast<int>(params::PathAccess::MediaWrite) == static_cast<int>(SecurityLevel::MediaWrite));
static_assert(static_cast<int>(params::PathAccess::FileWrite) == static_cast<int>(SecurityLevel::FileWrite));

template <class P>
std::vector<PathSecuritySpec> PathSecuritySpecsOf() {
    std::vector<PathSecuritySpec> specs;
    for (const params::PathParam& p : P::kPathParams) {
        PathSecuritySpec spec;
        spec.paramKey = p.key;
        spec.level = static_cast<SecurityLevel>(p.access);
        spec.isArray = p.isArray;
        spec.skipInvalid = p.skipInvalid;
        if (p.nestedKey) spec.nestedKey = p.nestedKey;
        spec.stringElements = p.stringElements;
        specs.push_back(std::move(spec));
    }
    return specs;
}

// 构造函数是 consteval：名字与 P::kMethod 不一致时执行到 throw，常量求值失败，
// 编译器在注册那一行报错（MSVC 为 C7595）。与 std::format 检查格式串是同一个做法。
template <class P>
struct MethodName {
    const char* value;
    consteval MethodName(const char* name) : value(name) {
        if (std::string_view(name) != std::string_view(P::kMethod)) {
            throw "RegisterApi: method name differs from the kMethod of its generated params type";
        }
    }
};

// P 与 R 都只从 handler 推导；名字参数放在非推导语境里，再按 P 做上面的编译期比对。
// R 必须是这个方法自己的 <Method>Result（void 方法为 void），拿 A 的参数返回 B 的结果在注册处编译失败。
template <class P, class R>
void RegisterApi(std::type_identity_t<MethodName<P>> method, Result<R> (*handler)(const P&)) {
    static_assert(std::is_same_v<R, typename P::Result>, "handler returns the result type of another method");
    detail::Registrar::Register(
        method.value,
        [handler](const json& raw) -> json {
            P parsed;
            std::string error;
            if (!FromJson(raw, parsed, error)) {
                return ApiEnvelope::MakeError(error, ApiErrorCode::INVALID_PARAMS);
            }
            return handler(parsed).ToJson();
        },
        PathSecuritySpecsOf<P>());
}

// handler 还要知道是哪个页面发起的调用时用这个重载：以下划线开头的键不进声明，
// 生成的结构体里没有 _callerHwnd，由包装层拿原始参数建 CallerContext 一并交给 handler。
template <class P, class R>
void RegisterApi(std::type_identity_t<MethodName<P>> method, Result<R> (*handler)(const P&, const CallerContext&)) {
    static_assert(std::is_same_v<R, typename P::Result>, "handler returns the result type of another method");
    detail::Registrar::Register(
        method.value,
        [handler](const json& raw) -> json {
            P parsed;
            std::string error;
            if (!FromJson(raw, parsed, error)) {
                return ApiEnvelope::MakeError(error, ApiErrorCode::INVALID_PARAMS);
            }
            return handler(parsed, CallerContext::FromParams(raw)).ToJson();
        },
        PathSecuritySpecsOf<P>());
}

// deferred：handler 自己决定何时回包，用 Send 把 Result<R> 交给 responder。responder 按引用
// 交给 handler，要跨线程或延后回包就按值拷走（与 BridgeCore 的 deferred handler 同一约定）；不要
// move 走它：它内部是共享状态的指针，移走后留下的空对象再回包会在主线程解空指针。
// 路径规格与同步调用一样交给 BridgeCore，在主线程进入 handler 前校验。
template <class P>
void RegisterApiDeferred(std::type_identity_t<MethodName<P>> method,
                         void (*handler)(const P&, const DeferredResponder&)) {
    detail::Registrar::RegisterDeferred(
        method.value,
        [handler](const json& raw, DeferredResponder responder) {
            P parsed;
            std::string error;
            if (!FromJson(raw, parsed, error)) {
                responder.SendJson(ApiEnvelope::MakeError(error, ApiErrorCode::INVALID_PARAMS));
                return;
            }
            handler(parsed, responder);
        }, PathSecuritySpecsOf<P>());
}

template <class P>
void RegisterApiDeferred(std::type_identity_t<MethodName<P>> method,
                         void (*handler)(const P&, const CallerContext&, const DeferredResponder&)) {
    detail::Registrar::RegisterDeferred(
        method.value,
        [handler](const json& raw, DeferredResponder responder) {
            P parsed;
            std::string error;
            if (!FromJson(raw, parsed, error)) {
                responder.SendJson(ApiEnvelope::MakeError(error, ApiErrorCode::INVALID_PARAMS));
                return;
            }
            handler(parsed, CallerContext::FromParams(raw), responder);
        }, PathSecuritySpecsOf<P>());
}

// ToJson 在调用线程完成，json 按值交给 responder，可以从任何线程调用。
template <class R>
void Send(const DeferredResponder& responder, const Result<R>& result) {
    responder.SendJson(result.ToJson());
}

}  // namespace api
