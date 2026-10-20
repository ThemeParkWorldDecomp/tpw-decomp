// tb_splash.h: the start-up splash screen with its progress bar (engine library, original
// tb_splash.cpp, functions at 0x1002cb24..0x1002da70).
//
// Init shows <path><width>:splash_<language>.tga for 2.5 seconds the first time (the
// "first splash"), then keeps <path><width>:welcome.tga with legal.tga (first time) or
// welcome_<language>.tga overlaid; InterpolateSplash redraws it and the bar, Close waits
// out the first splash's 3 seconds and frees it.
#pragma once
#include <cstdint>

// An image kept for redrawing (0x10 bytes on the Mac).
class CDrawToBackBuffer {
public:
    CDrawToBackBuffer() : mps_Data(nullptr) {}   // inline in CSplash::Init
    ~CDrawToBackBuffer();                       // Close()
    int32_t Close();                            // frees the image (gFree)
    // Copies a TGA's rows into the image, its bottom row at line y from the top; x is
    // not used. Asserts when there is no image.
    int32_t Overlay(char* name, int32_t x, int32_t y);

    void* mps_Data;            // 0x00  pixels from TGALoadAndConvert (name from the assert text)
    int32_t mWidth;            // 0x04
    int32_t mHeight;           // 0x08
    int32_t mBytesPerPixel;    // 0x0c  4
};

class CSplash {
public:
    // Inline in the original (the static initialiser's stores).
    CSplash() : mFrame(0), mProgress(-1), mFrames(500), mBackBuffer(nullptr), mF14(0) {}
    ~CSplash();                // Close()

    // language and path default to "english" and "data:splash:"; frames is the number of
    // InterpolateSplash calls the bar spans (0: 500; negative: fixed 640 layout and no
    // first splash).
    int32_t Init(char* language, char* path, int32_t frames);
    int32_t Close();
    // Three boxes through gei_DrawPrimitive; 0 if one fails.
    int32_t WinProgressBar();
    int32_t InterpolateSplash();

    int32_t mFrame;                    // 0x00  InterpolateSplash calls so far
    int32_t mProgress;                 // 0x04  -1; the bar's fill
    int32_t mFrames;                   // 0x08  500 by default
    int32_t mFill;                     // 0x0c  the bar's fill (0..318)
    CDrawToBackBuffer* mBackBuffer;    // 0x10  the welcome image
    int32_t mF14;                      // 0x14  0 (set by the static initialiser only)
    float mScale;                      // 0x18  100 / mFrames: percent per frame
    uint8_t mF1C[0x28 - 0x1c];
    uint32_t mStartMs;                 // 0x28  milliseconds at the first InterpolateSplash
    int32_t mDrawCount;                // 0x2c  frames that drew the image (up to 3)
    uint8_t mF30[0x38 - 0x30];
};

extern CSplash gei_Splash;          // engine data 0x10169110 (0x38 bytes)
extern int32_t gSplashFirst;        // engine data 0x10051dd8: 1 until the first Close (name invented)
extern uint32_t gSplashStartMs;     // engine data 0x1016910c: when Init finished (name invented)
