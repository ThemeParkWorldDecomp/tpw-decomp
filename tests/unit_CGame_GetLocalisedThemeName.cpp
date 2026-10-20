// Test library for CGame::GetLocalisedThemeName (unit v5015; libtpw_unit_CGame_GetLocalisedThemeName.dylib), main app:
//   src/Game_level_CGame_GetLocalisedThemeName.cpp
//
// Fakes: TPUniStrEq (answers 1 for the global the case's "theme" queue picks), TbITable::GetConstString, UIMain_ConvertMulti, the
// char string's constructor and destructor, and the output string's operator=(const char16_t*) (a derived class that logs).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Game.h"
#include "TPString.h"
#include "TbITable.h"
#include "Text.h"
#include "ui_main.h"

using namespace calllog;

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
// The wide string's virtual functions only fill the vtable (the test never calls them).
#define TB_WIDE_STUBS(C)                                                                                         \
    template <> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                          \
    template <> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                             \
    template <> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                          \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }            \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate<C>&) { return *this; } \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }                          \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }                          \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }           \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }                         \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
TB_WIDE_STUBS(char16_t)
TB_WIDE_STUBS(char)
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("dtor", {});
}

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    ev("ctor", {});
}

InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}   // CText's member; the real one is in a file this library does not compile
CText gText;
TbDynamicStringTemplate<char16_t> gGameThemeFantasy;
TbDynamicStringTemplate<char16_t> gGameThemeHallow;
TbDynamicStringTemplate<char16_t> gGameThemeJungle;
TbDynamicStringTemplate<char16_t> gGameThemeSpace;

namespace {
const TbDynamicStringTemplate<char16_t>* const kThemes[4] = {&gGameThemeFantasy, &gGameThemeHallow, &gGameThemeJungle, &gGameThemeSpace};

struct OutString : TbDynamicStringTemplate<char16_t> {
    TbDynamicStringTemplate<char16_t>& operator=(const char16_t* text) override {
        ev("assign", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text)), 1});
        return *this;
    }
};
}  // namespace

bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>& a, const TbDynamicStringTemplate<char16_t>& b) {
    ev("TPUniStrEq", {lab(&a), lab(&b)});
    int64_t pick = ret("theme");
    return pick >= 0 && pick < 4 && &b == kThemes[pick];
}

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("Convert", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
    return reinterpret_cast<char16_t*>(reinterpret_cast<uintptr_t>(text) + 0x100);
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(0x1000 + id * 16));
}

extern "C" {

uint64_t tpw_gltn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fantasy", &gGameThemeFantasy}, {"hallow", &gGameThemeHallow}, {"jungle", &gGameThemeJungle}, {"space", &gGameThemeSpace},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gltn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_gltn_makeout() {
    void* p = tpw::ArenaAllocZeroed(sizeof(OutString));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (p) OutString()));
}

void tpw_gltn_settable(uint64_t table) { gText.mResourcePointers[22] = reinterpret_cast<void*>(static_cast<uintptr_t>(table)); }

int32_t tpw_gltn_layout(int64_t*) { return 0; }

void tpw_gltn_call(uint64_t self, uint64_t theme, uint64_t out) {
    reinterpret_cast<CGame*>(static_cast<uintptr_t>(self))
        ->GetLocalisedThemeName(*reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(static_cast<uintptr_t>(theme)),
                                *reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(static_cast<uintptr_t>(out)));
}

}  // extern "C"
