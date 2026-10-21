// Unit w118 (the main panel): CreateMainPanel, InitStaffPanel, the vertical gradient (constructor, destructor, CalcVerts, CalcUVs, Show), the
// number and cash text objects (constructors, destructors), SetOriginalPosition, the three small message functions and the window scripts run
// for real; every callee outside the unit is a fake that records through calllog (see tests/test_MainPanel.py).
//
// The windows are fixed stores labelled 1 to 30, and GetWindow answers by the child's id (a queue per id; the default is the window the
// script gives that id). Objects the functions make with `new` are labelled as they are allocated. The text sprite's base constructor and
// the base destructors are fakes that record the events the Mac's inlined chain makes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <typeinfo>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TbTextFormatter.h"
#include "UI_Base.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {
void* gAllocated[64];
int32_t gAllocatedCount;
void* gMeshBlocks[64];
int32_t gMeshBlockCount;
bool KeepBlock(void* p, bool);
}  // namespace
#define TPW_RELEASE_OBSERVER KeepBlock
#include "calllog.cpp"
using namespace calllog;

// ---- the globals the unit does not define (UI_Base.cpp owns the screen size and src/game/ui/ui_staffPanel.cpp, which is not linked, the staff panels)
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gGameWindow;
InterfaceWindow* gStaffPanelA;
InterfaceWindow* gStaffPanelB;
InterfaceWindow* gStaffPanelC;

namespace {
constexpr int kWindows = 30;
constexpr int kFonts = 16;
// labels: the windows 1..30, the draw object store 31, the data token 33, the name string 70, the fonts 40..55 and, for the object the
// operations build in the fixed block, the block 60 and its parts 61..69
constexpr int kDrawStoreLabel = 31, kTokenLabel = 33, kFontLabel = 40, kBlockLabel = 60, kNameLabel = 70;
constexpr int kKindNone = 0, kKindText = 1, kKindNumber = 2, kKindCash = 3, kKindGradient = 4;

alignas(16) unsigned char gWindows[kWindows][sizeof(InterfaceWindow)];
alignas(16) unsigned char gFonts[kFonts][16];
alignas(16) unsigned char gDrawStore[16];
alignas(16) unsigned char gBlock[1024];
CoolRect gStaged;
char gName[] = "meter.wct";
// SetText's argument is a non-const pointer
char16_t gCounterText[8];

// the child ids of the scripts and the window each answers by default
struct Child {
    int32_t id;
    int label;
};
const Child kChildren[] = {
    {0x33, 3}, {0x37, 4}, {0x34, 5}, {0x36, 6}, {0x2d, 7}, {0x2e, 8}, {0x2f, 9}, {0x32, 10}, {0x30, 11}, {0x31, 12}, {0x1d, 13}, {0x21, 14},
    {0x22, 15}, {0x23, 16}, {0x24, 17}, {0x1e, 18}, {0x1f, 19}, {0x20, 20}, {0x25, 21}, {0x95c, 22}, {0x963, 23}, {0x95d, 24},
    {0x147, 26}, {0x148, 27}, {0x149, 28},
};

bool KeepBlock(void* p, bool) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] == p) return true;
    }
    return p == gBlock;
}

void* Obj(int32_t label) {
    if (label >= 1 && label <= kWindows) return gWindows[label - 1];
    if (label >= kFontLabel && label < kFontLabel + kFonts) return gFonts[label - kFontLabel];
    switch (label) {
        case kDrawStoreLabel: return gDrawStore;
        case kTokenLabel: return &gDrawStore[8];
        case kNameLabel: return gName;
        case kBlockLabel: return gBlock;
    }
    return nullptr;
}

// What kind of draw object a pointer is: 0 for NULL and for anything that did not come from `new` here.
int32_t KindOf(const InterfaceDrawObject* object) {
    if (object == nullptr) return kKindNone;
    bool known = false;
    for (int32_t i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] == object) known = true;
    }
    if (!known) return 9;
    const std::type_info& type = typeid(*object);
    if (type == typeid(InterfaceTextObjectSprite)) return kKindText;
    if (type == typeid(InterfaceNumberTextObject)) return kKindNumber;
    if (type == typeid(InterfaceCashAndGTickTextObject)) return kKindCash;
    if (type == typeid(InterfaceDrawEngineVGradient)) return kKindGradient;
    return 8;
}

// The objects the operations build and look at in the block (all of the class under test).
template <typename T>
T* InBlock() {
    return reinterpret_cast<T*>(gBlock);
}
}  // namespace

// Nothrow allocation records the Mac size and keeps the block until the next case; the queue "New" can make it fail.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = size;
    if (size == sizeof(InterfaceTextObjectSprite)) mac = 0x19c;
    else if (size == sizeof(InterfaceDrawEngineVGradient)) mac = 0x124;
    void* block = nullptr;
    if (ret("New", 1)) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        if (gAllocatedCount < 64) gAllocated[gAllocatedCount++] = block;
        newlab(block);
        ++tpw_cl_heap[0];
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the draw object base class and the sprite parts: constructors and destructors record, the rest is never reached
InterfaceDrawObject::InterfaceDrawObject(void* data) {
    ev("DrawObject.ctor", {lab(this), lab(data)});
    mData = data;
    mShown = 0;
    mFrameBase = 0;
    mFrame = 0;
    mFrameCount = 0;
    mInLibrary = 0;
    mResizable = 0;
    mRect.Set(0, 0, 0, 0);
    mRect2.Set(0, 0, 0, 0);
    mF28 = 0;
    mF2A = 0;
    mF2C = 0;
    mF2E = 0;
    mF30 = 0;
    mF32 = 0;
    mDepth = 0;
}
InterfaceDrawObject::~InterfaceDrawObject() { ev("DrawObject.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

SSpriteGameData::SSpriteGameData() { ev("GameData.ctor", {lab(this)}); }
SSpriteEngineData::SSpriteEngineData() { ev("Surface.ctor", {lab(this)}); }
SSpriteEngineData::~SSpriteEngineData() { ev("Surface.dtor", {lab(this)}); }
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() { ev("List.ctor", {lab(this)}); }
CSpriteList::~CSpriteList() { ev("List.dtor", {lab(this)}); }
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() { ev("Formatter.ctor", {lab(this)}); }
TbTextFormatter::~TbTextFormatter() { ev("Formatter.dtor", {lab(this)}); }

// The sprite object and the text sprite: only their constructors and destructors are reached (the Mac's constructor and destructor of the
// number and cash objects have these inlined). The text sprite's own constructor is the real one (src/game/ui/InterfaceTextObjectSprite.cpp).
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}

// The Change functions of the new classes are not the unit's (the gradient's is w42's; the number and cash objects' are untranslated).
void InterfaceNumberTextObject::Change(uint32_t, DOChange*) {}
void InterfaceCashAndGTickTextObject::Change(uint32_t, DOChange*) {}
void InterfaceDrawEngineVGradient::Change(uint32_t, DOChange*) {}

// ---- the game type: silent (the guard cannot be reset between cases; the case sets the type)
CGameType::CGameType() { mType = kGameTypeNormal; }

// ---- the mesh calls. DMCreateMesh answers from the queue "Create" and, when it answers 1, makes the arrays the constructor writes into.
int32_t DMLoadTextures(sDTextureSet* set) {
    ev("DMLoadTextures", {lab(set)});
    return static_cast<int32_t>(ret("Load", 1));
}
void DMFreeTextures(sDTextureSet* set) { ev("DMFreeTextures", {lab(set)}); }
int32_t DMFreeMesh(tag_sDMeshSet* set) {
    ev("DMFreeMesh", {lab(set)});
    return 1;
}
namespace {
// (the arena keeps its own books with the plain operator new, which the heap counts must not see)
void* MeshBlock(std::size_t size, bool arena) {
    int32_t heap[4];
    std::memcpy(heap, tpw_cl_heap, sizeof heap);
    void* block = arena ? tpw::ArenaAlloc(size) : std::malloc(size);
    std::memcpy(tpw_cl_heap, heap, sizeof heap);
    std::memset(block, 0xa5, size);
    if (gMeshBlockCount < 64) gMeshBlocks[gMeshBlockCount++] = block;
    return block;
}
}  // namespace
int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    ev("DMCreateMesh", {lab(set), lab(textures), flags}, {name != nullptr ? name : "(null)"});
    const int32_t made = static_cast<int32_t>(ret("Create", 1));
    if (made != 0) {
        set->mVertexSets = static_cast<sVertexPosnSet*>(MeshBlock(2 * 0x30, false));
        set->mPoints = static_cast<s3DPoint*>(MeshBlock(16 * 0xc, false));
        set->mUVSets = static_cast<sUVSet*>(MeshBlock(2 * 0x20, false));
        set->mFaces = static_cast<sFace*>(MeshBlock(4 * 8, false));
        sMatrix* matrix = static_cast<sMatrix*>(MeshBlock(0x40, false));
        newlab(matrix);
        set->mEntries[0].mMatrix = matrix;
        set->mEntries[0].mFaceSets = static_cast<sFaceSet*>(MeshBlock(0x10, false));
        textures->mTextures = static_cast<sTexture*>(MeshBlock(8, true));
    }
    return made;
}
int32_t DMEnableMeshRender(tag_sDMeshSet* set) {
    ev("DMEnableMeshRender", {lab(set)});
    return 1;
}
int32_t DMDisableMeshRender(tag_sDMeshSet* set) {
    ev("DMDisableMeshRender", {lab(set)});
    return 1;
}
void SetMatrixIdentity(sMatrix* matrix) { ev("SetMatrixIdentity", {lab(matrix)}); }

// ---- text helpers
void AdjustJustificationForHebrew(int32_t& justification) {
    ev("AdjustJustification", {justification});
    justification = static_cast<int32_t>(ret("Adjust", justification));
}
void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* extent) {
    ev("MeasureText", {font, horizontal, vertical, wrap, rect.left, rect.top, rect.right, rect.bottom}, {narrow(text).c_str()});
    extent->left = static_cast<int32_t>(ret("MeasureL", 0));
    extent->top = static_cast<int32_t>(ret("MeasureT", 0));
    extent->right = static_cast<int32_t>(ret("MeasureR", 0));
    extent->bottom = static_cast<int32_t>(ret("MeasureB", 0));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(Obj(kFontLabel + static_cast<int32_t>(index & 15)));
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(Obj(static_cast<int32_t>(ret("Draw", kDrawStoreLabel))));
}

// ---- the message functions only their identities are compared (the eight real ones are the unit's, the others never run)
#define CALLBACK(name) \
    int32_t name(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
CALLBACK(AerialCallback)
CALLBACK(RailsCallback)
CALLBACK(StaffPanelMsg)
CALLBACK(ModifiedCheckButtonsCallback)
CALLBACK(StaffLocMsg)

// ---- what unit w131's five handlers in src/game/ui/ui_mainpanel2.cpp reach (they are tested in tests/unit_UIPanels.cpp; here they only have to link)
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CFSGlobalMasterSave* gMasterSave;
CUI2DMap gUI2DMap;
CParkAnalyser* CWorld::GetParkAnalyser() { return nullptr; }
float CParkAnalyser_GetHappinessLevel(CParkAnalyser*, int32_t) { return 0.0f; }
CFSGlobalMasterSave::CFSGlobalMasterSave() {}
int32_t CFSGlobalMasterSave::HowManyKeys() const { return 0; }
int32_t CFSGlobalMasterSave::SpendingPower() const { return 0; }
void GotoLastUsedPanel(int32_t) {}
void ShowControlPanel() {}
void RemoveControlPanel() {}
void CUI2DMap::CreateMapPanel() {}
void CreateResearchPanel() {}
void DoCashWin(InterfaceWindow*) {}
int32_t UI_PostMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void LLCC_SystemSetSessionCost(int32_t) {}
int16_t InterfaceWindow::setAnimTimer(int32_t, int32_t) { return 0; }
int32_t InterfaceWindow::startAnimTimer(int32_t) { return 0; }

namespace {
int32_t CallbackId(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    if (callback == GoldenStuffMsg) return 1;
    if (callback == AerialCallback) return 2;
    if (callback == BobbleCallback) return 3;
    if (callback == CashWindowCallback) return 4;
    if (callback == MainPanelMsg) return 5;
    if (callback == RailsCallback) return 6;
    if (callback == HappyGaugeCallback) return 7;
    if (callback == HappyBarMsg) return 8;
    if (callback == DateWindowCallback) return 9;
    if (callback == MainPanelButtonsMsg) return 10;
    if (callback == StaffPanelMsg) return 11;
    if (callback == ModifiedCheckButtonsCallback) return 12;
    if (callback == StaffLocMsg) return 13;
    return -1;
}
int32_t ScriptId(int16_t* script) {
    if (script == gMainPanelScript) return 1;
    if (script == gStaffPanelScriptA) return 2;
    if (script == gStaffPanelScriptB) return 3;
    return 0;
}
}  // namespace

// ---- the windows: every member records; Enable and SetText are the virtual ones the Mac reaches through the vtable at +0x118
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b) {
    mParent = parent;
    (void)attributes;
    (void)id;
    (void)l;
    (void)t;
    (void)r;
    (void)b;
}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Enable", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this)}, {narrow(text).c_str()});
    return 0;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceWindow* InterfaceWindow::Build(int16_t* script, InterfaceWindowCallback callback) {
    const int32_t id = ScriptId(script);
    ev("Build", {lab(this), id, CallbackId(callback)});
    const int32_t defaults[] = {0, 29, 29, 25};
    char key[16];
    std::snprintf(key, sizeof key, "Build%d", id);
    return static_cast<InterfaceWindow*>(Obj(static_cast<int32_t>(ret(key, defaults[id & 3]))));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int label = 0;
    for (const Child& child : kChildren) {
        if (child.id == id) label = child.label;
    }
    char key[16];
    std::snprintf(key, sizeof key, "W%x", id);
    return static_cast<InterfaceWindow*>(Obj(static_cast<int32_t>(ret(key, label))));
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) { ev("SetCallback", {lab(this), CallbackId(callback)}); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = static_cast<int16_t>(ret("RectL", 0));
    rect->mTop = static_cast<int16_t>(ret("RectT", 0));
    rect->mRight = static_cast<int16_t>(ret("RectR", 0));
    rect->mBottom = static_cast<int16_t>(ret("RectB", 0));
    return 1;
}
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    ev("SetPosition", {lab(this), rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    return 1;
}
int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    ev("ShiftPosition", {lab(this), dx, dy});
    return 1;
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object), KindOf(object)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object), KindOf(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, reinterpret_cast<intptr_t>(value)});
    return 1;
}
void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }
void InterfaceWindow::unlink() { ev("unlink", {lab(this)}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default", 0));
}

// ---- the C interface the Python module drives
namespace {
// Fixed labels of the parts of the object in the block (the Mac's offsets are in the Python module).
void* PartAddress(int32_t label, int32_t kind) {
    if (kind == kKindGradient) {
        auto* g = InBlock<InterfaceDrawEngineVGradient>();
        switch (label) {
            case 61: return &g->mMeshSet;
            case 62: return &g->mTextures;
            case 63: return &g->mMatrix;
            case 64: return &g->mItem;
            case 65: return &g->mTextureEntry;
        }
    } else if (kind != kKindNone) {
        auto* t = InBlock<InterfaceTextObjectSprite>();
        switch (label) {
            case 66: return &t->mFormatter;
            case 67: return &t->mSprite;
            case 68: return &t->mEngineData;
            case 69: return &t->mList;
        }
    }
    return nullptr;
}

void PushText(int32_t* out, int& k, const char* text) {
    for (int i = 0; i < 24; ++i) {
        out[k++] = text != nullptr ? static_cast<unsigned char>(text[i]) : 0xfe;
        if (text == nullptr || text[i] == 0) break;
    }
}

// The text object's written fields (the Mac offsets 0xdc to 0xe4 and 0x48 to 0x4a) and its class.
void DumpText(int32_t* out, int& k) {
    auto* t = InBlock<InterfaceTextObjectSprite>();
    int32_t kind = kKindNone;
    const std::type_info& type = typeid(*t);
    if (type == typeid(InterfaceTextObjectSprite)) kind = kKindText;
    else if (type == typeid(InterfaceNumberTextObject)) kind = kKindNumber;
    else if (type == typeid(InterfaceCashAndGTickTextObject)) kind = kKindCash;
    out[k++] = kind;
    out[k++] = t->mJustify;
    out[k++] = t->mVJustify;
    out[k++] = t->mWrap;
    out[k++] = t->mF0DF;
    out[k++] = static_cast<uint8_t>(t->mFontId);
    out[k++] = t->mColourMode;
    out[k++] = t->mOffsetX;
    out[k++] = t->mOffsetY;
    for (int i = 0; i < 4; ++i) out[k++] = t->TextColour()[i];   // (the fourth, the alpha, is left as it was)
    out[k++] = lab(t->mData);
}

template <typename T>
void PushWords(int32_t* out, int& k, const T* values, int count) {
    for (int i = 0; i < count; ++i) {
        int32_t word;
        std::memcpy(&word, &values[i], sizeof(word));
        out[k++] = word;
    }
}

// The gradient's written fields, in the order of the Python module's reader; the arrays only when the mesh was made.
void DumpGradient(int32_t* out, int& k) {
    auto* g = InBlock<InterfaceDrawEngineVGradient>();
    out[k++] = typeid(*g) == typeid(InterfaceDrawEngineVGradient) ? kKindGradient : 0;
    out[k++] = reinterpret_cast<uintptr_t>(g->mData) != 0;
    out[k++] = g->mTextureEntry.mFlags;
    out[k++] = lab(g->mTextureEntry.mName);
    out[k++] = g->mTextures.mFlags;
    out[k++] = g->mTextures.mCount;
    PushText(out, k, g->mTextures.mFolder);
    out[k++] = g->mTextures.mSharedFolder == nullptr;
    out[k++] = g->mTextures.mEntries == &g->mTextureEntry;
    out[k++] = g->mTextures.mInfos == nullptr;
    out[k++] = g->mMeshSet.mF00;
    out[k++] = g->mMeshSet.mCount;
    out[k++] = g->mMeshSet.mEntries == &g->mItem;
    out[k++] = g->mMeshSet.mMatrix == &g->mMatrix;
    out[k++] = g->mItem.mFlags;
    out[k++] = g->mItem.mFaceCount;
    out[k++] = g->mItem.mVertexCount;
    out[k++] = g->mItem.mUVCount;
    out[k++] = g->mItem.mFaceSetCount;
    out[k++] = g->mItem.mPointCount;
    PushWords(out, k, g->mBox, 6);
    PushWords(out, k, g->mItem.mBox, 6);
    PushWords(out, k, &g->mFraction, 1);
    // what the constructor leaves as it found it: the entry's +0xe, the texture entry's +8 and +0xc and the matrix
    out[k++] = g->mItem.mF0E;
    out[k++] = g->mTextureEntry.mF08;
    out[k++] = g->mTextureEntry.mF0C;
    PushWords(out, k, &g->mMatrix.m[0][0], 16);
    if (reinterpret_cast<uintptr_t>(g->mData) == 0) return;
    PushWords(out, k, &g->mMeshSet.mVertexSets[0].x[0], 24);
    PushWords(out, k, &g->mMeshSet.mPoints[0].x, 48);
    PushWords(out, k, &g->mMeshSet.mUVSets[0].mF00[0], 16);
    for (int i = 0; i < 4; ++i) {
        const sFace& face = g->mMeshSet.mFaces[i];
        out[k++] = face.mIndexAndFlag;
        for (uint16_t index : face.mF02) out[k++] = index;
    }
    const sFaceSet& faceSet = g->mItem.mFaceSets[0];
    out[k++] = faceSet.mTexture.get() == g->mTextures.mTextures;
    out[k++] = faceSet.mFirstFace;
    out[k++] = faceSet.mFaceCount;
    out[k++] = faceSet.mF08;
    out[k++] = faceSet.mF0A;
    for (uint8_t byte : faceSet.mF0C) out[k++] = byte;
    out[k++] = g->mTextures.mTextures != nullptr;
    out[k++] = lab(g->mItem.mMatrix);
}

InterfaceWindow** WindowGlobal(int i) {
    InterfaceWindow** all[] = {&gMainPanel, &gGoldenStuffPanel, &gAerialWindow, &gSlideoutTab, &gStaffPanelA, &gStaffPanelB, &gStaffPanelC,
                               &gGameWindow, &gUIRootWindow};
    return all[i];
}
}  // namespace

extern "C" {
void* tpw_mp_obj(int32_t label) { return Obj(label); }
// Labels 61..65 are the gradient's parts and 66..69 the text object's: asked for by the kind the case builds.
void* tpw_mp_part(int32_t label, int32_t kind) { return PartAddress(label, kind); }

// Frees what the last case left behind and puts the world in its starting state: windows constructed over 0xa5 bytes, the block 0xa5.
void tpw_mp_reset() {
    tpw_cl_on = 0;
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    for (int32_t i = 0; i < gMeshBlockCount; ++i) {
        if (tpw::InArena(gMeshBlocks[i])) tpw::ArenaFree(gMeshBlocks[i]);
        else std::free(gMeshBlocks[i]);
    }
    gMeshBlockCount = 0;
    for (auto& store : gWindows) {
        std::memset(store, 0xa5, sizeof store);
        auto* window = new (store) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
        // (the Mac's block starts as 0xa5 bytes; the member rectangle's constructor would clear them)
        window->mOriginalRect.mLeft = window->mOriginalRect.mTop = window->mOriginalRect.mRight = window->mOriginalRect.mBottom = static_cast<int16_t>(0xa5a5);
    }
    std::memset(gBlock, 0xa5, sizeof gBlock);
    for (int i = 0; i < 8; ++i) gCounterText[i] = 0;
}

// The state: nine window pointers (labels), the nine words, the screen size and the game type.
void tpw_mp_state(const int32_t* in) {
    for (int i = 0; i < 9; ++i) {
        void* p = Obj(in[i]);
        std::memcpy(WindowGlobal(i), &p, sizeof p);
    }
    gMainPanelFontIndex = in[9];
    gLastPanelLists = in[10];
    gLastPanelPark = in[11];
    gLastPanelFinance = in[12];
    gStaffPanelState = in[13];
    gUIRealScreenWidth = static_cast<int16_t>(in[14]);
    gUIRealScreenHeight = static_cast<int16_t>(in[15]);
    TheGameType().mType = static_cast<GameType>(in[16]);
    for (int32_t& count : tpw_cl_heap) count = 0;
}

// The dump: the globals, the original rectangle of every window, the heap counts and the object in the block (kind 1 text objects, 2
// the gradient).
int32_t tpw_mp_dump(int32_t* out, int32_t objectKind) {
    int k = 0;
    for (int i = 0; i < 9; ++i) out[k++] = lab(*WindowGlobal(i));
    out[k++] = gMainPanelFontIndex;
    out[k++] = gLastPanelLists;
    out[k++] = gLastPanelPark;
    out[k++] = gLastPanelFinance;
    out[k++] = gStaffPanelState;
    out[k++] = gUIRealScreenWidth;
    out[k++] = gUIRealScreenHeight;
    out[k++] = TheGameType().mType;
    for (int i = 0; i < kWindows; ++i) {
        auto* w = reinterpret_cast<InterfaceWindow*>(gWindows[i]);
        out[k++] = w->mOriginalRect.mLeft;
        out[k++] = w->mOriginalRect.mTop;
        out[k++] = w->mOriginalRect.mRight;
        out[k++] = w->mOriginalRect.mBottom;
    }
    for (int32_t count : tpw_cl_heap) out[k++] = count;
    if (objectKind == 1) DumpText(out, k);
    if (objectKind == 2) DumpGradient(out, k);
    return k;
}

// The rectangle SetOriginalPosition copies (the original rectangles of all windows start at 0xa5a5 from the reset).
void tpw_mp_rect(const int32_t* in) {
    gStaged.mLeft = static_cast<int16_t>(in[0]);
    gStaged.mTop = static_cast<int16_t>(in[1]);
    gStaged.mRight = static_cast<int16_t>(in[2]);
    gStaged.mBottom = static_cast<int16_t>(in[3]);
}

int32_t tpw_mp_call(int32_t op, int32_t a, int32_t b, int32_t c, int32_t d) {
    switch (op) {
        case 0: CreateMainPanel(); break;
        case 1: InitStaffPanel(); break;
        case 2: new (gBlock) InterfaceDrawEngineVGradient(a != 0 ? gName : nullptr); break;
        case 3: new (gBlock) InterfaceNumberTextObject(a != 0 ? Obj(kTokenLabel) : nullptr); break;
        case 4: new (gBlock) InterfaceCashAndGTickTextObject(a != 0 ? Obj(kTokenLabel) : nullptr); break;
        case 5:
        case 6:
        case 7: {
            // build the object (with its events), then destroy it with the flag
            InterfaceDrawObject* object;
            if (op == 5) object = new (gBlock) InterfaceNumberTextObject(nullptr);
            else if (op == 6) object = new (gBlock) InterfaceCashAndGTickTextObject(nullptr);
            else object = new (gBlock) InterfaceDrawEngineVGradient(gName);
            if (static_cast<int16_t>(a) > 0) delete object;
            else object->~InterfaceDrawObject();
            break;
        }
        case 8:
        case 9:
        case 10: {
            auto* g = new (gBlock) InterfaceDrawEngineVGradient(gName);
            if (op == 10) {
                g->mShown = static_cast<int16_t>(b);
                g->Show(static_cast<int16_t>(a));
            } else {
                std::memcpy(&g->mFraction, &a, sizeof(float));
                if (op == 8) g->CalcVerts();
                else g->CalcUVs();
            }
            break;
        }
        case 11: {
            auto* w = reinterpret_cast<InterfaceWindow*>(Obj(a));
            return w->SetOriginalPosition(b != 0 ? nullptr : &gStaged);
        }
        case 12: return MainPanelMsg(reinterpret_cast<InterfaceWindow*>(Obj(a)), b, c, d);
        case 13: return HappyGaugeCallback(reinterpret_cast<InterfaceWindow*>(Obj(a)), b, c, d);
        case 14: return DateWindowCallback(reinterpret_cast<InterfaceWindow*>(Obj(a)), b, c, d);
    }
    return 0;
}

int16_t* tpw_mp_script(int32_t which) {
    int16_t* scripts[] = {gMainPanelScript, gStaffPanelScriptA, gStaffPanelScriptB};
    return scripts[which];
}
}
