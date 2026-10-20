// Fakes and test entry points for unit w71a, the MPEG audio decoder object CMpegBase, part 1 (libtpw_unit_MpegBase.dylib,
// driven by tests/test_MpegBase.py; see calllog.h). The program under test is "sound symbols shared":
//   src/TbMpegBase.cpp  the pool (new, delete, TopUp, SetLevel, ClearCache), the constructor and destructor, OpenSynth, OpenLayer,
//                       Open (two forms), Close, Reset, GetBits, SeekBits, ProcessHeader, DecodeHeader, GetHeader (two forms),
//                       Decode, Read, Decode_Layer1, get_sb_stereo, muldiv and the two static objects
// Real: all of those. Fakes (each records what the original's callee would be given; the PowerPC side records the same):
//   CMpegBase::PolySynth (unit w71b): records the output offset, the channel count, the bit position and the fractions (8 a
//       line), and writes a deterministic function of them into the output so that Read has samples to move
//   CMpegBase::Decode_Layer2 (unit w71b): records the output offset, bit position and frame size, answers the queue "L2" and
//       writes a deterministic pattern
//   the runtime heap (operator new and delete: the observer below records "new" with the size, labelled blocks record
//       "delete" through calllog.cpp), and, in the fake-derived mode, the three virtuals a derived class overrides (SeekBits,
//       DecodeHeader, GetHeader: recorded, answering the queues "DH" and "GH").
//
// The object is a block from the runtime heap, filled with one byte, then loaded from a "Mac image": the 0x24f0 bytes of the
// Mac's object in the Mac's byte order (the synthesis buffers and the shorts of the frame as bytes, the fields as big-endian
// words). tpw_mp_snapshot makes the same image from the host object (the pool link as a label, the stream pointer as an
// offset in the stream buffer or 0xffffffff, the vptr as 1 for the base class and 2 for the fake-derived one).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

#include "TbMpegBase.h"
#include "calllog.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {
// The allocation observer of calllog.cpp (included below): a block the code asked the runtime for is a "new" event of its size
// (the host object's size is reported as the Mac's 0x24f0) and gets the next label, so that the "delete" of the original's
// __dl__FPv matches the host's operator delete.
void MpegAllocated(std::size_t size, bool, void* block) {
    if (!on()) return;
    ev("new", {static_cast<int64_t>(size == sizeof(CMpegBase) ? 0x24f0 : size)});
    newlab(block);
}
}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) MpegAllocated(size, array, p)
#include "calllog.cpp"

namespace {

constexpr int kImageSize = 0x24f0;
constexpr int kOutSize = 0x4000;
constexpr int kStreamSize = 0x10000;

// A derived class that overrides the three virtuals, as CMpegMacFilePlayerUnpooled does.
class FakeDerived : public CMpegBase {
public:
    void SeekBits(int32_t byte) override { ev("SeekBits", {lab(this), byte}); }
    int32_t DecodeHeader() override {
        ev("DecodeHeader", {lab(this)});
        return static_cast<int32_t>(ret("DH", 0));
    }
    int32_t GetHeader() override {
        ev("GetHeader", {lab(this)});
        return static_cast<int32_t>(ret("GH", 0));
    }
};

CMpegBase* gObject;                  // the object under test (a runtime heap block)
bool gDerived;                       // it is a FakeDerived
bool gObjectGone;                    // it was deleted (it belongs to the pool)
uint8_t gStream[kStreamSize + 64];   // the bit stream
uint8_t gOut[kOutSize];              // the output buffer
void* gBlocks[16];                   // the blocks the pool operations took
void* gStatics[4];                   // the _CS_CMpegBase and CMpegBase_DecodeMethod objects
int32_t gPolyCalls;                  // PolySynth calls and Decode_Layer2 calls in this case (the samples depend on them)
int32_t gL2Calls;
int8_t gSbAlloc[64];
uint32_t gSbScale[64];
int32_t gSbMul[32 * 64];

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
void put32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

// Where an output pointer points, the same way on both sides: an offset in shorts in the output buffer, 0x10000 plus an offset
// in the object's frame, or -1.
int64_t Where(const int16_t* p) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
    if (b >= gOut && b < gOut + kOutSize) return (b - gOut) / 2;
    if (gObject != nullptr && p >= gObject->mFrame && p < gObject->mFrame + 0x920) return 0x10000 + (p - gObject->mFrame);
    return -1;
}

// The Mac's object size 0x24f0 stands for the host object's size in the sizes the pool operations are given.
std::size_t PoolSize(int64_t size) {
    return size == 0x24f0 ? sizeof(CMpegBase) : static_cast<std::size_t>(static_cast<uint32_t>(size));
}

void Put16(int16_t* p, uint32_t v) {
    if (Where(p) >= 0) *p = static_cast<int16_t>(v & 0xffff);
}

}  // namespace

// ---- the callees outside the unit (unit w71b)

// 64 or 32 fractions in, 32 samples a channel out. The Mac's PolySynth writes into the output from `out` a granule's samples.
void CMpegBase::PolySynth(int16_t* out, float* fraction) {
    int nch = mChannels == 2 ? 2 : 1;
    ev("PolySynth", {lab(this), Where(out), mChannels, mBitPos, mChannels32});
    for (int i = 0; i < 32 * nch; i += 8) {
        ev("Fraction", {i, fbits(fraction[i]), fbits(fraction[i + 1]), fbits(fraction[i + 2]), fbits(fraction[i + 3]),
                        fbits(fraction[i + 4]), fbits(fraction[i + 5]), fbits(fraction[i + 6]), fbits(fraction[i + 7])});
    }
    int32_t call = gPolyCalls++;
    if (out != nullptr)
        for (int i = 0; i < 32 * nch; ++i) Put16(out + i, ((fbits(fraction[i]) >> 7) ^ (i * 0x9e3 + call * 0x35)));
}

int32_t CMpegBase::Decode_Layer2(int16_t* out) {
    ev("Decode_Layer2", {lab(this), Where(out), mBitPos, mFrameBytes});
    int32_t call = gL2Calls++;
    int32_t shorts = mFrameBytes < mFrameBytes2 ? mFrameBytes : mFrameBytes2;
    if (shorts > 0x1200) shorts = 0x1200;
    shorts /= 2;
    if (out != nullptr)
        for (int i = 0; i < shorts; ++i) Put16(out + i, i * 0x57 + call * 0x1b3 + (mBitPos & 0xffff));
    return static_cast<int32_t>(ret("L2", 0));
}

// ---- the entry points
namespace {

void Swap16(uint8_t* p, int n) {
    for (int i = 0; i + 1 < n; i += 2) std::swap(p[i], p[i + 1]);
}

// The image into the object (everything but the pool link).
void LoadImage(CMpegBase& o, const uint8_t* img) {
    std::memcpy(o.mSynthBuf, img, 0x1200);
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

void SnapshotImage(const CMpegBase& o, uint8_t* img, int fill, int vptr) {
    std::memset(img, fill, kImageSize);
    std::memcpy(img, o.mSynthBuf, 0x1200);
    std::memcpy(img + 0x1200, o.mSynthSpare, 0x40);
    std::memcpy(img + 0x1240, o.mFrame, 0x1240);
    Swap16(img + 0x1240, 0x1240);
    put32(img + 0x2480, vptr);
    put32(img + 0x2484, lab(o.mNextPooled));
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

}  // namespace

extern "C" {

// Starts a case: the object (mode 1 is the fake-derived class) filled with one byte, the buffers cleared, the counters reset.
void tpw_mp_begin(int mode, int fill, int outFill) {
    gDerived = mode == 1;
    gObjectGone = false;
    void* block = ::operator new(sizeof(CMpegBase));
    std::memset(block, fill, sizeof(CMpegBase));
    if (gDerived) gObject = ::new (block) FakeDerived();
    else gObject = ::new (block) CMpegBase();
    std::memset(reinterpret_cast<uint8_t*>(block) + sizeof(void*), fill, sizeof(CMpegBase) - sizeof(void*));
    gObject->mNextPooled = nullptr;
    std::memset(gStream, 0, sizeof(gStream));
    std::memset(gOut, outFill, sizeof(gOut));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gStatics, 0, sizeof(gStatics));
    std::memset(gMpegLayer1Alloc, 0, sizeof(gMpegLayer1Alloc));
    std::memset(gMpegLayer1Scale, 0, sizeof(gMpegLayer1Scale));
    std::memset(gMpegLayer1Sample, 0, sizeof(gMpegLayer1Sample));
    gPolyCalls = 0;
    gL2Calls = 0;
    using_MMX = 0;
    CMpegBase::mFirstCMpegBase = nullptr;
    CMpegBase::mCurrentCMpegBase = 0;
    CMpegBase::mLowCMpegBase = 0;
    CMpegBase::mHighCMpegBase = 0;
    CMpegBase::mTotalCMpegBase = 0;
    CMpegBase::mLevelCMpegBase = 10;
}

// Ends it: the pool is cleared and everything handed out is released (the log is off by now).
void tpw_mp_end() {
    for (void*& block : gBlocks) {
        if (block != nullptr) ::operator delete(block);
        block = nullptr;
    }
    for (void*& block : gStatics) {
        if (block != nullptr) ::operator delete(block);
        block = nullptr;
    }
    if (gObject != nullptr && !gObjectGone) ::operator delete(gObject);
    gObject = nullptr;
    CMpegBase::ClearCache();
}

void* tpw_mp_object() { return gObject; }
void* tpw_mp_out() { return gOut; }
void* tpw_mp_stream() { return gStream; }
void tpw_mp_set_stream(const uint8_t* data, int size) {
    if (size > kStreamSize) size = kStreamSize;
    std::memcpy(gStream, data, size);
}
void tpw_mp_load(const uint8_t* image) { LoadImage(*gObject, image); }
void tpw_mp_snapshot(uint8_t* image, int fill) {
    int vptr = gDerived && !gObjectGone ? 2 : 1;
    SnapshotImage(*gObject, image, fill, vptr);
}
void tpw_mp_set_out(const uint8_t* data) { std::memcpy(gOut, data, kOutSize); }
void tpw_mp_get_out(uint8_t* data) {
    std::memcpy(data, gOut, kOutSize);
    Swap16(data, kOutSize);   // shorts in the Mac's byte order
}

// Layer I's scratch arrays (the bit allocations, scale factor indices and samples): 64 words each.
void tpw_mp_scratch(uint32_t* out) {
    std::memcpy(out, gMpegLayer1Alloc, 256);
    std::memcpy(out + 64, gMpegLayer1Scale, 256);
    std::memcpy(out + 128, gMpegLayer1Sample, 256);
}
void tpw_mp_set_sb(const int8_t* alloc, const uint32_t* scale, const int32_t* mulstab) {
    std::memcpy(gSbAlloc, alloc, 64);
    std::memcpy(gSbScale, scale, 256);
    std::memcpy(gSbMul, mulstab, sizeof(gSbMul));
}

void tpw_mp_pool_init(int32_t current, int32_t low, int32_t high, int32_t total, int32_t level) {
    CMpegBase::mCurrentCMpegBase = current;
    CMpegBase::mLowCMpegBase = low;
    CMpegBase::mHighCMpegBase = high;
    CMpegBase::mTotalCMpegBase = total;
    CMpegBase::mLevelCMpegBase = level;
}

// The pool's six statics and the chain of idle objects (labels, 0 ends it; at most 40 shown).
void tpw_mp_pool_state(int64_t* out) {
    out[0] = lab(CMpegBase::mFirstCMpegBase);
    out[1] = CMpegBase::mCurrentCMpegBase;
    out[2] = CMpegBase::mLowCMpegBase;
    out[3] = CMpegBase::mHighCMpegBase;
    out[4] = CMpegBase::mTotalCMpegBase;
    out[5] = CMpegBase::mLevelCMpegBase;
    int n = 0;
    for (CMpegBase* o = CMpegBase::mFirstCMpegBase; o != nullptr && n < 40; o = o->mNextPooled) out[6 + n++] = lab(o);
    out[6 + n] = -9;
}

void tpw_mp_set_mmx(int32_t value) { using_MMX = value; }
int32_t tpw_mp_get_mmx() { return using_MMX; }

// Table checks: the host tables, one word at a time (the Python side compares them with the Mac's data).
uint32_t tpw_mp_muls_bits(int row, int scale) { return fbits(MpegMultiplier(row, scale)); }
int32_t tpw_mp_int_alloc(int index) { return MpegIntegerAllocation(index); }

enum Op {
    OP_CTOR, OP_DTOR, OP_DTOR_NULL, OP_NEW, OP_DELETE, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE, OP_CS_NEW, OP_CS_DTOR, OP_DM_NEW,
    OP_DM_CTOR, OP_DM_DTOR, OP_DM_DTOR_NULL, OP_INIT_TABLES, OP_CLOSE_TABLES, OP_OPEN_SYNTH, OP_CLOSE_SYNTH, OP_OPEN_LAYER,
    OP_CLOSE_LAYER, OP_OPEN_PTR, OP_OPEN_HDR, OP_CLOSE, OP_RESET, OP_GETBITS, OP_SEEKBITS, OP_PROCESS_HEADER, OP_DECODE_HEADER,
    OP_GET_HEADER, OP_GET_HEADER_H, OP_DECODE, OP_READ, OP_LAYER1, OP_SB_STEREO, OP_MULDIV, OP_SET_MMX, OP_VIRTUAL_GET_HEADER,
    OP_NEW_DEFAULT_OBJECT
};

// One operation. Results come back as a 64-bit integer (labels for pointers).
int64_t tpw_mp_op(int op, int64_t a, int64_t b, int64_t c) {
    CMpegBase& o = *gObject;
    switch (op) {
    case OP_CTOR:
        // a re-construction over the filled block
        if (gDerived) ::new (gObject) FakeDerived();
        else ::new (gObject) CMpegBase();
        return 0;
    case OP_DTOR:
        if (a > 0) {
            // the deleting form: the block goes to the pool
            gObjectGone = true;
            delete gObject;
        } else {
            gObject->~CMpegBase();
        }
        return 0;
    case OP_DTOR_NULL:
        return 0;   // the Mac does nothing for a NULL this; no C++ spelling of it
    case OP_NEW: {
        void* block = CMpegBase::operator new(PoolSize(b));
        gBlocks[a] = block;
        return lab(block);
    }
    case OP_DELETE:
        CMpegBase::operator delete(a < 0 ? nullptr : gBlocks[a], PoolSize(b));
        if (a >= 0 && c != 0) gBlocks[a] = nullptr;
        return 0;
    case OP_TOPUP:
        CMpegBase::TopUp();
        return 0;
    case OP_SETLEVEL:
        CMpegBase::SetLevel(static_cast<int32_t>(a));
        return 0;
    case OP_CLEARCACHE:
        CMpegBase::ClearCache();
        return 0;
    case OP_CS_NEW: {
        gStatics[0] = new _CS_CMpegBase();
        return lab(gStatics[0]);
    }
    case OP_CS_DTOR:
        if (a == -100) return 0;   // the Mac does nothing for a NULL this; no C++ spelling of it
        if (a > 0) {
            delete static_cast<_CS_CMpegBase*>(gStatics[0]);
            gStatics[0] = nullptr;
        } else {
            static_cast<_CS_CMpegBase*>(gStatics[0])->~_CS_CMpegBase();
        }
        return 0;
    case OP_DM_NEW:
        gStatics[1] = new CMpegBase_DecodeMethod();
        return lab(gStatics[1]);
    case OP_DM_CTOR:
        ::new (gStatics[1]) CMpegBase_DecodeMethod();
        return 0;
    case OP_DM_DTOR:
        if (a > 0) {
            delete static_cast<CMpegBase_DecodeMethod*>(gStatics[1]);
            gStatics[1] = nullptr;
        } else {
            static_cast<CMpegBase_DecodeMethod*>(gStatics[1])->~CMpegBase_DecodeMethod();
        }
        return 0;
    case OP_DM_DTOR_NULL:
        return 0;
    case OP_INIT_TABLES:
        CMpegBase::Init_Tables();
        return 0;
    case OP_CLOSE_TABLES:
        CMpegBase::Close_Tables();
        return 0;
    case OP_OPEN_SYNTH:
        o.OpenSynth();
        return 0;
    case OP_CLOSE_SYNTH:
        o.CloseSynth();
        return 0;
    case OP_OPEN_LAYER:
        return o.OpenLayer();
    case OP_CLOSE_LAYER:
        return o.CloseLayer();
    case OP_OPEN_PTR:
        // the stream is the data pointer (a < 0: a NULL pointer)
        return o.Open(a < 0 ? nullptr : static_cast<void*>(gStream + a), static_cast<int32_t>(b));
    case OP_OPEN_HDR:
        return o.Open(static_cast<uint32_t>(a), static_cast<int32_t>(b));
    case OP_CLOSE:
        return o.Close();
    case OP_RESET:
        o.Reset();
        return 0;
    case OP_GETBITS:
        return static_cast<int64_t>(o.GetBits(static_cast<int32_t>(a)));
    case OP_SEEKBITS:
        o.CMpegBase::SeekBits(static_cast<int32_t>(a));
        return 0;
    case OP_PROCESS_HEADER:
        return o.ProcessHeader(static_cast<uint32_t>(a));
    case OP_DECODE_HEADER:
        return o.CMpegBase::DecodeHeader();
    case OP_GET_HEADER:
        return o.CMpegBase::GetHeader();
    case OP_GET_HEADER_H:
        return o.GetHeader(static_cast<uint32_t>(a));
    case OP_DECODE:
        return o.Decode(a < 0 ? nullptr : reinterpret_cast<int16_t*>(gOut + a));
    case OP_READ:
        return o.Read(a < 0 ? nullptr : gOut + a, static_cast<int32_t>(b));
    case OP_LAYER1:
        return o.Decode_Layer1(a < 0 ? nullptr : reinterpret_cast<int16_t*>(gOut + a));
    case OP_SB_STEREO:
        o.get_sb_stereo(static_cast<int32_t>(a), reinterpret_cast<char(*)[2]>(gSbAlloc), reinterpret_cast<uint32_t(*)[32]>(gSbScale),
                        reinterpret_cast<int16_t*>(gOut + b), gSbMul);
        return 0;
    case OP_MULDIV:
        return muldiv(static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<int32_t>(c));
    case OP_SET_MMX:
        using_MMX = static_cast<int32_t>(a);
        return 0;
    case OP_VIRTUAL_GET_HEADER:
        return o.GetHeader();   // through the vtable
    default:
        return -12345;
    }
}

}  // extern "C"
