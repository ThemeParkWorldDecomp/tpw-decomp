// TbSoundCodecLoop.cpp: name proven by __sinit_TbSoundCodecLoop_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "helpers/TbSoundSampleWAVHelpers.h"
#include "LbTime.h"
#include "ppc_math.h"
#include "snd_containers.h"
#include "snd_handles.h"
#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include <cmath>
#include <cstring>
#include <new>

// The sound library's value classes (unit w77, program "sound symbols shared", Code at 0x10000000): TbSoundPosition,
// TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo and TbSoundSystemInfo. Declared in
// include/TbSoundPosition.h and include/TbSoundInfo.h. TbSoundSampleInfo::SetBank (0x1000df50) is in src/TbSoundBank.cpp.

// ---- TbSoundVolumeControl ----------------------------------------------------------------------------------------------

// 0x10009d9c (16 bytes)
TbSoundVolumeControl::TbSoundVolumeControl() {
    mVolumes = nullptr;
    mCount = 0;
}

CUniqueHandle::CUniqueHandle() : mValue(0) {}
CUniqueHandle::CUniqueHandle(void* value) : mValue(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value))) {}
void CLowest::operator()(uint32_t priority, TbSoundPipeChannel* channel) {
    if (priority < mPriority) {
        mPriority = priority;
        mChannel = channel;
    }
}

CAutoCriticalSection::CAutoCriticalSection() : mValue(0) {}
CAutoCriticalSection::~CAutoCriticalSection() = default;

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

namespace {

struct LoopPool {
    using Object = _TbSoundCodecLoop;
    static Object*& first() { return Object::mFirst_TbSoundCodecLoop; }
    static int32_t& current() { return Object::mCurrent_TbSoundCodecLoop; }
    static int32_t& low() { return Object::mLow_TbSoundCodecLoop; }
    static int32_t& high() { return Object::mHigh_TbSoundCodecLoop; }
    static int32_t& total() { return Object::mTotal_TbSoundCodecLoop; }
    static int32_t& level() { return Object::mLevel_TbSoundCodecLoop; }
    static Object*& link(Object* object) { return object->mLoopPoolLink; }
};

}  // namespace

void _TbSoundCodecLoop::ClearCache() { PoolClearCache<LoopPool>(); }

// ---------------------------------------------------------------- _TbSoundCodecLoop

void* _TbSoundCodecLoop::operator new(std::size_t size) { return PoolNew<LoopPool>(size); }
void _TbSoundCodecLoop::operator delete(void* object, std::size_t size) { PoolDelete<LoopPool>(object, size); }
void _TbSoundCodecLoop::TopUp() { PoolTopUp<LoopPool>(); }
void _TbSoundCodecLoop::SetLevel(int32_t level) { mLevel_TbSoundCodecLoop = level; }
// Asks the codec behind (mNext, never tested for NULL) for the rest of the buffer, always with stopped = true, and when it
// answers exactly 1 (finished) restarts it through GetCodec() (slot +0x58) and its Restart (slot +0x28). `done` is stored
// back after every round, so it is the bytes delivered so far; the loop ends when it reaches the size (signed compare) and the
// answer is 0. The argument `stopped` is not used. A codec that delivers nothing and never answers anything else would loop
// for ever (it is not tested).
int32_t _TbSoundCodecLoop::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    done = 0;
    while (done < size) {
        int32_t got = 0;
        int32_t result = mNext->GetDataChunk(static_cast<uint8_t*>(buffer) + done, size - done, got, true);
        if (result == 1) GetCodec()->Restart();
        done += got;
    }
    return 0;
}

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

void TbSoundSPCodec::Restart() {
    GetCodec()->Restart();
}

_TbSoundCodecLoop::~_TbSoundCodecLoop() {}

const uint8_t* TbSoundSPCodec::GetDataStart() {
    return GetCodec()->GetDataStart();
}

void TbSoundSPCodec::SetDataPosition(uint32_t position) {
    GetCodec()->SetDataPosition(position);
}

uint32_t TbSoundSPCodec::GetDataPosition() {
    return GetCodec()->GetDataPosition();
}

uint32_t TbSoundSPCodec::GetDataSize() {
    return GetCodec()->GetDataSize();
}

void TbSoundSPCodec::SetSampleRate(uint16_t rate) {
    GetCodec()->SetSampleRate(rate);
}

uint32_t TbSoundSPCodec::GetSampleRate() {
    return GetCodec()->GetSampleRate();
}

uint32_t TbSoundSPCodec::GetBitRate() {
    return GetCodec()->GetBitRate();
}

uint32_t TbSoundSPCodec::GetChannels() {
    return GetCodec()->GetChannels();
}

void TbSoundSPCodec::GetInfo(TbSoundSampleInfo& info) {
    GetCodec()->GetInfo(info);
}

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

extern "C" void __register_global_object(void* object, void* destructor, void* node);

uint32_t gCS_TbSoundCodecLoopObject;        // 0x1003ec18
uint32_t gCS_TbSoundCodecLoopNode;          // 0x1003ec1c

void* DestructStandInCodecLoop(void* self, int16_t) { return self; }

void __sinit_TbSoundCodecLoop_cpp() {
    __register_global_object(&gCS_TbSoundCodecLoopObject, reinterpret_cast<void*>(&DestructStandInCodecLoop),
                             &gCS_TbSoundCodecLoopNode);
}
