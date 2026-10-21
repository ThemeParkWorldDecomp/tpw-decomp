// rsseHelpers.h: file-local helpers that src/game/rides/rsse.cpp shares with World.cpp, Thoughts.cpp and
// RideEffects.cpp. Before the regroup, the functions using them shared a source file (2 files in all), which had one
// copy of each; that copy now lives here, static inline, so each file that includes this header keeps its own internal
// copy.
#pragma once

#include "ThingReceivers.h"
#include "World.h"

// A person (kind 1, 0x22c bytes). CPerson has no members on the host, so the constructor's stores go by offset: the thoughts at +0x30, the pilot
// at +0xd4, seven floats, the two arrays of four thing ids (their constructor, 0x10116ccc, is a bare return: nothing is written) and one float.
static inline void InitPerson(void* block) {
    unsigned char* const bytes = static_cast<unsigned char*>(block);
    new (bytes + 0x30) CThoughts;   // 0x1012ec88
    new (bytes + 0xd4) Pilot;   // 0x10134610
    new (bytes + 0x19c) float(0.0f);
    new (bytes + 0x1a4) float(0.0f);
    new (bytes + 0x1a8) float(0.0f);
    new (bytes + 0x1ac) float(0.0f);
    new (bytes + 0x1b0) float(0.0f);
    new (bytes + 0x1b4) float(0.0f);
    new (bytes + 0x1b8) float(0.0f);
    new (bytes + 0x20c) float(0.0f);
}

// The five staff classes (kinds 4 to 8) share the offsets (CStaff carries the layout): the thoughts at +0x30, the pilot at +0xd4, the hiring
// time (a time stamp of two words, the second one first) and the happiness and energy floats.
static inline void InitStaff(void* block) {
    CStaff* const staff = static_cast<CStaff*>(block);
    new (staff->Thoughts()) CThoughts;   // 0x1012ec88
    new (staff->mVehicleA) Pilot;   // 0x10134610
    staff->mTimeHired[1] = 0;
    staff->mTimeHired[0] = 0;
    staff->mHappiness = 0.0f;
    staff->mEnergy = 0.0f;
}

// An object (kind 3, 0x448 bytes): the build time, the three condition floats and the six histories (the rings are not written).
static inline void InitObject(void* block) {
    CObject* const object = static_cast<CObject*>(block);
    object->mBuilt[1] = 0;
    object->mBuilt[0] = 0;
    object->mF40 = 0.0f;
    object->mF44 = 0.0f;
    object->mF48 = 0.0f;
    new (&object->mHistory6C) CHistory<uint32_t, 30>;   // 0x10111514
    new (&object->mHistoryF4) CHistory<uint32_t, 30>;
    new (&object->mHistory1A4) CHistory<uint32_t, 30>;
    new (&object->mHistory22C) CHistory<uint32_t, 30>;
    new (&object->mHistory2B4) CHistory<uint32_t, 30>;
    new (&object->mHistory33C) CHistory<int32_t, 30>;   // 0x10110fa0
}

static inline float ReadFloat(const uint8_t* base, uint32_t offset) {
    float value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}
