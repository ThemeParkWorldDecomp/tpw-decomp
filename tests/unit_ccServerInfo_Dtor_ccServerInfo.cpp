// Test library for ccServerInfo::~ccServerInfo (unit v5140; libtpw_unit_ccServerInfo_Dtor_ccServerInfo.dylib), main app:
//   src/game/online/FS_Online.cpp
//
// free is a recording fake (this library's own definition wins over libc's inside the library); it logs the first word of the block,
// the case's tag, only while the destructor runs.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"

using namespace calllog;

namespace {
bool gInDtor = false;
}

extern "C" void free(void* p) {
    if (gInDtor && p != nullptr) ev("free", {static_cast<int64_t>(*static_cast<uint32_t*>(p))});
}

extern "C" {

uint64_t tpw_ccs_addr(const char*) { return 0; }

uint64_t tpw_ccs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_ccs_off(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

int32_t tpw_ccs_layout(int64_t* out) {
    static ccServerInfo s;
    char* b = reinterpret_cast<char*>(&s);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&s.mStr08) - b;
    out[n++] = reinterpret_cast<char*>(&s.mStr0C) - b;
    out[n++] = reinterpret_cast<char*>(&s.mStr16) - b;
    out[n++] = reinterpret_cast<char*>(&s.mStr1A) - b;
    out[n++] = reinterpret_cast<char*>(&s.mStr22) - b;
    out[n++] = static_cast<int64_t>(sizeof(ccServerInfo));
    return n;
}

void tpw_ccs_dtor(uint64_t object) {
    gInDtor = true;
    reinterpret_cast<ccServerInfo*>(static_cast<uintptr_t>(object))->~ccServerInfo();
    gInDtor = false;
}

}  // extern "C"
