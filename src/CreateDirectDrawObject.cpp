// bullfrog WinScrn.cpp, the DirectDraw object, mode list, palette and static initialiser part (unit v5041).
#include "WinScrn.h"

#include <cstddef>
#include <cstring>
#include <new>

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1006b82c

// The file's statics (names invented). The surfaces are 0x50 bytes on the Mac, in the order A, B, C.
TbSurface gWinScrnSurfaceA;
TbSurface gWinScrnSurfaceB;
TbSurface gWinScrnSurfaceC;
_ScreenFPUHandler gScreenFPUHandler;
uint8_t gScreenFPUHandlerNode[0x2c];
const void* const kScreenFPUHandlerVtable[2] = {nullptr, nullptr};
uint8_t kScreenFPUHandlerDtorVector[8];

// 0x1006a514
TbResult CreateDirectDrawObject(NS_MacDoze::_GUID* guid, IDirectDraw** out, void**) {
    TbResult result;
    if (DirectDrawCreate(guid, out, nullptr) != 0) {
        result.mCode = -1;
    } else {
        if (gWinScrnHWnd != nullptr) LbApp_SetHWnd(gWinScrnHWnd);
        result.mCode = 0;
    }
    return result;
}

// 0x1006a45c
bool EnumModesCallback(DDSURFACEDESC* desc, void* list) {
    // The Mac operator new never answers NULL; the listing tests for it all the same.
    TbDisplayMode* node = static_cast<TbDisplayMode*>(::operator new(sizeof(TbDisplayMode), std::nothrow));
    if (node == nullptr) return false;
    node->mWidth = desc->dwWidth;
    node->mHeight = desc->dwHeight;
    node->mBitCount = desc->ddpfPixelFormat.dwRGBBitCount;
    TbList* l = static_cast<TbList*>(list);
    LbList_Insert(l, l->mTailPrev, node);
    return true;
}

// 0x1006a304
TbPaletteEntry* LbScreen_GetPaletteEntries(TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    uint8_t* table = reinterpret_cast<uint8_t*>(&_lbGlobalPalette) + start * 4;
    if (gWinScrnPalette == nullptr) {
        std::memcpy(entries, table, count << 2);
    } else if (gWinScrnPalette->GetEntries(0, start, count, reinterpret_cast<TbPaletteEntry*>(table)) == 0) {
        std::memcpy(entries, table, count << 2);
    }
    return entries;
}

// 0x1006a220
void LbScreen_SetPaletteEntries(const TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    std::memcpy(reinterpret_cast<uint8_t*>(&_lbGlobalPalette) + start * 4, entries, count << 2);
    if (gWinScrnPalette != nullptr && gWinScrnSurfaceB.mDDSurface != nullptr)
        gWinScrnPalette->SetEntries(0, start, count, entries);
}

// 0x1006a198
void LbScreen_SwapClear(TbColour colour) {
    LbScreen_Swap(0);
    LbSurface_ClearRect(gWinScrnSurface, nullptr, colour, 0);
}

// 0x1006a968
static void InitSurface(TbSurface& s) {
    for (int i = 0; i < 5; i++) s.mColour[i] = 0;
    s.mF20 = 0;
    s.mDDSurface = nullptr;
    new (&s.mArea) TbRenderArea();   // the out-of-line constructor, 0x10016630
}

void __sinit_WinScrn_cpp() {
    InitSurface(gWinScrnSurfaceC);
    InitSurface(gWinScrnSurfaceB);
    InitSurface(gWinScrnSurfaceA);
    gScreenFPUHandler.mF00 = 0;
    gScreenFPUHandler.mVtable = kScreenFPUHandlerVtable;
    gScreenFPUHandler.mF08 = 0;
    gScreenFPUHandler.mF0A = 0;
    gScreenFPUHandler.mF0C = 0;
    __register_global_object(&gScreenFPUHandler, kScreenFPUHandlerDtorVector, gScreenFPUHandlerNode);
}
