// bullfrog WinScrn.cpp (unit v5737): LbScreen_Terminate, LbApp_ShowMessage, LbScreen_GetDirectDraw, the display mode enumeration.
#include "TbFPUPrecision.h"
#include "WinScrn.h"

#include <cstdarg>
#include <cstdio>

#include "LbSCBack.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// 0x10068b10, 232 B
void LbScreen_Terminate() {
    if (gWinScrnInitialised != 0) {
        gWinScrnInitialised = 0;
        ReleaseMode();
        if (gWinScrnDirectDraw != nullptr) {
            gWinScrnDirectDraw->RestoreDisplayMode();
            gWinScrnDirectDraw->Release();
            gWinScrnDirectDraw = nullptr;
            LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
        }
        if (gWinScrnHWnd != nullptr) NS_MacDoze::DestroyWindow(static_cast<GrafPort*>(gWinScrnHWnd));
        gWinScrnHWnd = nullptr;
    }
    while (TbListNode* node = LbList_GetFirst(&gWinScrnModeList)) {
        node->mPrev->mNext = node->mNext;
        node->mNext->mPrev = node->mPrev;
        LbSE_OperatorDelete(node);
    }
}

// 0x10068e54, 240 B
void LbApp_ShowMessage(const char* format, ...) {
    char text[0x82c];
    va_list args;
    va_start(args, format);
    vsprintf(text, format, args);
    va_end(args);
    GrafPort* owner = gWinScrnCoopWindow;
    if (gWinScrnDirectDraw != nullptr) IDirectDraw_FlipToGDISurface(gWinScrnDirectDraw);
    if (gWinScrnCoopWindow != nullptr)
        NS_MacDoze::SetWindowPos(gWinScrnCoopWindow, reinterpret_cast<GrafPort*>(static_cast<intptr_t>(-2)), 0, 0, 0, 0, 3);
    NS_MacDoze::MessageBoxA(owner, text, "TO DO", 0x2000);
    if (gWinScrnCoopWindow != nullptr)
        NS_MacDoze::SetWindowPos(gWinScrnCoopWindow, reinterpret_cast<GrafPort*>(static_cast<intptr_t>(-1)), 0, 0, 0, 0, 3);
}

// 0x100687a8, 72 B
IDirectDraw* LbScreen_GetDirectDraw(int32_t addRef) {
    if (addRef != 0 && gWinScrnDirectDraw != nullptr) gWinScrnDirectDraw->AddRef();
    return gWinScrnDirectDraw;
}

// 0x10068f70, 100 B
TbResult LbScreen_ModeFindFirst(TbScreenModeFind* find, TbScreenMode* mode) {
    find->mCursor = LbList_GetFirst(&gWinScrnModeList);
    return LbScreen_ModeFindNext(find, mode);
}

// 0x10069024, 72 B: the Mac's listing is not in the brief; from the draft. The cursor is a TbDisplayMode.
TbResult LbScreen_ModeFindNext(TbScreenModeFind* find, TbScreenMode* mode) {
    TbResult result;
    TbDisplayMode* node = static_cast<TbDisplayMode*>(find->mCursor);
    if (node->mNext == nullptr) {
        result.mCode = -1;
        return result;
    }
    int32_t* out = reinterpret_cast<int32_t*>(mode);
    uint32_t height = node->mHeight;
    out[0] = static_cast<int32_t>(node->mWidth);
    out[1] = static_cast<int32_t>(height);
    out[2] = static_cast<int32_t>(node->mBitCount);
    find->mCursor = node->mNext;
    result.mCode = 0;
    return result;
}
