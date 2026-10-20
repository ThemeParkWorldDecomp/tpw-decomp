#include <cstddef>
#include "RenderContext8.h"
#include "LbDraw.h"
#include "LbSurface.h"

struct TbPalette { uint8_t bytes[1024]; };
TbPalette _lbGlobalPalette;
TbPixelCaps _lbCurrentPixelCaps;

TbRenderArea* _lbpCurrentRA;
TbRect _lbCurrentClipRect;
uint8_t* _lbCurrentOriginPtr;
uint32_t _lbCurrentDrawFlags;
uint8_t* _lbCurrentGhostTable;
uint8_t* _lbpCurrentFadeRow;

extern "C" {
uint64_t tpw_rcircle_global(unsigned i) {
    const void* globals[]={&_lbpCurrentRA,&_lbCurrentClipRect,&_lbCurrentOriginPtr,
        &_lbCurrentDrawFlags,&_lbCurrentGhostTable,&_lbpCurrentFadeRow};
    return reinterpret_cast<uintptr_t>(globals[i]);
}
uint32_t tpw_rcircle_layout(unsigned i) {
    const uint32_t offsets[]={sizeof(TbRenderArea),offsetof(TbRenderArea,mData),
        offsetof(TbRenderArea,mWidth),offsetof(TbRenderArea,mPictureHeight),offsetof(TbRenderArea,mStride)};
    return offsets[i];
}
void tpw_rcircle_run(int32_t x,int32_t y,uint32_t radius,const TbColour* colour) {
    RenderContext8 context;
    context.LbDraw_CircleOutline(x,y,radius,*colour);
}
}
