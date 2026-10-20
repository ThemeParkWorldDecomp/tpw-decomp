// Test library for BMP_Tick (unit v5682; libtpw_unit_BMP_Tick.dylib, tests/test_BMP_Tick.py), main app:
//   src/BMP_Tick.cpp   BMP_Tick
//
// The callees outside the unit (BMP_AI, BMP_Move, BMP_Collision, BMP_InsideArea, BMP_OutsideArea, BMP_ControlSound, GetFinishLine,
// SetNextBuoyTarget) are fakes that record the bump's index in the list, its flags word and the pointers as offsets from the arena base.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "TPAdvisor.h"

using namespace calllog;

tabCOLLIST gColList;

namespace {

uint32_t* Words(void* p) { return reinterpret_cast<uint32_t*>(p); }

int64_t Off(const void* p) {
    return p == nullptr ? 0 : static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p) - tpw::gArenaBase);
}

int64_t Index(tagBUMP* b) {
    tagBUMPWORLD* w = reinterpret_cast<tagBUMPWORLD*>(&gColList);
    return (reinterpret_cast<uint8_t*>(b) - reinterpret_cast<uint8_t*>(w->bumps.get())) / 0xac;
}

void Log(const char* name, tagBUMP* b) { ev(name, {Index(b), static_cast<int64_t>(Words(b)[0])}); }

}  // namespace

void BMP_AI(tagBUMP* b) {
    Log("BMP_AI", b);
    int64_t r = ret("BMP_AI");
    if (r & 1) Words(b)[0] &= ~1u;
    if (r & 2) Words(b)[0] |= 0x200000u;
}
void BMP_Move(tagBUMP* b) { Log("BMP_Move", b); }
void BMP_Collision(tagBUMP* b) { Log("BMP_Collision", b); }
void BMP_InsideArea(tagBUMP* b) { Log("BMP_InsideArea", b); }
void BMP_OutsideArea(tagBUMP* b) { Log("BMP_OutsideArea", b); }
void BMP_ControlSound(tagBUMP* b) { Log("BMP_ControlSound", b); }
tabBUOY* GetFinishLine(tagBUMPRIDE* ride) {
    ev("GetFinishLine", {Off(ride)});
    int64_t r = ret("GetFinishLine");
    return r == 0 ? nullptr : reinterpret_cast<tabBUOY*>(tpw::gArenaBase + r);
}
void SetNextBuoyTarget(tabBUOY* buoy, tagBUMP* b) { ev("SetNextBuoyTarget", {Off(buoy), Index(b), static_cast<int64_t>(Words(b)[0])}); }

extern "C" {

uint64_t tpw_bt_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gColList));
    return 0;
}

uint64_t tpw_bt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPWORLD, bumps));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPWORLD, bumpCount));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSLOT));
    return n;
}

uint64_t tpw_bt_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_bt_tick() { BMP_Tick(); }

}  // extern "C"
