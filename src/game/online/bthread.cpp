// bthread.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "Debug.h"
#include "email.h"
#include "ESP_sprites.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "GameSys.h"
#include "LbDrawPrimitives.h"
#include "LbSCBack.h"
#include "lobby.h"
#include "m3d2.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "rsse.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "UI_pump.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.

uint32_t bthread_create(void* (*start)(void*), void* arg) {
    uint32_t id;
    NS_MacDoze::_beginthreadex(nullptr, 0, start, arg, 0, &id);
    return id;
}

// email.cpp range of the main app (SimTheme Park 2.04b3): the player's outbox and address book constructors, and the bullfrog
// thread library's mutex (the inventory files bthread_mutex here; UI_pump.h declares the class). PARTIAL: the outbox's destructor and the
// two loaders are in src/email_load.cpp (unit w90); the rest of the file is untranslated.

// The critical section's handle is the mutex object itself. The Mac's functions also load 0 into r3 afterwards.
void bthread_mutex::lock() { NS_MacDoze::EnterCriticalSection(&mHandle); }

void bthread_mutex::unlock() { NS_MacDoze::LeaveCriticalSection(&mHandle); }

// Unit v5962: the embedded-entry counters (0x101fff0c, 0x101ffe74), EnableRideHead (0x10200230), bthread_cond (0x10200e34, 0x10200ea8,
// 0x10200f30) and CalculateOrientFromTrackMorph (0x101fec54).

// 0x10200e34
bthread_cond::bthread_cond() { mHandle = NS_MacDoze::CreateEvent(nullptr, 0, 0, nullptr); }

// 0x10200ea8
bthread_cond* bthread_cond::Destroy(bthread_cond* self, int16_t flag) {
    if (self != nullptr) {
        NS_MacDoze::CloseHandle(self->mHandle);
        if (flag > 0) bthread_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10200f30
bool bthread_cond::wait(int32_t milliseconds) {
    uint32_t result;
    if (milliseconds < 0) {
        result = NS_MacDoze::WaitForSingleObject(mHandle, 0xffffffffu);
    } else {
        result = NS_MacDoze::WaitForSingleObject(mHandle, static_cast<uint32_t>(milliseconds));
    }
    return result == 0;
}

bthread_mutex* bthread_mutex::Construct(bthread_mutex* self) {
    NS_MacDoze::InitializeCriticalSection(reinterpret_cast<void**>(self));
    return self;
}

int32_t bthread_cond::signal() {
    NS_MacDoze::SetEvent(mHandle);
    return 0;
}

namespace argdeque_callee {
void VectorDtor(void* vec, int16_t flag) {
    if (vec) {
        online_callee::ArgumentVectorTearDown(vec);
        if (flag > 0) ftp_callee::OperatorDelete(vec);
    }
}
}  // namespace argdeque_callee

void* operator_new(uint32_t size, void* where) { return where; }

Connection* Connection::Destroy(Connection* self, int16_t flag) {
    if (self) {
        self->mF00 = 0x102b1610u;
        ftp_callee::CSockDtor(reinterpret_cast<FTPClient*>(self), 0);
        if (flag > 0) ftp_callee::OperatorDelete(self);
    }
    return self;
}
