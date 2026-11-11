#pragma once

// 全局热键的按键串解析与注册校验。纯函数，不碰窗口与系统热键表，供 KeyboardApi 与 gtest 共用。

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace keyboard_hotkey_rules {

// 把 "Ctrl+Alt+K" 这类按键串解析为 MOD_* 修饰键与虚拟键码。大小写不敏感；修饰键可重复、
// 顺序任意。键名不认识时返回 false。
inline bool ParseKeyString(const std::string& keyStr, UINT& modifiers, UINT& vk) {
    modifiers = 0;
    vk = 0;

    std::string key = keyStr;

    auto findAndRemove = [&key](const std::string& mod, UINT flag, UINT& mods) {
        size_t pos;
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        std::string lowerMod = mod;
        std::transform(lowerMod.begin(), lowerMod.end(), lowerMod.begin(), ::tolower);

        while ((pos = lowerKey.find(lowerMod + "+")) != std::string::npos) {
            mods |= flag;
            key.erase(pos, mod.length() + 1);
            lowerKey.erase(pos, lowerMod.length() + 1);
        }
    };

    findAndRemove("Ctrl", MOD_CONTROL, modifiers);
    findAndRemove("Control", MOD_CONTROL, modifiers);
    findAndRemove("Alt", MOD_ALT, modifiers);
    findAndRemove("Shift", MOD_SHIFT, modifiers);
    findAndRemove("Win", MOD_WIN, modifiers);

    while (!key.empty() && (key[0] == ' ' || key[0] == '+'))
        key = key.substr(1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '+'))
        key.pop_back();

    std::string upperKey = key;
    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);

    static const std::map<std::string, UINT> keyMap = {
        // Letters
        {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'},
        {"F", 'F'}, {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'},
        {"K", 'K'}, {"L", 'L'}, {"M", 'M'}, {"N", 'N'}, {"O", 'O'},
        {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}, {"S", 'S'}, {"T", 'T'},
        {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'}, {"Y", 'Y'}, {"Z", 'Z'},

        // Numbers
        {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
        {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},

        // Function keys
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},

        // Special keys
        {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"RETURN", VK_RETURN},
        {"TAB", VK_TAB}, {"ESCAPE", VK_ESCAPE}, {"ESC", VK_ESCAPE},
        {"BACKSPACE", VK_BACK}, {"DELETE", VK_DELETE}, {"DEL", VK_DELETE},
        {"INSERT", VK_INSERT}, {"INS", VK_INSERT},
        {"HOME", VK_HOME}, {"END", VK_END},
        {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT},
        {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},

        // Media keys
        {"PLAYPAUSE", VK_MEDIA_PLAY_PAUSE}, {"MEDIASTOP", VK_MEDIA_STOP},
        {"NEXTTRACK", VK_MEDIA_NEXT_TRACK}, {"PREVTRACK", VK_MEDIA_PREV_TRACK},
        {"VOLUMEUP", VK_VOLUME_UP}, {"VOLUMEDOWN", VK_VOLUME_DOWN},
        {"VOLUMEMUTE", VK_VOLUME_MUTE},

        // Punctuation
        {"-", VK_OEM_MINUS}, {"=", VK_OEM_PLUS},
        {"[", VK_OEM_4}, {"]", VK_OEM_6},
        {";", VK_OEM_1}, {"'", VK_OEM_7},
        {",", VK_OEM_COMMA}, {".", VK_OEM_PERIOD},
        {"/", VK_OEM_2}, {"\\", VK_OEM_5},
        {"`", VK_OEM_3},
    };

    auto it = keyMap.find(upperKey);
    if (it != keyMap.end()) {
        vk = it->second;
        return true;
    }

    if (upperKey.length() == 1) {
        unsigned char c = static_cast<unsigned char>(upperKey[0]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            vk = static_cast<UINT>(c);
            return true;
        }
    }

    return false;
}

inline bool IsMediaKey(UINT vk) {
    switch (vk) {
    case VK_MEDIA_PLAY_PAUSE:
    case VK_MEDIA_STOP:
    case VK_MEDIA_NEXT_TRACK:
    case VK_MEDIA_PREV_TRACK:
    case VK_VOLUME_UP:
    case VK_VOLUME_DOWN:
    case VK_VOLUME_MUTE:
        return true;
    default:
        return false;
    }
}

// 系统级热键会在所有程序里截走这个组合，所以只收带 Ctrl、Alt 或 Win 的组合（媒体键例外），
// 并拒绝文本编辑与窗口切换的常用组合（文本编辑那组与 Columns UI 的同一名单）。
// 可以注册时返回 nullptr，否则返回拒绝原因，接在按键串后面拼成错误信息。
inline const char* RejectReason(UINT modifiers, UINT vk) {
    const UINT mods = modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN);
    if (IsMediaKey(vk)) return nullptr;
    if ((mods & (MOD_CONTROL | MOD_ALT | MOD_WIN)) == 0) {
        return "needs Ctrl, Alt or Win; only a media key may be registered alone";
    }
    if (mods == MOD_CONTROL) {
        switch (vk) {
        case 'A': case 'C': case 'V': case 'X': case 'Y': case 'Z':
            return "is a text-editing shortcut and cannot be taken system-wide";
        case VK_ESCAPE:
            return "is reserved by Windows";
        default:
            break;
        }
    }
    if (mods == (MOD_CONTROL | MOD_SHIFT) && vk == 'Z') {
        return "is a text-editing shortcut and cannot be taken system-wide";
    }
    if (mods == MOD_ALT && (vk == VK_F4 || vk == VK_TAB || vk == VK_SPACE)) {
        return "is reserved by Windows";
    }
    return nullptr;
}

}  // namespace keyboard_hotkey_rules
