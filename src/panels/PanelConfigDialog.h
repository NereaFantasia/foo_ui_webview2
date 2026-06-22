/**
 * PanelConfigDialog - 面板配置对话框
 * 
 * CUI/DUI 面板共用的模态配置对话框。
 * 提供模板选择（含"跟随全局设置"选项）和当前资源路径显示。
 */

#pragma once
#include "pch.h"
#include "panels/PanelConfig.h"

/**
 * 显示面板配置对话框
 * 
 * @param parent 父窗口句柄
 * @param config [in/out] 当前配置，对话框关闭后包含新值
 * @return true 如果用户点击 OK 且配置发生了变化
 */
bool ShowPanelConfigDialog(HWND parent, PanelConfig& config);
