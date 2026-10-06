#pragma once

// ============================================
// AdvconfigI18n.h - Advanced Preferences 条目的双语显示名
//
// foobar2000 SDK 的 advconfig_entry_checkbox_impl 把显示名存成私有常量，只能通过
// 派生类覆写 get_name() 才能按组件呈现语言返回中英文。这里的派生类只做这一件事：
// GUID、configStore 键（fb2k::advconfig_autoName(guid)）、默认值、排序优先级和
// get_preferences_flags() 全部沿用基类，已保存的配置值不受影响。
//
// 语言在 get_name() 被宿主调用时才判定。全局静态构造阶段只保存两份字面量指针，
// 此时 core_api 尚未就绪，i18n 只能回退到操作系统语言，提前求值会把这个回退值
// 固化进条目名。
//
// 需要重启才生效的条目通过 prefFlags 传 preferences_state::needs_restart，
// 由宿主统一提示，不再写进标题文本。
// ============================================

#include <foobar2000/SDK/foobar2000.h>
#include "utils/I18n.h"

namespace advconfig_i18n {

// 两份 UTF-8 字面量；必须指向静态存储期字符串，条目不拷贝它们。
struct BilingualName {
    const char* en;
    const char* zh;
};

class CheckboxEntry : public advconfig_entry_checkbox_impl {
public:
    CheckboxEntry(BilingualName name, const GUID& guid, const GUID& parent, double priority,
                  bool initialState, uint32_t prefFlags)
        : advconfig_entry_checkbox_impl(name.en, fb2k::advconfig_autoName(guid), guid, parent, priority,
                                        initialState, /*isRadio*/ false, prefFlags),
          name_(name) {}

    void get_name(pfc::string_base& out) override { out = i18n::TU(name_.en, name_.zh); }

private:
    const BilingualName name_;
};

// 与 SDK 的 advconfig_checkbox_factory 同形的 get()/set()，调用方无需区分两种工厂。
class CheckboxFactory : public service_factory_single_t<CheckboxEntry> {
public:
    CheckboxFactory(BilingualName name, const GUID& guid, const GUID& parent, double priority,
                    bool initialState, uint32_t prefFlags = 0)
        : service_factory_single_t<CheckboxEntry>(name, guid, parent, priority, initialState, prefFlags) {}

    bool get() const { return get_static_instance().get_state_(); }
    void set(bool value) { get_static_instance().set_state(value); }
};

} // namespace advconfig_i18n
