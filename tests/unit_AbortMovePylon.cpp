// Test library for the pylon-move abort and the small coaster predicates (unit v5967; libtpw_unit_AbortMovePylon.dylib, tests/test_AbortMovePylon.py), main app:
//   src/game/park/TPHField.cpp, src/game/rides/CoasterEdit.cpp, src/game/rides/CoasterTrack.cpp   AbortMovePylon, SingleHFieldTextureChange, PlacePylon (4 arguments), IsCoasterUseable, IsTrackValid,
//                                        IsTrackClosed (void*), DecCoasterConst2
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "HFieldTextures.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "MainWindowCallback.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "keytable_actions.h"
#include "ui_main.h"

using namespace calllog;

int32_t IsTrackClosed(tag_sCoasterInstance* instance);

// ---- globals of other files
tag_sCoasterInstance* gCoasterEditInstance;
int32_t gExtrudeTextureKind;
uint32_t gCoasterWord16BC;
int32_t gPylonMoveZ;
int32_t gPylonMoveX;
uint32_t gCoasterWord16E0;
tag_sPylon* gPylonMoveOrigData;
tag_sTrackBrace* gPylonMoveBrace;
tag_sTrackBrace* gPylonMoveMark2;
tag_sTrackBrace* gPylonMoveMark1;
uint32_t gCoasterInputFlags;
tag_sM3D2Header* gHeightfieldHeader;
sExtraTextureTable gExtraTextureTable;
CActionRecorder gActionRecorder;

// pointers the fakes answer (set by the test through tpw_av_addr)
uint64_t gSlotFind;
uint64_t gSlotPylon;
uint64_t gSlotBrace;

namespace {
int64_t CanonD(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0xfffffffffffffull) != 0) bits = 0x7ff8000000000000ull;
    return static_cast<int64_t>(bits);
}
}  // namespace

// ---- the fakes
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }

void MoveBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sPylon* pylon, uint32_t x, uint32_t z, float* a, float* b,
               int32_t c, int32_t d) {
    ev("MoveBrace", {lab(instance), lab(brace), lab(pylon), x, z, lab(a), lab(b), c, d});
}

void LLCC_AbortPylonMove() { ev("LLCC_AbortPylonMove", {}); }
void LockPointedAtRide(int32_t flag) { ev("LockPointedAtRide", {flag}); }
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }

void SetHeightfieldTexture(tag_sM3D2Header* header, sExtraTextureTable* table, int32_t x, int32_t y, int32_t a, int32_t b, int32_t c) {
    ev("SetHeightfieldTexture", {lab(header), lab(table), x, y, a, b, c});
}

void TPForce3DRefresh(uint32_t flags) { ev("TPForce3DRefresh", {flags}); }

tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    ev("Find", {entrance});
    return reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(gSlotFind));
}

int32_t IsTrackClosed(tag_sCoasterInstance* instance) {
    ev("ITC", {lab(instance)});
    return static_cast<int32_t>(ret("ITC"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    double a = va_arg(ap, double);
    double b = va_arg(ap, double);
    va_end(ap);
    int64_t sum = 0;
    for (const char* p = format; *p; ++p) sum += static_cast<uint8_t>(*p);
    int64_t da = CanonD(a), db = CanonD(b);
    ev("CTRACE", {level, sum, static_cast<int64_t>(std::strlen(format)), (da >> 32) & 0xffffffff, da & 0xffffffff, (db >> 32) & 0xffffffff,
                  db & 0xffffffff});
}

int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, tag_sPylon** outPylon, tag_sTrackBrace** outBrace, uint8_t level, uint32_t extra) {
    ev("PlacePylon7", {x, z, flag, level, extra});
    *outPylon = reinterpret_cast<tag_sPylon*>(static_cast<uintptr_t>(gSlotPylon));
    *outBrace = reinterpret_cast<tag_sTrackBrace*>(static_cast<uintptr_t>(gSlotBrace));
    return static_cast<int32_t>(ret("PP"));
}

extern "C" {

uint64_t tpw_av_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},
        {"kind", &gExtrudeTextureKind},
        {"w16bc", &gCoasterWord16BC},
        {"movez", &gPylonMoveZ},
        {"movex", &gPylonMoveX},
        {"w16e0", &gCoasterWord16E0},
        {"orig", &gPylonMoveOrigData},
        {"brace", &gPylonMoveBrace},
        {"mark2", &gPylonMoveMark2},
        {"mark1", &gPylonMoveMark1},
        {"flags", &gCoasterInputFlags},
        {"hdr", &gHeightfieldHeader},
        {"table", &gExtraTextureTable},
        {"rec", &gActionRecorder},
        {"slot_find", &gSlotFind},
        {"slot_pylon", &gSlotPylon},
        {"slot_brace", &gSlotBrace},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_av_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_av_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF140));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNumber));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    return n;
}

void tpw_av_abort() { AbortMovePylon(); }
void tpw_av_single(int32_t x, int32_t y, uint64_t tile) { SingleHFieldTextureChange(x, y, reinterpret_cast<const CCellBase::CELL_IndexType*>(static_cast<uintptr_t>(tile))); }
uint32_t tpw_av_place(uint32_t x, uint32_t z, int32_t flag, uint64_t slot) {
    return static_cast<uint32_t>(PlacePylon(x, z, flag, reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(slot))));
}
uint32_t tpw_av_usable(uint64_t obj) { return static_cast<uint32_t>(IsCoasterUseable(reinterpret_cast<CObject*>(static_cast<uintptr_t>(obj)))) & 0xff; }
uint32_t tpw_av_valid(uint64_t obj) { return static_cast<uint32_t>(IsTrackValid(reinterpret_cast<void*>(static_cast<uintptr_t>(obj)))); }
uint32_t tpw_av_closed(uint64_t obj) { return static_cast<uint32_t>(IsTrackClosed(reinterpret_cast<void*>(static_cast<uintptr_t>(obj)))); }
uint32_t tpw_av_dec() { return static_cast<uint32_t>(DecCoasterConst2()); }

}  // extern "C"
