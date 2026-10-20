// Test library for the lobby's small leaves (libtpw_unit_LobbyLeaves2.dylib; see tests/test_LobbyLeaves2.py, unit w58):
//   src/rsse_normals.cpp   RecalculateInvalidNodeFaceNormals, RecalculateInvalidFaceNormals (no callees)
//   src/rsse_rotate.cpp    X/Y/ZRotateLMatrixNoTrans, RotateLMatrixXYZNoTrans, RotateLMatrixXYZNoTransHi (no callees but cos and sin)
//   src/bfl_tbrect.cpp     TbRect::Bounding (and Intersection)
//   src/ui_main_meshes.cpp DeInitGeneralUI and UIInterface::DeInitGeneral; src/InterfaceFont.cpp InterfaceFont::Release
//
// The face normals and the rotations run on arena records the test builds. The end of the interface calls the cursor table's
// DeAllocate, UI_CloseSystem, the string heap's DeAllocate and HFreeHeap, which are fakes here that record through calllog, and
// deletes up to three windows through the window's virtual destructor: the windows are TestWindow objects of a pool whose
// destructor and operator delete only record (the storage is the pool's), so a case can look at them afterwards.
//
// src/ui_main_meshes.cpp holds the interface's start-up as well, and the library must link, so the rest of what that file calls is
// stood in for below ("link stand-ins"): none of it runs in these tests.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "fake_strings.h"

#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "InGame.h"
#include "InterfaceWindow.h"
#include "LbTime.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"
#include "Text.h"
#include "UIInterface.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "ui_main.h"
#include "ui_textmanager.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
TPGlobals gTPGlobals;   // the game's default names, and the sine table
// The text object is only ever handed to fakes: storage under its symbol, never constructed.
#ifdef __APPLE__
extern unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
#else
extern unsigned char gTextStorage[sizeof(CText)] __asm__("gText");
#endif
alignas(16) unsigned char gTextStorage[sizeof(CText)] = {};
int32_t gScreenWidth;
InterfaceFont gInGameFont;   // 0x1037f1a0
UIInterface gUIInterface;

// ---- fakes: what the end of the interface calls. The cursor table's DeAllocate (src/ui_DrawObject.cpp) and the font's Release are
// real; each fake records how far they have got (the delete[] blocks so far, the table's count, whether the font has a bitmap), so that
// the order of the calls shows in the log.
namespace {
int64_t Progress() { return tpw_cl_heap[3]; }
}  // namespace
int32_t UI_CloseSystem() {
    ev("UI_CloseSystem", {Progress(), gCursorTable.mCount, gInGameFont.mBitmap != nullptr});
    return 1;
}
void InterfaceStringHeap::DeAllocate() {
    ev("StringHeap.DeAllocate", {Progress(), gCursorTable.mCount, gInGameFont.mBitmap != nullptr});
}
int32_t HFreeHeap(sHeapInfo* heap) {
    ev("HFreeHeap", {lab(heap), Progress(), gCursorTable.mCount, gInGameFont.mBitmap != nullptr});
    return 1;
}

// The windows: the base class's members record the destructor, nothing else is ever called. The vtable is emitted with DrawFunction.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this), Progress(), gCursorTable.mCount, gInGameFont.mBitmap != nullptr}); }
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

namespace {

constexpr int kWindows = 3;

// A window of the pool: deleting one only records (and counts the block), as the Mac side's fake destructor does.
class TestWindow : public InterfaceWindow {
public:
    TestWindow() : InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0) {}
    static void operator delete(void* object) {
        ++tpw_cl_heap[2];
        ev("delete", {lab(object)});
    }
};
alignas(TestWindow) unsigned char gWindowStorage[kWindows][sizeof(TestWindow)];

std::vector<void*> gBlocks;

// A zeroed arena block that lives until the next tpw_ll_reset.
void* Block(size_t bytes) {
    void* p = tpw::ArenaAllocZeroed(bytes);
    gBlocks.push_back(p);
    return p;
}

}  // namespace

// ---- link stand-ins: what the rest of the linked sources call (InitGeneralUI and the meshes: never run here)

extern "C" {

// ---- the arena (the records of the mesh file keep Ptr32 fields)
uint64_t tpw_ll_base() { return reinterpret_cast<uint64_t>(tpw::ArenaBase()); }

// Frees the blocks of the previous case, the font's bitmap and the window globals.
void tpw_ll_reset() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
    delete[] gInGameFont.mBitmap;
    gInGameFont.mBitmap = nullptr;
    gUIGeneralWindowA = gUIGeneralWindowB = gUIGeneralWindowC = nullptr;
    gUIHeap = nullptr;
    gCursorTable.mEntries = nullptr;
    gCursorTable.mCount = 0;
}

// A zeroed arena block (the test fills it).
uint64_t tpw_ll_alloc(uint32_t bytes) { return reinterpret_cast<uint64_t>(Block(bytes)); }

// ---- the face normals
void tpw_ll_normals_node(tag_sNode* node) { RecalculateInvalidNodeFaceNormals(node); }
void tpw_ll_normals_header(tag_sM3D2Header* header) { RecalculateInvalidFaceNormals(header); }

// ---- the rotations. which: 0 X, 1 Y, 2 Z. The sine table: `size` entries, scale, mask = size - 1, the first `size` words of the table.
void tpw_ll_rotate(int32_t which, sMatrix* matrix, float c, float s) {
    if (which == 0) XRotateLMatrixNoTrans(matrix, c, s);
    else if (which == 1) YRotateLMatrixNoTrans(matrix, c, s);
    else ZRotateLMatrixNoTrans(matrix, c, s);
}
void tpw_ll_rotate_xyz(int32_t hi, sMatrix* matrix, int32_t order, float* angles) {
    if (hi != 0) RotateLMatrixXYZNoTransHi(matrix, static_cast<eRotateParams>(order), angles);
    else RotateLMatrixXYZNoTrans(matrix, static_cast<eRotateParams>(order), angles);
}
void tpw_ll_set_sin(uint32_t size, uint32_t mask, float scale, const float* table) {
    gTPGlobals.mSinTableSize = size;
    gTPGlobals.mSinTableMask = mask;
    gTPGlobals.mSinTableScale = scale;
    const uint32_t n = size < 0x1000 ? size : 0x1000;
    std::memcpy(gTPGlobals.mSinTable, table, n * sizeof(float));
}

// ---- the rectangle: out = a.Bounding(b)
void tpw_ll_bounding(const int32_t* a, const int32_t* b, int32_t* out) {
    const TbRect left = {a[0], a[1], a[2], a[3]};
    const TbRect right = {b[0], b[1], b[2], b[3]};
    const TbRect result = left.Bounding(right);
    out[0] = result.left;
    out[1] = result.top;
    out[2] = result.right;
    out[3] = result.bottom;
}

// ---- the end of the interface
// A new window in place of pool window `index` (the last one's destructor left the base class's vtable pointer in it).
void* tpw_ll_window(int32_t index) { return new (gWindowStorage[index]) TestWindow(); }
// The cursor table: `count` entries, all NULL, in a new[] block (0: no block), and its two words.
void tpw_ll_set_cursors(int32_t count, uint32_t entries) {
    gCursorTable.mEntries = entries != 0 ? new InterfaceCursor*[count]() : nullptr;
    gCursorTable.mCount = static_cast<uint16_t>(count);
}
void tpw_ll_get_cursors(int64_t* out) {
    out[0] = gCursorTable.mEntries != nullptr;
    out[1] = gCursorTable.mCount;
}
// The three globals, as pool indices (-1: NULL).
void tpw_ll_set_windows(int32_t a, int32_t b, int32_t c) {
    gUIGeneralWindowA = a >= 0 ? static_cast<InterfaceWindow*>(static_cast<TestWindow*>(tpw_ll_window(a))) : nullptr;
    gUIGeneralWindowB = b >= 0 ? static_cast<InterfaceWindow*>(static_cast<TestWindow*>(tpw_ll_window(b))) : nullptr;
    gUIGeneralWindowC = c >= 0 ? static_cast<InterfaceWindow*>(static_cast<TestWindow*>(tpw_ll_window(c))) : nullptr;
}
void tpw_ll_get_windows(void** out) {
    out[0] = gUIGeneralWindowA;
    out[1] = gUIGeneralWindowB;
    out[2] = gUIGeneralWindowC;
}
void tpw_ll_set_heap(sHeapInfo* heap) { gUIHeap = heap; }
void* tpw_ll_get_heap() { return gUIHeap; }
// The debug font: a bitmap of `bitmap` bytes made with new[] (0: none) and its words.
void tpw_ll_set_font(int32_t bitmap, uint32_t f00, uint32_t f02, uint32_t f04, int32_t height, int32_t id) {
    delete[] gInGameFont.mBitmap;
    gInGameFont.mBitmap = bitmap > 0 ? new uint8_t[bitmap] : nullptr;
    if (bitmap > 0) std::memset(gInGameFont.mBitmap, 0x5c, bitmap);
    gInGameFont.mF00 = static_cast<uint16_t>(f00);
    gInGameFont.mF02 = static_cast<uint16_t>(f02);
    gInGameFont.mF04 = static_cast<uint16_t>(f04);
    gInGameFont.mHeight = static_cast<int16_t>(height);
    gInGameFont.mID = id;
}
void tpw_ll_get_font(int32_t* out) {
    out[0] = gInGameFont.mF00;
    out[1] = gInGameFont.mF02;
    out[2] = gInGameFont.mF04;
    out[3] = gInGameFont.mHeight;
    out[4] = gInGameFont.mBitmap != nullptr;
    out[5] = gInGameFont.mID;
}
// member 0: DeInitGeneralUI, 1: UIInterface::DeInitGeneral
void tpw_ll_deinit(int32_t member) {
    if (member == 0) DeInitGeneralUI();
    else gUIInterface.DeInitGeneral();
}

}  // extern "C"

namespace {
unsigned char gDummy[16];
}
void OverrideTexturePaths(tag_sM3D2Header*, int32_t, int32_t) {}
void CreateEngineMesh(tag_sMesh*) {}
void DestroyEngineMesh(tag_sMesh*) {}
void DestroyMeshInstance(tag_sMesh*, sHeapInfo*, uint32_t) {}
void EnableMeshDraw(tag_sMesh*, sMatrix*, int32_t) {}
void DisableMeshDraw(tag_sMesh*) {}
int32_t HNewHeap(uint32_t, uint32_t, uint32_t, sHeapInfo** heap) {
    *heap = reinterpret_cast<sHeapInfo*>(gDummy);
    return 1;
}
bool TPStrEq(const TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return false; }
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>&) {}
int32_t SetupDirectoryForMeshLoad(char*, char*, char*) { return 1; }
void TRACE(const char*, ...) {}
uint32_t LbTime_GetClock() { return 0; }
tag_sMesh* LoadMesh(char*, void*, int32_t, int32_t, int32_t) { return nullptr; }

CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, bfl::FileSystem::CFileStorage&, uint32_t) { return 0; }
bfl::FileSystem::CFileStorage* CTPBaseResource::pGetCurrentSearchStorage() const { return nullptr; }
int32_t CTPBaseResource::FindFirst(const TbDynamicStringTemplate<char>&, bfl::FileSystem::CFileInfo&) { return 0; }
int32_t CTPBaseResource::FindNext(bfl::FileSystem::CFileInfo&) { return 0; }
int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>&, CTPBaseResource**) { return 0; }
char* CText::GetLanguageName() { return nullptr; }
bool CText::IsHebrew() const { return false; }

void InterfaceFont::Gen8x8DebugFont(int16_t, int16_t, int16_t, int16_t, int16_t) {}
void UI_InitSystem(int32_t, int32_t, bool) {}
void UI_SetClockTickFunction(clockTickFunction*) {}
int32_t EffectPeekMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void UI_SetPeekCallBack(int32_t (*)(InterfaceWindow*, int32_t, int32_t, int32_t)) {}
void InterfaceStringHeap::Allocate(int32_t, int32_t, int32_t, int32_t) {}
void InitialiseCOLOURCursors() {}
void UI_UseCursorTable(CursorTable*) {}
void UI_SetStandardCursor(uint16_t) {}
void UI_SetCursor(int16_t) {}
void UI_HideCursor() {}
void InitHelpText() {}

// The title text object and its bases, blank (the class tree is not translated).
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
TbTextFormatter::TbTextFormatter() {}
TbTextFormatter::~TbTextFormatter() {}
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceTitleTextObject::~InterfaceTitleTextObject() {}
void InterfaceTitleTextObject::UpdateTextures(char16_t*) {}
void InterfaceDrawEngineMesh::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() { return this; }
void InterfaceDrawEngineResizableFrame::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableFrame::InstanceMe() { return this; }
void InterfaceDrawEngineResizableSlider::Change(uint32_t, DOChange*) {}
InterfaceDrawObject* InterfaceDrawEngineResizableSlider::InstanceMe() { return this; }
