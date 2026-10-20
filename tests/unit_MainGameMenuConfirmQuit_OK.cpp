// Test library for the main menu's quit answer, item callback and fade, and CFSLoadSave::SaveInternal / GetFirstSaveFileDetails
// (unit v5958; libtpw_unit_MainGameMenuConfirmQuit_OK.dylib, tests/test_MainGameMenuConfirmQuit_OK.py), main app:
//   src/ui_iterators_MainGameMenuConfirmPublish_OK_MainGameMenuConfirmQuit_OK.cpp
//
// The callees outside the unit are fakes that record what the real ones read (the name and the arguments, objects as labels) and answer
// from the test's queues. The strings carry a token in their text pointer (a 32-bit number the fakes mix: an append mixes the two
// tokens in order, an assignment copies one), so a wrong string, a wrong order or a wrong field shows in the next call that reads the
// pointer. Nothing here follows a text pointer except the file names the directory fake writes. The list functions are the
// header's templates, run for real on both sides (their calls to operator new and delete are logged). The globals of other files the
// unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "bfl_FileSystem.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem;
CFSUserInfo* gUserInfo;
UIMenu* gMainMenu;
CSaveLoad gSaveLoad;

namespace {

typedef TbDynamicStringTemplate<char16_t> WideString;
typedef TbDynamicStringTemplate<char> NarrowString;
typedef TbTList<TFSLoadSaveSaveFileDetails*, TFSLoadSaveSaveFileDetails*> DetailsList;

bool gActive = false;          // true while one of the unit's functions runs: the fakes record only then
bool gReturnCtor = false;      // the next wide constructor is the result of ReturnUsersPathWithTheme
int64_t gReturnUser = 0;

#define EV(...) \
    do { \
        if (gActive) ev(__VA_ARGS__); \
    } while (0)

int64_t I(int64_t v) { return v; }
int64_t LB(const void* p) { return gActive ? static_cast<int64_t>(lab(p)) : 0; }
int64_t NL(const void* p) { return gActive ? static_cast<int64_t>(newlab(p)) : 0; }

uint32_t Mix(uint32_t h, uint32_t o) { return h * 31u + o + 1u; }
uint32_t Th(const char* s) {
    uint32_t h = 0;
    for (; *s != 0; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}
template <class Str> uint32_t Tok(const Str& s) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mData)); }
template <class Str> void SetTok(Str& s, uint32_t t) {
    s.mData = reinterpret_cast<decltype(s.mData)>(static_cast<uintptr_t>(t));
}
uint32_t PtrTok(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

// The mapping from the host's object sizes to the Mac's, for the log of operator new.
int64_t MacSize(size_t n) {
    if (n == sizeof(CFSOnlineSystem)) return 0x404;
    if (n == sizeof(CFSUserInfo)) return 0xd4;
    if (n == sizeof(TFSLoadSaveSaveFileDetails)) return 0x1c;
    if (n == sizeof(DetailsList::CNode)) return 12;
    return static_cast<int64_t>(n);
}

const char* const kNames[6] = {"Alpha.TPWS", "beta", "Gamma Park.TPWS", "d.TPWS", "", "Z.TPW"};
uint32_t Stamp0(int32_t k) { return 0x11110000u + static_cast<uint32_t>(k) * 0x01010101u; }
uint32_t Stamp1(int32_t k) { return 0x2000u + static_cast<uint32_t>(k) * 0x3f1u + ((k & 1) ? 0x80000000u : 0u); }

void FillEntry(bfl::FileSystem::CFileInfo& info, int32_t k) {
    int32_t i = ((k % 6) + 6) % 6;
    std::strcpy(info.mNameBuffer, kNames[i]);
    uint32_t t0 = Stamp0(i), t1 = Stamp1(i);
    std::memcpy(info.mF08, &t0, 4);
    std::memcpy(info.mF08 + 4, &t1, 4);
}

}  // namespace

// ---- operator new and delete: logged while a function runs (the Mac's sizes), memory from the arena
#if 0
void* operator new(size_t n) {
    void* p = tpw::ArenaAllocZeroed(n == 0 ? 4 : n);
    EV("new", {I(MacSize(n))});
    return p;
}
void operator delete(void* p) noexcept { EV("delete", {LB(p)}); }
void operator delete(void* p, size_t) noexcept { EV("delete", {LB(p)}); }
#endif

// ---- the strings (wide: ctor, copy ctor, dtor, =, +=; narrow: ctor, ctor(text), dtor, =, +=, Length, Left); the rest of the
// virtual table is here only so that the table links
template <> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    if (gReturnCtor) {
        gReturnCtor = false;
        SetTok(*this, static_cast<uint32_t>(ret("userspath")));
        EV("ReturnUsersPathWithTheme", {NL(this), gReturnUser});
    } else {
        EV("wctor", {NL(this)});
    }
}
template <> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const TbDynamicStringTemplate<char16_t>& other) {
    mLength = 0;
    mData = other.mData;
    mCapacity = 0;
    mGrowBy = 0;
    EV("wcopyctor", {NL(this), LB(&other)});
}
template <> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { EV("wdtor", {LB(this)}); }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate<char16_t>& other) {
    EV("wassign", {LB(this), LB(&other)});
    mData = other.mData;
    return *this;
}
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>& other) {
    EV("wappend", {LB(this), LB(&other)});
    SetTok(*this, Mix(Tok(*this), Tok(other)));
    return *this;
}
template <> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

template <> TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    EV("cctor", {NL(this)});
}
template <> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    mLength = 0;
    mCapacity = 0;
    mGrowBy = 0;
    SetTok(*this, Th(text));
    EV("cctor_cstr", {NL(this)}, {text});
}
template <> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { EV("cdtor", {LB(this)}); }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>& other) {
    EV("cassign_dyn", {LB(this), LB(&other)});
    mData = other.mData;
    return *this;
}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>& other) {
    EV("cassign_base", {LB(this), LB(&other)});
    mData = other.mData;
    return *this;
}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& other) {
    EV("cappend", {LB(this), LB(&other)});
    SetTok(*this, Mix(Tok(*this), Tok(other)));
    return *this;
}
template <> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

template <> int32_t TbStringBase<char>::Length() const {
    EV("Length", {LB(this)});
    return static_cast<int32_t>(ret("Length"));
}
template <> TbStringBase<char> TbStringBase<char>::Left(uint32_t count) const {
    EV("Left", {LB(this), I(count)});
    uint32_t token = PtrTok(mData) + count * 7u + 1u;
    return TbStringBase<char>(reinterpret_cast<char*>(static_cast<uintptr_t>(token)), 0);
}

// ---- gTPGlobals (after the specialisations: its strings use them)
// (raw zeroed storage under the symbol of the global: TPGlobals' constructor would pull in half the program)
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");

// ---- the string conversions
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    EV("UniStringToString", {LB(&out), LB(&in)});
    SetTok(out, Tok(in) * 3u + 7u);
    return 0;
}
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    EV("StringToUniString", {LB(&out), LB(&in)});
    SetTok(out, Tok(in) * 5u + 3u);
    return 0;
}

// ---- the user info, the online system, the game state, the menu
// (the constructor under its symbol: a member-wise one would pull in the constructors of the members)
extern "C" void tpw_q_userinfo_ctor(CFSUserInfo* self) __asm__("__ZN11CFSUserInfoC1Ev");
extern "C" void tpw_q_userinfo_ctor(CFSUserInfo* self) { EV("CFSUserInfo", {NL(self)}); }
void CFSUserInfo::SaveGlobalMasterSave() { EV("SaveGlobalMasterSave", {LB(this)}); }
TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    gReturnCtor = true;
    gReturnUser = LB(this);
    return TbDynamicStringTemplate<char16_t>();
}
CFSOnlineSystem::CFSOnlineSystem() { EV("CFSOnlineSystem", {NL(this)}); }
void CFSOnlineSystem::StopProcessing() { EV("StopProcessing", {LB(this)}); }
void GS_SetCurrentGameStatus(GS_GAME_STATES status) { EV("GS_Set", {I(status)}); }
void HideMainMenu() { EV("HideMainMenu", {}); }

// ---- the window
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    EV("SetFontColour", {LB(this), I(red), I(green), I(blue), I(alpha)});
}
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    EV("setExtraParam", {LB(this), I(index), I(PtrTok(value))});
    return 0;
}
void* InterfaceWindow::getExtraParam(int16_t index) {
    EV("getExtraParam", {LB(this), I(index)});
    return reinterpret_cast<void*>(static_cast<intptr_t>(static_cast<int32_t>(ret("getExtraParam"))));
}
int32_t UIMenu_DefaultItemCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    EV("DefaultCB", {LB(window), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("DefaultCB"));
}

// ---- the save
int32_t CSaveLoad::Save(const char* path, CSaveLoadHeader* header, LoadType type) {
    EV("Save", {LB(this), I(PtrTok(path)), LB(header), I(type)});
    return static_cast<int32_t>(ret("Save"));
}

// ---- the file storage
namespace bfl {
namespace FileSystem {
CFileStorage::CFileStorage() : mAccess(nullptr) { EV("CFileStorage", {NL(this)}); }
CFileStorage::~CFileStorage() { EV("CFileStorage_dtor", {LB(this)}); }
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    EV("FindFirst", {LB(this), I(PtrTok(pattern))});   // (the entry is a stack object: its address has no stable label)
    int32_t result = static_cast<int32_t>(ret("FindFirst"));
    if (result >= 0) FillEntry(info, static_cast<int32_t>(ret("entry")));
    return TbError(result);
}
TbError CFileStorage::FindNext(CFileInfo& info) {
    EV("FindNext", {LB(this)});
    int32_t result = static_cast<int32_t>(ret("FindNext"));
    if (result >= 0) FillEntry(info, static_cast<int32_t>(ret("entry")));
    return TbError(result);
}
TbError CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
const char* CFileStorage::GetRootPath() { return nullptr; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }
TbError CDiscFileStorage::Open(const char* path) {
    EV("Open", {LB(this), I(PtrTok(path))});
    return TbError(static_cast<int32_t>(ret("Open")));
}
}  // namespace FileSystem
}  // namespace bfl

bool FS_IsSubDirectory(const bfl::FileSystem::CFileInfo& info) {
    uint32_t t0, t1;
    std::memcpy(&t0, info.mF08, 4);
    std::memcpy(&t1, info.mF08 + 4, 4);
    EV("FS_IsSubDirectory", {I(Th(info.mName)), I(t0), I(t1)});
    return ret("FS_IsSubDirectory") != 0;
}

extern "C" {

// The host address of a global or of a member of gTPGlobals by the module's name for it (ADDR's keys).
uint64_t tpw_q_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem}, {"gUserInfo", &gUserInfo},
        {"gMainMenu", &gMainMenu},         {"gSaveLoad", &gSaveLoad},
        {"allfiles", &gTPGlobals.mAllFiles}, {"saveext", &gTPGlobals.mSaveExtension},
        {"wsep", &gTPGlobals.mWideSeparator},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_q_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_q_layout(int64_t* out) {
    alignas(16) static char list_buf[256];
    alignas(16) static char str_buf[256];
    CFSLoadSave* list = reinterpret_cast<CFSLoadSave*>(list_buf);
    TbStringBase<char16_t>* str = reinterpret_cast<TbStringBase<char16_t>*>(str_buf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&list->mHead) - list_buf;                 // list head
    out[n++] = reinterpret_cast<char*>(&list->mTail) - list_buf;                 // list tail
    out[n++] = reinterpret_cast<char*>(&list->mCount) - list_buf;                // list count
    out[n++] = static_cast<int64_t>(offsetof(DetailsList::CNode, mNext));       // node next
    out[n++] = static_cast<int64_t>(offsetof(DetailsList::CNode, mPrev));       // node previous
    out[n++] = reinterpret_cast<char*>(&str->mData) - str_buf;                   // the text pointer of a string
    out[n++] = static_cast<int64_t>(offsetof(TFSLoadSaveSaveFileDetails, mStampHigh));
    out[n++] = static_cast<int64_t>(offsetof(TFSLoadSaveSaveFileDetails, mStampLow));
    return n;
}

int64_t tpw_q_lab(uint64_t p) { return static_cast<int64_t>(lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p)))); }

// Builds a details object (muted) / adds it to a list (muted): the world of a case.
uint64_t tpw_q_new_details() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new TFSLoadSaveSaveFileDetails)); }
void tpw_q_make_list(uint64_t p) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(p))) CFSLoadSave; }
void tpw_q_prefill(uint64_t list, uint64_t details) {
    reinterpret_cast<CFSLoadSave*>(static_cast<uintptr_t>(list))->AddTail(reinterpret_cast<TFSLoadSaveSaveFileDetails*>(static_cast<uintptr_t>(details)));
}

// ---- the functions
uint32_t tpw_q_MainGameMenuConfirmQuit_OK(uint64_t context) {
    gActive = true;
    int32_t r = MainGameMenuConfirmQuit_OK(reinterpret_cast<void*>(static_cast<uintptr_t>(context)));
    gActive = false;
    return static_cast<uint32_t>(r);
}
uint32_t tpw_q_MainMenuItemCB(uint64_t window, int32_t message, int32_t a, int32_t b) {
    gActive = true;
    int32_t r = MainMenuItemCB(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
    gActive = false;
    return static_cast<uint32_t>(r);
}
void tpw_q_itemOnFrame(uint64_t window) {
    gActive = true;
    itemOnFrame(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)));
    gActive = false;
}
uint32_t tpw_q_SaveInternal(uint64_t list, uint64_t name, uint64_t extension) {
    gActive = true;
    int32_t r = reinterpret_cast<CFSLoadSave*>(static_cast<uintptr_t>(list))
                    ->SaveInternal(*reinterpret_cast<const WideString*>(static_cast<uintptr_t>(name)),
                                   *reinterpret_cast<const WideString*>(static_cast<uintptr_t>(extension)));
    gActive = false;
    return static_cast<uint32_t>(r);
}
uint64_t tpw_q_GetFirstSaveFileDetails(uint64_t list) {
    gActive = true;
    TFSLoadSaveSaveFileDetails* r = reinterpret_cast<CFSLoadSave*>(static_cast<uintptr_t>(list))->GetFirstSaveFileDetails();
    gActive = false;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
