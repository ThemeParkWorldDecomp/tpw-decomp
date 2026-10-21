// Test library for unit w27 (libtpw_unit_CursorClasses.dylib; see tests/test_CursorClasses.py and calllog.h):
//
//   src/game/ui/InterfaceBFCursor.cpp  InterfaceBFCursor's destructor, GetVisible, Show, Hide and Use
//   src/game/ui/InterfaceCursor.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceDrawObject.cpp      InterfaceCursor and CursorTable (unit w21's translation, tested here again)
//   src/game/ui/InterfaceBFCursor.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_main.cpp         InterfaceBFCursor::LoadData (the real cursor loader on top of the picture loader)
//   src/bullfrog/AndyGLL.cpp, src/bullfrog/LbDrawText.cpp, src/bullfrog/Lbgfsave.cpp, src/bullfrog/TbRenderArea.cpp    TbRenderArea, TbGraphicFileLoader, TbTGALoader, _TgaGLL, _AndyGLL, LbPalette_FromCompact
//
// Everything they call that is not part of the unit is a fake that records through calllog: the file layer (one byte
// stream the test sets, served by every file object), the library's heap (blocks filled with 0xa5 and a margin),
// the rectangle helpers, the surface, the pixel converter, the platform's cursor and pixel classes. The Python
// module drives the library through tpw_cc_op (a script of operations on a small world: one loader, areas, a line
// reader, two pixel blocks, files, cursors and tables) and compares the state dumps tpw_cc_dump_* writes with the
// PowerPC side's, in the order the Python module's tables list.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "LbFile.h"
#include "Light.h"
#include "MapWho.h"
#include "TPConfig.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "calllog.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;
using namespace SamsUtilities;
using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;
using bfl::FileSystem::CWadFile;
using bfl::FileSystem::_TbBufferedFile;

namespace {

// ---- the fixed labels of the world (the Python module's LAB_ table)
constexpr int kLoaderLabel = 1, kAreaLabel = 2, kSurfaceLabel = 4, kLoaderAreaLabel = 5, kGllLabel = 6, kFileLabel = 7,
              kBlockLabel = 8, kPaletteLabel = 10, kLoaderPaletteLabel = 11, kUCursorLabel = 20, kCursorLabel = 30,
              kTableLabel = 50;
constexpr int kAreas = 3;            // A, B and S (the surface's); index 3 is the loader's own area
constexpr int kBlocks = 2;           // D1, D2
constexpr int kBlockSize = 0x4000;
constexpr int kMargin = 0x400;       // room past a LbMem block, for the runs that write past a line
constexpr int kCursorSlots = 6, kUCursors = 3, kTables = 2;

// ---- the world's storage
alignas(16) unsigned char gLoaderStore[0x600];
alignas(16) unsigned char gAreaStore[kAreas][sizeof(TbRenderArea)];
alignas(16) unsigned char gGllStore[0x100];
alignas(16) unsigned char gBlockStore[kBlocks][kBlockSize + kMargin];
alignas(16) unsigned char gPaletteStore[0x400];
TbTGALoader* gLoader;     // the loader, constructed in gLoaderStore (NULL until the script makes it)
_TgaGLL* gGll;
TbSurface gSurface;

TbRenderArea* AreaAt(int64_t index) {
    if (index == 3) return &static_cast<TbTGALoader*>(static_cast<void*>(gLoaderStore))->mArea;
    return index >= 0 && index < kAreas ? reinterpret_cast<TbRenderArea*>(gAreaStore[index]) : nullptr;
}
unsigned char* BlockAt(int64_t index) { return index >= 1 && index <= kBlocks ? gBlockStore[index - 1] : nullptr; }

// ---- the registry of blocks a pointer can be described by: the world's and the heap fake's
struct Block {
    const unsigned char* start;
    std::size_t extent;   // the block and the margin after it a run may write into
    int32_t label;
    bool freed;           // a heap block the library has let go of: it stays, so that a pointer left in the world still describes
};
constexpr int kBlockMax = 64;
Block gRegistry[kBlockMax];
int gRegistryCount;

void Register(const void* start, std::size_t extent, int32_t label) {
    if (gRegistryCount < kBlockMax) gRegistry[gRegistryCount++] = {static_cast<const unsigned char*>(start), extent, label, false};
}
// Marks a live heap block (a label of 100 or more) as let go of; false for anything else. The memory is kept until the next
// case (a block freed and made again would take the label of a pointer left behind).
bool Release(const void* start) {
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].start == start && gRegistry[i].label >= 100 && !gRegistry[i].freed) {
            gRegistry[i].freed = true;
            return true;
        }
    }
    return false;
}

// A pointer as the dumps write it, two numbers: the label of the block it lies in and the offset, (0, 0) for NULL, (-2, 0)
// for the untouched fill, (-1, raw) for anything else. A pointer a little before one of the world's blocks (a bottom-up
// copy of an empty picture starts a row before its area) counts as in that block, with a negative offset.
void Describe(const void* pointer, int64_t* out) {
    const auto* p = static_cast<const unsigned char*>(pointer);
    if (p == nullptr) {
        out[0] = out[1] = 0;
        return;
    }
    for (int i = 0; i < gRegistryCount; ++i) {
        if (p >= gRegistry[i].start && p <= gRegistry[i].start + gRegistry[i].extent) {
            out[0] = gRegistry[i].label;
            out[1] = p - gRegistry[i].start;
            return;
        }
    }
    if (reinterpret_cast<uintptr_t>(p) == 0xa5a5a5a5a5a5a5a5ull) {
        out[0] = -2;
        out[1] = 0;
        return;
    }
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label < 100 && p < gRegistry[i].start && gRegistry[i].start - p <= 0x1000) {
            out[0] = gRegistry[i].label;
            out[1] = p - gRegistry[i].start;
            return;
        }
    }
    out[0] = -1;
    out[1] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu);
}

uint32_t Checksum(const unsigned char* data, std::size_t size) {
    uint32_t sum = 0x811c9dc5u;
    for (std::size_t i = 0; i < size; ++i) sum = (sum ^ data[i]) * 0x01000193u;
    return sum;
}

// ---- the file stream every fake file serves
constexpr int kStreamMax = 1 << 16;
unsigned char gStream[kStreamMax];
int64_t gStreamSize;
int64_t gStreamPos;

// The access object behind each file: Read is inline in CFile and goes through it. One per file object, taken from a
// pool; it knows its file for the log.
class FakeAccess : public CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("CFile.Read", {lab(mOwner), size});
        int64_t remaining = gStreamSize - gStreamPos;
        if (remaining < 0) remaining = 0;
        const int64_t count = size < remaining ? size : remaining;
        if (dest != nullptr && count > 0) std::memcpy(dest, gStream + gStreamPos, static_cast<std::size_t>(count));
        gStreamPos += count;
        return static_cast<int32_t>(ret("CFile.Read", count));
    }
    const CFile* mOwner = nullptr;
};
constexpr int kAccessPool = 16;
FakeAccess gAccess[kAccessPool];
int gAccessUsed;

CWadFile gFile0;   // the file the script hands to the functions that take one

}  // namespace

// ---- the file layer
namespace bfl {
namespace FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(-1); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

CFile::CFile() : mImplementation(nullptr) {
    ev("CFile.ctor", {newlab(this)});
    if (gAccessUsed < kAccessPool) {
        gAccess[gAccessUsed].mOwner = this;
        mImplementation = &gAccess[gAccessUsed++];
    }
}
CFile::~CFile() { ev("CFile.dtor", {lab(this)}); }
void CFile::Close() {}
// Positions within the stream; a position outside it fails and leaves the old one.
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    const int32_t signedOffset = static_cast<int32_t>(offset);
    ev("CFile.Seek", {lab(this), signedOffset, whence});
    int64_t target;
    if (whence == 0) target = signedOffset;
    else if (whence == 1) target = gStreamPos + signedOffset;
    else if (whence == 2) target = gStreamSize + signedOffset;
    else target = -1;
    int64_t answer = -1;
    if (target >= 0 && target <= gStreamSize) {
        gStreamPos = target;
        answer = target;
    }
    return static_cast<int32_t>(ret("CFile.Seek", answer));
}
int32_t CFile::GetSize() {
    ev("CFile.GetSize", {lab(this)});
    return static_cast<int32_t>(ret("CFile.GetSize", gStreamSize));
}
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(-1); }
int32_t CFile::GetMacFileReference() { return -1; }

CWadFile::~CWadFile() {}
// A file that opens is at its start.
TbError CWadFile::Open(const char* name, const char* storagePath) {
    ev("CWadFile.Open", {lab(this)}, {name, storagePath});
    const int32_t answer = static_cast<int32_t>(ret("CWadFile.Open", 0));
    if (answer >= 0) gStreamPos = 0;
    return TbError(answer);
}

TbError _TbBufferedFile::Open(CFile& file, uint32_t size) {
    ev("Buffered.Open", {lab(this), lab(&file), size});
    return TbError(static_cast<int32_t>(ret("Buffered.Open", 0)));
}

}  // namespace FileSystem
}  // namespace bfl

// ---- the library's heap: blocks filled with 0xa5, a margin after each (the run-length reader can write past a line)
void* LbMem_Alloc(uint32_t size) {
    if (ret("LbMem_Alloc", 1) == 0) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    auto* block = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(size) + kMargin + 1));
    std::memset(block, 0xa5, static_cast<std::size_t>(size) + kMargin);
    const int32_t label = newlab(block);
    Register(block, static_cast<std::size_t>(size) + kMargin, label);
    ev("LbMem_Alloc", {size, label});
    return block;
}
// (A pointer that is not one of the fake heap's blocks is only logged: the script may free the world's pixel blocks.)
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    if (Release(block)) forget(block);
}

// ---- the rectangle helpers (the library's, from their listings), the surface and the pixel converter
uint8_t LbRect_Intersection(TbRect* rect, const TbRect* other) {
    ev("LbRect_Intersection", {rect->left, rect->top, rect->right, rect->bottom, other->left, other->top, other->right,
                               other->bottom});
    rect->left = other->left < rect->left ? rect->left : other->left;
    rect->top = other->top < rect->top ? rect->top : other->top;
    rect->right = rect->right < other->right ? rect->right : other->right;
    rect->bottom = rect->bottom < other->bottom ? rect->bottom : other->bottom;
    // (the answer can be set by the test: the clipped rectangle then keeps what the real function made of it)
    return static_cast<uint8_t>(ret("LbRect_Intersection", rect->left < rect->right && rect->top < rect->bottom ? 1 : 0));
}
void LbRect_Empty(TbRect* rect) {
    ev("LbRect_Empty", {rect->left, rect->top, rect->right, rect->bottom});
    rect->right = rect->left;
    rect->bottom = rect->top;
}
const TbRenderArea& TbSurface::GetRenderArea() const {
    ev("TbSurface.GetRenderArea", {lab(this)});
    return *AreaAt(2);
}

CConverter::CConverter() { ev("CConverter.ctor", {newlab(this)}); }
void CConverter::Init(const TbPixFormat* destination, const TbPixFormat* source, const TbPalette* palette, uint32_t flags) {
    ev("CConverter.Init", {lab(this), lab(destination), lab(source), lab(palette), flags});
}
// Every byte of the line is changed so that the converted pixels show in the picture: exclusive-or with 0x5a.
void CConverter::ConvBuf(uint8_t* source, uint8_t* dest, int32_t count) {
    ev("CConverter.ConvBuf", {lab(this), lab(source), lab(dest), count});
    for (int32_t i = 0; i < count; ++i) dest[i] = static_cast<uint8_t>(source[i] ^ 0x5a);
}
void CConverter::Release() { ev("CConverter.Release", {lab(this)}); }

// ---- the platform's cursor: the library's nest count (it starts at 1) and recording members
namespace {
int32_t gShowCount = 1;
}
namespace SamsUtilities {
void UCursor::Set() { ev("UCursor.Set", {lab(this)}); }
void UCursor::Reset() { ev("UCursor.Reset", {lab(this)}); }
int32_t UCursor::Show() {
    ev("UCursor.Show");
    ++gShowCount;
    return gShowCount;
}
int32_t UCursor::Hide() {
    ev("UCursor.Hide");
    --gShowCount;
    return gShowCount;
}
uint8_t UCursor::IsVisible() {
    ev("UCursor.IsVisible");
    return gShowCount > 0 ? 1 : 0;
}
void UCursor::SetRate(const UMicroseconds& rate) { ev("SetRate", {lab(this), rate.mValue}); }
UCursorOpenGL::UCursorOpenGL(UPoint& hotSpot, UPixels* frames, int32_t count, UOpenGL* context) {
    ev("UCursorOpenGL", {newlab(this), hotSpot.x, hotSpot.y, lab(frames), count, context == nullptr});
}
UCursorOpenGL::~UCursorOpenGL() { ev("UCursor.dtor", {lab(this)}); }
void UCursorOpenGL::SetHotspot(UPoint& hotSpot) { ev("UCursor.SetHotspot", {lab(this), hotSpot.x, hotSpot.y}); }
}  // namespace SamsUtilities

// ---- what the cursor loader (LoadData) reaches: the finder, the pixel classes, the trace
namespace {
struct PixelsInfo {
    const void* object = nullptr;
    const unsigned char* source = nullptr;   // the wrapping constructor's data
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
const unsigned char* gPixelBase;   // the picture's pixels: the frames' data are logged relative to it
char gFoundPath[160];
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
    if (gPixelBase == nullptr) gPixelBase = static_cast<const unsigned char*>(data);   // the first frame starts the picture
    const int64_t offset = static_cast<const unsigned char*>(data) - gPixelBase;
    ev("UPixels.SetData", {lab(this), format, offset, width, height, stride, Checksum(static_cast<const unsigned char*>(data), 64),
                           palette == nullptr ? 0 : Checksum(static_cast<const unsigned char*>(static_cast<void*>(palette)), 0x400)});
}
void UPixels::SetTransparentColour(UPixelValue colour) { ev("UPixels.Transparent", {lab(this), colour}); }

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    return gFoundPath[0] != 0 ? gFoundPath : nullptr;
}

// ---- what the unit's other sources link against
CursorTable gCursorTable;
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
CSystem::CSystem() {}
CSystem gei_System;
void CSystem::UpdateLights() {}
CMapLinkBase::~CMapLinkBase() {}
CLight* gev_LightChain;
namespace {
int32_t gMapStorage;
}
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(&gMapStorage);
int32_t CMapWho::Link(CMapLinkBase*, int32_t) { return 1; }
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CPlayerSettings gPlayerSettings;
void UI_FlushAllMessages() {}
void UI_EnableHelpText() {}
void UI_DisableHelpText() {}

// ======================================================================= the world

namespace {

// The paths and storage names the script uses, set by the Python module (the same strings go to the PowerPC side).
constexpr int kTexts = 8;
char gTexts[kTexts][80];

const char* Text(int64_t index) { return index >= 0 && index < kTexts ? gTexts[index] : nullptr; }

// The cursors: a slot holds a base cursor or a bullfrog cursor made on the heap, and a table keeps pointers to them.
struct CursorSlot {
    InterfaceCursor* cursor;
    int kind;   // 0 none, 1 base class, 2 InterfaceBFCursor
};
CursorSlot gCursors[kCursorSlots];
UCursorOpenGL* gUCursors[kUCursors];
CursorTable* gTables[kTables];   // made on the heap (the script can delete one)

CursorTable* TableAt(int64_t index) { return index >= 0 && index < kTables ? gTables[index] : nullptr; }

void Put(int64_t*& out, int64_t value) { *out++ = value; }
void PutPointer(int64_t*& out, const void* pointer) {
    Describe(pointer, out);
    out += 2;
}

// An area as 13 numbers: the four masks, depth, palettised flag, pixels (2), width, height, stride.
void PutArea(int64_t*& out, const TbRenderArea& area) {
    for (int i = 0; i < 4; ++i) {
        uint32_t mask;
        std::memcpy(&mask, area.mF00 + 4 * i, 4);
        Put(out, mask);
    }
    Put(out, static_cast<uint32_t>(area.mDepth));
    Put(out, static_cast<uint32_t>(area.mPalettised));
    PutPointer(out, area.mData);
    Put(out, static_cast<uint32_t>(area.mWidth));
    Put(out, static_cast<uint32_t>(area.mPictureHeight));
    Put(out, static_cast<uint32_t>(area.mStride));
}

void PutHeader(int64_t*& out, const TgaHeader& header) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&header);
    for (int i = 0; i < 18; ++i) Put(out, bytes[i]);
}

}  // namespace

extern "C" {

// Starts a case: empties the world, fills the blocks and storage, takes the stream the test serves and labels the objects.
void tpw_cc_begin(const unsigned char* stream, int32_t streamSize) {
    gLoader = nullptr;
    gGll = nullptr;
    for (int i = 0; i < gRegistryCount; ++i)
        if (gRegistry[i].label >= 100) std::free(const_cast<unsigned char*>(gRegistry[i].start));
    gRegistryCount = 0;
    gAccessUsed = 0;
    gShowCount = 1;
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = 0;
    gPixelsInfoCount = 0;
    gPixelBase = nullptr;
    gFoundPath[0] = 0;
    for (int i = 0; i < kCursorSlots; ++i) gCursors[i] = {nullptr, 0};
    for (int i = 0; i < kUCursors; ++i) gUCursors[i] = nullptr;
    for (int i = 0; i < kTables; ++i) gTables[i] = nullptr;
    std::memset(gLoaderStore, 0xa5, sizeof gLoaderStore);
    std::memset(gAreaStore, 0xa5, sizeof gAreaStore);
    std::memset(gGllStore, 0xa5, sizeof gGllStore);
    std::memset(gBlockStore, 0xa5, sizeof gBlockStore);
    std::memset(gPaletteStore, 0xa5, sizeof gPaletteStore);
    gStreamSize = streamSize < kStreamMax ? streamSize : kStreamMax;
    if (gStreamSize > 0) std::memcpy(gStream, stream, static_cast<std::size_t>(gStreamSize));
    gStreamPos = 0;
    // The supplied file takes the first access object of the pool again.
    gAccess[0].mOwner = &gFile0;
    gAccessUsed = 1;
    for (int i = 0; i < kBlocks; ++i) Register(gBlockStore[i], kBlockSize + kMargin, kBlockLabel + i);
    Register(gPaletteStore, sizeof gPaletteStore, kPaletteLabel);
    for (int i = 0; i < kAreas; ++i) Register(gAreaStore[i], sizeof(TbRenderArea), kAreaLabel + (i == 2 ? 2 : i));
    Register(gLoaderStore, sizeof gLoaderStore, kLoaderLabel);
    Register(gGllStore, sizeof gGllStore, kGllLabel);
}

// The labels, once the call log has been reset (tpw_cl_reset clears them).
void tpw_cc_labels() {
    // (The loader itself has no label: on the host its first member is its area, at the same address.)
    tpw_cl_label(kAreaLabel, gAreaStore[0]);
    tpw_cl_label(kAreaLabel + 1, gAreaStore[1]);
    tpw_cl_label(kSurfaceLabel, &gSurface);
    tpw_cl_label(kLoaderAreaLabel, &reinterpret_cast<TbTGALoader*>(gLoaderStore)->mArea);
    tpw_cl_label(kLoaderPaletteLabel, reinterpret_cast<TbTGALoader*>(gLoaderStore)->mPalette);
    tpw_cl_label(kGllLabel, gGllStore);
    tpw_cl_label(kFileLabel, &gFile0);
    for (int i = 0; i < kBlocks; ++i) tpw_cl_label(kBlockLabel + i, gBlockStore[i]);
    tpw_cl_label(kPaletteLabel, gPaletteStore);
}

void tpw_cc_set_text(int32_t index, const char* text) {
    if (index >= 0 && index < kTexts) std::snprintf(gTexts[index], sizeof gTexts[index], "%s", text);
}
void tpw_cc_set_found_path(const char* text) { std::snprintf(gFoundPath, sizeof gFoundPath, "%s", text); }
void tpw_cc_set_show_count(int32_t count) { gShowCount = count; }
void tpw_cc_set_pixel_base(int32_t area) {
    const TbRenderArea* a = AreaAt(area);
    gPixelBase = a != nullptr ? a->mData : nullptr;
}

void tpw_cc_fill_block(int32_t index, const unsigned char* bytes, int32_t count) {
    unsigned char* block = BlockAt(index);
    if (block != nullptr && count > 0) std::memcpy(block, bytes, static_cast<std::size_t>(count < kBlockSize ? count : kBlockSize));
}
int32_t tpw_cc_block_bytes(int32_t index, unsigned char* out, int32_t count) {
    const unsigned char* block = BlockAt(index);
    if (block == nullptr) return 0;
    std::memcpy(out, block, static_cast<std::size_t>(count < kBlockSize + kMargin ? count : kBlockSize + kMargin));
    return 1;
}
// The bytes of a live heap block (by its label), for a failure message.
int32_t tpw_cc_heap_bytes(int32_t label, unsigned char* out, int32_t count) {
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label == label) {
            std::memcpy(out, gRegistry[i].start, std::min<std::size_t>(count, gRegistry[i].extent));
            return static_cast<int32_t>(gRegistry[i].extent - kMargin);
        }
    }
    return -1;
}

// One operation of the script on the loader world (codes 1 to 49) or the cursor world (100 and up); a[] are its
// arguments. Returns the answer (0 when it has none).
int64_t tpw_cc_op(int32_t op, const int64_t* a) {
    switch (op) {
    // ---- TbRenderArea: a[0] the area
    case 1:   // the default constructor
        new (AreaAt(a[0])) TbRenderArea;
        return 0;
    case 2:   // the surface constructor (the surface's area is S)
        new (AreaAt(a[0])) TbRenderArea(&gSurface);
        return 0;
    case 3: {   // the constructor from a size, a format, pixels and a stride: width, height, six format words, block, offset, stride
        const TbSize size = {static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])};
        const TbPixFormat format(static_cast<uint32_t>(a[3]), static_cast<uint32_t>(a[4]), static_cast<uint32_t>(a[5]),
                                 static_cast<uint32_t>(a[6]), static_cast<int32_t>(a[7]), static_cast<int32_t>(a[8]));
        unsigned char* data = BlockAt(a[9]);
        if (data != nullptr) data += a[10];
        new (AreaAt(a[0])) TbRenderArea(size, format, data, static_cast<uint32_t>(a[11]));
        return 0;
    }
    case 4: return AreaAt(a[0])->AllocMemory().mCode;
    case 5: AreaAt(a[0])->FreeMemory(); return 0;
    case 6: return AreaAt(a[0])->GetMemSize();
    case 7: {   // SetupClientRA(point, size): area, client, x, y, width, height
        const TbPoint point = {static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])};
        const TbSize size = {static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5])};
        return AreaAt(a[0])->SetupClientRA(*AreaAt(a[1]), point, size).mCode;
    }
    case 8: {   // SetupClientRA(rect): area, client, left, top, right, bottom
        const TbRect rect = {static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]), static_cast<int32_t>(a[5])};
        return AreaAt(a[0])->SetupClientRA(*AreaAt(a[1]), rect).mCode;
    }
    // ---- the loaders
    case 10:   // TbGraphicFileLoader's constructor
        new (gLoaderStore) TbGraphicFileLoader;
        return 0;
    case 11:   // its destructor; a[0] 1: the deleting form, which also frees the object (counted, the storage is the world's)
        reinterpret_cast<TbGraphicFileLoader*>(gLoaderStore)->~TbGraphicFileLoader();
        if (a[0] > 0) ++tpw_cl_heap[2];
        return 0;
    case 12:   // TbTGALoader's constructor
        gLoader = new (gLoaderStore) TbTGALoader;
        return 0;
    case 13:   // its destructor, with the flag as above
        gLoader->~TbTGALoader();
        if (a[0] > 0) ++tpw_cl_heap[2];
        return 0;
    case 14: {   // ReadHeader(path, second, area): path text, second text (-1: NULL), area (-1: NULL)
        TbRenderArea* area = a[2] >= 0 ? AreaAt(a[2]) : nullptr;
        return gLoader->ReadHeader(Text(a[0]), a[1] >= 0 ? Text(a[1]) : nullptr, area).mCode;
    }
    case 15: return gLoader->ReadHeader(gFile0, a[0] >= 0 ? AreaAt(a[0]) : nullptr).mCode;
    case 16:   // Load(area, path, second, other)
        return gLoader->Load(*AreaAt(a[0]), Text(a[1]), a[2] >= 0 ? Text(a[2]) : nullptr, a[3] >= 0 ? AreaAt(a[3]) : nullptr).mCode;
    case 17: return gLoader->Load(*AreaAt(a[0]), gFile0, a[1] >= 0 ? AreaAt(a[1]) : nullptr).mCode;
    case 18: return gLoader->IsValid(gFile0) ? 1 : 0;
    case 19: return gLoader->ReadHeaderInfo(gFile0, *AreaAt(a[0])).mCode;
    case 20: return gLoader->ReadData(*AreaAt(a[0]), gFile0, *AreaAt(a[1])).mCode;
    case 21: return gLoader->ValidImage(gFile0) ? 1 : 0;
    // ---- the line readers
    case 22: {   // a buffered file made and destroyed: a[0] 1 is `delete`, 0 the destructor alone
        void* memory = ::operator new(sizeof(_TbBufferedFile));
        --tpw_cl_heap[0];
        auto* file = new (memory) _TbBufferedFile;
        if (a[0] > 0) {
            delete file;
        } else {
            file->~_TbBufferedFile();
            std::free(memory);
        }
        return 0;
    }
    case 29:   // makes the line reader (its vtable) over the 0xa5 storage
        gGll = new (gGllStore) _TgaGLL;
        return 0;
    case 30: return gGll->ReadHeaderInfo(gFile0, gLoader).mCode;
    case 31: return gGll->ReadData(*AreaAt(a[0]), gFile0, gLoader).mCode;
    case 32: return gGll->ReadFirstLine(gFile0).mCode;
    case 33: return gGll->ReadNextLine(gFile0).mCode;
    case 34: return gGll->ReverseScanLine(BlockAt(a[0]) + a[1]).mCode;
    case 35: return gGll->TGALoadRLE(gFile0).mCode;
    case 36: return gGll->GetSourceLineLength();
    case 37: return gGll->DoRead(*AreaAt(a[0]), gFile0, gLoader).mCode;
    case 38: gGll->WriteLine(); return 0;
    case 39:   // LbPalette_FromCompact(destination block, offset, source block, offset, count)
        LbPalette_FromCompact(reinterpret_cast<TbPalette*>(BlockAt(a[0]) + a[1]), BlockAt(a[2]) + a[3], static_cast<uint32_t>(a[4]));
        return 0;
    // ---- pokes: set the state a case starts from
    case 40: {   // an area's field: area, field (0 to 3 the masks, 4 depth, 5 palettised, 6 pixels, 7 width, 8 height, 9 stride), value, offset
        TbRenderArea* area = AreaAt(a[0]);
        if (a[1] <= 3) {
            const uint32_t mask = static_cast<uint32_t>(a[2]);
            std::memcpy(area->mF00 + 4 * a[1], &mask, 4);
        } else if (a[1] == 4) area->mDepth = static_cast<int32_t>(a[2]);
        else if (a[1] == 5) area->mPalettised = static_cast<int32_t>(a[2]);
        else if (a[1] == 6) {
            unsigned char* block = BlockAt(a[2]);
            area->mData = block != nullptr ? block + a[3] : (a[2] == 0 ? nullptr : reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(0x12340000u + a[3])));
        } else if (a[1] == 7) area->mWidth = static_cast<int32_t>(a[2]);
        else if (a[1] == 8) area->mPictureHeight = static_cast<int32_t>(a[2]);
        else area->mStride = static_cast<int32_t>(a[2]);
        return 0;
    }
    case 41: {   // a line reader's field: field, value, offset (field 0 the line buffer, 1 line size, 2 dest bytes, 3 source bytes, 4 bottom-up, 5 dest, 6 step, 7 rle, 8 right to left, 9 lines left, 10 line bytes, 11 + i header byte i)
        _TgaGLL* g = reinterpret_cast<_TgaGLL*>(gGllStore);
        unsigned char* block = BlockAt(a[1]);
        switch (a[0]) {
        case 0: g->mLine = block != nullptr ? block + a[2] : nullptr; break;
        case 1: g->mLineSize = static_cast<uint32_t>(a[1]); break;
        case 2: g->mDestBytes = static_cast<uint32_t>(a[1]); break;
        case 3: g->mSourceBytes = static_cast<uint32_t>(a[1]); break;
        case 4: g->mBottomUp = static_cast<int32_t>(a[1]); break;
        case 5: g->mDest = block != nullptr ? block + a[2] : nullptr; break;
        case 6: g->mDestStep = static_cast<int32_t>(a[1]); break;
        case 7: g->mRle = static_cast<int32_t>(a[1]); break;
        case 8: g->mRightToLeft = static_cast<int32_t>(a[1]); break;
        case 9: g->mLinesLeft = static_cast<uint32_t>(a[1]); break;
        case 10: g->mLineBytes = static_cast<uint32_t>(a[1]); break;
        default: reinterpret_cast<unsigned char*>(&g->mHeader)[a[0] - 11] = static_cast<unsigned char>(a[1]); break;
        }
        return 0;
    }
    case 42:   // a byte of the loader's palette: index, value
        gLoader->mPalette[a[0]] = static_cast<unsigned char>(a[1]);
        return 0;
    case 43: gLoader->mHasPalette = static_cast<int32_t>(a[0]); return 0;
    case 44: {   // the stream's position
        gStreamPos = a[0];
        return 0;
    }
    // ---- the cursors (a[0] a slot, a[1] ...)
    case 100: {   // an InterfaceCursor: slot
        void* memory = ::operator new(sizeof(InterfaceCursor));
        --tpw_cl_heap[0];   // (the script's own allocation is not the unit's heap traffic)
        std::memset(memory, 0xa5, sizeof(InterfaceCursor));
        gCursors[a[0]] = {new (memory) InterfaceCursor, 1};
        tpw_cl_label(kCursorLabel + static_cast<int32_t>(a[0]), memory);
        return 0;
    }
    case 101: {   // an InterfaceBFCursor: slot, the platform cursor (-1: none)
        void* memory = ::operator new(sizeof(InterfaceBFCursor));
        --tpw_cl_heap[0];
        std::memset(memory, 0xa5, sizeof(InterfaceBFCursor));
        auto* cursor = new (memory) InterfaceBFCursor;
        gCursors[a[0]] = {cursor, 2};
        tpw_cl_label(kCursorLabel + static_cast<int32_t>(a[0]), memory);
        if (a[1] >= 0) cursor->mCursor = gUCursors[a[1]];
        return 0;
    }
    case 102: {   // a platform cursor made on the heap, labelled: index
        const int32_t was = tpw_cl_on;
        tpw_cl_on = 0;
        void* memory = ::operator new(sizeof(UCursorOpenGL));
        --tpw_cl_heap[0];
        std::memset(memory, 0xa5, sizeof(UCursorOpenGL));
        UPoint hot = {0, 0};
        gUCursors[a[0]] = new (memory) UCursorOpenGL(hot, nullptr, 0, nullptr);
        tpw_cl_label(kUCursorLabel + static_cast<int32_t>(a[0]), memory);
        tpw_cl_on = was;
        return 0;
    }
    case 103: {   // a call on a cursor: slot, which, a, b
        InterfaceCursor* cursor = gCursors[a[0]].cursor;
        switch (a[1]) {
        case 0: cursor->~InterfaceCursor(); gCursors[a[0]] = {nullptr, 0}; return 0;   // the destructor without the delete: the slot is given up (it may hold a pointer to a freed object)
        case 1: delete cursor; gCursors[a[0]] = {nullptr, 0}; return 0;    // with it
        case 2: return cursor->GetVisible();
        case 3: cursor->SetData(nullptr, static_cast<char>(a[2])); return 0;
        case 4: cursor->SetHot(static_cast<char>(a[2]), static_cast<char>(a[3])); return 0;
        case 5: cursor->Show(); return 0;
        case 6: cursor->Hide(); return 0;
        case 7: cursor->Draw(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3])); return 0;
        case 8: cursor->Use(static_cast<int16_t>(a[2])); return 0;
        default: cursor->Animate(static_cast<int32_t>(a[2])); return 0;
        }
    }
    case 104: {   // a cursor's field: slot, field (0 visible, 1 hot x, 2 hot y, 3 mF0C), value
        InterfaceCursor* cursor = gCursors[a[0]].cursor;
        if (a[1] == 0) cursor->mVisible = static_cast<uint8_t>(a[2]);
        else if (a[1] == 1) cursor->mHotX = static_cast<int8_t>(a[2]);
        else if (a[1] == 2) cursor->mHotY = static_cast<int8_t>(a[2]);
        else cursor->mF0C = static_cast<uint32_t>(a[2]);
        return 0;
    }
    case 110: {   // a cursor table's constructor: table
        void* memory = ::operator new(sizeof(CursorTable));
        --tpw_cl_heap[0];
        std::memset(memory, 0xa5, sizeof(CursorTable));
        gTables[a[0]] = new (memory) CursorTable;
        tpw_cl_label(kTableLabel + static_cast<int32_t>(a[0]), memory);
        return 0;
    }
    case 111: return TableAt(a[0])->Allocate(static_cast<uint16_t>(a[1]));
    case 112: return TableAt(a[0])->SetEntry(static_cast<uint16_t>(a[1]), a[2] >= 0 ? gCursors[a[2]].cursor : nullptr);
    case 113: return lab(TableAt(a[0])->GetEntry(static_cast<uint16_t>(a[1])));
    case 114: {   // DeAllocate deletes the cursors the table keeps: those slots are gone
        CursorTable* table = TableAt(a[0]);
        if (table->mEntries != nullptr) {
            for (int k = 0; k < table->mCount; ++k)
                for (int i = 0; i < kCursorSlots; ++i)
                    if (gCursors[i].cursor != nullptr && gCursors[i].cursor == table->mEntries[k]) gCursors[i] = {nullptr, 0};
        }
        table->DeAllocate();
        return 0;
    }
    case 115:   // its destructor: a[1] 1 is `delete`, which also frees the table
        if (a[1] > 0) {
            delete gTables[a[0]];
            gTables[a[0]] = nullptr;
        } else {
            TableAt(a[0])->~CursorTable();
        }
        return 0;
    case 120: {   // LoadData on a fresh InterfaceBFCursor in slot a[0], reading the stream as file text a[1]
        void* memory = ::operator new(sizeof(InterfaceBFCursor));
        --tpw_cl_heap[0];
        std::memset(memory, 0xa5, sizeof(InterfaceBFCursor));
        auto* cursor = new (memory) InterfaceBFCursor;
        gCursors[a[0]] = {cursor, 2};
        tpw_cl_label(kCursorLabel + static_cast<int32_t>(a[0]), memory);
        return cursor->LoadData(Text(a[1]));
    }
    default: return -12345;
    }
}

// The loader world's state: the areas, the loader, the line reader, the blocks. Returns the number of numbers written.
int32_t tpw_cc_dump_world(int64_t* out) {
    int64_t* start = out;
    for (int i = 0; i < 4; ++i) PutArea(out, *AreaAt(i));
    // the loader: the palette flag, the palette bytes and the header (the TGA loader's members after the area)
    auto* loader = reinterpret_cast<TbTGALoader*>(gLoaderStore);
    Put(out, static_cast<uint32_t>(loader->mHasPalette));
    for (int i = 0; i < 0x400; ++i) Put(out, loader->mPalette[i]);
    PutHeader(out, loader->mHeader);
    // the line reader
    auto* gll = reinterpret_cast<_TgaGLL*>(gGllStore);
    PutPointer(out, gll->mLine);
    Put(out, gll->mLineSize);
    Put(out, gll->mDestBytes);
    Put(out, gll->mSourceBytes);
    Put(out, static_cast<uint32_t>(gll->mBottomUp));
    PutPointer(out, gll->mDest);
    Put(out, static_cast<uint32_t>(gll->mDestStep));
    Put(out, static_cast<uint32_t>(gll->mRle));
    Put(out, static_cast<uint32_t>(gll->mRightToLeft));
    Put(out, gll->mLinesLeft);
    Put(out, gll->mLineBytes);
    PutHeader(out, gll->mHeader);
    // the world's blocks, with their margins, and the live heap blocks (label, size, checksum)
    for (int i = 0; i < kBlocks; ++i) Put(out, Checksum(gBlockStore[i], kBlockSize + kMargin));
    Put(out, Checksum(gPaletteStore, sizeof gPaletteStore));
    int64_t* count = out++;
    Block heap[kBlockMax];
    int64_t live = 0;
    for (int i = 0; i < gRegistryCount; ++i) {
        if (gRegistry[i].label < 100 || gRegistry[i].freed) continue;
        int at = static_cast<int>(live++);
        while (at > 0 && heap[at - 1].label > gRegistry[i].label) {
            heap[at] = heap[at - 1];
            --at;
        }
        heap[at] = gRegistry[i];
    }
    for (int64_t i = 0; i < live; ++i) {
        Put(out, heap[i].label);
        Put(out, static_cast<int64_t>(heap[i].extent - kMargin));
        Put(out, Checksum(heap[i].start, heap[i].extent));
    }
    *count = live;
    Put(out, gStreamPos);
    Put(out, tpw_cl_heap[2]);
    return static_cast<int32_t>(out - start);
}

// The cursor world's state: each slot (kind, visible, hot spot, the untouched bytes, the last word, the platform cursor), the
// tables (made, count, entries) and the nest count.
int32_t tpw_cc_dump_cursors(int64_t* out) {
    int64_t* start = out;
    for (int i = 0; i < kCursorSlots; ++i) {
        const CursorSlot& slot = gCursors[i];
        Put(out, slot.kind);
        if (slot.kind == 0) continue;
        Put(out, lab(slot.cursor));
        Put(out, slot.cursor->mVisible);
        Put(out, slot.cursor->mHotX);
        Put(out, slot.cursor->mHotY);
        bool untouched = true;
        for (int k = 0; k < 9; ++k) untouched = untouched && slot.cursor->mF03[k] == 0xa5;
        Put(out, untouched ? 1 : 0);
        Put(out, slot.cursor->mF0C);
        if (slot.kind == 2) Put(out, lab(static_cast<InterfaceBFCursor*>(slot.cursor)->mCursor));
    }
    for (int i = 0; i < kTables; ++i) {
        const CursorTable* table = TableAt(i);
        Put(out, table != nullptr ? 1 : 0);
        if (table == nullptr) continue;
        Put(out, table->mCount);
        Put(out, table->mEntries == nullptr ? 0 : 1);
        for (int k = 0; k < table->mCount && table->mEntries != nullptr && k < 16; ++k) Put(out, lab(table->mEntries[k]));
    }
    Put(out, gShowCount);
    return static_cast<int32_t>(out - start);
}

// The heap traffic (blocks handled by new, new[], delete, delete[]).
void tpw_cc_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}

}  // extern "C"
