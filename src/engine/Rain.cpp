// Rain.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "MapWho.h"
#include "platform/d3d_types.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "Wavelet.h"
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <new>

// The original assertions name tb_rain.cpp (inventory: tb_resource_display.cpp).

CRain* gei_Rain = nullptr;

int32_t Rain_Init() {
    if (gei_Rain != nullptr) return 0;
    gei_Rain = new (std::nothrow) CRain;
    if (gei_Rain == nullptr) {
        gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754000u));
        return 0;
    }
    gei_Rain->Init();
    return 1;
}

int32_t Rain_Close() {
    if (gei_Rain == nullptr) return 0;
    gei_Rain->Close();
    delete gei_Rain;
    gei_Rain = nullptr;
    return 1;
}

CRain::CRain() {
    std::memset(mMatrix, 0, sizeof mMatrix);
    mMatrix[0] = mMatrix[5] = mMatrix[10] = mMatrix[15] = 1.0f;
    mRainType = 1;
    mClock = 0;
    mF3334 = -1;
    mRaindrops = 0;
    mDirection = {0.0f, -1.0f, 0.0f};
    mF3328 = {0.0f, 0.0f, 0.0f};
    mF3340 = 0.1f;
    mRainTexture = nullptr;
    mRainVelocity = 5.0f;
    mLength = 0.1f;
    mSnowTexture = nullptr;
    mSnowVelocity = 0.7f;
    mF3354 = 0.1f;
    mLightningTexture = nullptr;
    mF3308 = 0;
    mF3310 = 0.6f;
    mFA180 = 6.0f;
    mFA184 = 2.0f;
    mFA16C = 1.3f;
    mFA188 = 0;
    mLightningFlash = 0;
    SetSnowCubicParameters(0.4f, 6.0f, 13.0f);
    mFA1B8 = 0.11f;
    mFA1AC = {0.0f, 0.0f, 0.0f};
    mFA1A8 = mFA1A4 = 0;
}

int32_t CRain::Init() {
    for (D3DVECTOR& v : mDrops) {
        v.x = (static_cast<float>(msl_rand() & 0x3fff) - 8192.0f) / 2048.0f;
        v.y = (static_cast<float>(msl_rand() & 0x3fff) - 8192.0f) / 2048.0f;
        v.z = (static_cast<float>(msl_rand() & 0x3fff) - 8192.0f) / 2048.0f;
    }
    for (int32_t i = 0; i < 128; ++i) {
        const float angle = (3.1415927410125732f * static_cast<float>(i)) * 0.0078125f;
        const float wave = 0.5f * (1.0f + static_cast<float>(std::cos(static_cast<double>(angle))));
        mEnvelope[i] = wave * ppc_fnmsubs(static_cast<float>(i), 0.0078125f, 0.99999f);
    }
    const auto offset = []() { return static_cast<float>((msl_rand() & 0x7ff) - 1024) / 30720.0f; };
    for (int32_t i = 0; i < 1024; ++i) {
        mSnowTimes[i] = static_cast<float>(msl_rand() % 75499 + 1) / 1000.0f;
        mSnowRates[i] = 1.0f / (0.001f + mSnowTimes[i]);
        mSnowOffsets[i].x = offset();
        mSnowOffsets[i].y = offset();
        mSnowOffsets[i].z = offset();
        // The second random time replaces the first; the rate keeps the first one's value.
        mSnowTimes[i] = static_cast<float>(i) + static_cast<float>(msl_rand() % 75499 + 1) / 1000.0f;
    }
    D3DVECTOR points[128];
    for (D3DVECTOR& p : points) {
        p.x = offset();
        p.y = offset();
        p.z = offset();
    }
    for (int32_t i = 0; i < 128; ++i) {
        const D3DVECTOR a = points[i];
        const D3DVECTOR b = points[(i + 127) & 127];
        const D3DVECTOR c = points[(i + 126) & 127];
        const D3DVECTOR d = points[(i + 125) & 127];
        mSnowCubics[i][0] = {(a.x - b.x) - (d.x - c.x), (a.y - b.y) - (d.y - c.y), (a.z - b.z) - (d.z - c.z)};
        const D3DVECTOR first = mSnowCubics[i][0];
        mSnowCubics[i][1] = {(d.x - c.x) - first.x, (d.y - c.y) - first.y, (d.z - c.z) - first.z};
        mSnowCubics[i][2] = {b.x - d.x, b.y - d.y, b.z - d.z};
        mSnowCubics[i][3] = c;
    }
    mClock = static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
    return 1;
}

int32_t CRain::Close() {
    for (CTextureEntry** slot : {&mSnowTexture, &mLightningTexture, &mRainTexture}) {
        if (*slot != nullptr) {
            (*slot)->Close();
            delete[] *slot;
            *slot = nullptr;
        }
    }
    return 1;
}

int32_t CRain::Load(int32_t kind, char* path) {
    if (path == nullptr) return 0;
    // The original leaves the slot register unset for kinds outside 1..3.
    CTextureEntry** slot = kind == 2 ? &mSnowTexture :
                           kind == 1 ? &mRainTexture : &mLightningTexture;
    if (*slot == nullptr) {
        *slot = gei_TLoadManager->LoadFile(path);
        if (*slot != nullptr) (*slot)->mFlags |= 0x80200u;
    }
    return *slot != nullptr;
}

int32_t CRain::GameLoad() {
    char* path = gFileName("Data:Generic:Weather:snow.tga", 1);
    Load(2, path);
    path = gFileName("Data:Generic:Weather:raindrop.tga", 1);
    Load(1, path);
    path = gFileName("Data:Generic:Weather:lightning.tga", 1);
    Load(3, path);
    mLightningFlash = 0;
    return 1;
}

void CRain::SetRainType(int32_t type) { mRainType = type; }
void CRain::SetLength(float length) { mLength = length; }
void CRain::SetRaindrops(int32_t count) { mRaindrops = count; }
void CRain::SetDirection(D3DVECTOR direction) { mDirection = direction; }
void CRain::SetDirection(float x, float y, float z) { mDirection = {x, y, z}; }
void CRain::SetWindParameters(float direction, float speed) {
    const auto angle = [](float degrees) { return 3.1415927410125732f * (degrees / 180.0f); };
    const auto normalize = [](D3DVECTOR v) {
        const float squared = v.z * v.z + ppc_fmadds(v.x, v.x, v.y * v.y);
        const float length = static_cast<float>(std::sqrt(static_cast<double>(squared)));
        if (length != 0.0f) {
            const float scale = 1.0f / length;
            v.x *= scale;
            v.y *= scale;
            v.z *= scale;
        }
        return v;
    };
    const float radians = angle(direction);
    const float co = static_cast<float>(std::cos(static_cast<double>(radians)));
    const float si = static_cast<float>(std::sin(static_cast<double>(radians)));
    mDirection = normalize({speed * co, -1.0f, speed * si});
    direction += 270.0f;
    speed *= 0.0017f;
    const float xAngles[] = {direction, direction - 1.9f, direction - 1.2f, 1.8f + direction};
    const float zAngles[] = {direction, direction - 2.3f, 1.7f + direction, 2.9f + direction};
    D3DVECTOR scroll;
    for (int32_t i = 0; i < 4; ++i) {
        scroll.x = speed * static_cast<float>(std::sin(static_cast<double>(angle(xAngles[i]))));
        scroll.y = -1.0f;
        scroll.z = speed * static_cast<float>(std::cos(static_cast<double>(angle(zAngles[i]))));
        gei_Sky.SetupScrollDirection(i, scroll);
    }
    scroll.x *= 0.2f;
    scroll.z *= 0.2f;
    mDirection = normalize(scroll);
}
void CRain::SetVelocity(float speed) {
    if (mRainType & 1) mRainVelocity = speed;
    if (mRainType & 2) mSnowVelocity = speed;
}
void CRain::LightningFlash() { mLightningFlash = 1; }

// The weather's frame members (engine library of SimTheme Park 2.04b3, in the inventory's
// tb_resource_display.cpp range):
//
//   0x10021554 CRain::Prepare            (196 bytes)
//   0x1002294c CRain::RenderLightning2   (396)
//   0x10023f40 CRain::Render             (1,128)
//
// Prepare asks the texture cache for the textures the weather will use; Render works out the
// frame time, draws the lightning, then moves the drops' wind offset and draws the rain or
// the snow (__RenderLightning, __RenderRain and __RenderSnow, still to do). RenderLightning2
// draws one bolt as a primitive of its own.

namespace {

// System flag 4: the game is stopped, so the weather has no frame time and stands still.
constexpr uint32_t kSystemStopped = 0x4;

// Rain types are bit masks: 1 rain, 2 snow.
constexpr int32_t kRain = 1;
constexpr int32_t kSnow = 2;

// The distances Prepare asks the cache for its textures at.
constexpr float kLightningDistance = 0.5f;   // 0x3f000000
constexpr float kRainDistance = 0.5f;
constexpr float kSnowDistance = 0.1f;        // 0x3dcccccd

// The blending mode of a bolt (0x200d52) and its vertex format (0x1c4), as GetNextPrimitive takes them.
constexpr uint32_t kLightningBlend = 0x200d52;
constexpr uint32_t kLightningVertexFormat = 0x1c4;
constexpr int32_t kTriangleList = 4;

// The wind offset of the drops wraps at 4 units either way.
float WrapOffset(float value) {
    if (value < -4.0f) return value + 8.0f;
    if (value > 4.0f) return value - 8.0f;
    return value;
}

// One axis of the camera's place wrapped into a window of eight units, in 1/16384 steps: the
// original works it out as (cvt_ul(16384 * (4 + scale * position)) & 0x1ffff) / 16384 - 4.
float WrappedCamera(float scale, float position) {
    const float shifted = 4.0f + scale * position;
    const uint32_t fixed = cw_cvt_fp2unsigned(static_cast<double>(16384.0f * shifted)) & 0x1ffffu;
    return ppc_fmsubs(1.0f / 16384.0f, static_cast<float>(fixed), 4.0f);
}

}  // namespace

// The lightning's texture when lightning is on (mF3308), the rain's or the snow's when there are
// drops. Always answers 1.
int32_t CRain::Prepare() {
    if (mF3308 != 0 && mLightningTexture != nullptr) gei_TCache->Prepare(mLightningTexture, kLightningDistance);
    if (mRaindrops != 0) {
        if (mRainType == kSnow) {
            if (mSnowTexture != nullptr) gei_TCache->Prepare(mSnowTexture, kSnowDistance);
        } else if (mRainType == kRain) {
            if (mRainTexture != nullptr) gei_TCache->Prepare(mRainTexture, kRainDistance);
        }
    }
    return 1;
}

// A bolt from `from` to `to`. The picture is the lightning texture's place on its page when it
// is on one (else all zero and no page). 1 when the draw primitive has no room for a primitive
// (nothing is drawn); 0 when building the bolt fails; else what ExecuteCurrentPrimitive answers.
// The count of the first lightning list (index mFA188, which is zero here) is cleared.
int32_t CRain::RenderLightning2(D3DVECTOR* from, D3DVECTOR* to) {
    CTextureEntry* texture = mLightningTexture;
    CTextureBase* page = texture != nullptr ? texture->GetTexture() : nullptr;
    if (page != nullptr) {
        mFA170 = texture->mUOffset;
        mFA174 = mLightningTexture->mVOffset;
        mFA178 = mFA170 + mLightningTexture->mUSize;
        mFA17C = mFA174 + mLightningTexture->mVSize;
    } else {
        mFA17C = mFA178 = mFA174 = mFA170 = 0.0f;
    }
    if (gei_DrawPrimitive->GetNextPrimitive(mF330C, kTriangleList, kLightningVertexFormat, kLightningBlend, page) ==
        nullptr) {
        return 1;
    }
    mFA188 = 0;
    mLightningCounts[mFA188] = 0;
    if (NewRenderLightning(from, to) == 0) return 0;
    if (AddLightningSection(nullptr, from) == 0) return 0;
    gei_DrawPrimitive->ProjectCurrentPrimitive(0);
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(0.0f);
}

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

namespace {

// The blending modes and the vertex format of the rain and snow primitives, as GetNextPrimitive
// takes them. The rain's depends on the display (flag 2 = 0x200d22, else 0x200d52).
constexpr uint32_t kRainBlend = 0x200d52;
constexpr uint32_t kRainBlendDisplay2 = 0x200d22;
constexpr uint32_t kSnowBlend = 0x556;
constexpr uint32_t kTLCTEXFormat = 0x1c4;
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

uint32_t GetCycleCount();

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

void CRain::SetSnowCubicParameters(float a, float b, float c) { mSnowParameters = {a, b, c}; }

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

// One frame of weather. The frame time is the milliseconds since the last call from the game's
// clock (at most 500; a longer gap counts as 500) in seconds, none when the game is stopped.
// The camera's place is wrapped into a window of eight units (rain falls the same way
// whatever the camera does), the view matrix's rotation kept without its translation, and the
// wind offset of the drops moved on (by the type's velocity along the direction; the y moves
// the opposite way to x and z); the rain or the snow is drawn from there. 0 when drawing the
// lightning, the rain or the snow fails.
int32_t CRain::Render() {
    uint32_t previous = mClock;
    mClock = gReadClock();
    const uint32_t now = mClock;
    if (static_cast<int32_t>(now - previous) > 500) previous = now - 500;
    mDeltaMilliseconds = static_cast<int32_t>(now - previous);
    mDeltaTime = static_cast<float>(mDeltaMilliseconds) / 1000.0f;
    if ((gei_System.mFlags & kSystemStopped) != 0) mDeltaTime = 0.0f;
    mF330C = 2;
    if ((gei_System.mFlags & kSystemStopped) != 0) mDeltaTime = 0.0f;
    if (__RenderLightning() == 0) return 0;
    if (mRaindrops == 0) return 1;

    // The camera's place scaled by the type's reach (a first-person view uses a fixed one).
    float reach = mF3340;
    if (gIsFirstPerson() != 0) reach = 0.3f;
    CMatrix* view = gei_ViewMatrix;
    float cameraX = WrappedCamera(reach, view->m[3][0]);
    float cameraY = WrappedCamera(reach, -view->m[3][1]);
    float cameraZ = WrappedCamera(reach, view->m[3][2]);

    // The drops are drawn in the camera's orientation without its place.
    std::memcpy(mMatrix, gei_InvViewMatrix, sizeof mMatrix);
    mMatrix[12] = 0.0f;
    mMatrix[13] = 0.0f;
    mMatrix[14] = 0.0f;

    const float velocity = mDeltaTime * ((mRainType & kRain) != 0 ? mRainVelocity : mSnowVelocity);
    const float stepX = mDirection.x * velocity;
    const float stepY = mDirection.y * velocity;
    const float stepZ = mDirection.z * velocity;
    mF3328.x = WrapOffset(mF3328.x - stepX);
    mF3328.y = WrapOffset(mF3328.y + stepY);
    mF3328.z = WrapOffset(mF3328.z - stepZ);

    cameraX = cameraX + mF3328.x;
    cameraY = cameraY + mF3328.y;
    cameraZ = cameraZ + mF3328.z;

    // The view matrix's third row (the direction the camera looks along); the original also
    // works out its dot product with the view's translation and then overwrites the result with
    // the (zero) translation of the matrix copied above.
    mFA1BC.x = gei_ViewMatrix->m[2][0];
    mFA1BC.y = gei_ViewMatrix->m[2][1];
    mFA1BC.z = gei_ViewMatrix->m[2][2];
    mFA1C8 = mMatrix[14];

    if ((mRainType & kRain) != 0) {
        if (__RenderRain(cameraX, cameraY, cameraZ) == 0) return 0;
    } else if ((mRainType & kSnow) != 0) {
        if (__RenderSnow(cameraX, cameraY, cameraZ) == 0) return 0;
    }
    return 1;
}

// tb_rain.cpp, the lightning bolts' own members (engine library):
//   0x100243cc CRain::AddLightning  (144)
//   0x100244a0 SLightning::Init     (368)
//   0x10021f48 GeneratePoint        (1092)
//   0x100223d8 CRain::NewRenderLightning (1324)
//   0x1002163c CRain::AddLightningSection (2240)
//
// The lobby's weather tick calls AddLightning for each strike; __RenderLightning
// (src/tb_rain_render.cpp) ages the bolts and draws them from the 16 slots filled here.
//
// Floating point. Every operation follows the listing: single-precision subtracts and
// multiplies are plain, fused multiply-adds and subtracts go through ppc_math.h, and the
// length is the double square root rounded to single (frsp).

// Adds a bolt from `start` to `end` if there is room (the slots are 16): the bolt is set up
// by SLightning::Init and counted. `a` is the bolt's life in milliseconds and `b` asks for a
// seed. Always answers 1, a full table included (the strike is dropped silently).
int32_t CRain::AddLightning(D3DVECTOR start, D3DVECTOR end, int32_t a, int32_t b) {
    if (mF3308 < 16) {
        mLightning[mF3308].Init(start, end, a, b);
        mF3308 = mF3308 + 1;
    }
    return 1;
}

// Sets a bolt up. The ends and the time are stored as given. The seed (an integer word) is the
// cycle counter ORed with 1 when `seeded` is not 0, else 0; mF28 is not written. The direction
// is start - end, and then normalised unless its length is exactly 0.0f (a NaN length is
// unequal, so the division runs and the NaNs stay NaNs; an infinite length gives a scale of
// 0, so the components become zeros, or NaN where a component is infinite). The length is
// taken as sqrt(z*z + (x*x + y*y)) with the square of x fused into the sum:
// f0 = y*y, f2 = z*z (single), f0 = x*x + f0 (one rounding), sum = f2 + f0.
void SLightning::Init(D3DVECTOR start, D3DVECTOR end, int32_t time, int32_t seeded) {
    mStart = start;
    mEnd = end;
    mTime = time;
    mSeed = seeded != 0 ? (GetCycleCount() | 1u) : 0u;

    mDirection.x = start.x - end.x;
    mDirection.y = start.y - end.y;
    mDirection.z = start.z - end.z;

    float x = mDirection.x;
    float y = mDirection.y;
    float z = mDirection.z;
    const float yy = y * y;
    const float zz = z * z;
    const float partial = ppc_fmadds(x, x, yy);
    const float sum = zz + partial;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(sum)));
    if (length != 0.0f) {
        const float scale = 1.0f / length;
        x = x * scale;
        y = y * scale;
        z = z * scale;
    }
    mDirection.x = x;
    mDirection.y = y;
    mDirection.z = z;
}

namespace {

float SingleSqrt(float sum) {
    return static_cast<float>(std::sqrt(static_cast<double>(sum)));
}

// Subdivision lengths fuse x into y, then z into that sum.
float SubdivisionLengthSquared(D3DVECTOR d) {
    return ppc_fmadds(d.z, d.z, ppc_fmadds(d.x, d.x, d.y * d.y));
}

}  // namespace

int32_t CRain::AddLightningSection(D3DVECTOR* from, D3DVECTOR* to) {
    if (from != nullptr && to != nullptr && from->x == to->x && from->y == to->y && from->z == to->z) return 1;
    const int32_t n = mLightningCounts[mFA188];
    mLightningCounts[mFA188] = n + 1;
    if (n == 0) {
        if (from == nullptr) return 1;
        D3DVECTOR input[2] = {*from, *to};
        float projected[2][4];
        MathUnitTransform(reinterpret_cast<sMatrix*>(gei_FinalMatrix), projected, input, 0x10, 0xc, 2, 0);
        float* point = mLightningPoints[mFA188];
        float* normal = mLightningNormals[mFA188];
        std::memcpy(point, projected[1], 16);
        normal[0] = projected[0][0] - projected[1][0];
        normal[1] = projected[0][1] - projected[1][1];
        const float sum = normal[0] * normal[0] + normal[1] * normal[1];
        if (static_cast<double>(sum) == 0.0) return 1;
        const float k = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(sum));
        normal[0] = normal[0] * k;
        normal[1] = normal[1] * k;
        float* edges = mLightningEdges[mFA188];
        std::memcpy(edges + 4, projected[1], 16);
        std::memcpy(edges, edges + 4, 16);
        edges[0] = edges[0] + normal[1];
        edges[1] = edges[1] - normal[0];
        edges[4] = edges[4] - normal[1];
        edges[5] = edges[5] + normal[0];
        return 1;
    }
    float p[4];
    gei_FinalMatrix->TransformVertex(p, to, 0);
    float* point = mLightningPoints[mFA188];
    float* edges = mLightningEdges[mFA188];
    float* normal = mLightningNormals[mFA188];
    float dx = p[0] - point[0];
    float dy = p[1] - point[1];
    const float sum = from == nullptr ? ppc_fmadds(dx, dx, dy * dy) : dx * dx + dy * dy;
    if (static_cast<double>(sum) == 0.0) return 1;
    const float k = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(sum));
    dx = dx * k;
    dy = dy * k;
    float v0[4], v1[4], v2[4], v3[4];
    if (from == nullptr) {
        std::memcpy(v0, edges, 16);
        std::memcpy(v1, edges + 4, 16);
        std::memcpy(v2, p, 16);
        std::memcpy(v3, p, 16);
        v3[0] = v3[0] + dy;
        v3[1] = v3[1] - dx;
        v2[0] = v2[0] - dy;
        v2[1] = v2[1] + dx;
    } else {
        float ex = dx + normal[0];
        float ey = dy + normal[1];
        const float eSum = ex * ex + ey * ey;
        if (static_cast<double>(eSum) == 0.0) return 1;
        const float k2 = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(eSum));
        ex = ex * k2;
        ey = ey * k2;
        std::memcpy(v0, edges, 16);
        std::memcpy(v1, edges + 4, 16);
        std::memcpy(v2, point, 16);
        std::memcpy(v3, point, 16);
        v3[0] = v3[0] + ey;
        v3[1] = v3[1] - ex;
        v2[0] = v2[0] - ey;
        v2[1] = v2[1] + ex;
        normal[0] = dx;
        normal[1] = dy;
        std::memcpy(point, p, 16);
        std::memcpy(edges, v3, 16);
        std::memcpy(edges + 4, v2, 16);
    }
    // The Mac leaves specular as stack contents; the host starts it at zero.
    SFVF_TLCTEX quad[kWorkVertices] = {};
    const float* vertices[4] = {v0, v1, v2, v3};
    const float u[4] = {mFA170, mFA178, mFA178, mFA170};
    const float v[4] = {mFA174, mFA174, mFA17C, mFA17C};
    for (int32_t i = 0; i < 4; ++i) {
        std::memcpy(&quad[i], vertices[i], 16);
        quad[i].colour = mLightningColor;
        quad[i].u = u[i];
        quad[i].v = v[i];
    }
    if (gei_DrawPrimitive->AddPoly_TLCTEX(quad, 4, 0x20, nullptr, 0) == 0) return 0;
    return 1;
}
void GeneratePoint(D3DVECTOR* out, D3DVECTOR* a, D3DVECTOR* b, float scale) {
    const D3DVECTOR d = {b->x - a->x, b->y - a->y, b->z - a->z};
    const D3DVECTOR mid = {0.5f * (b->x + a->x), 0.5f * (b->y + a->y), 0.5f * (b->z + a->z)};
    const float m2 = mid.z * mid.z + (mid.x * mid.x + mid.y * mid.y);
    const float t1 = static_cast<float>(static_cast<int16_t>(Random()) % 0x1fff) / 8192.0f;
    const float t1Squared = t1 * t1;
    const float t2 = static_cast<float>(static_cast<int16_t>(Random()) % 0x1fff) / 8192.0f;
    float t = t2 * t1Squared;
    if (Random() & 1) t = -t;
    Random();
    D3DVECTOR p = mid;
    const int32_t component = static_cast<int16_t>(Random()) % 3;
    if (component == 0) {
        if (d.x != 0.0f) p.x = mid.x - m2 / d.x;
        else if (d.y != 0.0f) p.y = mid.y - m2 / d.y;
        else p.z = mid.z - m2 / d.z;
    } else if (component == 1) {
        if (d.y != 0.0f) p.y = mid.y - m2 / d.y;
        else if (d.z != 0.0f) p.z = mid.z - m2 / d.z;
        else p.x = mid.x - m2 / d.x;
    } else {
        if (d.z != 0.0f) p.z = mid.z - m2 / d.z;
        else if (d.x != 0.0f) p.x = mid.x - m2 / d.x;
        else p.y = mid.y - m2 / d.y;
    }
    const D3DVECTOR cross = {ppc_fmsubs(d.y, p.z, d.z * p.y),
                             ppc_fmsubs(d.z, p.x, d.x * p.z),
                             ppc_fmsubs(d.x, p.y, d.y * p.x)};
    const float lenD = SingleSqrt(SubdivisionLengthSquared(d));
    const float lenP = SingleSqrt(ppc_fmadds(p.z, p.z, ppc_fmadds(p.y, p.y, p.x * p.x)));
    const float factor = (t * (scale * lenD)) / lenP;
    p.x = p.x * factor;
    p.y = p.y * factor;
    p.z = p.z * factor;
    const float lenCross = SingleSqrt(ppc_fmadds(cross.z, cross.z,
                                               ppc_fmadds(cross.x, cross.x, cross.y * cross.y)));
    const float crossFactor = 0.0f / lenCross;
    out->x = cross.x * crossFactor + (mid.x + p.x);
    out->y = cross.y * crossFactor + (mid.y + p.y);
    out->z = cross.z * crossFactor + (mid.z + p.z);
}

int32_t CRain::NewRenderLightning(D3DVECTOR* from, D3DVECTOR* to) {
    D3DVECTOR mid;
    GeneratePoint(&mid, to, from, mF3310);
    if ((Random() & 0x1ff) < 9 && mFA188 < mLightningDepth) {
        ++mFA188;
        D3DVECTOR dir = {to->x - from->x, to->y - from->y, to->z - from->z};
        const float scale = 1.0f / static_cast<float>(static_cast<double>(mFA188 * 0x60));
        const float length = SingleSqrt(dir.z * dir.z + ppc_fmadds(dir.x, dir.x, dir.y * dir.y));
        if (length != 0.0f) {
            const float reciprocal = 1.0f / length;
            dir.x = dir.x * reciprocal;
            dir.y = dir.y * reciprocal;
            dir.z = dir.z * reciprocal;
        }
        const float f = 2.5f * (scale * static_cast<float>((Random() & 0x3ff) + 0x200));
        D3DVECTOR branch = {dir.x * f, dir.y * f, dir.z * f};
        float g = scale * static_cast<float>((Random() & 0x3ff) + 0x200);
        const D3DVECTOR direction = mLightning[mCurrentLightning].mDirection;
        branch.x = ppc_fmadds(direction.x, g, branch.x);
        branch.y = ppc_fmadds(direction.y, g, branch.y);
        branch.z = ppc_fmadds(direction.z, g, branch.z);
        const float dot = ppc_fmadds(direction.z, branch.z,
                                     ppc_fmadds(direction.x, branch.x, direction.y * branch.y));
        if (dot < 0.0f) {
            g = g * 0.5f;
            branch.x = ppc_fmadds(0.5f, branch.x, direction.x * g);
            branch.y = ppc_fmadds(0.5f, branch.y, direction.y * g);
            branch.z = ppc_fmadds(0.5f, branch.z, direction.z * g);
        }
        D3DVECTOR point = {branch.x + from->x, branch.y + from->y, branch.z + from->z};
        mLightningCounts[mFA188] = 0;
        if (NewRenderLightning(&point, from) == 0) return 0;
        if (AddLightningSection(nullptr, &point) == 0) return 0;
        --mFA188;
    }
    const float jitter = mFA184 * (0.00048828125f * static_cast<float>((Random() & 0xfff) - 0x800));
    D3DVECTOR delta = {mid.x - to->x, mid.y - to->y, mid.z - to->z};
    // bge tests that LT is clear, so an unordered comparison also subdivides.
    if (!(SubdivisionLengthSquared(delta) < mFA180 + jitter)) {
        if (NewRenderLightning(&mid, to) == 0) return 0;
    } else {
        if (AddLightningSection(&mid, to) == 0) return 0;
    }
    delta = {mid.x - from->x, mid.y - from->y, mid.z - from->z};
    if (!(SubdivisionLengthSquared(delta) < mFA180 + jitter)) {
        if (NewRenderLightning(from, &mid) == 0) return 0;
    } else {
        if (AddLightningSection(from, &mid) == 0) return 0;
    }
    return 1;
}
