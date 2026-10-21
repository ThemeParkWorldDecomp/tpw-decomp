// The weak stand-ins of the unit library tpw_unit_CParser_Parse (cmake/units/CParser_Parse.cmake), as
// src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser_CParser_Parse.cpp of before the
// regroup (v5L30b) defined them, unchanged. The regroup put each function into the src file of its original source
// file, where these defaults would sit next to the real definitions, so it dropped them; the unit library compiles
// them from here, so that it builds what it built before.

#include <cstdint>
#include <string>
#include "online.h"
#include "ui_ime.h"

namespace cparser_callee {
// Weak stand-ins (the set and string library are not translated); the test library replaces them with recording fakes.
__attribute__((weak)) void* TreeFind(void* tree, const char16_t*) { return static_cast<char*>(tree) + 8; }
__attribute__((weak)) void TreeDestroy(void*, void*) {}
}  // namespace cparser_callee
