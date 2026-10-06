#pragma once
#include "settings/AdvconfigI18n.h"

// Preferences → Advanced → Tools → WebView2 UI 下的四个复选叶子，定义在 settings/AdvancedConfig.cpp。
// 运行时读取走 security_config（settings/SecurityConfig.h）；偏好页只读显示时直接读这几个工厂。
extern advconfig_i18n::CheckboxFactory g_cfg_cdp_keepalive;
extern advconfig_i18n::CheckboxFactory g_cfg_local_network;
extern advconfig_i18n::CheckboxFactory g_cfg_allow_insecure;
extern advconfig_i18n::CheckboxFactory g_cfg_allow_insecure_tls;
