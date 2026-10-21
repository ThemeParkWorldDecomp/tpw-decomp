// LBLobbyView.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of LBLobbyView code, named after it.

#include "Light.h"
#include "lobby.h"
#include "OptionsBalance.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_sky.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include <cmath>

// Lobby camera, lighting, fog, sky colour and fade (main 0x100a8b40..0x100a9b64).

namespace {
// The original mixes single products with a double fused x*x + y*y sum.
void Normalize(s3DPoint& v) {
    const float length = static_cast<float>(std::sqrt(static_cast<double>(v.z * v.z) +
        ppc_fmadd(static_cast<double>(v.x), static_cast<double>(v.x), static_cast<double>(v.y * v.y))));
    v.x /= length;
    v.y /= length;
    v.z /= length;
}
s3DPoint Cross(const s3DPoint& a, const s3DPoint& b) {
    return {ppc_fmsubs(a.y, b.z, a.z * b.y), ppc_fmsubs(a.z, b.x, a.x * b.z),
            ppc_fmsubs(a.x, b.y, a.y * b.x)};
}
void LightColor(CLight* light, uint32_t color) {
    gei_System.UpdateLights();
    light->mColorWord = color;
    light->mColor[0] = static_cast<float>(color & 0xff) / 255.0f;
    light->mColor[1] = static_cast<float>(color >> 24) / 255.0f;
    light->mColor[2] = static_cast<float>((color >> 16) & 0xff) / 255.0f;
    light->mColor[3] = static_cast<float>((color >> 8) & 0xff) / 255.0f;
}
void LightNormal(CLight* light, const s3DPoint& normal) {
    gei_System.UpdateLights();
    light->mNormal = normal;
}
} // namespace

CLBLobbyView::CLBLobbyView() : mLight(nullptr) {}
CLBLobbyView::~CLBLobbyView() = default;

void CLBLobbyView::Init() {
    mSavedDisplayFlags = gei_Display.mFlags;
    mSavedFogColor = gei_Display.mFogColor;
    mSavedFogNear = gei_Display.mFogNear;
    mSavedFogFar = gei_Display.mFogFar;
    TbDynamicStringTemplate<char> sky("Data:Levels:fantasy");
    gei_Sky.SkyLoad(sky.mData);
    gei_Sky.SetSpherePos({0.0f, 180.0f, 0.0f});
    gei_Display.mFlags = (gei_Display.mFlags & ~0x10000u) | 0x4000000u;
    SetFogOff();
    SetFogSettings(0x44ddffffu, 50.0f, 300.0f);
    mPosition = {500.0f, 100.0f, 500.0f};
    mTarget = {500.0f, 0.0f, 500.0f};
    mFov = 115.0f;
    SetGlobalAmbientLight(0, 0);
    SetGlobalDirectionalLight(0, nullptr, nullptr, 0);
    mLightUpOffset = 0.0f;
    mLightRightOffset = 0.0f;
    mLightDownward = 0;
    MoveView();
    for (int i = 0; i < 3; ++i) mSkyCurrent[i] = mSkyTarget[i] = 0;
    mFadeCurrent = mFadeTarget = 0;
}

void CLBLobbyView::Tick() {
    gei_ProjectionMatrix.Projection(0.1f, 1000.0f, mFov);
    gei_Gamut->Init(mFov);
    MoveView();
    HandleCamera();
    UpdateSky();
    UpdatedFade();
}
void CLBLobbyView::Render() {}
void CLBLobbyView::Close() {
    SetGlobalAmbientLight(0, 0);
    SetGlobalDirectionalLight(0, nullptr, nullptr, 0);
    gei_Display.mFlags = mSavedDisplayFlags;
    gei_Display.SetFogColor(mSavedFogColor);
    gei_Display.mFogNear = mSavedFogNear;
    gei_Display.mFogFar = mSavedFogFar;
    gei_Sky.ResetSpherePos();
}

void CLBLobbyView::WindowMessagesFn(int32_t, int32_t, int32_t) {}
void CLBLobbyView::MoveView() {
    SetExternalCamera(1);
    SetMatrixIdentity(&gExternalCameraMatrix);
    s3DPoint forward = {mTarget.x - mPosition.x, mTarget.y - mPosition.y, mTarget.z - mPosition.z};
    Normalize(forward);
    s3DPoint right = Cross({0.0f, 1.0f, 0.0f}, forward);
    if (right.x == 0.0f && right.y == 0.0f && right.z == 0.0f) right = {1.0f, 0.0f, 0.0f};
    Normalize(right);
    s3DPoint up = Cross(forward, right);
    Normalize(up);
    const s3DPoint rows[] = {right, up, forward, mPosition};
    for (int i = 0; i < 4; ++i) {
        gExternalCameraMatrix.m[i][0] = rows[i].x;
        gExternalCameraMatrix.m[i][1] = rows[i].y;
        gExternalCameraMatrix.m[i][2] = rows[i].z;
    }
    if (mLight != nullptr && mLightFollowsView != 0) {
        s3DPoint position = {
            ppc_fmadds(right.x, mLightRightOffset, ppc_fmadds(up.x, mLightUpOffset, mPosition.x)),
            ppc_fmadds(right.y, mLightRightOffset, ppc_fmadds(up.y, mLightUpOffset, mPosition.y)),
            ppc_fmadds(right.z, mLightRightOffset, ppc_fmadds(up.z, mLightUpOffset, mPosition.z))};
        s3DPoint normal = {mTarget.x - position.x, mLightDownward ? 0.0f : mTarget.y - position.y,
                           mTarget.z - position.z};
        Normalize(normal);
        if (mLightDownward != 0) {
            normal.y -= 0.3f;
            Normalize(normal);
        }
        LightNormal(mLight, normal);
    }
}

void CLBLobbyView::SetGlobalAmbientLight(int32_t enabled, uint32_t color) {
    GlobalDiffuse[0] = 1.0f;
    GlobalDiffuse[1] = enabled ? static_cast<float>(color >> 24) / 255.0f : 0.0f;
    GlobalDiffuse[2] = enabled ? static_cast<float>((color >> 16) & 0xff) / 255.0f : 0.0f;
    GlobalDiffuse[3] = enabled ? static_cast<float>((color >> 8) & 0xff) / 255.0f : 0.0f;
}
void CLBLobbyView::SetGlobalDirectionalLight(int32_t enabled, const uint32_t* color,
                                             const s3DPoint* normal, int32_t followsView) {
    if (enabled == 0) {
        if (mLight != nullptr) {
            gei_MapWho->Unlink(mLight, 0x70);
            delete mLight;
            mLight = nullptr;
        }
    } else if (mLight == nullptr) {
        s3DPoint direction = normal != nullptr ? *normal : s3DPoint{1.0f, 0.0f, 0.0f};
        Normalize(direction);
        mLight = new CLight;
        mLight->mFlags = 2;
        LightColor(mLight, color != nullptr ? *color : 0);
        LightNormal(mLight, direction);
        gei_MapWho->Link(mLight, 0x70);
    } else {
        if (color != nullptr) LightColor(mLight, *color);
        if (normal != nullptr) {
            s3DPoint direction = *normal;
            Normalize(direction);
            LightNormal(mLight, direction);
        }
    }
    mLightFollowsView = followsView;
}
void CLBLobbyView::SetFogOn() { gei_Display.mFlags |= 0x2000u; }
void CLBLobbyView::SetFogOff() { gei_Display.mFlags &= ~0x2000u; }
void CLBLobbyView::SetFogSettings(uint32_t color, float nearDistance, float farDistance) {
    gei_Display.SetFogColor(color);
    gei_Display.mFogNear = nearDistance;
    gei_Display.mFogFar = farDistance;
}
void CLBLobbyView::UpdateSky() {
    uint32_t color = gei_Display.mFogColor;
    if (gOptionsBalance.mValues[6].mInteger > 1 && static_cast<int32_t>(gPlayerSettings.mCurrent.mRenderer) == 1 &&
        (mSkyCurrent[0] != mSkyTarget[0] || mSkyCurrent[1] != mSkyTarget[1] || mSkyCurrent[2] != mSkyTarget[2])) {
        // The original uses the blue target for red as well; preserve it.
        mSkyCurrent[0] += (mSkyTarget[2] - mSkyCurrent[0]) >> 4;
        mSkyCurrent[1] += (mSkyTarget[1] - mSkyCurrent[1]) >> 4;
        mSkyCurrent[2] += (mSkyTarget[2] - mSkyCurrent[2]) >> 4;
        color = (static_cast<uint32_t>(mSkyCurrent[0] >> 16) << 24) |
                (static_cast<uint32_t>(mSkyCurrent[1] >> 16) << 16) |
                (static_cast<uint32_t>(mSkyCurrent[2] >> 16) << 8) | 0xffu;
    }
    for (auto& word : gei_Sky.mColors[0]) word = color;
}
void CLBLobbyView::UpdatedFade() {
    const int32_t step = (mFadeTarget - mFadeCurrent) >> 3;
    if (step > 1000) mFadeCurrent += step;
    else if (step > 1) mFadeCurrent = mFadeTarget - 1;
    else mFadeCurrent = mFadeTarget;
    if (mFadeCurrent > 0) {
        gei_Display.mFlags |= 0x80000u;
        const int32_t fade = mFadeCurrent >> 8;
        gei_Display.mFade = fade < 0 ? 0 : fade > 255 ? 255 : static_cast<uint32_t>(fade);
    } else gei_Display.mFlags &= ~0x80000u;
}
