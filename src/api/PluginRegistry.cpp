/**
 * PluginRegistry.cpp - external-plugin API registration system (implementation)
 */

#include "pch.h"
#include "api/PluginRegistry.h"
#include "api/BridgeCore.h"
#include "core/WebViewContext.h"

// ============================================
// 保留命名空间（内部使用）
// ============================================

const std::unordered_set<std::string> PluginRegistry::RESERVED_NAMESPACES = {
    "playback",
    "playlist",
    "library",
    "artwork",
    "config",
    "window",
    "ui",
    "test",
    "system",      // 预留
    "bridge",      // 预留
    "internal"     // 预留
};

// ============================================
// 单例获取
// ============================================

PluginRegistry& PluginRegistry::GetInstance() {
    static PluginRegistry instance;
    return instance;
}

// 导出函数供外部插件调用
extern "C" WEBVIEW_API PluginRegistry& GetPluginRegistry() {
    return PluginRegistry::GetInstance();
}

// ============================================
// 插件管理
// ============================================

bool PluginRegistry::RegisterPlugin(
    const std::string& pluginNamespace,
    const std::string& name,
    const std::string& version,
    const std::string& author,
    const std::string& description
) {
    // 检查是否为保留命名空间
    if (RESERVED_NAMESPACES.count(pluginNamespace)) {
        console::printf("[PluginRegistry] Error: Namespace '%s' is reserved", pluginNamespace.c_str());
        return false;
    }
    
    // 验证命名空间格式（只允许小写字母、数字、下划线）
    for (char c : pluginNamespace) {
        if (!std::isalnum(c) && c != '_') {
            console::printf("[PluginRegistry] Error: Invalid namespace format '%s'", pluginNamespace.c_str());
            return false;
        }
    }
    
    std::lock_guard lock(mutex_);
    
    // 检查是否已注册
    if (plugins_.count(pluginNamespace)) {
        console::printf("[PluginRegistry] Plugin '%s' already registered, updating...", pluginNamespace.c_str());
        // 更新信息但保留已注册的 API
        auto& existing = plugins_[pluginNamespace];
        existing.name = name;
        existing.version = version;
        existing.author = author;
        existing.description = description;
        return true;
    }
    
    // 创建插件信息
    PluginInfo info;
    info.name = name;
    info.pluginNamespace = pluginNamespace;
    info.version = version;
    info.author = author;
    info.description = description;
    
    plugins_[pluginNamespace] = info;
    
    console::printf("[PluginRegistry] Plugin registered: %s (%s) v%s", 
        name.c_str(), pluginNamespace.c_str(), version.c_str());
    
    // 发送插件注册事件
    EmitPluginEvent("plugin:registered", info);
    
    return true;
}

void PluginRegistry::UnregisterPlugin(const std::string& pluginNamespace) {
    std::lock_guard lock(mutex_);
    
    auto it = plugins_.find(pluginNamespace);
    if (it == plugins_.end()) {
        return;
    }
    
    PluginInfo info = it->second;
    
    // 移除该插件的所有 API
    std::string prefix = pluginNamespace + ".";
    for (auto apiIt = externalHandlers_.begin(); apiIt != externalHandlers_.end(); ) {
        if (apiIt->first.rfind(prefix, 0) == 0) {
            // 从 BridgeCore 注销
            BridgeCore::GetInstance().UnregisterApi(apiIt->first);
            
            // 移除 API 信息
            apiInfos_.erase(apiIt->first);
            
            apiIt = externalHandlers_.erase(apiIt);
        } else {
            ++apiIt;
        }
    }
    
    plugins_.erase(it);
    
    console::printf("[PluginRegistry] Plugin unregistered: %s", pluginNamespace.c_str());
    
    // 发送插件注销事件
    EmitPluginEvent("plugin:unregistered", info);
}

std::vector<PluginInfo> PluginRegistry::GetRegisteredPlugins() const {
    std::lock_guard lock(mutex_);
    
    std::vector<PluginInfo> result;
    result.reserve(plugins_.size());
    
    for (const auto& [ns, info] : plugins_) {
        result.push_back(info);
    }
    
    return result;
}

bool PluginRegistry::IsPluginRegistered(const std::string& pluginNamespace) const {
    std::lock_guard lock(mutex_);
    return plugins_.count(pluginNamespace) > 0;
}

// ============================================
// API 管理
// ============================================

bool PluginRegistry::RegisterExternalApi(
    const std::string& pluginNamespace,
    const std::string& methodName,
    ExternalApiHandler handler,
    const std::string& description,
    const std::string& version
) {
    std::lock_guard lock(mutex_);
    
    // 检查插件是否已注册
    auto pluginIt = plugins_.find(pluginNamespace);
    if (pluginIt == plugins_.end()) {
        console::printf("[PluginRegistry] Error: Plugin '%s' not registered", pluginNamespace.c_str());
        return false;
    }
    
    // 构建完整 API 名称
    std::string fullName = pluginNamespace + "." + methodName;
    
    // Prevent overriding built-in APIs
    if (BridgeCore::GetInstance().HasApi(fullName) && !externalHandlers_.count(fullName)) {
        console::printf("[PluginRegistry] Error: '%s' conflicts with built-in API", fullName.c_str());
        return false;
    }
    
    // 检查是否已存在（外部插件覆盖自己的 API 是允许的）
    if (externalHandlers_.count(fullName)) {
        console::printf("[PluginRegistry] Warning: API '%s' already exists, replacing...", fullName.c_str());
    }
    
    // 存储处理器
    externalHandlers_[fullName] = handler;
    
    // 创建 API 信息
    ApiInfo apiInfo;
    apiInfo.fullName = fullName;
    apiInfo.pluginName = pluginIt->second.name;
    apiInfo.pluginNamespace = pluginNamespace;
    apiInfo.methodName = methodName;
    apiInfo.description = description;
    apiInfo.version = version;
    apiInfo.isExternal = true;
    
    apiInfos_[fullName] = apiInfo;
    
    // 更新插件的 API 列表
    auto& apis = pluginIt->second.apis;
    if (std::find(apis.begin(), apis.end(), fullName) == apis.end()) {
        apis.push_back(fullName);
    }
    
    // 注册到 BridgeCore（桥接转发）
    BridgeCore::GetInstance().RegisterApi(fullName, [this, fullName](const json& params) -> json {
        std::lock_guard innerLock(mutex_);
        
        auto it = externalHandlers_.find(fullName);
        if (it != externalHandlers_.end()) {
            try {
                return it->second(params);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::string("External API error: ") + e.what());
            }
        }
        
        throw std::runtime_error("External API not found: " + fullName);
    });
    
    console::printf("[PluginRegistry] API registered: %s", fullName.c_str());
    
    // 发送 API 注册事件
    EmitApiEvent("api:registered", apiInfo);
    
    return true;
}

void PluginRegistry::UnregisterExternalApi(
    const std::string& pluginNamespace,
    const std::string& methodName
) {
    std::lock_guard lock(mutex_);
    
    std::string fullName = pluginNamespace + "." + methodName;
    
    auto it = externalHandlers_.find(fullName);
    if (it == externalHandlers_.end()) {
        return;
    }
    
    // 获取 API 信息用于事件
    ApiInfo apiInfo;
    auto infoIt = apiInfos_.find(fullName);
    if (infoIt != apiInfos_.end()) {
        apiInfo = infoIt->second;
    }
    
    // 从 BridgeCore 注销
    BridgeCore::GetInstance().UnregisterApi(fullName);
    
    // 移除处理器和信息
    externalHandlers_.erase(it);
    apiInfos_.erase(fullName);
    
    // 从插件的 API 列表中移除
    auto pluginIt = plugins_.find(pluginNamespace);
    if (pluginIt != plugins_.end()) {
        auto& apis = pluginIt->second.apis;
        apis.erase(std::remove(apis.begin(), apis.end(), fullName), apis.end());
    }
    
    console::printf("[PluginRegistry] API unregistered: %s", fullName.c_str());
    
    // 发送 API 注销事件
    EmitApiEvent("api:unregistered", apiInfo);
}

void PluginRegistry::RegisterInternalApi(
    const std::string& fullName,
    const std::string& description
) {
    std::lock_guard lock(mutex_);
    
    // 解析命名空间和方法名
    size_t dotPos = fullName.find('.');
    std::string ns = (dotPos != std::string::npos) ? fullName.substr(0, dotPos) : "";
    std::string method = (dotPos != std::string::npos) ? fullName.substr(dotPos + 1) : fullName;
    
    ApiInfo apiInfo;
    apiInfo.fullName = fullName;
    apiInfo.pluginName = "foo_ui_webview2";
    apiInfo.pluginNamespace = ns;
    apiInfo.methodName = method;
    apiInfo.description = description;
    apiInfo.version = "1.0.0";
    apiInfo.isExternal = false;
    
    apiInfos_[fullName] = apiInfo;
}

// ============================================
// API 发现
// ============================================

std::vector<ApiInfo> PluginRegistry::ListAvailableApis(
    bool includeInternal,
    bool includeExternal
) const {
    std::lock_guard lock(mutex_);
    
    std::vector<ApiInfo> result;
    
    // 首先从 BridgeCore 获取所有已注册的 API
    auto bridgeApis = BridgeCore::GetInstance().GetRegisteredApiNames();
    
    for (const auto& apiName : bridgeApis) {
        // 检查是否在 apiInfos_ 中有详细信息
        auto it = apiInfos_.find(apiName);
        
        if (it != apiInfos_.end()) {
            // 使用已有的详细信息
            const auto& info = it->second;
            if ((includeInternal && !info.isExternal) || 
                (includeExternal && info.isExternal)) {
                result.push_back(info);
            }
        } else {
            // 为未注册到 apiInfos_ 的 API 创建基本信息
            if (includeInternal) {
                ApiInfo info;
                info.fullName = apiName;
                info.pluginName = "foo_ui_webview2";
                info.isExternal = false;
                
                // 解析命名空间和方法名
                size_t dotPos = apiName.find('.');
                if (dotPos != std::string::npos) {
                    info.pluginNamespace = apiName.substr(0, dotPos);
                    info.methodName = apiName.substr(dotPos + 1);
                } else {
                    info.pluginNamespace = "";
                    info.methodName = apiName;
                }
                
                info.description = "";
                info.version = "1.0.0";
                
                result.push_back(info);
            }
        }
    }
    
    // 按名称排序
    std::sort(result.begin(), result.end(), [](const ApiInfo& a, const ApiInfo& b) {
        return a.fullName < b.fullName;
    });
    
    return result;
}

std::vector<ApiInfo> PluginRegistry::GetApisByNamespace(const std::string& pluginNamespace) const {
    // 获取所有 API，然后按命名空间筛选
    auto allApis = ListAvailableApis(true, true);
    
    std::vector<ApiInfo> result;
    for (const auto& api : allApis) {
        if (api.pluginNamespace == pluginNamespace) {
            result.push_back(api);
        }
    }
    
    return result;  // ListAvailableApis 已经排序
}

std::vector<ApiInfo> PluginRegistry::SearchApis(const std::string& query) const {
    // 获取所有 API，然后搜索
    auto allApis = ListAvailableApis(true, true);
    
    std::vector<ApiInfo> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (const auto& info : allApis) {
        std::string lowerName = info.fullName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        std::string lowerDesc = info.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
        
        if (lowerName.find(lowerQuery) != std::string::npos ||
            lowerDesc.find(lowerQuery) != std::string::npos) {
            result.push_back(info);
        }
    }
    
    return result;  // ListAvailableApis 已经排序
}

json PluginRegistry::GetApiStats() const {
    // 获取所有 API 来计算统计
    auto allApis = ListAvailableApis(true, true);
    
    int internalCount = 0;
    int externalCount = 0;
    std::unordered_map<std::string, int> byNamespace;
    
    for (const auto& info : allApis) {
        if (info.isExternal) {
            externalCount++;
        } else {
            internalCount++;
        }
        byNamespace[info.pluginNamespace]++;
    }
    
    json namespaceStats = json::object();
    for (const auto& [ns, count] : byNamespace) {
        namespaceStats[ns] = count;
    }
    
    std::lock_guard lock(mutex_);  // 只锁定获取 plugins_ 大小
    
    return {
        {"totalApis", (int)allApis.size()},
        {"internalApis", internalCount},
        {"externalApis", externalCount},
        {"pluginCount", (int)plugins_.size()},
        {"byNamespace", namespaceStats}
    };
}

// ============================================
// 初始化
// ============================================

void PluginRegistry::Initialize() {
    RegisterDiscoveryApis();
    console::print("[PluginRegistry] Initialized");
}

void PluginRegistry::RegisterDiscoveryApis() {
    auto& bridge = BridgeCore::GetInstance();
    
    // system.listAvailableApis - 列出所有可用 API
    bridge.RegisterApi("system.listAvailableApis", [this](const json& params) -> json {
        bool includeInternal = params.value("includeInternal", true);
        bool includeExternal = params.value("includeExternal", true);
        
        auto apis = ListAvailableApis(includeInternal, includeExternal);
        
        json result = json::array();
        for (const auto& api : apis) {
            result.push_back(api.toJson());
        }
        
        return result;
    });
    RegisterInternalApi("system.listAvailableApis", "List all available APIs");
    
    // system.getApisByNamespace - 获取指定命名空间的 API
    bridge.RegisterApi("system.getApisByNamespace", [this](const json& params) -> json {
        std::string ns = params.value("namespace", "");
        if (ns.empty()) {
            throw std::runtime_error("namespace is required");
        }
        
        auto apis = GetApisByNamespace(ns);
        
        json result = json::array();
        for (const auto& api : apis) {
            result.push_back(api.toJson());
        }
        
        return result;
    });
    RegisterInternalApi("system.getApisByNamespace", "Get APIs by namespace");
    
    // system.searchApis - 搜索 API
    bridge.RegisterApi("system.searchApis", [this](const json& params) -> json {
        std::string query = params.value("query", "");
        if (query.empty()) {
            throw std::runtime_error("query is required");
        }
        
        auto apis = SearchApis(query);
        
        json result = json::array();
        for (const auto& api : apis) {
            result.push_back(api.toJson());
        }
        
        return result;
    });
    RegisterInternalApi("system.searchApis", "Search APIs by name or description");
    
    // system.getApiStats - 获取 API 统计
    bridge.RegisterApi("system.getApiStats", [this](const json& /*params*/) -> json {
        return GetApiStats();
    });
    RegisterInternalApi("system.getApiStats", "Get API statistics");
    
    // system.getRegisteredPlugins - 获取已注册的插件列表
    bridge.RegisterApi("system.getRegisteredPlugins", [this](const json& /*params*/) -> json {
        auto plugins = GetRegisteredPlugins();
        
        json result = json::array();
        for (const auto& plugin : plugins) {
            result.push_back(plugin.toJson());
        }
        
        return result;
    });
    RegisterInternalApi("system.getRegisteredPlugins", "Get list of registered external plugins");
    
    // system.isPluginRegistered - 检查插件是否已注册
    bridge.RegisterApi("system.isPluginRegistered", [this](const json& params) -> json {
        std::string ns = params.value("namespace", "");
        if (ns.empty()) {
            throw std::runtime_error("namespace is required");
        }
        
        return {{"success", true}, {"registered", IsPluginRegistered(ns)}};
    });
    RegisterInternalApi("system.isPluginRegistered", "Check if a plugin is registered");
    
    console::print("[PluginRegistry] Discovery APIs registered");
}

// ============================================
// 事件发送
// ============================================

void PluginRegistry::EmitPluginEvent(const std::string& eventType, const PluginInfo& plugin) {
    // Plugin lifecycle events are system-wide notifications — broadcast to all instances
    WebViewContext::GetInstance().BroadcastEvent(eventType, plugin.toJson());
}

void PluginRegistry::EmitApiEvent(const std::string& eventType, const ApiInfo& api) {
    // API lifecycle events are system-wide notifications — broadcast to all instances
    WebViewContext::GetInstance().BroadcastEvent(eventType, api.toJson());
}
