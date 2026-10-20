// Test library for the ride animation leaves, the fly accessors, the ride pointers, Debug.cpp, the keyboard actions and the static
// initialisers (unit v51077; libtpw_unit_UpdateRideAnimSpeedAllChannels.dylib, tests/test_UpdateRideAnimSpeedAllChannels.py), main app:
//   src/advisor_leaves_UpdateRideAnimSpeedAllChannels.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read: the two string constructors (the object and a
// hash of the text), __register_global_object (the object, which destructor, the node), ShowCityLobbyController, UIInterface::EndCoasterEdit
// and CVariableTime::IncreaseTimeRate / DecreaseTimeRate. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "rsse.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "FS_Online.h"
#include "HLSprite.h"
#include "TPCoaster.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "UIInterface.h"
#include "keytable_actions.h"
#include "lobby.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
TPGlobals gTPGlobals;
void* gFlyBases[100];
CClock gClock;
UIInterface gUIInterface;
char16_t gGlobeNoCityName[] = u"North Pole\0upload";   // 0x1023e91e: the name, then the string the globe constructor gets (at 0x16)

// ---- the unit's own file-statics (defined in the src file)
extern unsigned char gV51077IslandW4938[], gV51077IslandNode494c[], gV51077IslandW4918[], gV51077IslandNode492c[], gV51077IslandN48f8[],
    gV51077IslandNode490c[], gV51077ScreenW49bc[], gV51077ScreenNode49d0[], gV51077GlobeW48d8[], gV51077GlobeNode48ec[], gV51077TicketWc1e4[],
    gV51077TicketNodec1f8[], gV51077RideW1edc[], gV51077RideNode1ef0[];
extern uint32_t gV51077Debug5944, gV51077Debug5948;
void __sinit_lobby_screen_cpp();
void __sinit_lobby_island_cpp();
void __sinit_lobby_globe_cpp();
void __sinit_GoldenTickets_cpp();
void __sinit_TPRide_cpp();
void __sinit_BlockSave_cpp();
void __sinit_Debug_cpp();

// ---- the destructor glue the sinit functions register (only their addresses matter)
void UIMain_DestroyWString(void*) {}
void UIMain_DestroyString(void*) {}

namespace {
int64_t TextHash(const char16_t* text, int64_t* length) {
    uint32_t h = 2166136261u;
    int64_t n = 0;
    while (n < 64 && text[n] != 0) {
        h = (h ^ static_cast<uint32_t>(text[n])) * 16777619u;
        n++;
    }
    *length = n;
    return h;
}
int64_t TextHash(const char* text, int64_t* length) {
    uint32_t h = 2166136261u;
    int64_t n = 0;
    while (n < 64 && text[n] != 0) {
        h = (h ^ static_cast<uint32_t>(static_cast<uint8_t>(text[n]))) * 16777619u;
        n++;
    }
    *length = n;
    return h;
}
}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    int64_t n;
    int64_t h = TextHash(text, &n);
    ev("ctorW", {lab(this), n, h});
}
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    int64_t n;
    int64_t h = TextHash(text, &n);
    ev("ctorN", {lab(this), n, h});
}

// The vtables the constructors above store need these (never called).
#define TPW_STRING_VIRTUALS(C)                                                                                                         \
    template <>                                                                                                                        \
    void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                                                          \
    template <>                                                                                                                        \
    void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                                                             \
    template <>                                                                                                                        \
    void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                                                          \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}                                                                          \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }                        \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate<C>&) { return *this; }             \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }                                      \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }                                      \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }                       \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }                                     \
    template <>                                                                                                                        \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}   // gTPGlobals's construction (silent)
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {}
TPW_STRING_VIRTUALS(char16_t)
TPW_STRING_VIRTUALS(char)

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    int64_t which = 0;
    if (destructor == reinterpret_cast<void*>(&UIMain_DestroyWString)) which = 1;
    if (destructor == reinterpret_cast<void*>(&UIMain_DestroyString)) which = 2;
    ev("register", {lab(object), which, lab(node)});
}

TPGlobals::~TPGlobals() {}
TPGlobals::TPGlobals() {}  // gTPGlobals's construction only; the test writes the words it reads
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}   // gClock's construction only; nothing is read from it
void ShowCityLobbyController() { ev("ShowCityLobbyController", {}); }
void UIInterface::EndCoasterEdit() { ev("EndCoasterEdit", {lab(this)}); }
void CVariableTime::IncreaseTimeRate() { ev("IncreaseTimeRate", {lab(this)}); }
void CVariableTime::DecreaseTimeRate() { ev("DecreaseTimeRate", {lab(this)}); }

extern "C" {

uint64_t tpw_ra_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"clock", &gClock},
        {"ui", &gUIInterface},
        {"tp", &gTPGlobals},
        {"fly", gFlyBases},
        {"dbgA", &gV51077Debug5944},
        {"dbgB", &gV51077Debug5948},
        {"iw1", gV51077IslandW4938},
        {"in1", gV51077IslandNode494c},
        {"iw2", gV51077IslandW4918},
        {"in2", gV51077IslandNode492c},
        {"ino", gV51077IslandN48f8},
        {"in3", gV51077IslandNode490c},
        {"sw", gV51077ScreenW49bc},
        {"sn", gV51077ScreenNode49d0},
        {"gw", gV51077GlobeW48d8},
        {"gn", gV51077GlobeNode48ec},
        {"tw", gV51077TicketWc1e4},
        {"tn", gV51077TicketNodec1f8},
        {"rw", gV51077RideW1edc},
        {"rn", gV51077RideNode1ef0},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ra_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ra_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mTimeControls));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF04));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF08));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF20));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF24));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF30));
    return n;
}

// ---- the functions
void tpw_ra_all(uint64_t set, float speed) { UpdateRideAnimSpeedAllChannels(reinterpret_cast<tag_sRideAnimSet*>(set), speed); }
void tpw_ra_imm(uint64_t set, float speed, int32_t index) {
    UpdateRideAnimSpeedImmediate(reinterpret_cast<tag_sRideAnimSet*>(set), speed, index);
}
void tpw_ra_cur(uint64_t set, uint64_t kind, uint64_t index, uint32_t control) {
    GetCurrentAnim(reinterpret_cast<tag_sRideAnimSet*>(set), reinterpret_cast<uint32_t*>(kind), reinterpret_cast<uint32_t*>(index), control);
}
void tpw_ra_flush(uint64_t set, uint32_t control) { FlushDeferredAnim(reinterpret_cast<tag_sRideAnimSet*>(set), control); }
float tpw_ra_remain(uint64_t control) { return GetAnimRemainingMilliseconds(reinterpret_cast<sAnimTimeControl*>(control)); }
uint32_t tpw_ra_gettime(int32_t which) { return GetRideAnimTime(which); }
void tpw_ra_settime(uint32_t time) { SetRideAnimTime(time); }
void tpw_ra_sinit(int32_t which) {
    switch (which) {
        case 0: __sinit_lobby_screen_cpp(); break;
        case 1: __sinit_lobby_island_cpp(); break;
        case 2: __sinit_lobby_globe_cpp(); break;
        case 3: __sinit_GoldenTickets_cpp(); break;
        case 4: __sinit_TPRide_cpp(); break;
        case 5: __sinit_BlockSave_cpp(); break;
        default: __sinit_Debug_cpp(); break;
    }
}
void tpw_ra_cb() { IslandOnlineParkInfoPanelCancelCallback(); }
void tpw_ra_flyset(int32_t fly, int32_t a) { FLY_SetRunway(fly, a); }
int32_t tpw_ra_flyget(int32_t fly) { return FLY_GetCarInBase(fly); }
void tpw_ra_dbg() { DEBUG_MESSAGE("text", 0); }
int32_t tpw_ra_abort() { return abort_coaster_edit(); }
int32_t tpw_ra_slow() { return slow_down_time(); }
int32_t tpw_ra_speed() { return speed_up_time(); }

}  // extern "C"
