// Test library for the static initialiser of tb_starfield.cpp (unit v5283), main app:
//   src/engine/tb_starfield.cpp  __sinit_tb_starfield_cpp, gStarfieldAtExitNode
//
// __register_global_object is a recording fake. gStarfield and ~CStarfield are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_starfield.h"

using namespace calllog;

CStarfield gStarfield;

CStarfield::~CStarfield() {}   // defined in another file on the Mac; not under test

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("__register_global_object", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_ss_addr(const char* name) {
    if (std::strcmp(name, "obj") == 0) return reinterpret_cast<uintptr_t>(&gStarfield);
    if (std::strcmp(name, "dtor") == 0) return reinterpret_cast<uintptr_t>(&StarfieldDestructorThunk);
    if (std::strcmp(name, "node") == 0) return reinterpret_cast<uintptr_t>(gStarfieldAtExitNode);
    return 0;
}

// Fills the object with a junk byte before the call.
void tpw_ss_junk(uint32_t byte) { std::memset(static_cast<void*>(&gStarfield), static_cast<int>(byte), sizeof(gStarfield)); }

// The seven fields (Mac order); pointers as their low 32 bits.
void tpw_ss_read(int64_t* out) {
    CStarfield& s = gStarfield;
    uint32_t sz;
    std::memcpy(&sz, &s.mSize, 4);
    out[0] = static_cast<uint32_t>(s.mCount);
    out[1] = static_cast<uint32_t>(s.mF04);
    out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mTexture));
    out[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mStars));
    out[4] = sz;
    out[5] = static_cast<uint32_t>(s.mF14);
    out[6] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mMatrix));
}

void tpw_ss_run() { __sinit_tb_starfield_cpp(); }

}  // extern "C"
