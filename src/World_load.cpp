// The world's load (main app of SimTheme Park 2.04b3, World.cpp; unit w207): CWorld::Load, the two LOADVAR instances it needs for its own
// enums (CWorld::WorldState 0x101415b4 and CThing::ThingModel 0x101416f0) and CThing::ForceThingId (Thing.cpp). The matching save is
// src/World_save.cpp (unit w205).
//
// The load reads what Save wrote: a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list
// of the thing table. It starts by resetting the table (InitThingHeap). The used list is rebuilt from the head's index and each entry's next
// index, and for every used entry the load MAKES the thing of the entry's kind in place (operator new of the kind's size, the part of the
// class's constructor the Mac inlines here, the kind byte, the entry's index as the thing's id) before the thing's own Serialise reads it.
// Every callee answers 0 for success; the first nonzero answer ends Load with 0 at once (nothing is called after it, and a thing that was
// made is not entered in its row), and a load that read everything relinks the free list, switches the sprite drawing by the world state
// and answers 1. The strings are at 0x10247498 + the offset in the comment.
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "analyser.h"

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// A thing table row in a world member (a two byte struct on the Mac).
ThingId* RowOf(uint16_t& member) { return reinterpret_cast<ThingId*>(&member); }

// The Mac's __construct_array: the part's constructor on each element, in ascending order.
template<class Part, size_t Count>
void ConstructAll(Part (&parts)[Count]) {
    for (size_t i = 0; i < Count; ++i) new (&parts[i]) Part;
}

// 0x1013dca8 (20 bytes, TbTimeStamp::TbTimeStamp): the second word, then the first. A time stamp is two words on the host.
void ClearStamp(uint32_t (&stamp)[2]) {
    stamp[1] = 0;
    stamp[0] = 0;
}

// A person (kind 1, 0x22c bytes). CPerson has no members on the host, so the constructor's stores go by offset: the thoughts at +0x30, the pilot
// at +0xd4, seven floats, the two arrays of four thing ids (their constructor, 0x10116ccc, is a bare return: nothing is written) and one float.
void InitPerson(void* block) {
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
void InitStaff(void* block) {
    CStaff* const staff = static_cast<CStaff*>(block);
    new (staff->Thoughts()) CThoughts;   // 0x1012ec88
    new (staff->mVehicleA) Pilot;   // 0x10134610
    staff->mTimeHired[1] = 0;
    staff->mTimeHired[0] = 0;
    staff->mHappiness = 0.0f;
    staff->mEnergy = 0.0f;
}

// An object (kind 3, 0x448 bytes): the build time, the three condition floats and the six histories (the rings are not written).
void InitObject(void* block) {
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

// The staff headquarters (kind 9, 0x68 bytes): the training centre and the five strikes.
void InitStaffHQ(void* block) {
    CStaffHQ* const hq = static_cast<CStaffHQ*>(block);
    new (&hq->mTraining) CTrainingCentre;
    ConstructAll(hq->mStrikes);   // 0x1012a950
}

// The advisor (kind 11, 0x16f0 bytes): the word at +0xc, the eight requests of the queue (the valid byte), the 0x15f message records (the time
// stamp word 0, the response index -1, the said-once byte 0, the kill count 0) and the valid byte of the last tutorial request.
void InitAdvisor(void* block) {
    CAdvisor* const advisor = static_cast<CAdvisor*>(block);
    std::memset(advisor->mF0C, 0, 4);
    ConstructAll(advisor->mQueue);   // 0x1000a0b4
    for (AdvisorMessageState& state : advisor->mMessageState) {   // 0x1000a06c
        state.mLastSaid.mTimeSig = 0;
        state.mResponseIndex = -1;
        state.mSaidOnce = 0;
        state.mKillCount = 0;
    }
    advisor->mLastTutorialRequest.mValid = 0;
}

// The park analyser (kind 13, 0x21c48 bytes). Its thing part is made by CThing's serialise constructor (a call on the Mac, 0x1013dce0). The
// counts, the map size and the rest of the scalars are not written (Serialise reads them).
void InitParkAnalyser(void* block) {
    CParkAnalyser* const analyser = static_cast<CParkAnalyser*>(block);
    const CSerialise serialise{};
    new (&analyser->mBase) CThing(serialise);
    ConstructAll(analyser->mCells);   // 0x100e2f3c
    ConstructAll(analyser->mNearbyCells);   // 0x100e2ef0 (a bare return)
    ConstructAll(analyser->mLastOfObjectType);   // 0x100f0abc
    new (&analyser->mVal1F0B8) CParkAnalyserVal<int>;   // 0x100ef230
    new (&analyser->mVal1F0C4) CParkAnalyserVal<int>;
    new (&analyser->mVal1F0D0) CParkAnalyserVal<int>;
    new (&analyser->mVal1F0DC) CParkAnalyserVal<int>;
    new (&analyser->mVal1F0E8) CParkAnalyserVal<int>;
    new (&analyser->mVal1F0F4) CParkAnalyserVal<int>;
    ConstructAll(analyser->mHistory1F100);   // 0x100f0708
    new (&analyser->mHistory20F10) CHistory<float, 144>;   // 0x100f03c4
    ConstructAll(analyser->mHistory21160);
    new (&analyser->mHistory21600) CHistory<int, 50>;   // 0x100efe40
    new (&analyser->mHistory216D8) CHistory<int, 5>;   // 0x100efb70
    ConstructAll(analyser->mOpinionHistory);   // 0x100ef5f8
    ClearStamp(analyser->mLastStrike);
    ClearStamp(analyser->mLastPuke);
    ClearStamp(analyser->mLastPrank);
    ClearStamp(analyser->mParkLastOpened);
    ClearStamp(analyser->mLastObjectBuilt);
    ClearStamp(analyser->mLastStaffHired);
}

// The research lab (kind 14, 0x146c bytes): the 256 projects (0x10123abc, a bare return) and the five categories (0x101242a0).
void InitResearchLab(void* block) {
    CResearchLab* const lab = static_cast<CResearchLab*>(block);
    ConstructAll(lab->mProjects);
    ConstructAll(lab->mCategories);
}

// The weather (kind 15, 0x68 bytes): the two time stamps' words in the listing's order (+0x14, +0x10, +0x1c, +0x18) and the forecast.
void InitWeather(void* block) {
    CWeather* const weather = static_cast<CWeather*>(block);
    weather->mF14 = 0;
    weather->mF10 = 0;
    weather->mF1C = 0;
    weather->mF18 = 0;
    new (&weather->mForecast) CWeatherForecast;   // 0x10139674
}

// The challenge headquarters (kind 19, 0x3e4 bytes): the 20 challenges (0x100f5a34) and the next event time.
void InitChallengeHQ(void* block) {
    CChallengeHQ* const hq = static_cast<CChallengeHQ*>(block);
    ConstructAll(hq->mChallenges);
    hq->mNextChallengeEventTime[1] = 0;
    hq->mNextChallengeEventTime[0] = 0;
}

// The tail every kind shares: the kind byte (the low byte of the model) goes into the block even when the block could not be made (the Mac
// then stores at address 2), the entry's index becomes the thing's id, the thing reads itself (save 0, version 1 and the file's version word
// as the last argument) and only then does the row take the thing. True when the thing's Serialise answered 0.
template<class Thing>
bool ReadThing(CThingArrayEntry* entry, void* block, uint32_t model, uint16_t id, void* file, uint32_t version) {
    CThing* const thing = static_cast<CThing*>(block);
    thing->mKind = static_cast<uint8_t>(model);
    thing->ForceThingId(ThingId{id});
    if (static_cast<Thing*>(block)->Serialise(file, 0, 1, version) != 0) return false;
    entry->mThing = block;
    return true;
}

}  // namespace

// The int instance's body for the two four byte enums the world loads.
template<> int32_t LOADVAR<CWorld::WorldState>(void* file, CWorld::WorldState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x101415b4, 236 bytes
}
template<> int32_t LOADVAR<CThing::ThingModel>(void* file, CThing::ThingModel* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x101416f0, 236 bytes
}

// 0x1013c9f0 (4,756 bytes). The fields are read in the order Save wrote them (the names the Mac gives the strings are in the comments, as in
// Save). Each entry of the used list is linked as soon as its next index is read: the next row, the previous entry (NULL for the first) and a
// cleared free link are stored BEFORE the kind is read, so a load that fails there leaves the entry linked. The entry's index is read after
// the kind and before the switch. The kinds 0, 2 and above 0x13 only assert; 18 (an online person) asserts too; all of them go on with the
// next entry and leave the row's thing as it was. The kinds 10, 12, 16 and 17 are made without a NULL test and without an inline constructor
// (a bare block); the others test the block and build their parts only when it was made.
int32_t CWorld::Load(void* file) {
    InitThingHeap();

    MacULong version = static_cast<MacULong>(0);
    if (LOADVAR<MacULong>(file, &version, "version", 0) != 0) return 0;   // 0x413
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mBusThing), "mArrivalVehicle_Size1 ", 0) != 0) return 0;   // 0x41b
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mSeaplaneThing), "mArrivalVehicle_Size2 ", 0) != 0) return 0;   // 0x432
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mFerryThing), "mArrivalVehicle_Size3 ", 0) != 0) return 0;   // 0x449
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mBankAccountThing), "mBankAccount ", 0) != 0) return 0;   // 0x460
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mArrivalVehicleThing), "mCurrentArrivalVehicle ", 0) != 0) return 0;   // 0x46e
    if (LOADVAR<MacULong>(file, reinterpret_cast<MacULong*>(&mTurn), "mGameTick ", 0) != 0) return 0;   // 0x486
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mMechanicHQThing), "mMechanicHQ ", 0) != 0) return 0;   // 0x491
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mParkAnalyserThing), "mParkAnalyser ", 0) != 0) return 0;   // 0x49e
    if (LOADVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF1DA710), "mParkClosed ", 0) != 0) return 0;   // 0x4ad
    if (LOADVAR<MacULong>(file, reinterpret_cast<MacULong*>(&mPersonCount), "mNumberOfVisitorsToDate ", 0) != 0) return 0;   // 0x4ba
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mGatesThing), "mParkGates ", 0) != 0) return 0;   // 0x4d3
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mLightsThing), "mTrafficLights ", 0) != 0) return 0;   // 0x4df
    if (LOADVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mStartTime), "mRandomSeed", 0) != 0) return 0;   // 0x4ef
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mResearchLabThing), "mResearchLab ", 0) != 0) return 0;   // 0x4fb
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mStaffHQThing), "mStaffHQ ", 0) != 0) return 0;   // 0x509
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mTagSystemThing), "mTagSystem ", 0) != 0) return 0;   // 0x513
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mUIMsgReceiverThing), "mUIMsgReceiver ", 0) != 0) return 0;   // 0x51f
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mWeatherThing), "mWeather ", 0) != 0) return 0;   // 0x52f
    if (LOADVAR<WorldState>(file, reinterpret_cast<WorldState*>(&mF1DA738), "mWorldState ", 0) != 0) return 0;   // 0x539
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA73C), "mFirstHandyman", 0) != 0) return 0;   // 0x546
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA73E), "mFirstMechanic", 0) != 0) return 0;   // 0x555
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA740), "mFirstEntertainer", 0) != 0) return 0;   // 0x564
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA744), "mFirstGuard", 0) != 0) return 0;   // 0x576
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA742), "mFirstResearcher", 0) != 0) return 0;   // 0x582
    if (LOADVARSTRUCT<ThingId>(file, RowOf(mF1DA746), "mFirstObject", 0) != 0) return 0;   // 0x593

    if (GetControlManager()->Serialise(file, 0, 1, static_cast<uint32_t>(version)) != 0) return 0;
    if (reinterpret_cast<CMacroAI*>(this)->Serialise(file, 0, 1, static_cast<uint32_t>(version)) != 0) return 0;
    if (GetMap()->Serialise(file, 0, 1, static_cast<uint32_t>(version)) != 0) return 0;

    // The used things: the head's index (not checked against the table; 0 is entry 0, not "none"), then for each entry the next entry's index
    // (0 for the last) and the kind. The entry's index doubles as the thing's id.
    uint32_t head = 0;
    if (LOADVAR<uint32_t>(file, &head, "Used Thing Head", 0) != 0) return 0;   // 0x615
    gUsedThingHead = &gThingArray[head];
    CThingArrayEntry* previous = nullptr;
    for (CThingArrayEntry* entry = gUsedThingHead; entry != nullptr; previous = entry, entry = entry->mNextUsed) {
        uint32_t next = 0;
        if (LOADVAR<uint32_t>(file, &next, "Used Thing Next", 0) != 0) return 0;   // 0x625
        entry->mNextUsed = next == 0 ? nullptr : &gThingArray[next];
        entry->mPrevUsed = previous;
        entry->mNextFree = nullptr;
        CThing::ThingModel model = static_cast<CThing::ThingModel>(0);
        if (LOADVAR<CThing::ThingModel>(file, &model, "thingmodel", 0) != 0) return 0;   // 0x635
        const uint16_t id = static_cast<uint16_t>(entry->mF04[0]);
        const uint32_t kind = static_cast<uint32_t>(model);
        const uint32_t fileVersion = static_cast<uint32_t>(version);
        switch (kind) {
        case 1: {
            void* block = ::operator new(0x22c);
            if (block != nullptr) InitPerson(block);
            if (!ReadThing<CPerson>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 3: {
            void* block = ::operator new(0x448);
            if (block != nullptr) InitObject(block);
            if (!ReadThing<CObject>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 4: {
            void* block = ::operator new(0x218);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CMechanic>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 5: {
            void* block = ::operator new(0x218);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CHandyman>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 6: {
            void* block = ::operator new(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CEntertainer>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 7: {
            void* block = ::operator new(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CGuard>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 8: {
            void* block = ::operator new(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CResearcher>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 9: {
            void* block = ::operator new(0x68);
            if (block != nullptr) InitStaffHQ(block);
            if (!ReadThing<CStaffHQ>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 10:
            if (!ReadThing<CMechanicHQ>(entry, ::operator new(0xe), kind, id, file, fileVersion)) return 0;
            break;
        case 11: {
            void* block = ::operator new(0x16f0);
            if (block != nullptr) InitAdvisor(block);
            if (!ReadThing<CAdvisor>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 12:
            if (!ReadThing<CTagSystem>(entry, ::operator new(0x80e), kind, id, file, fileVersion)) return 0;
            break;
        case 13: {
            void* block = ::operator new(0x21c48);
            if (block != nullptr) InitParkAnalyser(block);
            if (!ReadThing<CParkAnalyser>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 14: {
            void* block = ::operator new(0x146c);
            if (block != nullptr) InitResearchLab(block);
            if (!ReadThing<CResearchLab>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 15: {
            void* block = ::operator new(0x68);
            if (block != nullptr) InitWeather(block);
            if (!ReadThing<CWeather>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 16:
            if (!ReadThing<CBankAccount>(entry, ::operator new(0x128), kind, id, file, fileVersion)) return 0;
            break;
        case 17:
            if (!ReadThing<CUIMsgReceiver>(entry, ::operator new(0xc), kind, id, file, fileVersion)) return 0;
            break;
        case 18:
            GAME_ASSERTMSG(0, "Should not be able to load online persons\n");   // 0x6ad
            break;
        case 19: {
            void* block = ::operator new(0x3e4);
            if (block != nullptr) InitChallengeHQ(block);
            if (!ReadThing<CChallengeHQ>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        default:
            GAME_ASSERTMSG(0, "Unknown thing model number loaded\n");   // 0x66b
            break;
        }
    }

    ReconstructFreeList();
    if (mF1DA738 == 4) {
        ESP_DisableSpriteDraw();
    } else {
        ESP_EnableSpriteDraw();
    }
    return 1;
}

// 0x1012dfa8 (16 bytes). The loader makes the thing's id from the table row it sits in (the row's index, a halfword).
void CThing::ForceThingId(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { mThingId = id.mIndex; }
