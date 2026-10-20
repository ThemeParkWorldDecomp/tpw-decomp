// Test library for nine small lobby, interface and ESP sprite functions (unit v51046; libtpw_unit_OnlineIslandParkDownloadCompleteCallback_2.dylib,
// tests/test_OnlineIslandParkDownloadCompleteCallback_2.py), main app:
//   src/CLBScreenComponentOnlineIslands_GotoNextIsland_OnlineIslandParkDownloadCompleteCallback_2.cpp
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues. The globals of other
// files the unit reads are defined here. Objects the unit only points at are forged blocks of the arena; the download globe's virtual IsVisible is
// reached through a vtable made here.
#define TPW_GLOBE_SHOWHIDE_REAL
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "TbDynamicStringTemplate.h"
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "World.h"
#include "tb_printf_opengl.h"

using namespace calllog;

void UIMain_DestroyString(void* object);
namespace mainmenu_callee {
CFSOnlineSystem* NewOnlineSystem();
}
extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count);

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
CWorld* gGameWorld;
ESPSpriteGroup gESPGroups[kESPGroups];
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
COpenGLPrintf gOpenGLPrintf;

// Objects the program constructs; the test never constructs them, only the unit's sinit does (through the fake constructors), so the storage
// is raw bytes under the same symbol names.
extern "C" {
alignas(16) uint8_t tpw_s_folder[64] __asm__("_gLobbyFolder");
alignas(16) uint8_t tpw_s_file[64] __asm__("_gLobbyDescriptorFile");
alignas(16) uint8_t tpw_s_hl[sizeof(CSpriteList) * 16] __asm__("_gHLSpriteLists");
alignas(16) uint8_t tpw_s_esplist[sizeof(CSpriteList)] __asm__("_gESPGlobalList");
alignas(16) uint8_t tpw_s_fileinfo[sizeof(bfl::FileSystem::CFileInfo)] __asm__("_gESPFileInfo");
}

namespace {

void* gFakeVT[48];   // the download globe's vtable: slot 0x2c (IsVisible) answers from the queue, the others stay unused

[[noreturn]] void Unused() { __builtin_trap(); }

int32_t FakeIsVisible(void* self) {
    ev("IsVisible", {lab(self)});
    int32_t v = static_cast<int32_t>(ret("IsVisible"));
    return v;
}

// The vtable slot (in pointers) of CLBScreenComponent::IsVisible, from a pointer to member: the Itanium ABI makes it 1 + the byte offset.
size_t IsVisibleSlot() {
    auto pmf = &CLBScreenComponent::IsVisible;
    struct Raw {
        intptr_t ptr;
        intptr_t adj;
    } raw;
    static_assert(sizeof pmf == sizeof raw, "pointer to member layout");
    std::memcpy(&raw, &pmf, sizeof raw);
    return static_cast<size_t>(raw.ptr - 1) / sizeof(void*);
}

}  // namespace

// ---- the fakes
void UIMain_DestroyString(void*) {}
void UIMain_DestroyWString(void*) {}

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    ev("construct_array", {lab(array), lab(ctor), lab(dtor), size, count});
}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

// The wide string's vtable needs every virtual defined; none is called.
#define TPW_STR_VIRTUALS(C)                                                                                                      \
    template <>                                                                                                                  \
    void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                                                    \
    template <>                                                                                                                  \
    void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                                                    \
    template <>                                                                                                                  \
    void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                                                       \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}                                                                    \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }                \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; }        \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }                               \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }                               \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }               \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }                              \
    template <>                                                                                                                  \
    TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
TPW_STR_VIRTUALS(char16_t)
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("NString", {lab(this)}, {text});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    char buffer[256];
    size_t n = 0;
    while (text[n] != 0 && n < sizeof buffer - 1) {
        buffer[n] = static_cast<char>(text[n]);
        ++n;
    }
    buffer[n] = 0;
    ev("WString", {lab(this)}, {buffer});
}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() { ev("CSpriteList", {lab(this)}); }
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 0; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }

void COpenGLPrintf::printf_xy(int32_t x, int32_t y, char* text) { ev("printf_xy", {lab(this), x, y}, {text}); }
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) { ev("PlayAnim", {lab(this), anim, loop}); }
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }
void CLBScreenComponentOnlineGlobe::Show() { ev("OnlineGlobeShow", {lab(this)}); }
void CLBScreenComponentOnlineIslands::DownloadFailFunction() { ev("DownloadFail", {lab(this)}); }
void CLBScreenComponentOnlineIslands::HandleOnlineBacktrack() { ev("OnlineBacktrack", {lab(this)}); }
void CLBScreenComponentOnlineIslands::CreateParks(TbPtrList<CFSOnlinePark>* parks) { ev("CreateParks", {lab(this), lab(parks)}); }
void CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack() { ev("GlobeBacktrack", {lab(this)}); }
void OnlineIslandsSlot4c(CLBScreenComponentOnlineIslands* screen) { ev("Slot4c", {lab(screen)}); }
void CreateWorldLobbyController(bool publish) { ev("CreateWorldLobbyController", {publish ? 1 : 0}); }
void CreateOnlineCityInfoOverlay() { ev("CreateOnlineCityInfoOverlay"); }

namespace mainmenu_callee {
// operator new(0x404), then the constructor (which the Mac runs only for a non-NULL block): the new system's word 0x384 comes from the queue.
CFSOnlineSystem* NewOnlineSystem() {
    ev("operator_new", {0x404});
    alignas(16) static uint8_t block[0x1000];
    static_assert(sizeof(CFSOnlineSystem) <= sizeof block, "block");
    std::memset(block, 0, sizeof block);
    CFSOnlineSystem* system = reinterpret_cast<CFSOnlineSystem*>(block);
    ev("CFSOnlineSystem", {newlab(system)});
    system->mF384 = static_cast<int32_t>(ret("CFSOnlineSystem_f384"));
    return system;
}
}  // namespace mainmenu_callee

alignas(16) static uint8_t gObjs[8][0x800];   // fixed objects the test points at (their labels are fixed in the log)

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_x_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"wide", gLobbyParserWide},
        {"wideNode", gLobbyParserWideNode},
        {"folder", &gLobbyFolder},
        {"folderNode", gLobbyFolderNode},
        {"file", &gLobbyDescriptorFile},
        {"fileNode", gLobbyDescriptorFileNode},
        {"wdtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"ndtor", reinterpret_cast<const void*>(&UIMain_DestroyString)},
        {"hlArray", gHLSpriteLists},
        {"ctorGlue", reinterpret_cast<const void*>(&HLSpriteListCtorGlue)},
        {"dtorGlue", reinterpret_cast<const void*>(&HLSpriteListDtorGlue)},
        {"arrDtor", gHLSpriteArrayDtor},
        {"arrNode", gHLSpriteArrayNode},
        {"espList", &gESPGlobalList},
        {"espNode", gESPGlobalListNode},
        {"fileInfo", &gESPFileInfo},
        {"fiDtor", gESPFileInfoDtor},
        {"fiNode", gESPFileInfoNode},
        {"printf", &gOpenGLPrintf},
        {"o0", gObjs[0]}, {"o1", gObjs[1]}, {"o2", gObjs[2]}, {"o3", gObjs[3]},
        {"o4", gObjs[4]}, {"o5", gObjs[5]}, {"o6", gObjs[6]}, {"o7", gObjs[7]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_x_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_x_layout(int64_t*) { return 0; }

// ---- the world
void tpw_x_set_gonline(uint64_t system) { gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(system); }
uint64_t tpw_x_get_gonline() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineSystem)); }
void tpw_x_set_f384(uint64_t system, int32_t value) { reinterpret_cast<CFSOnlineSystem*>(system)->mF384 = value; }
int32_t tpw_x_get_f384(uint64_t system) { return reinterpret_cast<CFSOnlineSystem*>(system)->mF384; }
void tpw_x_set_screens(uint64_t screens, uint64_t advisor) {
    gLobbyScreens = reinterpret_cast<CLBScreenSystem*>(screens);
    gLobbyScreens->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(advisor);
}
void tpw_x_mk_globe(uint64_t object) {
    (void)&Unused;
    (void)IsVisibleSlot;
    for (void*& slot : gFakeVT) slot = reinterpret_cast<void*>(&FakeIsVisible);   // only IsVisible is called through it
    *reinterpret_cast<void***>(object) = gFakeVT;
}
void tpw_x_isl_setup(uint64_t screen, int32_t state, uint64_t island, uint64_t meshB) {
    auto* s = reinterpret_cast<CLBScreenComponentIslands*>(screen);
    s->mCameraState = state;
    s->mCurrent = reinterpret_cast<CLBIsland*>(island);
    if (island != 0) reinterpret_cast<CLBIsland*>(island)->mMeshB = reinterpret_cast<CLBMeshInstance*>(meshB);
}
int32_t tpw_x_isl_state(uint64_t screen) { return reinterpret_cast<CLBScreenComponentIslands*>(screen)->mCameraState; }
void tpw_x_set_world(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(world); }
void tpw_x_set_group(int32_t index, int32_t first) { gESPGroups[index].mFirst = first; }
void tpw_x_set_sprite(int32_t index, uint64_t sprite, int32_t actions) {
    gESPSprites[index] = reinterpret_cast<ESP_CSprite*>(sprite);
    reinterpret_cast<ESP_CSprite*>(sprite)->mActionCount = static_cast<uint16_t>(actions);
}
void tpw_x_fps_fill(uint64_t object) { std::memset(reinterpret_cast<CIntrface*>(object)->mF10, 0xaa, sizeof reinterpret_cast<CIntrface*>(object)->mF10); }
void tpw_x_fps_buf(uint64_t object, uint8_t* out) { std::memcpy(out, reinterpret_cast<CIntrface*>(object)->mF10, 0x14); }
void tpw_x_fi_fill() { std::memset(tpw_s_fileinfo, 0xaa, sizeof tpw_s_fileinfo); }
void tpw_x_fi_state(int64_t* out) {
    auto* info = reinterpret_cast<bfl::FileSystem::CFileInfo*>(tpw_s_fileinfo);
    out[0] = info->mName == info->mNameBuffer ? 1 : 0;
    out[1] = static_cast<uint8_t>(info->mNameBuffer[0]);
}

// ---- the functions
void tpw_x_dlcb(int32_t error, uint64_t context) {
    OnlineIslandParkDownloadCompleteCallback(error, reinterpret_cast<const void*>(context));
}
void tpw_x_top10(int32_t error, uint64_t context, uint64_t list) {
    PostGetTop10ParksListCallbackFunction(error, reinterpret_cast<const void*>(context), reinterpret_cast<TbPtrList<CFSOnlinePark>*>(list));
}
uint32_t tpw_x_isl_cancel(uint64_t screen) { return reinterpret_cast<CLBScreenComponentIslands*>(screen)->ProcessCancelIslands(); }
uint32_t tpw_x_globe_cancel(uint64_t globe) {
    return reinterpret_cast<CLBScreenComponentDownloadGlobe*>(globe)->CLBScreenComponentDownloadGlobe::ProcessCancel();
}
void tpw_x_globe_show(uint64_t globe) { reinterpret_cast<CLBScreenComponentDownloadGlobe*>(globe)->CLBScreenComponentDownloadGlobe::Show(); }
void tpw_x_fps(uint64_t object, float fps) { reinterpret_cast<CIntrface*>(object)->DrawFPSCounter(fps); }
int32_t tpw_x_esp(int32_t set, int32_t sprites) { return ESP_GetRandomActionFrom(static_cast<ESP_Sprites>(set), sprites); }
void tpw_x_sinit_lobby() { __sinit_lobby_parser_cpp(); }
void tpw_x_sinit_hl() { __sinit_HLSprite_cpp(); }

}  // extern "C"
