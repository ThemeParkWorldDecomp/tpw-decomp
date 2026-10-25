// LbMpeg.cpp: name proven by __sinit_LbMpeg_cpp in the sound library.

#include "CMpegBase.h"
#include "helpers/LbMpegHelpers.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "TbMpegBase.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>

// The sound library's MPEG audio decoder object, part 1 (unit w71a): CMpegBase's pool, bit reader, header parser, frame
// reader and layer I decoder (program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp). See include/TbMpegBase.h
// for the layout. PolySynth, dct64, Decode_Layer2 and II_step_two (unit w71b) are declared there and defined elsewhere.

extern int32_t using_MMX;

namespace {

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

}  // namespace

int32_t muldiv(int32_t a, int32_t b, int32_t c);

void CMpegBase::ClearCache() {
    while (mFirstCMpegBase != nullptr) {
        CMpegBase* object = mFirstCMpegBase;
        mFirstCMpegBase = object->mNextPooled;
        ::operator delete(object);
    }
    mCurrentCMpegBase = 0;
    mTotalCMpegBase = 0;
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

// ---------------------------------------------------------------- the pool

CMpegBase::CMpegBase() { mOpen = 0; }

CMpegBase::~CMpegBase() {
    if (mOpen != 0) Close();
}

// The two static objects the Mac's initialiser and finaliser run.
namespace {
_CS_CMpegBase sCleanup;
CMpegBase_DecodeMethod sDecodeMethod;
}  // namespace

// ---------------------------------------------------------------- synthesis and layer set-up

void CMpegBase::OpenSynth() { std::memset(mSynthBuf, 0, using_MMX == 0 ? 0x1200 : 0x900); }

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

void CMpegBase::CloseSynth() {}

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

// ---------------------------------------------------------------- decoding

int32_t CMpegBase::Decode(int16_t* out) {
    if (DecodeHeader() != 0) return -1;
    if (mLayer == 2) return Decode_Layer2(out);
    if (mLayer == 1) return Decode_Layer1(out);
    return 0;
}

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

void CMpegBase::Reset() {
    mFrameConsumed = mFrameBytes2;
    mFrameStartBit = 0;
    mBitPos = 0;
    mFramesDecoded = 0;
    mPosition = 0;
    SeekBits(0);
}

int32_t CMpegBase::Close() {
    if (mOpen != 0) {
        mOpen = 0;
        CloseLayer();
        CloseSynth();
    }
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

// The sound library's MPEG file player (unit w80c; program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp):
// CMpegMacFilePlayerUnpooled, its pool SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10> and the pool's queue
// UInterruptSafeQueue<unsigned char[28004]>. See include/CMpegBase.h for the layout.
//
// How the host differs from the Mac:
//  - The Mac started a File Manager read (PBReadAsync) into the buffer that was not being decoded and trusted it to be complete
//    by the time the decoder had used up the other one. The host has no asynchronous File Manager: RequestBuffer performs the
//    read at once through tpw::ReadFileReference (platform/file_handles.cpp, the open file named by ioRefNum), which is the only
//    order the original was correct under. The result is what the Mac's completion would have stored: ioActCount the bytes
//    read, ioPosOffset advanced by them, ioResult 0, -39 (eofErr) for a short read or -51 (rfNumErr) for an unknown reference.
//    The positioning mode is taken as fsFromStart, which is the one the player sets.
//  - Enqueue and Dequeue (the Mac OS queue calls) are written out below. The host has no interrupts, but the players are
//    returned to the pool from the audio thread, so they lock.
//  - DebugStr and SamsUtilities::UDebug are the platform layer's (they log). The sound library's own UDebug (0x10003950, 36
//    bytes) returns at once; the platform's is the one the host links, so it has no definition here.
//  - __sinit_LbMpeg_cpp (0x10003acc) constructs the file's static objects in this order: CMpegBase_DecodeMethod at 0x1003e370
//    and _CS_CMpegBase at 0x1003e360 (both in src/TbMpegBase.cpp), then gPoolQueue and gPoolCleanup (below), and registers their
//    destructors to run at exit. On the host these are the file's static objects, and the destructors run at exit.

using SamsUtilities::UInterruptSafeQueue;
using Pool = SamsUtilities::MpegFilePlayerPool;
using Queue = Pool::Queue;

namespace {

// The Pascal strings DebugStr is given (0x1002f217 and 0x1002f223).
const unsigned char kHelloWorld[] = "\x0b" "hello world";
const unsigned char kReadingSync[] = "\x0c" "reading sync";

const int16_t kEofErr = -39;
const int16_t kRfNumErr = -51;

// mullw, add: the 32-bit wrap of the Mac's arithmetic.
inline int32_t MulWrap(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
inline int32_t AddWrap(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

// The next 16-byte boundary at or after p (the Mac's (this + offset + 15) & ~15).
inline uint8_t* Align16(uint8_t* p) {
    return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(p) + 15) & ~static_cast<uintptr_t>(15));
}

// PBReadAsync and PBReadSync (InterfaceLib): the read the parameter block describes, done at once (see the top). The
// asynchronous form answers noErr (its ioResult is already final); the synchronous one answers the result.
int16_t PBReadAsync(MacFileIoParams* block) {
    const int32_t actual = tpw::ReadFileReference(block->ioRefNum, static_cast<uint32_t>(block->ioPosOffset), block->ioBuffer,
                                                  static_cast<uint32_t>(block->ioReqCount));
    block->ioActCount = actual > 0 ? actual : 0;
    block->ioPosOffset = AddWrap(block->ioPosOffset, block->ioActCount);
    block->ioResult = actual < 0 ? kRfNumErr : actual < block->ioReqCount ? kEofErr : 0;
    return 0;
}

int16_t PBReadSync(MacFileIoParams* block) {
    PBReadAsync(block);
    return block->ioResult;
}

// The Mac OS queue (Enqueue, Dequeue): append at the tail; unlink anywhere, qErr (-1) when the element is not in the queue.
std::mutex gQueueLock;

template <class Q>
void Enqueue(typename Q::Element* element, Q* queue) {
    std::lock_guard<std::mutex> guard(gQueueLock);
    element->mLink = nullptr;
    if (queue->mTail == nullptr) {
        queue->mHead = element;
    } else {
        queue->mTail->mLink = element;
    }
    queue->mTail = element;
}

template <class Q>
int16_t Dequeue(typename Q::Element* element, Q* queue) {
    std::lock_guard<std::mutex> guard(gQueueLock);
    typename Q::Element* previous = nullptr;
    for (typename Q::Element* node = queue->mHead; node != nullptr; node = node->mLink) {
        if (node == element) {
            if (previous == nullptr) {
                queue->mHead = node->mLink;
            } else {
                previous->mLink = node->mLink;
            }
            if (queue->mTail == node) queue->mTail = previous;
            return 0;
        }
        previous = node;
    }
    return -1;
}

}  // namespace

// ---------------------------------------------------------------- the player

// 0x100031e4. The two buffers are 16-aligned inside the object, the third "buffer" is the sample header's cached first 0x2000
// bytes, which is where decoding starts (mCurrent 2). The first read from the file starts after them: start + 0x2000.
int32_t CMpegMacFilePlayerUnpooled::Open(bfl::FileSystem::CFile* file, int32_t start, int32_t size, char* data) {
    mBuffers[0] = Align16(mBufferSpace);
    mBuffers[1] = mBuffers[0] + 0x2000;
    mBuffers[2] = reinterpret_cast<uint8_t*>(data);
    mCurrent = 2;
    mReadPosition = 0;
    std::memset(&mIo, 0, sizeof mIo);
    mIo.ioRefNum = static_cast<int16_t>(file->GetMacFileReference());
    mIo.ioPosMode = 1;
    mIo.ioPosOffset = AddWrap(start, 0x2000);
    if (mOpen != 0) Close();
    mOpen = 1;
    mFile = file;
    mData = Align16(mFrameSpace);
    mFrameBuffer = Align16(mFrameSpace);
    if (GetHeader() != 0) return -1;
    OpenSynth();
    OpenLayer();
    mFrames = muldiv(AddWrap(size, mT1), mT4, MulWrap(mT1, AddWrap(mT3, MulWrap(mT2, mT4))));
    mTotalBytes = MulWrap(mFrames, mFrameBytes);
    mStart = start;
    Reset();
    return 0;
}

// 0x1000339c: the first four bytes are a frame header; the answer is 0 whether it parses or not (ProcessHeader's is dropped).
int32_t CMpegMacFilePlayerUnpooled::GetHeader() {
    uint8_t word[4];
    ReadBuffered(word, 4);
    ProcessHeader(LoadBE32(word));
    return 0;
}

// 0x10003420: the next frame: its header, then, for a frame header, the frame's bytes (mFrameSize of them) into the frame
// buffer, which the decoder reads from the start. -1 when the header is not one.
int32_t CMpegMacFilePlayerUnpooled::DecodeHeader() {
    uint8_t word[4];
    ReadBuffered(word, 4);
    if (ProcessHeader(LoadBE32(word)) == -1) return -1;
    ReadBuffered(mFrameBuffer, static_cast<uint32_t>(mFrameSize));
    mBitPos = 0;
    return 0;
}

// 0x100034d4. Byte 0 restarts at the cached buffer (the file read then goes on from start + 0x2000); any other offset reads the
// current buffer from there (the Mac left a DebugStr "hello world" in that path). The next buffer is read ahead when the
// stream goes on past the cached 0x2000 bytes.
void CMpegMacFilePlayerUnpooled::SeekBits(int32_t byte) {
    if (byte == 0) {
        mCurrent = 2;
        mIo.ioPosOffset = AddWrap(mStart, 0x2000);
    } else {
        DebugStr(kHelloWorld);
        mIo.ioPosOffset = AddWrap(mStart, byte);
        RequestBuffer(mCurrent, false);
    }
    if (AddWrap(byte, 0x2000) < mTotalBytes) RequestBuffer(NextBuffer(), false);
    mReadPosition = 0;
    mBitPos = 0;
}

// 0x100035c4: the two real buffers take turns after the cached one.
int32_t CMpegMacFilePlayerUnpooled::NextBuffer() {
    if (mCurrent == 2) return 0;
    return 1 - mCurrent;
}

// 0x10003620. A read that reaches the end of the current buffer takes what is left of it, switches to the next buffer (which
// the read in flight has filled: the Mac assumed so, the host's reads are done) and, when that read got all 0x2000 bytes, starts
// the one after; a short read (the end of the file) leaves the rest of that buffer zeroed. A read that does not reach the end
// is a copy.
uint32_t CMpegMacFilePlayerUnpooled::ReadBuffered(void* out, uint32_t bytes) {
    uint8_t* destination = static_cast<uint8_t*>(out);
    int32_t position = mReadPosition;
    if (static_cast<uint32_t>(position) + bytes >= 0x2000) {
        int32_t first = 0x2000 - position;
        BlockMoveData(mBuffers[mCurrent] + position, destination, first);
        destination += first;
        bytes -= static_cast<uint32_t>(first);
        mCurrent = NextBuffer();
        mReadPosition = 0;
        int32_t filled = mIo.ioActCount;
        if (filled == 0x2000) {
            RequestBuffer(NextBuffer(), false);
        } else if (filled < 0x2000) {
            uint8_t* buffer = mBuffers[mCurrent];
            uint32_t count = static_cast<uint32_t>(0x2000 - filled);
            uint32_t blocks = count >> 3;
            if (blocks != 0) {
                do {
                    buffer[filled] = 0;
                    buffer[filled + 1] = 0;
                    buffer[filled + 2] = 0;
                    buffer[filled + 3] = 0;
                    buffer[filled + 4] = 0;
                    buffer[filled + 5] = 0;
                    buffer[filled + 6] = 0;
                    buffer[filled + 7] = 0;
                    filled += 8;
                    --blocks;
                } while (blocks != 0);
                count &= 7;
            }
            if (count != 0) {
                do {
                    buffer[filled] = 0;
                    ++filled;
                    --count;
                } while (count != 0);
            }
        }
    }
    BlockMoveData(mBuffers[mCurrent] + mReadPosition, destination, static_cast<int32_t>(bytes));
    mReadPosition = AddWrap(mReadPosition, static_cast<int32_t>(bytes));
    return bytes;
}

// 0x10003868: start the read of 0x2000 bytes into buffer `buffer`, unless one is in flight (ioResult > 0), which is logged
// and ignored. Every caller in the player asks for the asynchronous form.
void CMpegMacFilePlayerUnpooled::RequestBuffer(int32_t buffer, bool sync) {
    if (mIo.ioResult < 1) {
        mIo.ioResult = 0;
        mIo.ioBuffer = mBuffers[buffer];
        mIo.ioReqCount = 0x2000;
        mIo.ioActCount = 0;
        int16_t result;
        if (sync) {
            DebugStr(kReadingSync);
            result = PBReadSync(&mIo);
        } else {
            result = PBReadAsync(&mIo);
        }
        if (result != 0) mIo.ioResult = result;
    } else {
        SamsUtilities::UDebug("tried to load buffer before previous load completed");
    }
}

// 0x100039a0: the same as CMpegBase::Close (which the sample codec calls through its CMpegBase pointer).
int32_t CMpegMacFilePlayerUnpooled::Close() {
    if (mOpen != 0) {
        mOpen = 0;
        CloseLayer();
        CloseSynth();
    }
    return 0;
}

// ---------------------------------------------------------------- the queue

namespace SamsUtilities {

template <class Data>
UInterruptSafeQueue<Data>::Element::Element() : mLink(nullptr), mType(0) {}

template <class Data>
typename UInterruptSafeQueue<Data>::Element* UInterruptSafeQueue<Data>::Element::GetNext() {
    return mLink;
}

template <class Data>
void* UInterruptSafeQueue<Data>::Element::GetData() {
    return reinterpret_cast<uint8_t*>(this) + sizeof(Element);
}

template <class Data>
typename UInterruptSafeQueue<Data>::Element* UInterruptSafeQueue<Data>::Element::GetElementFromDataPointer(void* data) {
    return reinterpret_cast<Element*>(static_cast<uint8_t*>(data) - sizeof(Element));
}


template <class Data>
UInterruptSafeQueue<Data>::UInterruptSafeQueue() : mFlags(0), mHead(nullptr), mTail(nullptr), mCount(0) {}

template <class Data>
typename UInterruptSafeQueue<Data>::Element* UInterruptSafeQueue<Data>::GetHead() {
    return mHead;
}

template <class Data>
int32_t UInterruptSafeQueue<Data>::GetSize() {
    return mCount;
}

template <class Data>
void UInterruptSafeQueue<Data>::Add(Element* element) {
    Enqueue(element, this);
    mCount = AddWrap(mCount, 1);
}

template <class Data>
bool UInterruptSafeQueue<Data>::Remove(Element* element) {
    int16_t result = Dequeue(element, this);
    if (result == 0) mCount = AddWrap(mCount, -1);
    return result == 0;
}

template class UInterruptSafeQueue<unsigned char[sizeof(CMpegMacFilePlayerUnpooled)]>;

// ---------------------------------------------------------------- the pool

template <>
Pool::Queue Pool::gPoolQueue{};               // 0x1003e33c, constructed by __sinit_LbMpeg_cpp
template <>
Pool::StaticCleanup Pool::gPoolCleanup{};     // 0x1003e32a
template <>
uint8_t Pool::gPoolKeepFull = 0;            // 0x1003e338
template <>
uint32_t Pool::gPoolTarget = 10;            // 0x1003943c

static_assert(sizeof(Pool) == sizeof(CMpegMacFilePlayerUnpooled), "the pooled wrapper adds nothing to the player");

// 0x10007ffc: for the player's size, the first element of the queue whose player has no read in flight, taken off the queue;
// NULL when there is none (no heap fallback). Any other size goes to the heap.
// (The game tests the answer: NULL is a pool with no idle player, not an allocation failure.)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnew-returns-null"
#pragma clang diagnostic pop

}  // namespace SamsUtilities
