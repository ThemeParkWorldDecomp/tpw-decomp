// Test library for CFSOnlineTheme's constructor and destructor (unit v5670; libtpw_unit_CFSOnlineTheme_CFSOnlineTheme.dylib,
// tests/test_CFSOnlineTheme_CFSOnlineTheme.py), main app:
//   src/CFSOnlineTheme_CFSOnlineTheme.cpp
//
// Fakes: the string classes (constructors, destructors, operator=) and UniStringToString record events. The list members run for real.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_OnlineList.h"
#include "TPString.h"

using namespace calllog;

// ---- fakes of the string classes (all virtual members need a body for the vtables)
#define STRING_FAKES(CH, CTOR, DTOR)                                                                                        \
    template<> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate() { ev(CTOR, {newlab(this)}); }                         \
    template<> TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() { ev(DTOR, {lab(this)}); }                           \
    template<> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                                  \
    template<> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                      \
    template<> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                                   \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; } \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }            \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; } \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }            \
    template<> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }

STRING_FAKES(char, "CStrCtor", "CStrDtor")
STRING_FAKES(char16_t, "WStrCtor", "WStrDtor")

template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t* text) {
    ev("WAssign", {lab(this)}, {narrow(text).c_str()});
    return *this;
}

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    ev("Uni", {lab(&out), lab(&in)});
    return 0;
}

extern "C" {

uint64_t tpw_th_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_th_alloc_theme() { return tpw_th_alloc(sizeof(CFSOnlineTheme)); }

// A theme list owner: only its list (at the base's mList) is used; the list is real.
uint64_t tpw_th_alloc_sys() {
    void* block = tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystemThemeList));
    CFSOnlineSystemThemeList* sys = reinterpret_cast<CFSOnlineSystemThemeList*>(block);
    sys->mList = new (tpw::ArenaAllocZeroed(sizeof(TbPtrList<CFSOnlineTheme>))) TbPtrList<CFSOnlineTheme>();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sys));
}

uint32_t tpw_th_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_th_add(uint64_t sys, uint64_t theme) {
    reinterpret_cast<CFSOnlineSystemThemeList*>(sys)->mList->AddTail(reinterpret_cast<CFSOnlineTheme*>(theme));
}

// The count, and the values of the nodes head to tail (up to n).
int32_t tpw_th_list(uint64_t sys, uint64_t* out, int32_t n) {
    TbPtrList<CFSOnlineTheme>* list = reinterpret_cast<CFSOnlineSystemThemeList*>(sys)->mList;
    int32_t i = 0;
    for (TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::CNode* node = list->mHead; node != nullptr && i < n; node = node->mNext)
        out[i++] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node->mData));
    return list->mCount;
}

void tpw_th_setlist(uint64_t theme, uint64_t sys) {
    reinterpret_cast<CFSOnlineTheme*>(theme)->mList = reinterpret_cast<CFSOnlineSystemThemeList*>(sys);
}

void tpw_th_get(uint64_t theme, uint64_t* out) {
    CFSOnlineTheme* t = reinterpret_cast<CFSOnlineTheme*>(theme);
    out[0] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(t->mList));
    out[1] = static_cast<uint32_t>(t->mThemeID);
}

uint64_t tpw_th_member(uint64_t theme, int32_t k) {
    CFSOnlineTheme* t = reinterpret_cast<CFSOnlineTheme*>(theme);
    void* p = k == 0 ? static_cast<void*>(&t->mText0C) : k == 1 ? static_cast<void*>(&t->mText20) : static_cast<void*>(&t->mText34);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

int32_t tpw_th_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ccThemeInfo, mStr04));
    out[n++] = static_cast<int64_t>(offsetof(ccThemeInfo, mStr08));
    out[n++] = static_cast<int64_t>(offsetof(ccThemeInfo, mStr0C));
    out[n++] = static_cast<int64_t>(sizeof(ccThemeInfo));
    return n;
}

// ---- the functions
void tpw_th_ctor(uint64_t theme, uint64_t info, uint64_t sys) {
    new (reinterpret_cast<void*>(static_cast<uintptr_t>(theme)))
        CFSOnlineTheme(reinterpret_cast<const ccThemeInfo*>(info), reinterpret_cast<CFSOnlineSystemThemeList*>(sys));
}

// The Mac's destructor takes the hidden delete flag (a short); the delete of the storage is the wrapper's event.
void tpw_th_dtor(uint64_t theme, int32_t flag) {
    if (theme == 0) return;
    CFSOnlineTheme* t = reinterpret_cast<CFSOnlineTheme*>(theme);
    t->CFSOnlineTheme::~CFSOnlineTheme();
    if (static_cast<int16_t>(flag) > 0) ev("Delete", {lab(t)});
}

}  // extern "C"
