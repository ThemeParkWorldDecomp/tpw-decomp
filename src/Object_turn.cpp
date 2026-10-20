// CObject, main app, unit w132 (the Mac files them under Object.cpp): the ride object's per-turn work UpdateTurn (0x1010d634, 684 bytes) and
// ModelState (0x1010daa0, 456 bytes), the ride script's creation CreateRideScript (0x101076b0, 328 bytes) and the engine ride's creation or
// restoring UpdateEngine (0x10107d38, 492 bytes). The ride's wear, breakdown, queue and state handlers, the script and ride instance
// makers are untranslated (the headers declare them; the launcher answers zero).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "ActionRecorder.h"
#include "Debug.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

// The two CPerson members ModelState calls, against their Mac names as src/Object.cpp does (CPerson is a placeholder class).
void ObjectForceRideAdmittance(CPerson*) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");   // 0x1011df70
void ObjectDismissFromQueue(CPerson*) __asm__("__ZN7CPerson16DismissFromQueueEv");         // 0x1011f1b8

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x3cd of the string pool at 0x1024294c

uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint32_t LowByte(float value) {
    return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xff;
}

constexpr uint16_t kNoThing = 0;   // the halfword at 0x102a0be2 and 0x102a0be4, both 0
}  // namespace

// Three jobs while the ride's state is not 3 or 4 (condemned rides do none of them, the fireworks apart):
// 1. A fireworks ride whose script variable 0 is 1 takes its map effect out of the region and sets the variable to 2.
// 2. Every eighth turn (the world's turn count), while the ride runs (mF64) and is not broken down: wears the ride out, and when the wear
//    shows (the low byte of the condition or of the ride quality is 0), it is not the relief ride and the script's variable 4 is 0: a
//    condition of 0 breaks the ride, else the condition goes down by 5 (clamped to 0 to 100; a NaN takes the plain subtraction) and the
//    ride breaks.
// 3. A ride with flag 1 whose quality's low byte is below 25 is marked worn.
// Last, a queued person who is gone or not queueing any more is forgotten.
void CObject::UpdateTurn() {
    if ((mFlags & 0x80) != 0) {
        RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        if (RSSE_Getvar(script, 0) == 1) {
            const uint16_t cell = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
            gWorldMap->RemoveRegionalEffector(kRegionEffectorFireworks, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 2);
        }
    }
    if (mState == 3 || mState == 4) return;
    if (mState != 1 && mF64 != 0 && (gWorld->mTurn & 7) == 0) {
        RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        AddWearAndTear();
        if ((LowByte(mF44) == 0 || LowByte(mF40) == 0) && (mFlags & 1) == 0 && RSSE_Getvar(script, 4) == 0) {
            const float condition = mF44;
            if (LowByte(condition) == 0) {
                RequestBreakdown();
            } else {
                const float lowered = -5.0f + condition;
                if (!(lowered > 100.0f)) {
                    if (!(lowered < 0.0f)) {
                        mF44 = mF44 + -5.0f;
                    } else {
                        mF44 = 0.0f;
                    }
                } else {
                    mF44 = 100.0f;
                }
                RequestBreakdown();
            }
        }
    }
    if ((mFlags & 1) != 0) {
        const float quality = static_cast<float>(LowByte(mF40));
        if (quality < 25.0f) SetWornFlag();
    }
    if (mQueuedPerson != kNoThing) {
        const uint16_t queued = mQueuedPerson;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[queued].mThing);
        if (person == nullptr || person->InQueueingState() == 0) mQueuedPerson = 0;
    }
}

// State 0 runs the ride's normal handler; 1, 2 and 4 (broken down, wants an upgrade, condemned) look at the script's variable 0: when it is not
// the queued person and that person (a thing in state 0xe) has not noticed he was admitted, he is forced onto the ride; else a queued person is
// taken out of the queue and dismissed. Either way the ride then lets its people off. State 3 does nothing; any other state asserts.
void CObject::ModelState() {
    if (mState == 3) return;
    if (mState == 0) {
        ProcessStateFunctioning();
        return;
    }
    if (mState < 0 || mState >= 5) {
        GAME_ASSERTMSG(0, "Unknown state in CObject::ModelState\n");
        return;
    }
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    const uint32_t variable = static_cast<uint32_t>(RSSE_Getvar(script, 0));
    const uint16_t queued = mQueuedPerson;
    bool dismiss = true;
    if (variable != queued) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[queued].mThing);
        if (WordAt(person, 0x220) == 0xe) {
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> personId = reinterpret_cast<CThing*>(person)->GetThingId();
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> rideId = mBase.GetThingId();
            CTRACE(2, "Object %d: script admitted person %d but he doesn't know yet, forcing him onto ride\n", rideId.mIndex, personId.mIndex);
            ObjectForceRideAdmittance(person);
            dismiss = false;
        }
    }
    if (dismiss && mQueuedPerson != kNoThing) {
        const uint16_t again = mQueuedPerson;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPerson* person = static_cast<CPerson*>(gThingArray[again].mThing);
        RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{mQueuedPerson});
        ObjectDismissFromQueue(person);
    }
    LetPeopleOff();
}

// Looks for the script "<ride folder>:<ride name>.rse" (a file name of the ride record); when it is not found says so (level 0xc) and takes the
// placeholder "Data:TestScript:test.rse". Starts the script with the ride's thing id (in the HIGH halfword of the word) and with 1 when the
// action recorder is recording, keeps its handle and tells the ride instance. The Mac's name is a wide string, widened to wchar_t for %ls here.
void CObject::CreateRideScript() {
    const char16_t* fileName = GetFileName();
    RideInfo* info = GetDBInfo();
    std::wstring wide;
    if (fileName != nullptr) {
        for (const char16_t* c = fileName; *c != 0; ++c) wide.push_back(static_cast<wchar_t>(*c));
    }
    char path[0x404];
    std::snprintf(path, sizeof(path), "%s:%ls%s", info->mFolder, fileName != nullptr ? wide.c_str() : nullptr, ".rse");
    const char* found = FindFilePath(path, 1, nullptr);
    if (found == nullptr) {
        CTRACE(0xc, "Ride script not located for %s - using placeholder\n", GetDBInfo()->mFolder);
        found = FindFilePath("Data:TestScript:test.rse", 1, nullptr);
    }
    const uint32_t thing = static_cast<uint32_t>(mBase.GetThingId().mIndex) << 16;
    if (gActionRecorder.mRecording != 0) {
        mF20 = static_cast<uint32_t>(RSSE_InitScript(found, thing, 1));
    } else {
        mF20 = static_cast<uint32_t>(RSSE_InitScript(found, thing, 0));
    }
    SetRideScriptID(mF1C, mF20);
}

// Makes (flag 0) or restores (flag nonzero) the ride's instance in the engine from the ride record (nothing when the database has none). A ride
// whose record has "don't apply offset" set (the kind 0x361) is placed at the record's engine offset, else at the balance sheet's; the other
// kind (0x32f) is placed at the map cell mF30 (1 based: x is the low seven bits, z the rest, 0 wrapping). Only the 0x32f kind tells the instance
// its script and puts the coaster queues back when it is restored; a made instance's number goes to mF1C.
void CObject::UpdateEngine(int32_t flag) {
    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
    if (info == nullptr) return;
    sFixedRideParams params;
    params.mRotation = mRotation;
    params.mF0C = info->Value(kRideSlot_Shape).mShape;
    if (info->Value(kRideSlot_DontApplyOffset).mInteger != 0) {
        const int32_t x = info->Value(kRideSlot_EngineMapOffsetOverrideX).mInteger;
        params.mX = x != 0 ? x : gMainBalance.mValues[kMainBalanceRideOffsetX].mInteger;
        const int32_t z = info->Value(kRideSlot_EngineMapOffsetOverrideY).mInteger;
        params.mZ = z != 0 ? z : gMainBalance.mValues[kMainBalanceRideOffsetZ].mInteger;
        if (flag != 0) {
            RestoreRideInstance(mF1C, info->mRideTypeNumber, 0x361, &params, 0, this,
                                info->Value(kRideSlot_EngineFootprintWidthOverride).mInteger,
                                info->Value(kRideSlot_EngineFootprintHeightOverride).mInteger, static_cast<int32_t>(mF20));
        } else {
            mF1C = InstanceRide(info->mRideTypeNumber, 0x361, &params, 0, static_cast<int32_t>(mF20), this,
                                info->Value(kRideSlot_EngineFootprintWidthOverride).mInteger,
                                info->Value(kRideSlot_EngineFootprintHeightOverride).mInteger, 0xffffffffu, 0);
        }
    } else {
        params.mX = static_cast<int32_t>((static_cast<uint32_t>(mF30) - 1u) & 0x7fu);
        params.mZ = static_cast<int32_t>((static_cast<uint32_t>(mF30) - 1u) >> 7);
        if (flag != 0) {
            RestoreRideInstance(mF1C, info->mRideTypeNumber, 0x32f, &params, 0, this, 0, 0, 0);
            SetRideScriptID(mF1C, mF20);
            HACKRestoreCoasterQueues(mF1C);
        } else {
            mF1C = InstanceRide(info->mRideTypeNumber, 0x32f, &params, 0, static_cast<int32_t>(mF20), this, 0, 0, 0xffffffffu, 0);
        }
    }
}
