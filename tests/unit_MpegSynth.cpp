// Test entry points for unit w71b, the second half of the MPEG audio decoder (libtpw_unit_MpegSynth.dylib, driven by
// tests/test_MpegSynth.py). The program under test is "sound symbols shared". The library links src/sound/MpegDecode.cpp (PolySynth,
// dct64, WRITE_SAMPLE, Decode_Layer2, II_step_two, II_step_twoMMX and the tables) with src/sound/FileAccess.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp (part 1: GetBits,
// the header code, Decode, Read ...) and has NO fakes: every callee is real on both sides, so the PCM that Read writes for a
// stream can be compared short by short with the original's. The object is loaded from a "Mac image" and written back as one
// exactly as in unit_MpegBase.cpp (same layout, same conventions: the pool link is not used here).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

#include "TbMpegBase.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr int kImageSize = 0x24f0;
constexpr int kOutSize = 0x4000;
constexpr int kStreamSize = 0x10000;
constexpr int kFracWords = 0x180;    // the fraction area the tests pass to PolySynth and II_step_two: float [3][2][32]
constexpr int kMulWords = 2048;      // the integer multiplier table of II_step_twoMMX

CMpegBase* gObject;
uint8_t gStream[kStreamSize + 64];
uint8_t gOut[kOutSize];
float gFrac[kFracWords];               // PolySynth's fractions and II_step_two's output
int16_t gShortFrac[kFracWords];        // II_step_twoMMX's output
int32_t gMul[kMulWords];
int8_t gBalloc[0x200];
int8_t gScale[0x300];
al_table gCustom[512];                 // a table of the test's own (II_step_two with "custom" tables)
float gDct[0x120 + 0x100 + 0x40];      // dct64's output areas: out0 (0x110 floats), out1 (0x100) and the input (32)

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
void put32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
void Swap16(uint8_t* p, int n) {
    for (int i = 0; i + 1 < n; i += 2) std::swap(p[i], p[i + 1]);
}
// The synthesis buffers are floats: the image holds them in the Mac's (big-endian) byte order.
void Swap32(uint8_t* p, int n) {
    for (int i = 0; i + 3 < n; i += 4) {
        std::swap(p[i], p[i + 3]);
        std::swap(p[i + 1], p[i + 2]);
    }
}

// The image into the object (everything but the vptr and the pool link).
void LoadImage(CMpegBase& o, const uint8_t* img) {
    std::memcpy(o.mSynthBuf, img, 0x1200);
    Swap32(reinterpret_cast<uint8_t*>(o.mSynthBuf), 0x1200);
    std::memcpy(o.mSynthSpare, img + 0x1200, 0x40);
    std::memcpy(o.mFrame, img + 0x1240, 0x1240);
    Swap16(reinterpret_cast<uint8_t*>(o.mFrame), 0x1240);
    o.mNextPooled = nullptr;
    o.mSampleRate = static_cast<int32_t>(be32(img + 0x2488));
    o.mSampleRate2 = static_cast<int32_t>(be32(img + 0x248c));
    o.mFrameBytes = static_cast<int32_t>(be32(img + 0x2490));
    o.mLayer = img[0x2494];
    o.mBitRate = static_cast<int32_t>(be32(img + 0x2498));
    o.mFrameSize = static_cast<int32_t>(be32(img + 0x249c));
    o.mFrameBytes2 = static_cast<int32_t>(be32(img + 0x24a0));
    o.mFrames = static_cast<int32_t>(be32(img + 0x24a4));
    uint32_t data = be32(img + 0x24a8);
    o.mData = data == 0xffffffffu ? nullptr : gStream + data;
    o.mBitPos = static_cast<int32_t>(be32(img + 0x24ac));
    o.mFrameStartBit = static_cast<int32_t>(be32(img + 0x24b0));
    o.mTotalBytes = static_cast<int32_t>(be32(img + 0x24b4));
    o.mPosition = static_cast<int32_t>(be32(img + 0x24b8));
    o.mSbLimit = static_cast<int32_t>(be32(img + 0x24bc));
    o.mOpen = img[0x24c0];
    o.mBo = img[0x24c1];
    o.mMpeg25 = img[0x24c2];
    o.mLsf = img[0x24c3];
    o.mSampleRateIndex2 = img[0x24c4];
    o.mUnused24c5 = img[0x24c5];
    o.mId = img[0x24c6];
    o.mNoCrc = img[0x24c7];
    o.mBitRateIndex = img[0x24c8];
    o.mSampleRateIndex = img[0x24c9];
    o.mPadding = img[0x24ca];
    o.mMode = img[0x24cb];
    o.mModeExt = img[0x24cc];
    o.mCopyright = img[0x24cd];
    o.mOriginal = img[0x24ce];
    o.mUnused24cf = img[0x24cf];
    o.mChannels = static_cast<int32_t>(be32(img + 0x24d0));
    o.mChannels32 = static_cast<int32_t>(be32(img + 0x24d4));
    o.mT1 = static_cast<int32_t>(be32(img + 0x24d8));
    o.mT2 = static_cast<int32_t>(be32(img + 0x24dc));
    o.mT3 = static_cast<int32_t>(be32(img + 0x24e0));
    o.mT4 = static_cast<int32_t>(be32(img + 0x24e4));
    o.mFramesDecoded = static_cast<int32_t>(be32(img + 0x24e8));
    o.mFrameConsumed = static_cast<int32_t>(be32(img + 0x24ec));
}

void SnapshotImage(const CMpegBase& o, uint8_t* img, int fill) {
    std::memset(img, fill, kImageSize);
    std::memcpy(img, o.mSynthBuf, 0x1200);
    Swap32(img, 0x1200);
    std::memcpy(img + 0x1200, o.mSynthSpare, 0x40);
    std::memcpy(img + 0x1240, o.mFrame, 0x1240);
    Swap16(img + 0x1240, 0x1240);
    put32(img + 0x2480, 1);   // the vptr: 1 stands for the base class
    put32(img + 0x2484, 0);
    put32(img + 0x2488, o.mSampleRate);
    put32(img + 0x248c, o.mSampleRate2);
    put32(img + 0x2490, o.mFrameBytes);
    img[0x2494] = o.mLayer;
    put32(img + 0x2498, o.mBitRate);
    put32(img + 0x249c, o.mFrameSize);
    put32(img + 0x24a0, o.mFrameBytes2);
    put32(img + 0x24a4, o.mFrames);
    put32(img + 0x24a8, o.mData == nullptr ? 0xffffffffu : static_cast<uint32_t>(o.mData - gStream));
    put32(img + 0x24ac, o.mBitPos);
    put32(img + 0x24b0, o.mFrameStartBit);
    put32(img + 0x24b4, o.mTotalBytes);
    put32(img + 0x24b8, o.mPosition);
    put32(img + 0x24bc, o.mSbLimit);
    img[0x24c0] = o.mOpen;
    img[0x24c1] = o.mBo;
    img[0x24c2] = o.mMpeg25;
    img[0x24c3] = o.mLsf;
    img[0x24c4] = o.mSampleRateIndex2;
    img[0x24c5] = o.mUnused24c5;
    img[0x24c6] = o.mId;
    img[0x24c7] = o.mNoCrc;
    img[0x24c8] = o.mBitRateIndex;
    img[0x24c9] = o.mSampleRateIndex;
    img[0x24ca] = o.mPadding;
    img[0x24cb] = o.mMode;
    img[0x24cc] = o.mModeExt;
    img[0x24cd] = o.mCopyright;
    img[0x24ce] = o.mOriginal;
    img[0x24cf] = o.mUnused24cf;
    put32(img + 0x24d0, o.mChannels);
    put32(img + 0x24d4, o.mChannels32);
    put32(img + 0x24d8, o.mT1);
    put32(img + 0x24dc, o.mT2);
    put32(img + 0x24e0, o.mT3);
    put32(img + 0x24e4, o.mT4);
    put32(img + 0x24e8, o.mFramesDecoded);
    put32(img + 0x24ec, o.mFrameConsumed);
}

// The alloc table an operation uses: 0..4 the Mac's, 5 the test's own (gCustom).
const al_table* Table(int64_t which) { return which == 5 ? gCustom : MpegAllocTable(static_cast<int32_t>(which)); }

}  // namespace

extern "C" {

// Starts a case: the object (a runtime block filled with one byte, then loaded), the buffers and scratch filled.
void tpw_ms_begin(int fill, int outFill, int scratchFill) {
    void* block = ::operator new(sizeof(CMpegBase));
    std::memset(block, fill, sizeof(CMpegBase));
    gObject = ::new (block) CMpegBase();
    std::memset(gStream, 0, sizeof(gStream));
    std::memset(gOut, outFill, sizeof(gOut));
    std::memset(gFrac, 0, sizeof(gFrac));
    std::memset(gShortFrac, 0, sizeof(gShortFrac));
    std::memset(gMul, 0, sizeof(gMul));
    std::memset(gBalloc, 0, sizeof(gBalloc));
    std::memset(gScale, 0, sizeof(gScale));
    std::memset(gCustom, 0, sizeof(gCustom));
    std::memset(gMpegL2Fraction, scratchFill, sizeof(gMpegL2Fraction));
    std::memset(gMpegL2Scale, scratchFill, sizeof(gMpegL2Scale));
    std::memset(gMpegL2Alloc, scratchFill, sizeof(gMpegL2Alloc));
    std::memset(gMpegL2Select, scratchFill, sizeof(gMpegL2Select));
    std::memset(gMpegDctBuf, scratchFill, sizeof(gMpegDctBuf));
    std::memset(gDct, 0, sizeof(gDct));
    using_MMX = 0;
}

void tpw_ms_end() {
    if (gObject != nullptr) {
        gObject->~CMpegBase();
        ::operator delete(gObject);
    }
    gObject = nullptr;
}

void tpw_ms_set_stream(const uint8_t* data, int size) {
    if (size > kStreamSize) size = kStreamSize;
    std::memcpy(gStream, data, size);
}
void tpw_ms_load(const uint8_t* image) { LoadImage(*gObject, image); }
void tpw_ms_snapshot(uint8_t* image, int fill) { SnapshotImage(*gObject, image, fill); }
void tpw_ms_set_out(const uint8_t* data) { std::memcpy(gOut, data, kOutSize); }
void tpw_ms_get_out(uint8_t* data) {
    std::memcpy(data, gOut, kOutSize);
    Swap16(data, kOutSize);   // shorts in the Mac's byte order
}

// The test's own areas: words (floats as raw bits, native order here).
void tpw_ms_set_frac(const uint32_t* words) { std::memcpy(gFrac, words, sizeof(gFrac)); }
void tpw_ms_get_frac(uint32_t* words) { std::memcpy(words, gFrac, sizeof(gFrac)); }
void tpw_ms_set_short_frac(const uint16_t* values) { std::memcpy(gShortFrac, values, sizeof(gShortFrac)); }
void tpw_ms_get_short_frac(uint16_t* values) { std::memcpy(values, gShortFrac, sizeof(gShortFrac)); }
void tpw_ms_set_mul(const int32_t* values) { std::memcpy(gMul, values, sizeof(gMul)); }
void tpw_ms_set_balloc(const int8_t* values) { std::memcpy(gBalloc, values, sizeof(gBalloc)); }
void tpw_ms_set_scale(const int8_t* values) { std::memcpy(gScale, values, sizeof(gScale)); }
// A table of the test's own: pairs of shorts {bits, d}.
void tpw_ms_set_custom(const int16_t* pairs, int entries) {
    for (int i = 0; i < entries && i < 512; ++i) {
        gCustom[i].bits = pairs[2 * i];
        gCustom[i].d = pairs[2 * i + 1];
    }
}
void tpw_ms_set_dct(const uint32_t* words) { std::memcpy(gDct, words, sizeof(gDct)); }
void tpw_ms_get_dct(uint32_t* words) { std::memcpy(words, gDct, sizeof(gDct)); }
void tpw_ms_set_dctbuf(const uint32_t* words) { std::memcpy(gMpegDctBuf, words, sizeof(gMpegDctBuf)); }

// Decode_Layer2's scratch and dct64's work array as bytes in memory order, the Mac's layout (0x1003e688): the fraction floats
// (0x300 bytes), the scale factors (0xc0), the allocations (0x40), the selectors (0x40) and the dct buffer floats (0x100).
void tpw_ms_scratch(uint8_t* out) {
    std::memcpy(out, gMpegL2Fraction, 0x300);
    std::memcpy(out + 0x300, gMpegL2Scale, 0xc0);
    std::memcpy(out + 0x3c0, gMpegL2Alloc, 0x40);
    std::memcpy(out + 0x400, gMpegL2Select, 0x40);
    std::memcpy(out + 0x440, gMpegDctBuf, 0x100);
}

// Table checks.
uint32_t tpw_ms_decwin_bits(int index) {
    uint32_t bits;
    std::memcpy(&bits, MpegDecwin() + index, 4);
    return bits;
}
uint32_t tpw_ms_cosine_bits(int index) {
    float v = MpegDct64Cosine(index);
    uint32_t bits;
    std::memcpy(&bits, &v, 4);
    return bits;
}
int32_t tpw_ms_alloc_entries(int table) { return MpegAllocTableEntries(table); }
int32_t tpw_ms_alloc_word(int table, int entry) {
    const al_table& e = MpegAllocTable(table)[entry];
    return (static_cast<int32_t>(e.bits) << 16) | static_cast<uint16_t>(e.d);
}
int32_t tpw_ms_group_byte(int d, int index) { return MpegGroupTable(d)[index]; }
int32_t tpw_ms_translate(int index) { return MpegLayer2Translate()[index]; }
int32_t tpw_ms_sblimit(int index) { return MpegLayer2SbLimit(index); }

enum Op {
    OP_POLYSYNTH, OP_LAYER2, OP_STEP2, OP_STEP2MMX, OP_OPEN_PTR, OP_READ, OP_DECODE, OP_DCT64, OP_WRITE_SAMPLE, OP_OPEN_SYNTH
};

// One operation. a, b, c, d are the operation's integers (see the Python side); a float for WRITE_SAMPLE is passed as bits.
int64_t tpw_ms_op(int op, int64_t a, int64_t b, int64_t c, int64_t d) {
    CMpegBase& o = *gObject;
    switch (op) {
    case OP_POLYSYNTH:
        // a: the output offset in bytes (-1 NULL), b: the fraction block (floats 64 apart in gFrac)
        o.PolySynth(a < 0 ? nullptr : reinterpret_cast<int16_t*>(gOut + a), gFrac + 64 * b);
        return 0;
    case OP_LAYER2:
        return o.Decode_Layer2(a < 0 ? nullptr : reinterpret_cast<int16_t*>(gOut + a));
    case OP_STEP2:
        // a: the table (0..4, 5 the custom one), b: x1, c: the offset in entries into the table
        o.II_step_two(reinterpret_cast<char*>(gBalloc), reinterpret_cast<float(*)[2][32]>(gFrac), reinterpret_cast<char*>(gScale),
                      static_cast<int32_t>(b), Table(a) + c);
        return 0;
    case OP_STEP2MMX:
        o.II_step_twoMMX(gMul, reinterpret_cast<char*>(gBalloc), reinterpret_cast<int16_t(*)[2][32]>(gShortFrac),
                         reinterpret_cast<char*>(gScale), Table(a) + c);
        return 0;
    case OP_OPEN_PTR:
        return o.Open(a < 0 ? nullptr : static_cast<void*>(gStream + a), static_cast<int32_t>(b));
    case OP_READ:
        return o.Read(a < 0 ? nullptr : gOut + a, static_cast<int32_t>(b));
    case OP_DECODE:
        return o.Decode(a < 0 ? nullptr : reinterpret_cast<int16_t*>(gOut + a));
    case OP_DCT64:
        // gDct: out0 at floats 0, out1 at 0x120, the 32 samples at 0x220
        dct64(gDct, gDct + 0x120, gDct + 0x220);
        return 0;
    case OP_WRITE_SAMPLE: {
        float value;
        uint32_t bits = static_cast<uint32_t>(a);
        std::memcpy(&value, &bits, 4);
        WRITE_SAMPLE(reinterpret_cast<int16_t*>(gOut + b), value);
        return 0;
    }
    case OP_OPEN_SYNTH:
        o.OpenSynth();
        return 0;
    default:
        return -12345;
    }
}

}  // extern "C"
