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
#include <cstring>

#include "Wavelet.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

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
