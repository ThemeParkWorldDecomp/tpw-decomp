#include "RenderContextDepthCircles.h"
#include "RenderContext8.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "LbSurface.h"
#include "LbDrawText.h"
#include "TpwPropFont.h"

#include <cstring>

// bullfrog 0x1002b7dc, 148 B: LbDraw.cpp's static initialiser. Zeroes the viewport (0x10074870), the clip rectangle
// (0x10074860) and the pixel capabilities (0x10074838, 32 bytes), then stores the vtable addresses of the four render
// contexts (0x10073720, 0x1007360c, 0x100736c4, 0x10073668) as the only word of _lbRenderContext8/16/24/32.
// On the host the contexts are placeholder objects holding the depth, and the vtables are not translated, so the
// context stores are not reproduced here (the placeholder's mDepth must stay intact).
void __sinit_LbDraw_cpp() {
    std::memset(&_lbCurrentViewPort, 0, sizeof(_lbCurrentViewPort));
    std::memset(&_lbCurrentClipRect, 0, sizeof(_lbCurrentClipRect));
    std::memset(&_lbCurrentPixelCaps, 0, sizeof(_lbCurrentPixelCaps));
}
