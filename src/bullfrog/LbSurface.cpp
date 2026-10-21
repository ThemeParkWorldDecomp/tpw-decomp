// LbSurface.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "LbDraw.h"
#include "LbPalette.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "platform/macdoze.h"
#include "WinScrn.h"
#include <cstring>
#include <initializer_list>

// Bullfrog WinScrn.cpp surface operations, unit w231.

uint32_t _lbCurrentDrawFlags;
uint8_t* _lbCurrentGhostTable;
uint8_t* _lbpCurrentFadeTable;
uint8_t* _lbpCurrentFadeRow;
uint8_t gSurfaceSysLockDisabled;
IDirectDraw* gSurfaceDirectDraw;

namespace {
constexpr int32_t kLost = static_cast<int32_t>(0x887601c2u);
using LongRect = SamsUtilities::URectangle::LongRect;

// The original copies each two-word pair before moving to the next pair.
void CopyFormat(void* to, const void* from) {
    auto* d = static_cast<uint8_t*>(to);
    const auto* s = static_cast<const uint8_t*>(from);
    for (unsigned i = 0; i != 24; i += 8) {
        uint32_t a, b;
        std::memcpy(&a, s + i, 4);
        std::memcpy(&b, s + i + 4, 4);
        std::memcpy(d + i, &a, 4);
        std::memcpy(d + i + 4, &b, 4);
    }
}
uint8_t* FadeOffset(uint8_t* base, uint32_t offset) {
    return reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(base) + offset);
}
uint32_t ShiftRight(uint32_t word, uint32_t shift) {
    shift &= 63;
    return shift < 32 ? word >> shift : 0;
}
uint32_t ShiftLeft(uint32_t word, uint32_t shift) {
    shift &= 63;
    return shift < 32 ? word << shift : 0;
}
uint32_t ColourWord(const uint8_t* c) {
    return uint32_t(c[0]) << 24 | uint32_t(c[1]) << 16 | uint32_t(c[2]) << 8 | c[3];
}
uint32_t ConvertedColour(const TbRenderArea& area, uint32_t colour) {
    uint32_t masks[4];
    std::memcpy(masks, area.mF00, sizeof masks);
    TbMaskInfo red, green, blue, alpha;
    red.Set(masks[0], 23);
    green.Set(masks[1], 15);
    blue.Set(masks[2], 7);
    alpha.Set(masks[3], 31);
    uint32_t result = 0;
    for (const auto* part : {&red, &green, &blue, &alpha}) {
        result |= ShiftLeft(ShiftRight(colour, part->mShiftLeft), part->mShiftRight) & part->mMask;
    }
    return result;
}
LongRect* DDRect(const TbRect* rect) {
    return reinterpret_cast<LongRect*>(const_cast<TbRect*>(rect));
}
void SourceKey(const TbSurface* source, const TbRenderArea& area) {
    const uint32_t value = area.mDepth == 8 ? source->mColour[4] : ConvertedColour(area, ColourWord(source->mColour));
    DDCOLORKEY key{value, value};
    source->mDDSurface->SetColorKey(8, &key);
}
void RestorePair(const TbSurface* dest, const TbSurface* source) {
    if (dest->mDDSurface->IsLost() == kLost) dest->mDDSurface->Restore();
    if (source->mDDSurface->IsLost() == kLost) source->mDDSurface->Restore();
}
}

TbRenderArea& LbSurface_MapToRenderArea(TbRenderArea& destination, TbSurface* surface) {
    const auto& area = surface->GetRenderArea();
    CopyFormat(&destination, &area);
    destination.mData = area.mData;
    destination.mWidth = area.mWidth;
    destination.mPictureHeight = area.mPictureHeight;
    destination.mStride = area.mStride;
    return destination;
}
TbPixFormat& LbSurface_GetPixelFormat(const TbSurface* surface, TbPixFormat& format) {
    CopyFormat(&format, &surface->GetRenderArea());
    return format;
}
void LbSurface_SetCurrent(TbSurface* s) { LbDraw_SetCurrent(s); }
TbSurface* LbSurface_GetCurrent() { return nullptr; }
uint8_t* LbSurface_GetOriginPtr(const TbSurface*) { return nullptr; }
uint32_t LbSurface_GetDrawFlags(const TbSurface* s) { return s->mDrawFlags; }
void LbSurface_SetDrawFlags(TbSurface* s, uint32_t flags) {
    s->mDrawFlags = flags;
    if (&s->GetRenderArea() == _lbpCurrentRA) _lbCurrentDrawFlags = flags;
}
void LbSurface_SetDrawFlagsOn(TbSurface* s, uint32_t flags) {
    s->mDrawFlags |= flags;
    if (&s->GetRenderArea() == _lbpCurrentRA) _lbCurrentDrawFlags |= flags;
}
void LbSurface_SetDrawFlagsOff(TbSurface* s, uint32_t flags) {
    s->mDrawFlags &= ~flags;
    if (&s->GetRenderArea() == _lbpCurrentRA) _lbCurrentDrawFlags &= ~flags;
}
uint8_t* LbSurface_GetFadeTable(const TbSurface* s) { return s->mFadeTable; }
void LbSurface_SetFadeTable(TbSurface* s, uint8_t* table) {
    s->mFadeTable = table;
    s->mFadeRow = nullptr;
    if (&s->GetRenderArea() == _lbpCurrentRA) {
        _lbpCurrentFadeTable = table;
        _lbpCurrentFadeRow = nullptr;
    }
}
int32_t LbSurface_GetFadeStep(const TbSurface* s) {
    const uint32_t difference = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s->mFadeTable) - reinterpret_cast<uintptr_t>(s->mFadeRow));
    return static_cast<int32_t>(difference) >> 8;
}
void LbSurface_SetFadeStep(TbSurface* s, uint32_t step) {
    const uint32_t offset = step << 8;
    s->mFadeRow = FadeOffset(s->mFadeTable, offset);
    if (&s->GetRenderArea() == _lbpCurrentRA) _lbpCurrentFadeRow = FadeOffset(_lbpCurrentFadeTable, offset);
}
uint8_t LbSurface_GetFadeColour(TbSurface* s, uint32_t index) { return s->mFadeRow[index]; }
uint8_t* LbSurface_GetGhostTable(const TbSurface* s) { return s->mGhostTable; }
void LbSurface_SetGhostTable(TbSurface* s, uint8_t* table) {
    s->mGhostTable = table;
    if (&s->GetRenderArea() == _lbpCurrentRA) _lbCurrentGhostTable = table;
}
void TbSurface::CalculateSurfaceArea(DDSURFACEDESC& desc) const {
    mArea.mData = static_cast<uint8_t*>(desc.lpSurface);
    mArea.mWidth = desc.dwWidth;
    mArea.mPictureHeight = desc.dwHeight;
    mArea.mStride = desc.lPitch;
    mArea.mDepth = desc.ddpfPixelFormat.dwRGBBitCount;
    mArea.mPalettised = mArea.mDepth == 8;
    if (mArea.mDepth != 8) {
        const uint32_t masks[]{desc.ddpfPixelFormat.dwRBitMask, desc.ddpfPixelFormat.dwGBitMask,
                               desc.ddpfPixelFormat.dwBBitMask, desc.ddpfPixelFormat.dwRGBAlphaBitMask};
        std::memcpy(mArea.mF00, masks, sizeof masks);
    }
}
const TbRenderArea& TbSurface::GetRenderArea() const {
    DDSURFACEDESC desc;
    desc.dwSize = 0x6c;
    if (mDDSurface->GetSurfaceDesc(&desc) >= 0) {
        if (desc.lpSurface == nullptr) desc.lpSurface = mArea.mData;
        mArea.mData = static_cast<uint8_t*>(desc.lpSurface);
        mArea.mWidth = desc.dwWidth;
        mArea.mPictureHeight = desc.dwHeight;
        mArea.mStride = desc.lPitch;
        mArea.mDepth = desc.ddpfPixelFormat.dwRGBBitCount;
        mArea.mPalettised = mArea.mDepth == 8;
        if (mArea.mDepth != 8) {
            const uint32_t masks[]{desc.ddpfPixelFormat.dwRBitMask, desc.ddpfPixelFormat.dwGBitMask,
                                   desc.ddpfPixelFormat.dwBBitMask, desc.ddpfPixelFormat.dwRGBAlphaBitMask};
            std::memcpy(mArea.mF00, masks, sizeof masks);
        }
    }
    return mArea;
}
void LbSurface_SetSysLock(int32_t enabled) { gSurfaceSysLockDisabled = enabled == 0; }
void LbSurface_RegisterDirectDraw(IDirectDraw* dd) { gSurfaceDirectDraw = dd; }
TbResult LbSurface_RegisterDirectDrawSurface(IDirectDrawSurface* dd, TbSurface* s) {
    std::memset(s, 0, sizeof *s);
    DDSURFACEDESC desc;
    desc.dwSize = 0x6c;
    desc.dwFlags = 0x100f;
    dd->GetSurfaceDesc(&desc);
    s->mF20 = 1;
    s->mDDSurface = dd;
    _InitialiseSurface(s, desc.dwWidth, desc.dwHeight, desc.ddpfPixelFormat.dwRGBBitCount, 0);
    return {0};
}
void _InitialiseSurface(TbSurface* s, uint32_t, uint32_t, uint32_t depth, uint32_t flags) {
    s->mDrawFlags = 0;
    s->mGhostTable = nullptr;
    s->mFadeTable = nullptr;
    s->mPalette = depth == 8 ? &_lbGlobalPalette : nullptr;
    s->mF1C = flags;
}
int32_t _CheckAndRestoreSurface(const TbSurface* s) {
    if (s->mDDSurface->IsLost() == kLost && s->mDDSurface->Restore() != 0) return 0;
    return 1;
}
TbResult LbSurface_ClearRect(const TbSurface* s, const TbRect* rect, TbColour colour, uint32_t flags) {
    DDBLTFX fx;
    fx.dwSize = 100;
    const auto& area = s->GetRenderArea();
    const uint32_t word = uint32_t(colour.mAlpha) << 24 | uint32_t(colour.mRed) << 16 |
                          uint32_t(colour.mGreen) << 8 | colour.mBlue;
    switch (area.mDepth) {
    case 8: fx.words[19] = colour.mIndex; break;
    case 16: {
        const uint32_t converted = ConvertedColour(area, word);
        fx.words[19] = (converted & 0xffff) | (converted << 16);
        break;
    }
    case 24: fx.words[19] = ConvertedColour(area, word); break;
    case 32: fx.words[19] = word << 8 | word >> 24; break;
    }
    return {s->mDDSurface->Blt(DDRect(rect), nullptr, nullptr, (flags ^ 0x01000000u) | 0x400, &fx) == 0 ? 0 : -1};
}
TbResult LbSurface_Create(uint32_t width, uint32_t height, uint32_t flags, TbSurface* s) {
    DDSURFACEDESC desc;
    desc.dwSize = 0x6c;
    desc.dwFlags = 7;
    desc.dwHeight = height;
    desc.dwWidth = width;
    desc.dwCaps = flags | 0x40;
    if (gSurfaceDirectDraw->CreateSurface(&desc, &s->mDDSurface, nullptr) != 0) return {-1};
    NS_MacDoze::ZeroMemory(&desc, sizeof desc);
    desc.dwSize = 0x6c;
    desc.dwFlags = 0x1009;
    s->mDDSurface->GetSurfaceDesc(&desc);
    s->mDrawFlags = 0;
    s->mGhostTable = nullptr;
    s->mFadeTable = nullptr;
    s->mPalette = desc.ddpfPixelFormat.dwRGBBitCount == 8 ? &_lbGlobalPalette : nullptr;
    s->mF1C = 0;
    return {0};
}
TbResult LbSurface_Delete(TbSurface* s) {
    s->mDDSurface->Release();
    s->mDDSurface = nullptr;
    return {0};
}
TbResult LbSurface_Blt(const TbSurface* dest, uint32_t x, uint32_t y, const TbSurface* source, const TbRect* rect, uint32_t flags) {
    const auto& sourceArea = source->GetRenderArea();
    dest->GetRenderArea();
    TbRect whole{0, 0, 0, 0};
    if (rect == nullptr) {
        whole.right = sourceArea.mWidth;
        whole.bottom = sourceArea.mPictureHeight;
        rect = &whole;
    }
    if (flags & 1) SourceKey(source, sourceArea);
    const int32_t result = dest->mDDSurface->BltFast(x, y, source->mDDSurface, DDRect(rect), flags ^ 0x10);
    if (result == kLost) RestorePair(dest, source);
    return {result == 0 ? 0 : -1};
}
TbResult LbSurface_StretchBlt(const TbSurface* dest, const TbRect* destRect, const TbSurface* source, const TbRect* sourceRect, uint32_t flags) {
    const auto& sourceArea = source->GetRenderArea();
    const auto& destArea = dest->GetRenderArea();
    const TbRect wholeDest{0, 0, destArea.mWidth, destArea.mPictureHeight};
    const TbRect wholeSource{0, 0, sourceArea.mWidth, sourceArea.mPictureHeight};
    TbRect localSource{}, localDest{};
    if (sourceRect == nullptr) {
        LbRect_Copy(&localSource, &wholeSource);
        sourceRect = &localSource;
    }
    if (destRect == nullptr) {
        LbRect_Copy(&localDest, &wholeDest);
        destRect = &localDest;
    }
    DDBLTFX fx;
    std::memset(&fx, 0, sizeof fx);
    fx.dwSize = 100;
    uint32_t ddFlags = flags ^ 0x01000000u;
    if (flags & 0x800) {
        ddFlags |= 0x800;
        fx.words[0] |= 8;
    }
    if (flags & 0x8000) {
        SourceKey(source, sourceArea);
        ddFlags |= 0x8000;
    }
    const int32_t result = dest->mDDSurface->Blt(DDRect(destRect), source->mDDSurface, DDRect(sourceRect), ddFlags, &fx);
    if (result == kLost) RestorePair(dest, source);
    if (result == 0) return {0};
    OS_dd_error(result);
    return {-1};
}
TbResult LbSurface_Lock(TbSurface* s, uint8_t** pixels) {
    DDSURFACEDESC desc;
    desc.dwSize = 0x6c;
    const uint32_t flags = gSurfaceSysLockDisabled != 0 ? 0x801 : 1;
    int32_t result = s->mDDSurface->Lock(nullptr, &desc, flags, nullptr);
    if (result == kLost) {
        if (s->mDDSurface->IsLost() != kLost || s->mDDSurface->Restore() == 0)
            result = s->mDDSurface->Lock(nullptr, &desc, flags, nullptr);
    }
    if (result < 0) return {result};
    s->CalculateSurfaceArea(desc);
    if (pixels != nullptr) *pixels = static_cast<uint8_t*>(desc.lpSurface);
    return {0};
}
void LbSurface_Unlock(TbSurface* s) { s->mDDSurface->Unlock(nullptr); }
IDirectDrawSurface* LbSurface_GetDDSurface(TbSurface* s, int32_t addRef) {
    if (addRef != 0 && s->mDDSurface != nullptr) s->mDDSurface->AddRef();
    return s->mDDSurface;
}

// 0x10067e70: every branch of the original's code comparison tree returns without effect.
void OS_dd_error(int32_t) {}
