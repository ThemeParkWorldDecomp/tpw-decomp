// Test library for CFSOnlineRide::~CFSOnlineRide (unit v5741; libtpw_unit_CFSOnlineRide_Dtor_CFSOnlineRide.dylib), main app:
//   src/CFSOnlineRide_Dtor_CFSOnlineRide.cpp
//
// The string destructor and operator delete are recording fakes (the same events as the PowerPC stubs). The list is the real TbTList code
// on native nodes; the ride objects are zeroed arena blocks (the destructor never reads the vtable).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"
#include "TbTList.h"

using namespace calllog;

namespace {
CFSOnlineRide* gRide = nullptr;
}

// ---- the fakes
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    int64_t offset = -1;
    if (gRide != nullptr) {
        if (this == &gRide->mText0C) offset = 0x0c;
        else if (this == &gRide->mText20) offset = 0x20;
        else if (this == &gRide->mText4C) offset = 0x4c;
    }
    ev("StrDtor", {offset, -1});
}

// The vtable of the string needs its other virtuals to link; never called here.
typedef TbDynamicStringTemplate<char16_t> Str;
template<> void Str::ResizeTo(uint32_t) {}
template<> void Str::Format(const char16_t*, ...) {}
template<> void Str::AllocBuffer(uint32_t) {}
template<> Str& Str::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> Str& Str::operator=(const Str&) { return *this; }
template<> Str& Str::operator=(const char16_t*) { return *this; }
template<> Str& Str::operator=(const char16_t&) { return *this; }
template<> Str& Str::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> Str& Str::operator+=(const char16_t&) { return *this; }
template<> Str& Str::operator+=(const char16_t*) { return *this; }

// operator delete is platform/arena.cpp's (not a recording fake): the PowerPC stub records nothing either.

extern "C" {

// ids: node ride ids (0 = the ride under test, 1..4 others); out: count, head id, tail id, then up to 16 ids walked from the head.
int32_t tpw_ride_run(const int32_t* ids, int32_t n, uint32_t flag, int32_t isnull, int32_t* out) {
    typedef TbPtrList<CFSOnlineRide> List;
    CFSOnlineRide* rides[5];
    for (int i = 0; i < 5; i++) rides[i] = static_cast<CFSOnlineRide*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineRide)));
    List* list = static_cast<List*>(tpw::ArenaAllocZeroed(sizeof(List)));
    CFSOnlineSystemRideList* sys = static_cast<CFSOnlineSystemRideList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemRideList)));
    sys->mList = list;
    for (int i = 0; i < 5; i++) rides[i]->mList = sys;
    List::CNode* prev = nullptr;
    for (int i = 0; i < n; i++) {
        List::CNode* node = new List::CNode(rides[ids[i]]);
        node->mPrev = prev;
        if (prev != nullptr) prev->mNext = node; else list->mHead = node;
        prev = node;
        list->mTail = node;
        list->mCount++;
    }
    gRide = rides[0];
    int32_t answer = 0;
    if (!isnull) {
        rides[0]->CFSOnlineRide::~CFSOnlineRide();   // qualified: no virtual dispatch (the object has no vptr)
        (void)flag;   // the delete of the ride itself is the compiler's delete expression; the arena block must not be freed
        answer = 1;
    }
    auto idOf = [&](CFSOnlineRide* p) -> int32_t {
        for (int i = 0; i < 5; i++) if (rides[i] == p) return i;
        return -99;
    };
    out[0] = list->mCount;
    out[1] = list->mHead ? idOf(list->mHead->mData) : -1;
    out[2] = list->mTail ? idOf(list->mTail->mData) : -1;
    int k = 0;
    for (List::CNode* p = list->mHead; p != nullptr && k < 16; p = p->mNext) out[3 + k++] = idOf(p->mData);
    for (; k < 16; k++) out[3 + k] = -1;
    gRide = nullptr;
    return answer;
}

}  // extern "C"
