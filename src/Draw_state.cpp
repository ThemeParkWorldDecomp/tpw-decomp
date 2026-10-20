// Unit w242: bfl::draw::CDraw state management (bullfrog symbols shared, Draw.cpp, 0x100651b8..0x100659a4).
// SetSpritePalette (0x10065a7c) is not part of this unit.
#include "DrawState.h"

#include <cstring>

namespace bfl {
namespace draw {

// The four render-context objects Draw.cpp's static initialiser (0x100661a8) builds: one word each, the vtable of
// the 8-, 16-, 24- and 32-bit renderer (0x100739a4, 0x10073958, 0x1007390c, 0x100738c0). CDraw only takes their
// addresses, so on the host they are opaque words.
CDrawContext gDrawContext8{};    // 0x100748dc
CDrawContext gDrawContext16{};   // 0x100748d8
CDrawContext gDrawContext24{};   // 0x100748d4
CDrawContext gDrawContext32{};   // 0x100748d0

namespace {

// SetCurrent's format test: the four mask words, the depth and whether the area is palettised (as a bool).
bool SameFormat(const TbRenderArea& a, const TbRenderArea& b) {
    return std::memcmp(a.mF00, b.mF00, sizeof(a.mF00)) == 0 &&
           a.mDepth == b.mDepth &&
           ((a.mPalettised == 0) == (b.mPalettised == 0));
}

}  // namespace

// 0x100651b8: both areas through the library's out-of-line constructor (0x10016630, which leaves the masks alone),
// then the clip, mCurrent's data pointer, the context, the surface and the dirty flag are zeroed.
CDraw::CDraw() {
    mClipRect = {0, 0, 0, 0};
    mCurrent.mData = nullptr;
    mRenderContext = nullptr;
    mSurface = nullptr;
    mPaletteDirty = 0;
}

// 0x1006524c: nothing to release.
CDraw::~CDraw() = default;

// 0x100652bc: a null area changes nothing. A format change marks the palette dirty; the area becomes both the
// current area and the view port, the clip covers it, and the context follows the depth (none when palettised).
void CDraw::SetCurrent(TbRenderArea* area) {
    if (!area) return;
    if (!SameFormat(mCurrent, *area)) mPaletteDirty = 1;
    mCurrent = *area;
    mViewPort = mCurrent;
    mClipRect = {0, 0, area->mWidth, area->mPictureHeight};
    if (mCurrent.mPalettised != 0) {
        mRenderContext = nullptr;
        return;
    }
    switch (mCurrent.mDepth) {
    case 8: mRenderContext = &gDrawContext8; break;
    case 16: mRenderContext = &gDrawContext16; break;
    case 24: mRenderContext = &gDrawContext24; break;
    case 32: mRenderContext = &gDrawContext32; break;
    default: mRenderContext = nullptr; break;
    }
}

// 0x1006552c: a null surface only clears the context; otherwise a copy of the surface's area goes to the other
// overload.
void CDraw::SetCurrent(TbSurface* surface) {
    if (!surface) {
        mRenderContext = nullptr;
        return;
    }
    TbRenderArea area = surface->GetRenderArea();
    SetCurrent(&area);
}

// 0x1006591c: the clip covers the whole view port.
void CDraw::ClearClipRect() {
    mClipRect = {0, 0, mViewPort.mWidth, mViewPort.mPictureHeight};
}

// 0x1006560c: the view port's rectangle cut by `rect`.
void CDraw::SetClipRect(const TbRect& rect) {
    ClearClipRect();
    mClipRect = mClipRect.Intersection(rect);
}

// 0x100656f0: `rect` cut to the current area becomes the view port, and the clip is the whole view port cut by the
// cut rectangle's size.
void CDraw::SetViewPort(const TbRect& rect) {
    const TbRect bounds{0, 0, mCurrent.mWidth, mCurrent.mPictureHeight};
    const TbRect clipped = bounds.Intersection(rect);
    mCurrent.SetupClientRA(mViewPort, clipped);
    const TbRect size{0, 0, clipped.right - clipped.left, clipped.bottom - clipped.top};
    ClearClipRect();
    mClipRect = mClipRect.Intersection(size);
}

const TbRenderArea& CDraw::GetCurrent() const { return mCurrent; }   // 0x10065868
const TbRect& CDraw::GetClipRect() const { return mClipRect; }       // 0x100658a4
const TbRenderArea& CDraw::GetViewPort() const { return mViewPort; } // 0x100658e0

// 0x100659a4: the view port goes back to the whole current area.
void CDraw::ClearViewPort() {
    mViewPort = mCurrent;
    ClearClipRect();
}

}  // namespace draw
}  // namespace bfl
