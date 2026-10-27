/**
 * PanelConfig - 面板实例配置数据结构（v2）
 * 
 * 为 DUI/CUI 面板提供统一的配置序列化格式。
 * 支持面板级别的模板覆盖、边框样式、透明背景、焦点策略等配置。
 * 
 * 二进制格式 v2（向后兼容 v1）：
 * [version:4B LE]
 * if version >= 1:
 *   [templateNameLen:4B][templateName:NB UTF-8]
 * if version >= 2:
 *   [panelNameLen:4B][panelName:NB UTF-8]
 *   [edgeStyle:1B]
 *   [flags:1B]  — bit0=transparentBackground, bit1=grabFocus,
 *                  bit2=enableDragDrop, bit3=enableDevTools
 *   [urlOverrideLen:4B][urlOverride:NB UTF-8]
 *   [approvedUrlOriginLen:4B][approvedUrlOrigin:NB UTF-8]  — 末尾追加，版本号仍为 2
 *
 * 追加字段不升版本号：旧版遇到更高版本会丢弃整份配置，而末尾多出的字节它不读。
 * 数据止于 urlOverride 时是旧版保存的，按已允许其来源迁移，见 Deserialize。
 */

#pragma once
#include "pch.h"

struct PanelConfig {
    // 配置版本号（用于向前兼容）
    static constexpr uint32_t CURRENT_VERSION = 2;
    
    // ========== v1 字段 ==========
    
    // 面板使用的模板名，空字符串表示跟随全局设置
    std::string templateName;
    
    // ========== v2 新增字段 ==========
    
    // 面板自定义名称（空=默认 "WebView2 Panel"）
    // 用于多面板环境区分标识，日志输出，前端 API 可读取
    std::string panelName;
    
    // 边框样式: 0=None, 1=Sunken(WS_EX_CLIENTEDGE), 2=Grey(WS_EX_STATICEDGE)
    uint8_t edgeStyle = 0;
    
    // WebView 背景是否透明（默认 true）
    bool transparentBackground = true;
    
    // 是否接受键盘焦点（默认 true）
    // 禁用后面板不干扰 fb2k 全局快捷键和 Selection API 的焦点追踪
    bool grabFocus = true;
    
    // 是否允许外部文件拖放到面板（默认 true）
    bool enableDragDrop = true;
    
    // 面板级 DevTools 开关（默认 false）
    // 逻辑：全局 advconfig 启用 OR 面板级启用 → DevTools 可用
    bool enableDevTools = false;
    
    // 直接 URL 覆盖（空=使用模板）
    // 非空时跳过模板查找逻辑，直接导航到此 URL
    std::string urlOverride;

    // 用户允许调用接口的远程来源，按 origin_policy::OriginOf 规范化；空=没有允许。
    // 只有等于 urlOverride 的来源时才生效，本机地址不需要允许。
    std::string approvedUrlOrigin;

    // ========== 构造函数 ==========
    
    PanelConfig() = default;
    explicit PanelConfig(const std::string& name) : templateName(name) {}
    
    // ========== 配置比较（热重载检测用）==========
    
    bool operator==(const PanelConfig& other) const {
        return templateName == other.templateName &&
               panelName == other.panelName &&
               edgeStyle == other.edgeStyle &&
               transparentBackground == other.transparentBackground &&
               grabFocus == other.grabFocus &&
               enableDragDrop == other.enableDragDrop &&
               enableDevTools == other.enableDevTools &&
               urlOverride == other.urlOverride &&
               approvedUrlOrigin == other.approvedUrlOrigin;
    }
    
    bool operator!=(const PanelConfig& other) const {
        return !(*this == other);
    }
    
    // 检测配置是否有变化（ApplyConfig 决策用）
    bool HasChanged(const PanelConfig& other) const {
        return *this != other;
    }
    
    // ========== 辅助方法 ==========
    
    // 是否使用面板级别的模板覆盖（非空即覆盖）
    bool HasTemplateOverride() const { return !templateName.empty(); }
    
    // 是否使用 URL 覆盖（非空即覆盖）
    bool HasUrlOverride() const { return !urlOverride.empty(); }

    // urlOverride 的来源；取不出 http(s) 来源时为空。
    std::string UrlOrigin() const;

    // 加载 urlOverride 时应登记为可信的来源；为空时页面只显示，不能调用接口。
    std::wstring TrustedUrlOrigin() const;
    
    // ========== 序列化 ==========
    
    /**
     * 序列化到字节数组
     * 格式 v2：见文件头注释
     */
    static std::vector<uint8_t> Serialize(const PanelConfig& config);
    
    /**
     * 从字节数组反序列化
     * @param data 数据指针
     * @param size 数据大小
     * @return 反序列化后的配置，失败则返回默认配置
     * @note 支持 v1 数据，v2 字段使用默认值
     */
    static PanelConfig Deserialize(const uint8_t* data, size_t size);
    
    // ========== stream_reader/writer 适配（CUI 用）==========
    
    /**
     * 写入到 stream_writer（CUI get_config 用）
     */
    static void Write(stream_writer* writer, const PanelConfig& config, abort_callback& abort);
    
    /**
     * 从 stream_reader 读取（CUI set_config 用）
     */
    static PanelConfig Read(stream_reader* reader, t_size size, abort_callback& abort);
};

// 填 panel.getConfig 的 config 或 panel:configChanged 的载荷。两者由同一份声明生成、字段相同，
// 方法与事件共用这一处，才不会对同一个面板说出两样话。
template <class Out>
void FillPanelConfig(const PanelConfig& cfg, Out& out) {
    out.panelName = cfg.panelName;
    out.templateName = cfg.templateName;
    out.edgeStyle = cfg.edgeStyle;
    out.urlOverride = cfg.urlOverride;
    out.transparentBackground = cfg.transparentBackground;
    out.grabFocus = cfg.grabFocus;
    out.enableDragDrop = cfg.enableDragDrop;
    out.enableDevTools = cfg.enableDevTools;
}
