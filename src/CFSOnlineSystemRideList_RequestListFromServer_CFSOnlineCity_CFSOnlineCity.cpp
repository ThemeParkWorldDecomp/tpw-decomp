// Unit v5994: the online city record's constructor and destructor body, the park record's assignment, the server record's destructor and
// the ride list's post-login callback (0x1017b604 .. 0x1017c96c).
#include <cstddef>
#include <cstring>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "platform/samsutils.h"

namespace onlinelist_callee {
// The list calls the Mac makes through the TbTList instances. Weak, so that the test library can replace them with recording fakes.
__attribute__((weak, noinline)) void CityAddTail(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) { list->AddTail(city); }
__attribute__((weak, noinline)) void* CityFind(TbPtrList<CFSOnlineCity>* list, CFSOnlineCity* city) { return list->Find(city); }
__attribute__((weak, noinline)) void CityRemoveAt(TbPtrList<CFSOnlineCity>* list, void*& position) { list->RemoveAt(position); }
__attribute__((weak, noinline)) void FreeCity(void* city) { ::operator delete(city); }
__attribute__((weak, noinline)) void* ServerFind(TbPtrList<CFSOnlineServer>* list, CFSOnlineServer* server) { return list->Find(server); }
__attribute__((weak, noinline)) void ServerRemoveAt(TbPtrList<CFSOnlineServer>* list, void*& position) { list->RemoveAt(position); }
}  // namespace onlinelist_callee

namespace {
typedef TbDynamicStringTemplate<char16_t> WideString;

template <class T>
T RawWord(const void* base, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(T));
    return value;
}
}  // namespace

// 0x1017bcb4 (312): the three strings made, the fields copied from the 0x30-byte info (the position scaled by 1.01), the city added to
// the list's tail.
CFSOnlineCity::CFSOnlineCity(const ccCityInfo* info, CFSOnlineSystemCityList* list) {
    WideString* name = new (mText0C) WideString;
    WideString* text20 = new (mText20) WideString;
    WideString* text44 = new (mText44) WideString;
    mList = list;
    mCityID = RawWord<int32_t>(info, 0x00);
    *name = static_cast<const char16_t*>(info->mBlock04.get());
    *text20 = static_cast<const char16_t*>(info->mBlock08.get());
    const double scale = 1.01;
    mPosX = static_cast<float>(scale * RawWord<float>(info, 0x0c));
    mPosY = static_cast<float>(scale * RawWord<float>(info, 0x14));
    mPosZ = static_cast<float>(scale * RawWord<float>(info, 0x10));
    mValue = RawWord<int32_t>(info, 0x18);
    *text44 = static_cast<const char16_t*>(info->mBlock1C.get());
    mF58 = RawWord<int32_t>(info, 0x20);
    mF5C = RawWord<int32_t>(info, 0x24);
    mF60 = RawWord<int32_t>(info, 0x28);
    mF64 = RawWord<int32_t>(info, 0x2c);
    onlinelist_callee::CityAddTail(mList->mList, this);
}

// 0x1017be40 (196), the part after the NULL check.
void CFSOnlineCity::Destruct(int16_t deleteFlag) {
    CFSOnlineSystemCityList* owner = mList;
    void* position = onlinelist_callee::CityFind(owner->mList, this);
    if (position != nullptr) onlinelist_callee::CityRemoveAt(owner->mList, position);
    reinterpret_cast<WideString*>(mText44)->~WideString();
    reinterpret_cast<WideString*>(mText20)->~WideString();
    reinterpret_cast<WideString*>(mText0C)->~WideString();
    if (deleteFlag > 0) onlinelist_callee::FreeCity(this);
}

// 0x1017c2e4 (324): every field copied, the strings through their virtual assignments.
CFSOnlinePark& CFSOnlinePark::operator=(const CFSOnlinePark& other) {
    mParkID = other.mParkID;
    mText0C = other.mText0C;
    mText20 = other.mText20;
    mText34 = other.mText34;
    mF48 = other.mF48;
    mF4C = other.mF4C;
    mF50 = other.mF50;
    mF54 = other.mF54;
    for (int32_t i = 0; i < 0x20; i++) mF58[i] = other.mF58[i];
    mF78 = other.mF78;
    mText7C = other.mText7C;
    mF90 = other.mF90;
    mText94 = other.mText94;
    mFA8 = other.mFA8;
    mFAA = other.mFAA;
    mFAC = other.mFAC;
    mFB0 = other.mFB0;
    mFB4 = other.mFB4;
    mFB8 = other.mFB8;
    mFBC = other.mFBC;
    return *this;
}

// 0x1017c96c (228): leaves the list, then the members' destructors (the strings, last to first) run. The deleting form is the compiler's.
// (the destructor itself is now defined by the CFSOnlineServer unit; this is the body, which the test library's destructor calls)
void CFSOnlineServer_DestructBody(CFSOnlineServer* self) {
    CFSOnlineServer* const server = self;
    CFSOnlineSystemServerList* owner = server->mList;
    void* position = onlinelist_callee::ServerFind(owner->mList, server);
    if (position != nullptr) onlinelist_callee::ServerRemoveAt(owner->mList, position);
}

// 0x1017b604 (140)
void RideListPostLoginCallbackFn(int32_t error, const void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    CFSOnlineSystemRideList* list = gOnlineSystem->mRideList;
    if (error != 0)
        list->CompleteGetList(1);
    else
        list->LoggedInGetList();
}
