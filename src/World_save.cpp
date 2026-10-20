// The world's save (main app of SimTheme Park 2.04b3, World.cpp; unit w205): CWorld::Save, the two SAVEVAR instances it needs for its own
// enums (CWorld::WorldState 0x1014182c and CThing::ThingModel 0x10141980), the staff and mechanic headquarters getters, the macro layer's
// destructor and CThing's empty serialise constructor. The two chunk reader destructors of the same range are in src/ChunkFile.cpp with their
// classes. CWorld::Load and the matching LOADVAR instances are the next unit.
//
// The save is a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list of the thing table:
// for each used entry the next entry's index, the thing's kind and the thing itself, written by its own Serialise by the kind. Every callee
// answers 0 for success; the first nonzero answer ends Save with 0 at once (nothing is called after it), and a save that wrote everything
// answers 1. The strings are at 0x10247498 + the offset in the comment.
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <new>

#include "AdvisorQueue.h"
#include "Debug.h"
#include "LbFile.h"
#include "Mouse.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "ThingReceivers.h"
#include "analyser.h"

static_assert(offsetof(CWorld, mStaffHQThing) == 0x1da718, "the staff headquarters' row index sits at +0x1da718");
static_assert(offsetof(CWorld, mMechanicHQThing) == 0x1da71a, "the mechanic headquarters' row index sits at +0x1da71a");

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The Mac makes the mouse and its default mode inline (as src/Mouse_staff.cpp's and src/LLCellSystem_modes.cpp's, each file has its own copy):
// the mouse (operator new(1)), then the default mode (operator new(8)), which the mouse takes; then the two words of the mouse's data are set
// to -1. A mouse that could not be made leaves the words alone and the default mode is still made and handed over (the mode may be NULL,
// nothing tests it). The answer is the mouse, NULL when it could not be made.
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

// A thing table row in a world member (a two byte struct on the Mac).
ThingId* RowOf(uint16_t& member) { return reinterpret_cast<ThingId*>(&member); }

// The thing's own serialiser, for the used list: save 1, version 1 and the file's version word as the last argument. True when it answered 0.
template<class Thing>
bool SaveThing(void* thing, void* file, uint32_t version) {
    return static_cast<Thing*>(thing)->Serialise(file, 1, 1, version) == 0;
}

}  // namespace

// The int instance's body for the two four byte enums the world saves.
template<> int32_t SAVEVAR<CWorld::WorldState>(void* file, CWorld::WorldState* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1014182c, 260 bytes
}
template<> int32_t SAVEVAR<CThing::ThingModel>(void* file, CThing::ThingModel* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10141980, 260 bytes
}

// 0x1013bf18 (2,740 bytes). The mouse comes first (the default mode is made before the mouse test, the mouse is stored even when it could not
// be made, and the call that hands it the mode has no NULL test). Each CTRACE takes the position read just before it, and each part's start
// is a position read again (two LbFile_GetPosition calls between two parts). The members are saved in the listing's order, not the layout's; the names the Mac
// gives the strings are in the comments (mF1DA710 "mParkClosed ", mPersonCount "mNumberOfVisitorsToDate ", mStartTime "mRandomSeed",
// mF1DA738 "mWorldState ", mTurn "mGameTick ", the six list heads "mFirstHandyman" to "mFirstObject"). mAdvisorThing and mChallengeHQThing
// are not saved. The long and unsigned long instances (0x1000f384, 0x1000f5dc) are the four byte MacLong and MacULong.
int32_t CWorld::Save(void* file) {
    int32_t start = LbFile_GetPosition(file);

    CMMDefault* const mode = new (std::nothrow) CMMDefault;
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);

    MacULong version = static_cast<MacULong>(2);
    if (SAVEVAR<MacULong>(file, &version, "version", 0) != 0) return 0;   // 0x413
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mBusThing), "mArrivalVehicle_Size1 ", 0) != 0) return 0;   // 0x41b
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mSeaplaneThing), "mArrivalVehicle_Size2 ", 0) != 0) return 0;   // 0x432
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mFerryThing), "mArrivalVehicle_Size3 ", 0) != 0) return 0;   // 0x449
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mBankAccountThing), "mBankAccount ", 0) != 0) return 0;   // 0x460
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mArrivalVehicleThing), "mCurrentArrivalVehicle ", 0) != 0) return 0;   // 0x46e
    if (SAVEVAR<MacULong>(file, reinterpret_cast<MacULong*>(&mTurn), "mGameTick ", 0) != 0) return 0;   // 0x486
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mMechanicHQThing), "mMechanicHQ ", 0) != 0) return 0;   // 0x491
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mParkAnalyserThing), "mParkAnalyser ", 0) != 0) return 0;   // 0x49e
    if (SAVEVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF1DA710), "mParkClosed ", 0) != 0) return 0;   // 0x4ad
    if (SAVEVAR<MacULong>(file, reinterpret_cast<MacULong*>(&mPersonCount), "mNumberOfVisitorsToDate ", 0) != 0) return 0;   // 0x4ba
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mGatesThing), "mParkGates ", 0) != 0) return 0;   // 0x4d3
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mLightsThing), "mTrafficLights ", 0) != 0) return 0;   // 0x4df
    if (SAVEVAR<MacLong>(file, reinterpret_cast<MacLong*>(&mStartTime), "mRandomSeed", 0) != 0) return 0;   // 0x4ef
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mResearchLabThing), "mResearchLab ", 0) != 0) return 0;   // 0x4fb
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mStaffHQThing), "mStaffHQ ", 0) != 0) return 0;   // 0x509
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mTagSystemThing), "mTagSystem ", 0) != 0) return 0;   // 0x513
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mUIMsgReceiverThing), "mUIMsgReceiver ", 0) != 0) return 0;   // 0x51f
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mWeatherThing), "mWeather ", 0) != 0) return 0;   // 0x52f
    if (SAVEVAR<WorldState>(file, reinterpret_cast<WorldState*>(&mF1DA738), "mWorldState ", 0) != 0) return 0;   // 0x539
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA73C), "mFirstHandyman", 0) != 0) return 0;   // 0x546
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA73E), "mFirstMechanic", 0) != 0) return 0;   // 0x555
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA740), "mFirstEntertainer", 0) != 0) return 0;   // 0x564
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA744), "mFirstGuard", 0) != 0) return 0;   // 0x576
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA742), "mFirstResearcher", 0) != 0) return 0;   // 0x582
    if (SAVEVARSTRUCT<ThingId>(file, RowOf(mF1DA746), "mFirstObject", 0) != 0) return 0;   // 0x593
    CTRACE(2, "\t World vars: saved %d bytes\n", LbFile_GetPosition(file) - start);   // 0x5a0

    start = LbFile_GetPosition(file);
    if (GetControlManager()->Serialise(file, 1, 1, static_cast<uint32_t>(version)) != 0) return 0;
    CTRACE(2, "\t mControlManager: saved %d bytes\n", LbFile_GetPosition(file) - start);   // 0x5be

    start = LbFile_GetPosition(file);
    if (reinterpret_cast<CMacroAI*>(this)->Serialise(file, 1, 1, static_cast<uint32_t>(version)) != 0) return 0;
    CTRACE(2, "\t mMacroAI: saved %d bytes\n", LbFile_GetPosition(file) - start);   // 0x5e1

    start = LbFile_GetPosition(file);
    if (GetMap()->Serialise(file, 1, 1, static_cast<uint32_t>(version)) != 0) return 0;
    CTRACE(2, "\t mMap: saved %d bytes\n", LbFile_GetPosition(file) - start);   // 0x5fd

    // The used things: the head's index, then for each entry (dead things too, and the thing is not tested for NULL) the next entry's index (0
    // for the last), the thing's kind byte and the thing by its kind. 18 (an online person) and the kinds the table does not know (0, 2 and
    // above 0x13) only assert and go on with the next entry.
    start = LbFile_GetPosition(file);
    if (SAVEVAR<uint32_t>(file, &gUsedThingHead->mF04[0], "Used Thing Head", 0) != 0) return 0;   // 0x615
    for (CThingArrayEntry* entry = gUsedThingHead; entry != nullptr; entry = entry->mNextUsed) {
        uint32_t next = entry->mNextUsed != nullptr ? entry->mNextUsed->mF04[0] : 0;
        if (SAVEVAR<uint32_t>(file, &next, "Used Thing Next", 0) != 0) return 0;   // 0x625
        CThing::ThingModel model = static_cast<CThing::ThingModel>(static_cast<CThing*>(entry->mThing)->mKind);
        if (SAVEVAR<CThing::ThingModel>(file, &model, "thingmodel", 0) != 0) return 0;   // 0x635
        const uint32_t fileVersion = static_cast<uint32_t>(version);
        switch (static_cast<uint32_t>(model)) {
        case 1: if (!SaveThing<CPerson>(entry->mThing, file, fileVersion)) return 0; break;
        case 3: if (!SaveThing<CObject>(entry->mThing, file, fileVersion)) return 0; break;
        case 4: if (!SaveThing<CMechanic>(entry->mThing, file, fileVersion)) return 0; break;
        case 5: if (!SaveThing<CHandyman>(entry->mThing, file, fileVersion)) return 0; break;
        case 6: if (!SaveThing<CEntertainer>(entry->mThing, file, fileVersion)) return 0; break;
        case 7: if (!SaveThing<CGuard>(entry->mThing, file, fileVersion)) return 0; break;
        case 8: if (!SaveThing<CResearcher>(entry->mThing, file, fileVersion)) return 0; break;
        case 9: if (!SaveThing<CStaffHQ>(entry->mThing, file, fileVersion)) return 0; break;
        case 10: if (!SaveThing<CMechanicHQ>(entry->mThing, file, fileVersion)) return 0; break;
        case 11: if (!SaveThing<CAdvisor>(entry->mThing, file, fileVersion)) return 0; break;
        case 12: if (!SaveThing<CTagSystem>(entry->mThing, file, fileVersion)) return 0; break;
        case 13: if (!SaveThing<CParkAnalyser>(entry->mThing, file, fileVersion)) return 0; break;
        case 14: if (!SaveThing<CResearchLab>(entry->mThing, file, fileVersion)) return 0; break;
        case 15: if (!SaveThing<CWeather>(entry->mThing, file, fileVersion)) return 0; break;
        case 16: if (!SaveThing<CBankAccount>(entry->mThing, file, fileVersion)) return 0; break;
        case 17: if (!SaveThing<CUIMsgReceiver>(entry->mThing, file, fileVersion)) return 0; break;
        case 18:
            GAME_ASSERTMSG(0, "Should not be able to save online persons\n");   // 0x640
            break;
        case 19: if (!SaveThing<CChallengeHQ>(entry->mThing, file, fileVersion)) return 0; break;
        default:
            GAME_ASSERTMSG(0, "Unknown thing model number loaded\n");   // 0x66b
            break;
        }
    }
    CTRACE(2, "\t Thing Array: saved %d bytes\n", LbFile_GetPosition(file) - start);   // 0x68e
    LbFile_GetPosition(file);   // the answer is dropped
    return 1;
}

// 0x1013e99c and 0x1013ea24 (96 bytes each): as GetAdvisor (src/World_GetAdvisor.cpp): the row index is read before the check that entry 0 of the
// table is free (the check only reports), the answer is the row's thing, NULL for a free row, and the index is not checked against the table.
CStaffHQ* CWorld::GetStaffHQ() {
    const uint16_t index = mStaffHQThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<CStaffHQ*>(gThingArray[index].mThing);
}

CMechanicHQ* CWorld::GetMechanicHQ() {
    const uint16_t index = mMechanicHQThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<CMechanicHQ*>(gThingArray[index].mThing);
}

// 0x1013a13c (96 bytes). The Mac's destructor tests `this`, runs the staff database's destructor on the first 0x2a0 bytes with a negative flag (no
// delete there) and deletes the object when its own flag (r4, a signed halfword) is positive. The NULL test and the flag have no host form: a
// delete-expression gives the deleting form.
CMacroAI::~CMacroAI() { reinterpret_cast<CStaffDB*>(this)->~CStaffDB(); }

// 0x1013dce0 (4 bytes): a bare return; the thing's fields are left as they were (the loader fills them in).
CThing::CThing(const CSerialise&) {}
