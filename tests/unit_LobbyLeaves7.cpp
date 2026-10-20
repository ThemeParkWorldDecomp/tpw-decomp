// Test library for the lobby's small leaves part 7 (unit w81; libtpw_unit_LobbyLeaves7.dylib, tests/test_LobbyLeaves7.py):
//   src/Game_theme_clear.cpp    CGame::ClearThemeResources 0x10149334
//   src/RideDB_clear.cpp        CRideDatabase::ClearDatabase 0x101546e8
//   src/NodeWrapping.cpp        StripNodeWrapping 0x101edafc
//   src/UI_activate.cpp         activateUI 0x101ca30c
//   src/UI_pause.cpp            UIInterface::Pause 0x101ef8a0
//   src/World_exit.cpp          CWorld::ExitLevel 0x1013a9fc, CWorld::Free 0x1013a264, CWorld::FreeThingHeap 0x1013ac08
//   src/StaffDB_dtor.cpp        CStaffDB::~CStaffDB 0x10128f68
//   src/SkyObjects_destroy.cpp  CSkyObjects::DestroySkyObjects 0x1015b804
//
// Real: the eight sources above (and TPGlobals.cpp, for the real default names). Fakes that record the same event on both sides: the
// game's resource manager (FindResource, UnregisterResource), CGame::GetCurrentThemeName, the ride database's DeleteCustomEffects, the
// ride record's DeallocStrings and destructor, the engine allocator (MReleaseAllocF, MReleaseFreeF), ERRORBox, UI_PostMessage, the window
// tree's GetChild and GetNext, PauseUI, the mouse's AssignMouseMode, CMap::DeleteMapInEngine, CWorld::DeleteAllThings,
// Vehicle::clear_behaviours, BMP_CloseSystem, the staff record's destructor and the sky's RemoveAdd. Operator delete is the call log's (it
// records the deletion of a labelled block). Each operation has one entry point: a description of numbers in, a dump of numbers out, the
// events in the call log. The objects of a case are built here (host) and in emulator memory at the Mac's layouts (test module).
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "fake_strings.h"

#include "Debug.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "RideDB.h"
#include "SkyObjects.h"
#include "StaffDB.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_sky.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
TPGlobals gTPGlobals;   // the game's default names (the real TPGlobals.cpp)
CMouse* gMouse;
CWorld* gGameWorld;
// The sky and the main balance are only addresses here (their classes are large): raw storage under the symbol names.
#ifdef __APPLE__
extern unsigned char gSkyStorage[sizeof(CSky)] __asm__("_gei_Sky");
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gSkyStorage[sizeof(CSky)] __asm__("gei_Sky");
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gSkyStorage[sizeof(CSky)] = {};
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

namespace {

bool Alive(const void* object) { return object != nullptr && lab(object) > 0; }

// A float's bits with every NaN made one value (the host's and the emulator's NaN payloads and signs differ).
uint32_t Canon(float value) {
    if (value != value) return 0x7fc00000u;
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

// ---------------------------------------------------------------------------------------------------- CGame::ClearThemeResources

enum ThemeLabel : int32_t { kGameLabel = 1, kThisLabel = 2, kManagerA = 3, kManagerB = 4, kResource = 50 };

CGame gThisGame;                         // the object the call is made on (never used by the function)
unsigned char gManagerBytes[2][16];      // two resource managers, only ever addresses
unsigned char gResourceBytes[16][16];    // the resources FindResource hands out

// What a resource manager fake does to gGame's manager pointer, from the case's queue: 1 puts the second manager there, 2 clears it, 3 the first.
void ManagerHook(const char* key) {
    const int64_t action = ret(key, 0);
    if (action == 1) gGame.mResourceManager = reinterpret_cast<CTPResourceManager*>(gManagerBytes[1]);
    if (action == 2) gGame.mResourceManager = nullptr;
    if (action == 3) gGame.mResourceManager = reinterpret_cast<CTPResourceManager*>(gManagerBytes[0]);
}

}  // namespace

const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    ManagerHook("SwapT");
    return mCurrentThemeName;
}
int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource) {
    ev("FindResource", {lab(this)}, {name.mData});
    const int32_t result = static_cast<int32_t>(ret("Find", 0));
    if (result == 1) *resource = reinterpret_cast<CTPBaseResource*>(gResourceBytes[ret("Res", 0) & 15]);
    ManagerHook("Swap");
    return result;
}
int32_t CTPResourceManager::UnregisterResource(CTPBaseResource* resource) {
    ev("UnregisterResource", {lab(this), lab(resource)});
    ManagerHook("SwapU");
    return 1;
}

namespace {

// description: the length of the current theme's name and its characters (at most 20), the first manager (0 none, 1 the first one)
// dump: gGame's manager as a label, whether gGame's theme name and the other object's are unchanged
int32_t RunTheme(const int64_t* d, int64_t* out) {
    char theme[24] = {};
    const int32_t length = static_cast<int32_t>(d[0]);
    for (int32_t i = 0; i < length; ++i) theme[i] = static_cast<char>(d[1 + i]);
    const int64_t manager = d[1 + length];
    tpw_cl_on = 0;
    gGame.mCurrentThemeName = theme;
    gThisGame.mCurrentThemeName = "the wrong object";
    gGame.mResourceManager = manager != 0 ? reinterpret_cast<CTPResourceManager*>(gManagerBytes[0]) : nullptr;
    tpw_cl_label(kGameLabel, &gGame);
    tpw_cl_label(kThisLabel, &gThisGame);
    tpw_cl_label(kManagerA, gManagerBytes[0]);
    tpw_cl_label(kManagerB, gManagerBytes[1]);
    for (int32_t k = 0; k < 16; ++k) tpw_cl_label(kResource + k, gResourceBytes[k]);

    tpw_cl_on = 1;
    gThisGame.ClearThemeResources();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = lab(gGame.mResourceManager);
    out[n++] = std::strcmp(gGame.mCurrentThemeName.mData, theme) == 0;
    out[n++] = std::strcmp(gThisGame.mCurrentThemeName.mData, "the wrong object") == 0;
    return n;
}

// ---------------------------------------------------------------------------------------------------- CRideDatabase::ClearDatabase

constexpr int kRides = 12;
enum DatabaseLabel : int32_t { kDatabase = 1, kRide = 20, kOtherRide = 80 };

alignas(16) unsigned char gDatabaseStorage[sizeof(CRideDatabase)];
CRideDatabase* gDatabase;
RideInfo* gOtherRide;

}  // namespace

// From the case's queue: the first fake changes the ride count behind the function's back (the Mac reads it again every time round).
void CRideDatabase::DeleteCustomEffects() {
    ev("DeleteCustomEffects", {lab(this)});
    const int64_t count = ret("CustomCount", -9999);
    if (count != -9999) mRideCount = static_cast<int32_t>(count);
}
// From the queue: 0 nothing, 1 the ride count is replaced by the next "DeallocCount", 2 this ride's slot is given the other ride (the Mac
// reads the slot again for the destructor).
void RideInfo::DeallocStrings() {
    ev("DeallocStrings", {lab(this)});
    const int64_t action = ret("Dealloc", 0);
    if (action == 1) gDatabase->mRideCount = static_cast<int32_t>(ret("DeallocCount", 0));
    if (action == 2) {
        for (int32_t i = 0; i < kRides; ++i) {
            if (gDatabase->mRides[i] == this) gDatabase->mRides[i] = gOtherRide;
        }
    }
}
// (the Mac passes the flag 1, which also frees the record: the host's delete does the same through the call log)
RideInfo::~RideInfo() { ev("RideInfo.dtor", {lab(this)}); }

namespace {

// description: the ride count, the cached id, the cached ride (0 none, 1 + k the k-th ride, 100 the other ride), the fill byte
// dump: the count, the cached id, the cached ride as a label, the first 16 slots as labels, whether the 12 rides and the other one are
// alive, whether the name builder and the words between are unchanged
int32_t RunDatabase(const int64_t* d, int64_t* out) {
    const int32_t count = static_cast<int32_t>(d[0]);
    const uint16_t cachedId = static_cast<uint16_t>(d[1]);
    const int64_t cached = d[2];
    tpw_cl_on = 0;
    gDatabase = reinterpret_cast<CRideDatabase*>(gDatabaseStorage);
    std::memset(gDatabaseStorage, static_cast<int>(d[3]), sizeof gDatabaseStorage);
    RideInfo* rides[kRides];
    for (int32_t i = 0; i < kRides; ++i) {
        rides[i] = new RideInfo;
        tpw_cl_label(kRide + i, rides[i]);
    }
    RideInfo* other = new RideInfo;
    gOtherRide = other;
    tpw_cl_label(kOtherRide, other);
    tpw_cl_label(kDatabase, gDatabase);
    for (int32_t i = 0; i < 0x14a; ++i) gDatabase->mRides[i] = i < kRides ? rides[i] : nullptr;
    gDatabase->mRideCount = count;
    gDatabase->mCachedId = cachedId;
    gDatabase->mCachedRide = cached == 0 ? nullptr : cached == 100 ? other : rides[cached - 1];
    unsigned char namesBefore[sizeof(CRideNameDB)];
    unsigned char wordsBefore[sizeof gDatabase->mF6AC + sizeof gDatabase->mF72C];
    std::memcpy(namesBefore, &gDatabase->mNames, sizeof namesBefore);
    std::memcpy(wordsBefore, gDatabase->mF6AC, sizeof gDatabase->mF6AC);
    std::memcpy(wordsBefore + sizeof gDatabase->mF6AC, &gDatabase->mF72C, sizeof gDatabase->mF72C);

    tpw_cl_on = 1;
    gDatabase->ClearDatabase();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = gDatabase->mRideCount;
    out[n++] = gDatabase->mCachedId;
    out[n++] = lab(gDatabase->mCachedRide);
    for (int32_t i = 0; i < 16; ++i) out[n++] = lab(gDatabase->mRides[i]);
    for (int32_t i = 0; i < kRides; ++i) out[n++] = Alive(rides[i]) ? 1 : 0;
    out[n++] = Alive(other) ? 1 : 0;
    out[n++] = std::memcmp(namesBefore, &gDatabase->mNames, sizeof namesBefore) == 0 &&
               std::memcmp(wordsBefore, gDatabase->mF6AC, sizeof gDatabase->mF6AC) == 0 &&
               std::memcmp(wordsBefore + sizeof gDatabase->mF6AC, &gDatabase->mF72C, sizeof gDatabase->mF72C) == 0;
    for (int32_t i = 0; i < kRides; ++i)
        if (Alive(rides[i])) ::operator delete(static_cast<void*>(rides[i]));
    if (Alive(other)) ::operator delete(static_cast<void*>(other));
    return n;
}

// ---------------------------------------------------------------------------------------------------- StripNodeWrapping

enum StripLabel : int32_t { kHeader = 1, kNode = 2, kData = 3, kPoint = 4 };

}  // namespace

// The engine allocator's fakes: a zeroed block from the arena (the Mac's flag 1 asks for zeroed memory), no block for a size of 0.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    if (size == 0) return nullptr;
    void* block = tpw::ArenaAllocZeroed(size);
    newlab(block);
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    va_end(args);
    ev("ERRORBox", {}, {format, name});
}

namespace {

// The description is a list of numbers (see the module): sizes, the amount, the pointers, the counter, the header's name, the texture
// coordinates (bits), the faces' vertices, the face sets, the vertex-to-point table, the points (bits) and the byte map.
// dump: every u and v afterwards (bits, NaNs as one value), the face sets' flag words, the counter, whether the faces, the tables, the
// points and the map are unchanged
int32_t RunStrip(const int64_t* d, int64_t* out) {
    const int64_t* p = d;
    const uint32_t vertices = static_cast<uint32_t>(*p++);
    const uint32_t faces = static_cast<uint32_t>(*p++);
    const uint32_t sets = static_cast<uint32_t>(*p++);
    const uint32_t points = static_cast<uint32_t>(*p++);
    const uint32_t amountBits = static_cast<uint32_t>(*p++);
    const int64_t dataSet = *p++;
    const int64_t pointSet = *p++;
    int32_t counter = static_cast<int32_t>(*p++);
    char name[0x14] = {};
    for (int i = 0; i < 0x13; ++i) name[i] = static_cast<char>(*p++);
    tpw_cl_on = 0;
    tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    tag_sNode* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
    sUVSet* uvs = static_cast<sUVSet*>(tpw::ArenaAllocZeroed(sizeof(sUVSet) * (vertices / 4 + 1)));
    sFace* faceArray = static_cast<sFace*>(tpw::ArenaAllocZeroed(sizeof(sFace) * (faces + 1)));
    sFaceSet* setArray = static_cast<sFaceSet*>(tpw::ArenaAllocZeroed(sizeof(sFaceSet) * (sets + 1)));
    sTexture* textures = static_cast<sTexture*>(tpw::ArenaAllocZeroed(sizeof(sTexture) * (sets + 1)));
    uint16_t* table = static_cast<uint16_t*>(tpw::ArenaAllocZeroed(sizeof(uint16_t) * (vertices + 1)));
    s3DPoint* pointArray = static_cast<s3DPoint*>(tpw::ArenaAllocZeroed(sizeof(s3DPoint) * (points + 1)));
    static unsigned char mapBytes[1024];
    std::memcpy(header->mName, name, sizeof name);
    node->mFaceSetCount = static_cast<uint16_t>(sets);
    node->mF5C = static_cast<uint16_t>(faces);
    node->mF5E = static_cast<uint16_t>(vertices);
    node->mUVs.set(uvs);
    node->mFaceSets.set(setArray);
    node->mFaces.set(faceArray);
    node->mF94.set(table);
    for (uint32_t i = 0; i < vertices; ++i) {
        uint32_t bits = static_cast<uint32_t>(*p++);
        std::memcpy(&uvs[i >> 2].mF00[i & 3], &bits, sizeof bits);
    }
    for (uint32_t i = 0; i < vertices; ++i) {
        uint32_t bits = static_cast<uint32_t>(*p++);
        std::memcpy(&uvs[i >> 2].mF00[4 + (i & 3)], &bits, sizeof bits);
    }
    for (uint32_t i = 0; i < faces; ++i)
        for (int k = 0; k < 3; ++k) faceArray[i].mF02[k] = static_cast<uint16_t>(*p++);
    for (uint32_t i = 0; i < sets; ++i) {
        setArray[i].mFirstFace = static_cast<uint16_t>(*p++);
        setArray[i].mFaceCount = static_cast<uint16_t>(*p++);
        const int64_t flags = *p++;
        // -1: no flag word; -2: the previous face set's word; else the word
        if (flags == -2 && i > 0) {
            setArray[i].mTexture = setArray[i - 1].mTexture;
        } else if (flags >= 0) {
            textures[i].mF00 = static_cast<uint16_t>(flags);
            setArray[i].mTexture.set(&textures[i]);
        }
    }
    for (uint32_t i = 0; i < vertices; ++i) table[i] = static_cast<uint16_t>(*p++);
    for (uint32_t i = 0; i < points; ++i) {
        uint32_t bits[3] = {static_cast<uint32_t>(*p++), static_cast<uint32_t>(*p++), static_cast<uint32_t>(*p++)};
        std::memcpy(&pointArray[i], bits, sizeof bits);
    }
    for (int i = 0; i < 1024; ++i) mapBytes[i] = static_cast<unsigned char>(*p++);
    float amount;
    std::memcpy(&amount, &amountBits, sizeof amount);
    void* data = dataSet != 0 ? mapBytes + 512 : nullptr;
    s3DPoint* point = pointSet != 0 ? pointArray : nullptr;
    tpw_cl_label(kHeader, header);
    tpw_cl_label(kNode, node);
    tpw_cl_label(kData, data);
    tpw_cl_label(kPoint, point);
    // The parts the function must not change: the faces, the vertex-to-point table, the points, the map and the face sets' records.
    auto snapshot = [&]() {
        std::string bytes;
        bytes.append(reinterpret_cast<const char*>(faceArray), sizeof(sFace) * (faces + 1));
        bytes.append(reinterpret_cast<const char*>(table), sizeof(uint16_t) * (vertices + 1));
        bytes.append(reinterpret_cast<const char*>(pointArray), sizeof(s3DPoint) * (points + 1));
        bytes.append(reinterpret_cast<const char*>(mapBytes), sizeof mapBytes);
        bytes.append(reinterpret_cast<const char*>(setArray), sizeof(sFaceSet) * (sets + 1));
        return bytes;
    };
    const std::string before = snapshot();

    tpw_cl_on = 1;
    StripNodeWrapping(header, node, amount, data, point, counter);
    tpw_cl_on = 0;

    int32_t n = 0;
    for (uint32_t i = 0; i < vertices; ++i) {
        out[n++] = Canon(uvs[i >> 2].mF00[i & 3]);
        out[n++] = Canon(uvs[i >> 2].mF00[4 + (i & 3)]);
    }
    for (uint32_t i = 0; i < sets; ++i) out[n++] = setArray[i].mTexture.get() != nullptr ? setArray[i].mTexture->mF00 : 0xffffffffll;
    out[n++] = counter;
    out[n++] = before == snapshot();
    return n;
}

// ---------------------------------------------------------------------------------------------------- activateUI

constexpr int kWindows = 48;
enum WindowLabel : int32_t { kWindow = 100 };

// The window tree: child and next links by window number (-1 none); a window is 16 bytes of storage that is only ever an address.
unsigned char gWindowBytes[kWindows][16];
int32_t gChild[kWindows];
int32_t gNext[kWindows];

InterfaceWindow* Window(int32_t index) { return index < 0 ? nullptr : reinterpret_cast<InterfaceWindow*>(gWindowBytes[index]); }
int32_t Index(const void* window) { return static_cast<int32_t>((static_cast<const unsigned char*>(window) - &gWindowBytes[0][0]) / 16); }

}  // namespace

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return Window(gChild[Index(this)]);
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    return Window(gNext[Index(this)]);
}
// From the case's queue: what a posted message does to the window's links: 1 clears its next, 2 skips its next, 3 clears its child.
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    const int32_t index = Index(window);
    const int64_t action = ret("Relink", 0);
    if (action == 1) gNext[index] = -1;
    if (action == 2 && gNext[index] >= 0) gNext[index] = gNext[gNext[index]];
    if (action == 3) gChild[index] = -1;
    return 0;
}

namespace {

// description: the active argument, the number of windows (at most kWindows), then for each window but the first its parent (the window
// the call starts at is number 0; a window is the last child of its parent so far)
// dump: the links afterwards (child then next, per window)
int32_t RunActivate(const int64_t* d, int64_t* out) {
    const int32_t active = static_cast<int32_t>(d[0]);
    const int32_t count = static_cast<int32_t>(d[1]);
    tpw_cl_on = 0;
    for (int32_t i = 0; i < kWindows; ++i) {
        gChild[i] = -1;
        gNext[i] = -1;
    }
    int32_t last[kWindows];
    for (int32_t i = 0; i < kWindows; ++i) last[i] = -1;
    for (int32_t i = 1; i < count; ++i) {
        const int32_t parent = static_cast<int32_t>(d[1 + i]);
        if (last[parent] < 0)
            gChild[parent] = i;
        else
            gNext[last[parent]] = i;
        last[parent] = i;
    }
    for (int32_t i = 0; i < count; ++i) tpw_cl_label(kWindow + i, gWindowBytes[i]);

    tpw_cl_on = 1;
    activateUI(Window(0), active);
    tpw_cl_on = 0;

    int32_t n = 0;
    for (int32_t i = 0; i < count; ++i) {
        out[n++] = gChild[i];
        out[n++] = gNext[i];
    }
    return n;
}

// ---------------------------------------------------------------------------------------------------- UIInterface::Pause

}  // namespace

void PauseUI() { ev("PauseUI"); }

namespace {

unsigned char gInterfaceBytes[16];

// description: nothing used
int32_t RunPause(const int64_t*, int64_t* out) {
    tpw_cl_on = 1;
    reinterpret_cast<UIInterface*>(gInterfaceBytes)->Pause();
    tpw_cl_on = 0;
    out[0] = 1;
    return 1;
}

// ---------------------------------------------------------------------------------------------------- CWorld

enum WorldLabel : int32_t { kWorld = 1, kOtherWorld = 2, kMouse = 3 };

CWorld* gUnderTest;     // the world ExitLevel is called on (its flag word is what the fakes show)
CWorld* gOtherWorld;

int64_t Flag() { return gUnderTest->mF1DA738; }

}  // namespace

// From the case's queue: 1 makes gGameWorld the other world, 2 clears it (the Mac reads the global again afterwards).
void CMouse::AssignMouseMode(CMouseMode* mode) {
    ev("AssignMouseMode", {lab(this), lab(mode), Flag()});
    const int64_t action = ret("MouseAction", 0);
    if (action == 1) gGameWorld = gOtherWorld;
    if (action == 2) gGameWorld = nullptr;
}
void CMap::DeleteMapInEngine() { ev("DeleteMapInEngine", {lab(reinterpret_cast<unsigned char*>(this) - 0x2d8), Flag()}); }
void CWorld::DeleteAllThings() { ev("DeleteAllThings", {lab(this), Flag()}); }
void Vehicle::clear_behaviours() { ev("clear_behaviours", {Flag()}); }
void BMP_CloseSystem() { ev("BMP_CloseSystem", {Flag()}); }

namespace {

// description: the mouse (0 none, 1 a mouse), gGameWorld (0 none, 1 this world, 2 the other world), the fill byte
// dump: gMouse as a label, gGameWorld as a label, the flag word, whether the mouse is alive, whether the rest of the world is unchanged
int32_t RunExit(const int64_t* d, int64_t* out) {
    const int64_t mouse = d[0], global = d[1], fill = d[2];
    tpw_cl_on = 0;
    static unsigned char* worldBytes = nullptr;
    static unsigned char* otherBytes = nullptr;
    if (worldBytes == nullptr) {
        worldBytes = static_cast<unsigned char*>(std::malloc(sizeof(CWorld)));
        otherBytes = static_cast<unsigned char*>(std::malloc(sizeof(CWorld)));
    }
    std::memset(worldBytes, static_cast<int>(fill), sizeof(CWorld));
    std::memset(otherBytes, 0x5a, sizeof(CWorld));
    gUnderTest = reinterpret_cast<CWorld*>(worldBytes);
    gOtherWorld = reinterpret_cast<CWorld*>(otherBytes);
    gUnderTest->mF1DA738 = 7;
    gOtherWorld->mF1DA738 = 9;
    CMouse* mouseObject = mouse != 0 ? new CMouse : nullptr;
    gMouse = mouseObject;
    gGameWorld = global == 0 ? nullptr : global == 1 ? gUnderTest : gOtherWorld;
    tpw_cl_label(kWorld, gUnderTest);
    tpw_cl_label(kOtherWorld, gOtherWorld);
    tpw_cl_label(kMouse, mouseObject);
    static unsigned char before[0x80];
    std::memcpy(before, worldBytes + sizeof(CWorld) - sizeof before, sizeof before);

    tpw_cl_on = 1;
    gUnderTest->ExitLevel();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = lab(gMouse);
    out[n++] = lab(gGameWorld);
    out[n++] = gUnderTest->mF1DA738;
    out[n++] = Alive(mouseObject) ? 1 : 0;
    // The world but for the flag word is as it was (the last 0x80 bytes hold the flag, whose bytes are skipped).
    static unsigned char after[0x80];
    std::memcpy(after, worldBytes + sizeof(CWorld) - sizeof after, sizeof after);
    const size_t flag = reinterpret_cast<unsigned char*>(&gUnderTest->mF1DA738) - (worldBytes + sizeof(CWorld) - sizeof after);
    std::memset(before + flag, 0, 4);
    std::memset(after + flag, 0, 4);
    out[n++] = std::memcmp(before, after, sizeof before) == 0;
    if (Alive(mouseObject)) ::operator delete(static_cast<void*>(mouseObject));
    gMouse = nullptr;
    gGameWorld = nullptr;
    return n;
}

// description: the operation (0 Free, 1 FreeThingHeap), the fill byte
// dump: the bytes around the world unchanged (the Mac's loop is silent; Free's call of FreeThingHeap is checked on the PowerPC side)
int32_t RunFree(const int64_t* d, int64_t* out) {
    static unsigned char worldBytes[0x100];
    std::memset(worldBytes, static_cast<int>(d[1]), sizeof worldBytes);
    static unsigned char before[0x100];
    std::memcpy(before, worldBytes, sizeof before);
    CWorld* world = reinterpret_cast<CWorld*>(worldBytes);
    tpw_cl_on = 1;
    if (d[0] == 0)
        world->Free();
    else
        world->FreeThingHeap();
    tpw_cl_on = 0;
    out[0] = std::memcmp(before, worldBytes, sizeof before) == 0;
    out[1] = 1;   // (the PowerPC side shows that Free called FreeThingHeap on the same world)
    return 2;
}

// ---------------------------------------------------------------------------------------------------- CStaffDB::~CStaffDB

}  // namespace

CStaffInfo* gStaffBase;
// (the Mac passes the flag -1 to each element: the host's explicit destructor call has none)
CStaffInfo::~CStaffInfo() { ev("CStaffInfo.dtor", {static_cast<int64_t>(this - gStaffBase)}); }

namespace {

enum StaffLabel : int32_t { kStaff = 1 };

// description: the flag (the Mac's r4, a 32-bit word of which the low half is looked at)
// dump: whether the block is alive afterwards
int32_t RunStaff(const int64_t* d, int64_t* out) {
    const int32_t flag = static_cast<int32_t>(d[0]);
    tpw_cl_on = 0;
    unsigned char* block = static_cast<unsigned char*>(::operator new(0x20 * sizeof(CStaffInfo)));
    std::memset(block, 0xa5, 0x20 * sizeof(CStaffInfo));
    gStaffBase = reinterpret_cast<CStaffInfo*>(block);
    tpw_cl_label(kStaff, block);
    CStaffDB* database = reinterpret_cast<CStaffDB*>(block);
    tpw_cl_on = 1;
    database->~CStaffDB();
    if (static_cast<int16_t>(flag) > 0) ::operator delete(static_cast<void*>(block));
    tpw_cl_on = 0;
    out[0] = Alive(block) ? 1 : 0;
    out[1] = 1;   // (the Mac answers `this`; the destructor of a NULL object does nothing: checked on the PowerPC side)
    if (Alive(block)) ::operator delete(static_cast<void*>(block));
    return 2;
}

// ---------------------------------------------------------------------------------------------------- CSkyObjects::DestroySkyObjects

constexpr int kObjects = 16;
// The balance word at the Mac's 0x6e4 as a value slot (the first slot is at 0x08, 4 bytes each), worked out here and not taken from the header.
constexpr int kBalanceSlot = (0x6e4 - 0x08) / 4;
enum SkyLabel : int32_t { kSkyObjects = 1, kSky = 2, kObject = 30, kOtherObject = 90 };

unsigned char gOtherObjectBytes[8];
void** gObjects;

}  // namespace

// From the case's queue: after the call the count (the Mac reads the balance word again every time round) is replaced by "Count", and the
// next slot is given the other object when "Replace" is 1.
int32_t CSky::RemoveAdd(void* texture) {
    ev("RemoveAdd", {lab(this), lab(texture)});
    const int64_t count = ret("Count", -9999);
    if (count != -9999) reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[kBalanceSlot].mInteger = static_cast<int32_t>(count);
    const int64_t replace = ret("Replace", -1);
    if (replace >= 0 && replace < kObjects) gObjects[replace] = gOtherObjectBytes;
    return 0;
}

namespace {

// description: the sky objects' count (the balance word), the number of objects built (at most kObjects), the fill byte
// dump: the count word, the pointers as labels (all kObjects), whether the object block is unchanged but for the pointers
int32_t RunSky(const int64_t* d, int64_t* out) {
    const int32_t count = static_cast<int32_t>(d[0]);
    const int32_t objects = static_cast<int32_t>(d[1]);
    tpw_cl_on = 0;
    reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[kBalanceSlot].mInteger = count;
    unsigned char* block = static_cast<unsigned char*>(::operator new(sizeof(void*) * kObjects));
    std::memset(block, static_cast<int>(d[2]), sizeof(void*) * kObjects);
    CSkyObjects* sky = reinterpret_cast<CSkyObjects*>(block);
    gObjects = sky->mObjects;
    unsigned char* things[kObjects] = {};
    for (int32_t i = 0; i < objects; ++i) {
        things[i] = static_cast<unsigned char*>(::operator new(8));
        tpw_cl_label(kObject + i, things[i]);
        gObjects[i] = things[i];
    }
    tpw_cl_label(kSkyObjects, sky);
    tpw_cl_label(kSky, gSkyStorage);
    tpw_cl_label(kOtherObject, gOtherObjectBytes);

    tpw_cl_on = 1;
    sky->DestroySkyObjects();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[kBalanceSlot].mInteger;
    for (int32_t i = 0; i < kObjects; ++i) out[n++] = i < objects ? lab(gObjects[i]) : 0;
    for (int32_t i = 0; i < objects; ++i) ::operator delete(static_cast<void*>(things[i]));
    ::operator delete(static_cast<void*>(block));
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------- the entry points

extern "C" {

int32_t tpw_l7_theme(const int64_t* d, int64_t* out) { return RunTheme(d, out); }
int32_t tpw_l7_database(const int64_t* d, int64_t* out) { return RunDatabase(d, out); }
int32_t tpw_l7_strip(const int64_t* d, int64_t* out) { return RunStrip(d, out); }
int32_t tpw_l7_activate(const int64_t* d, int64_t* out) { return RunActivate(d, out); }
int32_t tpw_l7_pause(const int64_t* d, int64_t* out) { return RunPause(d, out); }
int32_t tpw_l7_exit(const int64_t* d, int64_t* out) { return RunExit(d, out); }
int32_t tpw_l7_free(const int64_t* d, int64_t* out) { return RunFree(d, out); }
int32_t tpw_l7_staff(const int64_t* d, int64_t* out) { return RunStaff(d, out); }
int32_t tpw_l7_sky(const int64_t* d, int64_t* out) { return RunSky(d, out); }

}  // extern "C"
