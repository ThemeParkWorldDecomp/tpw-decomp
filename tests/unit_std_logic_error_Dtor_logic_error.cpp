// Test library for the Metrowerks exception destructors, the char ref-counted pointer and the 3D line blend (unit v5949;
// libtpw_unit_std_logic_error_Dtor_logic_error.dylib, tests/test_std_logic_error_Dtor_logic_error.py), main app:
//   src/TPMain_std_logic_error_Dtor_logic_error.cpp
//
// Fakes: operator new / delete / delete[], CColour::GetFormattedValue and IntensifyPix<2>, each recording what the real one reads.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "Light.h"
#include "DrawRenderClasses.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
}  // namespace IME

namespace {
uint8_t* gNewBlk = nullptr;
uint8_t* gBase = nullptr;

uint32_t U(int64_t v) { return static_cast<uint32_t>(v); }
int64_t CanonHi(double d, bool hi) {
    uint64_t b;
    std::memcpy(&b, &d, 8);
    if ((b & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (b & 0xfffffffffffffull) != 0) b = 0x7ff8000000000000ull;
    return hi ? static_cast<int64_t>(b >> 32) : static_cast<int64_t>(b & 0xffffffffu);
}
}  // namespace

namespace IME {
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return ret("new") ? gNewBlk : nullptr;
}
void MslDelete(void* p) { ev("delete", {lab(p)}); }
void MslDeleteArray(void* p) { ev("delete[]", {lab(p)}); }
}  // namespace IME

namespace bfl { namespace draw {
uint32_t CColour::GetFormattedValue(const TbPixFormat& format) const {
    ev("GetFormattedValue", {lab(this), lab(&format)});
    return static_cast<uint32_t>(ret("fmt"));
}
}}  // namespace bfl::draw

template <>
void tpw_intensify::IntensifyPix<2>(const CTraits<2>&, uint8_t* pix, double alpha, uint32_t colour, uint32_t mask1, uint32_t mask2,
                                    uint32_t mask3, uint32_t keepMask, int32_t x, int32_t y, const TbRect& rect) {
    ev("Intensify", {U(pix - gBase), CanonHi(alpha, true), CanonHi(alpha, false), colour, mask1, mask2, mask3, keepMask, U(x), U(y)});
    ev("IntensifyRect", {U(rect.left), U(rect.top), U(rect.right), U(rect.bottom)});
}

extern "C" {

uint64_t tpw_slec_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblA", IME::gMslExcVtblA},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_slec_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_slec_setup(uint64_t newBlk, uint64_t base) {
    gNewBlk = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(newBlk));
    gBase = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(base));
}

int32_t tpw_slec_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mData));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mStride));
    out[n++] = static_cast<int64_t>(sizeof(TbRenderArea));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslRefCountedChars));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslExceptionObject, mRc));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslExceptionObject));
    return n;
}

uint64_t tpw_slec_rcdtor(uint64_t self, int32_t flag) {
    return reinterpret_cast<uintptr_t>(IME::MslRefCountedCharsDtor25e0(reinterpret_cast<IME::MslRefCountedChars*>(self), static_cast<int16_t>(flag)));
}
void tpw_slec_rcctor(uint64_t self, uint64_t p) {
    IME::MslRefCountedCharsCtor(reinterpret_cast<IME::MslRefCountedChars*>(self), reinterpret_cast<char*>(p));
}
uint64_t tpw_slec_logic(uint64_t self, int32_t flag) {
    return reinterpret_cast<uintptr_t>(IME::MslLogicErrorDtor(reinterpret_cast<IME::MslExceptionObject*>(self), static_cast<int16_t>(flag)));
}
uint64_t tpw_slec_oor(uint64_t self, int32_t flag) {
    return reinterpret_cast<uintptr_t>(IME::MslOutOfRangeDtor(reinterpret_cast<IME::MslExceptionObject*>(self), static_cast<int16_t>(flag)));
}
void tpw_slec_aa(uint64_t traits, uint64_t area, uint64_t clip, uint64_t colour, uint64_t from, uint64_t to) {
    tpw_antialias::Antialiased<2>(*reinterpret_cast<tpw_intensify::CTraits<2>*>(traits), *reinterpret_cast<TbRenderArea*>(area),
                                  *reinterpret_cast<TbRect*>(clip), *reinterpret_cast<bfl::draw::CColour*>(colour),
                                  *reinterpret_cast<TbPoint*>(from), *reinterpret_cast<TbPoint*>(to));
}

}  // extern "C"
