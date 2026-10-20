// Original lobby view versus recorded engine dependencies.
#include <cstring>
#include <new>
namespace { void observe_allocation(std::size_t size, bool array, void* block); }
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#include "lobby.h"
#include "Light.h"
#include "OptionsBalance.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include "calllog.h"
#include "rsse.h"
#include "tb_hfield.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_sky.h"
#include "fake_strings.h"
using namespace calllog;

namespace {
void observe_allocation(std::size_t size, bool, void* block) {
    if (on()) std::memset(block, 0xa5, size);
}
alignas(CLBLobbyView) unsigned char storage[sizeof(CLBLobbyView)];
CLBLobbyView* view;
uint32_t lightResults[3];
int32_t gamutStorage;
int32_t mapStorage;
float Float(uint32_t word) { float f; std::memcpy(&f, &word, 4); return f; }
uint32_t Canon(float f) {
    const uint32_t bits = fbits(f);
    return (bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0 ? 0x7fc00000u : bits;
}
}
CDisplay::CDisplay() {}
CDisplay gei_Display;
void CDisplay::SetFogColor(uint32_t color) { ev("Fog", {color, gei_Display.mFlags}); gei_Display.mFogColor = color; }
CSky::CSky() {}
CSky::~CSky() {}
CSky gei_Sky;
int32_t CSky::SkyLoad(char* path) { ev("SkyLoad", {}, {path}); return 1; }
void CSky::SetSpherePos(D3DVECTOR pos) { ev("Sphere", {fbits(pos.x), fbits(pos.y), fbits(pos.z)}); }
void CSky::ResetSpherePos() { ev("ResetSphere"); }
CSystem::CSystem() {}
CSystem gei_System;
void CSystem::UpdateLights() { ev("Lights"); }
float GlobalDiffuse[4];
CLight* gev_LightChain;
CMapLinkBase::~CMapLinkBase() {}
CGamut* gei_Gamut = reinterpret_cast<CGamut*>(&gamutStorage);
void CGamut::Init(float fov) { ev("Gamut", {fbits(fov)}); }
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(&mapStorage);
int32_t CMapWho::Link(CMapLinkBase* light, int32_t list) { ev("Link", {light != nullptr, list}); return 1; }
int32_t CMapWho::Unlink(CMapLinkBase* light, int32_t list) { ev("Unlink", {light != nullptr, list}); return 1; }
CMatrix gei_ProjectionMatrix;
void CMatrix::Projection(float nearDistance, float farDistance, float fov) {
    ev("Projection", {fbits(nearDistance), fbits(farDistance), fbits(fov)});
}
sMatrix gExternalCameraMatrix;
void SetExternalCamera(int32_t active) { ev("External", {active}); }
void SetMatrixIdentity(sMatrix* matrix) {
    ev("Identity"); std::memset(matrix, 0, sizeof *matrix); for (int i = 0; i < 4; ++i) matrix->m[i][i] = 1.0f;
}
void HandleCamera() { ev("HandleCamera"); }
CLBLobbySystem::CLBLobbySystem() { ev("BaseCtor"); }
CLBLobbySystem::~CLBLobbySystem() { ev("BaseDtor"); }
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}
const BalanceDescription* COptionsBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* COptionsBalance::GetBalanceValuesStart() { return mValues; }
COptionsBalance::~COptionsBalance() {}
COptionsBalance gOptionsBalance;
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
CPlayerSettings gPlayerSettings;

extern "C" {
void tpw_lv_begin(const uint32_t* seed) {
    gei_Display.mFlags = seed[0]; gei_Display.mFogColor = seed[1];
    gei_Display.mFogNear = Float(seed[2]); gei_Display.mFogFar = Float(seed[3]); gei_Display.mFade = seed[4];
    gOptionsBalance.mValues[6].mInteger = static_cast<int32_t>(seed[5]);
    gPlayerSettings.mCurrent.mRenderer = static_cast<CPlayerSettings::eRENDERER>(seed[6]);
    for (int i = 0; i < 16; ++i) gExternalCameraMatrix.m[i / 4][i % 4] = Float(seed[7 + i]);
    for (int i = 0; i < 4; ++i) GlobalDiffuse[i] = Float(seed[23 + i]);
    for (auto& layer : gei_Sky.mColors) for (auto& color : layer) color = 0x12345678;
    std::memset(storage, 0xa5, sizeof storage);
    view = new (storage) CLBLobbyView;
    gev_LightChain = nullptr;
    std::memset(lightResults, 0, sizeof lightResults);
    std::memset(tpw_cl_heap, 0, sizeof tpw_cl_heap);
}
void tpw_lv_op(int32_t op, const uint32_t* a) {
    switch (op) {
    case 1: view->Init(); break;
    case 2: view->Tick(); break;
    case 3: view->Render(); break;
    case 4: view->Close(); break;
    case 5: view->WindowMessagesFn(a[0], a[1], a[2]); break;
    case 6:
        view->mFov = Float(a[0]); view->mPosition = {Float(a[1]), Float(a[2]), Float(a[3])};
        view->mTarget = {Float(a[4]), Float(a[5]), Float(a[6])};
        view->mLightUpOffset = Float(a[7]); view->mLightRightOffset = Float(a[8]); view->mLightDownward = a[9]; break;
    case 7: view->MoveView(); break;
    case 8: view->SetGlobalAmbientLight(a[0], a[1]); break;
    case 9: {
        s3DPoint direction = {Float(a[3]), Float(a[4]), Float(a[5])};
        view->SetGlobalDirectionalLight(a[0], a[1] ? &a[2] : nullptr, a[6] ? &direction : nullptr, a[7]); break;
    }
    case 10: view->SetFogOn(); break;
    case 11: view->SetFogOff(); break;
    case 12: view->SetFogSettings(a[0], Float(a[1]), Float(a[2])); break;
    case 13:
        for (int i = 0; i < 3; ++i) { view->mSkyCurrent[i] = a[i]; view->mSkyTarget[i] = a[i + 3]; } break;
    case 14: view->UpdateSky(); break;
    case 15: view->mFadeCurrent = a[0]; view->mFadeTarget = a[1]; break;
    case 16: view->UpdatedFade(); break;
    case 17: view->~CLBLobbyView(); view = nullptr; break;
    case 18:
        view->mLight->mFlags = a[0];
        lightResults[0] = view->mLight->Prepare();
        lightResults[1] = view->mLight->GetLinkType();
        lightResults[2] = view->mLight->GetData() == view->mLight->mData;
        break;
    case 19: new (view->mLight) CLight; break;
    }
}
int32_t tpw_lv_state(uint32_t* out) {
    int n = 0;
    if (view != nullptr) {
        out[n++] = Canon(view->mFov);
        for (float v : {view->mPosition.x, view->mPosition.y, view->mPosition.z, view->mTarget.x, view->mTarget.y, view->mTarget.z}) out[n++] = Canon(v);
        out[n++] = view->mLight != nullptr;
        out[n++] = view->mLightFollowsView;
        out[n++] = Canon(view->mLightUpOffset); out[n++] = Canon(view->mLightRightOffset); out[n++] = view->mLightDownward;
        out[n++] = view->mSavedDisplayFlags; out[n++] = view->mSavedFogColor;
        out[n++] = Canon(view->mSavedFogNear); out[n++] = Canon(view->mSavedFogFar);
        for (auto v : view->mSkyCurrent) out[n++] = v;
        for (auto v : view->mSkyTarget) out[n++] = v;
        out[n++] = view->mFadeCurrent; out[n++] = view->mFadeTarget;
    }
    out[n++] = gei_Display.mFlags; out[n++] = gei_Display.mFogColor;
    out[n++] = Canon(gei_Display.mFogNear); out[n++] = Canon(gei_Display.mFogFar); out[n++] = gei_Display.mFade;
    for (const auto& row : gExternalCameraMatrix.m) for (float v : row) out[n++] = Canon(v);
    for (float v : GlobalDiffuse) out[n++] = Canon(v);
    for (const auto& layer : gei_Sky.mColors) for (auto v : layer) out[n++] = v;
    if (view != nullptr && view->mLight != nullptr) {
        const auto* light = view->mLight;
        out[n++] = light->mFlags; out[n++] = light->mColorWord;
        for (float v : light->mColor) out[n++] = Canon(v);
        out[n++] = Canon(light->mNormal.x); out[n++] = Canon(light->mNormal.y); out[n++] = Canon(light->mNormal.z);
        out[n++] = light->mNext == light;
    }
    for (auto v : lightResults) out[n++] = v;
    out[n++] = gev_LightChain != nullptr;
    return n;
}
}
