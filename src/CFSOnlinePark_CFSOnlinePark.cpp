// CFSOnlinePark constructor from a ccParkInfo (0x1017c0d8) and destructor (0x1017c464).
#include <cstdint>
#include <cstring>

#include "FS_OnlineList.h"

namespace {

// The ccParkInfo is a loaded 0x66 byte image: fields are read at their Mac offsets (unaligned past 0x56).
uint32_t InfoWord(const ccParkInfo* info, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(info) + offset, 4);
    return value;
}

uint16_t InfoHalf(const ccParkInfo* info, uint32_t offset) {
    uint16_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(info) + offset, 2);
    return value;
}

// A string pointer of the image: a Ptr32 (an offset from the arena base on the host), null for 0.
template <class T>
const T* InfoPtr(const ccParkInfo* info, uint32_t offset) {
    Ptr32<const T> ptr;
    ptr.mOffset = InfoWord(info, offset);
    return ptr.get();
}

}  // namespace

CFSOnlinePark::CFSOnlinePark(const ccParkInfo* info, CFSOnlineSystemParkList* list) {
    // The five strings are built first (0x0c, 0x20, 0x34, 0x7c, 0x94) by the member initialisers.
    mList = list;
    mParkID = static_cast<int32_t>(InfoWord(info, 0x00));
    mText0C = InfoPtr<char16_t>(info, 0x04);
    mText20 = InfoPtr<char16_t>(info, 0x08);
    mText34 = InfoPtr<char16_t>(info, 0x0c);
    mF48 = static_cast<int32_t>(InfoWord(info, 0x10));
    mF4C = static_cast<int32_t>(InfoWord(info, 0x14));
    mF50 = static_cast<int32_t>(InfoWord(info, 0x18));
    mF54 = static_cast<int32_t>(InfoWord(info, 0x1c));
    for (int32_t i = 0; i < 0x20; i++) mF58[i] = reinterpret_cast<const uint8_t*>(info)[0x20 + i];
    mF78 = static_cast<int32_t>(InfoWord(info, 0x40));
    mText7C = InfoPtr<char>(info, 0x44);
    mF90 = static_cast<int32_t>(InfoWord(info, 0x48));
    mText94 = InfoPtr<char16_t>(info, 0x4c);
    mFA8 = static_cast<int16_t>(InfoHalf(info, 0x50));
    mFAA = static_cast<int16_t>(InfoHalf(info, 0x52));
    mFAC = static_cast<int16_t>(InfoHalf(info, 0x54));
    mFB0 = static_cast<int32_t>(InfoWord(info, 0x56));
    mFB4 = static_cast<int32_t>(InfoWord(info, 0x5a));
    mFB8 = static_cast<int32_t>(InfoWord(info, 0x5e));
    mFBC = static_cast<int32_t>(InfoWord(info, 0x62));
    mList->mList->AddTail(this);
}

// The deleting flag is the caller's business here; the strings are destroyed after the body in reverse order, as on the Mac.
// (A NULL this, which the Mac destructor tolerates, cannot happen in C++.)
CFSOnlinePark::~CFSOnlinePark() {
    CFSOnlineSystemParkList* list = mList;
    if (list != nullptr) {
        void* position = list->mList->Find(this);
        if (position != nullptr) list->mList->RemoveAt(position);
    }
}
