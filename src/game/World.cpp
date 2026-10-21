// World.cpp: name proven by __sinit_World_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BlockSave.h"
#include "CellBase.h"
#include "ChunkFile.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "ExitErr.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "helpers/analyserHelpers.h"
#include "helpers/ResearchLabHelpers.h"
#include "helpers/rsseHelpers.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LevelInit2.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "ParkMessages.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "tb_hfield.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

// The world's load (main app of SimTheme Park 2.04b3, World.cpp; unit w207): CWorld::Load, the two LOADVAR instances it needs for its own
// enums (CWorld::WorldState 0x101415b4 and CThing::ThingModel 0x101416f0) and CThing::ForceThingId (Thing.cpp). The matching save is
// src/World_save.cpp (unit w205).
//
// The load reads what Save wrote: a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list
// of the thing table. It starts by resetting the table (InitThingHeap). The used list is rebuilt from the head's index and each entry's next
// index, and for every used entry the load MAKES the thing of the entry's kind in place (operator new of the kind's size, the part of the
// class's constructor the Mac inlines here, the kind byte, the entry's index as the thing's id) before the thing's own Serialise reads it.
// The Mac's operator new is the game heap, so the host takes every thing's block from the arena (unit v5L31): the lists keep a thing's
// address in a 32-bit word.
// Every callee answers 0 for success; the first nonzero answer ends Load with 0 at once (nothing is called after it, and a thing that was
// made is not entered in its row), and a load that read everything relinks the free list, switches the sprite drawing by the world state
// and answers 1. The strings are at 0x10247498 + the offset in the comment.

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

// CWorld's construction (World.cpp, main app of SimTheme Park 2.04b3; unit w113): the constructor, AllocateThingHeap, InitThingHeap
// and ObjectControl's empty constructor. Allocate is in World_alloc.cpp.

// 0x10139f70 (376 bytes). The Mac builds the members in this order: the staff database, the park time, the arrival settings, the map
// cells (0x4000 of 0x44 bytes), the track cells (0x4000 of 0x28), the region effect cells (0x4000 of 10), then the two grid
// registrations, then the object controls (0x96 of 0x20), and last its own stores. The host's implicit member construction runs in
// declaration order (the object controls before the registrations): nothing can tell, ObjectControl's constructor is empty and the
// registrations touch only their globals. No other member is initialised: a world built over 0xa5 keeps 0xa5 there.
CWorld::CWorld() {
    CCellBase::RegisterMapGrid(mMapCells, sizeof(CMapCell));
    CCellBase::RegisterTrackGrid(mTrackCells, sizeof(CTrackCell));
    // Each halfword goes through a stack halfword on the Mac; the value is 0.
    mF1DA73C = 0;
    mF1DA73E = 0;
    mF1DA740 = 0;
    mF1DA744 = 0;
    mF1DA742 = 0;
    mF1DA746 = 0;
    mF1DA710 = 1;
    gFreeThingHead = &gThingArray[0];
    gUsedThingHead = nullptr;
}

// 0x1013a10c (4 bytes): a bare blr. The 0x96 object controls of the world are never initialised.
ObjectControl::ObjectControl() {}

// The world's save (main app of SimTheme Park 2.04b3, World.cpp; unit w205): CWorld::Save, the two SAVEVAR instances it needs for its own
// enums (CWorld::WorldState 0x1014182c and CThing::ThingModel 0x10141980), the staff and mechanic headquarters getters, the macro layer's
// destructor and CThing's empty serialise constructor. The two chunk reader destructors of the same range are in src/ChunkFile.cpp with their
// classes. CWorld::Load and the matching LOADVAR instances are the next unit.
//
// The save is a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list of the thing table:
// for each used entry the next entry's index, the thing's kind and the thing itself, written by its own Serialise by the kind. Every callee
// answers 0 for success; the first nonzero answer ends Save with 0 at once (nothing is called after it), and a save that wrote everything
// answers 1. The strings are at 0x10247498 + the offset in the comment.

static_assert(offsetof(CWorld, mStaffHQThing) == 0x1da718, "the staff headquarters' row index sits at +0x1da718");
static_assert(offsetof(CWorld, mMechanicHQThing) == 0x1da71a, "the mechanic headquarters' row index sits at +0x1da71a");

namespace {

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

// 0x1013a13c (96 bytes). The Mac's destructor tests `this`, runs the staff database's destructor on the first 0x2a0 bytes with a negative flag (no
// delete there) and deletes the object when its own flag (r4, a signed halfword) is positive. The NULL test and the flag have no host form: a
// delete-expression gives the deleting form.
CMacroAI::~CMacroAI() { reinterpret_cast<CStaffDB*>(this)->~CStaffDB(); }

// CWorld::Allocate (World.cpp, main app of SimTheme Park 2.04b3; unit w113), in a file of its own so that a test library can fake
// AllocateThingHeap and reach the error branch.

// 0x1013a1c0 (124 bytes). AllocateThingHeap always answers 1, so the error branch (CExitError type 8, value 0, no text) is dead code.
// The save block name is the original's own typo ("World Struture"); RegisterSaveBlock drops it.
bool CWorld::Allocate() {
    if (!AllocateThingHeap()) {
        gExitError.SetError(CExitError::kType8, 0, nullptr);
        return false;
    }
    gBlockSave.RegisterSaveBlock(CBlockSave::kWorld, this, sizeof(CWorld), "World Struture");
    return true;
}

// CWorld::ExitLevel, CWorld::Free and CWorld::FreeThingHeap (main app, SimTheme Park 2.04b3, World.cpp; unit w81).

// 0x1013a264 (36 bytes).
void CWorld::Free() { FreeThingHeap(); }

// CWorld::SetupNewLevel (World.cpp, main app 0x1013a2ac, SimTheme Park 2.04b3; unit w117) and the globals of World.cpp it needs.

// The two names (see World.h). __sinit_World_cpp (0x10141c14) builds the strings on the Mac; until it is translated they are
// defined here with their texts.
TbDynamicStringTemplate<char16_t> gWorldGatesName(u"gates");
TbDynamicStringTemplate<char16_t> gWorldLightsName(u"lights");

namespace {

// A thing of `size` bytes (the Mac's size: the host classes are placeholders), made with its constructor; the thing's id is the half word
// at its start. The Mac tests the allocation for NULL and still asks for the id (the host's allocation never fails, so that path is not
// reproduced, as in unit w106).
template <class T> uint16_t MakeThing(uint32_t size) {
    void* block = tpw::ArenaAlloc(size);   // the Mac's game heap: the arena (a list keeps a thing's address in 32 bits)
    T* thing = new (block) T;
    return reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
}

// The first ride whose item name is `name` (ignoring case), walking the database from its first ride; NULL when none is. _wcsicmp is not
// called for a NULL ride.
RideInfo* FindRide(const TbDynamicStringTemplate<char16_t>& name) {
    RideInfo* ride = gRideDatabase.GetFirstRide();
    while (ride != nullptr && NS_MacDoze::_wcsicmp(ride->mItemName, name.mData) != 0) ride = gRideDatabase.GetNextRide();
    return ride;
}

// The "gates" or "lights" object: a CObject (0x448 bytes) on map cell 1 (cell 0, 0) of the ride's object type; its thing id.
uint16_t MakeFeatureObject(const RideInfo* ride) {
    void* block = tpw::ArenaAlloc(0x448);
    CObject* object = new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1},
                                          tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
    return reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
}

}  // namespace

// The set-up of a new level, in the Mac's order. The answer starts true and is cleared by a failed first or second balance file (the easy
// mode file's failure only asserts). The control manager's address comes from the global gGameWorld, not from `this`. The game type word is
// read twice (the Mac re-reads it after the second file). The ten things are made in a fixed order and the world keeps their ids; the
// ids of the four halfwords at 0x1da72a to 0x1da730 are cleared.
bool CWorld::SetupNewLevel(char* levelFolder) {
    bool result = true;
    mF1DA738 = 1;
    gGameWorld->GetControlManager()->SetupInitialControls();
    gActionRecorder.ResetActionRecording();
    std::memset(gParkRequiredRides, 0, sizeof gParkRequiredRides);
    char buffer[1040];
    if (static_cast<int32_t>(TheGameType().mType) == 1) {
        if (gMainBalance.Load("data:levels:online_Standard.sam", CBalanceTable::kLoadFresh) == 0) result = false;
        std::sprintf(buffer, "%s:%s", levelFolder, "online_Standard.sam");
        if (gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay) == 0) result = false;
    } else {
        const int32_t first = gMainBalance.Load("data:levels:Standard.sam", CBalanceTable::kLoadFresh);
        std::sprintf(buffer, "%s:%s", levelFolder, "Standard.sam");
        const int32_t second = gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay);
        result = second != 0 && first != 0;
        if (static_cast<int32_t>(TheGameType().mType) == 2) {
            std::sprintf(buffer, "%s:%s%s", levelFolder, gTPGlobals.mF427C.mData, "Standard.sam");
            if (gMainBalance.Load(buffer, CBalanceTable::kLoadOverlay) == 0)
                GAME_ASSERTMSG(0, "There is no Easy_standard.sam file for this theme - This is not critical");
        }
    }
    mTurn = 0;
    mStartTime = static_cast<uint32_t>(std::time(nullptr));
    GetMap()->Init();
    reinterpret_cast<CMacroAI*>(this)->Init();
    mPersonCount = 0;
    mStaffHQThing = MakeThing<CStaffHQ>(0x68);
    mMechanicHQThing = MakeThing<CMechanicHQ>(0xe);
    mAdvisorThing = MakeThing<CAdvisor>(0x16f0);
    mTagSystemThing = MakeThing<CTagSystem>(0x80e);
    mParkAnalyserThing = MakeThing<CParkAnalyser>(0x21c48);
    mResearchLabThing = MakeThing<CResearchLab>(0x146c);
    mWeatherThing = MakeThing<CWeather>(0x68);
    mBankAccountThing = MakeThing<CBankAccount>(0x128);
    mUIMsgReceiverThing = MakeThing<CUIMsgReceiver>(0xc);
    mChallengeHQThing = MakeThing<CChallengeHQ>(0x3e4);
    mArrivalVehicleThing = 0;
    mBusThing = 0;
    mSeaplaneThing = 0;
    mFerryThing = 0;

    // The "gates" and "lights" features of the ride database.
    const RideInfo* gates = FindRide(gWorldGatesName);
    if (gates == nullptr) {
        GAME_ASSERTMSG(0, "Could not find the gates feature\n");
        mGatesThing = 0;
    } else {
        mGatesThing = MakeFeatureObject(gates);
    }
    const RideInfo* lights = FindRide(gWorldLightsName);
    if (lights == nullptr) {
        // (the original copies the name and hands the copy's five words to the assertion, whose text has no conversion; the host cannot
        // pass a class through "...", so the words are dropped)
        const TbDynamicStringTemplate<char16_t> copy(gWorldLightsName);
        GAME_ASSERTMSG(0, "Could not find the LIGHTSNAME WAS HERE (BR 990809) feature\n");
        mLightsThing = 0;
    } else {
        mLightsThing = MakeFeatureObject(lights);
        CTRACE(2, "Found the LIGHTSNAME WAS HERE (BR 990809) feature\n");
    }

    GetResearchLab()->Init();
    BMP_InitSystem();
    Pilot::init_behaviours();

    // The first message: event 0, to the message centre (made here when there is none).
    CMsgEvent message{EventType(0)};
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    gMessageCentre->PostMessage(message);
    mF1DA738 = 0;
    return result;
}

// 0x1013a9fc (184 bytes). Flag word 0x1da738 is 2 while the level is taken apart and 1 afterwards. The mouse, when there is one, is
// told to drop its mode and deleted (`delete` of a CMouse is that call and then the plain operator delete), the global cleared after.
// The map is the one of the WORLD gGameWorld points to (the Mac reads that global again here), not of `this`. DeleteAllThings answers
// something in r3 which the Mac hands on to Vehicle::clear_behaviours, but that function takes no argument and overwrites r3 first.
void CWorld::ExitLevel() {
    mF1DA738 = 2;
    CMouse* mouse = gMouse;
    if (mouse != nullptr) {
        delete mouse;
    }
    gMouse = nullptr;
    gGameWorld->GetMap()->DeleteMapInEngine();
    DeleteAllThings();
    Vehicle::clear_behaviours();
    BMP_CloseSystem();
    mF1DA738 = 1;
}

// 0x1013aadc (60 bytes): the world pointer is stored first, then the map pointer (the Mac adds 0x2d8 with no NULL test), then the
// thing heap is built.
bool CWorld::AllocateThingHeap() {
    gWorld = this;
    gWorldMap = GetMap();
    InitThingHeap();
    return true;
}

// 0x1013ab48 (148 bytes), a leaf that never reads `this`. Entry 0 is the "no thing" sentinel (index 0, never on the free list);
// entries 1 to 0x27fe chain to their successor; entry 0x27ff ends the free chain. Every word of every entry is written; the loop
// counter is unsigned (cmplwi). The used list starts empty and the free list at entry 1.
void CWorld::InitThingHeap() {
    CThingArrayEntry* const table = gThingArray;
    table[0].mNextFree = nullptr;
    table[0].mNextUsed = nullptr;
    table[0].mPrevUsed = nullptr;
    table[0].mThing = nullptr;
    table[0].mF04[0] = 0;
    for (uint32_t i = 1; i < 0x27ff; ++i) {
        table[i].mNextFree = &table[i + 1];
        table[i].mNextUsed = nullptr;
        table[i].mPrevUsed = nullptr;
        table[i].mThing = nullptr;
        table[i].mF04[0] = i;
    }
    table[0x27ff].mNextFree = nullptr;
    table[0x27ff].mNextUsed = nullptr;
    table[0x27ff].mPrevUsed = nullptr;
    table[0x27ff].mThing = nullptr;
    table[0x27ff].mF04[0] = 0x27ff;
    gUsedThingHead = nullptr;
    gFreeThingHead = &table[1];
}

// 0x1013ac08 (24 bytes). The Mac counts a register up to 0x2800 and does nothing in the loop (the compiler kept the loop, its body
// compiled to nothing). Kept as a loop for the record; the host compiler drops it, which nothing can observe.
void CWorld::FreeThingHeap() {
    for (uint32_t slot = 0; slot < 0x2800; ++slot) {
    }
}

// The world's turn and its thing lists (main app, SimTheme Park 2.04b3, World.cpp; unit w121): CWorld::DoGameTurn, DoOnlineGameTurn,
// UpdateThingsInEngine, UpdateBumperCarsInEngine, AddThingToThingList, _CThingArrayEntry::FreeMe, DeleteAllThings, DeletePendingThings,
// GetNextUsedThing, GetUsedThingHead, IsParkClosed and ParkGates_GetStatus.
//
// The thing table (gThingArray, five words an entry on the Mac) holds a doubly linked list of the used entries, newest first, and a singly
// linked list of the free ones. Every walk below follows the link of the row named by the THING'S OWN ID (what CThing::GetThingId answers),
// which is the row the thing sits in only while the id and the row agree. Every walk asserts, once per row, that entry 0 is free; the
// release GAME_ASSERTMSG does nothing, so the assert changes only the call log.

namespace {

// The "invalid thing id" that ends a walk. The Mac reads it from seven separate 16 bit words in initialised data (0x102a4610, 0x102a4612,
// 0x102a4614, 0x102a4616, 0x102a46c0, 0x102a46c2, 0x102a46d8, one for each function that uses it), all 0 and never written.
constexpr uint16_t kNoThing = 0;

// Fields of the thing object that CThing does not name yet (Mac offsets): a CObject's script handle and, for a kind 3 thing, a ride id word.
constexpr std::size_t kObjectScriptOffset = 0x20;
constexpr std::size_t kObjectRideOffset = 0x24;

// The strings at 0x10247498 + the offset in the comment.
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x151
const char kUsedSlotText[] = "Trying to use already-used slot\n";                                        // +0x194
const char kFreeEmptyText[] = "Trying to free empty slot\n";                                             // +0x1b5
const char kArrayFullText[] = "FATAL ERROR - Thing Array is full!  Consider increasing its size!\n";    // +0x1d0
const char kAddingText[] = "Adding thing %d to thing list\n";                                            // +0x213
const char kRemovingText[] = "Removing thing %d from thing list\n";                                      // +0x232
const char kStaffText[] = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Counting staff outside park\n";                                       // +0x255 (94 marks)

// The index word of an entry, cut to 16 bits (the Mac loads the whole word and stores a halfword); 0 for no entry.
uint16_t IndexOf(const CThingArrayEntry* entry) { return entry != nullptr ? static_cast<uint16_t>(entry->mF04[0]) : uint16_t(0); }

int32_t ReadWord(const CThing* thing, std::size_t offset) {
    int32_t word;
    std::memcpy(&word, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof word);
    return word;
}

// The staff kinds 4 to 8 (the Mac builds this as a chain of tests on 4, 5, 6, 8 and 7).
bool IsStaffKind(uint8_t kind) { return kind >= 4 && kind <= 8; }

}  // namespace

// 0x1013adb0 (40 bytes). The id of the newest used entry, 0 when the list is empty. Does not read the world.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const { return {IndexOf(gUsedThingHead)}; }

// 0x1013ae08 (112 bytes). The id of the used entry after the one of the thing's own id, 0 at the end of the list. The thing is not
// tested for NULL and the world is not read.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    const uint16_t id = thing->GetThingId().mIndex;
    return {IndexOf(gThingArray[id].mNextUsed)};
}

// The world's sequences and small helpers (main app, SimTheme Park 2.04b3, World.cpp; unit w194): CWorld::PopulatePark,
// StartingPopulationForOnline, StartBankruptcySequence, StartWinningSequence, IsPlayingWinningAnim, EndWinningSequence, GetPrevUsedThing,
// SeedRand, ReconstructFreeList, GetNewPersonIndex, FlagItemAsSelected, ConvertDLRidesUsedToIndices, RestoreEngineObjects and
// DeleteEngineObjects, and the pending list's TbTList::RemoveHead and AddHead (the list of thing ids that CThing::KillMe queues).
//
// The three engine walks (the bankruptcy, Restore and Delete) follow the used list of the thing table exactly as src/World_turn.cpp's
// UpdateThingsInEngine does: each row asserts that entry 0 is free, then the thing is tested, then the walk follows the link of the row
// named by the THING'S OWN ID (GetThingId is called even for a NULL thing). The release GAME_ASSERTMSG does nothing.

// The two globals of World.cpp this unit owns (original names unknown): the id of the thing that plays the winning animation (the "End"
// feature's object; 0 for none) and the ride database name of that feature. __sinit_World_cpp (0x10141c14) builds the string on the Mac;
// until it is translated it is defined here with its text, as World_setup.cpp does for the gates and the lights.
uint16_t gWinningAnimThing = 0;                                // 0x1034cad4
TbDynamicStringTemplate<char16_t> gWorldEndName(u"End");       // 0x1034cad8

namespace {

const char kNoFeatureText[] = "Could not find the %s feature\n";                                          // +0x2d1
const char kEndBeforeStartText[] = "Calling EndWinningSequence before StartWinningSequence\n";           // +0x2f0
const char kVisitorText[] = "Your park has received its %dth visitor!\n";                                 // +0x77f
const char kNotInDatabaseText[] = "You've managed to build a ride without it being in the ride database";   // +0x7a9 (no newline)
const char kDigits[] = "0123456789";                                                                      // +0x7ee

// The world's generator as the Mac inlines it into PopulatePark and StartingPopulationForOnline (the same as CWorld::GetRand, which these
// two do not call): state = state * 0x19660d + 0x3c6ef35f, stored; the answer is the state's absolute value.
uint32_t NextRand(uint32_t& state) {
    state = state * 0x0019660du + 0x3c6ef35fu;
    return static_cast<int32_t>(state) >= 0 ? state : 0u - state;
}

// A thing of `size` bytes (the Mac's size: the host classes are placeholders), made by its constructor over the block; the result is dropped
// (the thing enters the thing table itself). The Mac tests the allocation for NULL and skips the constructor then; the host's never fails.
template <class T, class... Args> void MakeThing_2(std::size_t size, Args... args) {
    void* block = tpw::ArenaAlloc(size);   // the Mac's game heap: the arena (a list keeps a thing's address in 32 bits)
    new (block) T(args...);
}

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

}  // namespace

// 0x1013aeb0 (112 bytes). The id of the used entry before the one of the thing's own id, 0 at the head of the list (the twin of
// GetNextUsedThing). The thing is not tested for NULL and the world is not read.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetPrevUsedThing(const CThing* thing) const {
    const uint16_t id = thing->GetThingId().mIndex;
    return {IndexOf(gThingArray[id].mPrevUsed)};
}

// 0x1013b054 (200 bytes). Takes the first free entry, makes it the head of the used list and stores the thing in it; answers the entry's
// index (0 when the table is full). The new entry's previous link is not written: a stale value survives (FreeMe clears it).
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::AddThingToThingList(CThing* thing) {
    CThingArrayEntry* entry = gFreeThingHead;
    if (entry == nullptr) {
        GAME_ASSERTMSG(0, kArrayFullText);
        return {0};
    }
    GAME_ASSERTMSG(entry->mThing == nullptr, kUsedSlotText);
    gFreeThingHead = entry->mNextFree;
    entry->mNextFree = nullptr;
    entry->mNextUsed = gUsedThingHead;
    if (gUsedThingHead != nullptr) gUsedThingHead->mPrevUsed = entry;
    gUsedThingHead = entry;
    entry->mThing = thing;
    CTRACE(2, kAddingText, gUsedThingHead->mF04[0]);
    return {static_cast<uint16_t>(gUsedThingHead->mF04[0])};
}

// 0x1013af58 (192 bytes). Unlinks the entry from the used list and pushes it on the free list, then releases the thing with the plain
// operator delete (no destructor runs) and clears the pointer. The Mac's delete gives the block back to the game heap: on the host that is
// the arena, where the things are made (unit v5L31); a block from anywhere else goes back to the host heap. The used head is overwritten
// whenever the entry's previous link is NULL, even for an entry that is not the head (a double free).
void CThingArrayEntry::FreeMe() {
    GAME_ASSERTMSG(mThing != nullptr, kFreeEmptyText);
    if (mNextUsed != nullptr) mNextUsed->mPrevUsed = mPrevUsed;
    if (mPrevUsed != nullptr) mPrevUsed->mNextUsed = mNextUsed;
    else gUsedThingHead = mNextUsed;
    mNextUsed = nullptr;
    mPrevUsed = nullptr;
    mNextFree = gFreeThingHead;
    gFreeThingHead = this;
    if (tpw::InArena(mThing)) tpw::ArenaFree(mThing);
    else ::operator delete(mThing);
    mThing = nullptr;
}

// 0x1013ac4c (308 bytes). Kills every live thing of the used list (an alive thing makes three entry 0 asserts, a dead one two), then frees
// what they queued. The word 0x1da738 is 2 while this runs and 1 after, whatever it held.
void CWorld::DeleteAllThings() {
    mF1DA738 = 2;
    uint16_t cur = GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        if (static_cast<CThing*>(gThingArray[cur].mThing)->mDead == 0) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            static_cast<CThing*>(gThingArray[cur].mThing)->KillMe();
        }
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        cur = GetNextUsedThing(static_cast<CThing*>(gThingArray[cur].mThing)).mIndex;
    }
    DeletePendingThings();
    mF1DA738 = 1;
}

// CWorld::AddToPendingDeleteList 0x1013b154 (48 bytes) and CWorld::GetRand 0x1013b1e4 (56 bytes), main app, unit w123c (the inventory files
// both under World.cpp).

// The id goes on the global pending-delete list (gPendingDeleteList, not a member of the world: the world pointer is never read). The
// list's AddHead 0x10141ad4 is an external callee (its specialization is declared in WorldThings.h, so the generic body is not built).
void CWorld::AddToPendingDeleteList(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { gPendingDeleteList.AddHead(id); }

// The generator is a linear congruence on the word at +0x1da708 (mStartTime, which SetupNewLevel seeds with time(NULL)): the new state is
// state * 0x19660d + 0x3c6ef35f in 32 bits. The original stores it twice (the same word) and reads it back. The answer is the state's
// absolute value taken as a signed word, but the stored state stays negative; 0x80000000 is its own negation. Unsigned arithmetic throughout:
// the signed overflow of the original is wrapping.
uint32_t CWorld::GetRand() {
    const uint32_t state = mStartTime * 0x0019660du + 0x3c6ef35fu;
    mStartTime = state;   // (stored twice and read back by the original: the repeat cannot show)
    return static_cast<int32_t>(state) >= 0 ? state : 0u - state;
}

// 0x1013b244 (12 bytes). Seeds the generator.
void CWorld::SeedRand(uint32_t seed) { mStartTime = seed; }

// 0x1013b278 (872 bytes). One turn: the counter, every live thing's Process, the closing of the park gates when it is closed, nobody is
// inside and no staff member is outside, then the pending deletions, the macro AI and the action recorder.
void CWorld::DoGameTurn() {
    mTurn = mTurn + 1;
    if (mF1DA738 != 4) {
        uint16_t cur = IndexOf(gUsedThingHead);
        while (cur != kNoThing) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
            if (thing != nullptr && thing->mDead == 0) thing->Process();
            const uint16_t id = thing->GetThingId().mIndex;
            cur = IndexOf(gThingArray[id].mNextUsed);
        }
    }
    // Every 30th turn, with a gates object, the park closed, its gates open (status 1) and no visitor inside: if no live staff member is
    // outside the park, close the gates. The scan stops at the first outsider without asking for its id, and does not test for NULL.
    if (mGatesThing != kNoThing && mTurn % 30 == 0 && IsParkClosed() != 0 && ParkGates_GetStatus() == 1 &&
        GetParkAnalyser()->NumberOfVisitorsInPark() == 0) {
        CTRACE(2, kStaffText);
        bool outsider = false;
        uint16_t cur = IndexOf(gUsedThingHead);
        while (cur != kNoThing) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
            if (thing->mDead == 0 && IsStaffKind(thing->mKind) && reinterpret_cast<CPeep*>(thing)->IsInsidePark() == 0) {
                outsider = true;
                break;
            }
            const uint16_t id = thing->GetThingId().mIndex;
            cur = IndexOf(gThingArray[id].mNextUsed);
        }
        if (!outsider) ParkGates_Close(0);
    }
    DeletePendingThings();
    reinterpret_cast<CMacroAI*>(this)->Update();
    gActionRecorder.DoTick();
}

// 0x1013b608 (116 bytes). A game turn when the game type is the online one (the game type object is built on first use).
void CWorld::DoOnlineGameTurn() {
    if (TheGameType().mType == kGameTypeOnline) DoGameTurn();
}

// 0x1013b6ac (436 bytes). Fills the park with its first people: when the gates are not open (status other than 1) the park is only opened
// (SetParkOpenOrClosed(1, 0)). Else every map cell (x outer, y inner) that is inside the level and is a park entrance makes a mechanic
// (even random) or a handyman (odd), and a path cell makes a person; nothing else does. The things enter the thing table themselves.
void CWorld::PopulatePark() {
    if (ParkGates_GetStatus() != 1) {
        SetParkOpenOrClosed(1, 0);
        return;
    }
    for (int32_t x = 0; x < 0x80; ++x) {
        for (int32_t y = 0; y < 0x80; ++y) {
            if (CMap::IsXYValid(x, y) == 0) continue;
            const uint16_t cell = static_cast<uint16_t>(x + y * 0x80 + 1);
            const CMapCell& c = mMapCells[cell - 1];
            if (c.IsEntrance() != 0) {
                if ((NextRand(mStartTime) & 1) == 0) MakeThing_2<CMechanic>(0x218, MapId{cell});
                else MakeThing_2<CHandyman>(0x218, MapId{cell});
            } else if (c.IsPath() != 0) {
                MakeThing_2<CPerson>(0x22c, MapId{cell});
            }
        }
    }
}

// 0x1013b88c (476 bytes). The people of an online park: the park is opened, then 25 people at the ticket booths, then for every ride in the
// park (the analyser's count; none for 0 or less) a mechanic and an entertainer at the park entrances. The order of each is: the block, the
// random number, the position, the constructor.
void CWorld::StartingPopulationForOnline() {
    SetParkOpenOrClosed(1, 0);
    for (int32_t i = 0; i < 25; ++i) {
        void* block = tpw::ArenaAlloc(0x22c);
        const uint32_t r = NextRand(mStartTime);
        new (block) CPerson(GetMap()->GetTicketBoothPos(static_cast<int32_t>(r & 1)));
    }
    const int32_t rides = GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    for (int32_t i = 0; i < rides; ++i) {
        void* mechanic = tpw::ArenaAlloc(0x218);
        uint32_t r = NextRand(mStartTime);
        new (mechanic) CMechanic(GetMap()->GetParkEntrancePos(static_cast<int32_t>(r & 1)));
        void* entertainer = tpw::ArenaAlloc(0x214);
        r = NextRand(mStartTime);
        new (entertainer) CEntertainer(GetMap()->GetParkEntrancePos(static_cast<int32_t>(r & 1)));
    }
}

// 0x1013baa4 (396 bytes). The state word becomes 4, the congratulation window of bankruptcy is told (with the gates object, NULL when its
// row is empty) and the sprites stop drawing; every live ride (kind 3) whose flag 4 (choosable) is set is asked to break down; the pending
// things are deleted and the park is closed.
void CWorld::StartBankruptcySequence() {
    const uint16_t gatesId = mGatesThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    CObject* gates = reinterpret_cast<CObject*>(gThingArray[gatesId].mThing);
    mF1DA738 = 4;
    gUIInterface.Bankrupt(gates);
    ESP_DisableSpriteDraw();
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3) {
            CObject* object = reinterpret_cast<CObject*>(thing);
            if ((object->mFlags & 4) != 0) object->RequestBreakdown();
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
    DeletePendingThings();
    SetParkOpenOrClosed(0, 1);
}

// 0x1013bc68 (308 bytes). Looks the "End" feature up in the ride database and makes its object (a CObject, 0x448 bytes, on map cell 1);
// its thing id is kept in gWinningAnimThing. When the database has none the assertion fires (its copy of the name is passed to the
// assertion by value on the Mac) and the id is cleared. The congratulation window is told either way, with a NULL object when none.
void CWorld::StartWinningSequence() {
    CObject* object = nullptr;
    RideInfo* ride = FindRide(gWorldEndName);
    if (ride == nullptr) {
        const TbDynamicStringTemplate<char16_t> copy(gWorldEndName);
        GAME_ASSERTMSG(0, kNoFeatureText, copy.mData);
        gWinningAnimThing = 0;
    } else {
        void* block = tpw::ArenaAlloc(0x448);
        object = new (block) CObject(MapId{1}, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
        gWinningAnimThing = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    }
    gUIInterface.Congratulations(object);
}

// 0x1013bdd0 (40 bytes). True while the "End" feature's object plays its animation.
bool CWorld::IsPlayingWinningAnim() { return gWinningAnimThing != kNoThing; }

// 0x1013be2c (188 bytes). Kills the winning animation's thing (the table row of gWinningAnimThing; not tested for NULL), clears the id and
// leaves the first person view of a ride.
void CWorld::EndWinningSequence() {
    GAME_ASSERTMSG(gWinningAnimThing != kNoThing, kEndBeforeStartText);
    const uint16_t id = gWinningAnimThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    static_cast<CThing*>(gThingArray[id].mThing)->KillMe();
    gWinningAnimThing = 0;
    StopRidingRideIn1stPerson();
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

template<> int32_t LOADVAR<CWorld::WorldState>(void* file, CWorld::WorldState* value, const char* name, int32_t noLog);

template<> int32_t LOADVAR<CThing::ThingModel>(void* file, CThing::ThingModel* value, const char* name, int32_t noLog);

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
            void* block = tpw::ArenaAlloc(0x22c);
            if (block != nullptr) InitPerson(block);
            if (!ReadThing<CPerson>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 3: {
            void* block = tpw::ArenaAlloc(0x448);
            if (block != nullptr) InitObject(block);
            if (!ReadThing<CObject>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 4: {
            void* block = tpw::ArenaAlloc(0x218);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CMechanic>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 5: {
            void* block = tpw::ArenaAlloc(0x218);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CHandyman>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 6: {
            void* block = tpw::ArenaAlloc(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CEntertainer>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 7: {
            void* block = tpw::ArenaAlloc(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CGuard>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 8: {
            void* block = tpw::ArenaAlloc(0x214);
            if (block != nullptr) InitStaff(block);
            if (!ReadThing<CResearcher>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 9: {
            void* block = tpw::ArenaAlloc(0x68);
            if (block != nullptr) InitStaffHQ(block);
            if (!ReadThing<CStaffHQ>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 10:
            if (!ReadThing<CMechanicHQ>(entry, tpw::ArenaAlloc(0xe), kind, id, file, fileVersion)) return 0;
            break;
        case 11: {
            void* block = tpw::ArenaAlloc(0x16f0);
            if (block != nullptr) InitAdvisor(block);
            if (!ReadThing<CAdvisor>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 12:
            if (!ReadThing<CTagSystem>(entry, tpw::ArenaAlloc(0x80e), kind, id, file, fileVersion)) return 0;
            break;
        case 13: {
            void* block = tpw::ArenaAlloc(0x21c48);
            if (block != nullptr) InitParkAnalyser(block);
            if (!ReadThing<CParkAnalyser>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 14: {
            void* block = tpw::ArenaAlloc(0x146c);
            if (block != nullptr) InitResearchLab(block);
            if (!ReadThing<CResearchLab>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 15: {
            void* block = tpw::ArenaAlloc(0x68);
            if (block != nullptr) InitWeather(block);
            if (!ReadThing<CWeather>(entry, block, kind, id, file, fileVersion)) return 0;
            break;
        }
        case 16:
            if (!ReadThing<CBankAccount>(entry, tpw::ArenaAlloc(0x128), kind, id, file, fileVersion)) return 0;
            break;
        case 17:
            if (!ReadThing<CUIMsgReceiver>(entry, tpw::ArenaAlloc(0xc), kind, id, file, fileVersion)) return 0;
            break;
        case 18:
            GAME_ASSERTMSG(0, "Should not be able to load online persons\n");   // 0x6ad
            break;
        case 19: {
            void* block = tpw::ArenaAlloc(0x3e4);
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

// 0x1013dd14 (96 bytes). Rebuilds the free list from the table: every entry from 1 to 0x27ff gets its next-free link cleared and, when it
// holds no thing, is chained after the previous free one (the first one becomes the head). Entry 0 is never touched, and the head is not
// written when no entry is free.
void CWorld::ReconstructFreeList() {
    uint32_t last = 0xffffffffu;
    for (uint32_t i = 1; i < 0x2800; ++i) {
        gThingArray[i].mNextFree = nullptr;
        if (gThingArray[i].mThing == nullptr) {
            if (last == 0xffffffffu) gFreeThingHead = &gThingArray[i];
            else gThingArray[last].mNextFree = &gThingArray[i];
            last = i;
        }
    }
}

// 0x1013dda8 (148 bytes). Frees the entry of every id the pending list holds (CThing::KillMe queues them). The world is not read.
void CWorld::DeletePendingThings() {
    while (gPendingDeleteList.mCount != 0) {
        const uint16_t id = gPendingDeleteList.RemoveHead().mIndex;
        CTRACE(2, kRemovingText, static_cast<uint32_t>(id));
        gThingArray[id].FreeMe();
    }
}

// 0x1013de70 (504 bytes). Moves the people and the staff (kinds 1, 4 to 8 and 0x12) that are alive; `step` goes through unchanged.
// A NULL thing in the list is skipped, but GetThingId is still asked about it (the Mac would read address 0 there).
void CWorld::UpdateThingsInEngine(float step) {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0) {
            const uint8_t kind = thing->mKind;
            if (kind == 1 || IsStaffKind(kind) || kind == 0x12) reinterpret_cast<CPeep*>(thing)->UpdatePositionInEngine(step);
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013e09c (316 bytes). Draws the cars of every live kind 3 thing (a CObject) whose ride id word at +0x24 is nonzero.
void CWorld::UpdateBumperCarsInEngine(float step) {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3 && ReadWord(thing, kObjectRideOffset) != 0)
            BMP_DrawCarsForRide(ReadWord(thing, kObjectRideOffset), step);
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013e210 (292 bytes). Every live ride (kind 3) is told to update its engine object (flag 1).
void CWorld::RestoreEngineObjects() {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3) reinterpret_cast<CObject*>(thing)->UpdateEngine(1);
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013e368 (528 bytes). Every live thing of kind 1 (a person), 4 to 8 (the staff) or 0x12 whose word at +0xc (its CDelayedScript's
// script handle) is not 0 gets its sprite script killed (the host classes do not name the member: the address of +0xc is a
// CDelayedScript); every other live ride (kind 3) gives its ride instance (the word at +0x1c, CObject::mF1C) back.
void CWorld::DeleteEngineObjects() {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0) {
            const uint8_t kind = thing->mKind;
            if (kind == 1 || IsStaffKind(kind) || kind == 0x12) {
                if (ReadWord(thing, 0xc) != 0)
                    reinterpret_cast<CDelayedScript*>(reinterpret_cast<unsigned char*>(thing) + 0xc)->KillSpriteScript();
            } else if (kind == 3) {
                DestroyRideInstance(reinterpret_cast<CObject*>(thing)->mF1C);
            }
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// The chunk file reader and the attribute map load (main app of SimTheme Park 2.04b3; the inventory files these under
// Entertainer.cpp, lobby.cpp, MapDelta.cpp and World.cpp, unreliable; unit w112c): CChunkFileReader, its handler classes, the list
// members it uses and CMap::LoadAttribMap, which reads the level's attribute map ("Base.map") into the cells' attribute bytes.

// 0x1013e5ac (216 bytes). The reader and its one handler are local objects: the magic is "TP2M" in the file, the chunk id "MAP ".
// The handler is added, the file read and the reader destroyed (its list nodes freed) before the answer is returned.
int32_t CMap::LoadAttribMap(const char* path) {
    CMapAttribFileReader reader(this);
    reader.AddChunkHandler(&reader.mHandler);
    return reader.ReadFile(path);
}
// Unit w205. 0x1013e6b0 (112 bytes): the Mac copy tests `this` twice (the second test is redundant), sets the list's vtable word, destroys the
// list part (RemoveAll) and deletes the object when its flag is positive. It does not run ~CChunkFileReader (0x1013e754); the host destructor
// does (an empty body), then the base chain's. The handler member's destructor is the implicit, trivial one.
CMapAttribFileReader::~CMapAttribFileReader() {}

// 0x1013e754 (108 bytes): the Mac copy sets the list's vtable word, destroys the list part (RemoveAll) and deletes the object when
// its flag is positive. The host destructor is the base chain's.
CChunkFileReader::~CChunkFileReader() {}

// The park's second initialisation and its teardown (main application, TPRide.cpp range; unit w110): LevelInit2 and LevelDeInit,
// with the four height field accessors of CWorld they read the level's place and size through.

namespace {
// The main balance's value slots for the four height field rows: the Mac's byte offsets 0x544, 0x548, 0x54c and 0x550, less the
// table's 8 bytes of header, over four (src/ChunkFile.cpp reads the first two with the same numbers).
constexpr int kBalanceHeightfieldXStart = (0x544 - 8) / 4;
constexpr int kBalanceHeightfieldYStart = (0x548 - 8) / 4;
constexpr int kBalanceHeightfieldWidth = (0x54c - 8) / 4;
constexpr int kBalanceHeightfieldHeight = (0x550 - 8) / 4;
}  // namespace

// 0x1013e89c, 0x1013e8dc, 0x1013e91c and 0x1013e95c (12 bytes each): the world is not used, the answer is a word of gMainBalance.
int32_t CWorld::GetHeightfieldXStart() const { return gMainBalance.mValues[kBalanceHeightfieldXStart].mInteger; }
int32_t CWorld::GetHeightfieldYStart() const { return gMainBalance.mValues[kBalanceHeightfieldYStart].mInteger; }
int32_t CWorld::GetHeightfieldWidth() const { return gMainBalance.mValues[kBalanceHeightfieldWidth].mInteger; }
int32_t CWorld::GetHeightfieldHeight() const { return gMainBalance.mValues[kBalanceHeightfieldHeight].mInteger; }

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

// World.cpp (main app): CWorld::GetAdvisor, 0x1013eab0.

static_assert(offsetof(CWorld, mAdvisorThing) == 0x1da71c, "the advisor's row index sits at +0x1da71c");

// The advisor is a thing: the world keeps its row of the thing table in a short (read before the check, as the Mac does). Entry 0 must stay
// free (an index of 0 means "no thing"); the check only reports. The answer is the row's first word, which is 0 when the row is free
// (a world whose advisor was never made answers NULL), and the index is not checked against the table's size.
CAdvisor* CWorld::GetAdvisor() {
    const uint16_t index = mAdvisorThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<CAdvisor*>(gThingArray[index].mThing);
}

// The research lab's projects, efforts and outputs (main app, unit w211): 21 CResearchLab functions (0x10121b7c FinalDestroy to 0x10123a5c
// GetResearchOutputFromCategory) and 5 CResearchProject ones (0x10123d7c SetupProject to 0x1012420c GetRemainingPoints). They choose a
// category's next or previous project, share the researchers' points over the categories, raise a category's level, answer the research
// panel, run the cheats, keep the lab's active flag, and set up, advance and measure a project.
//
// The lab keeps 0x100 projects of 0x14 bytes at +0xc and five categories of 0x10 bytes at +0x140c (an effort, an output, the index of the
// current project and the highest group open). A project is done when its progress (a float) has reached its score. Every project needs a
// ride record (CRideDatabase) and every finished one makes the ride's object control available (CControlManager).

// CWorld's thing getters, main app 0x1013eb38..0x1013ee88 (World.cpp), unit w123b. The same code as GetAdvisor (src/World_GetAdvisor.cpp, unit w103).

static_assert(offsetof(CWorld, mParkAnalyserThing) == 0x1da720, "the analyser's row index sits at +0x1da720");
static_assert(offsetof(CWorld, mResearchLabThing) == 0x1da722, "the research lab's row index sits at +0x1da722");
static_assert(offsetof(CWorld, mWeatherThing) == 0x1da724, "the weather's row index sits at +0x1da724");
static_assert(offsetof(CWorld, mBankAccountThing) == 0x1da726, "the bank account's row index sits at +0x1da726");

namespace {
// The text of the assertion at World.cpp string base + 0x151 (0x102475e9).
constexpr const char* kRowZeroText = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

// The seven getters in one body: the halfword id is read BEFORE the assertion (the saved copy is used even if the assertion changes the world), the
// assertion is made on row 0 being free, and word 0 of the saved id's row is read AFTER it (so a change the assertion made to the row shows). The id is
// not checked against the table's size and a failed assertion does not change the answer.
void* RowThing(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    return gThingArray[id].mThing;
}
}  // namespace

const CParkAnalyser* CWorld::GetParkAnalyser() const {
    const uint16_t id = mParkAnalyserThing;
    return static_cast<const CParkAnalyser*>(RowThing(id));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    const uint16_t id = mParkAnalyserThing;
    return static_cast<CParkAnalyser*>(RowThing(id));
}

// The park analyser's queries over the objects and the ride database (main app, analyser.cpp range; unit w151): 26 functions from
// CParkAnalyser::NumberOfToiletsInPark 0x100e8f28 to SetLastOfObjectTypeResearched 0x100ea968.
//
// Three walks, all with the call sequence of the listings:
//  * the used-thing walk of HowBusyIsPark (src/ParkAnalyser_readers.cpp): GetWorld().GetUsedThingHead(), then per thing GetWorld(), the assertion
//    that entry 0 of the thing table is free, the thing from gThingArray[id].mThing, the test, GetWorld().GetNextUsedThing(thing). The end id
//    is a halfword in initialised data (a different one per function), all 0 and never written. An object is a thing of kind 3.
//  * the object list walk (NumberOfBrokenDownRidesInPark, NumberOfWornRidesInPark): from the world's halfword at +0x1da746 (the head of the
//    object list), each object's halfword at +0x0c is the next id, 0 ends. No kind test.
//  * the ride database walk: GetFirstRide, GetNextRide, and per ride the object control of its id, from the CONST GetObjectControl (0x100fa1b8)
//    of the control manager at world + 0x1d943c. A ride record's golden ticket slot (46, +0xc4) is read as a signed word.

const CResearchLab* CWorld::GetResearchLab() const {
    const uint16_t id = mResearchLabThing;
    return static_cast<const CResearchLab*>(RowThing(id));
}

CResearchLab* CWorld::GetResearchLab() {
    const uint16_t id = mResearchLabThing;
    return static_cast<CResearchLab*>(RowThing(id));
}

CWeather* CWorld::GetWeather() {
    const uint16_t id = mWeatherThing;
    return static_cast<CWeather*>(RowThing(id));
}

CBankAccount* CWorld::GetBankAccount() {
    const uint16_t id = mBankAccountThing;
    return static_cast<CBankAccount*>(RowThing(id));
}

// The halfword at +0x1da736. The neighbours at +0x1da728 (the UI receiver) and +0x1da738 (the world state word) are not read.
CChallengeHQ* CWorld::GetChallengeHQ() {
    const uint16_t id = mChallengeHQThing;
    return static_cast<CChallengeHQ*>(RowThing(id));
}

// CWorld::Register, the six overloads (main app, SimTheme Park 2.04b3, World.cpp; unit w139): 0x1013ef14 CHandyman, 0x1013f144 CMechanic,
// 0x1013f374 CEntertainer, 0x1013f5ac CGuard, 0x1013f7d8 CResearcher and 0x1013fa10 CObject, 116 bytes each, all the same code.
//
// The world keeps a singly linked list of each kind of thing, newest first: the head is a halfword thing id in the world. Register reads the
// old head, asks the thing for its id (CThing::GetThingId), makes that id the new head, and only then reads the list's end sentinel (a
// halfword constant in the data, 0 on the Mac: 0x102a46c8 and the next five), and, when the old head was not the sentinel, writes the old head
// into the new thing as its link: +0x20c (a halfword) for the five staff kinds, +0xc for an object.

namespace {

constexpr uintptr_t kStaffNextOffset = 0x20c;  // the staff block's link to the next staff member of its kind
constexpr uintptr_t kObjectNextOffset = 0xc;   // the object's link

// The staff classes have no members (the staff block is only reached through the thing), so the thing is viewed by address.
template <class T>
void RegisterThing(T* thing, uint16_t& head, uintptr_t nextOffset) {
    const uint16_t old = head;
    head = reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
    if (old != kNoThing) {
        *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(thing) + nextOffset) = old;
    }
}

}  // namespace

void CWorld::Register(CHandyman* staff) { RegisterThing(staff, mF1DA73C, kStaffNextOffset); }

// CWorld::Unregister, the six overloads (main app, SimTheme Park 2.04b3, World.cpp; unit w178): 0x1013efb8 CHandyman, 0x1013f1e8 CMechanic,
// 0x1013f41c CEntertainer, 0x1013f650 CGuard, 0x1013f880 CResearcher and 0x1013fab4 CObject, 344 bytes each, all the same code.
//
// The reverse of CWorld::Register (src/World_register.cpp): the world's list of one kind of thing runs from the newest to the oldest through a
// halfword thing id in each member (+0x20c for the five staff kinds, +0xc for an object). Unregister takes the thing out of the list: when it is
// the head the head becomes its link; otherwise the walk from the head finds the member just before it and gives that member the thing's link.
// Every step of the walk (and the unlinking) first asserts that gThingArray[0] holds no thing, with the message below. The order of the reads
// around those assertions is the listing's: the head is read before the first assertion and its member's link after it; the thing's link is
// read after the last assertion.

namespace {

void AssertRowZeroEmpty() {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
}

uint16_t& LinkOf(uint16_t id, uintptr_t nextOffset) {
    return *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(gThingArray[id].mThing) + nextOffset);
}

template <class T>
void UnregisterThing(T* thing, uint16_t& head, uintptr_t nextOffset) {
    const uint16_t id = reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
    uint16_t current = head;
    if (current == id) {
        const uint16_t first = head;
        AssertRowZeroEmpty();
        head = LinkOf(first, nextOffset);
    } else {
        uint16_t previous;
        do {
            previous = current;
            AssertRowZeroEmpty();
            current = LinkOf(previous, nextOffset);
        } while (current != id);
        AssertRowZeroEmpty();
        void* object = gThingArray[id].mThing;
        AssertRowZeroEmpty();
        const uint16_t link = *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(object) + nextOffset);
        LinkOf(previous, nextOffset) = link;
    }
}

}  // namespace

void CWorld::Unregister(CHandyman* staff) { UnregisterThing(staff, mF1DA73C, kStaffNextOffset); }

void CWorld::Register(CMechanic* staff) { RegisterThing(staff, mF1DA73E, kStaffNextOffset); }

void CWorld::Unregister(CMechanic* staff) { UnregisterThing(staff, mF1DA73E, kStaffNextOffset); }

void CWorld::Register(CEntertainer* staff) { RegisterThing(staff, mF1DA740, kStaffNextOffset); }

void CWorld::Unregister(CEntertainer* staff) { UnregisterThing(staff, mF1DA740, kStaffNextOffset); }

void CWorld::Register(CGuard* staff) { RegisterThing(staff, mF1DA744, kStaffNextOffset); }

void CWorld::Unregister(CGuard* staff) { UnregisterThing(staff, mF1DA744, kStaffNextOffset); }

void CWorld::Register(CResearcher* staff) { RegisterThing(staff, mF1DA742, kStaffNextOffset); }

void CWorld::Unregister(CResearcher* staff) { UnregisterThing(staff, mF1DA742, kStaffNextOffset); }

void CWorld::Register(CObject* object) { RegisterThing(object, mF1DA746, kObjectNextOffset); }

void CWorld::Unregister(CObject* object) { UnregisterThing(object, mF1DA746, kObjectNextOffset); }

// The park gates and the arrival vehicle (main app, SimTheme Park 2.04b3, World.cpp; unit w193): CWorld::SetParkOpenOrClosed (0x1013fc3c),
// ParkGates_Open (0x1014007c), ParkGates_Close (0x1014013c), ArrivalVehicle_DoNextLeg (0x10140234), ArrivalVehicle_GetStatus (0x101406cc),
// ArrivalVehicle_CrossingIsClear (0x1014080c), ArrivalVehicle_PeopleWaitingToLeave (0x10140b6c), ArrivalVehicle_PeopleLeftToDropOff
// (0x10140d98) and ArrivalVehicle_IsSeagoing (0x10140de0).
//
// The gates are the "gates" feature object (mGatesThing): its RSSE script's variable 0 is the order (0 close, 1 open, 2 close after the
// visitors have gone) and variable 1 the state. The arrival vehicle is one of three feature objects of the ride database ("bus", "seaplane",
// "Ferry"), made on demand and driven through the script's variable 0 (go) and 1 (status). Every walk or read of the thing table asserts that
// entry 0 is free first; the release GAME_ASSERTMSG does nothing, so the assert changes only the call log. Every callee outside the unit is
// untranslated or translated elsewhere (the thing virtuals, the script variables, the message centre, the ride database, the park analyser).

// The three vehicle names (see World.h): __sinit_World_cpp (0x10141c14) builds the strings on the Mac; until it is translated they are
// defined here with their texts.
TbDynamicStringTemplate<char16_t> gArrivalBusName(u"bus");
TbDynamicStringTemplate<char16_t> gArrivalSeaplaneName(u"seaplane");
TbDynamicStringTemplate<char16_t> gArrivalFerryName(u"Ferry");

namespace {

const char kClosedText[] = "closed";                                                                   // +0x6d8
const char kOpenedText[] = "opened";                                                                   // +0x6df
const char kParkText[] = "\n*** You have just %s your park ***\n";                                     // +0x6e6
const char kNoVehicleText[] = "Fatal error: Could not find either the bus or the plane feature";      // +0x70b
const char kNoFeatureText_2[] = "Could not find the 'vehicle_name was here' feature\n";                  // +0x74b

// The trace level of the park message (a magic number of the original).
constexpr uint32_t kParkTrace = 0x75bcd15;

// The kind byte of a person and the word of its state at +0x220 (the names are those of src/ParkAnalyser_visitors.cpp); 0x15 is the state
// of a visitor waiting to leave by the vehicle.
constexpr uint8_t kKindPerson = 1;
constexpr uint32_t kPeepState = 0x220;
constexpr int32_t kPeepStateGettingOff = 0x15;
constexpr int32_t kCellBytes = 0x44;

// The message centre the Mac builds on first use (src/BankAccount.cpp has the same helper).
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The thing of a table row, after the assert on row 0 that every read makes. A free row answers NULL (the Mac then reads through it).
void* RowThing_2(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    return gThingArray[id].mThing;
}

CObject* ObjectOfRow(uint16_t id) { return static_cast<CObject*>(RowThing_2(id)); }

// The script of an object: the handle at +0x20 is read at each use.
RSSE_SCRIPT* ScriptOf(const CObject* object) { return RSSE_GetScriptPointer(static_cast<int32_t>(object->mF20)); }

int32_t ReadWord_2(const void* thing, std::size_t offset) {
    int32_t word;
    std::memcpy(&word, static_cast<const unsigned char*>(thing) + offset, sizeof word);
    return word;
}

// The map cell of a 1 based cell id: the map plus (id - 1) * 0x44 (id 0 gives the 0x44 bytes before the map).
const CMapCell* CellOf(const CMap* map, uint16_t id) {
    return reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(map) +
                                              static_cast<intptr_t>((static_cast<int32_t>(id) - 1) * kCellBytes));
}

// The body of ArrivalVehicle_GetStatus: -1 without a vehicle, else variable 1 of its script; a 6 (an error) sends the vehicle away
// (variable 1 is cleared and the world forgets the vehicle) and answers -1. The Mac repeats this code inline in CrossingIsClear.
int32_t VehicleStatus(CWorld& world) {
    if (world.mArrivalVehicleThing == kNoThing) return -1;
    const CObject* vehicle = ObjectOfRow(world.mArrivalVehicleThing);
    const int32_t status = RSSE_Getvar(ScriptOf(vehicle), 1);
    if (status != 6) return status;
    if (world.mArrivalVehicleThing != kNoThing) {
        const CObject* again = ObjectOfRow(world.mArrivalVehicleThing);
        RSSE_Setvar(ScriptOf(again), 1, 0);
        world.mArrivalVehicleThing = 0;
    }
    return -1;
}

}  // namespace

// 0x1013fc3c (780 bytes). With a gate flag only the gates are closed (ParkGates_Close(flag)). Else open == 0 closes the park: the first time
// (the word at +0x1da710 was 0) the word is set, the gates are closed when nobody is in the park and the gates answer state 1, and every
// choosable object (mFlags bit 4) of the world's object list is closed. open != 0 opens it: the word is cleared, the gates opened and every
// choosable object that IsOpenable reopened. The event (4 closed, 3 opened) is posted and the trace made even when the word did not change.
// IsParkClosed is asked after the word was set, so the Mac's test is only ever true in a real run (the tests queue it).
void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) {
    if (gateFlag != 0) {
        ParkGates_Close(gateFlag);
        return;
    }
    if (open == 0) {
        if (mF1DA710 == 0) {
            mF1DA710 = 1;
            if (IsParkClosed() != 0) {
                // GetParkAnalyser is inline here: the id is read before the assert.
                const uint16_t analyserId = mParkAnalyserThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
                const CParkAnalyser* analyser = static_cast<const CParkAnalyser*>(gThingArray[analyserId].mThing);
                if (analyser->NumberOfVisitorsInPark() == 0) {
                    if (ParkGates_GetStatus() == 1) ParkGates_Close(0);
                }
            }
            uint16_t id = mF1DA746;
            while (id != kNoThing) {
                CObject* object = ObjectOfRow(id);
                if ((object->mFlags & 4) != 0) object->CloseObject();
                id = object->mF0C;
            }
        }
        CMsgEvent message(static_cast<EventType>(4));
        MessageCentreForPost()->PostMessage(message);
    } else {
        if (mF1DA710 != 0) {
            mF1DA710 = 0;
            ParkGates_Open();
            uint16_t id = mF1DA746;
            while (id != kNoThing) {
                CObject* object = ObjectOfRow(id);
                if ((object->mFlags & 4) != 0) {
                    if (object->IsOpenable() != 0) object->Reopen();
                }
                id = object->mF0C;
            }
        }
        CMsgEvent message(static_cast<EventType>(3));
        MessageCentreForPost()->PostMessage(message);
    }
    CTRACE(kParkTrace, kParkText, mF1DA710 != 0 ? kClosedText : kOpenedText);
}

// 0x1013ff7c (12 bytes). The whole word at +0x1da710 (the constructor sets 1: the park starts closed).
int32_t CWorld::IsParkClosed() const { return static_cast<int32_t>(mF1DA710); }

// 0x1013ffb4 (148 bytes). The first variable of the "gates" object's script: -1 without a gates object (nothing else happens then), else
// whatever RSSE_Getvar answers for variable 1. The object is not tested for NULL.
int32_t CWorld::ParkGates_GetStatus() const {
    const uint16_t gate = mGatesThing;
    if (gate == kNoThing) return -1;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(ReadWord(static_cast<const CThing*>(gThingArray[gate].mThing), kObjectScriptOffset));
    return RSSE_Getvar(script, 1);
}

// 0x1014007c (148 bytes). Reads variable 1 of the gates object's script (the answer is dropped) and sets variable 0 to 1. Unlike
// ParkGates_GetStatus the gates thing id is not tested: with no gates object the Mac reads through the NULL thing of row 0.
void CWorld::ParkGates_Open() {
    const CObject* gates = ObjectOfRow(mGatesThing);
    RSSE_Getvar(ScriptOf(gates), 1);
    RSSE_Setvar(ScriptOf(gates), 0, 1);
}

// 0x1014013c (200 bytes). As ParkGates_Open, but variable 0 is set to 0 for flag 0 and to 2 for any other flag.
void CWorld::ParkGates_Close(int32_t flag) {
    const CObject* gates = ObjectOfRow(mGatesThing);
    RSSE_Getvar(ScriptOf(gates), 1);
    if (flag == 0) {
        RSSE_Setvar(ScriptOf(gates), 0, 0);
    } else {
        RSSE_Setvar(ScriptOf(gates), 0, 2);
    }
}

// 0x10140234 (1120 bytes). With a vehicle in use its script variable 0 is set to 1 (go). Else a vehicle is chosen and made: leg 0 picks at
// random from the world's generator (state = state * 0x19660d + 0x3c6ef35f, then the absolute value of the state modulo 3: 0 bus, 1 seaplane,
// 2 ferry), legs 1 to 3 pick 0 to 2. The wanted feature object is the one of that type's slot (mBusThing, mSeaplaneThing, mFerryThing) when
// it exists (variable 0 is set to 1), else it is made from the ride database's record of that name on map cell 1 (variable 1 is set to 1, the
// other path's variable 0 is the Mac's own quirk) and its id kept in the slot. A name the database lacks is traced and the next type is tried
// in the order ferry, seaplane, bus; when all three have failed the Mac asserts and answers no vehicle. A leg outside 0 to 3 chooses nothing
// and the Mac then reads address 0 (not reproduced: the translation answers the vehicle in use). The answer is the vehicle in use, through a
// hidden result.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::ArrivalVehicle_DoNextLeg(int32_t leg) {
    if (mArrivalVehicleThing != kNoThing) {
        const CObject* vehicle = ObjectOfRow(mArrivalVehicleThing);
        RSSE_Setvar(ScriptOf(vehicle), 0, 1);
        return {mArrivalVehicleThing};
    }
    TbDynamicStringTemplate<char16_t> name;
    gRideDatabase.GetFirstRide();   // the answer is dropped
    uint32_t tried = 0;             // bit 1 bus, 2 seaplane, 4 ferry: the types chosen so far
    uint16_t* slot = nullptr;
    for (;;) {
        if (tried == 0) {
            int32_t choice;
            if (leg == 0) {
                mStartTime = mStartTime * 0x19660du + 0x3c6ef35fu;
                uint32_t state = mStartTime;
                if (static_cast<int32_t>(state) < 0) state = 0u - state;
                choice = static_cast<int32_t>(state % 3);
            } else {
                choice = leg - 1;
            }
            if (choice == 1) {
                name = gArrivalSeaplaneName;
                tried = 2;
                slot = &mSeaplaneThing;
            } else if (choice == 0) {
                name = gArrivalBusName;
                tried = 1;
                slot = &mBusThing;
            } else if (choice == 2) {
                name = gArrivalFerryName;
                tried = 4;
                slot = &mFerryThing;
            } else {
                break;
            }
        } else if ((tried & 4) == 0) {
            name = gArrivalFerryName;
            tried += 4;
            slot = &mFerryThing;
        } else if ((tried & 2) == 0) {
            name = gArrivalSeaplaneName;
            tried += 2;
            slot = &mSeaplaneThing;
        } else {
            if ((tried & 1) != 0) {
                GAME_ASSERTMSG(0, kNoVehicleText);
                break;
            }
            name = gArrivalBusName;
            tried += 1;
            slot = &mBusThing;
        }
        if (*slot != kNoThing) {
            const CObject* existing = ObjectOfRow(*slot);
            RSSE_Setvar(ScriptOf(existing), 0, 1);
            mArrivalVehicleThing = *slot;
            break;
        }
        RideInfo* ride = gRideDatabase.GetFirstRide();
        while (ride != nullptr && NS_MacDoze::_wcsicmp(ride->mItemName, name.mData) != 0) ride = gRideDatabase.GetNextRide();
        if (ride == nullptr) {
            CTRACE(2, kNoFeatureText_2);
            continue;
        }
        // The Mac tests the allocation for NULL and still asks for the id (the host's allocation never fails: not reproduced, as in unit w117).
        void* block = tpw::ArenaAlloc(0x448);
        CObject* object = new (block) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1},
                                              tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
        *slot = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
        RSSE_Setvar(ScriptOf(object), 1, 1);
        mArrivalVehicleThing = *slot;
        break;
    }
    return {mArrivalVehicleThing};
}

// 0x101406cc (264 bytes). -1 without a vehicle; else variable 1 of its script (see VehicleStatus for the 6).
int32_t CWorld::ArrivalVehicle_GetStatus() { return VehicleStatus(*this); }

// 0x1014080c (804 bytes). 1 when there is no vehicle or it is seagoing. Else the status (VehicleStatus, inline on the Mac) is read up to three
// times, each time afresh: 3 answers 0; then more than 9 people left to drop off answers 1; then status 1 answers 0; then status 2 answers 0
// and anything else 1.
int32_t CWorld::ArrivalVehicle_CrossingIsClear() {
    if (mArrivalVehicleThing == kNoThing || ArrivalVehicle_IsSeagoing() != 0) return 1;
    if (VehicleStatus(*this) == 3) return 0;
    if (ArrivalVehicle_PeopleLeftToDropOff() > 9) return 1;
    if (VehicleStatus(*this) == 1) return 0;
    return VehicleStatus(*this) != 2 ? 1 : 0;
}

// 0x10140b6c (488 bytes). 1 when a visitor in state 0x15 stands on one of the four map cells beside the bus stop A: the stop, the cell to its
// right, and the same two cells two rows up. Each cell id is the sum cut to 16 bits (the Mac adds (dy << 7) + stop + dx and stores a halfword,
// so a stop near the edge wraps). Only the FIRST thing of each cell is looked at: the Mac reads the thing's cell link (mCellNext) and drops it.
int32_t CWorld::ArrivalVehicle_PeopleWaitingToLeave() {
    const CMap* map = GetMap();
    const int32_t dx[4] = {0, 1, 0, 1};
    const int32_t dy[4] = {0, 0, -2, -2};
    uint16_t cells[4];
    for (int32_t i = 0; i < 4; ++i) {
        const uint16_t stop = GetMap()->GetBusStopPos(0).mIndex;
        cells[i] = static_cast<uint16_t>(dx[i] + (dy[i] << 7) + stop);
    }
    for (int32_t i = 0; i < 4; ++i) {
        const uint16_t id = CellOf(map, cells[i])->mWho;
        if (id != kNoThing) {
            const CThing* thing = static_cast<const CThing*>(RowThing_2(id));
            if (thing->mKind == kKindPerson && ReadWord_2(thing, kPeepState) == kPeepStateGettingOff) return 1;
        }
    }
    return 0;
}

// 0x10140d98 (8 bytes). The people still on the bus, the word at +0x2d0 (CArrival's mPeopleOnBus).
int32_t CWorld::ArrivalVehicle_PeopleLeftToDropOff() { return mArrival.mPeopleOnBus; }

// 0x10140de0 (76 bytes). 1 when a vehicle is in use and it is not the bus (the seaplane and the ferry sail).
int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    if (mArrivalVehicleThing != kNoThing && mBusThing != mArrivalVehicleThing) return 1;
    return 0;
}

// 0x10140e64 (80 bytes). Counts the people made: the trace shows the count and the answer is the count read back.
int32_t CWorld::GetNewPersonIndex() {
    ++mPersonCount;
    CTRACE(2, kVisitorText, mPersonCount);
    return static_cast<int32_t>(mPersonCount);
}

// 0x10140ee4 (104 bytes). Sets the bit of the ride's downloaded ride number (1 based) in the park's ride bitmap; a ride that has no number
// (0) sets nothing. The ride record is not tested for NULL.
void CWorld::FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    const int32_t number = gRideDatabase.GetRideInfo(id)->Value(kRideSlot_DownloadedRideNumber).mInteger;
    if (number != 0) gParkRequiredRides[static_cast<uint32_t>(number - 1) >> 3] |= static_cast<uint8_t>(1 << ((number - 1) & 7));
}

// World.cpp (main app 0x10140fac, unit w114): whether the ride database has the rides a park needs, and the two ride masks.

uint8_t gRidesOwned[0x20];          // 0x1034cb98
uint8_t gParkRequiredRides[0x20];   // 0x1034cbb8

// Rebuilds gRidesOwned from DownloadedRideNumber (minus one: byte n >> 3, bit n & 7).
// IsPreviewRide excludes a ride when `previews` is nonzero; a missing required ride gives 0.
// The original runs the mask 1, 2, 4 .. 0x80 over each byte but tests `(mask & byte) == 1`, which only the mask 1 can satisfy: only bit 0
// of each of the 32 bytes is checked (kept; the other seven bits of each byte never matter). The walk writes byte n >> 3 of the mask
// with no bound: a ride bit above 256 writes past the 32 bytes (real ones are small). The mask keeps the last result.
int32_t CWorld::DoIHaveTheseRides(const char* required, int32_t previews) {
    std::memset(gRidesOwned, 0, 0x20);
    for (RideInfo* ride = gRideDatabase.GetFirstRide(); ride != nullptr; ride = gRideDatabase.GetNextRide()) {
        bool counts = true;
        if (ride->Value(kRideSlot_IsPreviewRide).mInteger != 0 && previews != 0) counts = false;
        if (static_cast<uint32_t>(ride->Value(kRideSlot_DownloadedRideNumber).mInteger) != 0 && counts) {
            const uint32_t bit = static_cast<uint32_t>(ride->Value(kRideSlot_DownloadedRideNumber).mInteger) - 1;
            gRidesOwned[bit >> 3] = static_cast<uint8_t>(gRidesOwned[bit >> 3] | (1 << (bit & 7)));
        }
    }
    for (uint32_t i = 0; i < 0x20; ++i) {
        for (int32_t mask = 1; mask < 0x100; mask <<= 1) {
            const int32_t needed = static_cast<int8_t>(required[i]);
            const int32_t owned = static_cast<int8_t>(gRidesOwned[i]);
            if ((mask & needed) == 1 && (mask & owned) == 0) return 0;
        }
    }
    return 1;
}

// 0x1014113c (304 bytes). Writes, for every bit set in the park's ride bitmap, the number found in the last folder name of the ride that
// has that downloaded ride number ("x\ride12.wad" gives 12), in bitmap order; the rest of the 0x200 byte array is zeroed first. A bit
// whose ride is not in the database asserts and adds nothing. The answer is always 0. The folder must hold a backslash and, after the last
// one, a digit (strrchr and strpbrk are not tested for NULL, as on the Mac).
int32_t CWorld::ConvertDLRidesUsedToIndices(uint16_t* out) {
    std::memset(out, 0, 0x200);
    int32_t k = 0;
    for (int32_t byte = 0; byte < 0x20; ++byte) {
        for (int32_t bit = 0; bit < 8; ++bit) {
            if (((1 << bit) & static_cast<int32_t>(static_cast<signed char>(gParkRequiredRides[byte]))) == 0) continue;
            RideInfo* ride = gRideDatabase.GetFirstRide();
            while (ride != nullptr && ride->Value(kRideSlot_DownloadedRideNumber).mInteger != bit + byte * 8 + 1)
                ride = gRideDatabase.GetNextRide();
            GAME_ASSERTMSG(ride != nullptr, kNotInDatabaseText);
            if (ride != nullptr) {
                const char* name = std::strrchr(ride->mFolder, '\\');
                name = std::strpbrk(name, kDigits);
                char* end;
                out[k++] = static_cast<uint16_t>(std::strtol(name, &end, 10));
            }
        }
    }
    return 0;
}

// 0x101414a4 (136 bytes). The pending list's RemoveHead: the head node leaves the list, the list counts down, the node is freed and the
// thing id it held is answered. An empty list answers a zero id (the Mac answers without writing its result: the caller sees whatever its
// result slot held).
template <>
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveHead() {
    if (mHead == nullptr) return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{};
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> value = node->mData;
    --mCount;
    delete node;
    return value;
}

// 0x10141ad4 (140 bytes). The pending list's AddHead: a new node (10 bytes on the Mac) becomes the head and the list counts up.
template <>
void TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::AddHead(
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> value) {
    CNode* node = new CNode(value);
    node->mNext = mHead;
    if (mHead == nullptr) mTail = node;
    else mHead->mPrev = node;
    mHead = node;
    ++mCount;
}

// __sinit_World_cpp (0x10141c14, 348 bytes): World.cpp's static initialiser. Declarations are local to this file.
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// New globals (Mac addresses in comments; names invented).
uint32_t gWorldWord1037ec00;   // 0x1037ec00, set to 0x10000
uint32_t gWorldWord1037ec04;   // 0x1037ec04, set to 0
char gSinitCtorVecThingEntry[8];   // 0x10266db8, TVector of CWorld::_CThingArrayEntry::_CThingArrayEntry
char gSinitDtorVecPendingList[8];  // 0x10266db0, TVector of TbTList<...>::~TbTList
char gSinitNodeList[16];       // 0x1034cbe8, registration node of gPendingDeleteList
char gSinitNodeBus[16];        // 0x1034cb8c
char gSinitNodeSeaplane[16];   // 0x1034cb6c
char gSinitNodeFerry[16];      // 0x1034cb4c
char gSinitNodeGates[16];      // 0x1034cb2c
char gSinitNodeLights[16];     // 0x1034cb0c
char gSinitNodeEnd[16];        // 0x1034caec

typedef TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> PendingList;

// 0x10141d94 (28 bytes, translated elsewhere): declared so that the generic constructor is not instantiated here.
template<> PendingList::TbTList();

void __sinit_World_cpp() {
    void* wdtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    gWorldWord1037ec04 = 0;
    gWorldWord1037ec00 = 0x10000;
    __construct_array(gThingArray, gSinitCtorVecThingEntry, nullptr, 0x14, 0x2800);
    PendingList* list = new (&gPendingDeleteList) PendingList();
    __register_global_object(list, gSinitDtorVecPendingList, gSinitNodeList);
    TbDynamicStringTemplate<char16_t>* s;
    s = new (&gArrivalBusName) TbDynamicStringTemplate<char16_t>(u"bus");
    __register_global_object(s, wdtor, gSinitNodeBus);
    s = new (&gArrivalSeaplaneName) TbDynamicStringTemplate<char16_t>(u"seaplane");
    __register_global_object(s, wdtor, gSinitNodeSeaplane);
    s = new (&gArrivalFerryName) TbDynamicStringTemplate<char16_t>(u"Ferry");
    __register_global_object(s, wdtor, gSinitNodeFerry);
    s = new (&gWorldGatesName) TbDynamicStringTemplate<char16_t>(u"gates");
    __register_global_object(s, wdtor, gSinitNodeGates);
    s = new (&gWorldLightsName) TbDynamicStringTemplate<char16_t>(u"lights");
    __register_global_object(s, wdtor, gSinitNodeLights);
    s = new (&gWorldEndName) TbDynamicStringTemplate<char16_t>(u"End");
    __register_global_object(s, wdtor, gSinitNodeEnd);
    gWinningAnimThing = 0;
}
