#include "tb_sky.h"
#include "tb_textureentry.h"
#include "tb_loaders.h"
#include "tb_error.h"
#include "Wavelet.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include <cmath>
#include <cstring>
#include <initializer_list>

CSky gei_Sky; // __sinit_tb_sky_cpp, 0x1002cac8; native static lifetime runs the same constructor.

int32_t CSky::Load(CTextureEntry** texture, char* path) {
    if (*texture != nullptr) {
        (*texture)->Close();
        delete[] *texture;
        *texture = nullptr;
    }
    *texture = gei_TLoadManager->LoadFile(path);
    if (*texture == nullptr) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
        return 0;
    }
    (*texture)->mFlags = ((*texture)->mFlags & ~0x100u) | 0x30200u;
    return 1;
}

int32_t CSky::LoadAll(char* sky, char* colors, char* cylinder) {
    if (!Load(&mSkyTexture, sky)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    if (!Load(&mColorTexture, colors)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    if (!Load(&mCylinderTexture, cylinder)) gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    InitColors();
    return 1;
}

int32_t CSky::LoadAdd(void** out, char* path, D3DVECTOR position, float size, int32_t a, int32_t b) {
    if (mAddCount >= 16) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754012u));
        return 0;
    }
    SSkyAdd& add = mAdds[mAddCount++];
    if (!Load(&add.mTexture, path)) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
        *out = nullptr;
        return 0;
    }
    add.mPosition = position;
    add.mF04 = a;
    add.mF08 = b;
    add.mSize = size;
    *out = add.mTexture;
    return 1;
}

void CSky::RemoveAdd(int32_t index) {
    CTextureEntry* texture = mAdds[index].mTexture;
    mAdds[index].mTexture = nullptr;
    if (texture != nullptr) {
        texture->Close();
        delete[] texture;
    }
}

int32_t CSky::RemoveAdd(void* texture) {
    int32_t index = mAddCount;
    // The inclusive search reads the Mac word mAddCount when the array is full.
    while (index >= 0) {
        void* candidate = index == 16 ? reinterpret_cast<void*>(static_cast<uintptr_t>(
                                               static_cast<uint32_t>(mAddCount))) : mAdds[index].mTexture;
        if (candidate == texture) break;
        --index;
    }
    if (index < 0) return 0;
    RemoveAdd(index);
    if (--mAddCount != 0) {
        SSkyAdd saved = mAdds[index];
        mAdds[index] = mAdds[mAddCount];
        mAdds[mAddCount] = saved;
    }
    return 1;
}

int32_t CSky::SkyLoad(char* folder) {
    char path[120];
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky.tga");
    if (!Load(&mSkyTexture, gFileName(path, 1)))
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky_rgb.tga");
    if (!Load(&mColorTexture, gFileName(path, 1))) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    } else {
        InitColors();
        mColorTexture->Close();
        delete[] mColorTexture;
        mColorTexture = nullptr;
    }
    std::strcpy(path, folder);
    std::strcat(path, ":sky:sky_cyl.tga");
    if (!Load(&mCylinderTexture, gFileName(path, 1)))
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754001u));
    return 1;
}

CSky::CSky() {
    std::memset(mMatrix, 0, sizeof mMatrix);
    mMatrix[0] = mMatrix[5] = mMatrix[10] = mMatrix[15] = 1.0f;
    mWidth = mHeight = 2400;
    mSphereRadius = 24000.0f;
    SetActiveLayers(gei_System.mRenderer == 4 ? 1 : 4);
    mSpherePosition = {0.0f, 300.0f, 0.0f};
    for (int32_t i = 0; i < 4; ++i) {
        mPosition[i] = {0.0f, 0.0f, 0.0f};
        mScrollDirection[i] = {ppc_fmadds(0.0005f, static_cast<float>(i), 0.0008f), 0.0f,
                              ppc_fnmsubs(0.00003f, static_cast<float>(i), 0.00009f)};
        mLayerAngles[i] = (3.1415927410125732f * static_cast<float>(7 - i)) / 112.0f;
    }
    mSkyTexture = mColorTexture = mCylinderTexture = nullptr;
    int out = 0;
    for (int side = 0; side < 2; ++side) {
        const int base = side * 36;
        for (int row = 0; row < 8; ++row) {
            for (int column = 0; column < 3; ++column) {
                const uint16_t a = base + row * 4 + column;
                const uint16_t b = a + 4;
                mCylinderIndices[out++] = a;
                mCylinderIndices[out++] = b + 1;
                mCylinderIndices[out++] = b;
                mCylinderIndices[out++] = a;
                mCylinderIndices[out++] = a + 1;
                mCylinderIndices[out++] = b + 1;
            }
        }
    }
    mAddCount = 0;
    mF2F0C = -49;
    mF2F10 = 56;
}

CSky::~CSky() { Close(); }

void CSky::SetSpherePos(D3DVECTOR pos) {
    mSpherePosition = pos;
    Init();
}
void CSky::SetSphereRadius(float radius) {
    mSphereRadius = radius;
    Init();
}
void CSky::ResetSpherePos() {
    mSpherePosition = {0.0f, 300.0f, 0.0f};
    Init();
}
void CSky::SetupScrollDirection(int32_t layer, D3DVECTOR direction) {
    mScrollDirection[layer] = direction;
}
void CSky::SetActiveLayers(int32_t layers) {
    if (layers < 0 || layers > 4)
        msl_assertion_failed("(layers>=0) && (layers<=SKYLAYERS)", "tb_sky.cpp", 0x160);
    mActiveLayers = layers;
}
CTextureEntry* CSky::GetSkyTEntry() { return mSkyTexture; }
D3DVECTOR* CSky::GetPosition() { return mPosition; }

int32_t CSky::Init() {
    const float stepX = static_cast<float>(mWidth) / 15.0f;
    const float stepZ = static_cast<float>(mHeight) / 15.0f;
    const float halfX = static_cast<float>(mWidth) * 0.5f;
    const float halfZ = static_cast<float>(mHeight) * 0.5f;
    for (int32_t row = 0; row < 16; ++row) {
        const float z = ppc_fmsubs(stepZ, static_cast<float>(row), halfZ);
        for (int32_t column = 0; column < 16; ++column) {
            D3DVECTOR& v = mGrid[column * 16 + row];
            const float x = ppc_fmsubs(stepX, static_cast<float>(column), halfX);
            const float length = static_cast<float>(std::sqrt(static_cast<double>(x * x + z * z)));
            v = {x + mSpherePosition.x, ppc_fnmsubs(0.2f, length, mSpherePosition.y), z + mSpherePosition.z};
            const uint32_t color = static_cast<uint32_t>(msl_rand() & 0x7f) * 0x10101u | 0xff808080u;
            for (auto& colors : mColors) colors[column * 16 + row] = color;
        }
    }
    const float radius = halfX * 0.6f;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(radius * radius)));
    const float low = ppc_fnmsubs(0.2f, length, mSpherePosition.y);
    const float high = ppc_fneg(low);
    const float range = low - high;
    const float y[] = {low, ppc_fnmsubs(range, 0.25f, low), ppc_fnmsubs(range, 0.5f, low), high};
    const float scales[] = {1.0f, 0.98f, 0.96f, 0.84f};
    // The second side repeats angle 16 and ends at 32.
    for (int32_t i = 0; i < 18; ++i) {
        const int32_t angleIndex = i < 9 ? i * 2 : (i - 1) * 2;
        const float angle = (3.1415927410125732f * static_cast<float>(angleIndex)) * 0.0625f;
        for (int32_t k = 0; k < 4; ++k) {
            const float co = static_cast<float>(std::cos(static_cast<double>(angle)));
            const float x = k == 0 ? ppc_fmadds(radius, co, mSpherePosition.x)
                                   : ppc_fmadds(scales[k], radius * co, mSpherePosition.x);
            const float si = static_cast<float>(std::sin(static_cast<double>(angle)));
            const float z = k == 0 ? ppc_fmadds(radius, si, mSpherePosition.z)
                                   : ppc_fmadds(scales[k], radius * si, mSpherePosition.z);
            mCylinder[i * 4 + k] = {x, y[k], z};
        }
    }
    mIndexCount = 0;
    // The index scan uses row-major neighbours, although the positions were filled column-first.
    for (int32_t row = 0; row < 15; ++row) {
        for (int32_t column = 0; column < 15; ++column) {
            const int32_t a = row * 16 + column;
            const int32_t b = a + 16;
            if (mGrid[a].y > high && mGrid[a + 1].y > high && mGrid[b + 1].y > high) {
                mIndices[mIndexCount++] = a + 1;
                mIndices[mIndexCount++] = a;
                mIndices[mIndexCount++] = b + 1;
            }
            if (mGrid[a].y > high && mGrid[b].y > high && mGrid[b + 1].y > high) {
                mIndices[mIndexCount++] = a;
                mIndices[mIndexCount++] = b;
                mIndices[mIndexCount++] = b + 1;
            }
        }
    }
    return 1;
}

// Samples the colour texture on a 16 x 16 grid: one colour per grid point, 0xRRGGBBAA.
// Pixel data is the Mac's byte for byte on every host (NOTES, the pixel converter's "byte
// order decision"), so the words the original loads are put together from bytes here.
void CSky::InitColors() {
    if (mColorTexture == nullptr) return;
    const int32_t stepX = mColorTexture->mWidth / 16;
    const int32_t stepY = mColorTexture->mHeight / 16;
    const int32_t startX = stepX / 2;
    const int32_t startY = stepY / 2;
    const auto* data = static_cast<const uint8_t*>(mColorTexture->GetData());
    const int32_t format = gei_TextureSettings.mFormat;
    if (format < 2 || format > 5) return;
    for (int32_t row = 0; row < 16; ++row) {
        for (int32_t column = 0; column < 16; ++column) {
            uint32_t color;
            if (format == 2 || format == 4) {
                const uint16_t pitch = static_cast<uint16_t>(mColorTexture->mPitch);
                const uint8_t* p = data + static_cast<intptr_t>(((startY + row * stepY) * pitch + startX + column * stepX) * 2);
                const uint32_t pixel = static_cast<uint32_t>(p[0]) << 8 | p[1];
                color = ((pixel & 0xf) << 4) | ((pixel & 0xf0) << 8) | ((pixel & 0xf000) << 16) |
                        ((pixel & 0xf00) << 12) | 0x080808ffu;
            } else {
                const uint8_t* p = data + static_cast<intptr_t>(((startY + row * stepY) * mColorTexture->mPitch + startX + column * stepX) * 4);
                color = static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
                        static_cast<uint32_t>(p[2]) << 8 | p[3];
            }
            for (auto& colors : mColors) colors[column * 16 + row] = color;
        }
    }
}

int32_t CSky::Close() {
    for (CTextureEntry** slot : {&mSkyTexture, &mColorTexture, &mCylinderTexture}) {
        if (*slot != nullptr) {
            (*slot)->Close();
            delete[] *slot;
            *slot = nullptr;
        }
    }
    return 1;
}
