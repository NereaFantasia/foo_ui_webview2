/**
 * PluginRegistry.cpp - external-plugin API registration system (implementation)
 */

#include "pch.h"
#include "api/PluginRegistry.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/ApiSchema.h"
#include "api/generated/PluginSchema.h"
#include "api/generated/SystemSchema.h"
#include "core/WebViewContext.h"

// ============================================
// 声明的共享形状：system.* 的结果与 plugin:* / api:* 事件都用它们
// ============================================

namespace {
    api::common::SystemApiInfo ToDeclared(const ApiInfo& info) {
        api::common::SystemApiInfo out;
        out.fullName = info.fullName;
        out.plugin = info.pluginName;
        out.namespace_ = info.pluginNamespace;
        out.method = info.methodName;
        out.description = info.description;
        out.version = info.version;
        out.isExternal = info.isExternal;
        return out;
    }

    api::common::SystemPluginInfo ToDeclared(const PluginInfo& plugin) {
        api::common::SystemPluginInfo out;
        out.name = plugin.name;
        out.namespace_ = plugin.pluginNamespace;
        out.version = plugin.version;
        out.author = plugin.author;
        out.description = plugin.description;
        out.apiCount = static_cast<int64_t>(plugin.apis.size());
        out.apis = plugin.apis;
        return out;
    }

    std::vector<api::common::SystemApiInfo> ToDeclared(const std::vector<ApiInfo>& infos) {
        std::vector<api::common::SystemApiInfo> out;
        out.reserve(infos.size());
        for (const auto& info : infos) out.push_back(ToDeclared(info));
        return out;
    }
} // anonymous namespace

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
    // unsigned char 转型：外部插件传入非 ASCII 字节时 isalnum(负值) 是 UB（Debug CRT 断言）
    for (char c : pluginNamespace) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
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
    api::emit::Broadcast<api::plugin::events::Registered>(ToDeclared(info));
    
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
    api::emit::Broadcast<api::plugin::events::Unregistered>(ToDeclared(info));
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
    BridgeCore::GetInstance().RegisterUndeclaredApi(fullName, [this, fullName](const json& params) -> json {
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
    api::emit::Broadcast<api::api_::events::Registered>(ToDeclared(apiInfo));
    
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
    api::emit::Broadcast<api::api_::events::Unregistered>(ToDeclared(apiInfo));
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
// system.* handlers：形状由 src/api/schema/system.ts 声明
// ============================================

namespace {
    namespace sys = api::system;

    api::Result<sys::ListAvailableApisResult> SystemListAvailableApis(const sys::ListAvailableApisParams& p) {
        sys::ListAvailableApisResult result;
        result.apis = ToDeclared(PluginRegistry::GetInstance().ListAvailableApis(p.includeInternal, p.includeExternal));
        return result;
    }

    api::Result<sys::GetApisByNamespaceResult> SystemGetApisByNamespace(const sys::GetApisByNamespaceParams& p) {
        sys::GetApisByNamespaceResult result;
        result.apis = ToDeclared(PluginRegistry::GetInstance().GetApisByNamespace(p.namespace_));
        return result;
    }

    api::Result<sys::SearchApisResult> SystemSearchApis(const sys::SearchApisParams& p) {
        sys::SearchApisResult result;
        result.apis = ToDeclared(PluginRegistry::GetInstance().SearchApis(p.query));
        return result;
    }

    api::Result<sys::GetApiStatsResult> SystemGetApiStats(const sys::GetApiStatsParams&) {
        // GetApiStats 仍产出 json（也供事件与日志用），这里只搬到声明的形状。
        const json stats = PluginRegistry::GetInstance().GetApiStats();
        sys::GetApiStatsResult result;
        result.totalApis = stats.at("totalApis").get<int64_t>();
        result.internalApis = stats.at("internalApis").get<int64_t>();
        result.externalApis = stats.at("externalApis").get<int64_t>();
        result.pluginCount = stats.at("pluginCount").get<int64_t>();
        for (const auto& [ns, count] : stats.at("byNamespace").items()) {
            result.byNamespace[ns] = count.get<int64_t>();
        }
        return result;
    }

    api::Result<sys::GetRegisteredPluginsResult> SystemGetRegisteredPlugins(const sys::GetRegisteredPluginsParams&) {
        sys::GetRegisteredPluginsResult result;
        for (const auto& plugin : PluginRegistry::GetInstance().GetRegisteredPlugins()) {
            result.plugins.push_back(ToDeclared(plugin));
        }
        return result;
    }

    api::Result<sys::IsPluginRegisteredResult> SystemIsPluginRegistered(const sys::IsPluginRegisteredParams& p) {
        sys::IsPluginRegisteredResult result;
        result.registered = PluginRegistry::GetInstance().IsPluginRegistered(p.namespace_);
        return result;
    }
} // anonymous namespace

// ============================================
// 初始化
// ============================================

void PluginRegistry::Initialize() {
    RegisterDiscoveryApis();
    console::print("[PluginRegistry] Initialized");
}

void PluginRegistry::RegisterDiscoveryApis() {
    // handler 都是单例上的自由函数，见文件上方的匿名命名空间。
    api::RegisterApi("system.listAvailableApis", SystemListAvailableApis);
    RegisterInternalApi("system.listAvailableApis", "List all available APIs");

    api::RegisterApi("system.getApisByNamespace", SystemGetApisByNamespace);
    RegisterInternalApi("system.getApisByNamespace", "Get APIs by namespace");

    api::RegisterApi("system.searchApis", SystemSearchApis);
    RegisterInternalApi("system.searchApis", "Search APIs by name or description");

    api::RegisterApi("system.getApiStats", SystemGetApiStats);
    RegisterInternalApi("system.getApiStats", "Get API statistics");

    api::RegisterApi("system.getRegisteredPlugins", SystemGetRegisteredPlugins);
    RegisterInternalApi("system.getRegisteredPlugins", "Get list of registered external plugins");

    api::RegisterApi("system.isPluginRegistered", SystemIsPluginRegistered);
    RegisterInternalApi("system.isPluginRegistered", "Check if a plugin is registered");

    console::print("[PluginRegistry] Discovery APIs registered");
}
