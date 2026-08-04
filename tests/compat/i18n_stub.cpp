// i18n stub for the unit-test project.
//
// src/utils/I18n.cpp resolves the UI language by probing foobar2000's own
// strings, so it needs the full SDK. The test project deliberately builds
// against the minimal shim in compat/fb2k_types.h, so it cannot compile that
// translation unit. Headers under test (e.g. window/TrayIcon.h) still use
// TR/TRU, so provide a deterministic English-only definition here: test
// expectations must not depend on the host machine's language.

#include "utils/I18n.h"

namespace i18n {

bool IsChineseLocale() { return false; }

LanguageSource GetLanguageSource() { return LanguageSource::Override; }

LanguageOverride GetLanguageOverride() { return LanguageOverride::English; }

void SetLanguageOverride(LanguageOverride) {}

void InvalidateLanguageCache() {}

} // namespace i18n
