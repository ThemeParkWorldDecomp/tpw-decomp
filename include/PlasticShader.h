// PlasticShader and ShadowGlow (main app, rsse.cpp range 0x100c8e40..0x100cb804; units w84 and w82): the two image shaders the
// ride sign is composed with (SignRenderer::sign_render, src/SignRenderer.cpp, holds two PlasticShaders and two ShadowGlows by value).
// Offsets describe the Mac's 32-bit layout; a Bitmap is 0x10 bytes there and larger on the host, so sizeof(PlasticShader) is 0x6c only
// on the Mac.
//
// RECONCILE: w82 (SignRenderer) writes a header of the same name with the same two classes and an inline PlasticShader constructor
// (0x100cb594, inside SignRenderer); w84 was written without it. The member names below come from what plastic_shade, bevel and
// blur_with_mask do with the words; the constructor is the one w82 describes (3, 10.0, 1.0, 1.0 in the four named words, the four
// bitmaps cleared; the other words are left alone).
#pragma once
#include <cstdint>

#include "Bitmap.h"

namespace bfl {
namespace FileSystem {
class CFile;
}
}  // namespace bfl

class PlasticShader {
public:
    PlasticShader() : mBevelWidth(3), mHeightScale(10.0f), mBlur(1.0f), mSpecular(1.0f) {
        mMask.clear();
        mFile.clear();
        mBevel.clear();
        mShaded.clear();
    }
    // 0x100ca558 (284 bytes): clear(), then the four bitmaps' inline destructors from mShaded (+0x5c) down to mMask (+0x2c); the
    // deleting form (flag above 0) frees the object.
    ~PlasticShader();
    // 0x100ca6a0 (88 bytes): Bitmap::clear on mMask, mFile, mBevel and mShaded.
    void clear();
    // 0x100ca724 (492 bytes): eleven words of the settings file, 4 bytes each (little-endian in the file), into +0 .. +0x28. Answers 1.
    int32_t load_settings(bfl::FileSystem::CFile* file);
    // 0x100ca95c (468 bytes): the bevel of `mask` (nonzero = inside) in `out` (one byte per pixel, as many as mask has): `width` rounds
    // of erosion, each marking the pixels it strips with a value from a smoothstep-like curve, then the surviving core (0xff) or'ed in.
    void bevel(Bitmap& mask, uint32_t width, Bitmap& out);
    // 0x100cab6c (912 bytes): a 2-D gaussian of sigma `sigma` over the pixels of `bitmap` whose `mask` byte is nonzero, in place.
    void blur_with_mask(Bitmap& bitmap, Bitmap& mask, float sigma);
    // 0x100caf40 (1260 bytes): lights the sign: for every pixel of mMask that is set, the tile of mFile (descimated to
    // mFile.mWidth x mPeriodY) shaded by the slope of `bevel`, into mShaded (4 bytes per pixel: the mask byte, then red, green, blue).
    // Nothing happens while mPeriodY is 0.
    void plastic_shade(Bitmap& bevel);

    uint32_t mBevelWidth;    // 0x00  3: the width the caller passes to bevel()
    float mHeightScale;      // 0x04  10.0: plastic_shade's slope scale (the bevel's byte differences times it over 255)
    float mBlur;             // 0x08  1.0: the sigma the caller passes to blur_with_mask (it blurs when above 0.0)
    float mAmbient;          // 0x0c  the tile colour times it is the unlit colour
    float mDiffuse;          // 0x10  times the tile colour times N.L
    float mSpecular;         // 0x14  1.0: times pow(N.H, mShininess), added to red, green and blue alike
    float mShininess;        // 0x18  the exponent of the specular term
    float mLightYaw;         // 0x1c  degrees (negated by plastic_shade)
    float mLightPitch;       // 0x20  degrees
    int32_t mPeriodY;        // 0x24  the tile's height (w82 calls it mLineWidth: sign_render stores the line's width here); 0 = no shading
    int32_t mOffsetY;        // 0x28  rows subtracted before the tile's wrap (w82: mLineX)
    Bitmap mMask;            // 0x2c  the sign's text mask (SignRenderer mMask1/2)
    Bitmap mFile;            // 0x3c  the texture the settings file names (loaded by the caller)
    Bitmap mBevel;           // 0x4c  the bevel (not touched by this unit's functions)
    Bitmap mShaded;          // 0x5c  plastic_shade's output
};

// The glow behind a sign's text: the text mask grown or shrunk, moved and blurred.
struct ShadowGlow {
    // 0x100cb7c0 (68 bytes): the deleting form frees the object (nothing else to destroy).
    ~ShadowGlow();
    // 0x100cb468 (236 bytes): `out` (made src's size, one byte per pixel) = src's first byte of each pixel moved by (mOffsetX,
    // mOffsetY), then eroded by -mGrow or dilated by mGrow, then blurred by mBlur unless that is 0.0. Answers 0 (and leaves `out`
    // cleared after a failure) or the last step's answer; 0 without touching `out` when src has no data.
    uint32_t shadow_glow(const Bitmap& src, Bitmap& out) const;

    uint8_t mRed, mGreen, mBlue, mAlpha;   // 0x00  (the caller's colourblt takes them)
    int32_t mGrow;                         // 0x04  negative erodes, positive dilates, by that many pixels
    float mBlur;                           // 0x08  0.0 skips the blur
    int32_t mOffsetX, mOffsetY;            // 0x0c, 0x10
};
static_assert(sizeof(ShadowGlow) == 0x14, "ShadowGlow is 0x14 bytes");
