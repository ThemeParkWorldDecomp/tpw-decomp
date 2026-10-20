// Unit v5194: __sinit_ui_2dmap_cpp 0x102319d4 (3776 bytes), the static initialiser of ui_2dmap.cpp: the map object gUI2DMap, a table of 23
// colour entries and seven global strings, each registered for destruction at exit.
#include <cstdint>
#include <cstring>
#include <new>

#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "ui_main.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
extern "C" void __construct_array(void* array, void* constructor, int32_t flags, uint32_t size, uint32_t count);   // runtime glue 0x10238758

alignas(16) CUI2DMapColourEntry gUI2DMapColours[23];       // 0x103970f0
CUI2DMapGlobalNode gUI2DMapImageNodes[6];                  // 0x10397088
CUI2DMapGlobalNode gUI2DMapNameNode;                       // 0x103970e4
CUI2DMapGlobalNode gUI2DMapNode;                           // 0x103b74b4
alignas(16) uint8_t gUI2DMapTgaName[0x40];                 // 0x103970d0
alignas(16) uint8_t gUI2DMapNameStrings[6][0x40];          // 0x10397010
// The same six strings as InitSprites reads them (the name at +8 of a 0x14-byte entry is the string's text on the Mac): the host's
// strings are a different size, so InitSprites reads this table, which the initialiser fills with each string's text.
CUI2DMapImageEntry gUI2DMapImages[6];                      // 0x10397010
uint32_t gUI2DMapCounterA;                                 // 0x103b74c4
uint32_t gUI2DMapCounterB;                                 // 0x103b74c0
uint32_t gUI2DMapVtableSlot;                               // 0x102b296c
uint32_t gUI2DMapDtorVector[2];                            // 0x102689f8
uint32_t gUI2DMapCtorVector[2];                            // 0x10268a00

static_assert(sizeof(TbDynamicStringTemplate<char>) <= 0x40, "string storage");

namespace {

void StoreWord(uint8_t* base, uint32_t offset, uint32_t value) { std::memcpy(base + offset, &value, 4); }

// The original builds each entry in a stack temporary (colour bytes, four zero words, four words never written) and copies it.
const uint8_t kColours[23][4] = {
    {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}, {0x00, 0x00, 0x00, 0xff}, {0x00, 0xff, 0x00, 0xff},
    {0xff, 0xff, 0x00, 0xff}, {0x00, 0x4e, 0xfe, 0xff}, {0x03, 0xff, 0xfc, 0xff}, {0x0b, 0x42, 0x6e, 0xff},
    {0x1f, 0xa6, 0x1f, 0xff}, {0x25, 0xc6, 0x25, 0xff}, {0x00, 0x17, 0x00, 0xff}, {0x00, 0x17, 0x00, 0x80},
    {0x96, 0x19, 0x19, 0xc8}, {0xfa, 0x19, 0x19, 0xc8}, {0x00, 0x00, 0xf8, 0xff}, {0x00, 0xc8, 0xf8, 0xff},
    {0x00, 0xfc, 0x30, 0xff}, {0xf8, 0xa8, 0x20, 0xff}, {0x00, 0x84, 0xf8, 0xff}, {0x00, 0xfc, 0xf8, 0xff},
    {0x00, 0x00, 0x00, 0xff}, {0x00, 0x00, 0xff, 0xff}, {0xc8, 0xfa, 0xc8, 0xff},
};

const char* const kImageNames[6] = {
    ".:data:2dmap:vsprite.tga", ".:data:2dmap:rsprite.tga", ".:data:2dmap:ssprite.tga",
    ".:data:2dmap:lsprite.tga", ".:data:2dmap:esprite.tga", ".:data:2dmap:gsprite.tga",
};

}  // namespace

extern "C" void __sinit_ui_2dmap_cpp() {
    uint8_t* map = gUI2DMap.mBytes;
    gUI2DMapCounterA = 0;
    gUI2DMapCounterB = 0x10000;
    StoreWord(map, 0x00, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gUI2DMapVtableSlot)));
    StoreWord(map, 0x10, 0);
    StoreWord(map, 0x14, 0);
    new (map + 0x2c) TbRenderArea();
    __construct_array(map + 0x60, gUI2DMapCtorVector, 0, 8, 0x4000);
    StoreWord(map, 0x04, 0);
    StoreWord(map, 0x18, 1);
    StoreWord(map, 0x1c, 1);
    StoreWord(map, 0x20, 1);
    StoreWord(map, 0x5c, 0x3f800000);   // 1.0f
    StoreWord(map, 0x20060, 0);
    __register_global_object(map, gUI2DMapDtorVector, &gUI2DMapNode);

    for (int i = 0; i < 23; i++) {
        std::memcpy(gUI2DMapColours[i].mColour, kColours[i], 4);
        std::memset(gUI2DMapColours[i].mZero, 0, sizeof(gUI2DMapColours[i].mZero));
        std::memset(gUI2DMapColours[i].mSpare, 0, sizeof(gUI2DMapColours[i].mSpare));
    }

    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    void* object = new (gUI2DMapTgaName) TbDynamicStringTemplate<char>("2dmap.tga");
    __register_global_object(object, destroy, &gUI2DMapNameNode);
    for (int i = 0; i < 6; i++) {
        object = new (gUI2DMapNameStrings[i]) TbDynamicStringTemplate<char>(kImageNames[i]);
        gUI2DMapImages[i].mName = static_cast<TbStringBase<char>*>(static_cast<TbDynamicStringTemplate<char>*>(object))->mData;
        __register_global_object(object, destroy, &gUI2DMapImageNodes[5 - i]);
    }
}
