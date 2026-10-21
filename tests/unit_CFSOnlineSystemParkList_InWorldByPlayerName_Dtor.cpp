// Test library for the park list "in the world by player name" (unit v5063; libtpw_unit_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.dylib,
// tests/test_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.py), main app:
//   src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp   the destructor and GetList
//
// The callees outside the unit are recording fakes: the online system's constructor, AreWeLoggedIn and Login, PostLoginCallbackFn_Park,
// the string's members (constructor, destructor, the virtual assignment), ccParkInfo's destructor, and the global operator new and delete
// (they record only outside the object builders, which run quiet). The park list bases' destructors are the program's own code.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem = nullptr;

namespace {

bool gQuiet = false;   // the builders of the test objects run with the operator new fake silent

typedef CFSOnlineSystemParkList_InWorldByPlayerName List;

}  // namespace

// ---- the global operator new and delete: record (the size; the label of the pointer), memory from the zeroed 32-bit arena
// (the shim's global operator new and delete are used: delete records "delete" with the label of the pointer, new records nothing)

// ---- the fakes: the same event name and arguments as the module's PowerPC stub
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
void CFSOnlineSystem::Login(void (*callback)(int32_t, const void*), const void* context) {
    ev("Login", {lab(this), callback == &PostLoginCallbackFn_Park ? 1 : 0, lab(context)});
}
void PostLoginCallbackFn_Park(int32_t error, const void* context) { ev("PostLoginCallbackFn_Park", {error, lab(context)}); }
ccParkInfo::~ccParkInfo() { ev("ccParkInfoDtor", {}); }

// The string: only the destructor and the assignment of text record; the other members exist for the vtable.
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { ev("StringDtor", {lab(this), -1}); }
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t* text) {
    ev("StringAssign", {lab(this)}, {narrow(text).c_str()});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

// The list base's destructor (another unit's, 0x10179b08 or its twin; the Mac inlines it into the derived destructors): the stored answer
// released (its count down, at zero the infos destroyed and freed and the answer freed), the request words cleared, the typed list
// deleted. The park records of a non-empty typed list are not made by this test (the list is always empty).
template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {
    if (mReceived != nullptr) {
        --mReceived->mRefCount;
        if (mReceived->mRefCount == 0) {
            if (mReceived->mInfos != nullptr) delete[] mReceived->mInfos;
            mReceived->mInfos = nullptr;
            ::operator delete(static_cast<void*>(mReceived));
        }
        mReceived = nullptr;
    }
    mF24 = 0;
    mF28 = 0;
    delete mList;
}

CFSOnlineSystemParkList::~CFSOnlineSystemParkList() {}   // 0x1016fd48: adds nothing to its base

// The list's constructor (inlined into the system's constructor on the Mac): the bases' inline constructors, the string made.
CFSOnlineSystemParkList_InWorldByPlayerName::CFSOnlineSystemParkList_InWorldByPlayerName() {}

extern "C" {

uint64_t tpw_pl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// (made with operator new, as the program frees some of these blocks with operator delete)
uint64_t tpw_pl_alloc(uint32_t size) {
    void* block = ::operator new(size);
    std::memset(block, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_pl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(List, mRequestID));
    out[n++] = static_cast<int64_t>(offsetof(List, mF08));
    out[n++] = static_cast<int64_t>(offsetof(List, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(List, mF10));
    out[n++] = static_cast<int64_t>(offsetof(List, mF14));
    out[n++] = static_cast<int64_t>(offsetof(List, mF18));
    out[n++] = static_cast<int64_t>(offsetof(List, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(List, mF20));
    out[n++] = static_cast<int64_t>(offsetof(List, mF24));
    out[n++] = static_cast<int64_t>(offsetof(List, mF28));
    out[n++] = static_cast<int64_t>(offsetof(List, mList));
    out[n++] = static_cast<int64_t>(offsetof(List, mReceived));
    out[n++] = static_cast<int64_t>(offsetof(List, mPlayerName));
    out[n++] = static_cast<int64_t>(offsetof(ccParkList, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ccParkList, mInfos));
    out[n++] = static_cast<int64_t>(offsetof(ccParkList, mF08));
    out[n++] = static_cast<int64_t>(offsetof(ccParkList, mRefCount));
    out[n++] = static_cast<int64_t>(sizeof(ccParkList));
    return n;
}

// A list made by its constructors (silently), at a 32-bit address.
uint64_t tpw_pl_make() {
    gQuiet = true;
    List* list = new List();   // from operator new: the deleting form frees it
    gQuiet = false;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(list));
}

// An array of count infos made with new[] (the cookie included), silently.
uint64_t tpw_pl_newinfos(int32_t count) {
    gQuiet = true;
    ccParkInfo* infos = new ccParkInfo[count];
    gQuiet = false;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(infos));
}

// ---- the functions
// The destructor with the delete flag: positive means the deleting form; NULL does nothing. Answers the pointer.
uint64_t tpw_pl_dtor(uint64_t object, int16_t flag) {
    List* list = reinterpret_cast<List*>(static_cast<uintptr_t>(object));
    if (list == nullptr) return 0;
    if (flag > 0)
        delete list;
    else
        list->~List();
    return object;
}

void tpw_pl_getlist(uint64_t object, uint64_t text, uint32_t callback, uint32_t context) {
    List* list = reinterpret_cast<List*>(static_cast<uintptr_t>(object));
    list->GetList(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text)),
                  reinterpret_cast<CFSOnlineSystemParkList::Callback>(static_cast<uintptr_t>(callback)),
                  reinterpret_cast<const void*>(static_cast<uintptr_t>(context)));
}

}  // extern "C"
