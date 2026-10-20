#include "tpw_probe.h"
// tb_rendermemory_execute.cpp (engine library of SimTheme Park 2.04b3; the render memory,
// whose asserts name tb_rendermemory.cpp and which the inventory files under
// tb_resource_display.cpp): the deferred drawing of the render memory.
//
//   0x10027a8c CRenderMemory::ExecuteUnrenderedPrimitives      (784 bytes)
//   0x100275cc CRenderMemory::qSortPrimitivesAscending_Texture (332)
//   0x10027764 CRenderMemory::qSortPrimitivesAscending_ZPos    (332)
//   0x100278f8 CRenderMemory::qSortPrimitivesDescending_ZPos   (332)
//
// CRenderList::Render calls ExecuteUnrenderedPrimitives after RenderByTexture. The primitives
// the frame made (CDrawPrimitive::ExecuteCurrentPrimitive leaves the ones that cannot be drawn
// at once in the table) are drawn here, sorted for the renderer: with the OpenGL renderer back
// to front by depth, or in the order the display flags ask for.
#include <algorithm>

#include "Wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"

namespace {

// The record table holds the primitives (the render memory's records are the same objects).
static_assert(sizeof(SPrimitive) == sizeof(SRenderLoadInfo), "a record is a primitive");
SPrimitive* Primitive(CRenderMemory& memory, int32_t number) { return reinterpret_cast<SPrimitive*>(&memory.mInfos[number]); }

// The renderer that sorts only by texture and the display flags that ask for the nearest first.
constexpr int32_t kRendererSortedByTexture = 4;
constexpr uint32_t kDisplayDepthMask = 0x0000000c;
constexpr uint32_t kDisplayNearestFirst = 0x20000000;

// The three sorts are one: the middle element (the 32-bit sum shifted right arithmetically) is
// the pivot, the key of which is read once; the first scan runs up while the element is before it
// and stops at `high`, the second runs down while the pivot is before the element and stops at
// `low`; elements out of place are swapped and the two parts are sorted in turn, the lower first.
// A NaN key (the depth sorts) stops both scans.
template <class Key, class KeyOf, class Before>
void SortRange(int32_t* list, int32_t low, int32_t high, KeyOf keyOf, Before before) {
    const int32_t middle = static_cast<int32_t>(static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1;
    const Key pivot = keyOf(list[middle]);
    int32_t first = low;
    int32_t last = high;
    do {
        while (first <= high && before(keyOf(list[first]), pivot)) ++first;
        while (last > low && before(pivot, keyOf(list[last]))) --last;
        if (last < first) break;
        std::swap(list[first], list[last]);
        --last;
        ++first;
    } while (first <= last);
    if (low < last) SortRange<Key>(list, low, last, keyOf, before);
    if (first < high) SortRange<Key>(list, first, high, keyOf, before);
}

}  // namespace

// Sorted by texture, the lowest address first (unsigned).
void CRenderMemory::qSortPrimitivesAscending_Texture(int32_t* list, int32_t low, int32_t high) {
    SortRange<uintptr_t>(
        list, low, high, [this](int32_t number) { return reinterpret_cast<uintptr_t>(Primitive(*this, number)->mTexture); },
        [](uintptr_t a, uintptr_t b) { return a < b; });
}

// Sorted by depth, the smallest first.
void CRenderMemory::qSortPrimitivesAscending_ZPos(int32_t* list, int32_t low, int32_t high) {
    SortRange<float>(
        list, low, high, [this](int32_t number) { return Primitive(*this, number)->mDepth; },
        [](float a, float b) { return a < b; });
}

// Sorted by depth, the largest first.
void CRenderMemory::qSortPrimitivesDescending_ZPos(int32_t* list, int32_t low, int32_t high) {
    SortRange<float>(
        list, low, high, [this](int32_t number) { return Primitive(*this, number)->mDepth; },
        [](float a, float b) { return b < a; });
}

int32_t CRenderMemory::ExecuteUnrenderedPrimitives() {
    // The records in use: the static ones are the numbers above mNextStatic, the dynamic ones
    // those below mNextDynamic.
    const int32_t records = (mInfoCount - mNextStatic) + mNextDynamic - 1;
    tpw_probe::Hit(tpw_probe::kExecuteUnrendered);
    if (records == 0) return 1;
    void* block = gMalloc(static_cast<uint32_t>(records) * 4);
    if (block == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    int32_t* list = static_cast<int32_t*>(block);
    int32_t used = 0;
    for (int32_t number = 0; number < mNextDynamic; ++number) {
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) == 0) list[used++] = number;
    }
    // A static record that is drawn at once by its blending mode is done with here.
    for (int32_t number = mNextStatic + 1; number < mInfoCount; ++number) {
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) != 0) continue;
        gei_DrawPrimitive->ExecutePrimitiveIfInstant(Primitive(*this, number));
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) == 0) list[used++] = number;
    }

    if (gei_System.mRenderer == kRendererSortedByTexture) {
        // The records with blending flag 0x1000 first (the scans meet in the middle), sorted by
        // texture; the others sorted by depth, farthest first.
        int32_t first = 0;
        int32_t last = used - 1;
        while (first < last) {
            while (first < used && (Primitive(*this, list[first])->mBlendingMode & SPrimitive::kSortSkipped) != 0) ++first;
            while (last > 0 && (Primitive(*this, list[last])->mBlendingMode & SPrimitive::kSortSkipped) == 0) --last;
            if (first <= last) {
                std::swap(list[first], list[last]);
                ++first;
                --last;
            }
        }
        if (first > 1) qSortPrimitivesAscending_Texture(list, 0, first - 1);
        if (used - 1 > first) qSortPrimitivesDescending_ZPos(list, first, used - 1);
    } else if (used > 1) {
        const uint32_t flags = gei_Display.mFlags;
        if ((flags & kDisplayDepthMask) == kDisplayDepthMask && (flags & kDisplayNearestFirst) != 0) {
            qSortPrimitivesAscending_ZPos(list, 0, used - 1);
        } else {
            qSortPrimitivesDescending_ZPos(list, 0, used - 1);
        }
    }

    tpw_probe::Hit(tpw_probe::kRecordsListed, static_cast<uint32_t>(used));
    for (int32_t k = 0; k < used; ++k) {
        if (gei_DrawPrimitive->ExternExecute(Primitive(*this, list[k])) == 0) {
            gFree(block);
            return 0;
        }
    }
    gFree(block);
    return 1;
}
