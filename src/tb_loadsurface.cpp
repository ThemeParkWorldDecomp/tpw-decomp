// tb_loadsurface.cpp: CLoadSurface::Display 0x10017270 (engine library, the inventory's
// tb_mapzoom.cpp range; declared in include/MapWho.h).
#include "MapWho.h"

#include "tb_ddraw.h"
#include "tb_error.h"

int32_t CLoadSurface::Display() {
    gDDResult = gei_DDraw->mBack->Blt(nullptr, mSurface, nullptr, 0x01000000, nullptr);   // DDBLT_WAIT
    if (gDDResult != 0) {
        gei_ErrorHandler.Push(2, gDDResult);
        return 0;
    }
    return 1;
}
