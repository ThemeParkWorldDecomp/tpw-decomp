#pragma once

#include <cstdint>

#include "TbGraphicFile.h"

struct TbPalette;

namespace bfl {
namespace draw {

// One of Draw.cpp's four static render-context objects (a vtable word on the Mac).
struct CDrawContext { void* mVtable; };
extern CDrawContext gDrawContext8;    // 0x100748dc
extern CDrawContext gDrawContext16;   // 0x100748d8
extern CDrawContext gDrawContext24;   // 0x100748d4
extern CDrawContext gDrawContext32;   // 0x100748d0

// Draw.cpp's drawing state (Mac layout: context +0, surface +4, current +8, view port +0x30, clip +0x58,
// palette dirty flag +0x868; the bytes between belong to SetSpritePalette, outside this unit).
class CDraw {
public:
    CDraw();
    ~CDraw();

    void SetCurrent(TbRenderArea* area);
    void SetCurrent(TbSurface* surface);
    void SetClipRect(const TbRect& rect);
    void SetViewPort(const TbRect& rect);
    const TbRenderArea& GetCurrent() const;
    const TbRect& GetClipRect() const;
    const TbRenderArea& GetViewPort() const;
    void ClearClipRect();
    void ClearViewPort();

    void* mRenderContext;
    TbSurface* mSurface;
    TbRenderArea mCurrent;
    TbRenderArea mViewPort;
    TbRect mClipRect;
    uint8_t mF68[0x868 - 0x68];
    uint32_t mPaletteDirty;

    // Unit v5336: 0x10065a7c, 940 B. Copies the palette to +0x468 and fills the 256-word table at +0x68.
    void SetSpritePalette(const TbPalette& palette, const class TbPixFormat* format);
};

} // namespace draw
} // namespace bfl
