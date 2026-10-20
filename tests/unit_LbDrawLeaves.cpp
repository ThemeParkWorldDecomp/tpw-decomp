// Fakes and test entry points for the Bullfrog library's rectangle, viewport, clip and palette leaves
// (src/bfl_lbdraw_leaves.cpp; libtpw_unit_LbDrawLeaves.dylib, driven by test_LbDrawLeaves.py; see calllog.h).
//
// The only callee outside the unit is TbPixelCaps::Set, which LbDraw_SetCurrent calls. Its fake records the capabilities
// object, the format it is given (by label, so that an area passed as the format is told from any other) and the format's
// six words, and writes those words into the capabilities so that the object is compared too.
//
// The drawing state is driven by a script on a small world: kAreas render areas the case describes, and a list of
// operations (set the current area, set the viewport, set the clip rectangle). After every operation the state is dumped
// into a list of words (the current area as its number, the viewport and clip rectangles, the origin pointer's low 32
// bits, the context's depth, the capabilities, and every area's words) that the PowerPC side builds the same way.
#include <cstdint>
#include <cstring>
#include <new>

#include "LbDraw.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "TbPixFmt.h"
#include "calllog.h"

using calllog::ev;
using calllog::lab;

// ---- the callee

void TbPixelCaps::Set(const TbPixFormat& format) {
    ev("TbPixelCaps::Set", {lab(this), lab(&format), format.mRedMask, format.mGreenMask, format.mBlueMask, format.mAlphaMask,
                            format.mBitsPerPixel, format.mPalettised});
    const uint32_t words[8] = {format.mRedMask, format.mGreenMask, format.mBlueMask, format.mAlphaMask,
                               static_cast<uint32_t>(format.mBitsPerPixel), static_cast<uint32_t>(format.mPalettised), 0xca95, 0};
    std::memcpy(this, words, sizeof words);
}

namespace {

constexpr int kAreas = 6;
constexpr int kWordsPerArea = 10;   // masks (4), depth, palettised, pixels, width, height, stride
alignas(TbRenderArea) unsigned char gAreaStore[kAreas][sizeof(TbRenderArea)];

TbRenderArea* Area(int index) { return reinterpret_cast<TbRenderArea*>(gAreaStore[index]); }

// What the PowerPC side dumps as the area number: 0..kAreas-1, -1 for none, -2 for any other pointer.
int32_t NumberOf(const TbRenderArea* area) {
    if (area == nullptr) return -1;
    for (int i = 0; i < kAreas; ++i)
        if (area == Area(i)) return i;
    return -2;
}

int32_t DepthOfContext() {
    if (_lbpRenderBase == nullptr) return 0;
    if (_lbpRenderBase == &_lbRenderContext8) return 8;
    if (_lbpRenderBase == &_lbRenderContext16) return 16;
    if (_lbpRenderBase == &_lbRenderContext24) return 24;
    if (_lbpRenderBase == &_lbRenderContext32) return 32;
    return -2;
}

}  // namespace

extern "C" {

// The rectangle helpers on caller-owned rectangles of four words.
int32_t tpw_lb_intersection(int32_t* rect, const int32_t* other) {
    return LbRect_Intersection(reinterpret_cast<TbRect*>(rect), reinterpret_cast<const TbRect*>(other));
}
void tpw_lb_empty(int32_t* rect) { LbRect_Empty(reinterpret_cast<TbRect*>(rect)); }
void tpw_lb_normalise(int32_t* rect) { LbRect_Normalise(reinterpret_cast<TbRect*>(rect)); }

// The palette search on 1,024 bytes of palette.
int32_t tpw_lb_find(const uint8_t* palette, int32_t red, int32_t green, int32_t blue) {
    return LbPalette_FindColour(reinterpret_cast<const TbPalette*>(palette), static_cast<uint8_t>(red), static_cast<uint8_t>(green),
                                static_cast<uint8_t>(blue));
}

// Runs a script. world: kWordsPerArea words for each of nAreas areas (the pixels word is any number, never dereferenced).
// ops: six words each: kind (0 SetCurrent of area a, -1 for NULL; 1 SetViewPort; 2 SetClipRect; for 1 and 2 `a` says whether
// there is a rectangle, b..e is it). out gets, after each operation, the state dump; the answer is the number of words.
int32_t tpw_lb_run(const int32_t* world, int32_t nAreas, const int32_t* ops, int32_t nOps, int64_t* out) {
    // The state starts as the library's static initialiser leaves it.
    _lbpCurrentRA = nullptr;
    _lbCurrentViewPort = {0, 0, 0, 0};
    _lbCurrentClipRect = {0, 0, 0, 0};
    _lbCurrentOriginPtr = nullptr;
    _lbpRenderBase = nullptr;
    std::memset(&_lbCurrentPixelCaps, 0, sizeof _lbCurrentPixelCaps);
    tpw_cl_label(1, &_lbCurrentPixelCaps);
    for (int i = 0; i < nAreas; ++i) {
        TbRenderArea* area = new (gAreaStore[i]) TbRenderArea(TbRenderArea::kCleared);
        const int32_t* w = world + i * kWordsPerArea;
        std::memcpy(area->mF00 + 0, &w[0], 4);
        std::memcpy(area->mF00 + 4, &w[1], 4);
        std::memcpy(area->mF00 + 8, &w[2], 4);
        std::memcpy(area->mF00 + 12, &w[3], 4);
        area->mDepth = w[4];
        area->mPalettised = w[5];
        area->mData = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(w[6])));
        area->mWidth = w[7];
        area->mPictureHeight = w[8];
        area->mStride = w[9];
        tpw_cl_label(10 + i, area);
    }
    int32_t n = 0;
    for (int op = 0; op < nOps; ++op) {
        const int32_t* o = ops + op * 6;
        const TbRect rect = {o[2], o[3], o[4], o[5]};
        switch (o[0]) {
        case 0: LbDraw_SetCurrent(o[1] < 0 ? nullptr : Area(o[1])); break;
        case 1: LbDraw_SetViewPort(o[1] != 0 ? &rect : nullptr); break;
        case 2: LbDraw_SetClipRect(o[1] != 0 ? &rect : nullptr); break;
        }
        out[n++] = NumberOf(_lbpCurrentRA);
        out[n++] = _lbCurrentViewPort.left;
        out[n++] = _lbCurrentViewPort.top;
        out[n++] = _lbCurrentViewPort.right;
        out[n++] = _lbCurrentViewPort.bottom;
        out[n++] = _lbCurrentClipRect.left;
        out[n++] = _lbCurrentClipRect.top;
        out[n++] = _lbCurrentClipRect.right;
        out[n++] = _lbCurrentClipRect.bottom;
        out[n++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(_lbCurrentOriginPtr));
        out[n++] = DepthOfContext();
        uint32_t caps[8];
        std::memcpy(caps, &_lbCurrentPixelCaps, sizeof caps);
        for (uint32_t word : caps) out[n++] = word;
        for (int i = 0; i < nAreas; ++i) {
            const TbRenderArea* area = Area(i);
            uint32_t masks[4];
            std::memcpy(masks, area->mF00, sizeof masks);
            for (uint32_t word : masks) out[n++] = word;
            out[n++] = static_cast<uint32_t>(area->mDepth);
            out[n++] = static_cast<uint32_t>(area->mPalettised);
            out[n++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(area->mData));
            out[n++] = static_cast<uint32_t>(area->mWidth);
            out[n++] = static_cast<uint32_t>(area->mPictureHeight);
            out[n++] = static_cast<uint32_t>(area->mStride);
        }
    }
    return n;
}

}  // extern "C"
