// Test library for GetTangentsFromZVector (unit v5011; libtpw_unit_GetTangentsFromZVector.dylib, tests/test_GetTangentsFromZVector.py), main app:
//   src/game/rides/CoasterPylon.cpp   GetTangentsFromZVector 0x1003be20
//
// The callees outside the unit are the MSL glue imports sqrt, __fpclassifyf and fmod: fakes that record the arguments (doubles as two words)
// and answer from the test (sqrt and fmod compute; __fpclassifyf answers from its queue). gTPGlobals is defined here, zero at start.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPGlobals.h"

using namespace calllog;

// 0x103b74c8: the class holds strings whose constructors live in other files, so the object is raw zeroed storage under the program's symbol
// name (as the Mac's bss is); the test writes the words it needs.
alignas(16) char gTPGlobalsRaw[sizeof(TPGlobals)] __asm__("_gTPGlobals");

namespace {

int64_t CanonHi(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0xfffffffffffffull) != 0) bits = 0x7ff8000000000000ull;
    return static_cast<int64_t>(bits >> 32);
}

int64_t CanonLo(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0xfffffffffffffull) != 0) bits = 0x7ff8000000000000ull;
    return static_cast<int64_t>(bits & 0xffffffffull);
}

}  // namespace

namespace MathLib {

double sqrt(double x) {
    ev("sqrt", {CanonHi(x), CanonLo(x)});
    return std::sqrt(x);
}

int __fpclassifyf(float x) {
    ev("fpclassify", {static_cast<int64_t>(fbits(x))});
    return static_cast<int>(ret("fpclassify"));
}

double fmod(double x, double y) {
    ev("fmod", {CanonHi(x), CanonLo(x), CanonHi(y), CanonLo(y)});
    return std::fmod(x, y);
}

}  // namespace MathLib

extern "C" {

uint64_t tpw_gtz_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTPGlobals", gTPGlobalsRaw},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gtz_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module relies on (HOST_LAYOUT).
int32_t tpw_gtz_layout(int64_t* out) {
    int32_t n = 0;
    static tag_sBezierControl c;
    static tag_sTrackBrace b;
    TPGlobals& g = *reinterpret_cast<TPGlobals*>(gTPGlobalsRaw);
    static tag_sCoasterType t;
    static sPylonControl p;
    auto off = [](const void* base, const void* member) {
        return static_cast<int64_t>(static_cast<const char*>(member) - static_cast<const char*>(base));
    };
    out[n++] = off(&c, &c.mBrace);
    out[n++] = off(&b, &b.mFlags);
    out[n++] = off(&b, &b.mF0C);
    out[n++] = off(&b, &b.mPrev);
    out[n++] = off(&b, &b.mNext);
    out[n++] = off(&b, &b.mFE8);
    out[n++] = off(&g, &g.mSinTableMask);
    out[n++] = off(&g, &g.mSinTableScale);
    out[n++] = off(&g, &g.mSinTable);
    out[n++] = off(&t, &t.mF08);
    out[n++] = off(&p, &p.mFlags);
    out[n++] = static_cast<int64_t>(sizeof(tag_sBezierControl));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    return n;
}

void tpw_gtz_call(uint64_t coaster, uint64_t point, uint64_t control, float a, float b, uint64_t pylon, uint64_t p7, uint64_t p8, int32_t flag,
                  int32_t unusedC) {
    GetTangentsFromZVector(reinterpret_cast<tag_sCoasterType*>(coaster), reinterpret_cast<s3DPoint*>(point),
                           reinterpret_cast<tag_sBezierControl*>(control), a, b, reinterpret_cast<sPylonControl*>(pylon),
                           reinterpret_cast<s3DPoint*>(p7), reinterpret_cast<s3DPoint*>(p8), flag, unusedC);
}

}  // extern "C"
