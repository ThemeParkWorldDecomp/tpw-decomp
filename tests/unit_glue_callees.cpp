// Test library for the weak library-glue callees (unit v5L49; libtpw_unit_glue_callees.dylib): src/game/online/online.cpp. No fakes: the
// definitions under test are the weak ones, and wcslen is platform/msl.cpp's.
#include <cstdint>

#include "calllog.h"
#include "online.h"
#include "ui_DrawObject.h"

extern "C" {

uint32_t tpw_glue_parser_wcslen(const char16_t* text) { return online_parser_callee::WcsLen(text); }
uint32_t tpw_glue_ostream_wcslen(const char16_t* text) { return wostream_callee::Wcslen(text); }
void tpw_glue_wcsset(char16_t* text, uint16_t c) { textobj_callee::WcsSet(text, static_cast<char16_t>(c)); }
void tpw_glue_sprintf_wide(char* buffer, const char* format, const char16_t* text) { ignore_callee::SprintfWide(buffer, format, text); }

}  // extern "C"
