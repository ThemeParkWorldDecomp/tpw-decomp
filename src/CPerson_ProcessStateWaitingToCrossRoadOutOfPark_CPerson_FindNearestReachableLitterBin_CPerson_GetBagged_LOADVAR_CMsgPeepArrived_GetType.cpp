// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).
#include "ThingReceivers.h"
#include "Serialise.h"
#include "VehicleSteer.h"
#include "MainBalance.h"

namespace {
inline uint8_t* Raw(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
template <class T> inline T& Fld(uint8_t* base, uint32_t offset) { return *reinterpret_cast<T*>(base + offset); }
}  // namespace

// The initialised words of the five static-initialiser functions (BSS on the Mac, set by the function).
int32_t gPersonSinitA;       // 0x1034c4a0
int32_t gPersonSinitB;       // 0x1034c4a4
int32_t gPersonQueueSinitA;  // 0x1034c4a8
int32_t gPersonQueueSinitB;  // 0x1034c4ac
int32_t gMechanicHQSinitA;   // 0x1034c2d0
int32_t gMechanicHQSinitB;   // 0x1034c2d4
int32_t gVehicleCallbacksSinitA;  // 0x1034caa0
int32_t gVehicleCallbacksSinitB;  // 0x1034caa4
int32_t gGuardSinitA;        // 0x1034c204
int32_t gGuardSinitB;        // 0x1034c208

// 0x1011b794 is CMsgPeepArrived::GetType (inline in the header, answers 0x1c); 0x10120b90 is CMsgPrankery::GetType (inline, 0xe).

// 0x101521e4: the flags are cleared and the person's id (the high halfword of the register on the Mac) is stored. The vtable stores are not modelled.
CMsgPeepArrived::CMsgPeepArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> peep) {
    mFlags = 0;
    mThingId = peep.mIndex;
}

// 0x10116c54: the vtable store is not modelled; the deleting form (flag above 0 as a signed halfword) deletes the object.
CMsgPeepArrived* CMsgPeepArrived_Dtor(CMsgPeepArrived* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

// 0x1011c5c4
CMsgPrankery* CMsgPrankery_Dtor(CMsgPrankery* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

void __sinit_Person_cpp() {
    gPersonSinitA = 0x10000;
    gPersonSinitB = 0;
}

void __sinit_PersonQueue_cpp() {
    gPersonQueueSinitA = 0x10000;
    gPersonQueueSinitB = 0;
}

void __sinit_MechanicHQ_cpp() {
    gMechanicHQSinitA = 0x10000;
    gMechanicHQSinitB = 0;
}

void __sinit_VehicleCallbacks_cpp() {
    gVehicleCallbacksSinitA = 0x10000;
    gVehicleCallbacksSinitB = 0;
}

void __sinit_guard_cpp() {
    gGuardSinitA = 0x10000;
    gGuardSinitB = 0;
}

// 0x1011b4b4: a byte of the balance row (12 bytes a row from Mac offset 0xa4) of the person's +0x1f0 byte. The index is not checked.
uint8_t CPerson::GetPreferredExcitement() const {
    uint32_t row = Fld<uint8_t>(Raw(this), 0x1f0);
    return static_cast<uint8_t>(gMainBalance.mValues[(0xa4 - 8) / 4 + 3 * row].mInteger);
}

// 0x10119ac8
void CPerson::AddRideToRecentTemporaryHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) {
    uint8_t* self = Raw(this);
    for (int32_t i = 3; i > 0; i--) Fld<uint16_t>(self, 0x1e8 + i * 2) = Fld<uint16_t>(self, 0x1e6 + i * 2);
    Fld<uint16_t>(self, 0x1e8) = ride.mIndex;
}

// 0x10119a30
void CPerson::AddRideToRecentHistory(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) {
    uint8_t* self = Raw(this);
    for (int32_t i = 3; i > 0; i--) Fld<uint16_t>(self, 0x1e0 + i * 2) = Fld<uint16_t>(self, 0x1de + i * 2);
    Fld<uint16_t>(self, 0x1e0) = ride.mIndex;
}

// 0x1011df70
void CPerson::ForceRideAdmittance() { ProcessStateWaitingForScriptToAdmitMe(); }

// 0x10117f80: back to the state saved at +0x224.
void CPerson::EndSpotAnim() { SetState(static_cast<PersonState>(Fld<int32_t>(Raw(this), 0x224))); }

// 0x1011f0b8: in state 0xb at the front of the queue (+0x1f1 is 0).
bool CPerson::ReadyForAdmitting() const {
    return Fld<int32_t>(Raw(this), 0x220) == 0xb && Fld<uint8_t>(Raw(this), 0x1f1) == 0;
}

// 0x10120b2c: state 8 looks at the saved state (+0x224); states 0xb to 0xe count.
int32_t CPerson::InQueueingState() const {
    int32_t state = Fld<int32_t>(Raw(this), 0x220);
    if (state == 8) state = Fld<int32_t>(Raw(this), 0x224);
    return (state >= 0xb && state < 0xf) ? 1 : 0;
}

// 0x1011524c
void CPeep::SetStandScript() {
    reinterpret_cast<CDelayedScript*>(Raw(this) + 0xc)->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3));
}

// 0x10113814: answers the old halfword at +0xcc and stores the cell the peep stands on (x byte + y byte * 128 + 1).
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CPeep::LastCellVisited() {
    uint8_t* self = Raw(this);
    uint16_t old = Fld<uint16_t>(self, 0xcc);
    Fld<uint16_t>(self, 0xcc) = static_cast<uint16_t>(Fld<uint8_t>(self, 4) + (Fld<uint8_t>(self, 6) * 128 + 1));
    return tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{old};
}

// 0x101052f4
void CMechanicHQ::FinalDestroy() { mBase.FinalDestroy(); }

// 0x10135474, 0x101354f0, 0x10135568: the vtable stores are not modelled.
Arrival* Arrival_Dtor(Arrival* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

Seek* Seek_Dtor(Seek* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

Behaviour* Behaviour_Dtor(Behaviour* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

// 0x1013567c
void BehaviourEntry_Ctor(BehaviourEntry* self) {
    self->mWeight = 0;
    self->mBehaviour = nullptr;
}
