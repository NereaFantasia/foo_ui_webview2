#include "StdAfx.h"
#include "DarkMode.h"

bool fb2k::isDarkMode() {
	auto api = ui_config_manager::tryGet();
	return api && api->is_dark_mode();
}

DarkMode::param_t fb2k::darkParams() {
	return readDarkModeParam(ui_config_manager::tryGet());
}

DarkMode::param_t fb2k::darkParams(bool v) {
	auto def = darkParams();
	if (def.bDark == v) return def;
	return { .bDark = v };
}