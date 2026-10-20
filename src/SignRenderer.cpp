// SignRenderer shell (main app, rsse.cpp, unit w82; 4,160 bytes).
#include "SignRenderer.h"
#include "FS_System.h"
#include "TPGlobals.h"
#include "platform/samsutils.h"
#include <cstring>
#include <initializer_list>

// 0x100cb594, 512 bytes. Settings words and unspecified shader words are untouched.
SignRenderer::SignRenderer() : mGlow1{}, mGlow2{} {
    mWavelet = CWavelet::Instance();
}

// 0x100cb82c, 220 bytes. Implicit member destruction runs shader 2, shader 1,
// output, background. TrueTypeRender's destructor is the empty deleting-destructor shell (0x100c9d50).
SignRenderer::~SignRenderer() {
    mWavelet->Release();
}

// 0x100cb934, 1,016 bytes. The Mac never closes this file, including on failure.
int32_t SignRenderer::sign_load_settings(const char* path) {
    auto* file = TPFile_Open(path, "rb");
    if (file == nullptr) return 0;
    auto word = [file](void* dest) {
        TPFile_Read(dest, 4, 1, file);
        SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(dest), 1);
    };
    word(&mVersion);
    if (mVersion < 100) return 0;
    word(&mF004);
    TPFile_Read(&mHasBackground, 1, 1, file);
    word(&mGlowMode1);
    word(&mGlowMode2);
    mFont1.load_settings(file);
    mShader1.load_settings(file);
    mFont2.load_settings(file);
    mShader2.load_settings(file);
    auto glow = [&](ShadowGlow& value) {
        TPFile_Read(&value.mRed, 1, 1, file);
        TPFile_Read(&value.mGreen, 1, 1, file);
        TPFile_Read(&value.mBlue, 1, 1, file);
        TPFile_Read(&value.mAlpha, 1, 1, file);
        word(&value.mGrow);
        word(&value.mBlur);
        word(&value.mOffsetX);
        word(&value.mOffsetY);
    };
    if (mGlowMode1 != 0) glow(mGlow1);
    if (mGlowMode2 != 0) glow(mGlow2);
    mShader1.mFile.load(file);
    mShader2.mFile.load(file);
    if (mHasBackground != 0) {
        if (mVersion == 100) mBackground.load(file);
        else mBackground.load_wavelet(file, mWavelet);
    }
    return 1;
}

// 0x100cbd68, 320 bytes.
int32_t SignRenderer::sign_install_fonts(const char* path) {
    TbDynamicStringTemplate<char> name;
    name = path;
    name += gTPGlobals.mSeparator;
    name += mFont1.mName;
    if (NS_MacDoze::AddFontResource(name.mData) == 0) return 0;
    if (std::strcmp(mFont1.mName, mFont2.mName) != 0) {
        name = path;
        name += gTPGlobals.mSeparator;
        name += mFont2.mName;
        if (NS_MacDoze::AddFontResource(name.mData) == 0) return 0;
    }
    return 1;
}

// 0x100cbee4, 2,092 bytes. Only the first height rows of the doubled output
// are split across the two textures, with destination rows traversed bottom-up.
int32_t SignRenderer::sign_render(NS_MacDoze::InternalHandle* dc, sign_renderer_options& options) {
    if (!options.mText1 || !options.mText2 || !options.mTexture1 || !options.mTexture2) return 0;
    const uint32_t width = static_cast<uint32_t>(options.mWidth) * 2;
    const uint32_t height = static_cast<uint32_t>(options.mHeight) * 2;
    mOutput.realloc(width, height, 4);
    mShader1.mMask.realloc(width, height, 1);
    mShader2.mMask.realloc(width, height, 1);
    Bitmap local1;
    local1.mWidth = width;
    local1.mHeight = height;
    local1.mBytesPerPixel = 1;
    local1.mData = new uint8_t[width * height];
    Bitmap local2;
    local2.mWidth = width;
    local2.mHeight = height;
    local2.mBytesPerPixel = 1;
    local2.mData = new uint8_t[width * height];
    mFont1.render_font_unicode(dc, options.mText1, mShader1.mMask);
    mFont2.render_font_unicode(dc, options.mText2, mShader2.mMask);
    if (mGlowMode1 == 0) local1.colour_fill(0, 0, 0, 0);
    else mGlow1.shadow_glow(mShader1.mMask, local1);
    if (mGlowMode2 == 0) local2.colour_fill(0, 0, 0, 0);
    else mGlow2.shadow_glow(mShader2.mMask, local2);
    auto shade = [](PlasticShader& shader, const TrueTypeRender& font) {
        shader.mPeriodY = font.mRight - font.mLeft;
        shader.mOffsetY = font.mLeft;
        if (shader.mMask.mData && shader.mFile.mData) {
            shader.mBevel.realloc(shader.mMask.mWidth, shader.mMask.mHeight, 1);
            shader.mShaded.realloc(shader.mMask.mWidth, shader.mMask.mHeight, 4);
            shader.bevel(shader.mMask, shader.mBevelWidth, shader.mBevel);
            if (shader.mBlur > 0.0f) shader.blur_with_mask(shader.mBevel, shader.mMask, shader.mBlur);
            shader.plastic_shade(shader.mBevel);
        }
    };
    shade(mShader1, mFont1);
    shade(mShader2, mFont2);
    mOutput.colour_fill(0, 0, 0, 0);
    if (mHasBackground) mBackground.alphablt(mOutput, 0, 0);
    auto blt = [&](Bitmap& bitmap, const ShadowGlow& glow) {
        bitmap.colourblt(mOutput, glow.mAlpha, glow.mRed, glow.mGreen, glow.mBlue);
    };
    if (mGlowMode1 == 2 && mGlowMode2 == 2) {
        Bitmap combined;
        combined.mWidth = local1.mWidth;
        combined.mHeight = local1.mHeight;
        combined.mBytesPerPixel = 1;
        combined.mData = new uint8_t[combined.mWidth * combined.mHeight];
        combined.colour_fill(0, 0, 0, 0);
        local1.copy_to(combined);
        local2.shadowblend(combined);
        blt(combined, mGlow1);
    } else if (mGlowMode1 == 2) blt(local1, mGlow1);
    else if (mGlowMode2 == 2) blt(local2, mGlow2);
    auto line1 = [&] {
        if (mGlowMode1 == 1) blt(local1, mGlow1);
        mShader1.mShaded.alphablt(mOutput, 0, 0);
    };
    auto line2 = [&] {
        if (mGlowMode2 == 1) blt(local2, mGlow2);
        mShader2.mShaded.alphablt(mOutput, 0, 0);
    };
    if (mF004 == 0) { line1(); line2(); }
    else { line2(); line1(); }
    uint8_t* source = mOutput.mData;
    if (options.mQuality == 1) mOutput.swizzle_for_gimex();
    if (options.mQuality == 1 || options.mQuality == 2) {
        const int32_t pixelSize = options.mQuality == 1 ? 4 : 2;
        for (int32_t y = 0; y < options.mHeight; ++y) {
            for (void* texture : {options.mTexture1, options.mTexture2}) {
                auto* dest = static_cast<uint8_t*>(texture) + (options.mHeight - 1 - y) * options.mWidth * pixelSize;
                for (int32_t x = 0; x < options.mWidth; ++x) {
                    if (pixelSize == 4) std::memcpy(dest, source, 4);
                    else {
                        uint16_t pixel = (source[0] >> 4) | (source[3] & 0xf0)
                            | ((source[1] & 0xf0) << 8) | ((source[2] & 0xf0) << 4);
                        std::memcpy(dest, &pixel, 2);
                    }
                    source += 4;
                    dest += pixelSize;
                }
            }
        }
    }
    return 1;
}
