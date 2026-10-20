// Test library for the key actions start_strike, clear_patrol_area, wheres_my_script and abort_all_requesters (unit v5341;
// libtpw_unit_clear_patrol_area.dylib, tests/test_clear_patrol_area.py), main app:
//   src/turn_object_counterclockwise_clear_patrol_area.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals
// of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "DateMessages.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "keytable_actions.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMessageCentre* gMessageCentre;
CWorld* gGameWorld;
UIInterface gUIInterface;
CIntrface gIntrface;

namespace {

float BitsFloat(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

// The mouse mode the test hands to gMouseMode: GetMode is the Mac's virtual call through the vtable slot +0x2c.
class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

// ---- the fakes: the same event name and arguments as the module's PowerPC stub
CThing* UIInterface::GetSelectedPeep() {
    ev("GetSelectedPeep", {lab(this)});
    return reinterpret_cast<CThing*>(static_cast<uintptr_t>(ret("peep")));
}

int32_t UIInterface::RemoveLocators() {
    ev("RemoveLocators", {lab(this)});
    return static_cast<int32_t>(ret("RemoveLocators"));
}

void CStaff::CancelPatrolArea() { ev("CancelPatrolArea", {lab(this)}); }

CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(ret("hq")));
}

void CStaffHQ::ForceStrike(StaffType type) { ev("ForceStrike", {lab(this), static_cast<int32_t>(type)}); }

CSubscriptionHeader::CSubscriptionHeader() { ev("hdr_ctor", {}); }
CSubscriptionHeader::~CSubscriptionHeader() {}

CMsgEndOfMonth::CMsgEndOfMonth() {
    ev("MsgEOM", {lab(this)});
    mFlags = 0x55;
}
CMsgEndOfMonth::~CMsgEndOfMonth() {}
int32_t CMsgEndOfMonth::GetType() const { return 0xc; }

int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), lab(&message), static_cast<int64_t>(message.mFlags)});
    return 0;
}

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
// The vtable of CMMDefault is emitted with its key function (the destructor): empty bodies, never called by the unit.
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, static_cast<int64_t>(flag ? 1 : 0)}); }

void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t* mode) {
    ev("SCR_GetScriptPosition", {static_cast<uint32_t>(handle)});
    *x = BitsFloat(ret("px"));
    *y = BitsFloat(ret("py"));
    *z = BitsFloat(ret("pz"));
    *mode = static_cast<int32_t>(ret("pm"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    double d[3];
    for (double& v : d) v = va_arg(args, double);
    int32_t mode = va_arg(args, int32_t);
    va_end(args);
    int64_t sum = 0;
    for (size_t i = 0; format[i]; i++) sum += static_cast<int64_t>(static_cast<unsigned char>(format[i])) * static_cast<int64_t>(i + 1);
    int64_t w[6];
    for (int i = 0; i < 3; i++) {
        uint64_t bits;
        std::memcpy(&bits, &d[i], 8);
        w[2 * i] = static_cast<int64_t>(bits >> 32);
        w[2 * i + 1] = static_cast<int64_t>(bits & 0xffffffffu);
    }
    ev("CTRACE", {level, sum, w[0], w[1], w[2], w[3], w[4], w[5], static_cast<uint32_t>(mode)});
}

extern "C" {

uint64_t tpw_cpa_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ui", &gUIInterface},     {"intr", &gIntrface.mPointedThing}, {"mw0", &gMouseWord1E0}, {"mw1", &gMouseWord1E4},
        {"mmode", &gMouseMode},    {"mouse", &gMouse},                 {"world", &gGameWorld},  {"centre", &gMessageCentre},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cpa_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A mouse mode whose GetMode answers from the queue "GetMode" (32-bit addressable).
uint64_t tpw_cpa_fakemode() {
    void* p = tpw::ArenaAllocZeroed(sizeof(FakeMode));
    new (p) FakeMode();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

// The globals are pointers on the host: a case writes their low halves only, so each case starts from zero.
void tpw_cpa_reset() {
    gMouse = nullptr;
    gMouseWord1E0 = 0;
    gMouseWord1E4 = 0;
    gMouseMode = nullptr;
    gMessageCentre = nullptr;
    gGameWorld = nullptr;
    gIntrface.mPointedThing = nullptr;
}

int32_t tpw_cpa_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CSubscriptionHeader));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    return n;
}

uint32_t tpw_cpa_clear_patrol_area() { return static_cast<uint32_t>(clear_patrol_area()); }
uint32_t tpw_cpa_wheres_my_script() { return static_cast<uint32_t>(wheres_my_script()); }
uint32_t tpw_cpa_start_strike() { return static_cast<uint32_t>(start_strike()); }
uint32_t tpw_cpa_abort_all_requesters() { return static_cast<uint32_t>(abort_all_requesters()); }

}  // extern "C"
