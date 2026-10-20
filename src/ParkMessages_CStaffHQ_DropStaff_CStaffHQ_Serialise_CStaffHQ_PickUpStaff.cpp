// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.
#include "Debug.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "World.h"
#include <cstdint>
#include <cstdlib>
#include <new>

// ---- globals the static initialisers store (names invented)
uint32_t gStaffHQSinitHigh;         // 0x1034c6e8
uint32_t gStaffHQSinitLow;          // 0x1034c6ec
uint32_t gThingSinitHigh;           // 0x1034c6f0
uint32_t gThingSinitLow;            // 0x1034c6f4
uint32_t gTrainingCentreSinitHigh;  // 0x1034c6f8
uint32_t gTrainingCentreSinitLow;   // 0x1034c6fc
uint32_t gVector2dSinitHigh;        // 0x1034c700
uint32_t gVector2dSinitLow;         // 0x1034c704
alignas(16) uint8_t gResearchLabString[64];      // 0x1034c4b8
alignas(16) uint8_t gResearchLabStringNode[64];  // 0x1034c4cc

namespace {
const char16_t kResearchLabText[] = u"gms.dat";   // 0x102457e0
}

// 0x1012c100: the id is the high halfword of r4, stored through the stack and reloaded.
void CStaffHQ::PickUpStaff(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) { mF60 = staff.mIndex; }

// 0x1012be24: element at 0x24 + 0xc * type, second word.
void CStaffHQ::StopStrikeInProgress(StaffType type) { mStrikes[static_cast<int32_t>(type)].mF04 = 0; }

// 0x1012bde0
int32_t CStaffHQ::IsStriking(StaffType type) { return static_cast<int32_t>(mStrikes[static_cast<int32_t>(type)].mF04); }

void CStaffHQ::FinalDestroy() { mBase.FinalDestroy(); }

void __sinit_StaffHQ_cpp() {
    gStaffHQSinitLow = 0;
    gStaffHQSinitHigh = 0x10000;
}

void CTagSystem::FinalDestroy() { mBase.FinalDestroy(); }

int32_t CTagSystem::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return mBase.Serialise(file, save, version, unused) != 0 ? 1 : 0;
}

void __sinit_Thing_cpp() {
    gThingSinitLow = 0;
    gThingSinitHigh = 0x10000;
}

void EventHistType_Ctor(CThoughts::EventHistType*) {}   // 0x1012ed48: blr

// 0x1012ed84: the script handle is the word at +0x84 of the bytes the peep holds.
void CThoughts::FinalDestroy() {
    int32_t handle = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x84);
    if (handle != 0) SCR_KillScript(handle);
}

void __sinit_TrainingCentre_cpp() {
    gTrainingCentreSinitLow = 0;
    gTrainingCentreSinitHigh = 0x10000;
}

void CUIMsgReceiver::FinalDestroy() { mBase.FinalDestroy(); }

int32_t CUIMsgReceiver::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return mBase.Serialise(file, save, version, unused) != 0 ? 1 : 0;
}

Vector2d* Vector2d_Dtor(Vector2d* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

void __sinit_vector2d_cpp() {
    gVector2dSinitLow = 0;
    gVector2dSinitHigh = 0x10000;
}

void __sinit_ResearchLab_cpp() {
    void* str = ::new (gResearchLabString) TbDynamicStringTemplate<char16_t>(kResearchLabText);
    __register_global_object(str, reinterpret_cast<void*>(&UIMain_DestroyWString), gResearchLabStringNode);
}

int32_t CMsgResearchCategoryUpdated_GetType(const CMsgResearchCategoryUpdated*) { return 0x12; }

CMsgResearchCategoryUpdated* CMsgResearchCategoryUpdated_Dtor(CMsgResearchCategoryUpdated* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Follow_Path* Follow_Path_Dtor(Follow_Path* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Avoid_Walls* Avoid_Walls_Dtor(Avoid_Walls* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Repel_Obstacle* Repel_Obstacle_Dtor(Repel_Obstacle* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Avoid_Obstacle* Avoid_Obstacle_Dtor(Avoid_Obstacle* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}

Separation* Separation_Dtor(Separation* self, int16_t flag) {
    if (self != nullptr && flag > 0) v51052_callee::OperatorDelete(self);
    return self;
}
