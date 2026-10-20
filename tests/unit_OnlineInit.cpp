// Test library for src/FS_Online_init.cpp and src/FS_OnlineList.cpp (libtpw_unit_OnlineInit.dylib; see tests/test_OnlineInit.py):
// the online system's constructor, its Init* and Release*Connection members, TickLogin, the theme, server and ride list constructors
// and the online park's default constructor.
//
// Real: those sixteen functions, the three list classes' constructors and the inline constructors of the other lists (the header's),
// the bullfrog strings. Fakes that record: the online library's connection classes (Auth, News, City, Upload, the FTP client), the
// debug log, the clock, CompleteLogin, the "cc" info destructors and every virtual destructor of the list family.
//
// The tests drive one object at a time: tpw_oi_system makes the online system over 0xa5 bytes (constructor traced or quiet),
// tpw_oi_apply writes a randomised state into it (words, strings, connections, received lists), tpw_oi_call runs one member and
// tpw_oi_dump writes everything observable as a list of integers in the order of the slot table below. The Python side reads the
// Mac object at the same offsets in the same order. Pointers are normalised: 0, the index of the allocation the code under test made
// (1000 + n), a label of a test object, or -1.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <new>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}  // namespace
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "LbTime.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {
// Allocations made by the code under test, in order (pointers to compare by index), kept while gTrack is set. Arrays (the strings'
// buffers) are not listed. gArray catches the block of the next array made by the test (to label it).
const void* gAllocs[512];
int gAllocN;
bool gTrack;
bool gWatchArray;
void* gLastArray;

void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (array) {
        if (gWatchArray) gLastArray = block;
        return;
    }
    std::memset(block, 0xa5, size);
    if (gTrack && gAllocN < 512) gAllocs[gAllocN++] = block;
}

// Recording is off outside the member under test.
struct Quiet {
    Quiet() { tpw_cl_on = 0; }
    ~Quiet() { tpw_cl_on = 0; }
};

enum { kLabelSystem = 1, kLabelConnection = 10, kLabelHolder = 20, kLabelArray = 30, kLabelInfo = 50, kLabelList = 300 };

// The login callback the system's callback words may hold (unit w69's CompleteLogin calls it): it records both arguments.
void FakeLoginCallback(int32_t error, void* context) {
    calllog::ev("LoginCallback", {error, static_cast<int32_t>(reinterpret_cast<uintptr_t>(context))});
}

alignas(16) unsigned char gSystemBlock[sizeof(CFSOnlineSystem)];
alignas(16) unsigned char gOtherBlock[512];
CFSOnlineSystem* gSystem;
int32_t gOtherKind;   // 0 none, 1 theme list, 2 server list, 3 ride list, 4 park

// ---- the slot table: every member of the online system in the order of the Mac's offsets
struct Slot {
    char kind;       // w word, h short, p pointer, f the login callback, x a context word held in a pointer, W wide string, S narrow string, L inline list
    std::string name;
    void* addr;
};

std::vector<Slot> BuildSlots(CFSOnlineSystem& s) {
    std::vector<Slot> slots;
#define SLOT(k, member) slots.push_back({k, #member, &s.member})
#define PAD(member, words)                                                                          \
    for (int32_t i = 0; i < (words); ++i) {                                                         \
        char name[40];                                                                              \
        std::snprintf(name, sizeof name, #member "[%d]", i);                                        \
        slots.push_back({'w', name, reinterpret_cast<unsigned char*>(&s.member) + 4 * i});         \
    }
    SLOT('p', mLogin);
    SLOT('f', mLoginCallback);
    SLOT('x', mLoginContext);
    SLOT('W', mLoginName);
    SLOT('W', mLoginPassword);
    SLOT('w', mF34);
    SLOT('w', mF38);
    SLOT('W', mLoginEmail);
    SLOT('w', mF50);
    PAD(mF54, 3)
    SLOT('w', mLoggedIn);
    SLOT('w', mLoginDone);
    SLOT('w', mLoginErrorID);
    SLOT('p', mNews);
    SLOT('w', mF70);
    SLOT('w', mNewsRequest1);
    SLOT('w', mNewsDone1);
    SLOT('S', mNewsText);
    SLOT('w', mNewsRequest2);
    SLOT('w', mNewsDone2);
    SLOT('W', mF98);
    SLOT('w', mNewsRequest3);
    SLOT('w', mNewsDone3);
    SLOT('W', mFB4);
    SLOT('f', mLoginCallbackKept);
    SLOT('x', mLoginContextKept);
    SLOT('w', mNewsFinished);
    SLOT('p', mCity);
    SLOT('w', mCityClock);
    SLOT('w', mCityListsWanted);
    SLOT('p', mCityList);
    SLOT('p', mParksTop10);
    SLOT('p', mParksInCity);
    SLOT('p', mParksByPlayer);
    SLOT('p', mParksByID);
    SLOT('p', mParksInCity2);
    SLOT('p', mThemeList);
    SLOT('p', mServerList);
    SLOT('p', mRideList);
    SLOT('p', mUpload);
    SLOT('w', mF108);
    SLOT('S', mF10C);
    PAD(mF120, 0x200 / 4)
    SLOT('w', mF320);
    SLOT('w', mUploadActive);
    SLOT('w', mUploadDone);
    SLOT('w', mUploadFailed);
    SLOT('S', mF330);
    SLOT('w', mF344);
    SLOT('W', mF348);
    SLOT('W', mF35C);
    SLOT('w', mF370);
    SLOT('w', mF374);
    SLOT('w', mF378);
    SLOT('w', mF37C);
    SLOT('w', mF380);
    SLOT('w', mF384);
    SLOT('w', mF388);
    SLOT('w', mFTPActive);
    SLOT('w', mFTPDone);
    SLOT('w', mFTPResult);
    SLOT('w', mF398);
    SLOT('p', mF39C);
    SLOT('p', mFTP);
    SLOT('p', mFTPPark);
    SLOT('w', mF3A8);
    SLOT('S', mF3AC);
    SLOT('w', mF3C0);
    SLOT('w', mF3C4);
    SLOT('L', mF3C8);
    PAD(mF3D8, 1)
    SLOT('S', mF3DC);
    SLOT('w', mFTPParkPending);
    PAD(mF3F4, 4)
#undef SLOT
#undef PAD
    return slots;
}

// ---- the texts the strings are given
const char* const kTexts[] = {"", "a", "Hello", "xyz 123", "A longer text than the others, 40 chars!"};
constexpr int32_t kTextCount = 5;
std::u16string AsWide(const char* text) {
    std::u16string wide;
    for (; *text != 0; ++text) wide += static_cast<char16_t>(static_cast<unsigned char>(*text));
    return wide;
}

// ---- normalisation and dumping
int64_t NormPtr(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < gAllocN; ++i)
        if (gAllocs[i] == p) return 1000 + i;
    const int32_t label = lab(p);
    return label > 0 ? label : -1;
}

template<class Char> void DumpString(const TbDynamicStringTemplate<Char>& s, std::vector<int64_t>& out) {
    uint32_t h = 2166136261u;
    int32_t n = 0;
    for (const Char* p = s.mData; p != nullptr && *p != 0 && n < 4096; ++p, ++n) h = (h ^ (static_cast<uint32_t>(*p) & 0xffff)) * 16777619u;
    out.push_back(s.mLength);
    out.push_back(h);
}

// The kind of a polymorphic object (the final class), as the Python side reads it from the Mac's vtable word.
template<class T> int64_t KindOf(const T* p) {
    if (p == nullptr) return 0;
    const std::type_info& t = typeid(*p);
    if (t == typeid(CFSOnlineSystemCityList)) return 1;
    if (t == typeid(CFSOnlineSystemParkList_InWorldByParkID)) return 2;
    if (t == typeid(CFSOnlineSystemParkList_InWorldByPlayerName)) return 3;
    if (t == typeid(CFSOnlineSystemParkList_InCity)) return 4;
    if (t == typeid(CFSOnlineSystemParkList_InCityTop10)) return 5;
    if (t == typeid(CFSOnlineSystemThemeList)) return 6;
    if (t == typeid(CFSOnlineSystemServerList)) return 7;
    if (t == typeid(CFSOnlineSystemRideList)) return 8;
    if (t == typeid(TbPtrList<CFSOnlineCity>)) return 9;
    if (t == typeid(TbPtrList<CFSOnlinePark>)) return 10;
    if (t == typeid(TbPtrList<CFSOnlineTheme>)) return 11;
    if (t == typeid(TbPtrList<CFSOnlineServer>)) return 12;
    if (t == typeid(TbPtrList<CFSOnlineRide>)) return 13;
    if (t == typeid(TbPtrList<TbDynamicStringTemplate<char16_t>>)) return 14;
    if (t == typeid(CFSOnlinePark)) return 15;
    return -1;
}

template<class LIST> void DumpTbList(const LIST* l, std::vector<int64_t>& out) {
    out.push_back(KindOf(l));
    out.push_back(NormPtr(l->mHead));
    out.push_back(NormPtr(l->mTail));
    out.push_back(l->mCount);
}

// A list object: kind, the nine words, the typed list and the stored answer; then the subclass's extra member.
template<class LIST, class RECEIVED> void DumpBase(const CFSOnlineSystemList<LIST, RECEIVED>& l, std::vector<int64_t>& out) {
    out.push_back(KindOf(&l));
    out.push_back(l.mRequestID);
    out.push_back(l.mF08);
    out.push_back(l.mF0C);
    out.push_back(l.mF10);
    out.push_back(l.mF14);
    out.push_back(l.mF18);
    out.push_back(l.mF1C);
    out.push_back(l.mF20);
    out.push_back(l.mF24);
    out.push_back(l.mF28);
    out.push_back(NormPtr(l.mList));
    out.push_back(NormPtr(l.mReceived));
    if (l.mList != nullptr) DumpTbList(l.mList, out);
}
void DumpList(const CFSOnlineSystemCityList* l, std::vector<int64_t>& out) { if (l != nullptr) DumpBase(*l, out); }
void DumpList(const CFSOnlineSystemParkList_InWorldByParkID* l, std::vector<int64_t>& out) {
    if (l == nullptr) return;
    DumpBase(*l, out);
    out.push_back(l->mF34);
}
void DumpList(const CFSOnlineSystemParkList_InWorldByPlayerName* l, std::vector<int64_t>& out) {
    if (l == nullptr) return;
    DumpBase(*l, out);
    DumpString(l->mPlayerName, out);
}
void DumpList(const CFSOnlineSystemParkList_InCity* l, std::vector<int64_t>& out) {
    if (l == nullptr) return;
    DumpBase(*l, out);
    out.push_back(l->mF34);
}
void DumpList(const CFSOnlineSystemParkList_InCityTop10* l, std::vector<int64_t>& out) {
    if (l == nullptr) return;
    DumpBase(*l, out);
    out.push_back(l->mF34);
}
void DumpList(const CFSOnlineSystemThemeList* l, std::vector<int64_t>& out) { if (l != nullptr) DumpBase(*l, out); }
void DumpList(const CFSOnlineSystemServerList* l, std::vector<int64_t>& out) { if (l != nullptr) DumpBase(*l, out); }
void DumpList(const CFSOnlineSystemRideList* l, std::vector<int64_t>& out) { if (l != nullptr) DumpBase(*l, out); }

template<class T> void Words(const T& field, std::vector<int64_t>& out) {
    int32_t v;
    std::memcpy(&v, &field, 4);
    out.push_back(v);
}

void DumpPark(const CFSOnlinePark* p, std::vector<int64_t>& out) {
    if (p == nullptr) return;
    out.push_back(KindOf(p));
    out.push_back(NormPtr(p->mList));
    out.push_back(p->mParkID);
    DumpString(p->mText0C, out);
    DumpString(p->mText20, out);
    DumpString(p->mText34, out);
    out.push_back(p->mF48);
    out.push_back(p->mF4C);
    out.push_back(p->mF50);
    out.push_back(p->mF54);
    for (int32_t i = 0; i < 8; ++i) {
        int32_t v;
        std::memcpy(&v, p->mF58 + 4 * i, 4);
        out.push_back(v);
    }
    out.push_back(p->mF78);
    DumpString(p->mText7C, out);
    out.push_back(p->mF90);
    DumpString(p->mText94, out);
    out.push_back(p->mFA8);
    out.push_back(p->mFAA);
    out.push_back(p->mFAC);
    int16_t pad;
    std::memcpy(&pad, p->mFAE, 2);
    out.push_back(pad);
    out.push_back(p->mFB0);
    out.push_back(p->mFB4);
    out.push_back(p->mFB8);
    out.push_back(p->mFBC);
}

void DumpSystem(CFSOnlineSystem& s, std::vector<int64_t>& out) {
    for (const Slot& slot : BuildSlots(s)) {
        switch (slot.kind) {
        case 'w': {
            int32_t v;
            std::memcpy(&v, slot.addr, 4);
            out.push_back(v);
            break;
        }
        case 'p': {
            void* p;
            std::memcpy(&p, slot.addr, sizeof p);
            out.push_back(NormPtr(p));
            break;
        }
        case 'f': {
            OnlineLoginCallback callback;
            std::memcpy(&callback, slot.addr, sizeof callback);
            out.push_back(callback == nullptr ? 0 : callback == &FakeLoginCallback ? 1 : -1);
            break;
        }
        case 'x': {
            void* p;
            std::memcpy(&p, slot.addr, sizeof p);
            out.push_back(static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))));
            break;
        }
        case 'W': DumpString(*static_cast<TbDynamicStringTemplate<char16_t>*>(slot.addr), out); break;
        case 'S': DumpString(*static_cast<TbDynamicStringTemplate<char>*>(slot.addr), out); break;
        case 'L': DumpTbList(static_cast<TbPtrList<TbDynamicStringTemplate<char16_t>>*>(slot.addr), out); break;
        }
    }
    DumpList(s.mCityList, out);
    DumpList(s.mParksTop10, out);
    DumpList(s.mParksInCity, out);
    DumpList(s.mParksByPlayer, out);
    DumpList(s.mParksByID, out);
    DumpList(s.mParksInCity2, out);
    DumpList(s.mThemeList, out);
    DumpList(s.mServerList, out);
    DumpList(s.mRideList, out);
    DumpPark(s.mFTPPark, out);
}

// ---- applying a randomised state
// The spec, in order: for each slot (word: the value; string: an index into kTexts; short: the value; pointer and list: nothing);
// then six connection choices (0 none, 1 a fake object); then for each of the nine lists nine words and five numbers describing the
// stored answer (kind 0 none, 1 a holder; its count, the number of infos (-1 for a NULL array), the reference count, the word at 8).
struct Spec {
    const int64_t* p;
    int64_t next() { return *p++; }
};

template<class R> void MakeReceived(R*& slot, int32_t index, Spec& spec) {
    const int64_t kind = spec.next();
    const int64_t count = spec.next();
    const int64_t infos = spec.next();
    const int64_t refs = spec.next();
    const int64_t word = spec.next();
    slot = nullptr;
    if (kind == 0) return;
    R* holder = new R;
    std::memset(static_cast<void*>(holder), 0xa5, sizeof *holder);
    holder->mCount = static_cast<int32_t>(count);
    holder->mInfos = nullptr;
    holder->mF08 = static_cast<int32_t>(word);
    holder->mRefCount = static_cast<int32_t>(refs);
    tpw_cl_label(kLabelHolder + index, holder);
    if (infos >= 0) {
        gWatchArray = true;
        holder->mInfos = new typename std::remove_pointer<decltype(holder->mInfos)>::type[infos];
        gWatchArray = false;
        tpw_cl_label(kLabelArray + index, gLastArray);
        for (int64_t k = 0; k < infos; ++k) tpw_cl_label(kLabelInfo + 4 * index + static_cast<int32_t>(k), &holder->mInfos[k]);
    }
    slot = holder;
}

template<class L> void ApplyList(L* list, int32_t index, Spec& spec) {
    tpw_cl_label(kLabelList + index, list);
    list->mRequestID = static_cast<int32_t>(spec.next());
    list->mF08 = static_cast<int32_t>(spec.next());
    list->mF0C = static_cast<int32_t>(spec.next());
    list->mF10 = static_cast<int32_t>(spec.next());
    list->mF14 = static_cast<int32_t>(spec.next());
    list->mF18 = static_cast<int32_t>(spec.next());
    list->mF1C = static_cast<int32_t>(spec.next());
    list->mF20 = static_cast<int32_t>(spec.next());
    list->mF24 = static_cast<int32_t>(spec.next());
    list->mF28 = static_cast<int32_t>(spec.next());
    MakeReceived(list->mReceived, index, spec);
}

void* NewConnection(int32_t index) {
    // (the FTP client is a real object: it is deleted through its virtual destructor)
    void* block = index == 5 ? static_cast<void*>(new FTPClient) : ::operator new(16);
    tpw_cl_label(kLabelConnection + index, block);
    return block;
}
}  // namespace

#ifdef TPW_ONLINE_TICK
void OnlineExtraCall(int32_t op);   // unit_OnlineTick.cpp: the members of unit w69
#endif

// ---- fakes: the string formatting (the strings are real otherwise; nothing here formats), the online library, the debug log, the clock
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}
void Auth::Release() { ev("Auth.Release", {lab(this)}); }
Auth::~Auth() { ev("Auth.dtor", {lab(this)}); }
void News::Release() { ev("News.Release", {lab(this)}); }
News::~News() { ev("News.dtor", {lab(this)}); }
void City::Release() { ev("City.Release", {lab(this)}); }
City::~City() { ev("City.dtor", {lab(this)}); }
void Upload::Release() { ev("Upload.Release", {lab(this)}); }
FTPClient::~FTPClient() { ev("FTP.dtor", {lab(this)}); }

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock");
    return static_cast<uint32_t>(ret("LbTime_GetClock"));
}

#ifndef TPW_ONLINE_TICK
void CFSOnlineSystem::CompleteLogin(int32_t result) { ev("CompleteLogin", {lab(this), result}); }
#endif

// The "cc" infos' destructors (the library's records): they record the element.
ccParkInfo::~ccParkInfo() { ev("ccParkInfo.dtor", {lab(this)}); }
ccCityInfo::~ccCityInfo() { ev("ccCityInfo.dtor", {lab(this)}); }
ccThemeInfo::~ccThemeInfo() { ev("ccThemeInfo.dtor", {lab(this)}); }
ccServerInfo::~ccServerInfo() { ev("ccServerInfo.dtor", {lab(this)}); }
ccRideInfo::~ccRideInfo() { ev("ccRideInfo.dtor", {lab(this)}); }

// The list family's destructors (untranslated; nothing here runs them).
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>, ccThemeList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineServer>, ccServerList>::~CFSOnlineSystemList() {}
template<> CFSOnlineSystemList<TbPtrList<CFSOnlineRide>, ccRideList>::~CFSOnlineSystemList() {}
CFSOnlineSystemCityList::~CFSOnlineSystemCityList() {}
// v5307 translated the city list's own overrides; the vtable this file's destructor emits needs them (nothing here runs them)
int32_t CFSOnlineSystemCityList::ChildLocked() { return 0; }
int32_t CFSOnlineSystemCityList::RequestListFromServer(City*, uint32_t) { return 0; }
void CFSOnlineSystemCityList::StoreReceivedList(const ccCityList*) {}
CFSOnlineSystemParkList::~CFSOnlineSystemParkList() {}
CFSOnlineSystemParkList_InWorldByParkID::~CFSOnlineSystemParkList_InWorldByParkID() {}
CFSOnlineSystemParkList_InWorldByPlayerName::~CFSOnlineSystemParkList_InWorldByPlayerName() {}
CFSOnlineSystemParkList_InCity::~CFSOnlineSystemParkList_InCity() {}
CFSOnlineSystemParkList_InCityTop10::~CFSOnlineSystemParkList_InCityTop10() {}
CFSOnlineSystemThemeList::~CFSOnlineSystemThemeList() {}
CFSOnlineSystemServerList::~CFSOnlineSystemServerList() {}
CFSOnlineSystemRideList::~CFSOnlineSystemRideList() {}
CFSOnlinePark::~CFSOnlinePark() {}

// ---- the entry points
extern "C" {
// The slot table as "kind:name" lines, to check against the Python side's schema.
int32_t tpw_oi_slots(char* out, int32_t cap) {
    alignas(16) static unsigned char probe[sizeof(CFSOnlineSystem)];
    std::string text;
    for (const Slot& s : BuildSlots(*reinterpret_cast<CFSOnlineSystem*>(probe))) text += std::string(1, s.kind) + ":" + s.name + "\n";
    std::snprintf(out, cap, "%s", text.c_str());
    return static_cast<int32_t>(text.size());
}

// A fresh online system over `fill` bytes, label 1. traced: the constructor's events are recorded (the heap counters count it too).
void tpw_oi_system(int32_t traced, int32_t fill) {
    Quiet quiet;
    gAllocN = 0;
    gOtherKind = 0;
    std::memset(gSystemBlock, fill, sizeof gSystemBlock);
    tpw_cl_label(kLabelSystem, gSystemBlock);
    gSystem = reinterpret_cast<CFSOnlineSystem*>(gSystemBlock);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    gTrack = true;
    if (traced != 0) tpw_cl_on = 1;
    new (gSystemBlock) CFSOnlineSystem();
    gTrack = false;
    if (traced == 0) std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

// Writes a randomised state; see Spec.
void tpw_oi_apply(const int64_t* values) {
    Quiet quiet;
    Spec spec{values};
    for (const Slot& slot : BuildSlots(*gSystem)) {
        switch (slot.kind) {
        case 'w': {
            const int32_t v = static_cast<int32_t>(spec.next());
            std::memcpy(slot.addr, &v, 4);
            break;
        }
        case 'f': {
            OnlineLoginCallback callback = spec.next() != 0 ? &FakeLoginCallback : nullptr;
            std::memcpy(slot.addr, &callback, sizeof callback);
            break;
        }
        case 'x': {
            void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(spec.next())));
            std::memcpy(slot.addr, &p, sizeof p);
            break;
        }
        case 'W': *static_cast<TbDynamicStringTemplate<char16_t>*>(slot.addr) = AsWide(kTexts[spec.next() % kTextCount]).c_str(); break;
        case 'S': *static_cast<TbDynamicStringTemplate<char>*>(slot.addr) = kTexts[spec.next() % kTextCount]; break;
        default: break;
        }
    }
    // The connections: the login (Auth), news, city, upload, the buffer at 0x39c and the FTP client; each a fresh block or NULL.
    gSystem->mLogin = spec.next() != 0 ? static_cast<Auth*>(NewConnection(0)) : nullptr;
    gSystem->mNews = spec.next() != 0 ? static_cast<News*>(NewConnection(1)) : nullptr;
    gSystem->mCity = spec.next() != 0 ? static_cast<City*>(NewConnection(2)) : nullptr;
    gSystem->mUpload = spec.next() != 0 ? static_cast<Upload*>(NewConnection(3)) : nullptr;
    gSystem->mF39C = spec.next() != 0 ? NewConnection(4) : nullptr;
    gSystem->mFTP = spec.next() != 0 ? static_cast<FTPClient*>(NewConnection(5)) : nullptr;
    ApplyList(gSystem->mCityList, 0, spec);
    ApplyList(gSystem->mParksTop10, 1, spec);
    ApplyList(gSystem->mParksInCity, 2, spec);
    ApplyList(gSystem->mParksByPlayer, 3, spec);
    ApplyList(gSystem->mParksByID, 4, spec);
    ApplyList(gSystem->mParksInCity2, 5, spec);
    ApplyList(gSystem->mThemeList, 6, spec);
    ApplyList(gSystem->mServerList, 7, spec);
    ApplyList(gSystem->mRideList, 8, spec);
    // The FTP client's progress words (read by TickFTPDownload), when there is a client.
    const int32_t ftpTotal = static_cast<int32_t>(spec.next());
    const int32_t ftpDone = static_cast<int32_t>(spec.next());
    if (gSystem->mFTP != nullptr) {
        gSystem->mFTP->mTotal = ftpTotal;
        gSystem->mFTP->mDone = ftpDone;
    }
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}

// Runs one member with recording on.
void tpw_oi_call(int32_t op) {
    gTrack = true;
    tpw_cl_on = 1;
    switch (op) {
    case 1: gSystem->InitLogin(); break;
    case 2: gSystem->ReleaseLoginConnection(); break;
    case 3: gSystem->InitNews(); break;
    case 4: gSystem->ReleaseNewsConnection(); break;
    case 5: gSystem->InitCity(); break;
    case 6: gSystem->ReleaseCityServerConnection(); break;
    case 7: gSystem->InitUpload(); break;
    case 8: gSystem->ReleaseUploadServerConnection(); break;
    case 9: gSystem->InitFTPDownload(); break;
    case 10: gSystem->ReleaseFTPDownloadServerConnection(); break;
    case 11: gSystem->TickLogin(); break;
#ifdef TPW_ONLINE_TICK
    default: OnlineExtraCall(op); break;
#endif
    }
    tpw_cl_on = 0;
    gTrack = false;
}

// A list or a park constructed over 0xa5 bytes (kind 1 theme list, 2 server list, 3 ride list, 4 park); traced.
void tpw_oi_block(int32_t kind, int32_t fill) {
    Quiet quiet;
    gAllocN = 0;
    gSystem = nullptr;
    gOtherKind = kind;
    std::memset(gOtherBlock, fill, sizeof gOtherBlock);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    tpw_cl_label(kLabelSystem, gOtherBlock);
    gTrack = true;
    tpw_cl_on = 1;
    switch (kind) {
    case 1: new (gOtherBlock) CFSOnlineSystemThemeList(); break;
    case 2: new (gOtherBlock) CFSOnlineSystemServerList(); break;
    case 3: new (gOtherBlock) CFSOnlineSystemRideList(); break;
    case 4: new (gOtherBlock) CFSOnlinePark(); break;
    }
    tpw_cl_on = 0;
    gTrack = false;
}

// Everything observable: the heap traffic since the last reset (operator new, operator delete), then the object.
static int32_t DumpAll(int64_t* out, int32_t cap) {
    std::vector<int64_t> values;
    values.push_back(tpw_cl_heap[0]);
    values.push_back(tpw_cl_heap[2]);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
    if (gSystem != nullptr) {
        DumpSystem(*gSystem, values);
    } else {
        switch (gOtherKind) {
        case 1: DumpList(reinterpret_cast<CFSOnlineSystemThemeList*>(gOtherBlock), values); break;
        case 2: DumpList(reinterpret_cast<CFSOnlineSystemServerList*>(gOtherBlock), values); break;
        case 3: DumpList(reinterpret_cast<CFSOnlineSystemRideList*>(gOtherBlock), values); break;
        case 4: DumpPark(reinterpret_cast<CFSOnlinePark*>(gOtherBlock), values); break;
        }
    }
    const int32_t n = static_cast<int32_t>(values.size() < static_cast<std::size_t>(cap) ? values.size() : cap);
    for (int32_t i = 0; i < n; ++i) out[i] = values[i];
    return static_cast<int32_t>(values.size());
}

int32_t tpw_oi_dump(int64_t* out, int32_t cap) {
    const int32_t n = DumpAll(out, cap);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);   // (the dump's own vectors and names went through operator new and delete)
    return n;
}
}  // extern "C"
