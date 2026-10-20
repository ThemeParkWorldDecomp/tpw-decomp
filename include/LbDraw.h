// The Bullfrog library's drawing state (bullfrog symbols shared, LbDraw.cpp, 0x1002b1c0..0x1002b7dc): the render area
// everything draws into, the viewport and the clip rectangle on it, the pointer to the viewport's first pixel and the
// drawing context of the area's depth. PARTIAL: the drawing members (LbDraw_Pixel, Line, Rectangle and the rest, which
// are members of the four context classes) and the other state words (the surface overload of LbDraw_SetCurrent keeps
// its flags and tables in more globals) are untranslated. The names are Ghidra's; the addresses are the Mac's.
//
// The viewport is a rectangle of the render area, in the area's pixels; the clip rectangle is in the viewport's own
// coordinates (0, 0 is the viewport's top left corner) and is never bigger than the part of the viewport that lies
// inside the area.
#pragma once
#include <cstdint>

#include "LbRect.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"

// The drawing contexts RenderContext8, 16, 24 and 32 (one per pixel depth; four-byte objects whose only member on the
// Mac is the vtable pointer, set by __sinit_LbDraw_cpp). Their drawing members are untranslated, so the host keeps a
// plain object per depth for _lbpRenderBase to point at. A unit that translates the classes replaces this.
struct RenderContext {
    int32_t mDepth;   // host only: the depth in bits
};

extern TbRenderArea* _lbpCurrentRA;       // 0x10074880: the area drawn into, NULL for none
extern TbRect _lbCurrentViewPort;         // 0x10074870
extern TbRect _lbCurrentClipRect;         // 0x10074860
extern uint8_t* _lbCurrentOriginPtr;      // 0x1007485c: the area's pixels plus the viewport's left and top
extern RenderContext* _lbpRenderBase;     // 0x10074858: the context of the area's depth
extern RenderContext _lbRenderContext8;   // 0x10074824
extern RenderContext _lbRenderContext16;  // 0x10074820
extern RenderContext _lbRenderContext24;  // 0x1007481c
extern RenderContext _lbRenderContext32;  // 0x10074818
extern TbPixelCaps _lbCurrentPixelCaps;   // 0x10074838 (an export of the library)

// 0x1002b1c0: the viewport is `rect` (put in order) or, for NULL, the whole area; the origin pointer follows and the
// clip rectangle is released. The area has to be set: the original dereferences it whatever the arguments.
void LbDraw_SetViewPort(const TbRect* rect);
// 0x1002b2c4: the clip rectangle is `rect` (in viewport coordinates, put in order) or, for NULL, the whole viewport,
// in either case cut down to the part of the viewport inside the area. A rectangle with nothing in it, or nothing of it
// inside the area, makes the clip rectangle 0, 0, 0, 0. The area has to be set (see above).
void LbDraw_SetClipRect(const TbRect* rect);
// 0x1002b588: draws into `area` from now on, with the whole area as the viewport, the context of its depth (8, 16, 24 or
// 32 bits; any other depth gets the 8-bit context) and the pixel capabilities of its format. NULL stops drawing.
// The overload for a TbSurface (0x1002b690) is untranslated.
void LbDraw_SetCurrent(TbRenderArea* area);

// unit w45a
// 0x1002b554 (8 bytes): the address of the clip rectangle, _lbCurrentClipRect on the Mac (TOC 0x10074860). Untranslated; the 4-bit
// font render method (ltms) calls it through the library's export. Whoever translates it answers &_lbCurrentClipRect.
TbRect* LbDraw_GetClipRect();

// unit v5717
// 0x1002b7dc (148 bytes): LbDraw.cpp's static initialiser: zeroes the viewport, the clip rectangle and the pixel capabilities (the
// host leaves the placeholder contexts alone; the Mac stores their vtable addresses).
void __sinit_LbDraw_cpp();
