// Test library for CFSOnlineRide's constructor (unit v5141; libtpw_unit_CFSOnlineRide_CFSOnlineRide.dylib,
// tests/test_CFSOnlineRide_CFSOnlineRide.py), main app:
//   src/game/online/FS_OnlineList.cpp   CFSOnlineRide::CFSOnlineRide and the (empty) destructor
//
// The string constructor and the virtual assignment from a wide C string are recording fakes (the same events as the module's
// PowerPC stubs). The list's AddTail is the header's real template, run on both sides; the helpers below build and read the lists.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"

using namespace calllog;

// ---- the fakes of the string class (its members are defined in files this library does not compile)
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {
    ev("StrCtor", {newlab(this)});
}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t* text) {
    ev("StrAssign", {static_cast<int64_t>(lab(this))}, {narrow(text).c_str()});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

typedef TbTList<CFSOnlineRide*, CFSOnlineRide*>::CNode RideNode;

extern "C" {

uint64_t tpw_rd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The base a Ptr32 offset counts from.
uint64_t tpw_rd_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_rd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ccRideInfo));
    out[n++] = static_cast<int64_t>(offsetof(ccRideInfo, mStr04));
    out[n++] = static_cast<int64_t>(offsetof(ccRideInfo, mStr24));
    out[n++] = static_cast<int64_t>(offsetof(ccRideInfo, mF30));
    return n;
}

// A ride list holding `count` nodes whose values are the fake pointers 0x1000 + i (made with the real AddTail; the list's vtable is
// not used by AddTail, so the zeroed blocks stand in for the constructed objects).
uint64_t tpw_rd_newlist(int32_t count) {
    auto* sys = static_cast<CFSOnlineSystemRideList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemRideList)));
    sys->mList = static_cast<TbPtrList<CFSOnlineRide>*>(tpw::ArenaAllocZeroed(sizeof(TbPtrList<CFSOnlineRide>)));
    for (int32_t i = 0; i < count; ++i)
        sys->mList->AddTail(reinterpret_cast<CFSOnlineRide*>(static_cast<uintptr_t>(0x1000 + i)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sys));
}

uint64_t tpw_rd_ctor(uint64_t info, uint64_t list) {
    void* mem = tpw::ArenaAllocZeroed(0x200);
    CFSOnlineRide* ride = new (mem) CFSOnlineRide(reinterpret_cast<const ccRideInfo*>(info),
                                                  reinterpret_cast<CFSOnlineSystemRideList*>(list));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ride));
}

// The ride's members: [list matches, id, f34..f48 (6), f60, f64, f68].
int32_t tpw_rd_read(uint64_t ridep, uint64_t list, int64_t* out) {
    auto* r = reinterpret_cast<CFSOnlineRide*>(ridep);
    int32_t n = 0;
    out[n++] = r->mList == reinterpret_cast<CFSOnlineSystemRideList*>(list) ? 1 : 0;
    out[n++] = static_cast<uint32_t>(r->mRideID);
    out[n++] = static_cast<uint32_t>(r->mF34);
    out[n++] = static_cast<uint32_t>(r->mF38);
    out[n++] = static_cast<uint32_t>(r->mF3C);
    out[n++] = static_cast<uint32_t>(r->mF40);
    out[n++] = static_cast<uint32_t>(r->mF44);
    out[n++] = static_cast<uint32_t>(r->mF48);
    out[n++] = static_cast<uint32_t>(r->mF60);
    out[n++] = static_cast<uint32_t>(r->mF64);
    out[n++] = static_cast<uint32_t>(r->mF68);
    return n;
}

// The list: [count, nodes walked, then per node (value code, next code, prev code), tail code]; a node code is its index + 1 (0 none),
// a value is 1 for the ride, else its low 32 bits.
int32_t tpw_rd_listread(uint64_t listp, uint64_t ridep, int64_t* out) {
    auto* sys = reinterpret_cast<CFSOnlineSystemRideList*>(listp);
    auto* l = sys->mList;
    std::vector<RideNode*> nodes;
    for (RideNode* p = l->mHead; p != nullptr && nodes.size() < 8; p = p->mNext) nodes.push_back(p);
    auto code = [&](RideNode* p) -> int64_t {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i] == p) return static_cast<int64_t>(i + 1);
        return 0;
    };
    int32_t n = 0;
    out[n++] = static_cast<uint32_t>(l->mCount);
    out[n++] = static_cast<int64_t>(nodes.size());
    for (RideNode* p : nodes) {
        out[n++] = reinterpret_cast<uintptr_t>(p->mData) == static_cast<uintptr_t>(ridep)
                       ? 1
                       : static_cast<int64_t>(reinterpret_cast<uintptr_t>(p->mData) & 0xffffffffu);
        out[n++] = code(p->mNext);
        out[n++] = code(p->mPrev);
    }
    out[n++] = code(l->mTail);
    return n;
}

}  // extern "C"
