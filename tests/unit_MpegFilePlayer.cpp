// Fakes and test entry points for unit w80c, the sound library's MPEG file player (libtpw_unit_MpegFilePlayer.dylib, driven by
// tests/test_MpegFilePlayer.py; see calllog.h). The program under test is "sound symbols shared":
//   src/sound/CSprocketsModule.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp, src/sound/StreamableMpeg2.cpp  CMpegMacFilePlayerUnpooled (Open, GetHeader, DecodeHeader, SeekBits, NextBuffer, ReadBuffered,
//                             RequestBuffer, Close), the pool SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10> and its queue
// Real beside them: src/sound/FileAccess.cpp, src/sound/LbMpeg.cpp, src/sound/MpegDecode.cpp (CMpegBase, unit w71a: the decoder the player extends; Open calls its OpenSynth, OpenLayer
// and Reset, ProcessHeader reads the frame header, Read decodes through the player's virtuals). Fakes (each records what the
// original's callee would be given; the PowerPC side records the same):
//   tpw::ReadFileReference: the host's read of the bank file, answered from the case's file image (the PowerPC side's PBReadAsync
//       and PBReadSync stubs do the same read, so both record "PBRead": reference, offset, count, where the buffer is)
//   BlockMoveData, DebugStr, SamsUtilities::UDebug, UAllocator::Clear, the file's GetMacFileReference (queue "REF"), the heap
//   (operator new: "new" with the size, labelled blocks record "delete"), and the two functions of unit w71b that CMpegBase's
//   Read and Decode call (PolySynth and Decode_Layer2, as in tests/unit_MpegBase.cpp).
//
// The player is a block of the runtime heap (mode 0), a fake-derived one whose GetHeader is a fake (mode 1: the queue "GH"
// answers it), the pooled wrapper inside a pool element (mode 2), or none (mode 3: pool operations only), filled with one byte and
// then loaded from a "Mac image": the 0x6d64 bytes of the Mac's object (the pointers as small label codes, see Code below) and the
// contents of the two buffers and of the frame buffer. tpw_mf_snapshot makes the same image from the host object.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>

#include "CMpegBase.h"
#include "calllog.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"


using Pool = SamsUtilities::MpegFilePlayerPool;
using PoolQueue = Pool::Queue;
using Element = PoolQueue::Element;

namespace {
// Every block the runtime heap hands out in a case is kept until the case ends (a freed block is not reused, so that no label
// is mistaken for another object), and freed then.
struct Tracked {
    void* block;
    bool freed;
};
Tracked gTracked[8192];
int gTrackedCount;

// The allocation observer of calllog.cpp (included below): a block the code asked the runtime for is a "new" event of its size
// (the host sizes stand for the Mac's: the element 0x6d6c, the player 0x6d64) and gets the next label, so that the "delete" of
// the original's __dl__FPv matches the host's operator delete.
void PlayerAllocated(std::size_t size, bool, void* block) {
    if (gTrackedCount < 8192) gTracked[gTrackedCount++] = Tracked{block, false};
    if (!on()) return;
    int64_t mapped = static_cast<int64_t>(size);
    if (size == sizeof(Element) + sizeof(CMpegMacFilePlayerUnpooled)) mapped = 0x6d6c;
    else if (size == sizeof(CMpegMacFilePlayerUnpooled)) mapped = 0x6d64;
    ev("new", {mapped});
    newlab(block);
}
}  // namespace
namespace {
// A block freed in a case is only marked (and kept until the case ends).
bool PlayerReleased(void* block, bool) {
    for (int i = 0; i < gTrackedCount; ++i) {
        if (gTracked[i].block == block && !gTracked[i].freed) {
            gTracked[i].freed = true;
            return true;
        }
    }
    return false;
}
}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) PlayerAllocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) PlayerReleased(p, array)
#include "calllog.cpp"

namespace {

constexpr int kOutSize = 0x4000;
constexpr int kDataSize = 0x2100;
constexpr int kFileMax = 0x14000;
constexpr int kBufferBytes = 0x2000;
constexpr int kFrameBytes = 0x800;
// The Mac image: the object (0x6d64 bytes), then buffer 0, buffer 1 and the frame buffer as they are in the object.
constexpr int kObjectImage = 0x6d64;
constexpr int kImageSize = kObjectImage + 2 * kBufferBytes + kFrameBytes;

// The player with its GetHeader a fake, as a derived class may override it (Open calls it through the vtable).
class FakePlayer : public CMpegMacFilePlayerUnpooled {
public:
    int32_t GetHeader() override {
        ev("GetHeader", {lab(this)});
        return static_cast<int32_t>(ret("GH", 0));
    }
};

CMpegMacFilePlayerUnpooled* gPlayer;   // the object under test (mode 0, 1, 2) or null (mode 3)
int gMode;
int gGone;                              // 1: deleted (its block is freed); 2: destroyed but its memory is valid
void* gBlock;                           // the heap block of the object (mode 0, 1), or the pool element (mode 2)
uint8_t gData[kDataSize + 64];          // the cached start of the sample (Open's `data`)
uint8_t gOut[kOutSize];                 // the output buffer of Read and ReadBuffered
uint8_t gFileBytes[kFileMax];           // the bank file's contents
int gFileSize;
int32_t gFileReference;                 // the reference number the file has (a different one is an unknown file)
bfl::FileSystem::CFile* gFileObject;    // the CFile object the player is opened over
SamsUtilities::UAllocator gAllocator;   // what the pool's allocator is
void* gBlocks[16];                      // blocks the pool operations took (NEW, NEWRAW)
Element* gElems[16];                    // elements the pool operations made
void* gStatics[2];                      // a StaticCleanup object, a prototype of each vtable
int32_t gPolyCalls;
int32_t gL2Calls;
uint32_t gInitialTarget;                // the pool's statics before any case changed them
uint8_t gInitialKeepFull;
alignas(16) uint8_t gProtoPlayer[sizeof(Pool)];   // a player and a pooled wrapper made for their vtable words
alignas(16) uint8_t gProtoPool[sizeof(Pool)];
alignas(16) uint8_t gProtoFake[sizeof(Pool)];

uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint16_t be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
void put32(uint8_t* p, uint32_t v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}
void put16(uint8_t* p, uint32_t v) {
    p[0] = v >> 8;
    p[1] = v;
}

uint8_t* Align16(uint8_t* p) { return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(p) + 15) & ~uintptr_t(15)); }
uint8_t* Buffer0() { return Align16(gPlayer->mBufferSpace); }
uint8_t* Frame() { return Align16(gPlayer->mFrameSpace); }

// A pointer as a small code (the same codes on the PowerPC side): 0 null, 1 buffer 0, 2 buffer 1, 3 the cached data, 4 the output,
// 5 the frame buffer, 0xff anything else.
uint32_t Code(const void* p) {
    if (p == nullptr) return 0;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    if (gPlayer != nullptr && gGone != 1) {
        if (b == Buffer0()) return 1;
        if (b == Buffer0() + kBufferBytes) return 2;
        if (b == Frame()) return 5;
    }
    if (b == gData) return 3;
    if (b == gOut) return 4;
    return 0xff;
}

uint8_t* Decode(uint32_t code) {
    switch (code) {
    case 1: return Buffer0();
    case 2: return Buffer0() + kBufferBytes;
    case 3: return gData;
    case 4: return gOut;
    case 5: return Frame();
    default: return nullptr;
    }
}

// Where a pointer points, as kind << 24 | offset: 1 buffer 0, 2 buffer 1, 3 the cached data, 4 the output, 5 the frame buffer,
// 7 anywhere else (a local variable), 0 null.
int64_t Where(const void* p) {
    if (p == nullptr) return 0;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    if (gPlayer != nullptr && gGone != 1) {
        if (b >= Buffer0() && b < Buffer0() + kBufferBytes) return (1 << 24) | (b - Buffer0());
        if (b >= Buffer0() + kBufferBytes && b < Buffer0() + 2 * kBufferBytes) return (2 << 24) | (b - Buffer0() - kBufferBytes);
        if (b >= Frame() && b < Frame() + kFrameBytes) return (5 << 24) | (b - Frame());
    }
    if (b >= gData && b < gData + sizeof(gData)) return (3 << 24) | (b - gData);
    if (b >= gOut && b < gOut + kOutSize) return (4 << 24) | (b - gOut);
    return 7 << 24;
}

// The label of a pool element, given the element or its data (1000 + the element's label).
int64_t PoolLab(const void* p) {
    if (p == nullptr) return 0;
    int64_t l = lab(p);
    if (l > 0) return l;
    l = lab(static_cast<const uint8_t*>(p) - sizeof(Element));
    return l > 0 ? 1000 + l : -1;
}

// The vptr of a live object as 1 (the player), 2 (the pooled wrapper), 3 (the fake-derived one), 0 (destroyed), 9 other.
uint32_t VptrLabel(const void* object) {
    if (gGone != 0) return 0;
    void* vptr;
    std::memcpy(&vptr, object, sizeof vptr);
    void* a; void* b; void* c;
    std::memcpy(&a, gProtoPlayer, sizeof a);
    std::memcpy(&b, gProtoPool, sizeof b);
    std::memcpy(&c, gProtoFake, sizeof c);
    return vptr == a ? 1 : vptr == b ? 2 : vptr == c ? 3 : 9;
}

}  // namespace

// ---- the callees outside the unit

// The read the Mac made through PBReadAsync, answered from the case's file image.
int32_t tpw::ReadFileReference(int32_t reference, uint32_t offset, void* buffer, uint32_t count) {
    ev("PBRead", {reference, static_cast<int64_t>(offset), static_cast<int64_t>(count), Where(buffer)});
    if (reference != gFileReference) return -1;
    if (offset >= static_cast<uint32_t>(gFileSize)) return 0;
    uint32_t n = static_cast<uint32_t>(gFileSize) - offset;
    if (n > count) n = count;
    std::memcpy(buffer, gFileBytes + offset, n);
    return static_cast<int32_t>(n);
}

void BlockMoveData(const void* src, void* dest, int32_t count) {
    ev("BlockMoveData", {Where(src), Where(dest), count});
    if (count > 0) std::memmove(dest, src, count);
}

void DebugStr(const unsigned char* message) {
    char text[64];
    std::memcpy(text, message + 1, message[0]);
    text[message[0]] = '\0';
    ev("DebugStr", {}, {text});
}

void SamsUtilities::UDebug(const char* format, ...) {
    ev("UDebug", {}, {format});
}

SamsUtilities::UAllocator* SamsUtilities::gMemory = nullptr;
SamsUtilities::UAllocator::UAllocator() : mOpaque{} {}

void SamsUtilities::UAllocator::Clear(void* dest, uint32_t size) {
    ev("Clear", {this == &gAllocator ? 9 : -1, PoolLab(dest), size == sizeof(CMpegMacFilePlayerUnpooled) ? 0x6d64 : size});
    std::memset(dest, 0, size);
}

namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::CFile(const CFile&) : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() {
    ev("GetMacFileReference", {lab(this)});
    return static_cast<int32_t>(ret("REF", gFileReference));
}
}  // namespace bfl::FileSystem

// CMpegBase's callees of unit w71b (PolySynth, Decode_Layer2), as unit_MpegBase.cpp fakes them.
namespace {
int64_t OutWhere(const int16_t* p) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
    if (b >= gOut && b < gOut + kOutSize) return (b - gOut) / 2;
    if (gPlayer != nullptr && p >= gPlayer->mFrame && p < gPlayer->mFrame + 0x920) return 0x10000 + (p - gPlayer->mFrame);
    return -1;
}
void Put16(int16_t* p, uint32_t v) {
    if (OutWhere(p) >= 0) *p = static_cast<int16_t>(v & 0xffff);
}
}  // namespace

void CMpegBase::PolySynth(int16_t* out, float* fraction) {
    int nch = mChannels == 2 ? 2 : 1;
    ev("PolySynth", {lab(this), OutWhere(out), mChannels, mBitPos, mChannels32});
    for (int i = 0; i < 32 * nch; i += 8) {
        ev("Fraction", {i, fbits(fraction[i]), fbits(fraction[i + 1]), fbits(fraction[i + 2]), fbits(fraction[i + 3]),
                        fbits(fraction[i + 4]), fbits(fraction[i + 5]), fbits(fraction[i + 6]), fbits(fraction[i + 7])});
    }
    int32_t call = gPolyCalls++;
    if (out != nullptr)
        for (int i = 0; i < 32 * nch; ++i) Put16(out + i, ((fbits(fraction[i]) >> 7) ^ (i * 0x9e3 + call * 0x35)));
}

int32_t CMpegBase::Decode_Layer2(int16_t* out) {
    ev("Decode_Layer2", {lab(this), OutWhere(out), mBitPos, mFrameBytes});
    int32_t call = gL2Calls++;
    int32_t shorts = mFrameBytes < mFrameBytes2 ? mFrameBytes : mFrameBytes2;
    if (shorts > 0x1200) shorts = 0x1200;
    shorts /= 2;
    if (out != nullptr)
        for (int i = 0; i < shorts; ++i) Put16(out + i, i * 0x57 + call * 0x1b3 + (mBitPos & 0xffff));
    return static_cast<int32_t>(ret("L2", 0));
}

// ---- the images
namespace {

void Swap16(uint8_t* p, int n) {
    for (int i = 0; i + 1 < n; i += 2) std::swap(p[i], p[i + 1]);
}

// The CMpegBase part of the Mac image (0x24f0 bytes; the data pointer as a code).
void LoadBase(CMpegBase& o, const uint8_t* img) {
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
    o.mData = Decode(be32(img + 0x24a8));
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

void SnapshotBase(const CMpegBase& o, uint8_t* img, int fill) {
    std::memset(img, fill, 0x24f0);
    std::memcpy(img, o.mSynthBuf, 0x1200);
    std::memcpy(img + 0x1200, o.mSynthSpare, 0x40);
    std::memcpy(img + 0x1240, o.mFrame, 0x1240);
    Swap16(img + 0x1240, 0x1240);
    put32(img + 0x2480, VptrLabel(&o));
    put32(img + 0x2484, lab(o.mNextPooled));
    put32(img + 0x2488, o.mSampleRate);
    put32(img + 0x248c, o.mSampleRate2);
    put32(img + 0x2490, o.mFrameBytes);
    img[0x2494] = o.mLayer;
    put32(img + 0x2498, o.mBitRate);
    put32(img + 0x249c, o.mFrameSize);
    put32(img + 0x24a0, o.mFrameBytes2);
    put32(img + 0x24a4, o.mFrames);
    put32(img + 0x24a8, Code(o.mData));
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

// The player's own part: the parameter block at 0x24f0 in the Mac's layout, the pointers as codes, the three buffers' contents.
void LoadPlayer(CMpegMacFilePlayerUnpooled& o, const uint8_t* img) {
    LoadBase(o, img);
    MacFileIoParams& io = o.mIo;
    const uint8_t* p = img + 0x24f0;
    io.qLink = be32(p + 0x00);
    io.qType = static_cast<int16_t>(be16(p + 0x04));
    io.ioTrap = static_cast<int16_t>(be16(p + 0x06));
    io.ioCmdAddr = be32(p + 0x08);
    io.ioCompletion = be32(p + 0x0c);
    io.ioResult = static_cast<int16_t>(be16(p + 0x10));
    io.ioNamePtr = be32(p + 0x12);
    io.ioVRefNum = static_cast<int16_t>(be16(p + 0x16));
    io.ioRefNum = static_cast<int16_t>(be16(p + 0x18));
    io.ioVersNum = static_cast<int8_t>(p[0x1a]);
    io.ioPermssn = static_cast<int8_t>(p[0x1b]);
    io.ioMisc = be32(p + 0x1c);
    io.ioBuffer = Decode(be32(p + 0x20));
    io.ioReqCount = static_cast<int32_t>(be32(p + 0x24));
    io.ioActCount = static_cast<int32_t>(be32(p + 0x28));
    io.ioPosMode = static_cast<int16_t>(be16(p + 0x2c));
    io.ioPosOffset = static_cast<int32_t>(be32(p + 0x2e));
    for (int i = 0; i < 3; ++i) o.mBuffers[i] = Decode(be32(img + 0x6534 + 4 * i));
    o.mCurrent = static_cast<int32_t>(be32(img + 0x6540));
    o.mReadPosition = static_cast<int32_t>(be32(img + 0x6544));
    o.mFrameBuffer = Decode(be32(img + 0x6d58));
    o.mFile = be32(img + 0x6d5c) == 2 ? gFileObject : nullptr;
    o.mStart = static_cast<int32_t>(be32(img + 0x6d60));
    std::memcpy(Buffer0(), img + kObjectImage, kBufferBytes);
    std::memcpy(Buffer0() + kBufferBytes, img + kObjectImage + kBufferBytes, kBufferBytes);
    std::memcpy(Frame(), img + kObjectImage + 2 * kBufferBytes, kFrameBytes);
}

void SnapshotPlayer(const CMpegMacFilePlayerUnpooled& o, uint8_t* img, int fill) {
    std::memset(img, fill, kImageSize);
    SnapshotBase(o, img, fill);
    const MacFileIoParams& io = o.mIo;
    uint8_t* p = img + 0x24f0;
    put32(p + 0x00, io.qLink);
    put16(p + 0x04, io.qType);
    put16(p + 0x06, io.ioTrap);
    put32(p + 0x08, io.ioCmdAddr);
    put32(p + 0x0c, io.ioCompletion);
    put16(p + 0x10, io.ioResult);
    put32(p + 0x12, io.ioNamePtr);
    put16(p + 0x16, io.ioVRefNum);
    put16(p + 0x18, io.ioRefNum);
    p[0x1a] = static_cast<uint8_t>(io.ioVersNum);
    p[0x1b] = static_cast<uint8_t>(io.ioPermssn);
    put32(p + 0x1c, io.ioMisc);
    put32(p + 0x20, Code(io.ioBuffer));
    put32(p + 0x24, io.ioReqCount);
    put32(p + 0x28, io.ioActCount);
    put16(p + 0x2c, io.ioPosMode);
    put32(p + 0x2e, io.ioPosOffset);
    for (int i = 0; i < 3; ++i) put32(img + 0x6534 + 4 * i, Code(o.mBuffers[i]));
    put32(img + 0x6540, o.mCurrent);
    put32(img + 0x6544, o.mReadPosition);
    put32(img + 0x6d58, Code(o.mFrameBuffer));
    put32(img + 0x6d5c, o.mFile == nullptr ? 0 : o.mFile == gFileObject ? 2 : 0xff);
    put32(img + 0x6d60, o.mStart);
    std::memcpy(img + kObjectImage, Buffer0(), kBufferBytes);
    std::memcpy(img + kObjectImage + kBufferBytes, Buffer0() + kBufferBytes, kBufferBytes);
    std::memcpy(img + kObjectImage + 2 * kBufferBytes, Frame(), kFrameBytes);
}

}  // namespace

extern "C" {

// Starts a case: the object (mode 0 the player, 1 the fake-derived one, 2 the pooled wrapper inside a pool element, 3 none)
// filled with one byte, the pool emptied, the file reference set.
void tpw_mf_begin(int mode, int fill, int outFill, int fileReference) {
    // the statics as the program starts (captured before the first case changes them)
    static const uint32_t sInitialTarget = Pool::gPoolTarget;
    static const uint8_t sInitialKeepFull = Pool::gPoolKeepFull;
    gInitialTarget = sInitialTarget;
    gInitialKeepFull = sInitialKeepFull;
    gMode = mode;
    gGone = 0;
    gFileReference = fileReference;
    gPlayer = nullptr;
    gBlock = nullptr;
    // prototypes of the vtables: constructed in place, never used again
    ::new (gProtoPlayer) CMpegMacFilePlayerUnpooled();
    ::new (gProtoPool) Pool();
    ::new (gProtoFake) FakePlayer();
    std::memset(gData, 0, sizeof(gData));
    std::memset(gOut, outFill, sizeof(gOut));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gElems, 0, sizeof(gElems));
    std::memset(gStatics, 0, sizeof(gStatics));
    gFileSize = 0;
    gTrackedCount = 0;
    gPolyCalls = 0;
    gL2Calls = 0;
    using_MMX = 0;
    gFileObject = new bfl::FileSystem::CFile();
    new (&Pool::gPoolQueue) PoolQueue();
    Pool::gPoolKeepFull = 0;
    Pool::gPoolTarget = 10;
    gAllocator = SamsUtilities::UAllocator();
    SamsUtilities::gMemory = &gAllocator;
    if (mode == 0 || mode == 1) {
        gBlock = ::operator new(sizeof(Pool));
        std::memset(gBlock, fill, sizeof(Pool));
        if (mode == 0) gPlayer = ::new (gBlock) CMpegMacFilePlayerUnpooled();
        else gPlayer = ::new (gBlock) FakePlayer();
    } else if (mode == 2) {
        gBlock = ::operator new(sizeof(Element) + sizeof(Pool));
        std::memset(gBlock, fill, sizeof(Element) + sizeof(Pool));
        Element* element = ::new (gBlock) Element();
        gPlayer = ::new (element->GetData()) Pool();
    }
    if (gPlayer != nullptr) {
        // the constructor set the vptr and mOpen; the rest is the fill
        std::memset(reinterpret_cast<uint8_t*>(gPlayer) + sizeof(void*), fill, sizeof(Pool) - sizeof(void*));
        gPlayer->mNextPooled = nullptr;
    }
}

// Ends it (the log is off by now): every block the case took is freed, the pool is emptied.
void tpw_mf_end() {
    new (&Pool::gPoolQueue) PoolQueue();
    for (int i = 0; i < gTrackedCount; ++i) std::free(gTracked[i].block);
    gTrackedCount = 0;
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gElems, 0, sizeof(gElems));
    std::memset(gStatics, 0, sizeof(gStatics));
    gBlock = nullptr;
    gPlayer = nullptr;
    gFileObject = nullptr;
}

void* tpw_mf_object() { return gPlayer; }
void* tpw_mf_element() { return gMode == 2 ? gBlock : nullptr; }
void* tpw_mf_file_object() { return gFileObject; }
void tpw_mf_set_file(const uint8_t* data, int size) {
    gFileSize = size > kFileMax ? kFileMax : size;
    std::memcpy(gFileBytes, data, gFileSize);
}
void tpw_mf_set_data(const uint8_t* data, int size) {
    if (size > kDataSize) size = kDataSize;
    std::memcpy(gData, data, size);
}
void tpw_mf_load(const uint8_t* image) {
    if (gPlayer != nullptr) LoadPlayer(*gPlayer, image);
}
void tpw_mf_snapshot(uint8_t* image, int fill) {
    if (gPlayer != nullptr && gGone != 1) SnapshotPlayer(*gPlayer, image, fill);
}
// The output buffer; with `shorts` its 16-bit samples in the Mac's byte order (Read's samples are native shorts here).
void tpw_mf_get_out(uint8_t* data, int shorts) {
    std::memcpy(data, gOut, kOutSize);
    if (shorts != 0) Swap16(data, kOutSize);
}
void tpw_mf_get_data(uint8_t* data) { std::memcpy(data, gData, kDataSize); }
uint32_t tpw_mf_image_size() { return kImageSize; }

// The pool: the queue (flags, head, tail, count), the two statics, then for each element in the chain its label, its next, its
// type word and its player's ioResult; -9 ends the chain (at most 40 shown).
void tpw_mf_pool_state(int64_t* out) {
    PoolQueue& q = Pool::gPoolQueue;
    out[0] = q.mFlags;
    out[1] = PoolLab(q.mHead);
    out[2] = PoolLab(q.mTail);
    out[3] = q.mCount;
    out[4] = Pool::gPoolKeepFull;
    out[5] = Pool::gPoolTarget;
    int n = 0;
    for (Element* e = q.mHead; e != nullptr && n < 40; e = e->mLink, ++n) {
        CMpegMacFilePlayerUnpooled* player = static_cast<CMpegMacFilePlayerUnpooled*>(e->GetData());
        out[6 + 4 * n] = PoolLab(e);
        out[7 + 4 * n] = PoolLab(e->mLink);
        out[8 + 4 * n] = e->mType;
        out[9 + 4 * n] = player->mIo.ioResult;
    }
    out[6 + 4 * n] = -9;
}

int32_t tpw_mf_get_mmx() { return using_MMX; }
uint32_t tpw_mf_initial_target() { return gInitialTarget; }
uint32_t tpw_mf_initial_keepfull() { return gInitialKeepFull; }
// 1 when the unit is built with the port fixes on (tpw-port issue #50): operator new then makes a new element when no player is idle,
// where the Mac answered NULL.
int32_t tpw_mf_portfix() {
    return 0;
}
void tpw_mf_set_pool(int keepFull, uint32_t target) {
    Pool::gPoolKeepFull = static_cast<uint8_t>(keepFull);
    Pool::gPoolTarget = target;
}

enum Op {
    OP_CTOR, OP_DTOR, OP_OPEN, OP_CLOSE, OP_GET_HEADER, OP_DECODE_HEADER, OP_SEEK_BITS, OP_NEXT_BUFFER, OP_READ_BUFFERED,
    OP_REQUEST_BUFFER, OP_READ, OP_RESET, OP_NEW, OP_DELETE, OP_MAINTAIN, OP_KEEP_FULL, OP_CLEAR_POOL, OP_IS_AVAILABLE,
    OP_REPORT, OP_MAKE, OP_QADD, OP_QREMOVE, OP_QHEAD, OP_QSIZE, OP_GET_NEXT, OP_GET_DATA, OP_FROM_DATA, OP_ELEMENT_CTOR,
    OP_QUEUE_CTOR, OP_PLANT, OP_STATIC_NEW, OP_STATIC_DTOR, OP_SINIT, OP_SET_TARGET, OP_NEW_RAW, OP_QPLANT, OP_ELEMENT_TYPE,
    OP_UDEBUG
};

// The Mac sizes the pool operations are given: 0x6d64 stands for the host player's size.
static std::size_t PoolSize(int64_t size) {
    return size == 0x6d64 ? sizeof(Pool) : static_cast<std::size_t>(static_cast<uint32_t>(size));
}

// One operation. Results come back as a 64-bit integer (labels for pointers).
int64_t tpw_mf_op(int op, int64_t a, int64_t b, int64_t c) {
    CMpegMacFilePlayerUnpooled& o = *gPlayer;
    switch (op) {
    case OP_CTOR:
        // a re-construction over the filled object
        gGone = 0;
        if (gMode == 2) ::new (gPlayer) Pool();
        else ::new (gPlayer) CMpegMacFilePlayerUnpooled();
        return 0;
    case OP_DTOR:
        if (a > 0) {
            if (gMode == 2) {
                gGone = 2;
                delete static_cast<Pool*>(gPlayer);
            } else {
                gGone = 1;
                delete gPlayer;
            }
        } else {
            gGone = 2;
            if (gMode == 2) static_cast<Pool*>(gPlayer)->~Pool();
            else gPlayer->~CMpegMacFilePlayerUnpooled();
        }
        return 0;
    case OP_OPEN:
        return o.Open(gFileObject, static_cast<int32_t>(a), static_cast<int32_t>(b), c < 0 ? nullptr : reinterpret_cast<char*>(gData + c));
    case OP_CLOSE:
        return o.CMpegMacFilePlayerUnpooled::Close();
    case OP_GET_HEADER:
        return o.CMpegMacFilePlayerUnpooled::GetHeader();
    case OP_DECODE_HEADER:
        return o.CMpegMacFilePlayerUnpooled::DecodeHeader();
    case OP_SEEK_BITS:
        o.CMpegMacFilePlayerUnpooled::SeekBits(static_cast<int32_t>(a));
        return 0;
    case OP_NEXT_BUFFER:
        return o.NextBuffer();
    case OP_READ_BUFFERED:
        return o.ReadBuffered(a < 0 ? nullptr : gOut + a, static_cast<uint32_t>(b));
    case OP_REQUEST_BUFFER:
        o.RequestBuffer(static_cast<int32_t>(a), b != 0);
        return 0;
    case OP_READ:
        return o.Read(a < 0 ? nullptr : gOut + a, static_cast<int32_t>(b));
    case OP_RESET:
        o.Reset();
        return 0;
    case OP_NEW: {
        void* block = Pool::operator new(PoolSize(b));
        gBlocks[a] = block;
        return PoolLab(block);
    }
    case OP_NEW_RAW: {
        // a plain heap block, filled with c
        void* block = ::operator new(PoolSize(b));
        std::memset(block, static_cast<int>(c), PoolSize(b));
        gBlocks[a] = block;
        return PoolLab(block);
    }
    case OP_DELETE:
        Pool::operator delete(gBlocks[a], PoolSize(b));
        if (c != 0) gBlocks[a] = nullptr;
        return 0;
    case OP_MAINTAIN:
        Pool::MaintainPoolLevel();
        return 0;
    case OP_KEEP_FULL:
        Pool::SetPoolKeepFull(static_cast<uint8_t>(a));
        return 0;
    case OP_CLEAR_POOL:
        Pool::ClearPool(static_cast<uint8_t>(a));
        return 0;
    case OP_IS_AVAILABLE:
        return Pool::IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(gElems[a]->GetData())) ? 1 : 0;
    case OP_REPORT:
        Pool::ReportPoolStatistics();
        return 0;
    case OP_MAKE:
        gElems[a] = Pool::MakeNewPoolElement();
        return PoolLab(gElems[a]);
    case OP_QADD:
        Pool::gPoolQueue.Add(gElems[a]);
        return 0;
    case OP_QREMOVE:
        return Pool::gPoolQueue.Remove(gElems[a]) ? 1 : 0;
    case OP_QHEAD:
        return PoolLab(Pool::gPoolQueue.GetHead());
    case OP_QSIZE:
        return Pool::gPoolQueue.GetSize();
    case OP_GET_NEXT:
        return PoolLab(gElems[a]->GetNext());
    case OP_GET_DATA:
        return PoolLab(gElems[a]->GetData());
    case OP_FROM_DATA:
        return PoolLab(Element::GetElementFromDataPointer(gElems[a]->GetData()));
    case OP_ELEMENT_CTOR:
        // the element in element slot a, or (b != 0) the raw block in block slot a
        ::new (b != 0 ? static_cast<Element*>(gBlocks[a]) : gElems[a]) Element();
        return 0;
    case OP_QUEUE_CTOR:
        ::new (&Pool::gPoolQueue) PoolQueue();
        return 0;
    case OP_PLANT:
        // the ioResult of the player in element a, and its count of bytes read
        static_cast<CMpegMacFilePlayerUnpooled*>(gElems[a]->GetData())->mIo.ioResult = static_cast<int16_t>(b);
        return 0;
    case OP_STATIC_NEW:
        gStatics[0] = new Pool::StaticCleanup();
        return lab(gStatics[0]);
    case OP_STATIC_DTOR:
        if (a > 0) {
            delete static_cast<Pool::StaticCleanup*>(gStatics[0]);
            gStatics[0] = nullptr;
        } else {
            static_cast<Pool::StaticCleanup*>(gStatics[0])->~StaticCleanup();
        }
        return 0;
    case OP_SINIT: {
        // __sinit_LbMpeg_cpp has no host function (the file's statics are constructed by the runtime): replay what it does in
        // its order. a and b are the two guard bytes (0x1003e329 for the queue, 0x1003e328 for the cleanup object).
        alignas(CMpegBase_DecodeMethod) uint8_t method[sizeof(CMpegBase_DecodeMethod) + 8];
        ::new (method) CMpegBase_DecodeMethod();
        ev("register", {1});
        ev("register", {2});
        int64_t guards = 0;
        if (a == 0) {
            ::new (&Pool::gPoolQueue) PoolQueue();
            a = 1;
        }
        guards |= a << 1;
        if (b == 0) {
            ev("register", {3});
            b = 1;
        }
        guards |= b;
        return guards;
    }
    case OP_SET_TARGET:
        Pool::gPoolTarget = static_cast<uint32_t>(a);
        return 0;
    case OP_QPLANT:
        // raw queue words: flags a, head (element b or none), tail (element c or none), the count in the next op's place
        Pool::gPoolQueue.mFlags = static_cast<uint16_t>(a);
        Pool::gPoolQueue.mHead = b < 0 ? nullptr : gElems[b];
        Pool::gPoolQueue.mTail = c < 0 ? nullptr : gElems[c];
        Pool::gPoolQueue.mCount = 0x1234;
        return 0;
    case OP_ELEMENT_TYPE:
        return (b != 0 ? static_cast<Element*>(gBlocks[a]) : gElems[a])->mType;
    case OP_UDEBUG:
        return 0;
    default:
        return -12345;
    }
}

}  // extern "C"
