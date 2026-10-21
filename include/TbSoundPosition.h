// TbSoundPosition.h: the sound library's coordinate record and the distance record (unit w77, "sound symbols shared"). Both
// are plain value classes: three 32-bit words, no virtuals (12 bytes on the Mac and on the host).
//
// The functions are in src/sound/TbSoundPosition.cpp, src/sound/TbSoundSampleInfo.cpp, src/sound/TbSoundSystemInfo.cpp, src/sound/SystemCommand.cpp, src/sound/AudioPlaceHolder.cpp, src/sound/TbSoundManager.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TbSysCommand.cpp, with their addresses in the sound library (Code at 0x10000000). Two stay inline:
// the (float, float, float) constructor, because the game's main program has its own inlined copy of it and builds positions
// on the stack with it (the sound library's copy of it, an unnamed inline, is not a function of its own), and the copy
// constructor and assignment, because several units hold positions by value (TbAudioEvent) and link without TbSoundInfo.cpp.
#pragma once
#include <cstdint>

class TbSoundPosition {
public:
    TbSoundPosition();                                              // 0x1000cb64 (20): three 0.0f
    TbSoundPosition(float x, float y, float z) : mX(x), mY(y), mZ(z) {}
    TbSoundPosition(int32_t x, int32_t y, int32_t z);               // 0x100246e4 (16): stores the three ints AS WORDS
    TbSoundPosition(const TbSoundPosition& other) : mX(other.mX), mY(other.mY), mZ(other.mZ) {}   // 0x1000cb08 (28)

    TbSoundPosition& operator=(const TbSoundPosition& other) {      // 0x100177c0 (28)
        mX = other.mX;
        mY = other.mY;
        mZ = other.mZ;
        return *this;
    }

    void Set(float x, float y, float z);                            // 0x1000c580 (16)
    void Set(int32_t x, int32_t y, int32_t z);                      // 0x1000c8e0 (16): stores the ints AS WORDS
    void SetX(float x);                                             // 0x1000c5c0 (8)
    void SetY(float y);                                             // 0x1000c5f4 (8)
    void SetZ(float z);                                             // 0x1000c628 (8)
    void Get(float& x, float& y, float& z) const;                   // 0x10015f3c (28)

    float length() const;                                           // 0x1000c65c (64): sqrt of z*z + (x*x + y*y), in floats
    double DotProduct(const TbSoundPosition& other) const;          // 0x1000c754 (40): two fused multiply-adds
    bool Valid() const;                                             // 0x1000c7c4 (236): all three finite (via the MathLib calls)

    bool operator!=(const TbSoundPosition& other) const;            // 0x1000c920 (52): memcmp of the 12 bytes
    TbSoundPosition& operator*=(float factor);                      // 0x1000c994 (40)
    TbSoundPosition& operator/=(float divisor);                     // 0x1000c9ec (40)
    TbSoundPosition operator-(const TbSoundPosition& other);        // 0x1000ca44 (132): by value (hidden result pointer in r3)
    TbSoundPosition operator+(const TbSoundPosition& other);        // 0x1000cba4 (132): by value

    float mX;   // +0
    float mY;   // +4
    float mZ;   // +8
};

// The distance record a sample is played with (12 bytes: three floats; the library names no member).
class TbSoundDistance {
public:
    TbSoundDistance();                                              // 0x10015950 (4, empty): the three floats stay as they were
    TbSoundDistance(float a, float b, float c);                     // 0x10001324 (16)

    float mA;   // +0
    float mB;   // +4
    float mC;   // +8
};
