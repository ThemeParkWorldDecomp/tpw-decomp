// Test library for CFSOnlinePark's constructor and destructor (unit v5542; libtpw_unit_CFSOnlinePark_CFSOnlinePark.dylib),
//   src/CFSOnlinePark_CFSOnlinePark.cpp
//
// The string class, its virtual methods and the park list's Find/AddTail/RemoveAt are recording fakes (the same events as the
// PowerPC stubs of tests/test_CFSOnlinePark_CFSOnlinePark.py).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"

using namespace calllog;

// ---- the string fakes
#define STR_FAKES(C, NAME)                                                                                        \
    template <> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() { ev(NAME "Ctor", {lab(this)}); }             \
    template <> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() { ev(NAME "Dtor", {lab(this)}); }            \
    template <> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                           \
    template <> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                              \
    template <> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                           \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) {         \
        return *this;                                                                                               \
    }                                                                                                               \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { \
        return *this;                                                                                               \
    }                                                                                                               \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C* text) {                  \
        ev(NAME "Assign", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(static_cast<std::make_unsigned<C>::type>(text[0]))), \
                           static_cast<int64_t>(static_cast<uint32_t>(static_cast<std::make_unsigned<C>::type>(text[1]))),          \
                           static_cast<int64_t>(static_cast<uint32_t>(static_cast<std::make_unsigned<C>::type>(text[2]))),          \
                           static_cast<int64_t>(static_cast<uint32_t>(static_cast<std::make_unsigned<C>::type>(text[3])))});       \
        return *this;                                                                                               \
    }                                                                                                               \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }       \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) {        \
        return *this;                                                                                               \
    }                                                                                                               \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }      \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }

STR_FAKES(char16_t, "W")
STR_FAKES(char, "C")

// ---- the list fakes
template <> void TbTList<CFSOnlinePark*, CFSOnlinePark*>::AddTail(CFSOnlinePark* value) { ev("AddTail", {lab(this), lab(value)}); }
template <> POSITION TbTList<CFSOnlinePark*, CFSOnlinePark*>::Find(CFSOnlinePark* value) {
    ev("Find", {lab(this), lab(value)});
    return reinterpret_cast<POSITION>(static_cast<uintptr_t>(ret("Find")));
}
template <> CFSOnlinePark* TbTList<CFSOnlinePark*, CFSOnlinePark*>::RemoveAt(POSITION& position) {
    ev("RemoveAt", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(position))});
    return nullptr;
}

extern "C" {

uint64_t tpw_pk_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pk_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlinePark) >= 0xc0 ? 1 : 0);
    return n;
}

// The host address of the idx-th string member (0x0c, 0x20, 0x34, 0x7c, 0x94).
uint64_t tpw_pk_member(uint64_t park, int32_t idx) {
    CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(park));
    const void* a = nullptr;
    switch (idx) {
        case 0: a = &p->mText0C; break;
        case 1: a = &p->mText20; break;
        case 2: a = &p->mText34; break;
        case 3: a = &p->mText7C; break;
        default: a = &p->mText94; break;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(a));
}

// Park memory with its list pointer set (the list's own list pointer set too when the list is not NULL).
void tpw_pk_prep(uint64_t park, uint64_t list, uint64_t tlist) {
    CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(park));
    CFSOnlineSystemParkList* l = reinterpret_cast<CFSOnlineSystemParkList*>(static_cast<uintptr_t>(list));
    if (l != nullptr) l->mList = reinterpret_cast<TbPtrList<CFSOnlinePark>*>(static_cast<uintptr_t>(tlist));
    p->mList = l;
}

// The fields copied to a buffer at their Mac offsets (host byte order, 32-bit pointer).
void tpw_pk_dump(uint64_t park, uint8_t* out) {
    CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(park));
    std::memset(out, 0, 0xc0);
    uint32_t l = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->mList));
    std::memcpy(out + 0x04, &l, 4);
    std::memcpy(out + 0x08, &p->mParkID, 4);
    std::memcpy(out + 0x48, &p->mF48, 4);
    std::memcpy(out + 0x4c, &p->mF4C, 4);
    std::memcpy(out + 0x50, &p->mF50, 4);
    std::memcpy(out + 0x54, &p->mF54, 4);
    std::memcpy(out + 0x58, p->mF58, 0x20);
    std::memcpy(out + 0x78, &p->mF78, 4);
    std::memcpy(out + 0x90, &p->mF90, 4);
    std::memcpy(out + 0xa8, &p->mFA8, 2);
    std::memcpy(out + 0xaa, &p->mFAA, 2);
    std::memcpy(out + 0xac, &p->mFAC, 2);
    std::memcpy(out + 0xb0, &p->mFB0, 4);
    std::memcpy(out + 0xb4, &p->mFB4, 4);
    std::memcpy(out + 0xb8, &p->mFB8, 4);
    std::memcpy(out + 0xbc, &p->mFBC, 4);
}

uint32_t tpw_pk_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

void tpw_pk_ctor(uint64_t park, uint64_t info, uint64_t list) {
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(park)))
        CFSOnlinePark(reinterpret_cast<const ccParkInfo*>(static_cast<uintptr_t>(info)),
                      reinterpret_cast<CFSOnlineSystemParkList*>(static_cast<uintptr_t>(list)));
}

void tpw_pk_dtor(uint64_t park, int32_t flag) {
    CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(static_cast<uintptr_t>(park));
    p->CFSOnlinePark::~CFSOnlinePark();
    if (static_cast<int16_t>(flag) > 0) ev("delete", {lab(p)});
}

}  // extern "C"
