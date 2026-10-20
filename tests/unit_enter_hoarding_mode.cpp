// Test library for the keytable actions win_ticket_7/8/9, enter_hoarding_mode, force_ride_breakdown, park_attraction_value (unit v5296;
// libtpw_unit_enter_hoarding_mode.dylib, tests/test_enter_hoarding_mode.py), main app:
//   src/ui_Postcard_panel_win_ticket_1_show_person_history_info_enter_hoarding_mode.cpp
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "analyser.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "intrface.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSGlobalMasterSave* gMasterSave;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CIntrface gIntrface;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

static_assert(kThingArraySize > 64, "the test uses ids below 64");

// ---- the fakes
// CFSGlobalMasterSave's members need this destructor (cleanup of the fake constructor); the string is never built.
template <> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeGlobal type, int32_t amount) {
    ev("EarnedTicket", {lab(this), static_cast<int64_t>(type), amount});
    return 0;
}

CMMPathDraw::CMMPathDraw(CELL_MODES mode) { ev("CMMPathDraw", {newlab(this), static_cast<int64_t>(mode)}); }
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

// The virtual functions the vtables need; never called.
CMMPathDraw::~CMMPathDraw() {}
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 0; }
void CMMPathDraw::OnAssign() {}
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 0; }
void CMMDefault::OnAssign() {}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition & 0xff}, {format}); }

bool CObject::IsBrokenDown() const {
    ev("IsBrokenDown", {lab(this)});
    return ret("IsBrokenDown") != 0;
}
void CObject::RequestBreakdown() { ev("RequestBreakdown", {lab(this)}); }

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    const int64_t off = static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gGameWorld));
    ev("GetFunnyTime", {off, (int64_t)(intptr_t)a, (int64_t)(intptr_t)b, (int64_t)(intptr_t)c, (int64_t)(intptr_t)d, (int64_t)(intptr_t)e,
                        (int64_t)(intptr_t)f, (int64_t)(intptr_t)g, (int64_t)(intptr_t)h});
    return TbTimeStamp();
}
TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    const int64_t off = static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gGameWorld));
    ev("GetFunnyTimeSinceGameTurn", {off, turn});
    return TbTimeDiff();
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x20004000));
}
int32_t CParkAnalyser::GetTotalParkAttractionPoints() {
    ev("GetTotalParkAttractionPoints", {lab(this)});
    return static_cast<int32_t>(ret("GetTotalParkAttractionPoints"));
}
void CParkAnalyser::GetThreeMostDominantThoughts(uint8_t& first, uint8_t& second, uint8_t& third) const {
    ev("GetThreeMostDominantThoughts", {lab(this)});
    first = static_cast<uint8_t>(ret("thought0"));
    second = static_cast<uint8_t>(ret("thought1"));
    third = static_cast<uint8_t>(ret("thought2"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    if (std::strncmp(format, "Park", 4) == 0) {
        int a = va_arg(ap, int);
        ev("CTRACE", {level, a}, {format});
    } else {
        int a = va_arg(ap, int);
        int b = va_arg(ap, int);
        int c = va_arg(ap, int);
        ev("CTRACE", {level, a, b, c}, {format});
    }
    va_end(ap);
}

extern "C" {

uint64_t tpw_eh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"master", &gMasterSave},   {"mouse", &gMouse},       {"w1e0", &gMouseWord1E0}, {"w1e4", &gMouseWord1E4},
        {"intrface", &gIntrface},   {"world", &gGameWorld},   {"slots", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_eh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_eh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mThingUnderPointer));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

uint32_t tpw_eh_win_ticket_7() { return static_cast<uint32_t>(win_ticket_7()); }
uint32_t tpw_eh_win_ticket_8() { return static_cast<uint32_t>(win_ticket_8()); }
uint32_t tpw_eh_win_ticket_9() { return static_cast<uint32_t>(win_ticket_9()); }
uint32_t tpw_eh_enter_hoarding_mode() { return static_cast<uint32_t>(enter_hoarding_mode()); }
uint32_t tpw_eh_force_ride_breakdown() { return static_cast<uint32_t>(force_ride_breakdown()); }
uint32_t tpw_eh_park_attraction_value() { return static_cast<uint32_t>(park_attraction_value()); }

}  // extern "C"
