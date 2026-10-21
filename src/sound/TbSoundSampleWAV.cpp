// TbSoundSampleWAV.cpp: name proven by __sinit_TbSoundSampleWAV_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "helpers/TbSoundSampleWAVHelpers.h"
#include "LbTime.h"
#include "snd_containers.h"
#include "snd_handles.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipeChannel.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's pipe channel (unit w79c, program "sound symbols shared"): TbSoundPipeChannel, the head of a sample's pipe
// chain (TbSoundCodecLoop.cpp, LbMpeg.cpp, TbSoundSampleCallback.cpp, TbSoundSampleWAV.cpp, TimerCodec.cpp and
// CSprocketsModule.cpp hold its out-of-line copies). See include/TbSoundPipeChannel.h for the layout and the slot order.

void TbSoundPipeChannel::SetIndex(uint16_t index) {
    mHandle.SetIndex(index);
}

int32_t CAtomicValue::operator++() {
    mValue = static_cast<int32_t>(static_cast<uint32_t>(mValue) + 1u);
    return mValue;
}

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

namespace {

// ---------------------------------------------------------------- the four pools
// The four classes have the same allocation code (the POOLED pattern) with their own statics, size and link word. Each trait
// names the statics of one class; the template functions below are the Mac's operator new, operator delete, TopUp and
// ClearCache of that class. (The Mac code is a separate copy per class: the listings differ only in the addresses, the size
// and the offset of the link.)

struct WAVPool {
    using Object = _TbSoundSampleWAV;
    static Object*& first() { return Object::mFirst_TbSoundSampleWAV; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleWAV; }
    static int32_t& low() { return Object::mLow_TbSoundSampleWAV; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleWAV; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleWAV; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleWAV; }
    static Object*& link(Object* object) { return object->mWAVPoolLink; }
};

}  // namespace

void _TbSoundSampleWAV::ClearCache() { PoolClearCache<WAVPool>(); }

// ---------------------------------------------------------------- _TbSoundSampleWAV

void* _TbSoundSampleWAV::operator new(std::size_t size) { return PoolNew<WAVPool>(size); }
void _TbSoundSampleWAV::operator delete(void* object, std::size_t size) { PoolDelete<WAVPool>(object, size); }
void _TbSoundSampleWAV::TopUp() { PoolTopUp<WAVPool>(); }
void _TbSoundSampleWAV::SetLevel(int32_t level) { mLevel_TbSoundSampleWAV = level; }
// Not timed: copies min(size, bytes left) from the data at the position and advances it. At the end of the data the timer
// is started for the bytes just copied plus 0x1200 (SetTime through the virtual slot +0x5c; a stopped codec starts none), and
// the answer is 1 unless the timer was started; the next call is then timed. Timed: nothing is copied, `done` is 0 and the
// answer is whether the time is over. The compare of the position with the size is unsigned, the minimum is signed (a
// position past the size gives a negative count that the copy takes as a huge unsigned one).
int32_t _TbSoundSampleWAV::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) {
    if (Timed() != 0) {
        done = 0;
        return IsTimeComplete();
    }
    int32_t left = static_cast<int32_t>(mDataSize - mDataPosition);
    done = LB_MIN<int32_t>(size, left);
    CMemoryCopy copier;
    copier.Copy(buffer, mDataStart + mDataPosition, static_cast<uint32_t>(done));
    mDataPosition += static_cast<uint32_t>(done);
    if (mDataPosition >= mDataSize) {
        if (!SetTime(stopped, static_cast<uint32_t>(done + 0x1200))) return 1;
    }
    return 0;
}

// ---------------------------------------------------------------- CMemoryCopy

// Slot +8 of the global copier (CStandardCopy::Copy 0x10014cac) is memcpy(dest, source, count).
void CMemoryCopy::Copy(void* dest, const void* source, uint32_t count) {
    std::memcpy(dest, source, count);
}

_TbSoundSampleWAV::~_TbSoundSampleWAV() {}

void _TbSoundSampleWAV::Restart() {
    mDataPosition = 0;
}

void _TbSoundSampleWAV::SetDataStart(void* start) {
    mDataStart = static_cast<const uint8_t*>(start);
}

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// ---- static initialisers -----------------------------------------------------------------------------------------------------

uint32_t gCS_TbSoundSampleWAVObject;        // 0x1003f2cc
uint32_t gCS_TbSoundSampleWAVNode;          // 0x1003f2d0
void __sinit_TbSoundSampleWAV_cpp() {
    __register_global_object(&gCS_TbSoundSampleWAVObject, reinterpret_cast<void*>(&_CS__TbSoundSampleWAV::Destruct),
                             &gCS_TbSoundSampleWAVNode);
}
