// Unit v5261: the layout commands that act on the editor's coaster (LayoutSetMovePylon and its neighbours, 0x10058c08 .. 0x1005901c).
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"

// Callees declared here (not in a header): DoDeleteMultiPylons 0x10053f98; SetCoasterEdit 0x100585f4 takes the instance.
void DoDeleteMultiPylons();
void SetCoasterEdit(void* coaster, int32_t flag);

namespace {

// The braces' pylon record sits at the brace's +0x100; its table of brace pointers starts at +0x30 (a pointer each: 4 bytes on the Mac).
tag_sTrackBrace* PylonTableEntry(tag_sTrackBrace* brace, int32_t wordOffset) {
    uint32_t index;
    std::memcpy(&index, reinterpret_cast<const uint8_t*>(brace) + 4, 4);   // the brace's word +4
    tag_sTrackBrace* const* table = reinterpret_cast<tag_sTrackBrace* const*>(reinterpret_cast<const uint8_t*>(brace->mData) + 0x30);
    return table[index + wordOffset];
}

sCoasterTrackParams* EditParams() {
    return reinterpret_cast<sCoasterTrackParams*>(&gCoasterEditInstance->mBlock3C);   // +0x3c
}

}  // namespace

// 0x10058c08 (76 bytes): swaps the brace with the table entry two places on (offset 0x38 - 0x30).
void LayoutStackPylonUp(uint16_t id) {
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), id);
    ExchangeTrackBraces(brace, PylonTableEntry(brace, 2));
}

// 0x10058c80 (76 bytes)
void LayoutStackPylonDown(uint16_t id) {
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), id);
    ExchangeTrackBraces(brace, PylonTableEntry(brace, 0));
}

// 0x10058cf8 (120 bytes)
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b) {
    gCoasterDragBrace = GetTrackBrace(EditParams(), a);
    DoDeleteMultiPylons();
    gCoasterDragBrace = GetTrackBrace(EditParams(), b);
    DoDeleteMultiPylons();
}

// 0x10058da4 (116 bytes); the second argument is unused
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c) {
    (void)a;
    (void)b;
    DisableCurrentPylonEditState();
    gCoasterWord16E0 = 2;
    tag_sTrackBrace* brace = GetTrackBrace(EditParams(), c);
    gCoasterDragBrace = brace;
    StartMovePylon(brace);
}

// 0x10058e48 (76 bytes)
void LayoutRegenerateTrack() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    if (instance->mFlags & 1) {
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        GenerateTrack(instance);
    }
}

// 0x10058ec0 (92 bytes)
void LayoutRegenerateTrack(uint32_t id) {
    tag_sCoasterInstance* instance = static_cast<tag_sCoasterInstance*>(gObjectTable[id]);
    if (instance != nullptr && (instance->mFlags & 1)) {
        gCoasterCurrent = instance;
        gCoasterCurrentData = &instance->mBlock3C;
        GenerateTrack(instance);
    }
}

// 0x10058fa0 (76 bytes)
void LayoutSetCoasterEditOld(uint32_t entrance) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    SetCoasterEdit(instance, entrance != 0 ? 1 : 0);
}

// 0x1005901c (88 bytes)
void LayoutSetCoasterEdit(uint32_t id) {
    void* instance = id != 0 ? gObjectTable[id] : nullptr;
    SetCoasterEdit(instance, id != 0 ? 1 : 0);
}
