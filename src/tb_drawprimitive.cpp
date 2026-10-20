// tb_drawprimitive.cpp (engine library of SimTheme Park 2.04b3; the inventory files the
// range, 0x10006954..0x10008230, under tb_error.cpp): the renderer-independent half of
// the draw primitive. PARTIAL: construction only; the primitive queue, clipping, fog and
// sprite members are still to do.
#include "tb_drawprimitive.h"

#include "tb_ddraw.h"
#include "tb_resource_system.h"

// Set when the card is an NVIDIA one (vendor 0x10de) and the renderer is 1, which the Mac
// build never is. Engine data 0x10052b38, not exported; name invented.
int32_t gDrawPrimitiveNvidia = 0;

CDrawPrimitive::CDrawPrimitive() {
    mFields[0] = 0;
    mFields[4] = 0xffffffff;
    mFields[5] = 0;
    mFields[6] = 0;
    mFields[7] = 0;
    mFields[1] = 0;
    mFields[8] = 0;
    if (gei_DDraw->mDeviceInfo.vendorID != 0x10de) return;
    if (gei_System.mRenderer != 1) return;
    gDrawPrimitiveNvidia = 1;
}

CDrawPrimitive::~CDrawPrimitive() {}
