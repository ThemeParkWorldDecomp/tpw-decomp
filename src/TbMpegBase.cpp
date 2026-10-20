// The sound library's MPEG audio decoder object, part 1 (unit w71a): CMpegBase's pool, bit reader, header parser, frame
// reader and layer I decoder (program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp). See include/TbMpegBase.h
// for the layout. PolySynth, dct64, Decode_Layer2 and II_step_two (unit w71b) are declared there and defined elsewhere.
#include "TbMpegBase.h"

#include <cmath>
#include <cstring>
#include <new>

#include "ppc_math.h"

int32_t using_MMX = 0;   // 0x1003e380

CMpegBase* CMpegBase::mFirstCMpegBase = nullptr;
int32_t CMpegBase::mCurrentCMpegBase = 0;
int32_t CMpegBase::mLowCMpegBase = 0;
int32_t CMpegBase::mHighCMpegBase = 0;
int32_t CMpegBase::mTotalCMpegBase = 0;
int32_t CMpegBase::mLevelCMpegBase = 10;

// Decode_Layer1's scratch: file-level arrays on the Mac too (the bit allocations at 0x1003e588, the scale factor indices at
// 0x1003e488 and the samples at 0x1003e388; 0x100 bytes each, two channels of 32 bands). Layer II keeps its own at 0x1003e688.
uint32_t gMpegLayer1Alloc[64];
int32_t gMpegLayer1Scale[64];
int32_t gMpegLayer1Sample[64];

namespace {

// divw: the quotient truncated towards zero; the dividend where the processor leaves it undefined (a zero divisor, the
// most negative number over -1), as the emulator answers.
inline int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// mullw: the low 32 bits of the product.
inline int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

inline int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

// mulhw, srawi 5, then the sign bit added: the compiler's division by 100, computed as the listing does.
inline int32_t DivBy100(int32_t x) {
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x51EB851F) * x) >> 32) >> 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

// lwz and lwzx on the big-endian stream, at any alignment.
inline uint32_t LoadBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

// 0x10035ca8: the sample rates by index (the index goes to 9: MPEG 2.5 with both rate bits set reads the word that follows the
// table, which is the first two halfwords of the bit rate table, 0 and 32; ProcessHeader cannot reach it, see there).
const int32_t kSampleRates[10] = {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000, 32};

// 0x10035ccc: the bit rates in kbit/s, [lsf][layer - 1][index] (the classic tabsel_123).
const int16_t kBitRates[2][3][16] = {
    {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
     {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
     {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}},
    {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}}};

// The words that follow the table (0x10035d8c): _mLevelCMpegBase, then 0, 1, 3, 7, 15, 31, 63. A header with the reserved
// layer bits (layer 4) and lsf set reads its bit rate from them, a halfword at a time.
const int16_t kAfterBitRates[8] = {0, 0, 1, 3, 7, 15, 31, 63};   // the low halfwords of words 1 to 7 (word 0 is the level)

// The Mac's lha of 0x10035cac + lsf * 0x60 + layer * 0x20 + index * 2: layer runs 1 to 4, so layer 4 of an lsf stream runs
// past the table into the words above, and layer 4 of a stream that is not lsf reads the lsf rows.
int32_t BitRateOf(unsigned lsf, unsigned layer, unsigned index) {
    unsigned flat = lsf * 48 + (layer - 1) * 16 + index;
    if (flat < 96) return kBitRates[flat / 48][(flat % 48) / 16][flat % 16];
    flat -= 96;
    unsigned word = flat / 2;
    bool low = (flat & 1) != 0;
    if (word == 0) {
        uint32_t level = static_cast<uint32_t>(CMpegBase::mLevelCMpegBase);
        return static_cast<int16_t>(low ? level & 0xffff : level >> 16);
    }
    return low ? kAfterBitRates[word] : 0;
}

// The multipliers' table: muls[27][64] of mpglib, computed the way mpglib's make_decode_tables does (the unit's test compares
// every word with the Mac's table, 0x10035df0).
struct MulsTable {
    float value[27][64];
    MulsTable() {
        static const double kMulMul[27] = {0.0, -2.0 / 3.0, 2.0 / 3.0, 2.0 / 7.0, 2.0 / 15.0, 2.0 / 31.0, 2.0 / 63.0, 2.0 / 127.0,
                                           2.0 / 255.0, 2.0 / 511.0, 2.0 / 1023.0, 2.0 / 2047.0, 2.0 / 4095.0, 2.0 / 8191.0,
                                           2.0 / 16383.0, 2.0 / 32767.0, 2.0 / 65535.0, -4.0 / 5.0, -2.0 / 5.0, 2.0 / 5.0,
                                           4.0 / 5.0, -8.0 / 9.0, -4.0 / 9.0, -2.0 / 9.0, 2.0 / 9.0, 4.0 / 9.0, 8.0 / 9.0};
        for (int k = 0; k < 27; ++k) {
            double m = kMulMul[k];
            int j = 3;
            for (int i = 0; i < 63; ++i, --j) value[k][i] = static_cast<float>(m * std::pow(2.0, static_cast<double>(j) / 3.0));
            value[k][63] = 0.0f;
        }
    }
};

const MulsTable& Muls() {
    static const MulsTable table;
    return table;
}

}  // namespace

int32_t MpegIntegerAllocation(int32_t index) {
    if (index < 0 || index > 31) return 0;   // the Mac reads other data there; no caller does
    return static_cast<int32_t>(1u - (1u << index));
}

float MpegMultiplier(int32_t row, int32_t scale) { return Muls().value[row][scale]; }

int32_t muldiv(int32_t a, int32_t b, int32_t c) { return Div(Mul(a, b), c); }

// ---------------------------------------------------------------- the pool

CMpegBase::CMpegBase() { mOpen = 0; }

CMpegBase::~CMpegBase() {
    if (mOpen != 0) Close();
}

void* CMpegBase::operator new(std::size_t size) {
    if (size == 0) size = 1;
    if (size != sizeof(CMpegBase)) return ::operator new(size);
    if (mFirstCMpegBase == nullptr) return ::operator new(size);
    --mCurrentCMpegBase;
    if (mCurrentCMpegBase < mLowCMpegBase) mLowCMpegBase = mCurrentCMpegBase;
    ++mTotalCMpegBase;
    CMpegBase* object = mFirstCMpegBase;
    mFirstCMpegBase = object->mNextPooled;
    return object;
}

void CMpegBase::operator delete(void* object, std::size_t size) {
    if (object == nullptr) return;
    if (size != sizeof(CMpegBase)) {
        ::operator delete(object);
        return;
    }
    CMpegBase* pooled = static_cast<CMpegBase*>(object);
    pooled->mNextPooled = mFirstCMpegBase;
    mFirstCMpegBase = pooled;
    ++mCurrentCMpegBase;
    if (mHighCMpegBase < mCurrentCMpegBase) mHighCMpegBase = mCurrentCMpegBase;
}

void CMpegBase::TopUp() {
    int32_t count = 0;
    for (CMpegBase* object = mFirstCMpegBase; object != nullptr; object = object->mNextPooled) ++count;
    for (; count < mLevelCMpegBase; ++count) {
        CMpegBase* object = static_cast<CMpegBase*>(::operator new(sizeof(CMpegBase)));
        object->mNextPooled = mFirstCMpegBase;
        ++mCurrentCMpegBase;
        mFirstCMpegBase = object;
    }
    if (mHighCMpegBase < mCurrentCMpegBase) mHighCMpegBase = mCurrentCMpegBase;
    if (mLowCMpegBase == 0) mLowCMpegBase = mCurrentCMpegBase;
}

void CMpegBase::SetLevel(int32_t level) { mLevelCMpegBase = level; }

void CMpegBase::ClearCache() {
    while (mFirstCMpegBase != nullptr) {
        CMpegBase* object = mFirstCMpegBase;
        mFirstCMpegBase = object->mNextPooled;
        ::operator delete(object);
    }
    mCurrentCMpegBase = 0;
    mTotalCMpegBase = 0;
}

_CS_CMpegBase::~_CS_CMpegBase() { CMpegBase::ClearCache(); }

CMpegBase_DecodeMethod::CMpegBase_DecodeMethod() { using_MMX = 0; }

CMpegBase_DecodeMethod::~CMpegBase_DecodeMethod() {
    if (using_MMX != 0) CMpegBase::Close_Tables();
}

// The two static objects the Mac's initialiser and finaliser run.
namespace {
_CS_CMpegBase sCleanup;
CMpegBase_DecodeMethod sDecodeMethod;
}  // namespace

// ---------------------------------------------------------------- synthesis and layer set-up

void CMpegBase::OpenSynth() { std::memset(mSynthBuf, 0, using_MMX == 0 ? 0x1200 : 0x900); }

void CMpegBase::CloseSynth() {}

int32_t CMpegBase::OpenLayer() {
    mT1 = 1;
    int32_t product = Mul(mBitRate, 0x23280);
    mT2 = Div(product, mSampleRate);
    mT3 = DivBy100(product - Mul(mT2, mSampleRate));
    mT4 = DivBy100(mSampleRate);
    mFrameBytes = mLayer == 1 ? Mul(mChannels, 0x300) : Mul(mChannels, 0x900);
    mBo = 1;
    mFrameBytes2 = mFrameBytes;
    return 0;
}

int32_t CMpegBase::CloseLayer() { return 0; }

void CMpegBase::Init_Tables() { Close_Tables(); }

void CMpegBase::Close_Tables() {}

// ---------------------------------------------------------------- the bit reader

void CMpegBase::SeekBits(int32_t byte) { mBitPos = static_cast<int32_t>(static_cast<uint32_t>(byte) << 3); }

// The big-endian word at the byte the position is in, shifted down to the bits wanted; the shifts are the processor's (slw and
// srw: the low six bits of the amount, 32 and more give 0).
uint32_t CMpegBase::GetBits(int32_t count) {
    int32_t position = mBitPos;
    uint32_t word = LoadBE32(mData + (position >> 3));
    mBitPos = position + count;
    return (ppc_slw(1, count) - 1u) & ppc_srw(word, (32 - count) - (position & 7));
}

// ---------------------------------------------------------------- the header

int32_t CMpegBase::ProcessHeader(uint32_t header) {
    if ((header & 0xfff00000) != 0xfff00000) return -1;
    mId = (header >> 19) & 1;
    mLayer = static_cast<uint8_t>(4 - ((header >> 17) & 3));
    mNoCrc = (header >> 16) & 1;
    mBitRateIndex = (header >> 12) & 0xf;
    mPadding = (header >> 9) & 1;
    mMode = (header >> 6) & 3;
    mModeExt = (header >> 4) & 3;
    mCopyright = (header >> 3) & 1;
    mOriginal = (header >> 2) & 1;
    // (The sync test above already needs bit 20, so this first branch and the MPEG 2.5 rate index below never run; they are
    // kept as the listing has them, and mSampleRateIndex2 is always written.)
    if ((header & 0x100000) == 0) {
        mLsf = 1;
        mMpeg25 = 1;
    } else {
        mLsf = mId == 0;
        mMpeg25 = 0;
    }
    if (mMpeg25 == 0) {
        uint8_t rate = (header >> 10) & 3;
        mSampleRateIndex = static_cast<uint8_t>(rate + mLsf * 3);
        mSampleRateIndex2 = static_cast<uint8_t>(rate + (mLsf == 0 ? 3 : 0));
    } else {
        mSampleRateIndex = static_cast<uint8_t>(((header >> 10) & 3) + 6);
    }
    mChannels = mMode == 3 ? 1 : 2;
    mSampleRate2 = mSampleRate = kSampleRates[mSampleRateIndex];
    mChannels32 = mChannels << 5;
    if (mBitRateIndex == 0) return -1;
    mBitRate = BitRateOf(mLsf, mLayer, mBitRateIndex);
    if (mLayer == 1) {
        mFrameSize = Div(Mul(mBitRate, 12000), mSampleRate);
        mFrameSize = Add(Mul(Add(mFrameSize, mPadding), 4), -4);
    } else {
        mFrameSize = Div(Mul(mBitRate, 0x23280), mSampleRate);
        if (mPadding != 0) mFrameSize = Add(mFrameSize, 1);
        mFrameSize = Add(mFrameSize, -4);
    }
    return 0;
}

int32_t CMpegBase::DecodeHeader() {
    mBitPos = mFrameStartBit;
    if (ProcessHeader(LoadBE32(mData + (mBitPos >> 3))) == -1) return -1;
    mBitPos = Add(mBitPos, 0x20);
    mFrameStartBit = Add(mBitPos, Mul(mFrameSize, 8));
    return 0;
}

int32_t CMpegBase::GetHeader() { return ProcessHeader(LoadBE32(mData)) == -1; }

int32_t CMpegBase::GetHeader(uint32_t header) { return ProcessHeader(header) == -1; }

// ---------------------------------------------------------------- open, close, reset

int32_t CMpegBase::Open(void* data, int32_t size) {
    if (mOpen != 0) Close();
    mOpen = 1;
    mData = static_cast<const uint8_t*>(data);
    if (GetHeader() != 0) return -1;
    OpenSynth();
    OpenLayer();
    Reset();
    mFrames = muldiv(Add(size, mT1), mT4, Mul(mT1, Add(mT3, Mul(mT2, mT4))));
    mTotalBytes = Mul(mFrames, mFrameBytes);
    return 0;
}

int32_t CMpegBase::Open(uint32_t header, int32_t size) {
    if (mOpen != 0) Close();
    mOpen = 1;
    mData = nullptr;
    if (GetHeader(header) != 0) return -1;
    OpenSynth();
    OpenLayer();
    Reset();
    mFrames = muldiv(Add(size, mT1), mT4, Mul(mT1, Add(mT3, Mul(mT2, mT4))));
    mTotalBytes = Mul(mFrames, mFrameBytes);
    return 0;
}

int32_t CMpegBase::Close() {
    if (mOpen != 0) {
        mOpen = 0;
        CloseLayer();
        CloseSynth();
    }
    return 0;
}

void CMpegBase::Reset() {
    mFrameConsumed = mFrameBytes2;
    mFrameStartBit = 0;
    mBitPos = 0;
    mFramesDecoded = 0;
    mPosition = 0;
    SeekBits(0);
}

// ---------------------------------------------------------------- decoding

int32_t CMpegBase::Decode(int16_t* out) {
    if (DecodeHeader() != 0) return -1;
    if (mLayer == 2) return Decode_Layer2(out);
    if (mLayer == 1) return Decode_Layer1(out);
    return 0;
}

// Whole frames go straight into the caller's buffer when the buffered frame is used up and one fits; otherwise a frame is
// decoded into mFrame and handed out in pieces. A failing Decode ends the loop.
int32_t CMpegBase::Read(void* out, int32_t bytes) {
    int32_t written = 0;
    uint8_t* destination = static_cast<uint8_t*>(out);
    while (written < bytes) {
        if (mFrameConsumed == mFrameBytes2 && bytes - written >= mFrameBytes2) {
            if (Decode(reinterpret_cast<int16_t*>(destination + written)) != 0) break;
            ++mFramesDecoded;
            written = Add(written, mFrameBytes2);
        } else {
            if (mFrameConsumed >= mFrameBytes2) {
                if (Decode(mFrame) != 0) break;
                mFrameConsumed = 0;
                ++mFramesDecoded;
            }
            int32_t count = mFrameBytes2 - mFrameConsumed;
            if (bytes - written < count) count = bytes - written;
            std::memcpy(destination + written, reinterpret_cast<uint8_t*>(mFrame) + mFrameConsumed, static_cast<std::size_t>(count));
            written = Add(written, count);
            mFrameConsumed = Add(mFrameConsumed, count);
        }
    }
    mPosition = Add(mFrameConsumed, Mul(mFramesDecoded - 1, mFrameBytes));
    return written;
}

// Layer I: the allocation (4 bits a band, one byte for the pair of channels when stereo), the scale factor indices (6 bits
// where something is allocated), then twelve granules of one sample per allocated band, converted with the multiplier table
// and handed to PolySynth with the 32 (mono) or 64 (stereo) fractions; the output moves on by mChannels32 shorts a granule.
int32_t CMpegBase::Decode_Layer1(int16_t* out) {
    float fraction[2][32];
    if (mChannels == 2) {
        for (int band = 0; band < 0x20; ++band) {
            uint32_t pair = GetBits(8);
            gMpegLayer1Alloc[band * 2] = static_cast<uint32_t>(static_cast<int32_t>(pair) >> 4);
            gMpegLayer1Alloc[band * 2 + 1] = pair & 0xf;
        }
        int32_t* scale = gMpegLayer1Scale;
        for (int band = 0; band < 0x20; ++band) {
            if (gMpegLayer1Alloc[band * 2] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
            if (gMpegLayer1Alloc[band * 2 + 1] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
        }
    } else {
        for (int band = 0; band < 0x20; ++band) gMpegLayer1Alloc[band] = GetBits(4);
        int32_t* scale = gMpegLayer1Scale;
        for (int band = 0; band < 0x20; ++band)
            if (gMpegLayer1Alloc[band] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
    }
    for (int granule = 0; granule < 12; ++granule) {
        if (mChannels == 2) {
            int32_t* sample = gMpegLayer1Sample;
            for (int band = 0; band < 0x20; ++band) {
                if (gMpegLayer1Alloc[band * 2] != 0) *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band * 2] + 1));
                if (gMpegLayer1Alloc[band * 2 + 1] != 0)
                    *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band * 2 + 1] + 1));
            }
            sample = gMpegLayer1Sample;
            const int32_t* scale = gMpegLayer1Scale;
            for (int band = 0; band < 0x20; ++band) {
                for (int channel = 0; channel < 2; ++channel) {
                    uint32_t allocation = gMpegLayer1Alloc[band * 2 + channel];
                    if (allocation == 0) {
                        fraction[channel][band] = 0.0f;
                    } else {
                        int32_t value = Add(Add(static_cast<int32_t>(ppc_slw(0xffffffffu, static_cast<int32_t>(allocation))), *sample++), 1);
                        fraction[channel][band] = static_cast<float>(value) * MpegMultiplier(static_cast<int32_t>(allocation) + 1, *scale++);
                    }
                }
            }
        } else {
            int32_t* sample = gMpegLayer1Sample;
            for (int band = 0; band < 0x20; ++band)
                if (gMpegLayer1Alloc[band] != 0) *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band] + 1));
            sample = gMpegLayer1Sample;
            const int32_t* scale = gMpegLayer1Scale;
            for (int band = 0; band < 0x20; ++band) {
                uint32_t allocation = gMpegLayer1Alloc[band];
                if (allocation == 0) {
                    fraction[0][band] = 0.0f;
                } else {
                    int32_t value = Add(Add(static_cast<int32_t>(ppc_slw(0xffffffffu, static_cast<int32_t>(allocation))), *sample++), 1);
                    fraction[0][band] = static_cast<float>(value) * MpegMultiplier(static_cast<int32_t>(allocation) + 1, *scale++);
                }
            }
        }
        PolySynth(out, &fraction[0][0]);
        out += mChannels32;
    }
    return 0;
}

// One band of layer II's integer path: the sample of each channel is its bits plus the allocation's offset, times the
// multiplier of (allocation + 1, scale factor), shifted down 12 and stored as a short (channel 0 at band, channel 1 at band
// + 32). An unallocated channel stores 0 and reads no bits.
void CMpegBase::get_sb_stereo(int32_t band, char (*alloc)[2], uint32_t (*scale)[32], int16_t* out, int32_t* mulstab) {
    int16_t result = 0;
    int32_t allocation = static_cast<int8_t>(alloc[band][0]);
    if (allocation != 0) {
        int32_t bits = static_cast<int32_t>(GetBits(allocation + 1));
        int32_t multiplier = mulstab[(allocation + 1) * 0x40 + static_cast<int32_t>(scale[0][band])];
        result = static_cast<int16_t>(Mul(Add(MpegIntegerAllocation(allocation), bits), multiplier) >> 12);
    }
    out[band] = result;
    result = 0;
    allocation = static_cast<int8_t>(alloc[band][1]);
    if (allocation != 0) {
        int32_t bits = static_cast<int32_t>(GetBits(allocation + 1));
        int32_t multiplier = mulstab[static_cast<int32_t>(scale[1][band]) + (allocation + 1) * 0x40];
        result = static_cast<int16_t>(Mul(multiplier, Add(MpegIntegerAllocation(allocation), bits)) >> 12);
    }
    out[band + 0x20] = result;
}
