// Test library for CPeep::UpdatePositionInEngine and CPeep::PeepWalks (unit v5974; libtpw_unit_CPeep_UpdatePositionInEngine.dylib,
// tests/test_CPeep_UpdatePositionInEngine.py), main app:
//   src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_UpdatePositionInEngine.cpp   both functions and the Peep.cpp statics
//
// The callees outside the unit are recording fakes; every pointer is logged as its offset from the peep block the test set with tpw_peep_base.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "World.h"
#include "DynamicMesh.h"
#include "CPeep_SAVEVAR_2.h"
#include "image_copies.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
int32_t gPeepC498;
int32_t gPeepC49C;
uint16_t gPersonTable[2];

extern uint8_t gPeepGuardC490, gPeepGuardC488, gPeepGuardC480, gPeepGuardC478, gPeepGuardC470, gPeepGuardC468, gPeepGuardC460;
extern int32_t gPeepBalloonIndex;
extern float gPeepBalloonTime, gPeepBalloonRate, gPeepBalloonScale, gPeepC474, gPeepBalloonDivisor, gPeepStepLag;
extern float gPeepScriptHeight, gPeepBalloonBase;
extern uint32_t gPeepBalloonRows;
extern float gPeepBalloonOffsets[20][3];
// The table as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(offsets, gPeepBalloonOffsets);

namespace {

char* gBase = nullptr;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Off(const void* p) { return static_cast<int64_t>(static_cast<const char*>(p) - gBase); }

}  // namespace

// ---- the fakes
void Pilot::get_pos(int32_t& x, int32_t& y) {
    ev("get_pos", {Off(this)});
    x = static_cast<int32_t>(ret("posx"));
    y = static_cast<int32_t>(ret("posy"));
}

void Pilot::move() { ev("move", {Off(this)}); }

Fixed Pilot::is_finished_path() const {
    ev("is_finished_path", {Off(this)});
    return static_cast<Fixed>(ret("isfin"));
}

void CThing::Move(MapPos x, MapPos y) {
    ev("Move", {Off(this), static_cast<int64_t>((x.mCell << 8) | x.mOffset), static_cast<int64_t>((y.mCell << 8) | y.mOffset)});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Off(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("thingid"));
    return t;
}

bool CPeep::IsStranded() {
    ev("IsStranded", {Off(this)});
    return ret("IsStranded") != 0;
}

void CPeep::SetStranded(int32_t stranded) { ev("SetStranded", {Off(this), static_cast<int64_t>(stranded)}); }

void CPerson::DrawBalloon(float x, float y, float z, int32_t mode) {
    ev("DrawBalloon", {Off(this), CanonBits(x), CanonBits(y), CanonBits(z), static_cast<int64_t>(mode)});
}

bool CGuard::IsCarrying() const {
    ev("IsCarrying", {Off(this)});
    return ret("IsCarrying") != 0;
}

void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetSpriteScript", {Off(this), static_cast<int64_t>(script)}); }

void CDelayedScript::SetServiceInterval(float rate) { ev("SetServiceInterval", {Off(this), CanonBits(rate)}); }

void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SCR_SetScriptPosition", {static_cast<int64_t>(handle), CanonBits(x), CanonBits(y), CanonBits(z), static_cast<int64_t>(mode)});
}

void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t keep, int32_t a, DYN_Type type) {
    ev("DC_AddSquareFaceToList", {CanonBits(x), CanonBits(y), CanonBits(z), static_cast<int64_t>(side), static_cast<int64_t>(texture),
                                  static_cast<int64_t>(rotation), CanonBits(unused1), static_cast<int64_t>(large), static_cast<int64_t>(keep),
                                  static_cast<int64_t>(a), static_cast<int64_t>(type)});
}

int32_t ESP_RURunningThis(int32_t handle, void* code) {
    ev("ESP_RURunningThis", {static_cast<int64_t>(handle), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(code)))});
    return static_cast<int32_t>(ret("ESP"));
}

int32_t LbMaths_ArcTan(int32_t x, int32_t y) {
    ev("LbMaths_ArcTan", {static_cast<int64_t>(x), static_cast<int64_t>(y)});
    return static_cast<int32_t>(ret("ArcTan"));
}

extern "C" {

uint64_t tpw_peep_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"g490", &gPeepGuardC490}, {"idx", &gPeepBalloonIndex}, {"g488", &gPeepGuardC488}, {"time", &gPeepBalloonTime},
        {"g480", &gPeepGuardC480}, {"rate", &gPeepBalloonRate}, {"g478", &gPeepGuardC478}, {"scale", &gPeepBalloonScale},
        {"g470", &gPeepGuardC470}, {"c474", &gPeepC474}, {"g468", &gPeepGuardC468}, {"divisor", &gPeepBalloonDivisor},
        {"g460", &gPeepGuardC460}, {"lag", &gPeepStepLag}, {"height", &gPeepScriptHeight}, {"base", &gPeepBalloonBase},
        {"rows", &gPeepBalloonRows}, {"offsets", gPeepBalloonOffsets}, {"c498", &gPeepC498}, {"table", gPersonTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_peep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_peep_base(uint64_t base) { gBase = reinterpret_cast<char*>(static_cast<uintptr_t>(base)); }

int32_t tpw_peep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MapPos));
    out[n++] = static_cast<int64_t>(sizeof(gPeepBalloonOffsets));
    return n;
}

void tpw_peep_update(uint64_t peep, float step) {
    reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))->UpdatePositionInEngine(step);
}

int32_t tpw_peep_walks(uint64_t peep) { return reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))->PeepWalks(); }

}  // extern "C"
