// The ride sign renderer shell (main app, rsse.cpp, unit w82).
#pragma once
#include <cstdint>
#include "Ptr32.h"
#include "platform/macdoze.h"
#include "TrueTypeRender.h"
#include "PlasticShader.h"
#include "Wavelet.h"

// Generated texture name records have the file's 22-byte layout. The dimensions
// stay big-endian, as CsTexture::LoadTextureFromDisk reads their bytes directly.
#pragma pack(push, 1)
struct RideSignTextureRecord {
    uint32_t mFlags;                    // 0x00
    uint8_t mUnused04[2];
    uint8_t mWidth[2];                  // 0x06
    uint8_t mHeight[2];                 // 0x08
    uint8_t mUnused0A[8];
    Ptr32<void> mData;                  // 0x12
};
#pragma pack(pop)
static_assert(sizeof(RideSignTextureRecord) == 0x16);

struct sign_renderer_options {
    const char16_t* mText1;
    const char16_t* mText2;
    void* mTexture1;
    void* mTexture2;
    int32_t mWidth;
    int32_t mHeight;
    int32_t mQuality;
};

class SignRenderer {
public:
    SignRenderer();                                                // 0x100cb594
    ~SignRenderer();                                               // 0x100cb82c
    int32_t sign_load_settings(const char* path);                  // 0x100cb934
    int32_t sign_install_fonts(const char* path);                  // 0x100cbd68
    int32_t sign_render(NS_MacDoze::InternalHandle* dc, sign_renderer_options& options); // 0x100cbee4
    // Mac offsets; native pointers expand the host object, as they do Bitmap.
    int32_t mVersion; // 0x000
    int32_t mF004;
    uint8_t mHasBackground; // 0x008
    uint8_t mPadding009[3];
    int32_t mGlowMode1, mGlowMode2; // 0x00c, 0x010
    CWavelet* mWavelet; // 0x014
    Bitmap mBackground, mOutput; // 0x018, 0x028
    TrueTypeRender mFont1, mFont2; // 0x038, 0x1c8
    PlasticShader mShader1, mShader2; // 0x358, 0x3c4
    ShadowGlow mGlow1, mGlow2; // 0x430, 0x444
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(SignRenderer) == 0x458);
#else
static_assert(sizeof(SignRenderer) == 0x4b8);
#endif
