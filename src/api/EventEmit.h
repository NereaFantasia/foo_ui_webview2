#pragma once
// 按声明发事件。事件名与载荷结构体都取自 src/api/schema/<ns>.ts 生成的描述
// api::<ns>::events::<Event>：发射处不写事件名，载荷用错结构体编译不过。
// scripts/api-schema/emits.mjs 另外检查已声明事件的名字不再以字面量出现在 src/ 里。
//
// 每个助手是一种投递方式，与声明里的 @delivery 相对应：
//   Broadcast        所有窗口（broadcast）
//   BroadcastExcept  除某个窗口外的所有窗口
//   Emit             一个页面的 bridge（window：事件所涉及的窗口或面板）
//   EmitTo           发起调用的页面（caller）
//   SendTo           按 windowId 指定的窗口（owner / target），返回是否送达
//   ToCaller         发起调用的窗口，只持有它的 windowId 与句柄时用（caller）：先按 windowId，
//                    再经 CallerContext::FromHwnd 按句柄解析，最后落到主窗口页面
//   Post             直接交给一个 WebViewHost，信封与 BridgeCore::EmitEvent 相同
// 载荷大到不该在主线程转 JSON 时，在工作线程构造 Prepared<E>，ToCaller 接受它。
// 声明带 @customName 的事件另有 SendToNamed / EmitNamed，以订阅方起的名字送达。
// 拖放的 dnd:* 事件不经这里：DropTargetBridge 手里只有本窗口的 sink，用它自己的
// Emit<E>，同样只收生成的载荷，名字取自描述。
#include "pch.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "core/WebViewContext.h"

#include <string>

namespace api::emit {

// 在构造它的线程上把 E 的载荷转成 JSON。只能由 E::Payload 构造，所以名字与形状仍由声明约束。
template <class E>
class Prepared {
public:
    explicit Prepared(const typename E::Payload& payload) : data_(ToJson(payload)) {}
    const nlohmann::json& Data() const { return data_; }

private:
    nlohmann::json data_;
};

namespace detail {

template <class E>
void ToCallerData(const std::string& windowId, HWND hwnd, const nlohmann::json& data) {
    if (!windowId.empty() && WebViewContext::GetInstance().SendEventTo(windowId, E::kName, data)) return;
    CallerContext::FromHwnd(hwnd).EmitEvent(E::kName, data);
}

}  // namespace detail

template <class E>
void Broadcast(const typename E::Payload& payload) {
    WebViewContext::GetInstance().BroadcastEvent(E::kName, ToJson(payload));
}

template <class E>
void BroadcastExcept(const typename E::Payload& payload, HWND excludeHwnd) {
    WebViewContext::GetInstance().BroadcastEventExcept(E::kName, ToJson(payload), excludeHwnd);
}

template <class E>
void Emit(BridgeCore& bridge, const typename E::Payload& payload) {
    bridge.EmitEvent(E::kName, ToJson(payload));
}

template <class E>
void EmitTo(const CallerContext& caller, const typename E::Payload& payload) {
    caller.EmitEvent(E::kName, ToJson(payload));
}

template <class E>
bool SendTo(const std::string& windowId, const typename E::Payload& payload) {
    return WebViewContext::GetInstance().SendEventTo(windowId, E::kName, ToJson(payload));
}

// 在主线程调用：事件最终交给 WebView2，它绑定 UI 线程。
template <class E>
void ToCaller(const std::string& windowId, HWND hwnd, const typename E::Payload& payload) {
    detail::ToCallerData<E>(windowId, hwnd, ToJson(payload));
}

template <class E>
void ToCaller(const std::string& windowId, HWND hwnd, const Prepared<E>& payload) {
    detail::ToCallerData<E>(windowId, hwnd, payload.Data());
}

// ponytail: 只有 ToCaller 接受 Prepared；别的投递方式遇到大载荷时再照此加重载。

// Host 只需要 PostEventMessage(const std::string&, const std::wstring&)；这里不包含
// WebViewHost.h，免得每个发事件的翻译单元都带上它。
template <class E, class Host>
void Post(Host& host, const typename E::Payload& payload) {
    const nlohmann::json message{{"type", "event"}, {"event", E::kName}, {"data", ToJson(payload)}};
    host.PostEventMessage(E::kName, std::wstring(pfc::stringcvt::string_wide_from_utf8(message.dump().c_str()).get_ptr()));
}

template <class E>
bool SendToNamed(const std::string& windowId, const std::string& name, const typename E::Payload& payload) {
    static_assert(E::kCustomName, "only an event declared with @customName goes out under another name");
    return WebViewContext::GetInstance().SendEventTo(windowId, name, ToJson(payload));
}

template <class E>
void EmitNamed(BridgeCore& bridge, const std::string& name, const typename E::Payload& payload) {
    static_assert(E::kCustomName, "only an event declared with @customName goes out under another name");
    bridge.EmitEvent(name, ToJson(payload));
}

}  // namespace api::emit
