#include <cstdint>
#include "WinScrn.h"
#include "platform/macdoze.h"
#include "LbSurface.h"

// 0x10069cac, 116 B
TbResult LbScreen_RestoreMode() {
    ReleaseMode();
    if (gWinScrnDirectDraw != nullptr) {
        gWinScrnDirectDraw->RestoreDisplayMode();
        reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnDirectDraw)->Release();
        gWinScrnDirectDraw = nullptr;
        LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
    }
    return {0};
}
