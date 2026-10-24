// tb_splash.cpp: name proven by __sinit_tb_splash_cpp in the engine library.

#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_loaders.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_splash.h"
#include "Wavelet.h"
#include <cstdio>
#include <cstring>


// tb_splash.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x1002cb24..0x1002da70).
//
// Times are milliseconds: the low 32 bits of UTimer::GetAbsolute() / 1000 (an unsigned
// 64-bit divide), compared as unsigned differences. Integer to float conversions are
// single precision (fsubs of the 2^52 bias), the zoom ratios double (fdiv, then frsp).

CSplash gei_Splash;
int32_t gSplashFirst = 1;
uint32_t gSplashStartMs;

namespace {

uint32_t NowMs() {
    return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
}


// The full-screen image draw (inline twice in the original): window coordinates (-1, -1),
// scaled to the display.
void DrawImage(const void* pixels, int32_t width, int32_t height) {
    glPushAttrib(GL_ENABLE_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glRasterPos3f(-1.0f, -1.0f, 0.5f);
    glPixelZoom(static_cast<float>(static_cast<double>(gei_Display.mWidth) / static_cast<double>(width)),
                static_cast<float>(static_cast<double>(gei_Display.mHeight) / static_cast<double>(height)));
    glDrawPixels(width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPopAttrib();
}

}  // namespace

CDrawToBackBuffer::~CDrawToBackBuffer() {
    Close();
}

int32_t CDrawToBackBuffer::Close() {
    if (mps_Data != nullptr) gFree(mps_Data);
    return 1;
}

int32_t CDrawToBackBuffer::Overlay(char* name, int32_t x, int32_t y) {
    if (mps_Data == nullptr) msl_assertion_failed("mps_Data", "tb_splash.cpp", 78);
    if (mps_Data == nullptr) return 1;
    int32_t width, height;
    void* image = TGALoadAndConvert(name, &width, &height, 0, nullptr, mBytesPerPixel);
    if (image == nullptr) return 1;
    int32_t row = (mHeight - y) - height;   // the first destination row
    int32_t skip = 0;
    if (row < 0) {
        skip = -row;
        row = 0;
    }
    uint8_t* dst = static_cast<uint8_t*>(mps_Data) + mBytesPerPixel * (row * mWidth);
    const uint8_t* src = static_cast<const uint8_t*>(image) + mBytesPerPixel * (skip * width);
    int32_t line = y;
    for (int32_t i = 0; i < height; i++) {
        if (line < mHeight) std::memcpy(dst, src, static_cast<uint32_t>(mBytesPerPixel * width));
        line++;
        src += mBytesPerPixel * width;
        dst += mBytesPerPixel * mWidth;
    }
    gFree(image);
    return 1;
}

CSplash::~CSplash() {
    Close();
}

int32_t CSplash::Init(char* language, char* path, int32_t frames) {
    bool fixed = frames < 0;
    if (frames < 0) frames = -frames;
    if (language == nullptr) language = const_cast<char*>("english");
    if (path == nullptr) path = const_cast<char*>("data:splash:");
    gei_System.mFlags |= CSystem::kFlag00000001;
    mFrame = 0;
    if (frames == 0) frames = 500;
    mFrames = frames;
    mScale = 100.0f / static_cast<float>(mFrames);
    int32_t width = gei_Display.mWidth;
    if (width < 640 || width > 1024) width = 640;
    int32_t legalY, welcomeY;   // where the overlay's bottom row goes
    char dir[260], splash[260], legal[260], welcomeLanguage[260], welcome[260];
    if (fixed) {
        legalY = 408;
        welcomeY = 224;
        std::snprintf(dir, sizeof dir, "%s%d:", path, 640);
    } else {
        if (width == 1024) {
            legalY = 652;
            welcomeY = 358;
        } else if (width == 800) {
            legalY = 510;
            welcomeY = 280;
        } else {
            legalY = 408;
            welcomeY = 224;
        }
        std::snprintf(dir, sizeof dir, "%s%d:", path, width);
    }
    std::snprintf(splash, sizeof splash, "%ssplash_%s.tga", dir, language);
    std::snprintf(legal, sizeof legal, "%slegal.tga", dir);
    std::snprintf(welcomeLanguage, sizeof welcomeLanguage, "%swelcome_%s.tga", dir, language);
    std::snprintf(welcome, sizeof welcome, "%swelcome.tga", dir);
    if (gSplashFirst != 0 && !fixed) {
        int32_t w, h;
        void* pixels = TGALoadAndConvert(splash, &w, &h, 0, nullptr, 4);
        if (pixels != nullptr) {
            DrawImage(pixels, w, h);
            glGetError();
            if (pixels != nullptr) gFree(pixels);
            gei_System.mFlags |= 0x08000000;
            RenderSystem_FlipScreen();
            uint32_t start = NowMs();
            while (NowMs() - start < 2500) SystemTask();
        }
        if (pixels != nullptr) gFree(pixels);
    }
    CDrawToBackBuffer* back = new CDrawToBackBuffer;
    mBackBuffer = back;
    back->mBytesPerPixel = 4;
    back->mps_Data = TGALoadAndConvert(welcome, &back->mWidth, &back->mHeight, 0, nullptr, back->mBytesPerPixel);
    if (!fixed) {
        if (gSplashFirst != 0) back->Overlay(legal, 0, legalY);
        else back->Overlay(welcomeLanguage, 0, welcomeY);
    }
    gei_Display.mFlags |= 0x40000000;
    gSplashStartMs = NowMs();
    mProgress = -1;
    mFill = 0;
    return 1;
}

int32_t CSplash::Close() {
    if (gSplashFirst != 0) {
        while (NowMs() - gSplashStartMs < 3000) SystemTask();
    }
    if (mBackBuffer != nullptr) {
        delete mBackBuffer;
        mBackBuffer = nullptr;
    }
    gei_Display.mFlags &= ~0x40000000u;
    gSplashFirst = 0;
    return 1;
}

int32_t CSplash::WinProgressBar() {
    int32_t width = gei_Display.mWidth;
    int32_t height = gei_Display.mHeight;
    int32_t percent = ppc_fctiwz(static_cast<float>(mFrame) * mScale);
    int32_t scaleX = static_cast<int32_t>(static_cast<uint32_t>(width) << 16) / 640;   // 16.16
    int32_t barHeight = (height * 11) / 480;
    int32_t top = (height * 446) / 480;
    int32_t left = (width * 14) / 640;
    gei_DrawPrimitive->BeginDraw();
    int32_t fill = percent > 100 ? 318 : (percent * 318) / 100;
    int32_t right = left + (((fill + 1) * scaleX) >> 16);
    int32_t seg = barHeight / 5;
    const int32_t dark = static_cast<int32_t>(0x800000ffu);
    const int32_t bright = static_cast<int32_t>(0xff0000ffu);
    if (gei_DrawPrimitive->DrawFilledBox(static_cast<float>(left), static_cast<float>(top), static_cast<float>(right),
                                         static_cast<float>(top + seg), dark, nullptr) == 0)
        return 0;
    int32_t seg4 = (barHeight * 4) / 5;
    if (gei_DrawPrimitive->DrawFilledBox(static_cast<float>(left), static_cast<float>(top) + static_cast<float>(seg),
                                         static_cast<float>(right), static_cast<float>(top + seg4), bright,
                                         nullptr) == 0)
        return 0;
    if (gei_DrawPrimitive->DrawFilledBox(static_cast<float>(left), static_cast<float>(top) + static_cast<float>(seg4),
                                         static_cast<float>(right), static_cast<float>(top + barHeight), dark,
                                         nullptr) == 0)
        return 0;
    gei_DrawPrimitive->EndDraw();
    mFill = fill;
    mProgress = fill;
    return 1;
}

int32_t CSplash::InterpolateSplash() {
    if (mFrame == 0) {
        mStartMs = NowMs();
        mDrawCount = 0;
    }
    SamsUtilities::UTimer::GetAbsolute();   // read and not used
    CDrawToBackBuffer* back = mBackBuffer;
    if (back == nullptr) {
        int32_t n = mDrawCount++;
        if (n < 3) RenderSystem_ClearViewport();
    } else if (mDrawCount < 3) {
        if (back->mps_Data != nullptr) DrawImage(back->mps_Data, back->mWidth, back->mHeight);
        glGetError();
        gei_System.mFlags |= 0x08000000;
        mDrawCount++;
    }
    mFrame++;
    if (gSplashFirst == 0 && WinProgressBar() == 0) return 0;
    if (RenderSystem_FlipScreen() == 0) return 0;
    return 1;
}
