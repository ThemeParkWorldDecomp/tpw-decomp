// Bullfrog pixel conversion, TbRaConvert.cpp. Field names except CInverseMap's are invented.
#pragma once
#include <cstdint>
#include "TbPixFmt.h"
#include "TbGraphicFile.h"

struct TbPalette;
class CConverterBase {
public:
    virtual ~CConverterBase(); // 0x10016de4, 76 B
    virtual void ConvertLineUp(uint8_t* dest, uint8_t* source, int32_t count) = 0;
    virtual void ConvertLineDown(uint8_t* dest, uint8_t* source, int32_t count) = 0;
};

// Mac size 16, including DoRead's stack slot. No host destructor: callers use Release.
class CConverter {
public:
    CConverter(); // 0x10016b10, 20 B; leaves the byte counts untouched
    void Init(const TbPixFormat* destination, const TbPixFormat* source, const TbPalette* palette,
              uint32_t unused); // 0x10016b4c, 580 B; fourth argument never read
    void ConvBuf(uint8_t* dest, uint8_t* source, int32_t count); // 0x10016e5c, 88 B
    void ConvertArea(uint8_t* dest, uint8_t* source, uint32_t destPitch, uint32_t sourcePitch,
                     uint32_t rows, uint32_t width); // 0x10016ee4, 212 B
    void Release(); // 0x10016ff4, 92 B
    CConverterBase* mConverter; // Mac +0
    uint32_t mSourceBytes; // +4
    uint32_t mDestBytes; // +8
    int32_t mKind; // +0xc
};
static_assert(sizeof(CConverter) == (sizeof(void*) == 4 ? 16 : 24), "CConverter host layout");

class CMaskConverter : public CConverterBase {
public:
    ~CMaskConverter() override; // 0x10018f30, 88 B
    void Init(const TbPixFormat& source, const TbPixFormat& destination); // 0x10017b78, 308 B
    void GetShiftBits(uint32_t source, uint32_t dest, uint32_t* shift, uint32_t* width); // 0x10018e34, 192 B
    void ConvertLineUp(uint8_t* dest, uint8_t* source, int32_t count) override; // 0x10017cf4, 2164 B
    void ConvertLineDown(uint8_t* dest, uint8_t* source, int32_t count) override; // 0x100185a4, 2132 B
    uint32_t mShift[4]; // Mac +4
    uint32_t mSourceMask[4]; // +0x14
    uint32_t mDestMask[4]; // +0x24
    uint32_t mWidth[4]; // +0x34
    uint32_t mDestBytes, mSourceBytes, mAlphaFill, mWiden; // +0x44..0x50
};

// Untranslated converters from another file. Mac sizes 0x430 and 0x43c.
class CPalToMaskConverter : public CConverterBase {
public:
    CPalToMaskConverter();
    ~CPalToMaskConverter() override; // 0x1004889c
    void Init(const TbPalette&, const TbPixFormat&); // 0x10047740
    void ConvertLineUp(uint8_t*, uint8_t*, int32_t) override; // 0x1004786c
    void ConvertLineDown(uint8_t*, uint8_t*, int32_t) override; // 0x10047fd0
    void GetShiftBits(uint32_t source, uint32_t destination,
                      uint32_t& shiftRight, uint32_t& shiftLeft); // 0x1004874c

    uint32_t mSourceMask[3];
    uint32_t mShiftRight[3];
    uint32_t mShiftLeft[3];
    uint32_t mAlphaMask;
    uint32_t mDestinationBytes;
    uint8_t mPalette[0x400];
};
class CMaskToPalConverter : public CConverterBase {
public:
    CMaskToPalConverter(const TbPixFormat&, const TbPalette&); // 0x10048924
    ~CMaskToPalConverter() override;
    void Init(const TbPixFormat&, const TbPalette&); // 0x10048a6c
    void Release(); // 0x10048c98
    void ConvertLineUp(uint8_t*, uint8_t*, int32_t) override;
    void ConvertLineDown(uint8_t*, uint8_t*, int32_t) override;
    void GetShiftBits(uint32_t source, uint32_t destination,
                      uint32_t& shiftRight, uint32_t& shiftLeft); // 0x10049228
    uint8_t BruteFindColour(uint8_t red, uint8_t green, uint8_t blue); // 0x10049378

    uint8_t mPalette[0x400];
    const TbPalette* mPaletteSource;
    uint8_t* mInverseMap;
    uint32_t mBits;
    uint32_t mSide;
    uint32_t mSourceMask[3];
    uint32_t mShiftRight[3];
    uint32_t mShiftLeft[3];
    uint32_t mSourceBytes;
};

// Mac extent at least 0x58. Names follow Spencer W. Thomas's inv_cmap.c.
class CInverseMap {
public:
    CInverseMap(); // 0x1001707c, 12 B
    virtual ~CInverseMap(); // 0x100170b0, 76 B
    void InvCMap(int32_t first, int32_t count, const TbPalette&, int32_t bits,
                uint32_t* distBuf, uint8_t* rgbMap); // 0x10017124, 676 B
    int32_t redloop(); // 0x10017408, 420 B
    int32_t greenloop(int32_t restart); // 0x100175d8, 724 B
    int32_t blueloop(int32_t restart); // 0x100178dc, 476 B
    void maxfill(uint32_t* buffer, int32_t side); // 0x10017ae4, 100 B
    int32_t cindex, bcenter, gcenter, rcenter; // Mac +4..0x10
    int32_t gdist, rdist, cdist, cbinc, cginc, crinc; // +0x14..0x28
    uint32_t *gdp, *rdp, *cdp; // +0x2c..0x34
    uint8_t *grgbp, *rrgbp, *crgbp; // +0x38..0x40
    int32_t gstride, rstride, x, xsqr, colormax; // +0x44..0x54
};
class TbRAConv {
public:
    TbResult Move(TbRenderArea& destination, TbRenderArea& source, TbPalette* palette); // 0x1001901c, 652 B
    ~TbRAConv(); // 0x10018fb4, 68 B
};
extern CConverter gRaConverter; // 0x10073f1c; namespace initialization represents __sinit_TbRaConvert_cpp, 0x100192f4, 56 B
extern int32_t gRedloopRxx; // 0x10073f18; loop static names invented
extern int32_t gGreenloopHere, gGreenloopMin, gGreenloopMax, gGreenloopGinc; // 0x10073f14..0x10073f08
extern int32_t gGreenloopGxx, gGreenloopGcdist; // 0x10073f04, 0x10073f00
extern uint32_t* gGreenloopGcdp; // 0x10073efc
extern uint8_t* gGreenloopGcrgbp; // 0x10073ef8
extern int32_t gBlueloopHere, gBlueloopMin, gBlueloopMax, gBlueloopBinc; // 0x10073ef4..0x10073ee8
