// Proof library (candidate 14, libtpw_unit_CUI2DMap_CoasterColour.dylib, tests/test_CUI2DMap_CoasterColour.py). The units of DrawCoasterTracks and of the static
// initialiser test them apart, and the first one fakes the colour scale, so neither sees which array the track colours come from. Here the three run together
// as they ship. The recording colour scale of the reused unit gets another name, so that the link takes the real one from src/game/ui/ui_2dmap.cpp.
#define UI2DMReturnScaleColour UI2DMReturnScaleColour_RecordingFake
#include "unit_CUI2DMap_UpdateSpritePosition.cpp"
#undef UI2DMReturnScaleColour

// The fakes and accessors of tests/unit_sinit_ui_2dmap_cpp.cpp, without its TbRenderArea constructor, which the unit above already defines.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "ui_main.h"

using namespace calllog;

CUI2DMap gUI2DMap;

void UIMain_DestroyString(void*) {}

// ---- the fakes

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("TbString", {lab(this)}, {text});
}
template <> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template <> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobalObject", {lab(object), lab(destructor), lab(node)});
}

extern "C" void __construct_array(void* array, void* constructor, int32_t flags, uint32_t size, uint32_t count) {
    ev("ConstructArray", {lab(array), lab(constructor), flags, static_cast<int64_t>(size), static_cast<int64_t>(count)});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    uint8_t* map = gUI2DMap.mBytes;
    const Entry entries[] = {
        {"map", map},
        {"map2c", gUI2DMap.HostArea()},   // the Mac's area at 0x2c is the host area here
        {"map60", map + 0x60},
        {"tbl", gUI2DMapColours},
        {"ca", &gUI2DMapCounterA},
        {"cb", &gUI2DMapCounterB},
        {"vt", &gUI2DMapVtableSlot},
        {"dvec", gUI2DMapDtorVector},
        {"cvec", gUI2DMapCtorVector},
        {"dglue", reinterpret_cast<const void*>(&UIMain_DestroyString)},
        {"tga", gUI2DMapTgaName},
        {"str0", gUI2DMapNameStrings[0]},
        {"str1", gUI2DMapNameStrings[1]},
        {"str2", gUI2DMapNameStrings[2]},
        {"str3", gUI2DMapNameStrings[3]},
        {"str4", gUI2DMapNameStrings[4]},
        {"str5", gUI2DMapNameStrings[5]},
        {"tgan", &gUI2DMapNameNode},
        {"node0", &gUI2DMapImageNodes[5]},
        {"node1", &gUI2DMapImageNodes[4]},
        {"node2", &gUI2DMapImageNodes[3]},
        {"node3", &gUI2DMapImageNodes[2]},
        {"node4", &gUI2DMapImageNodes[1]},
        {"node5", &gUI2DMapImageNodes[0]},
        {"nodem", &gUI2DMapNode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapColourEntry));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapColourEntry, mZero));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapColourEntry, mSpare));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapGlobalNode));
    out[n++] = static_cast<int64_t>(sizeof(gUI2DMapColours));
    return n;
}

// The low 32 bits of the address of the vtable slot global (what the function stores in the map's first word).
uint32_t tpw_sm_vtlow() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gUI2DMapVtableSlot)); }

void tpw_sm_run() { __sinit_ui_2dmap_cpp(); }

}  // extern "C"
