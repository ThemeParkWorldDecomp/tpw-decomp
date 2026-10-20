// The original assertions name tb_rain.cpp (inventory: tb_resource_display.cpp).
#include "tb_rain.h"
#include "tb_sky.h"
#include "tb_textureentry.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>

CRain* gei_Rain = nullptr;

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

void CRain::SetRainType(int32_t type) { mRainType = type; }
void CRain::SetLength(float length) { mLength = length; }
void CRain::SetRaindrops(int32_t count) { mRaindrops = count; }
void CRain::SetDirection(D3DVECTOR direction) { mDirection = direction; }
void CRain::SetDirection(float x, float y, float z) { mDirection = {x, y, z}; }
void CRain::SetVelocity(float speed) {
    if (mRainType & 1) mRainVelocity = speed;
    if (mRainType & 2) mSnowVelocity = speed;
}
void CRain::LightningFlash() { mLightningFlash = 1; }
void CRain::SetSnowCubicParameters(float a, float b, float c) { mSnowParameters = {a, b, c}; }

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
