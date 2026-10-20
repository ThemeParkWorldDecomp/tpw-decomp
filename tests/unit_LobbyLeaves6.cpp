// Test library for the lobby's small leaves part 6 (unit w75; libtpw_unit_LobbyLeaves6.dylib, tests/test_LobbyLeaves6.py):
//   src/Game_exit.cpp            CGame::ExitLevel 0x101496ec
//   src/InGame_start.cpp         CInGame::Start 0x1014b304
//   src/ui_progress_cursor.cpp   CursorLibrary::RemoveAll 0x101cc8ec, FuckOffUIProgressBar 0x101e0294
//   src/ui_image_dtor.cpp        InterfaceImage::~InterfaceImage 0x101cfebc
//   src/MeshDestroy.cpp          DestroyEngineMesh 0x10065004, DestroyMeshInstance 0x100631c4
//   src/RideWrapping.cpp         StripRideWrapping 0x101ee150
//
// Real: the six sources above. Fakes that record the same event on both sides: CWorld::ExitLevel and Free, the world's CStaffDB
// destructor, CRideDatabase::ClearDatabase, CSky::Close, the sky objects' destructor and DestroySkyObjects, the message centre's
// subscription headers' destructor, CGame::ClearThemeResources, UI_SendMessage, the cursors' and the engine mesh's destructors,
// InterfaceWindow's destructor, DisableMeshDraw, MReleaseFreeF and StripNodeWrapping. Operator delete is the call log's (it records
// the deletion of a labelled block). Each operation has one entry point: a description of numbers in, a dump of numbers out, the
// events in the call log. The world of a case is built here (host) and in emulator memory at the Mac's layouts (test module).
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "Colours.h"
#include "Game.h"
#include "InGame.h"
#include "InterfaceWindow.h"
#include "LbPalette.h"
#include "SkyObjects.h"
#include "StaffDB.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "World.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_mesh.h"
#include "tb_resource_display.h"
#include "tb_sky.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
CRideDatabase gRideDatabase;
// The sky and the display are only addresses here (their classes are large): raw storage under the engine's symbol names.
#ifdef __APPLE__
extern unsigned char gSkyStorage[sizeof(CSky)] __asm__("_gei_Sky");
extern unsigned char gDisplayStorage[sizeof(CDisplay)] __asm__("_gei_Display");
#else
extern unsigned char gSkyStorage[sizeof(CSky)] __asm__("gei_Sky");
extern unsigned char gDisplayStorage[sizeof(CDisplay)] __asm__("gei_Display");
#endif
alignas(16) unsigned char gSkyStorage[sizeof(CSky)] = {};
alignas(16) unsigned char gDisplayStorage[sizeof(CDisplay)] = {};
struct TbPalette {
    uint8_t mEntries[256][4];
};
TbPalette _lbGlobalPalette;
CColours gColours;

// ---- colours
TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}
CColours::CColours() : mRed(0), mGreen(1), mBlue(2), mYellow(3), mWhite(4), mBlack(nullptr) {}

namespace {

enum Label : int32_t {
    kGame = 1, kWorld = 2, kWorld2 = 3, kDatabase = 4, kSky = 5, kSkyObjects = 6, kSkyObjects2 = 7, kCentre = 8,
    kMesh = 1, kOtherMesh = 2, kEngine = 3, kAllocation = 4,
    kNode = 10, kCursor = 40,
    kHeader = 1, kData = 7, kPoint = 8, kFirstArrayNode = 100, kFirstMiniNode = 200, kStray = 300,
    kImage = 1, kWindowDummy = 9,
};

// ---------------------------------------------------------------------------------------------------------- CGame::ExitLevel

CGame* gExitGame;
CWorld* gOtherWorld;
CSkyObjects* gOtherSky;
CSubscriptionHeader* gCentreBlock;

// What a fake of the world does to the global, from the case's queue: 1 puts the other world there, 2 clears it.
void WorldHook(const char* key) {
    const int64_t action = ret(key, 0);
    if (action == 1) gGameWorld = gOtherWorld;
    if (action == 2) gGameWorld = nullptr;
}

}  // namespace

void CWorld::ExitLevel() {
    ev("CWorld.ExitLevel", {lab(this)});
    WorldHook("ExitAction");
}
void CWorld::Free() {
    ev("CWorld.Free", {lab(this)});
    WorldHook("FreeAction");
}
// (the Mac's call passes the flag -1, which keeps the block; the host has no such argument, the PowerPC side checks it)
CStaffDB::~CStaffDB() { ev("CStaffDB.dtor", {lab(this), 1}); }
void CRideDatabase::ClearDatabase() { ev("CRideDatabase.ClearDatabase", {lab(this)}); }
int32_t CSky::Close() {
    ev("CSky.Close", {lab(this)});
    return 0;
}
CSkyObjects::CSkyObjects() {}
CSkyObjects::~CSkyObjects() { ev("CSkyObjects.dtor", {lab(this)}); }
void CSkyObjects::DestroySkyObjects() {
    ev("CSkyObjects.Destroy", {lab(this)});
    // From the case's queue: the sky objects member is replaced behind the function's back (the Mac reads it again afterwards).
    if (ret("SkyAction", 0) == 1) gExitGame->mSkyObjects = gOtherSky;
}
CSubscriptionHeader::CSubscriptionHeader() {}
CSubscriptionHeader::~CSubscriptionHeader() { ev("CSubscriptionHeader.dtor", {static_cast<int64_t>(this - gCentreBlock)}); }
void CGame::ClearThemeResources() { ev("CGame.ClearThemeResources", {lab(this)}); }

namespace {

bool Alive(const void* object) { return object != nullptr && lab(object) > 0; }
int64_t LabelOrZero(const void* object) { return lab(object); }

// description: world (0 none, 1 a world), flags (the display word), sky (0 none, 1 an object), centre (0 none, 1 a block), fill (byte)
// dump: the world global, the sky objects member, the message centre global (each a label, 0 NULL), the display word, five alive flags
// (world, other world, sky objects, other sky objects, message centre block), the rest of the game object unchanged.
int32_t RunExit(const int64_t* d, int64_t* out) {
    const int64_t world = d[0], flags = d[1], sky = d[2], centre = d[3], fill = d[4];
    alignas(16) static unsigned char gameStorage[sizeof(CGame) + 64];
    std::memset(gameStorage, static_cast<int>(fill), sizeof gameStorage);
    gExitGame = reinterpret_cast<CGame*>(gameStorage);
    gExitGame->mSkyObjects = nullptr;
    tpw_cl_on = 0;
    CWorld* worldObject = world != 0 ? static_cast<CWorld*>(::operator new(0x40)) : nullptr;
    gOtherWorld = static_cast<CWorld*>(::operator new(0x40));
    CSkyObjects* skyObject = sky != 0 ? new CSkyObjects : nullptr;
    gOtherSky = new CSkyObjects;
    CMessageCentre* centreObject = centre != 0 ? new CMessageCentre : nullptr;
    gCentreBlock = centreObject != nullptr ? centreObject->mSubscriptions : nullptr;
    gGameWorld = worldObject;
    gExitGame->mSkyObjects = skyObject;
    gMessageCentre = centreObject;
    reinterpret_cast<CDisplay*>(gDisplayStorage)->mFlags = static_cast<uint32_t>(flags);
    tpw_cl_label(kGame, gExitGame);
    tpw_cl_label(kWorld, worldObject);
    tpw_cl_label(kWorld2, gOtherWorld);
    tpw_cl_label(kDatabase, &gRideDatabase);
    tpw_cl_label(kSky, gSkyStorage);
    tpw_cl_label(kSkyObjects, skyObject);
    tpw_cl_label(kSkyObjects2, gOtherSky);
    tpw_cl_label(kCentre, centreObject);
    unsigned char before[sizeof gameStorage];
    std::memcpy(before, gameStorage, sizeof before);

    tpw_cl_on = 1;
    gExitGame->ExitLevel();
    tpw_cl_on = 0;

    // The game object but for the sky objects member is as it was.
    unsigned char after[sizeof gameStorage];
    std::memcpy(after, gameStorage, sizeof after);
    const size_t offset = reinterpret_cast<unsigned char*>(&gExitGame->mSkyObjects) - gameStorage;
    std::memset(after + offset, 0, sizeof(CSkyObjects*));
    std::memset(before + offset, 0, sizeof(CSkyObjects*));
    int32_t n = 0;
    out[n++] = LabelOrZero(gGameWorld);
    out[n++] = LabelOrZero(gExitGame->mSkyObjects);
    out[n++] = LabelOrZero(gMessageCentre);
    out[n++] = reinterpret_cast<CDisplay*>(gDisplayStorage)->mFlags;
    out[n++] = Alive(worldObject);
    out[n++] = Alive(gOtherWorld);
    out[n++] = Alive(skyObject);
    out[n++] = Alive(gOtherSky);
    out[n++] = Alive(centreObject);
    out[n++] = std::memcmp(before, after, sizeof before) == 0;

    // Clean up what is alive.
    if (Alive(worldObject)) ::operator delete(worldObject);
    if (Alive(gOtherWorld)) ::operator delete(gOtherWorld);
    if (Alive(skyObject)) delete skyObject;
    if (Alive(gOtherSky)) delete gOtherSky;
    if (Alive(centreObject)) delete centreObject;
    gGameWorld = nullptr;
    gMessageCentre = nullptr;
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------- CInGame::Start

namespace {

// description: the 1024 palette bytes, the 5 x 50 bytes of the ramps' colours, the 5 x 6 bytes after each ramp's colours and mBlack
// (as the Mac keeps them: padding and pointer, for the host only compared with themselves), nothing else
// dump: the 250 colour bytes, the palette unchanged, the rest of the ramps unchanged
CColourRamp* Ramp(int i) {
    CColourRamp* ramps[5] = {&gColours.mRed, &gColours.mGreen, &gColours.mBlue, &gColours.mYellow, &gColours.mWhite};
    return ramps[i];
}

int32_t RunStart(const int64_t* d, int64_t* out) {
    for (int i = 0; i < 1024; ++i) _lbGlobalPalette.mEntries[i / 4][i % 4] = static_cast<uint8_t>(d[i]);
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 10; ++c) {
            TbColour& colour = Ramp(r)->mColours[c];
            const int64_t* b = d + 1024 + (r * 10 + c) * 5;
            colour.mAlpha = static_cast<uint8_t>(b[0]);
            colour.mRed = static_cast<uint8_t>(b[1]);
            colour.mGreen = static_cast<uint8_t>(b[2]);
            colour.mBlue = static_cast<uint8_t>(b[3]);
            colour.mIndex = static_cast<uint8_t>(b[4]);
        }
    }
    TbPalette paletteBefore = _lbGlobalPalette;
    // The parts of the ramps and of gColours that are not colours: marked, and compared afterwards.
    TbColour* selected[5];
    for (int r = 0; r < 5; ++r) {
        Ramp(r)->mPadding[0] = static_cast<uint8_t>(0x30 + r);
        Ramp(r)->mPadding[1] = static_cast<uint8_t>(0x40 + r);
        selected[r] = Ramp(r)->mSelected;
    }
    TbColour* black = gColours.mBlack;

    CInGame::Start();

    int32_t n = 0;
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 10; ++c) {
            const TbColour& colour = Ramp(r)->mColours[c];
            out[n++] = colour.mAlpha;
            out[n++] = colour.mRed;
            out[n++] = colour.mGreen;
            out[n++] = colour.mBlue;
            out[n++] = colour.mIndex;
        }
    }
    out[n++] = std::memcmp(&paletteBefore, &_lbGlobalPalette, sizeof paletteBefore) == 0;
    bool rest = black == gColours.mBlack;
    for (int r = 0; r < 5; ++r)
        rest = rest && selected[r] == Ramp(r)->mSelected && Ramp(r)->mPadding[0] == 0x30 + r && Ramp(r)->mPadding[1] == 0x40 + r;
    out[n++] = rest;
    return n;
}

// ------------------------------------------------------------------------------------------ CursorLibrary::RemoveAll

constexpr int kMaxNodes = 12;
CursorLibrary::Node* gNodes[kMaxNodes];
int32_t gPoison;

class TestCursor : public InterfaceCursor {
public:
    explicit TestCursor(int32_t index) : mIndex(index) {}
    ~TestCursor() override {
        ev("Cursor.dtor", {lab(this)});
        // The next node was read before this ran: pointing it elsewhere changes nothing.
        if (mIndex == gPoison) gNodes[mIndex]->mNext = nullptr;
    }
    int32_t mIndex;
};

// description: nodes (0..kMaxNodes), cursor bits (bit i: node i has a cursor), poison (-1 none, else the node whose cursor's
// destructor clears its node's next)
// dump: the head unchanged, then per node: node alive, cursor alive
int32_t RunCursors(const int64_t* d, int64_t* out) {
    const int32_t count = static_cast<int32_t>(d[0]);
    const uint32_t bits = static_cast<uint32_t>(d[1]);
    gPoison = static_cast<int32_t>(d[2]);
    tpw_cl_on = 0;
    InterfaceCursor* cursors[kMaxNodes] = {};
    for (int i = 0; i < count; ++i) {
        gNodes[i] = new CursorLibrary::Node{};
        tpw_cl_label(kNode + i, gNodes[i]);
        if ((bits >> i) & 1) {
            cursors[i] = new TestCursor(i);
            tpw_cl_label(kCursor + i, cursors[i]);
        }
        gNodes[i]->mCursor = cursors[i];
    }
    for (int i = 0; i + 1 < count; ++i) gNodes[i]->mNext = gNodes[i + 1];
    gCursorLibrary = count > 0 ? gNodes[0] : nullptr;
    CursorLibrary::Node* const head = gCursorLibrary;

    tpw_cl_on = 1;
    CursorLibrary::RemoveAll();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = gCursorLibrary == head;
    // (the poisoned node's successor and the nodes behind it are freed all the same, so what is alive is told by the labels)
    for (int i = 0; i < count; ++i) {
        out[n++] = Alive(gNodes[i]) ? 1 : 0;
        out[n++] = cursors[i] != nullptr && Alive(cursors[i]) ? 1 : 0;
    }
    for (int i = 0; i < count; ++i) {
        if (Alive(cursors[i])) delete cursors[i];
        if (Alive(gNodes[i])) delete gNodes[i];
    }
    gCursorLibrary = nullptr;
    return n;
}

}  // namespace

InterfaceCursor::InterfaceCursor() {}
InterfaceCursor::~InterfaceCursor() {}
uint8_t InterfaceCursor::GetVisible() { return 0; }
void InterfaceCursor::SetData(void*, char) {}
void InterfaceCursor::Show() {}
void InterfaceCursor::Hide() {}
void InterfaceCursor::Use(int16_t) {}
void InterfaceCursor::Draw(int16_t, int16_t) {}
void InterfaceCursor::Animate(int32_t) {}

// ------------------------------------------------------------------------------------------ FuckOffUIProgressBar

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

namespace {

unsigned char gWindowDummy[16];

// description: the progress bar window (0 none, 1 a window)
// dump: the global as a label (not cleared)
int32_t RunProgress(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    gUIProgressBar = d[0] != 0 ? reinterpret_cast<InterfaceWindow*>(gWindowDummy) : nullptr;
    tpw_cl_label(kWindowDummy, gWindowDummy);
    tpw_cl_on = 1;
    FuckOffUIProgressBar();
    tpw_cl_on = 0;
    out[0] = lab(gUIProgressBar);
    gUIProgressBar = nullptr;
    return 1;
}

}  // namespace

// ------------------------------------------------------------------------------------------ InterfaceImage::~InterfaceImage

// The window the image sits on: its constructor and the virtuals are the unit's; the destructor records.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
// (the Mac's call passes the flag 0 and the vtable word is the image's by then: the PowerPC side checks both)
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this), 1, 0}); }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceImage::InterfaceImage(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                               int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
int32_t InterfaceImage::WhatAmI() { return 0; }
int32_t InterfaceImage::SetValue(int32_t) { return 0; }
int32_t InterfaceImage::GetValue() { return 0; }

namespace {

// description: the flag (the Mac's r4, a 32-bit word of which the low half is looked at)
// dump: whether the image is alive afterwards, the answer
int32_t RunImage(const int64_t* d, int64_t* out) {
    const int32_t flag = static_cast<int32_t>(d[0]);
    tpw_cl_on = 0;
    InterfaceImage* image = new InterfaceImage(nullptr, 0, 0, 0, 0, 0, 0);
    tpw_cl_label(kImage, image);
    tpw_cl_on = 1;
    if (static_cast<int16_t>(flag) > 0)
        delete image;
    else
        image->~InterfaceImage();
    tpw_cl_on = 0;
    out[0] = Alive(image) ? 1 : 0;
    out[1] = 1;   // (the Mac answers `this`; the host's destructor answers nothing: the PowerPC side checks r3)
    // (the non-deleting form leaves the object: free it now, the destructor having run)
    if (Alive(image)) ::operator delete(static_cast<void*>(image));
    return 2;
}

}  // namespace

// ------------------------------------------------------------------------------------------ DestroyEngineMesh, DestroyMeshInstance

CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() { ev("EngineMesh.dtor", {lab(this)}); }
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }

// From the case's queue: whether the fake takes the mesh out of the draw lists as the real one does (clears flag 1).
void DisableMeshDraw(tag_sMesh* mesh) {
    ev("DisableMeshDraw", {lab(mesh)});
    if (ret("Disable", 0) == 1) mesh->mF00 &= ~1u;
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

namespace {

// description: op (0 DestroyEngineMesh, 1 DestroyMeshInstance), flags, other (0 none, 1 another mesh, 2 the mesh itself), the mesh's
// count, the other mesh's count, engine mesh (0 none, 1 one), allocation (0 none, 1 a block), the heap and flags arguments of
// DestroyMeshInstance (unused), fill (a byte for the words nothing uses)
// dump: flags, the other pointer as a label, the engine pointer as a label, the counts, alive flags (engine, allocation), the rest
// of the mesh unchanged
int32_t RunMesh(const int64_t* d, int64_t* out) {
    const int64_t op = d[0], flags = d[1], other = d[2], count = d[3], otherCount = d[4], engine = d[5], allocation = d[6];
    const int64_t fill = d[9];
    tpw_cl_on = 0;
    tag_sMesh* mesh = new tag_sMesh{};
    tag_sMesh* otherMesh = new tag_sMesh{};
    std::memset(mesh->mF0C, static_cast<int>(fill), sizeof mesh->mF0C);
    mesh->mF10 = static_cast<int32_t>(fill * 0x01010101);
    std::memset(mesh->mF14, static_cast<int>(fill), sizeof mesh->mF14);
    std::memset(mesh->mF38, static_cast<int>(fill), sizeof mesh->mF38);
    mesh->mF00 = static_cast<uint32_t>(flags);
    mesh->mF08 = other == 0 ? nullptr : other == 1 ? otherMesh : mesh;
    mesh->mEngineMeshes = static_cast<int32_t>(count);
    otherMesh->mEngineMeshes = static_cast<int32_t>(otherCount);
    CMesh* engineMesh = engine != 0 ? new CMesh : nullptr;
    mesh->mEngineMesh = engineMesh;
    void* block = allocation != 0 ? ::operator new(8) : nullptr;
    mesh->mAllocation = block;
    tpw_cl_label(kMesh, mesh);
    tpw_cl_label(kOtherMesh, otherMesh);
    tpw_cl_label(kEngine, engineMesh);
    tpw_cl_label(kAllocation, block);
    tag_sMesh before = *mesh;

    tpw_cl_on = 1;
    if (op == 0)
        DestroyEngineMesh(mesh);
    else
        DestroyMeshInstance(mesh, reinterpret_cast<sHeapInfo*>(static_cast<intptr_t>(d[7])), static_cast<uint32_t>(d[8]));
    tpw_cl_on = 0;

    // The mesh but for the flags, the engine pointer and the count that went down is as it was.
    tag_sMesh after = *mesh;
    after.mF00 = before.mF00;
    after.mEngineMesh = before.mEngineMesh;
    after.mEngineMeshes = before.mEngineMeshes;
    int32_t n = 0;
    out[n++] = mesh->mF00;
    out[n++] = lab(mesh->mF08);
    out[n++] = lab(mesh->mEngineMesh);
    out[n++] = mesh->mEngineMeshes;
    out[n++] = otherMesh->mEngineMeshes;
    out[n++] = Alive(engineMesh) ? 1 : 0;
    out[n++] = Alive(block) ? 1 : 0;
    out[n++] = std::memcmp(&before, &after, sizeof before) == 0;
    if (Alive(engineMesh)) delete engineMesh;
    if (block != nullptr) ::operator delete(block);
    delete mesh;
    delete otherMesh;
    return n;
}

}  // namespace

// ------------------------------------------------------------------------------------------ StripRideWrapping

namespace {

const int32_t* gSeen[64];
int32_t gSeenCount;
unsigned char gDataDummy[16];
s3DPoint gPointDummy;

}  // namespace

void StripNodeWrapping(tag_sM3D2Header* header, tag_sNode* node, float amount, void* data, s3DPoint* point, int32_t& counter) {
    int32_t id = 0;
    for (int32_t i = 0; i < gSeenCount; ++i)
        if (gSeen[i] == &counter) id = i + 1;
    if (id == 0 && gSeenCount < 64) {
        gSeen[gSeenCount++] = &counter;
        id = gSeenCount;
    }
    ev("StripNodeWrapping", {lab(header), lab(node), fbits(amount), lab(data), lab(point), id, counter});
    counter = static_cast<int32_t>(ret("Counter", 0));
}

namespace {

// description: the full-node count (+0x42), the count at +0x44, the node argument (0 none, 1 + k the k-th full node, 1000 + k the
// k-th mini node, -1 a node that is in neither array), the flags of the full nodes then of the mini nodes (as many as the arrays
// have), the amount's bits, whether the data and the point pointers are set
int32_t RunStrip(const int64_t* d, int64_t* out) {
    const int32_t all = static_cast<int32_t>(d[0]);
    const int32_t split = static_cast<int32_t>(d[1]);
    const int64_t node = d[2];
    const int32_t fullCount = all < split ? all : split;
    const int32_t miniCount = all > split ? all - split : 0;
    const int64_t* flags = d + 3;
    const uint32_t amountBits = static_cast<uint32_t>(d[3 + fullCount + miniCount]);
    const int64_t dataSet = d[4 + fullCount + miniCount], pointSet = d[5 + fullCount + miniCount];
    tpw_cl_on = 0;
    tag_sRideAnimSet* set = new tag_sRideAnimSet{};
    tag_sMesh* mesh = new tag_sMesh{};
    tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    tag_sNode* full = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode) * (fullCount + 1)));
    tag_sMiniNode* mini = static_cast<tag_sMiniNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sMiniNode) * (miniCount + 1)));
    tag_sNode stray{};
    set->mHeads = mesh;
    mesh->mHeader = header;
    header->mAllNodeCount = static_cast<uint16_t>(all);
    header->mNodeCount = static_cast<uint16_t>(split);
    header->mNodes.set(full);
    header->mMiniNodes.set(mini);
    tpw_cl_label(kHeader, header);
    for (int i = 0; i < fullCount; ++i) {
        full[i].mFlags = static_cast<uint32_t>(flags[i]);
        tpw_cl_label(kFirstArrayNode + i, &full[i]);
    }
    for (int i = 0; i < miniCount; ++i) {
        mini[i].mFlags = static_cast<uint32_t>(flags[fullCount + i]);
        tpw_cl_label(kFirstMiniNode + i, &mini[i]);
    }
    tpw_cl_label(kStray, &stray);
    tpw_cl_label(kData, gDataDummy);
    tpw_cl_label(kPoint, &gPointDummy);
    tag_sNode* argument = node == 0 ? nullptr
                          : node == -1 ? &stray
                          : node >= 1000 ? static_cast<tag_sNode*>(&mini[node - 1000])
                                         : &full[node - 1];
    float amount;
    std::memcpy(&amount, &amountBits, sizeof amount);
    gSeenCount = 0;

    tpw_cl_on = 1;
    StripRideWrapping(set, argument, amount, dataSet != 0 ? gDataDummy : nullptr, pointSet != 0 ? &gPointDummy : nullptr);
    tpw_cl_on = 0;

    out[0] = gSeenCount;
    delete set;
    delete mesh;
    return 1;
}

}  // namespace

// ------------------------------------------------------------------------------------------ the entry points

extern "C" {

int32_t tpw_l6_exit(const int64_t* d, int64_t* out) { return RunExit(d, out); }
int32_t tpw_l6_start(const int64_t* d, int64_t* out) { return RunStart(d, out); }
int32_t tpw_l6_cursors(const int64_t* d, int64_t* out) { return RunCursors(d, out); }
int32_t tpw_l6_progress(const int64_t* d, int64_t* out) { return RunProgress(d, out); }
int32_t tpw_l6_image(const int64_t* d, int64_t* out) { return RunImage(d, out); }
int32_t tpw_l6_mesh(const int64_t* d, int64_t* out) { return RunMesh(d, out); }
int32_t tpw_l6_strip(const int64_t* d, int64_t* out) { return RunStrip(d, out); }

}  // extern "C"
