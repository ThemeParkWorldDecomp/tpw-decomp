// The weather's drawing (engine library of SimTheme Park 2.04b3, in the inventory's
// tb_resource_display.cpp range; the original file is tb_rain.cpp by its assertions):
//
//   0x10022b20 CRain::__LightningFlash   (192 bytes)
//   0x10022c10 CRain::__RenderLightning  (700)
//   0x10022efc CRain::__RenderRain       (1,156)
//   0x100233f8 CRain::__RenderSnow       (2,844)
//   0x10038b14 GetCycleCount             (40)
//
// CRain::Render (src/tb_rain_frame.cpp) calls the first three every frame. __RenderLightning
// draws the white flash of a lightning strike (a sprite over the whole screen), ages the bolts
// and draws each live one through RenderLightning2. __RenderRain draws a streak for each drop
// and __RenderSnow a small triangle with a quarter of the snow texture for each flake; both
// move the drops into the camera's space first (the wrapped positions Render works out) and
// add the polygons one by one to a primitive of their own.
//
// Floating point. Every float operation follows the listing: the single-precision adds,
// subtracts and multiplies are plain, the fused multiply-adds (fmadds) go through ppc_fmadds,
// and a float -> int conversion is fctiwz (ppc_fctiwz).
//
// The polygons' vertices are built on the stack in arrays with room for the vertices the clipper
// adds (the original keeps them in its frame and lets the clipper use the arrays as work space),
// and the specular colour of a vertex is never set: stack leftovers in the original, zero here
// and left to what the clipper writes into the array afterwards.
#include <cstring>

#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_textureentry.h"
#include "MapWho.h"

namespace {

// The blending modes and the vertex format of the rain and snow primitives, as GetNextPrimitive
// takes them. The rain's depends on the display (flag 2 = 0x200d22, else 0x200d52).
constexpr uint32_t kRainBlend = 0x200d52;
constexpr uint32_t kRainBlendDisplay2 = 0x200d22;
constexpr uint32_t kSnowBlend = 0x556;
constexpr uint32_t kTLCTEXFormat = 0x1c4;
constexpr int32_t kTriangleList = 4;
constexpr int32_t kVertexStride = 0x20;
// gei_Display.mFlags: 2 chooses the rain's blending mode; 0x100 chooses the snow's wind (clear:
// each flake jitters at random, set: the flakes follow the cubic curves of the snow tables).
constexpr uint32_t kDisplayRainBlend = 0x2;
constexpr uint32_t kDisplayCubicSnow = 0x100;

// The flash of a lightning strike: a sprite over the whole screen.
constexpr uint32_t kFlashBlend = 0x9a1;

// The lightning's colour grows over the last tenth of a bolt's life and fades over the rest;
// the bolt's time is counted in milliseconds, 500 to a cycle.
constexpr int32_t kBoltCycle = 500;

// The vertices of a polygon plus what the clipper can add to it (four plus six planes).
constexpr size_t kWorkVertices = 16;

// The drops of the weather (CRain::mDrops).
constexpr int32_t kDrops = 1024;

// The wind offset of the drops wraps at 4 units either way.
float Wrap(float value) {
    if (value > 4.0f) return value - 8.0f;
    if (value < -4.0f) return value + 8.0f;
    return value;
}

// The inlined math unit call of tb_matrix.h (its assertion names line 152): strided vertices
// through a matrix.
void MathUnitTransform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                       int32_t type) {
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(matrix, out, in, outStride, inStride, count, type);
}

// A texture entry's place on its page: the page it is on (flag 0x1000, else none) and the corners
// of its picture. All zero without a page.
struct SPicture {
    CTextureBase* mPage = nullptr;
    float mU0 = 0.0f, mV0 = 0.0f, mU1 = 0.0f, mV1 = 0.0f;
    float mUSize = 0.0f, mVSize = 0.0f;
};

SPicture PictureOf(CTextureEntry* entry) {
    SPicture picture;
    if (entry != nullptr) {
        picture.mPage = entry->GetTexture();
        if (picture.mPage != nullptr) {
            picture.mUSize = entry->mUSize;
            picture.mVSize = entry->mVSize;
            picture.mU0 = entry->mUOffset;
            picture.mV0 = entry->mVOffset;
        }
    }
    return picture;
}

// The snow tables' curve at time `t`: the whole part of t (fctiwz, which saturates a huge t) picks one
// of the 128 sets of four vectors A, B, C, D (modulo 128) and the fraction is the curve's parameter f.
// Each coordinate is D + (C * f + (A * f^3 + B * f^2)), the products B * f^2 plain, the two inner
// steps fused multiply-adds.
D3DVECTOR SnowCubic(const CRain& rain, float t) {
    const int32_t whole = ppc_fctiwz(static_cast<double>(t));
    const float fraction = t - static_cast<float>(whole);
    const D3DVECTOR* set = rain.mSnowCubics[whole & 0x7f];
    const float square = fraction * fraction;
    const float cube = square * fraction;
    D3DVECTOR result;
    float x = set[1].x * square;
    x = ppc_fmadds(set[0].x, cube, x);
    x = ppc_fmadds(set[2].x, fraction, x);
    result.x = set[3].x + x;
    float y = set[1].y * square;
    y = ppc_fmadds(set[0].y, cube, y);
    y = ppc_fmadds(set[2].y, fraction, y);
    result.y = set[3].y + y;
    float z = set[1].z * square;
    z = ppc_fmadds(set[0].z, cube, z);
    z = ppc_fmadds(set[2].z, fraction, z);
    result.z = set[3].z + z;
    return result;
}

}  // namespace

// The Mac's time base register, read through the platform timer: its low 32 bits (a counter that
// only goes up; the Mac's runs at about 25 MHz, the host's in nanoseconds).
uint32_t GetCycleCount() { return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsoluteCycles()); }

// The flash of a strike: the flash flag is cleared and a white sprite covering the screen (corners
// at +-1, depth 0.001, blending mode 0x9a1, no texture, colour -1) is drawn. Answers DrawSprite's.
int32_t CRain::__LightningFlash() {
    mLightningFlash = 0;
    float corners[4][4] = {
        {-1.0f, -1.0f, 0.001f, 1.0f},
        {1.0f, -1.0f, 0.001f, 1.0f},
        {1.0f, 1.0f, 0.001f, 1.0f},
        {-1.0f, 1.0f, 0.001f, 1.0f},
    };
    SSpriteDrawParameters sprite;
    sprite.mFlags = 0;
    sprite.mVertices = corners;
    sprite.mF08 = -1;
    sprite.mBlendingMode = kFlashBlend;
    sprite.mTexture = nullptr;
    sprite.mF14 = 0;   // not set by the original
    sprite.mU0 = sprite.mV0 = sprite.mU1 = sprite.mV1 = 0.0f;
    sprite.mF28 = 0;
    return gei_DrawPrimitive->DrawSprite(&sprite, -1);
}

// The lightning for one frame: the flash when one was asked for, then every bolt from the last to
// the first. A bolt's time (milliseconds) runs down by the frame's; a bolt whose time has run out
// is removed (the last bolt takes its place, so the bolt that moves in was already handled this
// frame), the others are drawn by RenderLightning2 after setting the colour and depth members it
// reads. With a seed the bolt's brightness follows its time modulo 500: from 192 up to 255 over
// the first 0.9 of each cycle, and from 256 down to 192 over the rest, plus 0..63 of random; the
// grey is put in the three colour bytes unmasked (a sum above 255 spills into the next byte) with
// alpha 255, the depth is 4 and the seed goes to mFA198. Without a seed the colour is white, the
// depth 0 and mFA198 the cycle counter. 0 when the flash or a bolt's drawing fails.
// The original's assertions name the count mF3308 "md_LightningsUsed"; they cannot fire (the count
// is positive inside the loop, and not below zero after it is lowered).
int32_t CRain::__RenderLightning() {
    if (mLightningFlash != 0 && __LightningFlash() == 0) return 0;

    int32_t index = mF3308;
    while (index != 0) {
        --index;
        SLightning& bolt = mLightning[index];
        bolt.mTime -= mDeltaMilliseconds;
        if (bolt.mTime <= 0) {
            if (mF3308 <= 0) msl_assertion_failed("md_LightningsUsed>0", "tb_rain.cpp", 0x387);
            mF3308 -= 1;
            if (index != mF3308) std::memcpy(&mLightning[index], &mLightning[mF3308], sizeof(SLightning));
            if (mF3308 < 0) msl_assertion_failed("md_LightningsUsed>=0", "tb_rain.cpp", 0x38a);
            continue;
        }

        mCurrentLightning = index;
        if (bolt.mSeed != 0) {
            mFA198 = static_cast<int32_t>(bolt.mSeed);
            const float fraction = static_cast<float>(bolt.mTime % kBoltCycle) / 500.0f;
            int32_t level;
            if (fraction < 0.9f) {
                const float scaled = 64.0f * (1.11f * (0.9f - fraction));
                level = 0xff - ppc_fctiwz(static_cast<double>(scaled));
            } else {
                const float scaled = 64.0f * (10.0f * (1.0f - fraction));
                level = ppc_fctiwz(static_cast<double>(scaled)) + 0xc0;
            }
            const uint32_t grey = static_cast<uint32_t>(level) + (static_cast<uint32_t>(msl_rand()) & 0x3f);
            mLightningColor = (grey << 16) | 0xff000000u | (grey << 8) | grey;
            mLightningDepth = 4;
        } else {
            mFA198 = static_cast<int32_t>(GetCycleCount());
            mLightningColor = 0xffffffffu;
            mLightningDepth = 0;
        }
        if (RenderLightning2(&bolt.mEnd, &bolt.mStart) == 0) return 0;
    }
    return 1;
}

// One streak of rain for every drop in front of the camera. The camera's place (x, y, z) is taken
// off the drops' positions (y added: the drops' y runs the other way), each coordinate wrapped
// into a window of eight units; the end of the streak is the drop plus mLength times the
// direction. Both ends go through the weather's matrix (the camera's orientation, rotation only)
// in place, the y is negated, and a drop with z <= 0 (or NaN) is skipped. A drop becomes a quad:
// the streak's width is a tenth of its extent turned a quarter turn, the colour grey 3 * int(12 *
// (6 - z)) (at least 0), masked by the light manager's colour with 0x80 (the near end) or 0xff
// (the far end) added, depth 0.04 + z of the near end for all four and w 0.1 + z of each end.
// The picture is the rain texture's place on its page (none: zeros, no page); the page takes the
// blending mode 0x200d22 when the display's flag 2 is set, else 0x200d52. The primitive's depth
// is zeroed after the projection. 1 when the draw primitive has no room for a primitive; 0 when
// AddPoly_TLCTEX answers 0 (it never does); else what ExecuteCurrentPrimitive answers.
int32_t CRain::__RenderRain(float x, float y, float z) {
    const SPicture picture = PictureOf(mRainTexture);
    const float u0 = picture.mU0;
    const float v0 = picture.mV0;
    const float u1 = picture.mU0 + picture.mUSize;
    const float v1 = picture.mV0 + picture.mVSize;

    // Each drop's two ends: x, y, z for the near end, then the far end.
    float points[kDrops * 6];
    for (int32_t i = 0; i < mRaindrops; ++i) {
        float* p = &points[i * 6];
        p[0] = mDrops[i].x - x;
        p[1] = mDrops[i].y + y;
        p[2] = mDrops[i].z - z;
        p[0] = Wrap(p[0]);
        p[1] = Wrap(p[1]);
        p[2] = Wrap(p[2]);
        p[3] = ppc_fmadds(mLength, mDirection.x, p[0]);
        p[4] = ppc_fmadds(mLength, mDirection.y, p[1]);
        p[5] = ppc_fmadds(mLength, mDirection.z, p[2]);
    }
    MathUnitTransform(reinterpret_cast<sMatrix*>(mMatrix), points, points, 12, 12, mRaindrops * 2, 2);

    const uint32_t blend = (gei_Display.mFlags & kDisplayRainBlend) != 0 ? kRainBlendDisplay2 : kRainBlend;
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(mF330C, kTriangleList, kTLCTEXFormat,
                                                                static_cast<int32_t>(blend), picture.mPage);
    if (primitive == nullptr) return 1;

    SFVF_TLCTEX quad[kWorkVertices] = {};
    const uint32_t light = static_cast<uint32_t>(gei_LightManager->mF04);
    float* p = points;
    for (int32_t remaining = mRaindrops; remaining != 0; --remaining, p += 6) {
        p[1] = ppc_fneg(p[1]);
        p[4] = ppc_fneg(p[4]);
        const float depth = p[2];
        if (!(depth > 0.0f)) continue;

        int32_t grey = ppc_fctiwz(static_cast<double>(12.0f * (6.0f - depth)));
        // A tenth of the streak's extent, turned a quarter: the width is perpendicular to the streak.
        const float stepY = 0.1f * (p[3] - p[0]);
        const float stepX = 0.1f * (p[4] - p[1]);
        if (grey < 0) grey = 0;
        const uint32_t level = static_cast<uint32_t>(grey) * 3u;
        const uint32_t colour = ((level & 0xff) << 24) | ((level & 0xff) << 16) | ((level & 0xff) << 8);
        const uint32_t nearColour = light & (colour | 0x80);
        const uint32_t farColour = light & (colour | 0xff);
        const float quadZ = 0.04f + depth;

        quad[0].x = p[0] + stepX;
        quad[0].y = p[1] - stepY;
        quad[0].z = quadZ;
        quad[0].rhw = 0.1f + depth;
        quad[0].colour = nearColour;
        quad[0].u = u1;
        quad[0].v = v1;

        quad[1].x = p[0] - stepX;
        quad[1].y = p[1] + stepY;
        quad[1].z = quadZ;
        quad[1].rhw = 0.1f + p[2];
        quad[1].colour = nearColour;
        quad[1].u = u0;
        quad[1].v = v1;

        quad[2].x = p[3] - stepX;
        quad[2].y = p[4] + stepY;
        quad[2].z = quadZ;
        quad[2].rhw = 0.1f + p[5];
        quad[2].colour = farColour;
        quad[2].u = u0;
        quad[2].v = v0;

        quad[3].x = p[3] + stepX;
        quad[3].y = p[4] - stepY;
        quad[3].z = quadZ;
        quad[3].rhw = 0.1f + p[5];
        quad[3].colour = farColour;
        quad[3].u = u1;
        quad[3].v = v0;

        if (gei_DrawPrimitive->AddPoly_TLCTEX(quad, 4, kVertexStride, nullptr, 0) == 0) return 0;
    }
    gei_DrawPrimitive->ProjectCurrentPrimitive(0);
    primitive->mDepth = 0.0f;
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f);
}

// One flake of snow for every drop in front of the camera: a triangle (apex up, a quarter of the
// snow texture's picture, chosen by the drop's number) scaled by the display's aspect and the
// flake size mF3354. Each drop is first put in the camera's space like the rain's, with a
// wobble: without display flag 0x100 a random jitter of up to +-16/2048 per coordinate (three
// rand() calls per drop, x then y then z); with it the flakes follow the snow tables: two wind
// vectors (cubics of the first two snow times, divided by 13), sixteen shared sway vectors (the
// times of the first sixteen drops moved on, divided by 7) and, per drop, its own cubic at its own
// time (advanced by 5 times the frame time here, stored back), each scaled by the cubic parameters
// (mSnowParameters.x for the drop's, .y for the sway, .z for the wind) and the wind's direction.
// Drops with z <= 0 are skipped; the others' x and y grow by z / 8 (perspective); the colour of
// every vertex is the light manager's colour itself and the blending mode 0x556. 1 when the draw
// primitive has no room for a primitive; 0 when AddPoly_TLCTEX answers 0 (it never does); else
// ExecuteCurrentPrimitive's. Unlike the rain the primitive's depth is not zeroed.
int32_t CRain::__RenderSnow(float x, float y, float z) {
    const SPicture picture = PictureOf(mSnowTexture);
    const uint32_t light = static_cast<uint32_t>(gei_LightManager->mF04);
    const float size = mF3354 * gei_Display.mF04;

    // The picture's two columns and rows (halves of the picture): the borders and the middle.
    const float u[3] = {picture.mU0, ppc_fmadds(picture.mUSize, 0.5f, picture.mU0), picture.mU0 + picture.mUSize};
    const float v[3] = {picture.mV0, ppc_fmadds(picture.mVSize, 0.5f, picture.mV0), picture.mV0 + picture.mVSize};

    float points[kDrops * 3];
    if ((gei_Display.mFlags & kDisplayCubicSnow) == 0) {
        for (int32_t i = 0; i < mRaindrops; ++i) {
            float jitter[3];
            for (float& axis : jitter) {
                const int32_t step = static_cast<int32_t>(static_cast<uint32_t>(msl_rand()) & 0x1f) - 16;
                axis = 0.00048828125f * static_cast<float>(step);
            }
            float* p = &points[i * 3];
            p[0] = jitter[0] + (mDrops[i].x - x);
            p[1] = jitter[1] + (mDrops[i].y + y);
            p[2] = jitter[2] + (mDrops[i].z - z);
            p[0] = Wrap(p[0]);
            p[1] = Wrap(p[1]);
            p[2] = Wrap(p[2]);
        }
    } else {
        const float step = mDeltaTime;
        const float windScale = mSnowParameters.z;
        const float swayScale = mSnowParameters.y;
        const float flakeScale = mSnowParameters.x;

        // The two wind vectors, from the first two snow times.
        D3DVECTOR wind[2];
        for (int32_t k = 0; k < 2; ++k) {
            const float t = ppc_fmadds(5.0f, step, mSnowTimes[k]) / 13.0f;
            D3DVECTOR w = SnowCubic(*this, t);
            w.x = w.x * windScale;
            w.y = w.y * windScale;
            w.z = w.z * windScale;
            w.x = w.x + mDirection.x;
            w.y = w.y * mDirection.y;
            w.z = w.z * mDirection.z;
            wind[k] = w;
        }
        // Sixteen sway vectors, alternately on one wind vector and the other.
        D3DVECTOR sway[16];
        for (int32_t j = 0; j < 16; ++j) {
            const float t = ppc_fmadds(5.0f, step, mSnowTimes[j & 15]) / 7.0f;
            D3DVECTOR s = SnowCubic(*this, t);
            s.x = s.x * swayScale;
            s.y = s.y * swayScale;
            s.z = s.z * swayScale;
            sway[j].x = s.x + wind[j & 1].x;
            sway[j].y = s.y + wind[j & 1].y;
            sway[j].z = s.z + wind[j & 1].z;
        }
        for (int32_t i = 0; i < mRaindrops; ++i) {
            mSnowTimes[i] = ppc_fmadds(5.0f, step, mSnowTimes[i]);
            D3DVECTOR flake = SnowCubic(*this, mSnowTimes[i]);
            const D3DVECTOR& shared = sway[i & 15];
            flake.x = ppc_fmadds(flake.x, flakeScale, shared.x);
            flake.y = ppc_fmadds(flake.y, flakeScale, shared.y);
            flake.z = ppc_fmadds(flake.z, flakeScale, shared.z);
            float* p = &points[i * 3];
            p[0] = flake.x + (mDrops[i].x - x);
            p[1] = flake.y + (mDrops[i].y + y);
            p[2] = flake.z + (mDrops[i].z - z);
            p[0] = Wrap(p[0]);
            p[1] = Wrap(p[1]);
            p[2] = Wrap(p[2]);
        }
    }

    const int32_t count = mRaindrops;
    MathUnitTransform(reinterpret_cast<sMatrix*>(mMatrix), points, points, 12, 12, count, 2);
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(mF330C, kTriangleList, kTLCTEXFormat,
                                                                static_cast<int32_t>(kSnowBlend), picture.mPage);
    if (primitive == nullptr) return 1;

    SFVF_TLCTEX triangle[kWorkVertices] = {};
    for (int32_t i = 0; i < mRaindrops; ++i) {
        float* p = &points[i * 3];
        if (!(p[2] > 0.0f)) continue;
        p[0] = ppc_fmadds(p[0], p[2] * 0.125f, p[0]);
        p[1] = ppc_fmadds(p[1], p[2] * 0.125f, p[1]);

        const int32_t column = i & 1;
        const int32_t row = (i >> 1) & 1;
        triangle[0].x = p[0];
        triangle[0].y = ppc_fneg(p[1] + mF3354);
        triangle[0].z = 0.1f + p[2];
        triangle[0].rhw = 0.1501f + p[2];
        triangle[0].colour = light;
        triangle[0].u = 0.5f * (u[column] + u[column + 1]);
        triangle[0].v = v[row];

        triangle[1].x = p[0] + size;
        triangle[1].y = ppc_fneg(p[1] - mF3354);
        triangle[1].z = 0.1f + p[2];
        triangle[1].rhw = 0.1501f + p[2];
        triangle[1].colour = light;
        triangle[1].u = u[column + 1];
        triangle[1].v = v[row + 1];

        triangle[2].x = p[0] - size;
        triangle[2].y = ppc_fneg(p[1] - mF3354);
        triangle[2].z = 0.1f + p[2];
        triangle[2].rhw = 0.1501f + p[2];
        triangle[2].colour = light;
        triangle[2].u = u[column];
        triangle[2].v = v[row + 1];

        if (gei_DrawPrimitive->AddPoly_TLCTEX(triangle, 3, kVertexStride, nullptr, 0) == 0) return 0;
    }
    gei_DrawPrimitive->ProjectCurrentPrimitive(0);
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f);
}
