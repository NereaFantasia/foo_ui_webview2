// TrayMenuTestSupport.h - Tray menu builders shared by the tray contract tests
// Used by test_taskbar_tray_contracts.cpp and test_tray_menu_contracts.cpp.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include "window/MenuTokenTable.h"
#include "window/TrayIcon.h"

inline TrayMenuItem MkTrayItem(std::string id, bool visible = true) {
    TrayMenuItem m;
    m.id = std::move(id);
    m.type = "normal";
    m.visible = visible;
    return m;
}

// Deterministic sequential token generator ("t0", "t1", ...).
inline MenuTokenTable::TokenGen SeqGen(std::shared_ptr<int> n) {
    return [n]() -> std::optional<std::string> { return "t" + std::to_string((*n)++); };
}
