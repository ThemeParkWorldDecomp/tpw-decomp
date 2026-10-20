// Fakes and test entry points for unit w62, the lobby's small leaves part 4 (libtpw_unit_LobbyLeaves4.dylib, driven by
// tests/test_LobbyLeaves4.py; see calllog.h):
//   src/lobby_globe.cpp  CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe 0x100ac90c, the download and publish globes' Init
//   src/FS_Online.cpp    CFSOnlineSystem::Tick 0x10170f80, StopProcessing 0x10171404
//   src/email.cpp        Outbox::Outbox 0x10203940, AddressBook::AddressBook 0x102064cc, bthread_mutex::lock 0x10200d8c, unlock 0x10200de0
//   src/Game.cpp         CGame::DeInit 0x1014965c
//   src/InGame.cpp       CInGame::Init 0x1014b2bc
// Every callee outside the unit is a fake that records the same event as the PowerPC side's stub: the screen base class's constructor, the
// nine online members, FuckOffUIProgressBar, ExitLevel, AssignMouseMode, CInGame::Start and the two critical section calls. The heap
// allocation of the globe's two lists is a recorded nothrow new whose answer (NULL or a block) the case scripts. The rest of src/Game.cpp
// and src/InGame.cpp that the library has to link (resources, strings, the game object) is quiet.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "Game.h"
#include "InGame.h"
#include "InterfaceFont.h"
#include "Mouse.h"
#include "SkyObjects.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"
#include "UI_pump.h"
#include "calllog.h"
#include "email.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "ui_ErrorDisplay.h"

using namespace calllog;
using String = TbDynamicStringTemplate<char>;

// ---- what src/Game.cpp links to and these tests never reach (quiet)
#ifdef __APPLE__
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
extern unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
#else
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("gGame");
extern unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
#endif
alignas(16) unsigned char gGameStorage[sizeof(CGame)];
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)];

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}
void FS_AddDirectorySeparator(String&) {}
bool TPStrGtr(const String&, const String&) { return false; }
CSkyObjects::~CSkyObjects() {}
bfl::FileSystem::CFileStorage::CFileStorage() {}
bfl::FileSystem::CFileStorage::~CFileStorage() {}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(-1); }
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(String&, const String&) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
CTPResourceManager::CTPResourceManager() {}
CTPResourceManager::~CTPResourceManager() {}
int32_t CTPResourceManager::RegisterResource(CTPBaseResource*) { return 1; }
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, const String&, bfl::FileSystem::CFileStorage&, uint32_t) { return 0; }
#define RESOURCE(Class)                                                                  \
    Class::Class(const String& name, const String& path) : CTPBaseResource(name, path) {} \
    Class::~Class() {}
CTPTextureResource::CTPTextureResource(const String& name, const String& path) : CTPBaseResource(name, path) {}
CTPTextureResource::~CTPTextureResource() {}
CTPIntermediate::~CTPIntermediate() {}
CTPAdvisorResource::CTPAdvisorResource(const String& name, const String& path) : CTPIntermediate(name, path) {}
CTPAdvisorResource::~CTPAdvisorResource() {}
CTPUIResource::CTPUIResource(const String& name, const String& path) : CTPTextureResource(name, path) {}
CTPUIResource::~CTPUIResource() {}
RESOURCE(CTPSoundResource)
RESOURCE(CTPFontResource)
RESOURCE(CTPSpriteResource)
RESOURCE(CTPLanguageResource)
RESOURCE(CTPFMVResource)
#undef RESOURCE
void CTPBaseResource::AddDiscStorage(const String&) {}
void CTPBaseResource::AddWADStorage(const String&) {}
void CTPBaseResource::AddAllStorages(const String&) {}
void CTPBaseResource::AddLocalStorage(const String&) {}

// ---- the screen base class: the constructor records and sets the visible word (the real one also registers the screen with the
// lobby's screen system, which this unit does not have); the rest of its virtuals and the globes' are never called
CLBScreenComponent::CLBScreenComponent() : mVisible(0) { ev("Screen.ctor", {newlab(this)}); }
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return mVisible; }
CLBScreenComponentOnlineGlobe::~CLBScreenComponentOnlineGlobe() {}
void CLBScreenComponentOnlineGlobe::Render() {}
void CLBScreenComponentOnlineGlobe::Close() {}
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }
// The globe's vtable (emitted here) names Tick and WindowMessagesFn, unit v5571's (src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Tick.cpp).
void CLBScreenComponentOnlineGlobe::Tick() {}
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe() {}
void CLBScreenComponentDownloadGlobe::Tick() {}
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
uint32_t CLBScreenComponentDownloadGlobe::ProcessCancel() { return 0; }
void CLBScreenComponentDownloadGlobe::Show() {}
void CLBScreenComponentDownloadGlobe::Hide() {}
CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe() {}
void CLBScreenComponentPublishGlobe::Tick() {}
void CLBScreenComponentPublishGlobe::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBScreenComponentPublishGlobe::Show() {}
void CLBScreenComponentPublishGlobe::Hide() {}

// ---- the globe's two lists come from a recorded nothrow new: the queue "New" answers 0 (a NULL, the Mac's failed allocation) or nonzero
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ev("operator_new");
    if (ret("New", 1) == 0) return nullptr;
    return std::malloc(size);
}

// ---- the online system's members
#define ONLINE(Name) \
    void CFSOnlineSystem::Name() { ev("CFSOnlineSystem." #Name, {lab(this)}); }
ONLINE(InitNews)
ONLINE(InitCity)
ONLINE(InitUpload)
ONLINE(InitFTPDownload)
ONLINE(TickLogin)
ONLINE(TickNews)
ONLINE(TickCity)
ONLINE(TickUpload)
ONLINE(TickFTPDownload)
#undef ONLINE
void FuckOffUIProgressBar() { ev("FuckOffUIProgressBar"); }

// ---- the game's exit: ExitLevel records and then does what the case says to the mouse (the queue "Exit": 0 nothing, 1 clears gMouse,
// 2 makes gMouse the case's second mouse)
CMouse* gMouse;
CMouse* gSpareMouse;
void CGame::ExitLevel() {
    ev("ExitLevel", {lab(this)});
    const int64_t action = ret("Exit", 0);
    if (action == 1) gMouse = nullptr;
    if (action == 2) gMouse = gSpareMouse;
}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
void CInGame::Start() { ev("CInGame.Start"); }

// ---- the critical section: the handle's address is recorded as a label
void NS_MacDoze::EnterCriticalSection(void** handle) { ev("EnterCriticalSection", {lab(handle)}); }
void NS_MacDoze::LeaveCriticalSection(void** handle) { ev("LeaveCriticalSection", {lab(handle)}); }

namespace {
constexpr int kGlobeWords = 31;   // 0x7c bytes in words
alignas(16) unsigned char gGlobeStore[sizeof(CLBScreenComponentDownloadGlobe)];
alignas(16) unsigned char gOnlineStore[sizeof(CFSOnlineSystem)];
alignas(16) unsigned char gOutboxStore[sizeof(Outbox)];
alignas(16) unsigned char gBookStore[sizeof(AddressBook)];
alignas(16) unsigned char gGameObject[sizeof(CGame)];
bthread_mutex gMutexes[2];

uint32_t Low(const void* pointer) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)); }

template<class T> T* Ptr(uint32_t word) { return reinterpret_cast<T*>(static_cast<uintptr_t>(word)); }

// The globe's fields as the Mac's 31 words (the vtable word is not part of it; pointers keep their low 32 bits).
void UnpackGlobe(CLBScreenComponentOnlineGlobe* g, const uint32_t* w) {
    g->mVisible = static_cast<int32_t>(w[1]);
    g->mGlobes = Ptr<TbTList<CLBGlobe*, CLBGlobe*>>(w[2]);
    std::memcpy(g->mF0C, w + 3, sizeof g->mF0C);
    std::memcpy(&g->mF1C, w + 7, 4);
    std::memcpy(&g->mF20, w + 8, 4);
    std::memcpy(&g->mF24, w + 9, 4);
    std::memcpy(&g->mF28, w + 10, 4);
    std::memcpy(&g->mF2C, w + 11, 4);
    std::memcpy(&g->mF30, w + 12, 4);
    std::memcpy(g->mF34, w + 13, 4);
    std::memcpy(&g->mF38, w + 14, 4);
    std::memcpy(&g->mF3C, w + 15, 4);
    std::memcpy(&g->mF40, w + 16, 4);
    std::memcpy(g->mF44, w + 17, sizeof g->mF44);
    g->mCityFlags = Ptr<TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>>(w[27]);
    g->mF70 = static_cast<int32_t>(w[28]);
    g->mF74 = static_cast<int32_t>(w[29]);
    g->mF78 = static_cast<int32_t>(w[30]);
}

void PackGlobe(const CLBScreenComponentOnlineGlobe* g, uint32_t* w, bool pointersAsFlags) {
    w[0] = 0;
    w[1] = static_cast<uint32_t>(g->mVisible);
    w[2] = pointersAsFlags ? g->mGlobes != nullptr : Low(g->mGlobes);
    std::memcpy(w + 3, g->mF0C, sizeof g->mF0C);
    std::memcpy(w + 7, &g->mF1C, 4);
    std::memcpy(w + 8, &g->mF20, 4);
    std::memcpy(w + 9, &g->mF24, 4);
    std::memcpy(w + 10, &g->mF28, 4);
    std::memcpy(w + 11, &g->mF2C, 4);
    std::memcpy(w + 12, &g->mF30, 4);
    std::memcpy(w + 13, g->mF34, 4);
    std::memcpy(w + 14, &g->mF38, 4);
    std::memcpy(w + 15, &g->mF3C, 4);
    std::memcpy(w + 16, &g->mF40, 4);
    std::memcpy(w + 17, g->mF44, sizeof g->mF44);
    w[27] = pointersAsFlags ? g->mCityFlags != nullptr : Low(g->mCityFlags);
    w[28] = static_cast<uint32_t>(g->mF70);
    w[29] = static_cast<uint32_t>(g->mF74);
    w[30] = static_cast<uint32_t>(g->mF78);
}

// A list as the test compares it: made (1), vtable right (the host's is by construction), head and tail NULL, count.
template<class List> void PackList(const List* list, uint32_t* out) {
    out[0] = list != nullptr;
    out[1] = list != nullptr;
    out[2] = list != nullptr && list->mHead == nullptr;
    out[3] = list != nullptr && list->mTail == nullptr;
    out[4] = list != nullptr ? static_cast<uint32_t>(list->mCount) : 0;
}
}  // namespace

// The outbox's destructor is untranslated; the test library's is the compiler's.
Outbox::~Outbox() = default;

extern "C" {
// ---- the globe. kind: 0 the constructor, 1 the download globe's Init, 2 the publish globe's Init. `in` is the object's 31 words before the
// call (the constructor keeps what it does not write); out receives the 31 words, then the two lists (5 words each: made, vtable right,
// head NULL, tail NULL, count). A pointer word is a made-flag after the constructor and the low 32 bits otherwise. The lists the
// constructor made are freed here (the calls were recorded already).
void tpw_ll4_globe(int32_t kind, const uint32_t* in, uint32_t* out) {
    auto* globe = reinterpret_cast<CLBScreenComponentDownloadGlobe*>(gGlobeStore);
    UnpackGlobe(globe, in);
    if (kind == 0) new (gGlobeStore) CLBScreenComponentDownloadGlobe;
    else if (kind == 1) globe->CLBScreenComponentDownloadGlobe::Init();
    else reinterpret_cast<CLBScreenComponentPublishGlobe*>(gGlobeStore)->CLBScreenComponentPublishGlobe::Init();
    PackGlobe(globe, out, kind == 0);
    PackList(kind == 0 ? globe->mGlobes : nullptr, out + kGlobeWords);
    PackList(kind == 0 ? globe->mCityFlags : nullptr, out + kGlobeWords + 5);
    if (kind == 0) {
        const int32_t on = tpw_cl_on;
        tpw_cl_on = 0;
        delete globe->mGlobes;
        delete globe->mCityFlags;
        tpw_cl_on = on;
    }
}

// ---- the online system, an object of the test's bytes labelled 1: ops are 0 Tick, 1 StopProcessing. Answers whether the bytes are still
// all `fill`.
int32_t tpw_ll4_online(int32_t which, int32_t fill) {
    std::memset(gOnlineStore, fill, sizeof gOnlineStore);
    tpw_cl_label(1, gOnlineStore);
    auto* system = reinterpret_cast<CFSOnlineSystem*>(gOnlineStore);
    if (which == 0) system->Tick();
    else system->StopProcessing();
    for (unsigned char byte : gOnlineStore)
        if (byte != static_cast<unsigned char>(fill)) return 0;
    return 1;
}

// ---- the outbox over bytes of `fill`: out gets the short at +0x54, the words at +0x58 and +0x5c, the bytes the constructor leaves (0x1c..0x27
// as three words, 0x4c as one), then five flags (the strings are empty) and four (the lists are empty).
void tpw_ll4_outbox(int32_t fill, uint32_t* out) {
    std::memset(gOutboxStore, fill, sizeof gOutboxStore);
    auto* box = new (gOutboxStore) Outbox;
    out[0] = box->mF54;
    out[1] = static_cast<uint32_t>(box->mF58);
    out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(box->mEmail));   // the word at +0x5c is a pointer now (unit w90)
    std::memcpy(out + 3, box->mF1C, 12);
    std::memcpy(out + 6, box->mF4C, 4);
    out[7] = box->mF00.empty();
    out[8] = box->mF04.empty();
    out[9] = box->mF08.empty();
    out[10] = box->mF18.empty();
    out[11] = box->mF50.empty();
    out[12] = box->mF0C.empty();
    out[13] = box->mF28.empty();
    out[14] = box->mF34.empty();
    out[15] = box->mF40.empty();
    box->~Outbox();
}

// ---- the address book: whether its list is empty.
int32_t tpw_ll4_book(int32_t fill) {
    std::memset(gBookStore, fill, sizeof gBookStore);
    auto* book = new (gBookStore) AddressBook;
    const int32_t empty = book->mAddresses.empty();
    book->~AddressBook();
    return empty;
}

// ---- the two mutexes (addresses for the labels, the handle word, the calls)
void* tpw_ll4_mutex(int32_t which) { return &gMutexes[which & 1]; }
void tpw_ll4_mutex_set(int32_t which, uint32_t handle) { gMutexes[which & 1].mHandle = Ptr<void>(handle); }
uint32_t tpw_ll4_mutex_get(int32_t which) { return Low(gMutexes[which & 1].mHandle); }
void tpw_ll4_mutex_run(int32_t which, int32_t lock) {
    if (lock != 0) gMutexes[which & 1].lock();
    else gMutexes[which & 1].unlock();
}

// ---- CGame::DeInit: a game object (label 1), a mouse (label 2, when `mouse`) in gMouse and a spare (label 3, when `spare`) that ExitLevel
// may put in gMouse. out: the label of gMouse afterwards (0 when cleared), then whether each mouse is still alive. Mice left alive are freed
// with the log off.
void tpw_ll4_deinit(int32_t mouse, int32_t spare, int32_t* out) {
    CMouse* first = mouse != 0 ? new CMouse : nullptr;
    CMouse* second = spare != 0 ? new CMouse : nullptr;
    gMouse = first;
    gSpareMouse = second;
    tpw_cl_label(1, gGameObject);
    tpw_cl_label(2, first);
    tpw_cl_label(3, second);
    reinterpret_cast<CGame*>(gGameObject)->DeInit();
    out[0] = lab(gMouse);
    out[1] = first != nullptr && lab(first) > 0;
    out[2] = second != nullptr && lab(second) > 0;
    const int32_t on = tpw_cl_on;
    tpw_cl_on = 0;
    if (out[1] != 0) delete first;
    if (out[2] != 0) delete second;
    tpw_cl_on = on;
    gMouse = gSpareMouse = nullptr;
}

// ---- CInGame::Init, `times` times
void tpw_ll4_ingame(int32_t times) {
    for (int32_t i = 0; i < times; ++i) CInGame::Init();
}
}
