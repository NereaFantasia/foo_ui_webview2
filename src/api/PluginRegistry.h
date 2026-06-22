/**
 * PluginRegistry.h - external-plugin API registration system
 * 
 * Lets other foobar2000 components register their APIs with foo_ui_webview2,
 * with namespace isolation, API discovery and dynamic-load notifications.
 */

#pragma once
#include "pch.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <string>

using json = nlohmann::json;

// ============================================
// External-plugin API handler signature
// ============================================

/**
 * @brief Handler type implemented by external plugins for a registered API.
 *
 * @param params JSON-encoded request parameters
 * @return JSON-encoded response
 */
using ExternalApiHandler = std::function<json(const json& params)>;

// ============================================
// API metadata
// ============================================

/**
 * @brief Descriptor of a single registered API entry.
 */
struct ApiInfo {
    std::string fullName;       // full name: "namespace.method"
    std::string pluginName;     // owning plugin display name
    std::string pluginNamespace; // plugin namespace
    std::string methodName;     // method name (no namespace prefix)
    std::string description;    // API description
    std::string version;        // API version
    bool isExternal;            // registered by an external plugin
    
    json toJson() const {
        return {
            {"fullName", fullName},
            {"plugin", pluginName},
            {"namespace", pluginNamespace},
            {"method", methodName},
            {"description", description},
            {"version", version},
            {"isExternal", isExternal}
        };
    }
};

/**
 * @brief Descriptor of a registered external plugin.
 */
struct PluginInfo {
    std::string name;           // plugin display name
    std::string pluginNamespace; // namespace (unique identifier)
    std::string version;        // plugin version
    std::string author;         // author
    std::string description;    // description
    std::vector<std::string> apis; // registered API names
    
    json toJson() const {
        json j = {
            {"name", name},
            {"namespace", pluginNamespace},
            {"version", version},
            {"author", author},
            {"description", description},
            {"apiCount", (int)apis.size()},
            {"apis", apis}
        };
        return j;
    }
};

// ============================================
// PluginRegistry - plugin registration hub
// ============================================

/**
 * @brief Registration hub for external-plugin APIs.
 *
 * Manages API registration from other foobar2000 components, providing:
 * - namespace isolation
 * - API discovery
 * - dynamic-load notifications
 *
 * Usage example (inside an external plugin):
 * @code
 * #include "PluginRegistry.h"
 *
 * void RegisterMyApis() {
 *     auto& registry = PluginRegistry::GetInstance();
 *
 *     registry.RegisterPlugin("my_plugin", "my_plugin", "1.0.0", "Author", "Description");
 *
 *     registry.RegisterExternalApi("my_plugin", "doSomething",
 *         [](const json& params) -> json {
 *             return {{"result", "done"}};
 *         },
 *         "Do something useful"
 *     );
 * }
 * @endcode
 */
class PluginRegistry {
public:
    static PluginRegistry& GetInstance();
    
    // Non-copyable
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;
    
    // ========================================
    // Plugin management
    // ========================================
    
    /**
     * @brief Register an external plugin.
     *
     * @param pluginNamespace plugin namespace (unique identifier, used as the API prefix)
     * @param name plugin display name
     * @param version plugin version
     * @param author author
     * @param description description
     * @return true on success
     */
    bool RegisterPlugin(
        const std::string& pluginNamespace,
        const std::string& name,
        const std::string& version = "1.0.0",
        const std::string& author = "",
        const std::string& description = ""
    );
    
    /**
     * @brief Unregister a plugin together with all of its APIs.
     *
     * @param pluginNamespace plugin namespace
     */
    void UnregisterPlugin(const std::string& pluginNamespace);
    
    /**
     * @brief Return the list of registered plugins.
     */
    std::vector<PluginInfo> GetRegisteredPlugins() const;
    
    /**
     * @brief Check whether a plugin namespace is already registered.
     */
    bool IsPluginRegistered(const std::string& pluginNamespace) const;
    
    // ========================================
    // API management
    // ========================================
    
    /**
     * @brief Register an external API handler.
     *
     * @param pluginNamespace plugin namespace
     * @param methodName method name (without the namespace prefix)
     * @param handler handler function
     * @param description API description
     * @param version API version
     * @return true on success
     */
    bool RegisterExternalApi(
        const std::string& pluginNamespace,
        const std::string& methodName,
        ExternalApiHandler handler,
        const std::string& description = "",
        const std::string& version = "1.0.0"
    );
    
    /**
     * @brief Unregister an external API.
     *
     * @param pluginNamespace plugin namespace
     * @param methodName method name
     */
    void UnregisterExternalApi(
        const std::string& pluginNamespace,
        const std::string& methodName
    );
    
    /**
     * @brief Register an internal API (one of foo_ui_webview2's own) for discovery.
     *
     * @param fullName full API name (e.g. "playback.play")
     * @param description API description
     */
    void RegisterInternalApi(
        const std::string& fullName,
        const std::string& description = ""
    );
    
    // ========================================
    // API discovery
    // ========================================
    
    /**
     * @brief List every available API.
     *
     * @param includeInternal include internal APIs
     * @param includeExternal include external-plugin APIs
     * @return list of API descriptors
     */
    std::vector<ApiInfo> ListAvailableApis(
        bool includeInternal = true,
        bool includeExternal = true
    ) const;
    
    /**
     * @brief List the APIs registered under one namespace.
     */
    std::vector<ApiInfo> GetApisByNamespace(const std::string& pluginNamespace) const;
    
    /**
     * @brief Search APIs by keyword.
     *
     * @param query search keyword
     * @return matching API descriptors
     */
    std::vector<ApiInfo> SearchApis(const std::string& query) const;
    
    /**
     * @brief Return aggregate API statistics.
     */
    json GetApiStats() const;
    
    // ========================================
    // Initialization
    // ========================================
    
    /**
     * @brief Initialize the registry and register the discovery APIs with BridgeCore.
     */
    void Initialize();

private:
    PluginRegistry() = default;
    
    mutable std::mutex mutex_;
    
    // 已注册的插件信息
    std::unordered_map<std::string, PluginInfo> plugins_;
    
    // 外部 API 处理器: "namespace.method" -> handler
    std::unordered_map<std::string, ExternalApiHandler> externalHandlers_;
    
    // 所有 API 信息: "namespace.method" -> ApiInfo
    std::unordered_map<std::string, ApiInfo> apiInfos_;
    
    // 内置命名空间（不允许外部插件使用）
    static const std::unordered_set<std::string> RESERVED_NAMESPACES;
    
    // 发送插件注册/注销事件
    void EmitPluginEvent(const std::string& eventType, const PluginInfo& plugin);
    void EmitApiEvent(const std::string& eventType, const ApiInfo& api);
    
    // 注册发现 API
    void RegisterDiscoveryApis();
};

// ============================================
// Export macro (for external plugins)
// ============================================

#ifdef FOO_UI_WEBVIEW2_EXPORTS
    #define WEBVIEW_API __declspec(dllexport)
#else
    #define WEBVIEW_API __declspec(dllimport)
#endif

/**
 * @brief Exported accessor returning the PluginRegistry singleton.
 *
 * External plugins call this to obtain the registration hub instance.
 */
extern "C" WEBVIEW_API PluginRegistry& GetPluginRegistry();
