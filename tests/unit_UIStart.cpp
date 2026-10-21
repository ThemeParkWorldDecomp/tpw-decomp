// Test library for the start of the interface (libtpw_unit_UIStart.dylib; see
// tests/test_UIStart.py): src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp (rectangles, the name hash), src/game/ui/InterfaceCursor.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceDrawObject.cpp
// (cursors, draw objects, the library, the engine mesh objects), src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_main.cpp
// (the interface's meshes, InitGeneralUI) and src/game/TextureTable.cpp (the text file reader).
//
// Real: all of those, and the game's default names (src/game/TPGlobals.cpp). Functional and
// unrecorded: the bullfrog strings, TPStrEq. Everything else the unit calls is a fake that
// records. Objects the unit makes with operator new get a label as they are allocated, and
// their memory is filled with 0xa5 first, so a field no constructor sets shows as that.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "InGame.h"
#include "LbTime.h"
#include "Quickload.h"
#include "TPAdvisor.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPResource.h"
#include "TPString.h"
#include "Text.h"
#include "UIInterface.h"
#include "platform/arena.h"
#include "rsse.h"
#include "ui_main.h"
#include "ui_textmanager.h"

#include "fake_strings.h"

using namespace calllog;
using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

namespace {
enum { kBase = 0, kEngine = 1, kTitle = 2, kNode = 3, kCursor = 4 };
std::map<const void*, int> gKinds;   // what each block the unit allocated is

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!on()) return;
    if (array) {   // the cursor table's entries (the strings here do not use operator new)
        std::memset(block, 0xa5, size);
        return;
    }
    int kind;
    int64_t mac;
    if (size == sizeof(InterfaceDrawObject)) kind = kBase, mac = 0x3c;
    else if (size == sizeof(InterfaceDrawEngineMesh)) kind = kEngine, mac = 0x84;
    else if (size == sizeof(InterfaceTitleTextObject)) kind = kTitle, mac = 0x19c;
    else if (size == sizeof(DrawObjectLibrary::Node)) kind = kNode, mac = 0x18;
    else if (size == sizeof(InterfaceCursor)) kind = kCursor, mac = 0x14;
    else return;
    std::memset(block, 0xa5, size);
    gKinds[block] = kind;
    newlab(block);
    ev("new", {mac});
}

constexpr int kSlots = 8, kMeshes = 6;
InterfaceDrawObject* gSlots[kSlots];
tag_sMesh* gMeshes[kMeshes];
tag_sMesh* gLoadable[kMeshes];       // the ones with a root node: what LoadMesh gives
int32_t gMeshCount, gLoadableCount, gNextMesh;
uint32_t gNodeWord;                  // what an object's node flags pointer is pointed at
char gLanguage[32] = "english";
unsigned char gManager[16], gStorage[16], gHeap[16];

struct FileList {
    char prefix[32];
    char names[24][40];
    int32_t count, cursor;
};
FileList gFiles[2];

class FakeResource : public CTPBaseResource {
public:
    FakeResource(const char* name, const char* path)
        : CTPBaseResource(TbDynamicStringTemplate<char>(name), TbDynamicStringTemplate<char>(path)) {}
    ~FakeResource() override {}
};
FakeResource* gResources[2];

int32_t NextFile(CTPBaseResource* resource, CFileInfo& info) {
    FileList& list = gFiles[resource == gResources[1] ? 1 : 0];
    if (list.cursor >= list.count) return 0;
    std::strcpy(info.mNameBuffer, list.prefix);
    std::strcat(info.mNameBuffer, list.names[list.cursor++]);
    info.mName = info.mNameBuffer + std::strlen(list.prefix);
    info.mF00 = 100;
    info.mAttributes = 0x81;
    return 1;
}

// The object a matrix belongs to, for EnableMeshDraw's record.
int32_t OwnerOfMatrix(const sMatrix* matrix) {
    for (const auto& entry : gKinds) {
        if (entry.second != kEngine) continue;
        const auto* object = static_cast<const InterfaceDrawEngineMesh*>(entry.first);
        if (&object->mMatrix == matrix) return lab(object);
    }
    return -1;
}
}  // namespace

// ---- globals of other files
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
TPGlobals gTPGlobals;   // the game's default names (".md2", "*.md2", "UI", "Langauge")
// The text object is only ever handed to fakes: storage under its symbol, never constructed.
#ifdef __APPLE__
extern unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
#else
extern unsigned char gTextStorage[sizeof(CText)] __asm__("gText");
#endif
alignas(16) unsigned char gTextStorage[sizeof(CText)] = {};
int32_t gScreenWidth;
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFont gInGameFont;

// ---- fakes: the engine and the game
tag_sMesh* LoadMesh(char* name, void* unused, int32_t noEngineMesh, int32_t pathArgument, int32_t oneFrame) {
    ev("LoadMesh", {unused == nullptr, noEngineMesh, pathArgument, oneFrame}, {name});
    if (!ret("Load", 1) || gLoadableCount == 0) return nullptr;
    return gLoadable[gNextMesh++ % gLoadableCount];
}
void OverrideTexturePaths(tag_sM3D2Header* header, int32_t checkFiles, int32_t unused) {
    ev("OverrideTexturePaths", {lab(header), checkFiles, unused});
}
void CreateEngineMesh(tag_sMesh* mesh) { ev("CreateEngineMesh", {lab(mesh)}); }
void DestroyEngineMesh(tag_sMesh* mesh) { ev("DestroyEngineMesh", {lab(mesh)}); }
void DestroyMeshInstance(tag_sMesh* mesh, sHeapInfo* heap, uint32_t flags) {
    ev("DestroyMeshInstance", {lab(mesh), lab(heap), flags});
}
void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t flags) {
    ev("EnableMeshDraw", {lab(mesh), OwnerOfMatrix(matrix), flags});
}
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw", {lab(mesh)}); }
int32_t HNewHeap(uint32_t a, uint32_t b, uint32_t c, sHeapInfo** heap) {
    ev("HNewHeap", {a, b, c, heap == &gUIHeap});
    *heap = reinterpret_cast<sHeapInfo*>(gHeap);
    return 1;
}
bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    const char* x = a.mData;
    const char* y = b.mData;
    const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; };
    for (; lower(*x) == lower(*y); ++x, ++y)
        if (*x == 0) return true;
    return false;
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("FS_AddDirectorySeparator", {}, {path.mData});
    if (path.Length() == 0 || path.mData[path.Length() - 1] != ':') path += ":";
}
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    ev("SetupDirectoryForMeshLoad", {}, {file, folder != nullptr ? folder : "(null)",
                                         sharedTextureFolder != nullptr ? sharedTextureFolder : "(null)"});
    return 1;
}
void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ev("TRACE", {va_arg(args, int)}, {format});
    va_end(args);
}
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock");
    return static_cast<uint32_t>(ret("Clock", 0x1234));
}

// ---- fakes: the resources
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const TbDynamicStringTemplate<char>&, CFileStorage&, uint32_t) { return 0; }
CFileStorage* CTPBaseResource::pGetCurrentSearchStorage() const {
    ev("pGetCurrentSearchStorage", {lab(this)});
    return reinterpret_cast<CFileStorage*>(gStorage);
}
// The files of the test's list, one per call; a queued result replaces the natural one.
int32_t CTPBaseResource::FindFirst(const TbDynamicStringTemplate<char>& pattern, CFileInfo& info) {
    ev("FindFirst", {lab(this)}, {pattern.mData});
    gFiles[this == gResources[1] ? 1 : 0].cursor = 0;
    const int32_t found = NextFile(this, info);
    const int64_t forced = ret("FindFirst", -1);
    return forced == -1 ? found : static_cast<int32_t>(forced);
}
int32_t CTPBaseResource::FindNext(CFileInfo& info) {
    ev("FindNext", {lab(this)});
    const int32_t found = NextFile(this, info);
    const int64_t forced = ret("FindNext", -1);
    return forced == -1 || found == 0 ? found : static_cast<int32_t>(forced);
}
int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource) {
    ev("FindResource", {lab(this)}, {name.mData});
    const int32_t result = static_cast<int32_t>(ret("FindResource", 1));
    if (result != 0) *resource = gResources[std::strcmp(name.mData, "UI") == 0 ? 0 : 1];
    return result;
}
char* CText::GetLanguageName() {
    ev("GetLanguageName", {lab(this)});
    return gLanguage;
}
bool CText::IsHebrew() const {
    ev("IsHebrew", {lab(this)});
    return ret("Hebrew", 0) != 0;
}

// ---- fakes: the UI system
void InterfaceFont::Gen8x8DebugFont(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e) {
    ev("Gen8x8DebugFont", {this == &gInGameFont, a, b, c, d, e});
}
void UI_InitSystem(int32_t a, int32_t b, bool hebrew) { ev("UI_InitSystem", {a, b, hebrew}); }
void UI_SetClockTickFunction(clockTickFunction* clock) { ev("UI_SetClockTickFunction", {clock == &gUIClockTick}); }
int32_t EffectPeekMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void UI_SetPeekCallBack(int32_t (*callback)(InterfaceWindow*, int32_t, int32_t, int32_t)) {
    ev("UI_SetPeekCallBack", {callback == EffectPeekMsg});
}
void InterfaceStringHeap::Allocate(int32_t a, int32_t b, int32_t c, int32_t d) { ev("InterfaceStringHeap::Allocate", {a, b, c, d}); }
void InitialiseCOLOURCursors() { ev("InitialiseCOLOURCursors"); }
void UI_UseCursorTable(CursorTable* table) { ev("UI_UseCursorTable", {table == &gCursorTable}); }
void UI_SetStandardCursor(uint16_t cursor) { ev("UI_SetStandardCursor", {cursor}); }
void UI_SetCursor(int16_t cursor) { ev("UI_SetCursor", {cursor}); }
void UI_HideCursor() { ev("UI_HideCursor"); }
void InitHelpText() { ev("InitHelpText"); }
// The end of the interface (DeInitGeneralUI, unit w58) is not run here; the file's other functions need these to link.
int32_t UI_CloseSystem() { return 1; }
void InterfaceStringHeap::DeAllocate() {}
void InterfaceFont::Release() {}
int32_t HFreeHeap(sHeapInfo*) { return 1; }

// The title text object: the base is real, the rest records. Its bases are the text object and the sprite draw object, which the unit
// does not translate: blank ones (and the parts the sprite draw object holds), so that the title can exist. The title's own Change and
// Show record, as they did when the class was one placeholder; the text object's Change is the one the title inherits, so the title
// overrides it here.
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
// The font renderer the formatter owns by value since w35a (fakes added at the w35c merge, 2026-10-12).
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    const DOTextStyle* style = change->mText;
    ev("TitleText.Change", {lab(this), flags, style->mF04, style->mF05, style->mF09, style->mColour[0], style->mColour[1],
                            style->mColour[2]});
}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(data) {
    ev("TitleText.ctor", {lab(this), data == nullptr});
}
InterfaceTitleTextObject::~InterfaceTitleTextObject() { ev("TitleText.dtor", {lab(this)}); }
void InterfaceTitleTextObject::UpdateTextures(char16_t*) {}

// The untranslated members of the engine mesh objects.
void InterfaceDrawEngineMesh::Change(uint32_t flags, DOChange* change) { ev("EngineMesh.Change", {lab(this), flags, change != nullptr}); }
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() {
    ev("EngineMesh.InstanceMe", {lab(this)});
    return this;
}
void InterfaceDrawEngineResizableFrame::Change(uint32_t flags, DOChange* change) { ev("Frame.Change", {lab(this), flags, change != nullptr}); }
InterfaceDrawObject* InterfaceDrawEngineResizableFrame::InstanceMe() {
    ev("Frame.InstanceMe", {lab(this)});
    return this;
}
void InterfaceDrawEngineResizableSlider::Change(uint32_t flags, DOChange* change) { ev("Slider.Change", {lab(this), flags, change != nullptr}); }
InterfaceDrawObject* InterfaceDrawEngineResizableSlider::InstanceMe() {
    ev("Slider.InstanceMe", {lab(this)});
    return this;
}

namespace {
bool Untouched(const void* data, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}

// An object's fields, in the order of PpcSide.dump in the Python module.
void Dump(const InterfaceDrawObject* object, int64_t*& out) {
    const auto found = gKinds.find(object);
    const int kind = found != gKinds.end() ? found->second : -1;
    *out++ = kind;
    *out++ = lab(object->mData);
    *out++ = object->mShown;
    *out++ = object->mFrameBase;
    *out++ = object->mFrame;
    *out++ = object->mFrameCount;
    *out++ = object->mInLibrary;
    *out++ = object->mResizable;
    for (const CoolRect* rect : {&object->mRect, &object->mRect2}) {
        *out++ = rect->mLeft;
        *out++ = rect->mTop;
        *out++ = rect->mRight;
        *out++ = rect->mBottom;
    }
    *out++ = object->mF28;
    *out++ = object->mF2A;
    *out++ = object->mF2C;
    *out++ = object->mF2E;
    *out++ = object->mF30;
    *out++ = object->mF32;
    *out++ = object->mDepth;
    if (kind == kEngine) {
        const auto* mesh = static_cast<const InterfaceDrawEngineMesh*>(object);
        *out++ = Untouched(&mesh->mMatrix, sizeof mesh->mMatrix);
        *out++ = mesh->mNodeFlags == nullptr ? 0 : mesh->mNodeFlags == &gNodeWord ? 1 : -1;
        *out++ = mesh->mLocalised;
    }
}

CFileInfo MakeInfo(const char* name) {
    CFileInfo info;
    std::strcpy(info.mNameBuffer, "pre:");
    std::strcat(info.mNameBuffer, name);
    info.mName = info.mNameBuffer + 4;
    info.mF00 = 100;
    info.mAttributes = 0x81;
    return info;
}

// The objects the library has, before it is emptied with their destruction.
bool InLibrary(const InterfaceDrawObject* object) {
    for (const DrawObjectLibrary::Node* node = gDrawObjectLibrary; node != nullptr; node = node->mNext)
        if (node->mObject == object) return true;
    return false;
}
}  // namespace

extern "C" {
void tpw_us_begin(const char* language, const char* path0, const char* path1, const char* prefix0, const char* prefix1) {
    for (auto& slot : gSlots) slot = nullptr;
    gKinds.clear();
    gDrawObjectLibrary = nullptr;
    gTitleText = nullptr;
    gUIHeap = nullptr;
    gCursorTable.mEntries = nullptr;
    gCursorTable.mCount = 0;
    gMeshCount = gLoadableCount = gNextMesh = 0;
    gNodeWord = 0;
    gScreenWidth = 800;
    gUIRealScreenWidth = 800;
    gUIRealScreenHeight = 600;
    std::strcpy(gLanguage, language);
    gGame.mResourceManager = reinterpret_cast<CTPResourceManager*>(gManager);
    gResources[0] = new FakeResource("UI", path0);
    gResources[1] = new FakeResource("Langauge", path1);
    std::strcpy(gFiles[0].prefix, prefix0);
    std::strcpy(gFiles[1].prefix, prefix1);
    gFiles[0].count = gFiles[1].count = 0;
}
void tpw_us_file(int32_t which, const char* name) {
    FileList& list = gFiles[which];
    std::strcpy(list.names[list.count++], name);
}
void* tpw_us_fixed(int32_t what) {
    switch (what) {
    case 0: return gManager;
    case 1: return gResources[0];
    case 2: return gResources[1];
    case 3: return gStorage;
    case 4: return gHeap;
    case 5: return gTextStorage;
    }
    return nullptr;
}
// A mesh image: `chain` nodes linked from the header's root, each with flags and a name;
// `count` is what the header says it has. Returns the mesh; *header gets its header.
void* tpw_us_mesh(int32_t count, int32_t chain, uint32_t headerFlags, uint32_t nodeFlags, const char* names, void** header) {
    auto* image = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    image->mFlags = headerFlags;
    image->mNodeCount = static_cast<uint16_t>(count);
    tag_sNode* previous = nullptr;
    const char* name = names;
    for (int32_t i = 0; i < chain; ++i) {
        auto* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        node->mFlags = nodeFlags + static_cast<uint32_t>(i) * 0x100;
        const size_t length = std::strcspn(name, "\n");
        char* text = static_cast<char*>(tpw::ArenaAllocZeroed(static_cast<uint32_t>(length + 1)));
        std::memcpy(text, name, length);
        node->mName = text;
        name += length + (name[length] != 0 ? 1 : 0);
        if (previous == nullptr) image->mF78 = node;
        else previous->mF0C = node;
        previous = node;
    }
    auto* mesh = static_cast<tag_sMesh*>(std::calloc(1, sizeof(tag_sMesh) + 64));
    mesh->mHeader = image;
    gMeshes[gMeshCount++] = mesh;
    if (chain > 0) gLoadable[gLoadableCount++] = mesh;
    *header = image;
    return mesh;
}

int64_t tpw_us_op(int32_t op, const int32_t* a, const char* text, const char* text2) {
    InterfaceDrawObject* object = a[0] >= 0 && a[0] < kSlots ? gSlots[a[0]] : nullptr;
    char name[64];
    std::strcpy(name, text);
    switch (op) {
    case 0: gSlots[a[0]] = new InterfaceDrawObject(a[1] >= 0 ? gMeshes[a[1]] : nullptr); return lab(gSlots[a[0]]);
    case 1: {
        void* mesh = a[2] >= 0 ? gMeshes[a[2]] : nullptr;
        gSlots[a[0]] = a[1] == 0 ? new InterfaceDrawEngineMesh(mesh) :
                       a[1] == 1 ? static_cast<InterfaceDrawEngineMesh*>(new InterfaceDrawEngineResizableFrame(mesh)) :
                                   static_cast<InterfaceDrawEngineMesh*>(new InterfaceDrawEngineResizableSlider(mesh));
        return lab(gSlots[a[0]]);
    }
    case 2:
        delete object;
        gSlots[a[0]] = nullptr;
        return 0;
    case 3: {
        DOChange change;
        std::memset(static_cast<void*>(&change), 0, sizeof change);
        change.mFrame = static_cast<int16_t>(a[2]);
        change.mDepth = static_cast<int16_t>(a[2] * 3 + 1);
        change.mF08 = static_cast<int16_t>(a[2] * 5 + 2);
        change.mFrameBase = static_cast<int16_t>(a[2] + 1);
        change.mF0C = static_cast<int16_t>(a[2] * 7 + 3);
        change.mF0E = static_cast<int16_t>(a[2] * 11 + 4);
        object->Change(static_cast<uint32_t>(a[1]), a[3] ? nullptr : &change);
        return 0;
    }
    case 4: object->Show(static_cast<int16_t>(a[1])); return 0;
    case 5: object->SetFrame(static_cast<int16_t>(a[1])); return 0;
    case 6: object->SetFrameBase(static_cast<int16_t>(a[1])); return 0;
    case 7: {
        int32_t x = 5, y = 6;
        const int32_t query = object->Query(static_cast<uint32_t>(a[1]), a[2]);
        const int32_t area = object->GetWritableDataArea(&x, &y);
        object->Refresh();
        object->Draw(1, &object->mRect, 2);
        object->Draw(1, &object->mRect, 2, nullptr);
        return query * 100 + area * 10 + (x == 5 && y == 6);
    }
    case 8: return DrawObjectLibrary::AddDrawObject(a[0] >= 0 ? object : nullptr, name);
    case 9: return lab(DrawObjectLibrary::FindDrawObject(name));
    case 10: return lab(DrawObjectLibrary::FindDrawObject(static_cast<uint32_t>(a[1])));
    case 11:   // then the list is the test's to forget: the original leaves its head dangling
        if (a[1] != 0) {
            for (auto& slot : gSlots)
                if (slot != nullptr && InLibrary(slot)) slot = nullptr;
        }
        DrawObjectLibrary::RemoveAll(static_cast<int16_t>(a[1]));
        gDrawObjectLibrary = nullptr;
        return 0;
    case 12: return UI_hash_cakes(text, static_cast<uint32_t>(a[1]));
    case 13: {
        TbDynamicStringTemplate<char> folder(text2);
        gSlots[a[0]] = TPResource_AddFrameMesh(MakeInfo(text), *reinterpret_cast<CFileStorage*>(gStorage), folder);
        return lab(gSlots[a[0]]);
    }
    case 14: {
        TbDynamicStringTemplate<char> folder(text2);
        gSlots[a[0]] = TPResource_AddSliderMesh(MakeInfo(text), *reinterpret_cast<CFileStorage*>(gStorage), folder);
        return lab(gSlots[a[0]]);
    }
    case 15:
        gSlots[a[0]] = TPResource_AddMeshDrawObject(MakeInfo(text), *reinterpret_cast<CFileStorage*>(gStorage),
                                                    TbDynamicStringTemplate<char>(text2));
        return lab(gSlots[a[0]]);
    case 16: return LoadAllUIMeshes();
    case 17: return LoadLocalisedUIMeshes();
    case 18: InitGeneralUI(); return 0;
    case 19: reinterpret_cast<UIInterface*>(gStorage)->InitGeneral(); return 0;
    case 20:
        gScreenWidth = a[1];
        gUIRealScreenWidth = static_cast<int16_t>(a[2]);
        gUIRealScreenHeight = static_cast<int16_t>(a[3]);
        return 0;
    case 21: {
        clockTickFunction base;
        return static_cast<int64_t>(base.tickyticky()) * 0x100000 + static_cast<clockTickFunction&>(gUIClockTick).tickyticky();
    }
    case 22:
        gNodeWord = static_cast<uint32_t>(a[1]);
        static_cast<InterfaceDrawEngineMesh*>(object)->mNodeFlags = &gNodeWord;
        return 0;
    case 23: object->mFrameCount = static_cast<int16_t>(a[1]); return 0;
    case 24: return lab(object->InstanceMe());
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module; returns how many values.
int32_t tpw_us_state(int64_t* out) {
    int64_t* p = out;
    for (const InterfaceDrawObject* object : gSlots) {
        *p++ = lab(object);
        if (object != nullptr) Dump(object, p);
    }
    int32_t nodes = 0;
    for (const DrawObjectLibrary::Node* node = gDrawObjectLibrary; node != nullptr && nodes < 60; node = node->mNext) ++nodes;
    *p++ = nodes;
    int32_t shown = 0;
    for (const DrawObjectLibrary::Node* node = gDrawObjectLibrary; node != nullptr && shown < 60; node = node->mNext, ++shown) {
        *p++ = lab(node);
        *p++ = node->mHash;
        for (int32_t i = 0; i < 12; i += 4) {
            *p++ = static_cast<int64_t>(static_cast<uint8_t>(node->mName[i])) << 24 | static_cast<uint8_t>(node->mName[i + 1]) << 16 |
                   static_cast<uint8_t>(node->mName[i + 2]) << 8 | static_cast<uint8_t>(node->mName[i + 3]);
        }
        *p++ = lab(node->mObject);
        if (node->mObject != nullptr) Dump(node->mObject, p);
    }
    for (int32_t i = 0; i < gMeshCount; ++i) {
        const tag_sM3D2Header* header = gMeshes[i]->mHeader;
        *p++ = header->mFlags;
        for (const tag_sNode* node = header->mF78; node != nullptr; node = node->mF0C) *p++ = node->mFlags;
    }
    *p++ = lab(gTitleText);
    if (gTitleText != nullptr) Dump(gTitleText, p);
    *p++ = gCursorTable.mCount;
    *p++ = gCursorTable.mEntries != nullptr;
    for (int32_t i = 0; i < gCursorTable.mCount && i < 0x20; ++i) *p++ = lab(gCursorTable.mEntries[i]);
    *p++ = lab(gUIHeap);
    *p++ = gNodeWord;
    return static_cast<int32_t>(p - out);
}

// ---- the text file reader: one call on a text; out = {result, position, cursor offset}
void tpw_us_parse(const char* text, uint32_t size, uint32_t position, int32_t op, const int32_t* flags, int64_t* out, char* string) {
    sFileParse parse;
    parse.mCursor = const_cast<char*>(text) + position;
    parse.mPos = position;
    parse.mSize = size;
    int32_t result = 0;
    string[0] = 0;
    switch (op) {
    case 0: result = IsComment(&parse); break;
    case 1: result = IsLetter(&parse); break;
    case 2: result = IsNumber(&parse); break;
    case 3: result = IsMiscChar(&parse); break;
    case 4: result = IsWhiteSpace(&parse); break;
    case 5: result = SkipWhitespace(&parse); break;
    case 6: result = GetString(&parse, string, flags[0], flags[1], flags[2], flags[3], flags[4]); break;
    }
    out[0] = result;
    out[1] = parse.mPos;
    out[2] = parse.mCursor - text;
}

// ---- rectangles, regions, cursors and the cursor table: scripts over two rectangles, a
// region, a table and the cursors made for it. out: what the operation answers.
namespace {
CoolRect* gRects[2];
CoolRegion* gRegion;
CursorTable* gTable;
alignas(16) unsigned char gTableStore[sizeof(CursorTable)];
InterfaceCursor* gCursors[6];
}
void tpw_us_geo_begin() {
    gRects[0] = gRects[1] = nullptr;
    gRegion = nullptr;
    gTable = nullptr;
    for (auto& cursor : gCursors) cursor = nullptr;
}
int64_t tpw_us_geo(int32_t op, const int32_t* a, int64_t* out) {
    const auto s = [&](int i) { return static_cast<int16_t>(a[i]); };
    CoolRect* rect = gRects[a[0] & 1];
    out[0] = out[1] = out[2] = out[3] = -7777;
    int16_t v[4] = {-7, -7, -7, -7};
    switch (op) {
    case 0: gRects[a[0] & 1] = new CoolRect; return 0;
    case 1: gRects[a[0] & 1] = new CoolRect(s(1), s(2), s(3), s(4)); return 0;
    case 2:
        delete rect;
        gRects[a[0] & 1] = nullptr;
        return 0;
    case 3: rect->Set(s(1), s(2), s(3), s(4)); return 0;
    case 4:
        rect->Get((a[1] & 1) ? &v[0] : nullptr, (a[1] & 2) ? &v[1] : nullptr, (a[1] & 4) ? &v[2] : nullptr, (a[1] & 8) ? &v[3] : nullptr);
        break;
    case 5: rect->GetDims((a[1] & 1) ? &v[0] : nullptr, (a[1] & 2) ? &v[1] : nullptr); break;
    case 6: return static_cast<CoolRegion*>(rect)->PointIn(s(1), s(2));
    case 7: static_cast<CoolRegion*>(rect)->Middle((a[1] & 1) ? &v[0] : nullptr, (a[1] & 2) ? &v[1] : nullptr); break;
    case 8: return rect->RectIntersects(gRects[~a[0] & 1]);
    case 9: static_cast<CoolRegion*>(rect)->Shift(s(1), s(2)); return 0;
    case 10: static_cast<CoolRegion*>(rect)->Adjust(s(1), s(2), s(3), s(4)); return 0;
    case 11: return *static_cast<CoolRegion*>(rect) == *gRects[~a[0] & 1];
    case 12: gRegion = new CoolRegion; return 0;
    case 13:
        delete gRegion;
        gRegion = nullptr;
        return 0;
    case 14: {
        const int64_t in = gRegion->PointIn(s(1), s(2));
        gRegion->Middle(&v[0], &v[1]);
        gRegion->Shift(s(1), s(2));
        gRegion->Adjust(s(1), s(2), s(3), s(4));
        for (int i = 0; i < 4; ++i) out[i] = v[i];
        return in * 2 + (*gRegion == *gRegion);
    }
    case 20: gTable = new (gTableStore) CursorTable; return 0;
    case 21: return gTable->Allocate(static_cast<uint16_t>(a[1]));
    case 22: gTable->DeAllocate(); return 0;
    case 23: {
        InterfaceCursor*& cursor = gCursors[a[1] % 6];
        if (a[3] && cursor == nullptr) cursor = new InterfaceCursor;
        const int32_t result = gTable->SetEntry(static_cast<uint16_t>(a[2]), cursor);
        if (result) cursor = nullptr;   // the table's now
        return result;
    }
    case 24: return lab(gTable->GetEntry(static_cast<uint16_t>(a[1])));
    case 25:
        gTable->~CursorTable();
        gTable = nullptr;
        return 0;
    case 26: {   // a cursor of the table: every member
        InterfaceCursor* cursor = gTable->GetEntry(static_cast<uint16_t>(a[1]));
        if (cursor == nullptr) return -1;
        out[0] = cursor->GetVisible();
        cursor->SetHot(static_cast<char>(a[2]), static_cast<char>(a[3]));
        if (a[4] & 1) cursor->Hide();
        if (a[4] & 2) cursor->Show();
        cursor->SetData(nullptr, 'x');
        cursor->Use(3);
        cursor->Draw(1, 2);
        cursor->Animate(5);
        out[1] = cursor->GetVisible();
        out[2] = cursor->mHotX;
        out[3] = cursor->mHotY;
        return cursor->mF0C;
    }
    }
    for (int i = 0; i < 4; ++i) out[i] = v[i];
    return 0;
}
// The rectangles' and the table's state.
int32_t tpw_us_geo_state(int64_t* out) {
    int64_t* p = out;
    for (const CoolRect* rect : gRects) {
        *p++ = rect != nullptr;
        if (rect != nullptr) {
            *p++ = rect->mLeft;
            *p++ = rect->mTop;
            *p++ = rect->mRight;
            *p++ = rect->mBottom;
        }
    }
    *p++ = gTable != nullptr;
    if (gTable != nullptr) {
        *p++ = gTable->mCount;
        for (int32_t i = 0; i < gTable->mCount && i < 16; ++i) *p++ = lab(gTable->mEntries[i]);
    }
    return static_cast<int32_t>(p - out);
}
}
