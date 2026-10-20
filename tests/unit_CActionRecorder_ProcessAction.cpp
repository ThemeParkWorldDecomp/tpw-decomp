// Test library for the CActionRecorder::ProcessAction overloads (unit v5254): every callee is a recording fake.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"

using namespace calllog;

void SetPlaceNormalPylon();
void SetPlaceSpecialPylon();
void LayoutRegenerateTrack();
void LayoutDeleteLastTrackBrace();
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b);
void LayoutReplayCoasterSpeed(uint32_t a, uint32_t b);
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c);

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
const char* const kFormats[] = {
    "Erroneous stuff going down\n",
    "AR replay - %d\n",
    "AR replay - %d (%d, %d)\n",
    "AR replay - %d (%d, %d, %d)\n",
    "AR replay - %d (%g, %d)\n",
    "AR replay - %d (%g, %g, %d)\n",
};
}  // namespace

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t ids = -1;
    for (int i = 0; i < 6; ++i)
        if (std::strcmp(kFormats[i], format) == 0) ids = i;
    int64_t words[8];
    int n = 0;
    va_list ap;
    va_start(ap, format);
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == 'd') {
            words[n++] = va_arg(ap, int);
        } else if (*p == 'g') {
            double d = va_arg(ap, double);
            uint64_t bits;
            std::memcpy(&bits, &d, 8);
            if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0xfffffffffffffull)) bits = 0x7ff8000000000000ull;
            words[n++] = static_cast<int32_t>(bits >> 32);
            words[n++] = static_cast<int32_t>(bits & 0xffffffffu);
        }
    }
    va_end(ap);
    int64_t v[10];
    int k = 0;
    v[k++] = level;
    v[k++] = ids;
    for (int i = 0; i < n; ++i) v[k++] = words[i];
    switch (k) {
        case 2: ev("CTRACE", {v[0], v[1]}); break;
        case 3: ev("CTRACE", {v[0], v[1], v[2]}); break;
        case 4: ev("CTRACE", {v[0], v[1], v[2], v[3]}); break;
        case 5: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4]}); break;
        case 6: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5]}); break;
        case 7: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5], v[6]}); break;
        default: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]}); break;
    }
}

void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("Down", {x, y, shift, flag}); }
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("Up", {x, y, shift, flag}); }
void LLCC_SystemSetRide(const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id, int32_t track) { ev("SetRide", {id.mIndex, track}); }
void LayoutSetLoftPylon(float value, uint32_t id) { ev("Loft", {CanonBits(value), static_cast<int64_t>(id)}); }
void LayoutSetRotatePylon(float value, uint32_t id) { ev("Rotate", {CanonBits(value), static_cast<int64_t>(id)}); }
void LayoutSetWobblePylon(float x, float z, uint32_t id) { ev("Wobble", {CanonBits(x), CanonBits(z), static_cast<int64_t>(id)}); }
void SetPlaceNormalPylon() { ev("Normal", {}); }
void SetPlaceSpecialPylon() { ev("Special", {}); }
void LayoutRegenerateTrack() { ev("Regen", {}); }
void LayoutDeleteLastTrackBrace() { ev("DelBrace", {}); }
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b) { ev("DelMulti", {a, b}); }
void LayoutReplayCoasterSpeed(uint32_t a, uint32_t b) { ev("Speed", {static_cast<int64_t>(a), static_cast<int64_t>(b)}); }
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c) { ev("Move", {a, b, c}); }

extern "C" {

uint64_t tpw_pa_addr(const char*) { return 0; }
uint64_t tpw_pa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
int32_t tpw_pa_layout(int64_t*) { return 0; }

void tpw_pa_call(int32_t op, int32_t action, uint32_t a, uint32_t b, uint32_t c, float fa, float fb2) {
    CActionRecorder* r = reinterpret_cast<CActionRecorder*>(tpw::ArenaAllocZeroed(sizeof(CActionRecorder)));
    switch (op) {
        case 0: r->ProcessAction(action); break;
        case 1: r->ProcessAction(action, static_cast<uint8_t>(a), static_cast<uint8_t>(b), static_cast<uint16_t>(c)); break;
        case 2: r->ProcessAction(action, static_cast<uint8_t>(a), static_cast<uint8_t>(b)); break;
        case 3: r->ProcessAction(action, static_cast<uint16_t>(a), static_cast<uint8_t>(b)); break;
        case 4: r->ProcessAction(action, fa, static_cast<uint16_t>(b)); break;
        case 5: r->ProcessAction(action, fa, fb2, static_cast<uint16_t>(c)); break;
        case 6: r->ProcessAction(action, static_cast<uint16_t>(a), static_cast<uint16_t>(b)); break;
        default: r->ProcessAction(action, static_cast<uint16_t>(a), static_cast<uint16_t>(b), static_cast<uint16_t>(c)); break;
    }
}

}  // extern "C"
