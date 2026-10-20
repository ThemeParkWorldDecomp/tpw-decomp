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
#include "CMpegBase.h"

#include <cstring>
#include <mutex>
#include <new>

#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"

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

// lwz of four bytes in the Mac's (big-endian) order.
inline uint32_t LoadBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
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

// 0x10003b98 (the deleting form frees the cleanup object itself).
template <>
Pool::StaticCleanup::~StaticCleanup() { ClearPool(1); }

// 0x10007d1c: the player's constructor, then the wrapper's vtable.
template <>
Pool::UPooled() {}

// 0x10007ffc: for the player's size, the first element of the queue whose player has no read in flight, taken off the queue;
// NULL when there is none (no heap fallback). Any other size goes to the heap.
// (The game tests the answer: NULL is a pool with no idle player, not an allocation failure.)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnew-returns-null"
template <>
void* Pool::operator new(std::size_t size) {
    if (size != sizeof(Pool)) return ::operator new(size);
    for (Queue::Element* element = gPoolQueue.GetHead(); element != nullptr; element = element->GetNext()) {
        void* player = element->GetData();
        if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
            return element->GetData();
        }
    }
    return nullptr;
}
#pragma clang diagnostic pop

// 0x10007e20: a player goes back to the end of the queue; anything else to the heap.
template <>
void Pool::operator delete(void* object, std::size_t size) {
    if (size == sizeof(Pool)) {
        Queue::Element* element = Queue::Element::GetElementFromDataPointer(object);
        gPoolQueue.Add(element);
    } else {
        ::operator delete(object);
    }
}

// 0x10003c58: free the elements whose player has no read in flight. `all` restarts from the head after each (so a player that
// stays busy at the head never lets it end); otherwise one pass.
template <>
void Pool::ClearPool(uint8_t all) {
    ReportPoolStatistics();
    if (all == 0) {
        Queue::Element* element = gPoolQueue.GetHead();
        while (element != nullptr) {
            Queue::Element* next = element->GetNext();
            void* player = element->GetData();
            if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
                ::operator delete(element);
            }
            element = next;
        }
    } else {
        Queue::Element* element;
        while ((element = gPoolQueue.GetHead()) != nullptr) {
            void* player = element->GetData();
            if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
                ::operator delete(element);
            }
        }
    }
}

// 0x10003f40: ioResult is not positive (no read in flight).
template <>
bool Pool::IsPoolObjectAvailable(const CMpegMacFilePlayerUnpooled* player) { return player->mIo.ioResult <= 0; }

// 0x10004040: empty.
template <>
void Pool::ReportPoolStatistics() {}

// 0x1002b5ec.
template <>
void Pool::SetPoolKeepFull(uint8_t flag) { gPoolKeepFull = flag; }

// 0x1002b964. Fill the queue up to the target (one element, or, with keep-full, until it is there or an element cannot be
// made); then, while it holds more than the target, free idle elements from the head (one only, unless keep-full).
template <>
void Pool::MaintainPoolLevel() {
    while (static_cast<uint32_t>(gPoolQueue.GetSize()) < gPoolTarget) {
        Queue::Element* element = MakeNewPoolElement();
        if (element == nullptr) break;
        gPoolQueue.Add(element);
        if (gPoolKeepFull == 0) break;
    }
    Queue::Element* element = gPoolQueue.GetHead();
    while (element != nullptr) {
        if (static_cast<uint32_t>(gPoolQueue.GetSize()) <= gPoolTarget) break;
        Queue::Element* next = element->GetNext();
        void* player = element->GetData();
        if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
            ::operator delete(element);
            if (gPoolKeepFull == 0) return;
        }
        element = next;
    }
}

// 0x1002bb08: an element (0x6d6c bytes on the Mac: 8 and the player's 0x6d64) with its player cleared through the allocator
// (UManager<UAllocator>::operator-> makes the allocator on first use, 0x1002bbe4).
template <>
Pool::Queue::Element* Pool::MakeNewPoolElement() {
    Queue::Element* element = static_cast<Queue::Element*>(::operator new(sizeof(Queue::Element) + sizeof(CMpegMacFilePlayerUnpooled)));
    if (element != nullptr) {
        ::new (element) Queue::Element();
        void* data = element->GetData();
        if (gMemory == nullptr) gMemory = new UAllocator;
        gMemory->Clear(data, sizeof(CMpegMacFilePlayerUnpooled));
    }
    return element;
}

}  // namespace SamsUtilities
