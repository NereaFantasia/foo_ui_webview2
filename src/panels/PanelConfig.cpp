/**
 * PanelConfig - 面板配置序列化/反序列化实现（v2）
 * 
 * v2 格式向后兼容 v1：
 * - 读取 v1 数据时，v2 字段使用默认值
 * - 写入始终使用 CURRENT_VERSION (2)
 */

#include "pch.h"
#include "panels/PanelConfig.h"

// ============================================
// 字节数组序列化（v2）
// ============================================

std::vector<uint8_t> PanelConfig::Serialize(const PanelConfig& config) {
    uint32_t version = CURRENT_VERSION;
    std::vector<uint8_t> data;
    
    // 预估大小（避免多次 realloc）
    size_t estSize = 4                                    // version
                   + 4 + config.templateName.size()       // v1: templateName
                   + 4 + config.panelName.size()          // v2: panelName
                   + 1 + 1                                // v2: edgeStyle + flags
                   + 4 + config.urlOverride.size();       // v2: urlOverride
    data.reserve(estSize);
    
    // 辅助写入函数
    auto writeU32 = [&](uint32_t v) {
        data.insert(data.end(), reinterpret_cast<uint8_t*>(&v),
                    reinterpret_cast<uint8_t*>(&v) + 4);
    };
    auto writeStr = [&](const std::string& s) {
        writeU32(static_cast<uint32_t>(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    };
    auto writeU8 = [&](uint8_t v) { 
        data.push_back(v); 
    };
    
    // 写入 version
    writeU32(version);
    
    // v1 字段：templateName
    writeStr(config.templateName);
    
    // v2 字段：panelName
    writeStr(config.panelName);
    
    // v2 字段：edgeStyle
    writeU8(config.edgeStyle);
    
    // v2 字段：flags（4个布尔压缩到1字节）
    uint8_t flags = 0;
    if (config.transparentBackground) flags |= 0x01;  // bit0
    if (config.grabFocus)             flags |= 0x02;  // bit1
    if (config.enableDragDrop)        flags |= 0x04;  // bit2
    if (config.enableDevTools)        flags |= 0x08;  // bit3
    writeU8(flags);
    
    // v2 字段：urlOverride
    writeStr(config.urlOverride);
    
    return data;
}

PanelConfig PanelConfig::Deserialize(const uint8_t* data, size_t size) {
    PanelConfig config;
    
    // 最小有效数据：version(4) + templateNameLen(4) = 8 字节
    if (!data || size < 8) {
        return config;
    }
    
    const uint8_t* ptr = data;
    const uint8_t* end = ptr + size;
    
    // 辅助读取函数（带边界检查）
    auto readU32 = [&](uint32_t& v) -> bool {
        if (ptr + 4 > end) return false;
        memcpy(&v, ptr, 4);
        ptr += 4;
        return true;
    };
    auto readStr = [&](std::string& s) -> bool {
        uint32_t len = 0;
        if (!readU32(len)) return false;
        if (ptr + len > end) return false;
        s.assign(reinterpret_cast<const char*>(ptr), len);
        ptr += len;
        return true;
    };
    auto readU8 = [&](uint8_t& v) -> bool {
        if (ptr + 1 > end) return false;
        v = *ptr++;
        return true;
    };
    
    // 读取 version
    uint32_t version = 0;
    if (!readU32(version)) return config;
    
    // 版本检查
    if (version == 0 || version > CURRENT_VERSION) {
        return config;  // 未知版本，返回默认配置
    }
    
    // v1 字段：templateName
    if (version >= 1 && !readStr(config.templateName)) return config;
    
    // v2 字段
    if (version >= 2) {
        // panelName
        if (!readStr(config.panelName)) return config;
        
        // edgeStyle
        if (!readU8(config.edgeStyle)) return config;
        
        // flags
        uint8_t flags = 0;
        if (!readU8(flags)) return config;
        config.transparentBackground = (flags & 0x01) != 0;
        config.grabFocus             = (flags & 0x02) != 0;
        config.enableDragDrop        = (flags & 0x04) != 0;
        config.enableDevTools        = (flags & 0x08) != 0;
        
        // urlOverride
        if (!readStr(config.urlOverride)) return config;
    }
    // 注意：v1 数据读取后，v2 字段保持构造函数默认值（transparentBackground=true 等）
    
    return config;
}

// ============================================
// stream_reader/writer 适配（CUI 用）
// ============================================

void PanelConfig::Write(stream_writer* writer, const PanelConfig& config, abort_callback& abort) {
    if (!writer) return;
    
    auto data = Serialize(config);
    writer->write(data.data(), data.size(), abort);
}

PanelConfig PanelConfig::Read(stream_reader* reader, t_size size, abort_callback& abort) {
    PanelConfig config;
    
    if (!reader || size == 0) {
        return config;
    }
    
    // 读取全部数据到缓冲区
    std::vector<uint8_t> buffer(size);
    t_size bytesRead = 0;
    
    try {
        // 使用 read 而非 read_object 以容错处理部分读取
        bytesRead = reader->read(buffer.data(), size, abort);
    } catch (...) {
        return config;
    }
    
    if (bytesRead > 0) {
        config = Deserialize(buffer.data(), bytesRead);
    }
    
    return config;
}
