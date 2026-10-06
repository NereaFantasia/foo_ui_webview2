#pragma once

// Specific PFC functions are meant to be possible to override and redirect to shared.dll, using obscure MSVC linked feature.
// Inclusion of this header enables the redirect.

#ifdef _WIN32
extern "C" BOOL pfc_winFormatSystemErrorMessageHook(pfc::string_base& out, DWORD code) {return uFormatSystemErrorMessage(out, code);}
extern "C" [[noreturn]] void pfc_crashHook() {uBugCheck();}
#endif
