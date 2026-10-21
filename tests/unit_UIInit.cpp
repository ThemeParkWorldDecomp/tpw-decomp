// Test library for the interface's start-up (libtpw_unit_UIInit.dylib; see tests/test_UIInit.py):
// src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp (UI_InitSystem, the message queues, the timer table, the pointer, the cursors),
// src/game/ui/InterfaceWindow.cpp (the constructor, PointIn, MakeRectAbsolute), src/game/ui/InterfaceBFCursor.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_main.cpp (the colour
// cursors, InterfaceBFCursor, InitLobbyUI), and under them the real src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp (rectangles),
// src/game/ui/UI_Base.cpp (the globals), src/game/ui/InterfaceCursor.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceDrawObject.cpp (cursors, the cursor table) and src/game/Light.cpp.
//
// Everything else the unit calls is a fake that records through calllog: the window's untranslated
// virtual members, the timers' constructor, the picture loaders and the platform's pixel and cursor
// classes, FindFilePath, TRACE, the engine's light and map. The memory of every object the unit makes
// with operator new is filled with 0xa5 first, so a member nothing sets compares as untouched.
//
// The Python module drives this library through extern "C" operations on a small world: eight
// windows in fixed storage, two message queues and a timed queue, six cursors, two cursor tables, a
// help text object, clocks, peek functions and three fake regions; and a second world of cursor
// pictures served to the picture loader. The dumps write the state in the order of the Python
// module's tables.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_CALLLOG_LABELS 2048   // the cursor loader makes dozens of objects a picture
#include "calllog.cpp"

#include "Debug.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPConfig.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbGraphicFile.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "fake_arena_on_heap.h"

// The unit links no arena: the windows' game heap (include/InterfaceWindow.h, unit v5L31) is the observed operator new above, and the one
// arena read of the real files (InterfaceDrawEngineMesh's constructor, which no case runs) has a null base.
namespace tpw {
uint8_t* gArenaBase = nullptr;
}  // namespace tpw

using namespace calllog;
using namespace SamsUtilities;

namespace {
constexpr int kSlots = 8;
constexpr int kCursors = 6;
constexpr int kRegions = 3;
constexpr int kTables = 2;
constexpr int kWindowLabel = 10, kQueueLabel = 30, kTimedLabel = 32, kCursorLabel = 20, kHelpLabel = 40, kClockLabel = 41,
              kDefaultClockLabel = 43, kRegionLabel = 50, kTableLabel = 60, kLinkLabel = 90, kFileLabel = 70;

// What the next array allocations are (host element size, Mac element size, cookie bytes), set by the driver:
// the host's arrays are of larger elements, so the event is the Mac's size worked out from the count.
struct Expect {
    std::size_t host;
    int64_t mac;
    std::size_t cookie;
};
constexpr int kExpectMax = 64;
Expect gExpectList[kExpectMax];
int gExpectHead, gExpectTail;
void ExpectClear() { gExpectHead = gExpectTail = 0; }
void ExpectPush(Expect expect) { gExpectList[gExpectTail++ % kExpectMax] = expect; }
bool ExpectEmpty() { return gExpectHead == gExpectTail; }
Expect ExpectPop() { return gExpectList[gExpectHead++ % kExpectMax]; }

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!on()) return;
    std::memset(block, 0xa5, size);
    if (array) {
        if (ExpectEmpty()) {
            ev("new[] unexpected", {static_cast<int64_t>(size)});
            return;
        }
        const Expect expect = ExpectPop();
        const int64_t count = static_cast<int64_t>((size - expect.cookie) / expect.host);
        ev("new[]", {count * expect.mac + static_cast<int64_t>(expect.cookie)});
        return;
    }
    int64_t mac = 0;
    if (size == sizeof(InterfaceWindow)) mac = 0x138;
    else if (size == sizeof(InterfaceMessageQueue)) mac = 0x14;
    else if (size == sizeof(InterfaceTimedMessages)) mac = 8;
    else if (size == sizeof(InterfaceBFCursor)) mac = 0x18;
    else if (size == sizeof(UCursorOpenGL)) mac = 0x84;
    else return;
    newlab(block);
    ev("new", {mac});
}

bool Untouched(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}
bool Zero(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0) return false;
    return true;
}
}  // namespace

// ---- the window's untranslated members (the unit's constructor reaches CalcClientArea)
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() { ev("CalcClientArea", {lab(this)}); }
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
int32_t UI_DefaultWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void activateUI(InterfaceWindow*, int32_t) {}

// ---- the timer table's elements: the constructor leaves a number in each (the order they were made in)
namespace {
uint32_t gTimerCtorCount;
}
InterfaceTimer::InterfaceTimer() { mStart = ++gTimerCtorCount; }
InterfaceTimer::~InterfaceTimer() {}

// ---- the clock objects
uint32_t clockTickFunction::tickyticky() { return 0; }

// ---- the help text object
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t, int16_t) {}
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*) {}
void InterfaceHelpText::OnMouseMovedOut() {}
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}
// The constructor and the two virtuals unit w36 added to the class (src/game/ui/InterfaceHelpText.cpp; the vtable must link here too).
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

namespace {
class FakeHelp : public InterfaceHelpText {
public:
    void Enable() override { ev("Help.Enable", {lab(this)}); }
    void Disable() override { ev("Help.Disable", {lab(this)}); }
};
FakeHelp gHelp;

// A cursor whose Show, Hide and Use record.
class FakeCursor : public InterfaceCursor {
public:
    void Show() override { ev("Cursor.Show", {lab(this)}); }
    void Hide() override { ev("Cursor.Hide", {lab(this)}); }
    void Use(int16_t a) override { ev("Cursor.Use", {lab(this), a}); }
};
FakeCursor gCursors[kCursors];
CursorTable gTables[kTables];

// A region that answers by the point.
class FakeRegion : public CoolRegion {
public:
    int32_t seed = 0;
    bool PointIn(int16_t x, int16_t y) override {
        ev("Region.PointIn", {lab(this), x, y});
        return ((x * 7 + y * 3 + seed) & 3) != 0;
    }
};
FakeRegion gRegions[kRegions];

int32_t PeekA(InterfaceWindow*, int32_t, int32_t, int32_t) { return 1; }
int32_t PeekB(InterfaceWindow*, int32_t, int32_t, int32_t) { return 2; }
int32_t OtherCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 3; }
clockTickFunction gClocks[2];
}  // namespace

// ---- the engine's cursors table and the other files' globals
CursorTable gCursorTable;
UIInterface gUIInterface;
int32_t gScreenWidth;
void CreateEngineMesh(tag_sMesh*) {}
void DestroyEngineMesh(tag_sMesh*) {}
void DestroyMeshInstance(tag_sMesh*, sHeapInfo*, uint32_t) {}
void EnableMeshDraw(tag_sMesh*, sMatrix*, int32_t) {}
void DisableMeshDraw(tag_sMesh*) {}
void InterfaceDrawEngineMesh::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() { return this; }
void InterfaceDrawEngineResizableFrame::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableFrame::InstanceMe() { return this; }
void InterfaceDrawEngineResizableSlider::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableSlider::InstanceMe() { return this; }

// ---- the engine, for InitLobbyUI
CSystem::CSystem() {}
CSystem gei_System;
void CSystem::UpdateLights() { ev("UpdateLights"); }
CMapLinkBase::~CMapLinkBase() {}
CLight* gev_LightChain;
namespace {
int32_t gMapStorage;
}
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(&gMapStorage);
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("Link", {lab(link), list});
    return 1;
}
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CPlayerSettings gPlayerSettings;

// ---- TRACE and the file finder
void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_end(args);
    ev("TRACE", {}, {format});
}

namespace {
// A cursor picture the finder can serve: what the loader fakes give back for it (fixed storage: the heap traffic
// of the unit is counted, so nothing here allocates).
constexpr int kPictureBytes = 16384;
struct Picture {
    bool found = false;
    char name[64];               // the file name the finder is asked for (compared without case)
    char path[160];              // what it answers
    int32_t depth = 0, width = 0, height = 0;
    int32_t headerCode = 0, loadCode = 0;
    bool hasPalette = false;
    unsigned char palette[0x400];
    int32_t pixelBytes = 0;
    unsigned char pixels[kPictureBytes];
};
constexpr int kPictures = 28;
Picture gPictures[kPictures];
int gPictureCount;
Picture* gCurrent;                     // the picture whose path was last answered
unsigned char gPixelStore[1 << 16];    // AllocMemory's memory
uint8_t* gPixelBase;

bool SameName(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const char x = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + 32) : *a;
        const char y = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + 32) : *b;
        if (x != y) return false;
        if (x == 0) return true;
    }
}

// The checksum of a block, as the Python module works it out.
uint32_t Checksum(const unsigned char* data, std::size_t size) {
    uint32_t sum = 0x811c9dc5u;
    for (std::size_t i = 0; i < size; ++i) sum = (sum ^ data[i]) * 0x01000193u;
    return sum;
}
}  // namespace

char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    const char* last = std::strrchr(name, ':');
    const char* wanted = last != nullptr ? last + 1 : name;
    for (int i = 0; i < gPictureCount; ++i) {
        if (gPictures[i].found && SameName(gPictures[i].name, wanted)) {
            gCurrent = &gPictures[i];
            return gPictures[i].path;
        }
    }
    return nullptr;
}

// ---- the picture loaders
TbRenderArea::TbRenderArea() {
    ev("RenderArea.ctor", {newlab(this)});
    mDepth = mHeight = mWidth = mF20 = mStride = 0;
    mData = nullptr;
}
TbResult TbRenderArea::AllocMemory() {
    mStride = mWidth * ((mDepth + 7) >> 3);
    ev("AllocMemory", {lab(this), mStride, mHeight});
    std::memset(gPixelStore, 0, sizeof gPixelStore);
    mData = gPixelStore;
    gPixelBase = gPixelStore;
    return TbResult{0};
}
TbTGALoader::TbTGALoader() {
    ev("TGALoader.ctor", {newlab(this)});
    mHasPalette = 0;
}
TbTGALoader::~TbTGALoader() { ev("TGALoader.dtor", {lab(this)}); }
TbResult TbGraphicFileLoader::ReadHeader(const char* path, const char* second, TbRenderArea* area) {
    auto* loader = static_cast<TbTGALoader*>(this);
    ev("ReadHeader", {lab(loader), lab(area), second == nullptr}, {path});
    Picture& picture = *gCurrent;
    area->mDepth = picture.depth;
    area->mHeight = picture.height;
    area->mWidth = picture.width;
    loader->mHasPalette = picture.hasPalette;
    if (picture.hasPalette) std::memcpy(loader->mPalette, picture.palette, sizeof loader->mPalette);
    return TbResult{picture.headerCode};
}
TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other) {
    ev("Load", {lab(this), lab(&area), second == nullptr, other == nullptr}, {path});
    Picture& picture = *gCurrent;
    std::size_t bytes = static_cast<std::size_t>(area.mStride) * static_cast<std::size_t>(area.mHeight);
    if (bytes > static_cast<std::size_t>(picture.pixelBytes)) bytes = static_cast<std::size_t>(picture.pixelBytes);
    if (area.mData != nullptr) std::memcpy(area.mData, picture.pixels, bytes);
    return TbResult{picture.loadCode};
}

// ---- the platform's pixels and cursors. Each pixel object keeps what the fakes need in a side table.
namespace {
struct PixelsInfo {
    const void* object = nullptr;
    const unsigned char* source = nullptr;   // the 6-argument constructor's data
    unsigned char buffer[0x400];             // the allocating constructor's buffer
};
constexpr int kPixelsInfos = 16;
PixelsInfo gPixelsInfo[kPixelsInfos];
int gPixelsInfoCount;
PixelsInfo& Info(const void* object) {
    for (int i = 0; i < gPixelsInfoCount; ++i)
        if (gPixelsInfo[i].object == object) return gPixelsInfo[i];
    PixelsInfo& added = gPixelsInfo[gPixelsInfoCount++ % kPixelsInfos];
    added.object = object;
    added.source = nullptr;
    std::memset(added.buffer, 0, sizeof added.buffer);
    return added;
}
}  // namespace

UPixels::UPixels() { ev("UPixels.ctor", {newlab(this)}); }
UPixels::UPixels(intptr_t format, int32_t width, int32_t height, UPixel* palette) {
    ev("UPixels.alloc", {newlab(this), format, width, height, palette == nullptr});
    PixelsInfo& info = Info(this);
    info.source = nullptr;
    std::memset(info.buffer, 0, sizeof info.buffer);
}
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    ev("UPixels.wrap", {newlab(this), format, Checksum(static_cast<const unsigned char*>(data), 0x400), width, height, stride,
                        palette == nullptr});
    PixelsInfo& info = Info(this);
    info.source = static_cast<const unsigned char*>(data);
    std::memset(info.buffer, 0, sizeof info.buffer);
}
UPixels::~UPixels() { ev("UPixels.dtor", {lab(this)}); }
void UPixels::Convert(UPixels& source, bool dither) {
    ev("UPixels.Convert", {lab(this), lab(&source), dither});
    PixelsInfo& to = Info(this);
    const PixelsInfo& from = Info(&source);
    for (int i = 0; i < 0x400; ++i) to.buffer[i] = from.source != nullptr ? static_cast<unsigned char>(from.source[i] ^ 0x5a) : 0;
}
void* UPixels::GetData() {
    ev("UPixels.GetData", {lab(this)});
    return Info(this).buffer;
}
void UPixels::SetData(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    const int64_t offset = static_cast<const uint8_t*>(data) - gPixelBase;
    ev("UPixels.SetData", {lab(this), format, offset, width, height, stride, Checksum(static_cast<const unsigned char*>(data), 64),
                           palette == nullptr ? 0 : Checksum(static_cast<const unsigned char*>(static_cast<void*>(palette)), 0x400)});
}
void UPixels::SetTransparentColour(UPixelValue colour) { ev("UPixels.Transparent", {lab(this), colour}); }

UCursorOpenGL::UCursorOpenGL(UPoint& hotSpot, UPixels* frames, int32_t count, UOpenGL* context) {
    ev("UCursorOpenGL", {newlab(this), hotSpot.x, hotSpot.y, lab(frames), count, context == nullptr});
}
void UCursor::SetRate(const UMicroseconds& rate) { ev("SetRate", {lab(this), rate.mValue}); }

// ---- the cursor made from a picture: its virtual members other than the constructor
InterfaceBFCursor::~InterfaceBFCursor() {}
uint8_t InterfaceBFCursor::GetVisible() { return 0; }
void InterfaceBFCursor::Show() {}
void InterfaceBFCursor::Hide() {}
void InterfaceBFCursor::Use(int16_t) {}

// ======================================================================= the world

namespace {
alignas(InterfaceWindow) unsigned char gWindowStore[kSlots][sizeof(InterfaceWindow)];
InterfaceWindow* gWindows[kSlots];
alignas(InterfaceMessageQueue) unsigned char gQueueStore[2][sizeof(InterfaceMessageQueue)];
alignas(InterfaceTimedMessages) unsigned char gTimedStore[sizeof(InterfaceTimedMessages)];
InterfaceMessageQueue* gMsgQueues[2];
InterfaceTimedMessages* gTimed;
alignas(InterfaceBFCursor) unsigned char gBFStore[sizeof(InterfaceBFCursor)];
InterfaceBFCursor* gBF;
const char* gFolderText = "alt:folder";
constexpr float kLightSeed[] = {0.25f, 0.5f, 0.75f, 0.125f, 0.375f, 0.625f, 0.875f};

// A window by slot, or NULL for a slot that is -1 or empty.
InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWindows[slot] : nullptr; }

InterfaceMessageQueue* QueueOf(int64_t k) { return k >= 0 && k < 2 ? gMsgQueues[k] : nullptr; }

// An object's label as the dump writes it: a pointer that is not a known object is -1, NULL is 0.
int64_t L(const void* object) { return lab(object); }

void Put(int64_t*& out, int64_t value) { *out++ = value; }
}  // namespace

extern "C" {

// Resets the world for a case. The globals the unit owns start as the Mac's initialised data and BSS would
// have them (the root window, the queues and the pointer all empty).
void tpw_uiinit_reset() {
    gUIRootWindow = nullptr;
    gUIActive = 1;
    gUIButtonFlags = 0;
    gUIPointerIn = nullptr;
    for (InterfaceWindow*& window : gUIButtonWindow) window = nullptr;
    gUICaptureDevice = nullptr;
    gUIKeyCapture.mKeys = nullptr;
    gUIKeyCapture.mWindow = nullptr;
    gUIDrawQueue = nullptr;
    gUIMessageQueue = nullptr;
    gUITimedMessages = nullptr;
    gUIPeekCallBack = nullptr;
    gUIStarted = 0;
    gUIHebrew = false;
    gUIPointerX = 0;
    gUIPointerY = 0;
    gUICursor = nullptr;
    gUICursorTable = nullptr;
    gUIStandardCursor = 0;
    gUICursorNumber = -1;
    gUIHelpText = nullptr;
    gUIClock = &gUIDefaultClock;
    gUITimers = nullptr;
    gUITimerCount = 0;
    gUITimerList = nullptr;
    gUITimersPaused = 0;
    gUILastWindow = nullptr;
    for (int i = 0; i < kSlots; ++i) gWindows[i] = nullptr;
    gMsgQueues[0] = gMsgQueues[1] = nullptr;
    gTimed = nullptr;
    gBF = nullptr;
    ExpectClear();
    gTimerCtorCount = 0;
    gPictureCount = 0;
    gCurrent = nullptr;
    gPixelsInfoCount = 0;
    gBFCursorFolderSet = 0;
    gBFCursorFolder = nullptr;
    gTbReadHeaderFailed = -1;
    gTbLoadFailed = -1;
    gPlayerSettings.mCurrent.mTooltipsOn = false;
    for (int i = 0; i < kRegions; ++i) gRegions[i].seed = 0;
}

// Names the world's fixed objects for the call log.
void tpw_uiinit_labels() {
    for (int i = 0; i < kSlots; ++i) tpw_cl_label(kWindowLabel + i, gWindowStore[i]);
    tpw_cl_label(kQueueLabel, gQueueStore[0]);
    tpw_cl_label(kQueueLabel + 1, gQueueStore[1]);
    tpw_cl_label(kTimedLabel, gTimedStore);
    for (int i = 0; i < kCursors; ++i) tpw_cl_label(kCursorLabel + i, &gCursors[i]);
    tpw_cl_label(kHelpLabel, &gHelp);
    tpw_cl_label(kClockLabel, &gClocks[0]);
    tpw_cl_label(kClockLabel + 1, &gClocks[1]);
    tpw_cl_label(kDefaultClockLabel, &gUIDefaultClock);
    for (int i = 0; i < kRegions; ++i) tpw_cl_label(kRegionLabel + i, &gRegions[i]);
    for (int i = 0; i < kTables; ++i) tpw_cl_label(kTableLabel + i, &gTables[i]);
    tpw_cl_label(kLinkLabel, static_cast<CMapLinkBase*>(&gUILight));
    tpw_cl_label(kFileLabel, gBFStore);
}

// The cursors and tables of a case: table k has `count` slots and the cursors the mask names (bit i: slot i
// holds cursor i % 6). Made with logging off.
void tpw_uiinit_table(int32_t k, int32_t count, uint32_t mask) {
    new (&gTables[k]) CursorTable;
    gTables[k].Allocate(static_cast<uint16_t>(count));
    for (int i = 0; i < count; ++i)
        if ((mask >> i) & 1) gTables[k].SetEntry(static_cast<uint16_t>(i), &gCursors[i % kCursors]);
}

void tpw_uiinit_region_seed(int32_t k, int32_t seed) { gRegions[k].seed = seed; }
void tpw_uiinit_expect(int32_t hostKind, int32_t count) {
    if (hostKind < 0) ExpectClear();
    // hostKind 0: a message queue, 1: the timed queue, 2: the timer table, 3: an array of UPixels, 4: a cursor table's pointers; `count` of them in a row.
    for (int i = 0; i < count; ++i) {
        if (hostKind == 0) ExpectPush({sizeof(InterfaceMessage), 16, 0});
        else if (hostKind == 1) ExpectPush({sizeof(InterfaceTimedMessage), 0x14, 0});
        else if (hostKind == 2) ExpectPush({sizeof(InterfaceTimer), 0x20, 8});
        else if (hostKind == 3) ExpectPush({sizeof(UPixels), 12, 8});
        else ExpectPush({sizeof(InterfaceCursor*), 4, 0});
    }
}

// One operation of the script; a[] are its arguments. Returns the answer (0 when it has none).
int64_t tpw_uiinit_op(int32_t op, const int64_t* a) {
    switch (op) {
    case 1: {   // a window: slot, parent slot, attributes, id, left, top, right, bottom
        std::memset(gWindowStore[a[0]], 0xa5, sizeof gWindowStore[a[0]]);
        gWindows[a[0]] = new (gWindowStore[a[0]]) InterfaceWindow(
            Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int16_t>(a[4]),
            static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        return L(gWindows[a[0]]);
    }
    case 2: {   // links: slot, parent, previous, next, child
        InterfaceWindow* w = Window(a[0]);
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return 0;
    }
    case 3: {   // fields: slot, attributes, destroying byte, region (0 own rectangle, 1 to 3 a fake region)
        InterfaceWindow* w = Window(a[0]);
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mDestroying = static_cast<uint8_t>(a[2]);
        w->mRegion = a[3] == 0 ? static_cast<CoolRegion*>(&w->mRect) : &gRegions[a[3] - 1];
        return 0;
    }
    case 4: {   // a global: which, value
        const int64_t v = a[1];
        switch (a[0]) {
        case 1: gUIRootWindow = Window(v); break;
        case 2: gUIPointerIn = Window(v); break;
        case 3: gUICaptureDevice = Window(v); break;
        case 4: gUIActive = static_cast<int32_t>(v); break;
        case 5: gUIPointerX = static_cast<int16_t>(v); break;
        case 6: gUIPointerY = static_cast<int16_t>(v); break;
        case 7: gUIDrawQueue = QueueOf(v); break;
        case 8: gUIMessageQueue = QueueOf(v); break;
        case 9: gUITimedMessages = v == 0 ? gTimed : nullptr; break;
        case 10: gUICursor = v >= 0 ? &gCursors[v] : nullptr; break;
        case 11: gUIHelpText = v != 0 ? &gHelp : nullptr; break;
        case 12: gUIButtonFlags = static_cast<int16_t>(v); break;
        case 13: gUICursorNumber = static_cast<int16_t>(v); break;
        case 14: gUIStandardCursor = static_cast<uint16_t>(v); break;
        case 15: gUICursorTable = v >= 0 ? &gTables[v] : nullptr; break;
        case 16: gPlayerSettings.mCurrent.mTooltipsOn = v != 0; break;
        case 17: case 18: case 19: gUIButtonWindow[a[0] - 17] = Window(v); break;
        }
        return 0;
    }
    case 5: UI_InitSystem(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), a[2] != 0); return 0;
    case 6: UI_InitTimerSystem(static_cast<int32_t>(a[0])); return 0;
    case 7: return UI_PostMessage(Window(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
    case 8: UI_FlushAllMessages(); return 0;
    case 9: UI_DevicePointerMove(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1])); return 0;
    case 10: return UI_UpdatePointerIn();
    case 11: return L(Window(a[0])->PointIn(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2])));
    case 12: {   // a rectangle made absolute by a window's parents: slot, left, top, right, bottom -> a checksum of the result
        CoolRect rect(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4]));
        Window(a[0])->MakeRectAbsolute(&rect);
        return (static_cast<int64_t>(static_cast<uint16_t>(rect.mLeft)) << 48) | (static_cast<int64_t>(static_cast<uint16_t>(rect.mTop)) << 32) |
               (static_cast<int64_t>(static_cast<uint16_t>(rect.mRight)) << 16) | static_cast<uint16_t>(rect.mBottom);
    }
    case 13: {   // the queue constructors: 0 and 1 the message queues, 2 the timed queue (over the 0xa5 fill)
        if (a[0] < 2) {
            std::memset(gQueueStore[a[0]], 0xa5, sizeof gQueueStore[a[0]]);
            gMsgQueues[a[0]] = new (gQueueStore[a[0]]) InterfaceMessageQueue;
        } else {
            std::memset(gTimedStore, 0xa5, sizeof gTimedStore);
            gTimed = new (gTimedStore) InterfaceTimedMessages;
        }
        return 0;
    }
    case 14: return a[0] < 2 ? gMsgQueues[a[0]]->Initialise(static_cast<int32_t>(a[1])) : gTimed->Initialise(static_cast<int32_t>(a[1]));
    case 15: return gMsgQueues[a[0]]->Add(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
    case 16:
        if (a[0] < 2) gMsgQueues[a[0]]->Flush();
        else gTimed->Flush();
        return 0;
    case 17: UI_UseCursorTable(a[0] >= 0 ? &gTables[a[0]] : nullptr); return 0;
    case 18: UI_SetStandardCursor(static_cast<uint16_t>(a[0])); return 0;
    case 19: UI_SetCursor(static_cast<int16_t>(a[0])); return 0;
    case 20: UI_HideCursor(); return 0;
    case 21: UI_ShowCursor(); return 0;
    case 22: UI_SetPeekCallBack(a[0] == 0 ? nullptr : a[0] == 1 ? PeekA : a[0] == 2 ? PeekB : OtherCallback); return 0;
    case 23: UI_SetClockTickFunction(a[0] >= 0 ? &gClocks[a[0]] : nullptr); return 0;
    case 24: UI_EnableHelpText(); return 0;
    case 25: UI_DisableHelpText(); return 0;
    case 26: InitLobbyUI(); return 0;
    case 27: gUIInterface.InitLobby(); return 0;
    case 30: UI_DeviceButtonDown(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2])); return gUIActive != 0;
    case 31: UI_DeviceButtonUp(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2])); return gUIActive != 0;
    case 35: UI_DeviceMouseWheel(a[0], static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3])); return gUIActive != 0;
    case 32: return UI_Keydown(static_cast<int16_t>(a[0]), static_cast<int32_t>(a[1]));   // unit w50: key, flags (the answer is the function's)
    case 33: return UI_Keyup(static_cast<int16_t>(a[0]), static_cast<int32_t>(a[1]));
    case 34: return UI_KeyChar(static_cast<int16_t>(a[0]), static_cast<int32_t>(a[1]));
    case 29: {   // a window's depth and pivot: slot, z, pivot x, pivot y
        InterfaceWindow* w = Window(a[0]);
        w->mZ = static_cast<int16_t>(a[1]);
        w->mPivotX = static_cast<int16_t>(a[2]);
        w->mPivotY = static_cast<int16_t>(a[3]);
        return 0;
    }
    case 28: {   // a window's callbacks: slot, which (0 callback, 1 default), code (0 NULL, 1 default, 2 other)
        InterfaceWindow* w = Window(a[0]);
        InterfaceWindowCallback value = a[2] == 0 ? nullptr : a[2] == 1 ? UI_DefaultWindowCallback : OtherCallback;
        (a[1] == 0 ? w->mCallback : w->mDefaultCallback) = value;
        return 0;
    }
    }
    return -1;
}

// ---- dumps. The orders are the Python module's tables.

void tpw_uiinit_dump_globals(int64_t* out) {
    Put(out, L(gUIRootWindow));
    Put(out, L(gUIPointerIn));
    Put(out, L(gUICaptureDevice));
    Put(out, L(gUIKeyCapture.mKeys));
    Put(out, L(gUIKeyCapture.mWindow));
    Put(out, L(gUIDrawQueue));
    Put(out, L(gUIMessageQueue));
    Put(out, L(gUITimedMessages));
    Put(out, gUIPeekCallBack == nullptr ? 0 : gUIPeekCallBack == PeekA ? 1 : gUIPeekCallBack == PeekB ? 2 : 3);
    Put(out, gUIStarted);
    Put(out, gUIHebrew);
    Put(out, gUIPointerX);
    Put(out, gUIPointerY);
    Put(out, L(gUICursor));
    Put(out, L(gUICursorTable));
    Put(out, gUIStandardCursor);
    Put(out, gUICursorNumber);
    Put(out, L(gUIHelpText));
    Put(out, L(gUIClock));
    Put(out, gUITimers != nullptr);
    Put(out, gUITimerCount);
    Put(out, L(gUITimerList));
    Put(out, gUITimersPaused);
    Put(out, L(gUILastWindow));
    Put(out, gUIButtonFlags);
    Put(out, gUIActive);
    for (InterfaceWindow* window : gUIButtonWindow) Put(out, L(window));
}

void tpw_uiinit_dump_window(int32_t slot, int64_t* out) {
    const InterfaceWindow* w = slot < 0 ? gUIRootWindow : Window(slot);
    Put(out, 1);
    Put(out, w->mDestroying);
    for (const CoolRect* rect : {&w->mRect, &w->mOriginalRect, &w->mPreviousRect, &w->mClientRect, &w->mRect34}) {
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    }
    Put(out, w->mRegion == nullptr ? 0 : w->mRegion == &w->mRect ? 1 : L(w->mRegion));
    Put(out, static_cast<int32_t>(w->mAttributes));
    Put(out, L(w->mFont));
    Put(out, w->mCursor);
    Put(out, L(w->mNext));
    Put(out, L(w->mPrevious));
    Put(out, L(w->mChild));
    Put(out, L(w->mParent));
    Put(out, static_cast<int32_t>(w->mF60));
    Put(out, static_cast<int32_t>(w->mF64));
    Put(out, Untouched(w->mF68, sizeof w->mF68));
    Put(out, w->mId);
    Put(out, w->mHelpText);
    Put(out, w->mF90);
    Put(out, L(w->mDrawObject));
    Put(out, L(w->mClientDrawObject));
    Put(out, Zero(w->mExtraParams, sizeof w->mExtraParams));
    Put(out, w->mZ);
    Put(out, w->mZRot);
    Put(out, w->mPivotX);
    Put(out, w->mPivotY);
    Put(out, L(w->mAnim));
    Put(out, w->mAnimFrame);
    Put(out, w->mAnimTicks);
    Put(out, w->mAnimStart);
    for (int i = 0; i < 4; ++i) Put(out, w->mAnimOffset[i]);
    Put(out, L(w->mSlideTimer));
    Put(out, Untouched(w->mSlideStep, sizeof w->mSlideStep) && Untouched(&w->mSlideSteps, sizeof w->mSlideSteps) &&
                 Untouched(w->mF10A, sizeof w->mF10A));
    Put(out, w->mCallback == nullptr ? 0 : w->mCallback == UI_DefaultWindowCallback ? 1 : 2);
    Put(out, w->mDefaultCallback == nullptr ? 0 : w->mDefaultCallback == UI_DefaultWindowCallback ? 1 : 2);
    Put(out, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) Put(out, L(w->mAnimTimers[i]));
    Put(out, static_cast<int32_t>(w->mF130));
    Put(out, w->mF134);
    Put(out, w->mAnimating);
}

// A queue: its words, then `limit` entries.
void tpw_uiinit_dump_queue(int32_t which, int32_t limit, int64_t* out) {
    const InterfaceMessageQueue* q = which == 0 ? gUIMessageQueue : which == 1 ? gUIDrawQueue : QueueOf(which - 2);
    if (q == nullptr) {
        Put(out, 0);
        return;
    }
    Put(out, L(q));
    Put(out, q->mEntries != nullptr);
    Put(out, q->mCount);
    Put(out, q->mHead);
    Put(out, q->mTail);
    Put(out, q->mQueued);
    if (q->mEntries == nullptr) return;
    for (int32_t i = 0; i < q->mCount && i < limit; ++i) {
        Put(out, q->mEntries[i].mWindow == reinterpret_cast<InterfaceWindow*>(1) ? -2 : L(q->mEntries[i].mWindow));   // 1 is the key capture's pseudo-window
        Put(out, q->mEntries[i].mMessage);
        Put(out, q->mEntries[i].mA);
        Put(out, q->mEntries[i].mB);
    }
    // everything past the limit: zero or not
    int64_t rest = 1;
    for (int32_t i = limit; i < q->mCount; ++i)
        if (q->mEntries[i].mWindow != nullptr || q->mEntries[i].mMessage != 0 || q->mEntries[i].mA != 0 || q->mEntries[i].mB != 0) rest = 0;
    Put(out, rest);
}

void tpw_uiinit_dump_timed(int32_t which, int32_t limit, int64_t* out) {
    const InterfaceTimedMessages* t = which == 0 ? gUITimedMessages : gTimed;
    if (t == nullptr) {
        Put(out, 0);
        return;
    }
    Put(out, L(t));
    Put(out, t->mEntries != nullptr);
    Put(out, t->mCount);
    if (t->mEntries == nullptr) return;
    for (int32_t i = 0; i < t->mCount && i < limit; ++i) {
        Put(out, L(t->mEntries[i].mWindow));
        Put(out, t->mEntries[i].mMessage);
        Put(out, t->mEntries[i].mA);
        Put(out, t->mEntries[i].mB);
        Put(out, static_cast<int32_t>(t->mEntries[i].mTime));
    }
}

// The timer table: the number each element's constructor left in it.
void tpw_uiinit_dump_timers(int32_t limit, int64_t* out) {
    for (int32_t i = 0; i < gUITimerCount && i < limit; ++i) Put(out, static_cast<int32_t>(gUITimers[i].mStart));
}

void tpw_uiinit_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}
void tpw_uiinit_heap_clear() {
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
}

// ---- the cursor pictures

// Adds a picture the finder knows: its name, the path it answers, the header's numbers and the bytes the loader serves.
void tpw_uiinit_picture(const char* name, const char* path, int32_t depth, int32_t width, int32_t height, int32_t headerCode,
                        int32_t loadCode, int32_t hasPalette, const unsigned char* palette, const unsigned char* pixels,
                        int32_t pixelBytes) {
    Picture& p = gPictures[gPictureCount++];
    p.found = true;
    std::strncpy(p.name, name, sizeof p.name - 1);
    p.name[sizeof p.name - 1] = 0;
    std::strncpy(p.path, path, sizeof p.path - 1);
    p.path[sizeof p.path - 1] = 0;
    p.depth = depth;
    p.width = width;
    p.height = height;
    p.headerCode = headerCode;
    p.loadCode = loadCode;
    p.hasPalette = hasPalette != 0;
    if (palette != nullptr) std::memcpy(p.palette, palette, sizeof p.palette);
    p.pixelBytes = pixelBytes < kPictureBytes ? pixelBytes : kPictureBytes;
    std::memcpy(p.pixels, pixels, static_cast<std::size_t>(p.pixelBytes));
}

// 1: the cursor constructor (over the 0xa5 fill); 2: LoadData(the path of picture a[0]); 3: LoadBFCurs(name);
// 4: InitialiseCOLOURCursors; 5: set the constants and the folder; 6: a table for gCursorTable.
int64_t tpw_uiinit_cursor_op(int32_t op, const char* text, const int64_t* a) {
    switch (op) {
    case 1:
        std::memset(gBFStore, 0xa5, sizeof gBFStore);
        gBF = new (gBFStore) InterfaceBFCursor;
        return 0;
    case 2:   // a[0]: the picture, by its number
        gCurrent = &gPictures[a[0] % gPictureCount];
        return gBF->LoadData(gCurrent->path);
    case 3: return L(LoadBFCurs(text));
    case 4: InitialiseCOLOURCursors(); return 0;
    case 5:
        gTbReadHeaderFailed = static_cast<int32_t>(a[0]);
        gTbLoadFailed = static_cast<int32_t>(a[1]);
        gBFCursorFolderSet = static_cast<char>(a[2]);
        gBFCursorFolder = a[3] != 0 ? gFolderText : nullptr;
        return 0;
    case 6:
        new (&gCursorTable) CursorTable;
        gCursorTable.Allocate(static_cast<uint16_t>(a[0]));
        return 0;
    }
    return -1;
}

void tpw_uiinit_dump_bfcursor(int64_t* out) {
    if (gBF == nullptr) {
        Put(out, 0);
        return;
    }
    Put(out, L(gBF));
    Put(out, gBF->mVisible);
    Put(out, gBF->mHotX);
    Put(out, gBF->mHotY);
    Put(out, Untouched(gBF->mF03, sizeof gBF->mF03));
    Put(out, static_cast<int32_t>(gBF->mF0C));
    Put(out, L(gBF->mCursor));
}

// LoadBFCurs's folder: the flag, and which string the pointer is (0 NULL, 1 the game's, 2 the test's, 3 other).
void tpw_uiinit_dump_folder(int64_t* out) {
    Put(out, gBFCursorFolderSet);
    Put(out, gBFCursorFolder == nullptr ? 0 : gBFCursorFolder == gFolderText ? 2 : std::strcmp(gBFCursorFolder, "data:UI:cursors") == 0 ? 1 : 3);
}

void tpw_uiinit_dump_table(int64_t* out) {
    Put(out, gCursorTable.mCount);
    for (int i = 0; i < gCursorTable.mCount; ++i) {
        const InterfaceCursor* c = gCursorTable.mEntries[i];
        Put(out, L(c));
        if (c != nullptr) {
            Put(out, c->mHotX);
            Put(out, c->mHotY);
            Put(out, L(static_cast<const InterfaceBFCursor*>(c)->mCursor));
        }
    }
}

// The interface's light: seeded, then read after InitLobbyUI. mColorWord, the four colours, the normal.
void tpw_uiinit_light_seed() {
    gUILight.mColorWord = 0x12345678u;
    for (int i = 0; i < 4; ++i) gUILight.mColor[i] = kLightSeed[i];
    gUILight.mNormal.x = kLightSeed[4];
    gUILight.mNormal.y = kLightSeed[5];
    gUILight.mNormal.z = kLightSeed[6];
}
void tpw_uiinit_dump_light(uint32_t* out) {
    *out++ = gUILight.mColorWord;
    for (int i = 0; i < 4; ++i) *out++ = fbits(gUILight.mColor[i]);
    *out++ = fbits(gUILight.mNormal.x);
    *out++ = fbits(gUILight.mNormal.y);
    *out++ = fbits(gUILight.mNormal.z);
}

}  // extern "C"
