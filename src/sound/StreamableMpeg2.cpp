// StreamableMpeg2.cpp: name proven by __sinit_StreamableMpeg2_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "helpers/StreamableMpeg2Helpers.h"
#include "LbTime.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleMPEG2.h"
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>

// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.

namespace {

struct MPEG2FilePool {
    using Object = _TbSoundSampleMPEG2File;
    static Object*& first() { return Object::mFirst_TbSoundSampleMPEG2File; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleMPEG2File; }
    static int32_t& low() { return Object::mLow_TbSoundSampleMPEG2File; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleMPEG2File; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleMPEG2File; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleMPEG2File; }
    static Object*& link(Object* object) { return object->mMPEG2FilePoolLink; }
};

}  // namespace

void _TbSoundSampleMPEG2File::ClearCache() { PoolClearCache<MPEG2FilePool>(); }

// ---------------------------------------------------------------- _TbSoundSampleMPEG2File

void* _TbSoundSampleMPEG2File::operator new(std::size_t size) { return PoolNew<MPEG2FilePool>(size); }
void _TbSoundSampleMPEG2File::operator delete(void* object, std::size_t size) { PoolDelete<MPEG2FilePool>(object, size); }
void _TbSoundSampleMPEG2File::TopUp() { PoolTopUp<MPEG2FilePool>(); }
void _TbSoundSampleMPEG2File::SetLevel(int32_t level) { mLevel_TbSoundSampleMPEG2File = level; }
// The memory codec's constructor, then the file, the start (the arguments are file, header, start, owner) and the header's data
// word (the cached first bytes of the sample). The pool link (+0x40) is not written.
_TbSoundSampleMPEG2File::_TbSoundSampleMPEG2File(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start,
                                                 TbRefRelease* owner)
    : _TbSoundSampleMPEG2(header, owner), mFile(file), mStart(start), mData(header->mData) {}

_TbSoundSampleMPEG2::~_TbSoundSampleMPEG2() {}

// Opens the file player (a CMpegMacFilePlayerUnpooled in its pooled wrapper, the allocation 0x6d64 bytes on the Mac) over the
// bank's file: the file, the start, the length and the cached first bytes. The rest is the memory codec's.
TbError _TbSoundSampleMPEG2File::OpenMpegFile() {
    if (mMpeg != nullptr) return TbError(0);
    using Player = SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>;
    void* block = Player::operator new(sizeof(Player));
    Player* player = nullptr;
    if (block != nullptr) player = ::new (block) Player();
    mMpeg = player;
    int32_t opened = player->Open(mFile, static_cast<int32_t>(mStart), static_cast<int32_t>(mDataSize), static_cast<char*>(mData));
    if (opened != -1) return FinishOpen();
    return TbError(-1);
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

using Pool = SamsUtilities::MpegFilePlayerPool;
using Queue = Pool::Queue;

// ---------------------------------------------------------------- the queue

namespace SamsUtilities {

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

}  // namespace SamsUtilities

// The bytes of the sample left from the file's position (mDataSize - (position - mStart), unsigned) cap the count; the minimum
// is tested as a SIGNED number (a position outside the sample makes it huge or zero) and fails with -1.
int32_t _TbSoundSampleMPEG2File::Read(void* buffer, uint32_t count) {
    uint32_t position = static_cast<uint32_t>(mFile->GetPos());
    uint32_t left = mDataSize - (position - mStart);
    const uint32_t& wanted = LB_MIN<uint32_t, uint32_t>(count, left);   // 0x100081c4 (include/snd_handles.h)
    if (static_cast<int32_t>(wanted) <= 0) return -1;
    return mFile->Read(buffer, wanted);
}

int32_t _TbSoundSampleMPEG2File::Seek(int32_t offset) {
    int32_t position = mFile->Seek(mStart + static_cast<uint32_t>(offset), 0);
    return static_cast<int32_t>(static_cast<uint32_t>(position) - mStart);
}
int32_t _TbSoundSampleMPEG2File::GetMpegBlock(void* buffer, int32_t bytes) {
    if (mMpeg == nullptr) return 0;
    return mMpeg->Read(buffer, bytes);
}

void _TbSoundSampleMPEG2File::CloseMpegFile() {
    if (mMpeg != nullptr) {
        mMpeg->Close();
        if (mMpeg != nullptr) delete mMpeg;
        mMpeg = nullptr;
    }
}

// No priming of a layer I stream, unlike the memory codec's.
void _TbSoundSampleMPEG2File::Restart() {
    mDataPosition = 0;
    mMpeg->Reset();
}

void _TbSoundSampleMPEG2File::SetNewDataPosition() {}

_TbSoundSampleMPEG2File::~_TbSoundSampleMPEG2File() {}

// The decoder's stream moves with the codec's (no test for a missing decoder).
void _TbSoundSampleMPEG2::SetDataStart(void* start) {
    mDataStart = static_cast<const uint8_t*>(start);
    mMpeg->mData = static_cast<const uint8_t*>(start);
}

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

uint32_t TbSoundSampleCodec::GetDataPosition() {
    return mDataPosition;
}

const uint8_t* TbSoundSampleCodec::GetDataStart() {
    return mDataStart;
}

uint32_t TbSoundSampleCodec::GetDataSize() {
    return mDataSize;
}
void TbSoundSampleCodec::GetInfo(TbSoundSampleInfo& info) {
    TbSoundSamplePipe::GetInfo(info);
}

TbSoundSPCodec* TbSoundSPCodec::GetCodec() {
    return static_cast<TbSoundSPCodec*>(mNext);
}

// Unit v51045: TbStaticFileBank::RegisterBank and three static initialisers of the sound library.

extern uint32_t gSinitDtor_10033d3c[2], gSinitDtor_10033d34[2], gSinitDtor_1003352c[2], gSinitDtor_100336dc[2];

uint32_t gSinit_1003ebe8[1], gSinit_1003ebec[3], gSinit_1003ebf8[3], gSinit_1003ebc8[3];
uint8_t gPoolQueueBuilt;     // 0x1003e329
uint8_t gPoolCleanupBuilt;   // 0x1003e328

// 0x10008760 (140)
void __sinit_StreamableMpeg2_cpp() {
    __register_global_object(gSinit_1003ebe8, gSinitDtor_100336dc, gSinit_1003ebec);
    if (static_cast<int8_t>(gPoolQueueBuilt) == 0) {
        new (&Pool::gPoolQueue) Pool::Queue();
        gPoolQueueBuilt = 1;
    }
    if (static_cast<int8_t>(gPoolCleanupBuilt) == 0) {
        __register_global_object(&Pool::gPoolCleanup, gSinitDtor_1003352c, gSinit_1003ebc8);
        gPoolCleanupBuilt = 1;
    }
}
