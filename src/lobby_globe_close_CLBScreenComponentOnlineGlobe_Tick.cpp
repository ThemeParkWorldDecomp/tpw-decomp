#include "lobby.h"

#include <cmath>
#include <cstring>

#include "FS_Online.h"
#include "TPSettings.h"
#include "ppc_math.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "platform/arena.h"

s3DPoint gGlobeCentre;       // 0x1029e71c
s3DPoint gGlobeViewTarget;   // 0x1029e728

namespace {

// The unit step of a clamp the original writes with explicit branches: below 0 gives 0, above 1 gives 1, else the value.
float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

// A 4-byte word of the globe's byte-array fields (mF0C, mF34, mF44 in lobby.h), read and written with memcpy, so the body
// reads like the Mac's field accesses.
template <class T>
struct Word {
    uint8_t* p;
    operator T() const {
        T v;
        std::memcpy(&v, p, 4);
        return v;
    }
    Word& operator=(T v) {
        std::memcpy(p, &v, 4);
        return *this;
    }
    Word& operator=(const Word& other) { return *this = static_cast<T>(other); }
};

}  // namespace

// 0x100acce0: picks the city flag under the cursor (the ray from the eye through the cursor against the globe's
// sphere), scrolls the globe by the cursor's position or the right button, then moves the view.
void CLBScreenComponentOnlineGlobe::Tick() {
    Word<float> mTickScale{mF0C + 4};       // Mac 0x10
    Word<float> mDamping{mF0C + 12};        // Mac 0x18
    Word<float> mSizeNow{mF34};             // Mac 0x34
    Word<float> mSpinX{mF44 + 0x00};        // Mac 0x44
    Word<float> mSpinXSpeed{mF44 + 0x04};   // Mac 0x48
    Word<float> mSpinY{mF44 + 0x08};        // Mac 0x4c
    Word<float> mSpinYSpeed{mF44 + 0x0c};   // Mac 0x50
    Word<float> mRadius{mF44 + 0x10};       // Mac 0x54
    Word<int32_t> mZoomedIn{mF44 + 0x14};   // Mac 0x58
    Word<float> mHitX{mF44 + 0x18};         // Mac 0x5c
    Word<float> mHitY{mF44 + 0x1c};         // Mac 0x60
    Word<float> mHitZ{mF44 + 0x20};         // Mac 0x64
    Word<int32_t> mPicking{mF44 + 0x24};    // Mac 0x68
    CLBLobby* lobby = gLobby;
    if (mPicking != 0) {
        float in[3];
        in[0] = 0.3862f * ((2.0f * static_cast<float>(lobby->mWidth)) / 2048.0f - 1.0f);
        in[1] = 0.3862f * (gei_Display.mF04 * ((2.0f * static_cast<float>(0x600 - lobby->mHeight)) / 1536.0f - 1.0f));
        in[2] = 1.0f;
        float out[3];
        gei_ViewMatrix->TransformVertex(out, in, 2);

        const float ex = gLobbyView->mPosition.x;
        const float ey = gLobbyView->mPosition.y;
        const float ez = gLobbyView->mPosition.z;
        const float cx = gGlobeCentre.x;
        const float cy = gGlobeCentre.y;
        const float cz = gGlobeCentre.z;

        float dx = out[0] - ex;
        float dy = out[1] - ey;
        float dz = out[2] - ez;
        float sqy = dy * dy;
        float sqz = dz * dz;
        double sum = static_cast<double>(sqz) + ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(sqy));
        float len = static_cast<float>(std::sqrt(sum));
        dy = dy / len;
        dx = dx / len;
        dz = dz / len;
        float ty = cy - ey;
        float tx = cx - ex;
        float tz = cz - ez;
        float proj = ppc_fmadds(dz, tz, ppc_fmadds(dx, tx, dy * ty));
        float py = ty - dy * proj;
        float px = tx - dx * proj;
        float pz = tz - dz * proj;
        float perp = ppc_fmadds(pz, pz, ppc_fmadds(px, px, py * py));
        float dist = 0.0f;
        bool hit = false;
        if (perp < 13456.0f) {
            float half = static_cast<float>(std::sqrt(static_cast<double>(13456.0f - perp)));
            dist = proj - half;
            if (!(dist >= 0.0010000000474974513f)) dist = proj + half;
            if (dist > 0.0010000000474974513f) hit = true;
        }
        CLBGlobeCityFlag* picked = nullptr;   // mF70 (Mac 0x70): stored as an arena offset below
        if (hit) {
            mHitX = ppc_fmadds(dist, dx, ex);
            mHitY = ppc_fmadds(dist, dy, ey);
            mHitZ = ppc_fmadds(dist, dz, ez);
            float best = 99999.0f;
            auto* node = mCityFlags->mHead;
            while (node != nullptr) {
                CLBGlobeCityFlag* flag = node->mData;
                node = node->mNext;
                const CFSOnlineCity* city = flag->mCity;
                float qy = city->mPosY - mHitY;
                float qx = city->mPosX - mHitX;
                float qz = city->mPosZ - mHitZ;
                float d = ppc_fmadds(qz, qz, ppc_fmadds(qx, qx, qy * qy));
                if (d < 100.0f && d < best) {
                    best = d;
                    picked = flag;
                }
            }
        }
        mF70 = picked != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(picked)) : 0;
        if (picked == nullptr)
            UpdateOnlineCityInfoOverlay(nullptr);
        else
            UpdateOnlineCityInfoOverlay(picked->mCity);
    }

    if (!gPlayerSettings.mCurrent.mRMBScrollOn) {
        float fx = Clamp01(static_cast<float>(lobby->mWidth) / 2048.0f);
        float fy = Clamp01(static_cast<float>(lobby->mHeight) / 1536.0f);
        float step = 50.0f * mTickScale;
        if (fx < 0.009999999776482582f) {
            mSpinXSpeed = mSpinXSpeed - step * (0.009999999776482582f - fx);
        } else if (fx > 0.9900000095367432f) {
            mSpinXSpeed = mSpinXSpeed + step * (fx - 0.9900000095367432f);
        }
        if (fy < 0.009999999776482582f) {
            mSpinYSpeed = mSpinYSpeed + step * (0.009999999776482582f - fy);
        } else if (fy > 0.9900000095367432f) {
            mSpinYSpeed = mSpinYSpeed - step * (fy - 0.9900000095367432f);
        }
    }

    if (gPlayerSettings.mCurrent.mRMBScrollOn && lobby->IsKeyDown(2) != 0) {
        mSpinXSpeed = mSpinXSpeed + mTickScale * (static_cast<float>(lobby->mWidth - 0x400) * 0.0009765625f);
        mSpinYSpeed = mSpinYSpeed - mTickScale * (static_cast<float>(lobby->mHeight - 0x300) / 768.0f);
    }

    mSpinXSpeed = mSpinXSpeed * mDamping;
    mSpinYSpeed = mSpinYSpeed * mDamping;
    mSpinX = ppc_fmadds(mSpinXSpeed, lobby->mTickUnits, mSpinX);
    mSpinY = ppc_fmadds(mSpinYSpeed, lobby->mTickUnits, mSpinY);
    if (mSpinY > mF40) {
        mSpinYSpeed = 0.0f;
        mSpinY = mF40;
    }
    if (mSpinY < -mF40) {
        mSpinYSpeed = 0.0f;
        mSpinY = -mF40;
    }

    float sizeTarget;
    float radiusTarget;
    if (mZoomedIn != 0) {
        sizeTarget = mF3C;
        radiusTarget = lobby->mGlobals->mGlobeRadiusIn;
    } else {
        sizeTarget = mF38;
        radiusTarget = lobby->mGlobals->mGlobeRadiusOut;
    }
    float radius = mRadius;
    mRadius = ppc_fmadds(lobby->mTickUnits, 0.30000001192092896f * (radiusTarget - radius), radius);
    float size = mSizeNow;
    mSizeNow = ppc_fmadds(lobby->mTickUnits, 0.30000001192092896f * (sizeTarget - size), size);
    size = mSizeNow;
    gLobbyView->mLightUpOffset = size;
    gLobbyView->mLightRightOffset = -size;
    gLobbyView->mLightDownward = 0;

    float cosX = static_cast<float>(std::cos(static_cast<double>(mSpinX)));
    float cosY = static_cast<float>(std::cos(static_cast<double>(mSpinY)));
    float sinX = static_cast<float>(std::sin(static_cast<double>(mSpinX)));
    float sinY = static_cast<float>(std::sin(static_cast<double>(mSpinY)));
    float r = mRadius;
    float rc = r * cosY;
    float ry = r * sinY;
    gLobbyView->mPosition.x = cosX * rc;
    gLobbyView->mPosition.y = ry;
    gLobbyView->mPosition.z = sinX * rc;
    gLobbyView->mTarget = gGlobeViewTarget;
}

// 0x100ad54c: the arrow keys (message 0x1000a, 0x2500 left, 0x2600 up, 0x2700 right, 0x2800 down) and the zoom button
// (message 0x10006, argument 2).
void CLBScreenComponentOnlineGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    (void)b;
    Word<int32_t> mZoomedIn{mF44 + 0x14};   // Mac 0x58
    if (message == 0x1000a) {
        if (a == 0x2700) {
            GlobeRight();
        } else if (a == 0x2600) {
            GlobeUp();
        } else if (a == 0x2500) {
            GlobeLeft();
        } else if (a == 0x2800) {
            GlobeDown();
        }
    } else if (message == 0x10006 && a == 2) {
        if (mZoomedIn != 0)
            GlobeZoomOut();
        else
            GlobeZoomIn();
        WorldLobbySetZoomButton(mZoomedIn);
    }
}
