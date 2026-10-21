// tb_resource_display.cpp: name proven by __sinit_tb_resource_display_cpp in the engine library.

#include "ppc_math.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include <new>

// tb_resource_display.cpp (engine library of SimTheme Park 2.04b3): CDisplay and the
// render list's start-up. PARTIAL: CRenderList's members (0x100262e0..0x10026954) and
// the rest of the inventory's range are still to do; CRenderMemory, which the inventory
// files here, is in tb_rendermemory.cpp (the name its own assertions give).

CDisplay gei_Display;   // built by __sinit_tb_resource_display_cpp (0x1002874c)

CDisplay::CDisplay() {
    mFlags = 0x410100;
    mWidth = 0;
    mHeight = 0;
    mTwoOverWidth = 0.0f;
    mTwoOverHeight = 0.0f;
    mBitsPerPixel = 0;
    mF04 = 0.0f;
    mHalfWidth = 0.0f;
    mHalfHeight = 0.0f;
    mZ68 = 0.0f;
    mZ50 = 0.0f;
    mZ54 = 0.1f;
    mZ58 = 0.2f;
    mZ5C = 1.0f;
    mZ4C = 0.003f;
    mF20 = 0;
    mFrameCount = 0;
    mF84 = 0;
    SetZSpace(1.0f);
    SetFogSpace(100.0f, 130.0f);
    SetFogColor(0x40008000);
    ZSetup(1000.0f, 10.0f, 20.0f);
    mF8B8 = 1.5f;
    mF8E8 = 0.3f;
    mF8EC = 0x80;
    mF8F0 = 0x20;
    mF8F4 = 1;
    mF8F8 = 0;
    mF8FC = 0x4f;
    mFirstPersonViewDistance = 400.0f;
    mF938 = 0.5f;
}

void CDisplay::SetDisplay(int32_t width, int32_t height, int32_t depth) {
    mWidth = width;
    mHeight = height;
    mTwoOverWidth = 2.0f / static_cast<float>(width);
    mTwoOverHeight = 2.0f / static_cast<float>(height);
    mBitsPerPixel = depth;
    mF04 = static_cast<float>(height) / static_cast<float>(width);
    mHalfWidth = (static_cast<float>(mWidth) - 0.999f) * 0.5f;
    mHalfHeight = (static_cast<float>(mHeight) - 0.999f) * 0.5f;
    ResetWindow();
    if (gei_System.mRenderer == 4) mFlags &= ~kFlag10000;
}

void CDisplay::SetWindow(float x, float y, float width, float height) {
    const float screenWidth = static_cast<float>(mWidth);
    const float screenHeight = static_cast<float>(mHeight);
    const int32_t windowWidth = ppc_fctiwz(width * screenWidth * 0.5f);
    const int32_t windowHeight = ppc_fctiwz(height * screenHeight * 0.5f);
    const int32_t left = ppc_fctiwz((1.0f + x) * screenWidth * 0.5f);
    const int32_t top = ppc_fctiwz(screenHeight * (1.0f - (y + height)) * 0.5f);
    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;
    mWindowHalfWidth = (static_cast<float>(windowWidth) - 0.999f) * 0.5f;
    mWindowHalfHeight = (static_cast<float>(windowHeight) - 0.999f) * 0.5f;
    mWindowCentreX = static_cast<float>(left) + mWindowHalfWidth;
    mWindowCentreY = static_cast<float>(top) + mWindowHalfHeight;
    mWindowCentreXBiased = 0.24975f + mWindowCentreX;
    mWindowCentreYBiased = 0.24975f + mWindowCentreY;
}

void CDisplay::ResetWindow() {
    mWindowWidth = mWidth;
    mWindowHeight = mHeight;
    mWindowHalfWidth = mHalfWidth;
    mWindowHalfHeight = mHalfHeight;
    mWindowCentreX = mWindowHalfWidth;
    mWindowCentreY = mWindowHalfHeight;
    mWindowCentreXBiased = 0.24975f + mWindowCentreX;
    mWindowCentreYBiased = 0.24975f + mWindowCentreY;
}

// For a 16-bit display: mTable16A[i] and mTable16B[i] place the byte i by the red, green
// and blue bit counts. The shift amounts come straight from the counts, so a count over
// 8 shifts everything out, as the PowerPC's shifts do.
void CDisplay::SetBitPattern(SBitData alpha, SBitData red, SBitData green, SBitData blue) {
    mAlphaBits = alpha;
    mRedBits = red;
    mGreenBits = green;
    mBlueBits = blue;
    if (mBitsPerPixel != 16) return;
    const int32_t redBits = red.mBits;
    const int32_t blueBits = blue.mBits;
    const uint32_t blueMask = ppc_slw(1, blueBits) - 1;
    for (uint32_t i = 0; i < 256; ++i) {
        mTable16A[i] = ppc_slw(ppc_srw(i, 8 - redBits), 24 - redBits) | ppc_slw(ppc_srw(i, redBits), redBits + 8);
        mTable16B[i] = ppc_slw(ppc_srw(i, blueBits), 16 - green.mBits) | ppc_slw(i & blueMask, 8 - blueBits);
    }
}

void CDisplay::SetZSpace(float scale) {
    mZ48 = scale;
    mZ60 = scale * (mZ54 - mZ50);
    mZ64 = mZ54 * scale;
}

void CDisplay::ZSetup(float a, float b, float c) {
    float ratio = b / c;
    mZ68 = b;
    mZ74 = ((mZ58 - mZ54) / b) * mZ48;
    mZ78 = mZ54 * mZ48;
    float k = (mZ58 - mZ5C) / (ratio - 1.0f);
    mZ6C = k * mZ48;
    mZ70 = (mZ5C - k) * mZ48;
}

void CDisplay::SetFogSpace(float nearDistance, float farDistance) {
    mFogNear = nearDistance;
    mFogFar = farDistance;
    const float range = farDistance - nearDistance;
    mFogScale = 0.0f < range ? 1.0f / range : 0.0f;
    mFogNearSquared = nearDistance * nearDistance;
    mFogFarSquared = farDistance * farDistance;
    mFogScaleSquared = mFogScale * mFogScale;
}

void CDisplay::SetFogColor(uint32_t argb) {
    mFogColor = argb;
    mFogBlue = static_cast<float>(argb & 0xff);
    mFogAlpha = static_cast<float>(argb >> 24);
    mFogRed = static_cast<float>(argb >> 16 & 0xff);
    mFogGreen = static_cast<float>(argb >> 8 & 0xff);
}

static uint32_t TopBit(uint32_t mask) {
    uint32_t bit = 0x80000000u;
    while (bit != 0 && (mask & bit) == 0) bit >>= 1;
    return bit;
}

void CDisplay::SetMaxBits() {
    mRedMaskBits = TopBit(mRedMask);
    mGreenMaskBits = TopBit(mGreenMask);
    mBlueMaskBits = TopBit(mBlueMask);
    mTopBitsMask = mBlueMaskBits | mRedMaskBits | mGreenMaskBits;
}

void CDisplay::OutputDebugInfo() {}
