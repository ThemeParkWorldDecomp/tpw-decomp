// Test library for the theme and the new level (libtpw_unit_NewLevel.dylib; see tests/test_NewLevel.py): src/game/Game.cpp (CGame::InitTheme,
// InitLevel, InitLevel_Load, InitLevel_Initialise, LoadTheme), src/game/World.cpp (CWorld::SetupNewLevel), src/game/save/FS_LoadSave.cpp
// (CFSLoadSave::Load and LoadInternal), src/game/park/LLCellSystem.cpp (the four cost setters), src/game/park/SkyObjects.cpp (the empty CSkyObjects
// constructor) and src/game/balance/HoardingsBalance.cpp, all real, with the real main balance and hoardings tables.
//
// Everything else is a fake on both sides that records through calllog: the balance reader (Init, UnInit, Load), the ride database
// (LoadTheme, GetFirstRide, GetNextRide), the ten thing constructors, CObject's, CThing::GetThingId, the world's leaves (control manager,
// action recorder, map, macro AI, research lab, bitmap system, pilots), the sky and the display, the message centre and its headers, the user
// info and the string conversions, CSaveLoad::Load, the trace and assertion output, `time` and _wcsicmp. Operator new logs its size ("new"; a
// block of the host's CFSUserInfo or CSkyObjects size is logged with the Mac's size, 0xd4 or 0x10). The bullfrog strings are functional.
//
// The Python module sets the world up with the tpw_nl_ functions below (recording off), then runs one function with tpw_nl_call (recording on)
// and reads the state back through tpw_nl_addr.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>

namespace {
void observe_allocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#include "platform/arena.h"

#include "ActionRecorder.h"
#include "Advisor.h"
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "HoardingsBalance.h"
#include "LLCellSystem.h"
#include "LevelInit.h"
#include "Main.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "SkyObjects.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "tb_resource_display.h"
#include "tb_sky.h"

using namespace calllog;

// The unit makes its things on the game heap (the Mac's operator new), which the host takes from the arena (unit v5L31). Here the arena is
// the call log's operator new and delete, so a thing's block is made, filled, labelled and freed as the Mac's is.
namespace tpw {
void* ArenaAlloc(size_t size) { return ::operator new(size); }
bool InArena(const void* address) { return address != nullptr; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
#include "fake_strings.h"

// ------------------------------------------------------------------ globals of other files

TPGlobals::TPGlobals() {}   // a blank object: the test sets the strings the functions read
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CommandLineOptions gCommandLineOptions;
uint8_t gParkRequiredRides[0x20];   // World_rides.cpp is not in this library (SetupNewLevel clears it)
CFSUserInfo* gUserInfo;
CFSLoadSave gLoadSave;
CSaveLoad gSaveLoad;
CMessageCentre* gMessageCentre;
CWorld* gGameWorld;
CRideDatabase gRideDatabase;
CActionRecorder gActionRecorder;
SAdvisorState gAdvisorState;
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSky::CSky() {}
CSky::~CSky() {}
CSky gei_Sky;
int32_t gHeightfieldRangeExtraX;
int32_t gHeightfieldRangeExtraY;
s3DPoint gGlobalLightDirection;
uint32_t gGlobalLightColour;

namespace {

constexpr int kLabelWorld = 1;
constexpr int kLabelOtherWorld = 2;
constexpr int kLabelMainBalance = 3;
constexpr int kLabelHoardingsA = 4;
constexpr int kLabelHoardingsB = 5;
constexpr int kLabelOldCentre = 6;
constexpr int kLabelOldSkyObjects = 7;
constexpr int kLabelOldUserInfo = 8;
constexpr int kLabelGame = 9;
constexpr int kLabelResearchLab = 10;
constexpr int kLabelRecorder = 11;
constexpr int kLabelRides = 12;
constexpr int kLabelLoadSave = 13;
constexpr int kLabelDisplay = 14;
constexpr int kLabelSky = 15;
constexpr int kLabelSaveLoad = 16;

constexpr int kMaxRides = 8;

unsigned char* gWorldA;                                   // the world under test (a block of the Mac's size)
alignas(16) unsigned char gWorldB[64];                    // another world's address (only its arithmetic is used)
alignas(CGame) unsigned char gGameBlock[sizeof(CGame)];   // the game object; only mSkyObjects is read or written
alignas(16) unsigned char gResearchLab[16];               // what GetResearchLab answers
alignas(16) unsigned char gOldCentre[sizeof(CMessageCentre)];   // a message centre that exists already
alignas(16) unsigned char gOldSkyObjects[sizeof(CSkyObjects)];
alignas(16) unsigned char gSkyBlock[0x20];                // the block the CSkyObjects constructor test uses
alignas(16) unsigned char gOldUserInfo[16];               // a user info that exists already (never dereferenced)
alignas(RideInfo) unsigned char gRideStorage[kMaxRides][sizeof(RideInfo)];
RideInfo* gRideRecords[kMaxRides];
int32_t gRideCount;
int32_t gRideCursor;
char gArgText[2048];                                      // the path argument
char gThemePath[2048];                                    // what get_theme_path answers
char gFindAnswer[2048];                                   // what FindFilePath answers
bool gFindNull;
TbDynamicStringTemplate<char16_t> gUsersPath;             // what ReturnUsersPathWithTheme answers
int32_t gCtorType;                                        // the type the game type's constructor makes
int32_t gHeadersMade;

CGame* Game() { return reinterpret_cast<CGame*>(gGameBlock); }

// Where `p` is, when it is `offset` bytes into one of the two worlds: 1, 2 or -1.
int64_t WhichWorld(const void* p, int64_t offset) {
    if (p == gWorldA + offset) return kLabelWorld;
    if (p == gWorldB + offset) return kLabelOtherWorld;
    return -1;
}

// A wide text as recorded (ASCII kept, anything else '?'), into a buffer that is not from operator new.
void Narrow(const char16_t* text, char* out, std::size_t size) {
    std::size_t n = 0;
    if (text == nullptr) {
        std::snprintf(out, size, "(null)");
        return;
    }
    for (; text[n] != 0 && n + 1 < size && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = 0;
}

// A word of the world under test as it is now (the value the original's big-endian store reads back as), and a digest of the sixteen thing
// ids at 0x1da718 to 0x1da736: the events that carry them show when the world's stores happen.
int64_t WorldWord(int64_t offset) {
    uint32_t word;
    std::memcpy(&word, gWorldA + offset, 4);
    return word;
}
int64_t IdDigest() {
    uint32_t hash = 0;
    for (int i = 0; i < 16; ++i) {
        uint16_t id;
        std::memcpy(&id, gWorldA + 0x1da718 + 2 * i, 2);
        hash = hash * 31 + id;
    }
    return hash;
}

// The id a thing constructor leaves in the object's first half word: the next of the queue "id".
void SetThingId(void* thing) {
    const uint16_t id = static_cast<uint16_t>(ret("id", 0x1234));
    std::memcpy(thing, &id, 2);
}

}  // namespace

// ------------------------------------------------------------------ the allocation observer

namespace {
void observe_allocation(std::size_t size, bool, void* block) {
    if (!on()) return;
    std::memset(block, 0xa5, size);
    int64_t logged = static_cast<int64_t>(size);
    if (size == sizeof(CFSUserInfo)) logged = 0xd4;
    else if (size == sizeof(CSkyObjects)) logged = 0x10;
    ev("new", {logged});
}
}  // namespace

// ------------------------------------------------------------------ debug output

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}
void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {flags}, {message}); }

// ------------------------------------------------------------------ the balance reader

void CBalanceTable::Init() { ev("Balance.Init", {lab(this)}); }
void CBalanceTable::UnInit() { ev("Balance.UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("Balance.Load", {lab(this), mode}, {path});
    return static_cast<int32_t>(ret("Balance.Load", 1));
}

// ------------------------------------------------------------------ the level, the theme and the engine

char* get_theme_path() {
    ev("get_theme_path");
    return gThemePath;
}
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, lab(result)}, {name});
    return gFindNull ? nullptr : gFindAnswer;
}
void OBJECT_Load(char* path) { ev("OBJECT_Load", {}, {path}); }
int32_t CSky::SkyLoad(char* folder) {
    ev("SkyLoad", {lab(this)}, {folder});
    return 0;
}
void CDisplay::SetFogColor(uint32_t argb) { ev("SetFogColor", {lab(this), argb}); }
void CSkyObjects::InitSkyObjects() { ev("InitSkyObjects", {newlab(this)}); }
// FS_System.cpp's directory separator (tested there): a separator unless the text ends in one (an empty text gets one).
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    const int32_t length = path.Length();
    if (length == 0 || path.mData[length - 1] != ':') path += gTPGlobals.mSeparator;
}

// ------------------------------------------------------------------ the ride database

int32_t CRideDatabase::LoadTheme(const char* path) {
    ev("RideDB.LoadTheme", {lab(this)}, {path});
    return static_cast<int32_t>(ret("RideDB.LoadTheme", 1));
}
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    gRideCursor = 0;
    return gRideCount > 0 ? gRideRecords[0] : nullptr;
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    ++gRideCursor;
    return gRideCursor < gRideCount ? gRideRecords[gRideCursor] : nullptr;
}
// The queue "wcsicmp" answers each call; with the queue empty the answer is 0 for texts that are equal ignoring case (ASCII), else 1.
int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    char first[160];
    char second[160];
    Narrow(a, first, sizeof first);
    Narrow(b, second, sizeof second);
    ev("wcsicmp", {}, {first, second});
    const int64_t queued = ret("wcsicmp", INT64_MAX);
    if (queued != INT64_MAX) return static_cast<int32_t>(queued);
    for (std::size_t i = 0;; ++i) {
        char16_t x = a[i];
        char16_t y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char16_t>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char16_t>(y + 32);
        if (x != y) return 1;
        if (x == 0) return 0;
    }
}

// ------------------------------------------------------------------ the things SetupNewLevel makes

#define THING_CTOR(cls)                                                  \
    cls::cls() {                                                         \
        ev(#cls, {newlab(this), WorldWord(0x1da714), IdDigest()});       \
        SetThingId(this);                                                \
    }
THING_CTOR(CStaffHQ)
THING_CTOR(CMechanicHQ)
THING_CTOR(CAdvisor)
THING_CTOR(CTagSystem)
THING_CTOR(CParkAnalyser)
THING_CTOR(CResearchLab)
THING_CTOR(CWeather)
THING_CTOR(CBankAccount)
THING_CTOR(CUIMsgReceiver)
THING_CTOR(CChallengeHQ)
CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a,
                 int32_t b) {
    ev("CObject", {newlab(this), map.mIndex, type.mIndex, a, b, IdDigest()});
    SetThingId(this);
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    return {mThingId};
}
void CResearchLab::Init() { ev("CResearchLab::Init", {lab(this)}); }
CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {WhichWorld(this, 0), IdDigest()});
    return reinterpret_cast<CResearchLab*>(gResearchLab);
}

// ------------------------------------------------------------------ the world's leaves

void CControlManager::SetupInitialControls() { ev("SetupInitialControls", {WhichWorld(this, 0x1d943c), 0x1d943c, WorldWord(0x1da738)}); }
void CActionRecorder::ResetActionRecording() { ev("ResetActionRecording", {lab(this)}); }
void CMap::Init() { ev("CMap::Init", {WhichWorld(this, 0x2d8), 0x2d8, WorldWord(0x1da70c), WorldWord(0x1da708)}); }
void CMacroAI::Init() { ev("CMacroAI::Init", {WhichWorld(this, 0), 0, WorldWord(0x1da708), WorldWord(0x1da714)}); }
void BMP_InitSystem() { ev("BMP_InitSystem"); }
void Pilot::init_behaviours() { ev("Pilot::init_behaviours"); }
CGameType::CGameType() : mType(static_cast<GameType>(gCtorType)) {}   // records nothing: the host's object is made once

// The first message: the event's object is labelled by its constructor and named again by PostMessage.
CMsgEvent::CMsgEvent(EventType type) : mEvent(type) { ev("CMsgEvent", {newlab(this), type}); }
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0x13; }
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("CMessageCentre::PostMessage", {lab(this), lab(&message), WorldWord(0x1da738)});
    return 1;
}
// The 29 subscription headers of a new CMessageCentre: the first of each 29 constructed is the centre itself (its address labels the centre)
// and stands for __construct_array's call, whose constructor and destructor tables and sizes the PowerPC side checks.
CSubscriptionHeader::CSubscriptionHeader() {
    if (gHeadersMade++ % 0x1d == 0) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ------------------------------------------------------------------ the park file

Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() { ev("CFSUserInfo", {newlab(this)}); }
TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    ev("ReturnUsersPathWithTheme", {lab(this)});
    return gUsersPath;
}
// The conversions keep Latin-1 (characters above 0xff become '?'); the texts are recorded.
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    static char text[4096];
    int32_t n = 0;
    for (; in.mData[n] != 0 && n < 4095; ++n) text[n] = in.mData[n] > 0xff ? '?' : static_cast<char>(in.mData[n]);
    text[n] = 0;
    ev("UniStringToString", {}, {text});
    out = text;
    return n + 1;
}
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    static char16_t text[4096];
    int32_t n = 0;
    for (; in.mData[n] != 0 && n < 4095; ++n) text[n] = static_cast<uint8_t>(in.mData[n]);
    text[n] = 0;
    ev("StringToUniString", {}, {in.mData});
    out = text;
    return n + 1;
}
int32_t CSaveLoad::Load(const char* path, CSaveLoadHeader* header, LoadType type) {
    ev("CSaveLoad::Load", {lab(this), lab(header), type}, {path});
    return static_cast<int32_t>(ret("CSaveLoad::Load", 1));
}

// ------------------------------------------------------------------ the C library (this library only)

extern "C" time_t time(time_t* out) {
    ev("time", {WorldWord(0x1da70c)});
    const time_t value = static_cast<time_t>(ret("time"));
    if (out != nullptr) *out = value;
    return value;
}

// ------------------------------------------------------------------ test entry points

extern "C" {

// The address of a named global (the module reads and writes it directly).
void* tpw_nl_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    const Entry entries[] = {
        {"extraX", &gHeightfieldRangeExtraX}, {"extraY", &gHeightfieldRangeExtraY}, {"ambient", &gGlobalAmbientColour},
        {"lightColour", &gGlobalLightColour}, {"boundX", &gLevelRangeMinX}, {"boundY", &gLevelRangeMinY},
        {"lightDir", &gGlobalLightDirection}, {"waterStart", &gWaterStartPos}, {"advOn", gAdvisorState.mAccessoriesOn},
        {"advOff", gAdvisorState.mAccessoriesOff}, {"cost0", &gLLCC47DC}, {"cost1", &gLLCC47D8}, {"cost2", &gLLCC47D4},
        {"cost3", &gLLCC47D0}, {"bitmap", gParkRequiredRides}, {"worldA", gWorldA}, {"skyBlock", gSkyBlock},
        {"messageCentre", &gMessageCentre}, {"userInfo", &gUserInfo}, {"skyObjects", &Game()->mSkyObjects},
        {"gameWorld", &gGameWorld}, {"displayFlags", &gei_Display.mFlags}, {"loadFile", &gCommandLineOptions.mLoadFile},
        {"file", gCommandLineOptions.mFile},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// Sets one of the narrow strings the functions read.
void tpw_nl_text(const char* which, const char* text) {
    if (std::strcmp(which, "arg") == 0) std::snprintf(gArgText, sizeof gArgText, "%s", text);
    else if (std::strcmp(which, "themePath") == 0) std::snprintf(gThemePath, sizeof gThemePath, "%s", text);
    else if (std::strcmp(which, "find") == 0) std::snprintf(gFindAnswer, sizeof gFindAnswer, "%s", text);
    else if (std::strcmp(which, "hoardings") == 0) gTPGlobals.mF40EC = text;
    else if (std::strcmp(which, "endhoardings") == 0) gTPGlobals.mF4100 = text;
    else if (std::strcmp(which, "easy") == 0) gTPGlobals.mF427C = text;
    else if (std::strcmp(which, "separator") == 0) gTPGlobals.mSeparator = text;
    else if (std::strcmp(which, "file") == 0) std::snprintf(gCommandLineOptions.mFile, sizeof gCommandLineOptions.mFile, "%s", text);
}
// Sets one of the wide strings.
void tpw_nl_wtext(const char* which, const uint16_t* chars, int32_t n) {
    static char16_t text[4096];
    for (int32_t i = 0; i < n && i < 4095; ++i) text[i] = chars[i];
    text[n < 4095 ? n : 4095] = 0;
    if (std::strcmp(which, "ext") == 0) gTPGlobals.mF4420 = text;
    else if (std::strcmp(which, "wsep") == 0) gTPGlobals.mWideSeparator = text;
    else if (std::strcmp(which, "gates") == 0) gWorldGatesName = text;
    else if (std::strcmp(which, "lights") == 0) gWorldLightsName = text;
    else if (std::strcmp(which, "users") == 0) gUsersPath = text;
}
// Whether FindFilePath answers NULL.
void tpw_nl_find_null(int32_t isNull) { gFindNull = isNull != 0; }
// The type the game type's constructor makes (the host's static is made once, so the module also sets the object's word).
void tpw_nl_gametype(int32_t type, int32_t ctorType) {
    gCtorType = ctorType;
    TheGameType().mType = static_cast<GameType>(type);
}
// One value slot of the main balance, as a word.
void tpw_nl_balance(int32_t slot, uint32_t word) {
    gMainBalance.mValues[slot].mBits = 0;
    gMainBalance.mValues[slot].mInteger = static_cast<int32_t>(word);
}
// Ride `index` of the database walk: its type number, its item name.
void tpw_nl_ride(int32_t index, uint16_t id, const uint16_t* name, int32_t n) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRideStorage[index]);
    std::memset(static_cast<void*>(ride), 0xa5, sizeof(RideInfo));
    ride->mId = id;
    for (int32_t i = 0; i < n && i < 0x3f; ++i) ride->mItemName[i] = name[i];
    ride->mItemName[n < 0x3f ? n : 0x3f] = 0;
    gRideRecords[index] = ride;
}
void tpw_nl_ride_count(int32_t n) { gRideCount = n; }

// A clean case: the labels, the fakes' state, the globals the functions write, the strings at their defaults. `fill` is the byte the world
// is made of (and the game object, and the sky block); `which` is the world gGameWorld holds (0 none, 1 the world under test, 2 the other).
void tpw_nl_reset(int32_t fill, int32_t which) {
    std::free(gWorldA);
    gWorldA = static_cast<unsigned char*>(std::malloc(sizeof(CWorld)));
    std::memset(gWorldA, fill, sizeof(CWorld));
    std::memset(gWorldB, fill, sizeof gWorldB);
    std::memset(gGameBlock, 0xa5, sizeof gGameBlock);
    std::memset(gSkyBlock, 0xa5, sizeof gSkyBlock);
    std::memset(gResearchLab, 0, sizeof gResearchLab);
    std::memset(gParkRequiredRides, 0xa5, sizeof gParkRequiredRides);
    gGameWorld = which == 1 ? reinterpret_cast<CWorld*>(gWorldA) : which == 2 ? reinterpret_cast<CWorld*>(gWorldB) : nullptr;
    gMessageCentre = nullptr;
    gUserInfo = nullptr;
    Game()->mSkyObjects = nullptr;
    gRideCount = 0;
    gRideCursor = 0;
    gFindNull = false;
    gHeadersMade = 0;
    gCtorType = 0;
    gei_Display.mFlags = 0;
    gCommandLineOptions.mLoadFile = 0;
    gCommandLineOptions.mFile[0] = 0;
    gThemePath[0] = 0;
    gFindAnswer[0] = 0;
    gArgText[0] = 0;
    gTPGlobals.mF40EC = "hoardings.sam";
    gTPGlobals.mF4100 = "endhoardings.sam";
    gTPGlobals.mF427C = "Easy_";
    gTPGlobals.mSeparator = ":";
    gTPGlobals.mF4420 = u".TPWS";
    gTPGlobals.mWideSeparator = u":";
    gWorldGatesName = u"gates";
    gWorldLightsName = u"lights";
    gUsersPath = u"";
    tpw_cl_label(kLabelWorld, gWorldA);
    tpw_cl_label(kLabelOtherWorld, gWorldB);
    tpw_cl_label(kLabelMainBalance, &gMainBalance);
    tpw_cl_label(kLabelHoardingsA, &gHoardingsBalance434C);
    tpw_cl_label(kLabelHoardingsB, &gHoardingsBalance4130);
    tpw_cl_label(kLabelOldCentre, gOldCentre);
    tpw_cl_label(kLabelOldSkyObjects, gOldSkyObjects);
    tpw_cl_label(kLabelOldUserInfo, gOldUserInfo);
    tpw_cl_label(kLabelGame, Game());
    tpw_cl_label(kLabelResearchLab, gResearchLab);
    tpw_cl_label(kLabelRecorder, &gActionRecorder);
    tpw_cl_label(kLabelRides, &gRideDatabase);
    tpw_cl_label(kLabelLoadSave, &gLoadSave);
    tpw_cl_label(kLabelDisplay, &gei_Display);
    tpw_cl_label(kLabelSky, &gei_Sky);
    tpw_cl_label(kLabelSaveLoad, &gSaveLoad);
}
// Which objects exist before the call: a message centre, sky objects, a user info (each at its fixed label).
void tpw_nl_existing(int32_t centre, int32_t skyObjects, int32_t userInfo) {
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gOldCentre) : nullptr;
    Game()->mSkyObjects = skyObjects != 0 ? reinterpret_cast<CSkyObjects*>(gOldSkyObjects) : nullptr;
    gUserInfo = userInfo != 0 ? reinterpret_cast<CFSUserInfo*>(gOldUserInfo) : nullptr;
}

// How many bytes of the world under test differ from `fill` in the first `length` bytes.
int64_t tpw_nl_world_changed(int32_t fill, int64_t length) {
    int64_t n = 0;
    for (int64_t i = 0; i < length; ++i)
        if (gWorldA[i] != static_cast<unsigned char>(fill)) ++n;
    return n;
}

// The calls: 0 InitTheme(arg text), 1 InitLevel, 2 InitLevel_Load, 3 InitLevel_Initialise, 4 LoadTheme(arg text), 5 SetupNewLevel(arg
// text) on the world under test, 6 Load(name), 7 LoadInternal(name, extension), 8 to 11 the path, queue, kart track and water track
// setters (value), 12 the CSkyObjects constructor over the sky block.
int64_t tpw_nl_call(int32_t op, int64_t value, const uint16_t* a, int32_t an, const uint16_t* b, int32_t bn) {
    static char16_t first[4096];
    static char16_t second[4096];
    for (int32_t i = 0; i < an && i < 4095; ++i) first[i] = a[i];
    first[an < 4095 ? an : 4095] = 0;
    for (int32_t i = 0; i < bn && i < 4095; ++i) second[i] = b[i];
    second[bn < 4095 ? bn : 4095] = 0;
    CGame* game = Game();
    switch (op) {
    case 0: return game->InitTheme(gArgText);
    case 1: return game->InitLevel();
    case 2: return game->InitLevel_Load();
    case 3: return game->InitLevel_Initialise();
    case 4: return game->LoadTheme(gArgText);
    case 5: return reinterpret_cast<CWorld*>(gWorldA)->SetupNewLevel(gArgText) ? 1 : 0;
    case 6: {
        const TbDynamicStringTemplate<char16_t> name(first);
        return gLoadSave.Load(name);
    }
    case 7: {
        const TbDynamicStringTemplate<char16_t> name(first);
        const TbDynamicStringTemplate<char16_t> extension(second);
        return gLoadSave.LoadInternal(name, extension);
    }
    case 8: LLCC_SystemSetPathCost(static_cast<int32_t>(value)); return 0;
    case 9: LLCC_SystemSetQueueCost(static_cast<int32_t>(value)); return 0;
    case 10: LLCC_SystemSetKartTrackCost(static_cast<int32_t>(value)); return 0;
    case 11: LLCC_SystemSetWaterTrackCost(static_cast<int32_t>(value)); return 0;
    case 12: new (gSkyBlock) CSkyObjects; return 0;
    }
    return -999;
}

}  // extern "C"
