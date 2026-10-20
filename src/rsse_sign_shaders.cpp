// The ride sign's shaders (main app, SimTheme Park 2.04b3, rsse.cpp range 0x100ca558..0x100cb804; unit w84): PlasticShader (clear,
// destructor, load_settings, bevel, blur_with_mask, plastic_shade) and ShadowGlow (shadow_glow, destructor). The filters they call
// are in rsse_sign_filters.cpp.
//
// The float arithmetic is the Mac's, single precision with fused multiply-adds where the listing has them (ppc_math.h); sin, cos,
// sqrt and pow are the C library's, called on the doubles the Mac converts to.
#include "PlasticShader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "FS_System.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// 0x10264f0c: degrees to radians as a float.
constexpr float kRadiansPerDegree = 0.017453292f;

// One setting of the file: four little-endian bytes, swapped into place (the Mac reads the words into the object and swaps them).
template <class T>
void ReadWord(T& field, bfl::FileSystem::CFile* file) {
    TPFile_Read(&field, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&field), 1);
}

// plastic_shade's clamp to 0 .. 255 of a float (the listing's compares: above 255 gives 255; otherwise the value, or 0 when it is
// below 0; a NaN passes through and fctiwz makes it 0x80000000).
float ClampColour(float value) {
    float result = 255.0f;
    if (!(value > 255.0f)) {
        result = value;
        if (value < 0.0f) result = 0.0f;
    }
    return result;
}

}  // namespace

// 0x100ca6a0 (88 bytes).
void PlasticShader::clear() {
    mMask.clear();
    mFile.clear();
    mBevel.clear();
    mShaded.clear();
}

// 0x100ca558 (284 bytes). clear() first; the four bitmaps are then destroyed (the members' inline destructors, last to first).
PlasticShader::~PlasticShader() { clear(); }

// 0x100ca724 (492 bytes).
int32_t PlasticShader::load_settings(bfl::FileSystem::CFile* file) {
    ReadWord(mBevelWidth, file);
    ReadWord(mHeightScale, file);
    ReadWord(mBlur, file);
    ReadWord(mAmbient, file);
    ReadWord(mDiffuse, file);
    ReadWord(mSpecular, file);
    ReadWord(mShininess, file);
    ReadWord(mLightYaw, file);
    ReadWord(mLightPitch, file);
    ReadWord(mPeriodY, file);
    ReadWord(mOffsetY, file);
    return 1;
}

// 0x100ca95c (468 bytes). The work buffer is a local Bitmap {width, height, 2, new[]}; erodebuffer_erode strips one layer of it per
// round and stamps (uchar)(256 * (2 t^3 - 3 t^2)) with t = 1 - i / width into `out`: negative for 0 < t < 1, so the byte is
// 256 less its magnitude (0 for the first round, t = 1).
void PlasticShader::bevel(Bitmap& mask, uint32_t width, Bitmap& out) {
    Bitmap work;
    work.mWidth = mask.mWidth;
    work.mHeight = mask.mHeight;
    work.mBytesPerPixel = 2;
    work.mData = new uint8_t[(mask.mWidth * mask.mHeight) << 1];
    const uint8_t* maskPixel = mask.mData;
    uint8_t* workPixel = work.mData;
    uint8_t* outPixel = out.mData;
    for (int32_t i = 0; i < static_cast<int32_t>(mask.mWidth) * static_cast<int32_t>(mask.mHeight); ++i) {
        workPixel[0] = *maskPixel == 0 ? 0 : 0xff;
        workPixel[1] = *maskPixel == 0 ? 0 : 0xff;
        workPixel += 2;
        ++maskPixel;
        *outPixel = 0;
        ++outPixel;
    }
    for (uint32_t i = 0; i < width; ++i) {
        const float t = 1.0f - static_cast<float>(i) / static_cast<float>(width);
        float twice = 2.0f * t;
        float thrice = 3.0f * t;
        twice = twice * t;
        thrice = thrice * t;
        const float curve = ppc_fmsubs(t, twice, thrice);
        const int32_t value = ppc_fctiwz(256.0f * curve);
        erodebuffer_erode(work, mask, static_cast<uint8_t>(value), out);
    }
    const uint8_t* core = work.mData;
    uint8_t* bevelOut = out.mData;
    for (int32_t i = 0; i < static_cast<int32_t>(work.mWidth) * static_cast<int32_t>(work.mHeight); ++i) {
        *bevelOut = *bevelOut | *core;
        core += 2;
        ++bevelOut;
    }
}

// 0x100cab6c (912 bytes). A size x size kernel (size = (int)(2.5 * sigma) + 3), a ring of `above` copies of the first row (the rows
// above the current one; the last is the current row's copy) and `below` pointers into the bitmap itself (the rows below, clamped to
// the last row: on the last rows they point at rows the pass has already written). The kernel is read straight through, one entry
// per sample, which is exactly size * size entries.
void PlasticShader::blur_with_mask(Bitmap& bitmap, Bitmap& mask, float sigma) {
    const int32_t n = ppc_fctiwz(2.5 * static_cast<double>(sigma));
    const int32_t size = n + 3;
    double* kernel = new double[static_cast<uint32_t>(size * size)];
    make_gaussian_filter_2d(kernel, size, static_cast<double>(sigma));
    const int32_t above = (size + 1) / 2;
    uint8_t** ring = new uint8_t*[static_cast<uint32_t>(above)];
    for (int32_t i = 0; i < above; ++i) {
        ring[i] = new uint8_t[bitmap.mWidth];
        std::memcpy(ring[i], bitmap.mData, bitmap.mWidth);
    }
    const int32_t belowCount = size - above;
    uint8_t** below = new uint8_t*[static_cast<uint32_t>(belowCount)];
    for (int32_t i = 0; i < belowCount; ++i) {
        const int32_t row = std::min(i + 1, static_cast<int32_t>(bitmap.mHeight) - 1);
        below[i] = bitmap.mData + static_cast<int32_t>(bitmap.mWidth) * row;
    }
    const int32_t centre = (size - 1) / 2;
    const int32_t end = (size + 2) / 2;
    uint8_t* pixel = bitmap.mData;
    const uint8_t* maskPixel = mask.mData;
    for (int32_t row = 0; row < static_cast<int32_t>(bitmap.mHeight); ++row) {
        uint8_t* nextRowPixel = pixel + bitmap.mWidth;
        const uint8_t* nextMaskRow = maskPixel + mask.mWidth;
        for (int32_t col = 0; col < static_cast<int32_t>(bitmap.mWidth); ++col) {
            if (*maskPixel != 0) {
                const double* tap = kernel;
                double total = 0.0;
                for (int32_t dy = -centre; dy < end; ++dy) {
                    const uint8_t* source = dy <= 0 ? ring[dy + centre] : below[dy - 1];
                    for (int32_t dx = -centre; dx < end; ++dx) {
                        int32_t x = static_cast<int32_t>(bitmap.mWidth) - 1;
                        if (col + dx <= x) {
                            x = col + dx;
                            if (x < 0) x = 0;
                        }
                        total = ppc_fmadd(*tap, static_cast<double>(source[x]), total);
                        ++tap;
                    }
                }
                *pixel = static_cast<uint8_t>(ppc_fctiwz(total));
            }
            ++maskPixel;
            ++pixel;
        }
        // The ring turns: the oldest copy becomes the newest and takes the first row below; the pointers below shift, the last one
        // taking the row size / 2 + 1 further on.
        uint8_t* oldest = ring[0];
        for (int32_t i = 0; i < above - 1; ++i) ring[i] = ring[i + 1];
        ring[above - 1] = oldest;
        std::memcpy(ring[above - 1], below[0], bitmap.mWidth);
        for (int32_t i = 0; i < belowCount - 1; ++i) below[i] = below[i + 1];
        const int32_t nextRow = std::min(row + size / 2 + 1, static_cast<int32_t>(bitmap.mHeight) - 1);
        below[belowCount - 1] = bitmap.mData + static_cast<int32_t>(bitmap.mWidth) * nextRow;
        maskPixel = nextMaskRow;
        pixel = nextRowPixel;
    }
    for (int32_t i = 0; i < above; ++i) delete[] ring[i];
    delete[] ring;
    delete[] below;
    delete[] kernel;
}

// 0x100caf40 (1260 bytes). The light is (0, 0, 1) turned by the yaw (negated) and the pitch; the half vector is (L + (0, 0, 1)) / 2,
// both normalised. Each lit pixel (mask byte set) takes the slope of `bevel` (the bytes below and to the right minus its own, read
// past the last row and column), makes the normal (nx, ny, 1)/|..|, and shades the tile's colour: ambient, plus diffuse * N.L when
// N.L is above 0, plus specular * pow(N.H, shininess) when N.H is above 0, times 255 and clamped. The output is the mask byte, then
// the three shaded bytes.
void PlasticShader::plastic_shade(Bitmap& bevelBitmap) {
    const int32_t periodY = mPeriodY;
    const uint8_t* bevelPixel = bevelBitmap.mData;
    const uint8_t* maskPixel = mMask.mData;
    uint8_t* shaded = mShaded.mData;
    if (periodY == 0) return;
    Bitmap tile;
    tile.mWidth = mFile.mWidth;
    tile.mHeight = static_cast<uint32_t>(periodY);
    tile.mBytesPerPixel = 4;
    tile.mData = new uint8_t[(tile.mWidth * static_cast<uint32_t>(periodY)) << 2];
    mFile.descimate(tile);

    const float yaw = kRadiansPerDegree * ppc_fneg(mLightYaw);
    const float yawSin = static_cast<float>(std::sin(static_cast<double>(yaw)));
    const float yawCos = static_cast<float>(std::cos(static_cast<double>(yaw)));
    const float pitch = kRadiansPerDegree * mLightPitch;
    const float turnedX = ppc_fmadds(yawCos, 1.0f, yawSin * 0.0f);
    const float turnedZ = ppc_fmadds(ppc_fneg(yawSin), 1.0f, yawCos * 0.0f);
    const float pitchSin = static_cast<float>(std::sin(static_cast<double>(pitch)));
    const float pitchCos = static_cast<float>(std::cos(static_cast<double>(pitch)));
    const float lightA = ppc_fmadds(pitchCos, turnedX, pitchSin * 0.0f);
    const float lightB = ppc_fmadds(ppc_fneg(pitchSin), turnedX, pitchCos * 0.0f);
    const float lightLength2 = ppc_fmadds(turnedZ, turnedZ, ppc_fmadds(lightA, lightA, lightB * lightB));
    const float lightScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(lightLength2)));
    const float lightX = lightA * lightScale;
    const float lightY = lightB * lightScale;
    const float lightZ = turnedZ * lightScale;

    const float sumX = lightX + 0.0f;
    const float sumY = lightY + 0.0f;
    const float sumZ = lightZ + 1.0f;
    const float halfX = sumX * 0.5f;
    const float halfY = sumY * 0.5f;
    const float halfZ = sumZ * 0.5f;
    const float halfLength2 = ppc_fmadds(halfZ, halfZ, ppc_fmadds(halfX, halfX, halfY * halfY));
    const float halfScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(halfLength2)));
    const float hx = halfX * halfScale;
    const float hy = halfY * halfScale;
    const float hz = halfZ * halfScale;

    const float ambient = mAmbient;
    const float diffuse = mDiffuse;
    const float specular = mSpecular;
    for (int32_t row = 0; row < static_cast<int32_t>(bevelBitmap.mHeight); ++row) {
        for (int32_t col = 0; col < static_cast<int32_t>(bevelBitmap.mWidth); ++col) {
            if (*maskPixel == 0) {
                shaded[0] = 0;
                shaded[1] = 0;
                shaded[2] = 0;
                shaded[3] = 0;
            } else {
                const int32_t down = static_cast<int32_t>(bevelPixel[bevelBitmap.mWidth]) - static_cast<int32_t>(bevelPixel[0]);
                const int32_t right = static_cast<int32_t>(bevelPixel[1]) - static_cast<int32_t>(bevelPixel[0]);
                const float slopeDown = mHeightScale * static_cast<float>(down);
                const float slopeRight = mHeightScale * static_cast<float>(right);
                const float gy = slopeDown / 255.0f;
                const float gx = slopeRight / 255.0f;
                const float negGy = -1.0f * gy;
                const float negGx = gx * -1.0f;
                const float nx = ppc_fmsubs(gx, 0.0f, negGy);
                const float ny = ppc_fmsubs(0.0f, gy, negGx);
                const float normal2 = 1.0f + ppc_fmadds(ny, ny, nx * nx);
                const float normalScale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(normal2)));
                const float unitX = nx * normalScale;
                const float unitY = ny * normalScale;
                const float unitZ = 1.0f * normalScale;
                const float nDotL = ppc_fmadds(unitZ, lightZ, ppc_fmadds(unitY, lightX, unitX * lightY));
                float nDotH = ppc_fmadds(unitZ, hz, ppc_fmadds(unitY, hx, unitX * hy));

                const int32_t tileWidth = static_cast<int32_t>(tile.mWidth);
                const int32_t tileHeight = static_cast<int32_t>(tile.mHeight);
                const int32_t tileRow = row - mOffsetY;
                const int32_t tileRowMod = tileRow - (tileRow / tileHeight) * tileHeight;
                const int32_t tileColMod = col - (col / tileWidth) * tileWidth;
                const int32_t texel = static_cast<int32_t>(tile.mBytesPerPixel) * (tileColMod + tileRowMod * tileWidth);
                const float tileR = static_cast<float>(tile.mData[texel + 1]);
                const float tileG = static_cast<float>(tile.mData[texel + 2]);
                const float tileB = static_cast<float>(tile.mData[texel + 3]);
                const float colourG = tileG / 255.0f;
                const float colourB = tileB / 255.0f;
                const float colourR = tileR / 255.0f;
                float outB = colourB * ambient;
                float outG = colourG * ambient;
                float outR = colourR * ambient;
                if (0.0 < static_cast<double>(nDotL)) {
                    const float lit = diffuse * nDotL;
                    outB = outB + colourB * lit;
                    outG = outG + colourG * lit;
                    outR = outR + colourR * lit;
                }
                if (0.0 < static_cast<double>(nDotH)) {
                    const float power = static_cast<float>(std::pow(static_cast<double>(nDotH), static_cast<double>(mShininess)));
                    const float shine = 1.0f * (specular * power);
                    outR = outR + shine;
                    outG = outG + shine;
                    outB = outB + shine;
                }
                outR = outR * 255.0f;
                outG = outG * 255.0f;
                outB = outB * 255.0f;
                shaded[0] = *maskPixel;
                shaded[1] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outR)));
                shaded[2] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outG)));
                shaded[3] = static_cast<uint8_t>(ppc_fctiwz(ClampColour(outB)));
            }
            shaded += 4;
            ++maskPixel;
            ++bevelPixel;
        }
    }
}

// 0x100cb7c0 (68 bytes).
ShadowGlow::~ShadowGlow() {}

// 0x100cb468 (236 bytes). The failure branch (a step answering other than 1, then out.clear() and 0) is the original's but cannot
// be reached: copy_offset_alpha answers 1 and the others fail only when new[] answers NULL.
uint32_t ShadowGlow::shadow_glow(const Bitmap& src, Bitmap& out) const {
    if (src.mData == nullptr) return 0;
    out.realloc(src.mWidth, src.mHeight, 1);
    uint32_t answer = static_cast<uint32_t>(copy_offset_alpha(out, src, mOffsetX, mOffsetY));
    if ((answer & 0xff) == 1) {
        if (mGrow < 0) {
            answer = static_cast<uint32_t>(erode(out, -mGrow));
        } else if (0 < mGrow) {
            answer = static_cast<uint32_t>(dilate(out, mGrow));
        }
        if ((answer & 0xff) == 1) {
            if (0.0 != static_cast<double>(mBlur)) return static_cast<uint32_t>(blur(out, static_cast<double>(mBlur)));
            return answer;
        }
    }
    out.clear();
    return 0;
}
