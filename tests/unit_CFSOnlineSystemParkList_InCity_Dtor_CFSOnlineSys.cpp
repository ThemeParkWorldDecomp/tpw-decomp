// Test library for the InCity park list's destructor and RequestListFromServer (unit v5532;
// libtpw_unit_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.dylib, tests/test_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.py), main app:
//   src/FS_Online_..._CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp (the derived destructor, RequestListFromServer)
//   src/game/online/FS_Online.cpp (the base destructor that does the Mac's inlined work)
//
// Fakes: UDebug, City::GetParkInCityList, the park's constructor and destructor, the park info destructor (each records an event).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {
char gCity[16];
}

// ---- the fakes
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

int32_t City::GetParkInCityList(void* context, int32_t city, uint32_t arg, int32_t flag) {
    ev("GetParkInCityList", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(context)), city, static_cast<int64_t>(arg), flag});
    return static_cast<int32_t>(ret("GetParkInCityList"));
}

// The common base's destructor is untranslated (its out-of-line copies); an empty one stands in (the park list's destructor does the work).
template<> CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList>::~CFSOnlineSystemList() {}

// The park's string members: empty stand-ins (the strings are untranslated here and never inspected).
#define FAKE_STRING(C)                                                                                             \
    template<> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() {}                                            \
    template<> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}                                           \
    template<> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                           \
    template<> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                              \
    template<> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                           \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }       \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }       \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; } \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }      \
    template<> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
FAKE_STRING(char16_t)
FAKE_STRING(char)

CFSOnlinePark::CFSOnlinePark() {}
CFSOnlinePark::~CFSOnlinePark() { ev("parkdtor", {0}); }
ccParkInfo::~ccParkInfo() { ev("infodtor", {0}); }

extern "C" {

uint64_t tpw_ic_addr(const char* name) {
    if (std::strcmp(name, "city") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gCity));
    return 0;
}

uint64_t tpw_ic_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ic_layout(int64_t* out) {
    (void)out;
    return 0;
}

// Builds a list on the heap (n parks, those in nullmask NULL; recvmode 0 none, 1 count 1 with infos, 2 count 1 without, 3 count 2),
// destroys it (flag > 0: the deleting form), and reports the words 0x24 and 0x28 and the count of a kept answer.
uint32_t tpw_ic_dtor(int32_t flag, int32_t isnull, int32_t n, uint32_t nullmask, int32_t recvmode, int32_t f24, int32_t f28, int32_t* out) {
    out[0] = out[1] = out[2] = 0;
    if (isnull) return 0;
    CFSOnlineSystemParkList_InCity* obj = new CFSOnlineSystemParkList_InCity();
    obj->mF24 = f24;
    obj->mF28 = f28;
    for (int32_t i = 0; i < n; ++i) obj->mList->AddTail(((nullmask >> i) & 1) ? nullptr : new CFSOnlinePark());
    ccParkList* received = nullptr;
    if (recvmode != 0) {
        received = static_cast<ccParkList*>(::operator new(sizeof(ccParkList)));
        received->mCount = 1;
        received->mInfos = recvmode == 2 ? nullptr : new ccParkInfo[1];
        received->mF08 = 0;
        received->mRefCount = recvmode == 3 ? 2 : 1;
        obj->mReceived = received;
    }
    on();
    if (flag > 0) {
        delete obj;
    } else {
        obj->~CFSOnlineSystemParkList_InCity();
        out[0] = obj->mF24;
        out[1] = obj->mF28;
        if (recvmode == 3) out[2] = received->mRefCount;
        ::operator delete(static_cast<void*>(obj));
    }
    return 1;
}

int32_t tpw_ic_request(int32_t f34, uint32_t ul) {
    CFSOnlineSystemParkList_InCity* obj = new CFSOnlineSystemParkList_InCity();
    obj->mF34 = f34;
    int32_t answer = obj->RequestListFromServer(reinterpret_cast<City*>(gCity), ul);
    delete obj;
    return answer;
}

}  // extern "C"
