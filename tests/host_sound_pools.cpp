// host_sound_pools.cpp: the sound library's pools of idle objects (TbSoundSamplePipe, CMpegBase and the codecs and callbacks
// of PoolNew and its kin, include/helpers/StreamableMpeg2Helpers.h) taken from on one thread and given back on another, as
// the game takes a codec for a new sound while SDL's audio thread gives back the ones whose samples ended. Each object
// handed out is stamped all through with its own number on the "game" thread and checked on the "audio" thread before it
// goes back; a give-back that overlaps a take hands the same object out twice (a stamp overwritten under its owner), or
// breaks the free list (a crash). On the Mac the give-back ran at interrupt time: it could land inside a take, but handing one
// object out twice needs two threads running at once. The port's fix (soundpools, include/helpers/SoundPoolLock.h)
// locks the pools. The pools' functions are the real ones, cut from their src files (tests/host_sound_pools_keep.cmake). Run
// by ctest ("sound_pools") in a build with the port's fixes; a build without them makes the program, which fails. Exits
// nonzero on a failure.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"

// The pools' words, defined with much else in src/game/advisor/AdvisorScoring.cpp and src/sound/MpegDecode.cpp (TbSoundSamplePipe's
// come with its pool, src/sound/TbSoundSamplePipe.cpp).
_TbSoundSampleWAV* _TbSoundSampleWAV::mFirst_TbSoundSampleWAV = nullptr;
int32_t _TbSoundSampleWAV::mCurrent_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLow_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mHigh_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mTotal_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLevel_TbSoundSampleWAV = 10;
_TbSoundSampleWAVFile* _TbSoundSampleWAVFile::mFirst_TbSoundSampleWAVFile = nullptr;
int32_t _TbSoundSampleWAVFile::mCurrent_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLow_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mHigh_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mTotal_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLevel_TbSoundSampleWAVFile = 10;
_TbSoundCodecLoop* _TbSoundCodecLoop::mFirst_TbSoundCodecLoop = nullptr;
int32_t _TbSoundCodecLoop::mCurrent_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLow_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mHigh_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mTotal_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLevel_TbSoundCodecLoop = 10;
TbTimerCodec* TbTimerCodec::mFirstTbTimerCodec = nullptr;
int32_t TbTimerCodec::mCurrentTbTimerCodec = 0;
int32_t TbTimerCodec::mLowTbTimerCodec = 0;
int32_t TbTimerCodec::mHighTbTimerCodec = 0;
int32_t TbTimerCodec::mTotalTbTimerCodec = 0;
int32_t TbTimerCodec::mLevelTbTimerCodec = 10;
TbSoundUserCallback* TbSoundUserCallback::mFirstTbSoundUserCallback = nullptr;
int32_t TbSoundUserCallback::mCurrentTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLowTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mHighTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mTotalTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLevelTbSoundUserCallback = 10;
_TbSoundSampleCallback* _TbSoundSampleCallback::mFirst_TbSoundSampleCallback = nullptr;
int32_t _TbSoundSampleCallback::mCurrent_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLow_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mHigh_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mTotal_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLevel_TbSoundSampleCallback = 10;
_TbSoundSampleCompletedCallback* _TbSoundSampleCompletedCallback::mFirst_TbSoundSampleCompletedCallback = nullptr;
int32_t _TbSoundSampleCompletedCallback::mCurrent_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLow_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mHigh_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mTotal_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLevel_TbSoundSampleCompletedCallback = 10;
_TbSoundSampleMPEG2* _TbSoundSampleMPEG2::mFirst_TbSoundSampleMPEG2 = nullptr;
int32_t _TbSoundSampleMPEG2::mCurrent_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLow_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mHigh_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mTotal_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLevel_TbSoundSampleMPEG2 = 10;
_TbSoundSampleMPEG2File* _TbSoundSampleMPEG2File::mFirst_TbSoundSampleMPEG2File = nullptr;
int32_t _TbSoundSampleMPEG2File::mCurrent_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLow_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mHigh_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mTotal_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLevel_TbSoundSampleMPEG2File = 10;
CMpegBase* CMpegBase::mFirstCMpegBase = nullptr;
int32_t CMpegBase::mCurrentCMpegBase = 0;
int32_t CMpegBase::mLowCMpegBase = 0;
int32_t CMpegBase::mHighCMpegBase = 0;
int32_t CMpegBase::mTotalCMpegBase = 0;
int32_t CMpegBase::mLevelCMpegBase = 10;

namespace {

constexpr int kRounds = 200000;   // objects handed from one thread to the other, per pool
constexpr int kRing = 64;         // objects in flight at most (playing sounds)

struct Slot {
    void* object;
    uint32_t stamp;
};

// The objects in flight: one writer (the game thread) and one reader (the audio thread).
struct Ring {
    Slot slots[kRing];
    std::atomic<uint32_t> head{0};   // written by the game thread
    std::atomic<uint32_t> tail{0};   // written by the audio thread
};

void Stamp(void* object, std::size_t size, uint32_t stamp) {
    uint8_t* bytes = static_cast<uint8_t*>(object);
    std::size_t i = 0;
    for (; i + 4 <= size; i += 4) std::memcpy(bytes + i, &stamp, 4);
    for (; i < size; ++i) bytes[i] = static_cast<uint8_t>(stamp);
}

bool StampIntact(const void* object, std::size_t size, uint32_t stamp) {
    const uint8_t* bytes = static_cast<const uint8_t*>(object);
    std::size_t i = 0;
    for (; i + 4 <= size; i += 4) {
        uint32_t word;
        std::memcpy(&word, bytes + i, 4);
        if (word != stamp) return false;
    }
    for (; i < size; ++i)
        if (bytes[i] != static_cast<uint8_t>(stamp)) return false;
    return true;
}

// The game thread takes objects (and tops the pool up now and then, as CSprocketsModule's service does); the audio thread
// checks each one and gives it back. Answers the number of objects found shared with another owner.
template <class T>
int Hammer() {
    const std::size_t size = sizeof(T);
    T::TopUp();
    Ring ring;
    std::atomic<int> shared{0};
    std::thread audio([&] {
        for (int done = 0; done < kRounds; ++done) {
            uint32_t tail = ring.tail.load(std::memory_order_relaxed);
            while (ring.head.load(std::memory_order_acquire) == tail) std::this_thread::yield();
            Slot slot = ring.slots[tail % kRing];
            if (!StampIntact(slot.object, size, slot.stamp)) shared.fetch_add(1, std::memory_order_relaxed);
            T::operator delete(slot.object, size);
            ring.tail.store(tail + 1, std::memory_order_release);
        }
    });
    for (int round = 0; round < kRounds; ++round) {
        uint32_t head = ring.head.load(std::memory_order_relaxed);
        while (head - ring.tail.load(std::memory_order_acquire) == kRing) std::this_thread::yield();
        void* object = T::operator new(size);
        if (object == nullptr) {
            std::fprintf(stderr, "FAILED: no object of %zu bytes\n", size);
            std::exit(1);
        }
        const uint32_t stamp = 0x5A000000u + static_cast<uint32_t>(round);
        Stamp(object, size, stamp);
        ring.slots[head % kRing] = Slot{object, stamp};
        ring.head.store(head + 1, std::memory_order_release);
        if ((round & 255) == 0) T::TopUp();
    }
    audio.join();
    return shared.load();
}

int gFailures = 0;

template <class T>
void Check(const char* name) {
    const int shared = Hammer<T>();
    std::printf("%-34s %zu bytes: %d objects handed out twice\n", name, sizeof(T), shared);
    if (shared != 0) ++gFailures;
}

}  // namespace

int main() {
    Check<TbSoundSamplePipe>("TbSoundSamplePipe");
    Check<CMpegBase>("CMpegBase");
    Check<_TbSoundSampleMPEG2>("_TbSoundSampleMPEG2");
    Check<_TbSoundSampleMPEG2File>("_TbSoundSampleMPEG2File");
    Check<_TbSoundSampleWAV>("_TbSoundSampleWAV");
    Check<_TbSoundSampleWAVFile>("_TbSoundSampleWAVFile");
    Check<_TbSoundCodecLoop>("_TbSoundCodecLoop");
    Check<TbTimerCodec>("TbTimerCodec");
    Check<TbSoundUserCallback>("TbSoundUserCallback");
    Check<_TbSoundSampleCallback>("_TbSoundSampleCallback");
    Check<_TbSoundSampleCompletedCallback>("_TbSoundSampleCompletedCallback");
    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d pools handed an object out twice\n", gFailures);
        return 1;
    }
    std::printf("sound pools: OK\n");
    return 0;
}
