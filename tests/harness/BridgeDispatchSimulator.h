// BridgeDispatchSimulator.h - Test double for BridgeCore dispatch logic
// Mirrors the real HandleMessage / SendResponse / SendError / EmitEvent contract
// without foobar2000 SDK, WebView2, or Win32 dependencies.
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class BridgeDispatchSimulator {
public:
    using ApiHandler = std::function<json(const json& params)>;

    // ---- Registration (mirrors BridgeCore::RegisterApi) ----
    void RegisterApi(const std::string& method, ApiHandler handler) {
        handlers_[method] = std::move(handler);
    }

    void UnregisterApi(const std::string& method) {
        handlers_.erase(method);
    }

    bool HasApi(const std::string& method) const {
        return handlers_.contains(method);
    }

    std::vector<std::string> GetRegisteredApiNames() const {
        std::vector<std::string> names;
        names.reserve(handlers_.size());
        for (const auto& [name, _] : handlers_) {
            names.push_back(name);
        }
        return names;
    }

    // ---- Message handling (mirrors BridgeCore::HandleMessage) ----
    // Takes UTF-8 JSON string directly (real BridgeCore takes wstring + WideToUtf8)
    void HandleMessage(const std::string& utf8Json) {
        try {
            json msg = json::parse(utf8Json);

            // Extract id (number or string)
            std::string id;
            if (msg.contains("id")) {
                if (msg["id"].is_number()) {
                    id = std::to_string(msg["id"].get<int>());
                } else if (msg["id"].is_string()) {
                    id = msg["id"].get<std::string>();
                }
            }

            // Extract and trim method
            std::string method = TrimAscii(msg.value("method", ""));
            json params = msg.value("params", json::object());

            if (method.empty()) {
                if (!id.empty()) {
                    SendError(id, -32600, "Invalid request: method is required",
                              "INVALID_REQUEST");
                }
                return;
            }

            // Find handler
            auto it = handlers_.find(method);
            if (it == handlers_.end()) {
                SendError(id, -32601, "Method not found: " + method,
                          "METHOD_NOT_FOUND", method);
                return;
            }

            // Execute handler (synchronously — real BridgeCore uses fb2k::inMainThread)
            try {
                json result = it->second(params);
                if (!id.empty()) {
                    SendResponse(id, result);
                }
            } catch (const std::exception& e) {
                if (!id.empty()) {
                    SendError(id, -1, e.what(), "INTERNAL_ERROR", method);
                }
            } catch (...) {
                if (!id.empty()) {
                    SendError(id, -1, "Unknown internal error", "INTERNAL_ERROR", method);
                }
            }

        } catch (const json::exception&) {
            // JSON parse error — silently dropped (mirrors real BridgeCore)
        }
    }

    // ---- Response/Error/Event formatting (mirrors BridgeCore) ----
    void SendResponse(const std::string& id, const json& result) {
        json response;
        response["type"] = "response";
        // Convert id back to number if possible
        try {
            response["id"] = std::stoi(id);
        } catch (...) {
            response["id"] = id;
        }
        response["result"] = result;
        sentMessages_.push_back(std::move(response));
    }

    void SendError(const std::string& id, int /*numericCode*/, const std::string& message,
                   const char* errorCode = "", const std::string& method = "") {
        json response;
        response["type"] = "response";
        try {
            response["id"] = std::stoi(id);
        } catch (...) {
            response["id"] = id;
        }
        response["error"] = message;
        if (errorCode && errorCode[0] != '\0') {
            response["code"] = errorCode;
        }
        sentMessages_.push_back(std::move(response));
    }

    void EmitEvent(const std::string& event, const json& data) {
        json message;
        message["type"] = "event";
        message["event"] = event;
        message["data"] = data;
        sentMessages_.push_back(std::move(message));
    }

    // ---- Test inspection ----
    const std::vector<json>& GetSentMessages() const { return sentMessages_; }
    size_t GetMessageCount() const { return sentMessages_.size(); }
    void ClearMessages() { sentMessages_.clear(); }

    const json& LastMessage() const {
        if (sentMessages_.empty()) {
            throw std::runtime_error("No messages sent");
        }
        return sentMessages_.back();
    }

private:
    static std::string TrimAscii(const std::string& in) {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
            ++start;
        }
        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
            --end;
        }
        return in.substr(start, end - start);
    }

    std::unordered_map<std::string, ApiHandler> handlers_;
    std::vector<json> sentMessages_;
};
