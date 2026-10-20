// tb_decode_wavelet.h: a wavelet-compressed texture file as loaded (engine library; the
// original file is tb_decode_wavelet.cpp by its assertions, 0x10005de4..0x100067b8; the
// inventory files the range under tb_depthoffield.cpp).
//
// CCompBase is the interface a compressed texture entry unpacks through (the entry's
// mData); CWaveLoad is its one implementation, a ".WCT" file's header and data, unpacked
// by the engine's CWavelet. In the original CCompBase is a virtual base of CWaveLoad
// (0x34 bytes: +0 the pointer to the base at +0x2c, +0x28 the vtable); here it is a plain
// base, which behaves the same.
#pragma once
#include <cstdint>

// Where to unpack to: the buffer, its size in pixels and its row length in bytes.
struct SUnpackDestination {
    void* mData;         // 0x00
    int32_t mWidth;      // 0x04
    int32_t mHeight;     // 0x08
    int32_t mPitch;      // 0x0c
};

class CCompBase {
public:
    virtual ~CCompBase();                                              // 0x100051d0
    virtual int32_t Blit(SUnpackDestination* dest, int32_t flip) = 0;
    virtual int32_t Decompress(SUnpackDestination* dest) = 0;
    virtual uint32_t AverageColor() = 0;
    virtual int32_t IsNaturallyFlipped();                              // 0x10005bf8: 0
};

// The file's header, 0x1c bytes, as read (the 16- and 32-bit fields are little-endian in
// the file; the average colour's four bytes are used in file order). In the loader it
// sits at +0x04.
struct SWaveHeader {
    uint8_t mVersion;            // 0x00  0x12 or 0x13
    uint8_t mAlphaThreshold;     // 0x01
    uint8_t mF06;                // 0x02  0x20: the file has a second block (alpha)
    uint8_t mLevel;              // 0x03  kept modulo 4
    uint16_t mWidth;             // 0x04
    uint16_t mHeight;            // 0x06
    uint16_t mQuality[4];        // 0x08  the four decompression scales
    uint32_t mDataSize;          // 0x10
    uint32_t mAlphaSize;         // 0x14
    uint32_t mAverageColour;     // 0x18
};
static_assert(sizeof(SWaveHeader) == 0x1c, "the wavelet file header is read straight into this");

class CWaveLoad : public CCompBase, public SWaveHeader {
public:
    CWaveLoad();                                    // 0x10005de4
    ~CWaveLoad() override;                          // 0x10005ea8
    // Reads `name` (a .WCT file) through the engine's file callbacks; 0 when it is not
    // there, is not version 0x12 or 0x13, or memory runs out.
    int32_t LoadTexture(char* name);                // 0x10005f68
    // The same from an open sprite file, whose own header gives the sizes.
    int32_t LoadSprite(void* file);                 // 0x10006260
    int32_t Close();                                // 0x1000640c
    int32_t Decompress(SUnpackDestination* dest) override;              // 0x100064b8
    uint32_t AverageColor() override;               // 0x1000660c
    int32_t Blit(SUnpackDestination* dest, int32_t flip) override;      // 0x10006644
    int32_t IsNaturallyFlipped() override;          // 0x100067b8: version 0x12 files are

    uint8_t* mAlphaData;         // 0x20  mAlphaSize bytes (engine allocation)
    uint8_t* mData;              // 0x24  mDataSize bytes
};
