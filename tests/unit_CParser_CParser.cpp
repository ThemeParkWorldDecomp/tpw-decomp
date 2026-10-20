// Test library for CParser::CParser and SetupWhitespaceSet (unit v5727; libtpw_unit_CParser_CParser.dylib), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser.cpp
//
// The callees (string ctor, tree ctor, SetupSeparatorSet, AddWhitespace) are fakes that record which field of the parser they got.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {
CParser* gThis = nullptr;
int64_t Field(const void* p) {
    if (p == &gThis->mString) return 0;
    if (p == gThis->mSeparators) return 1;
    if (p == gThis->mWhitespace) return 2;
    return -1;
}
}  // namespace

namespace online_callee {
void WStringCtor(void* self) { ev("WStringCtor", {Field(self)}); }
}
namespace parser_callee {
void TreeCtor(void* tree, const void*, const void*) { ev("TreeCtor", {Field(tree)}); }
}
void CParser::SetupSeparatorSet() { ev("SetupSeparatorSet", {}); }
void CParser::AddWhitespace(char16_t c) { ev("AddWhitespace", {static_cast<int64_t>(c)}); }

extern "C" {

uint64_t tpw_CParser_addr(const char* name) {
    if (std::strcmp(name, "vt") == 0) return reinterpret_cast<uintptr_t>(gCParserVtableBlock + 8);
    return 0;
}

uint64_t tpw_CParser_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_CParser_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParser, mVtable));
    return n;
}

uint64_t tpw_CParser_vtable(uint64_t obj) { return reinterpret_cast<uintptr_t>(reinterpret_cast<CParser*>(obj)->mVtable); }

void tpw_CParser_ctor(uint64_t obj) {
    gThis = reinterpret_cast<CParser*>(obj);
    new (gThis) CParser();
}

void tpw_CParser_setup_ws(uint64_t obj) {
    gThis = reinterpret_cast<CParser*>(obj);
    gThis->SetupWhitespaceSet();
}

}  // extern "C"
