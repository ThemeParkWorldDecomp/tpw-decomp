// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.
#include <cstddef>
#include <cstdint>
#include <new>

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

// ---- the animation time controls (0x38 bytes each)

void UpdateRideAnimSpeedAllChannels(tag_sRideAnimSet* set, float speed) {
    for (uint32_t i = 0; i < set->mTimeCount; i++) static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF30 = speed;
}

void UpdateRideAnimSpeedImmediate(tag_sRideAnimSet* set, float speed, int32_t flag) {
    static_cast<sAnimTimeControl*>(set->mTimeControls)[static_cast<uint32_t>(flag)].mF0C = speed;
}

// The answer is the time control's flags word (r3 is loaded from it last).
uint32_t GetCurrentAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    const sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[control];
    if (kind != nullptr) *kind = time.mF04;
    if (index != nullptr) *index = time.mF08;
    return time.mFlags;
}

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) {
    sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[flag];
    if (time.mF24 != 0xc) time.mF24 = 0xc;
}

float GetAnimRemainingMilliseconds(sAnimTimeControl* control) {
    const float kMillisecondsPerFrame = 33.333332f;   // 0x10264e50: 0x42055555
    float left = control->mF1C - control->mF20;
    return kMillisecondsPerFrame * left;
}

uint32_t GetRideAnimTime(int32_t which) {
    if (which != 0) return gTPGlobals.mF4018;
    return gTPGlobals.mF4010;
}

void SetRideAnimTime(uint32_t time) { gTPGlobals.mF4010 = time; }

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
using N = TbDynamicStringTemplate<char>;
const char16_t kIslandW[] = u"upload\0gms.dat";   // 0x1023e9f0 (the second string starts at 7)
const char16_t kScreenW[] = u"upload";             // 0x1023eb56
const char16_t kTicketW[] = u"gms.dat";            // 0x102417e4
const char16_t kRideW[] = u"gms.dat";              // 0x1023d8e8
}  // namespace

alignas(16) unsigned char gV51077IslandW4938[sizeof(W)];   // 0x102e4938
unsigned char gV51077IslandNode494c[16];                    // 0x102e494c
alignas(16) unsigned char gV51077IslandW4918[sizeof(W)];   // 0x102e4918
unsigned char gV51077IslandNode492c[16];                    // 0x102e492c
alignas(16) unsigned char gV51077IslandN48f8[sizeof(N)];   // 0x102e48f8
unsigned char gV51077IslandNode490c[16];                    // 0x102e490c
alignas(16) unsigned char gV51077ScreenW49bc[sizeof(W)];   // 0x102e49bc
unsigned char gV51077ScreenNode49d0[16];                    // 0x102e49d0
alignas(16) unsigned char gV51077GlobeW48d8[sizeof(W)];    // 0x102e48d8
unsigned char gV51077GlobeNode48ec[16];                     // 0x102e48ec
alignas(16) unsigned char gV51077TicketWc1e4[sizeof(W)];   // 0x1034c1e4
unsigned char gV51077TicketNodec1f8[16];                    // 0x1034c1f8
alignas(16) unsigned char gV51077RideW1edc[sizeof(W)];     // 0x102d1edc
unsigned char gV51077RideNode1ef0[16];                      // 0x102d1ef0
uint32_t gV51077Debug5944;                                  // 0x102da944
uint32_t gV51077Debug5948;                                  // 0x102da948

void __sinit_lobby_screen_cpp() {
    void* object = new (static_cast<void*>(gV51077ScreenW49bc)) W(kScreenW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077ScreenNode49d0);
}

void __sinit_lobby_island_cpp() {
    void* wide1 = new (static_cast<void*>(gV51077IslandW4938)) W(kIslandW);
    __register_global_object(wide1, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077IslandNode494c);
    void* wide2 = new (static_cast<void*>(gV51077IslandW4918)) W(kIslandW + 7);
    __register_global_object(wide2, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077IslandNode492c);
    void* narrow = new (static_cast<void*>(gV51077IslandN48f8)) N("");
    __register_global_object(narrow, reinterpret_cast<void*>(&UIMain_DestroyString), gV51077IslandNode490c);
}

void __sinit_lobby_globe_cpp() {
    void* object = new (static_cast<void*>(gV51077GlobeW48d8)) W(gGlobeNoCityName + 11);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077GlobeNode48ec);
}

void __sinit_GoldenTickets_cpp() {
    void* object = new (static_cast<void*>(gV51077TicketWc1e4)) W(kTicketW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077TicketNodec1f8);
}

void __sinit_TPRide_cpp() {
    void* object = new (static_cast<void*>(gV51077RideW1edc)) W(kRideW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077RideNode1ef0);
}

void __sinit_BlockSave_cpp() {}

void __sinit_Debug_cpp() {
    gV51077Debug5948 = 0;
    gV51077Debug5944 = 0x10000;
}

// ---- the lobby callback, the fly accessors, Debug.cpp, the ride pointers, the keyboard actions

void IslandOnlineParkInfoPanelCancelCallback() { ShowCityLobbyController(); }

int32_t FLY_SetRunway(int32_t fly, int32_t a) {
    if (fly <= 0 || fly >= 100) return fly;
    void* base = gFlyBases[fly];
    if (base == nullptr) return fly;
    *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + 0x44) = a;
    return fly;   // the original leaves the base pointer in r3 here; nobody reads the answer
}

int32_t FLY_GetCarInBase(int32_t fly) {
    if (fly > 0 && fly < 100) {
        void* base = gFlyBases[fly];
        if (base != nullptr) return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + 0x3c);
    }
    return 0;
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) {}

int32_t abort_coaster_edit() {
    gUIInterface.EndCoasterEdit();
    return 1;
}

int32_t slow_down_time() {
    gClock.mTime.mPausableTime.mVariableTime.DecreaseTimeRate();
    return 1;
}

int32_t speed_up_time() {
    gClock.mTime.mPausableTime.mVariableTime.IncreaseTimeRate();
    return 1;
}
