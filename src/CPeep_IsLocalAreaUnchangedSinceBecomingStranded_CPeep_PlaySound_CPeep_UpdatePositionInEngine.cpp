#include <cmath>
#include <cstdint>
#include <cstring>
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include "DynamicMesh.h"
#include "CPeep_SAVEVAR_2.h"
#include "ppc_math.h"

// Statics of Peep.cpp used by UpdatePositionInEngine (names invented): a guard byte and the value it protects, set the first time through.
uint8_t gPeepGuardC490;      // 0x1034c490
int32_t gPeepBalloonIndex;   // 0x1034c494: the balloon offset row in use
uint8_t gPeepGuardC488;      // 0x1034c488
float gPeepBalloonTime;      // 0x1034c48c: 0 to 1
uint8_t gPeepGuardC480;      // 0x1034c480
float gPeepBalloonRate;      // 0x1034c484: 0.1
uint8_t gPeepGuardC478;      // 0x1034c478
float gPeepBalloonScale;     // 0x1034c47c: 1.5
uint8_t gPeepGuardC470;      // 0x1034c470
float gPeepC474;             // 0x1034c474: 0.1 (written, never read)
uint8_t gPeepGuardC468;      // 0x1034c468
float gPeepBalloonDivisor;   // 0x1034c46c: 0.5
uint8_t gPeepGuardC460;      // 0x1034c460
float gPeepStepLag;          // 0x1034c464: 0.35
// Initialised data of Peep.cpp (names invented).
float gPeepScriptHeight = 2.5f;      // 0x102a0d98
float gPeepBalloonBase = 1.0f;       // 0x102a0d9c
uint32_t gPeepBalloonRows = 20;      // 0x102a0da0
// 0x102a0da4: 12 bytes a row, the balloon's x, y, z offset by frame (the image's data): it bobs up 0.2 and back over the 20 rows.
float gPeepBalloonOffsets[20][3] = {
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.02f, 0.0f}, {0.0f, 0.04f, 0.0f}, {0.0f, 0.06f, 0.0f},
    {0.0f, 0.08f, 0.0f}, {0.0f, 0.1f, 0.0f}, {0.0f, 0.12f, 0.0f}, {0.0f, 0.14f, 0.0f},
    {0.0f, 0.16f, 0.0f}, {0.0f, 0.18f, 0.0f}, {0.0f, 0.2f, 0.0f}, {0.0f, 0.18f, 0.0f},
    {0.0f, 0.16f, 0.0f}, {0.0f, 0.14f, 0.0f}, {0.0f, 0.12f, 0.0f}, {0.0f, 0.1f, 0.0f},
    {0.0f, 0.08f, 0.0f}, {0.0f, 0.06f, 0.0f}, {0.0f, 0.04f, 0.0f}, {0.0f, 0.0f, 0.0f},
};

namespace {

char* Bytes(void* p) { return static_cast<char*>(p); }
template <class T> T& At(void* p, int off) { return *reinterpret_cast<T*>(Bytes(p) + off); }

// Mac code addresses the sprite scripts are told apart by.
void* const kWalkScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f2ac));
void* const kCarryScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f948));
void* const kRunScript = reinterpret_cast<void*>(static_cast<uintptr_t>(0x1029f308));

// divwu: the emulator answers 0 for a zero divisor.
uint32_t Divwu(uint32_t a, uint32_t b) { return b == 0 ? 0 : a / b; }

}  // namespace

void CPeep::UpdatePositionInEngine(float step) {
    if (gPeepGuardC490 == 0) { gPeepBalloonIndex = 0; gPeepGuardC490 = 1; }
    if (gPeepGuardC488 == 0) { gPeepBalloonTime = 0.0f; gPeepGuardC488 = 1; }
    if (gPeepGuardC480 == 0) { gPeepBalloonRate = 0.1f; gPeepGuardC480 = 1; }
    if (gPeepGuardC478 == 0) { gPeepBalloonScale = 1.5f; gPeepGuardC478 = 1; }
    if (gPeepGuardC470 == 0) { gPeepC474 = 0.1f; gPeepGuardC470 = 1; }
    if (gPeepGuardC468 == 0) { gPeepBalloonDivisor = 0.5f; gPeepGuardC468 = 1; }
    if (gPeepGuardC460 == 0) { gPeepStepLag = 0.35f; gPeepGuardC460 = 1; }

    const float d = step - gPeepStepLag;
    float f28;
    if (d < -1.0f) {
        f28 = -1.0f;
    } else {
        f28 = 2.0f;
        if (!(d > 2.0f)) f28 = d;
    }
    Pilot* pilot = reinterpret_cast<Pilot*>(Bytes(this) + 0xd4);
    const int32_t last190 = At<int32_t>(this, 0x190);
    const int32_t last194 = At<int32_t>(this, 0x194);
    int32_t px, py;
    pilot->get_pos(px, py);
    const float lx = static_cast<float>(last190), ly = static_cast<float>(last194);
    const int32_t ix = ppc_fctiwz(static_cast<double>(ppc_fmadds(f28, static_cast<float>(px - last190), lx)));
    const int32_t iy = ppc_fctiwz(static_cast<double>(ppc_fmadds(f28, static_cast<float>(py - last194), ly)));
    const float f30 = (10.0f * static_cast<float>(ix)) / 65536.0f;
    f28 = (10.0f * static_cast<float>(iy)) / 65536.0f;

    float f27;
    if (step < 0.0f) {
        f27 = 0.0f;
    } else {
        f27 = 1.0f;
        if (!(step > 1.0f)) f27 = step;
    }
    int32_t qx, qy;
    pilot->get_pos(qx, qy);
    const int32_t jx = ppc_fctiwz(static_cast<double>(ppc_fmadds(f27, static_cast<float>(qx - last190), lx)));
    const int32_t jy = ppc_fctiwz(static_cast<double>(ppc_fmadds(f27, static_cast<float>(qy - last194), ly)));
    f27 = (10.0f * static_cast<float>(jx)) / 65536.0f;
    const float f26 = (10.0f * static_cast<float>(jy)) / 65536.0f;

    if (At<int32_t>(this, 0x28) == 0) {
        SCR_SetScriptPosition(At<int32_t>(this, 0xc), f27, 0.0f, f26, static_cast<int32_t>(((static_cast<uint32_t>(At<int32_t>(this, 0x1c)) + 0x480u) >> 8) & 7));
    }
    if (IsStranded()) {
        const uint16_t cell = static_cast<uint16_t>(static_cast<uint8_t>(Bytes(this)[4]) + (static_cast<uint8_t>(Bytes(this)[6]) << 7) + 1);
        const int32_t cx = static_cast<int32_t>((static_cast<uint32_t>(cell) - 1u) & 0x7f);
        const int32_t cy = static_cast<int32_t>((static_cast<uint32_t>(cell) - 1u) >> 7);
        DC_AddSquareFaceToList(10.0f * static_cast<float>(cx), 1.0f, 10.0f * static_cast<float>(cy), 0, 1, 0, 1.0f, 0, 0, 0, static_cast<DYN_Type>(0));
    }
    if (At<int32_t>(this, 0xb4) != 0) {
        SCR_SetScriptPosition(At<int32_t>(this, 0xb4), f27, 0.0f + gPeepScriptHeight, f26, 0);
    }
    if (static_cast<uint8_t>(Bytes(this)[2]) == 1 && At<int32_t>(this, 0x210) != 0) {
        const uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        const uint32_t sum = static_cast<uint32_t>(gPeepBalloonIndex) + id;
        const uint32_t row = sum - Divwu(sum, gPeepBalloonRows) * gPeepBalloonRows;
        const float* t = gPeepBalloonOffsets[0] + row * 3;
        const float dz = std::fabs(f28 - f26);
        const float dx = std::fabs(f30 - f27);
        float q = (dx + dz) / gPeepBalloonDivisor;
        q = gPeepBalloonBase - q;
        const float base = 0.0f + q;
        const float sc = gPeepBalloonScale;
        reinterpret_cast<CPerson*>(this)->DrawBalloon(ppc_fmadds(sc, t[0], f30), ppc_fmadds(sc, t[1], base), ppc_fmadds(sc, t[2], f28), 0);
        const float time = gPeepBalloonTime;
        if (static_cast<double>(time) >= 1.0) {
            const uint32_t n = static_cast<uint32_t>(gPeepBalloonIndex) + 1u;
            gPeepBalloonIndex = static_cast<int32_t>(n - Divwu(n, gPeepBalloonRows) * gPeepBalloonRows);
            gPeepBalloonTime = 0.0f;
        } else {
            gPeepBalloonTime = time + gPeepBalloonRate;
        }
    }
}

int32_t CPeep::PeepWalks() {
    Pilot* pilot = reinterpret_cast<Pilot*>(Bytes(this) + 0xd4);
    int32_t x0, y0, x1, y1;
    pilot->get_pos(x0, y0);
    pilot->move();
    pilot->get_pos(x1, y1);
    const int16_t hx = static_cast<int16_t>(x1 >> 8), hy = static_cast<int16_t>(y1 >> 8);
    MapPos mx, my;
    mx.mCell = static_cast<uint8_t>(static_cast<uint16_t>(hx) >> 8);
    mx.mOffset = static_cast<uint8_t>(hx);
    my.mCell = static_cast<uint8_t>(static_cast<uint16_t>(hy) >> 8);
    my.mOffset = static_cast<uint8_t>(hy);
    reinterpret_cast<CThing*>(this)->Move(mx, my);
    SetStranded(0);
    if (At<int32_t>(this, 0x18c) != 0) return 2;
    if (gPeepC498 == pilot->is_finished_path()) return 0;
    const int32_t angle = LbMaths_ArcTan(-(x1 - x0), y1 - y0);
    At<uint32_t>(this, 0x1c) = static_cast<uint32_t>(angle) & 0xffffu;
    bool fast = false;
    if (At<uint16_t>(this, 0xc2) > gPersonTable[0]) {
        const uint8_t kind = static_cast<uint8_t>(Bytes(this)[2]);
        if (kind == 1 || kind == 7) fast = true;
    }
    void* code = kWalkScript;
    int32_t script = 1;
    if (static_cast<uint8_t>(Bytes(this)[2]) == 7) {
        if (reinterpret_cast<CGuard*>(this)->IsCarrying()) {
            code = kCarryScript;
            script = 0x17;
        }
    }
    CDelayedScript* ds = reinterpret_cast<CDelayedScript*>(Bytes(this) + 0xc);
    if (!fast) {
        if (ESP_RURunningThis(At<int32_t>(this, 0xc), code) == 0) ds->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(script));
    }
    if (fast) {
        if (ESP_RURunningThis(At<int32_t>(this, 0xc), kRunScript) == 0) ds->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(2));
    }
    const int32_t ddx = x1 - x0, ddy = y1 - y0;
    const int32_t sq = static_cast<int32_t>(static_cast<uint32_t>(ddx) * static_cast<uint32_t>(ddx) + static_cast<uint32_t>(ddy) * static_cast<uint32_t>(ddy));
    float rate = static_cast<float>(std::sqrt(static_cast<double>(sq))) / 26112.0f;
    if (!fast) rate = rate * 2.0f;
    ds->SetServiceInterval(rate);
    return 1;
}
