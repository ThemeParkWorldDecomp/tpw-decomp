// tb_capture.h: screenshots (engine library, original tb_capture.cpp, functions at
// 0x10002ddc..0x10003424). GAMESYSTEM_Screenshot (main app) picks a free file name and
// calls SetCapture; the frame loop calls Capture, which copies the back buffer, converts
// it to 24-bit pixels and writes an uncompressed TGA.
#pragma once
#include <cstdint>

class CCapture {
public:
    // Inline constructor (the static initialiser zeroes these four fields).
    CCapture() : mScreen(nullptr), mRGB(nullptr), mPixels(0), mPending(nullptr) {}

    int32_t SaveUncompressedTGA(char* name);
    int32_t ConvertCapture();
    bool GetCapture();
    int32_t Capture();
    int32_t SetCapture(char* name);

    void* mScreen;       // 0x00  copy of the back buffer (gMalloc), freed through gFree
    void* mRGB;          // 0x04  24-bit pixels, bottom row last
    int32_t mPixels;     // 0x08  width * height
    char mName[0x80];    // 0x0c  file name (SetCapture copies it unchecked)
    char* mPending;      // 0x8c  mName while a capture is requested, else NULL
};

extern CCapture gei_Capture;   // engine data 0x10052a40 (0x90 bytes)
