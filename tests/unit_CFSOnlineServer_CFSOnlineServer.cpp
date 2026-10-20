// Test library for CFSOnlineServer's constructor (unit v5049; libtpw_unit_CFSOnlineServer_CFSOnlineServer.dylib,
// tests/test_CFSOnlineServer_CFSOnlineServer.py), main app:
//   src/CFSOnlineServer_CFSOnlineServer.cpp   CFSOnlineServer::CFSOnlineServer
//
// The string members' constructor and operator=(const char16_t*) are recording fakes (the same events as the PowerPC stubs); the list's
// AddTail runs for real on both sides.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"

using namespace calllog;

namespace {
CFSOnlineServer* gServer = nullptr;   // the object under construction (the fakes name a string by its member)

int StrIndex(const void* p) {
    if (gServer == nullptr) return -1;
    if (p == &gServer->mText10) return 0;
    if (p == &gServer->mText24) return 1;
    if (p == &gServer->mText40) return 2;
    if (p == &gServer->mText54) return 3;
    if (p == &gServer->mText6C) return 4;
    return -1;
}
}  // namespace

// ---- the vtable's key function and the fakes of the string class (explicit specialisations: the src file only sees the declarations)

template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() { ev("StrCtor", {StrIndex(this)}); }
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) { ev("Unexpected", {1}); }
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) { ev("Unexpected", {2}); }
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) { ev("Unexpected", {3}); }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) {
    ev("Unexpected", {4});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) {
    ev("Unexpected", {5});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t* text) {
    ev("StrAssign", {StrIndex(this), text[0], text[1], text[2], text[3]});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) {
    ev("Unexpected", {6});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) {
    ev("Unexpected", {7});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) {
    ev("Unexpected", {8});
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) {
    ev("Unexpected", {9});
    return *this;
}

extern "C" {

uint64_t tpw_osv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 field holds for a host block.
uint32_t tpw_osv_off(uint64_t address) { return address != 0 ? tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))) : 0; }

int32_t tpw_osv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ccServerInfo));
    out[n++] = static_cast<int64_t>(offsetof(ccServerInfo, mStr22));
    out[n++] = static_cast<int64_t>(offsetof(ccServerInfo, mStr16));
    out[n++] = static_cast<int64_t>(offsetof(ccServerInfo, mF14));
    return n;
}

// A server list with its (empty) pointer list made, as the list's constructor does.
uint64_t tpw_osv_make_list() {
    CFSOnlineSystemServerList* list = static_cast<CFSOnlineSystemServerList*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemServerList)));
    list->mList = new TbPtrList<CFSOnlineServer>;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(list));
}

// The constructor: the object is made in its own block; returns its address.
uint64_t tpw_osv_ctor(uint64_t info, uint64_t list) {
    void* mem = tpw::ArenaAllocZeroed(sizeof(CFSOnlineServer));
    gServer = static_cast<CFSOnlineServer*>(mem);
    CFSOnlineServer* obj = new (mem) CFSOnlineServer(reinterpret_cast<const ccServerInfo*>(static_cast<uintptr_t>(info)),
                                                     reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(list)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(obj));
}

// The scalar members of a server: vtable word (always 1 here), list match, 08, 0c, 38, 3c, 68.
int32_t tpw_osv_srv(uint64_t object, uint64_t list, int64_t* out) {
    const CFSOnlineServer* o = reinterpret_cast<const CFSOnlineServer*>(static_cast<uintptr_t>(object));
    int32_t n = 0;
    out[n++] = 1;
    out[n++] = o->mList == reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(list)) ? 1 : 0;
    out[n++] = static_cast<uint32_t>(o->mF08);
    out[n++] = static_cast<uint32_t>(o->mF0C);
    out[n++] = static_cast<uint32_t>(o->mF38);
    out[n++] = static_cast<uint16_t>(o->mF3C);
    out[n++] = static_cast<uint32_t>(o->mF68);
    return n;
}

// The list: count, kind of head, kind of tail, then per node (up to 5) the kind and whether its prev is the node before.
// A kind is the position (1..n) in objs, 9 for another non-null value, 0 for null.
int32_t tpw_osv_list(uint64_t list_, const uint64_t* objs, int32_t n, int64_t* out) {
    CFSOnlineSystemServerList* list = reinterpret_cast<CFSOnlineSystemServerList*>(static_cast<uintptr_t>(list_));
    auto kind = [&](const void* p) -> int64_t {
        if (p == nullptr) return 0;
        for (int32_t i = 0; i < n; ++i)
            if (reinterpret_cast<uintptr_t>(p) == objs[i]) return i + 1;
        return 9;
    };
    TbPtrList<CFSOnlineServer>* l = list->mList;
    int32_t k = 0;
    out[k++] = l->mCount;
    out[k++] = l->mHead != nullptr ? kind(l->mHead->mData) : 0;
    out[k++] = l->mTail != nullptr ? kind(l->mTail->mData) : 0;
    TbPtrList<CFSOnlineServer>::CNode* prev = nullptr;
    TbPtrList<CFSOnlineServer>::CNode* node = l->mHead;
    for (int i = 0; i < 5; ++i) {
        if (node == nullptr) {
            out[k++] = 0;
            out[k++] = 0;
            continue;
        }
        out[k++] = kind(node->mData);
        out[k++] = node->mPrev == prev ? 1 : 0;
        prev = node;
        node = node->mNext;
    }
    return k;
}

}  // extern "C"
