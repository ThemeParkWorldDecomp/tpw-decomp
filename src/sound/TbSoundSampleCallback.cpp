// TbSoundSampleCallback.cpp: name proven by __sinit_TbSoundSampleCallback_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "helpers/TbSoundSampleCallbackHelpers.h"
#include "LbFile.h"
#include "LbTime.h"
#include "PlaceHolderHelpers.h"
#include "ppc_math.h"
#include "snd_commands.h"
#include "snd_containers.h"
#include "TbMapStreamer.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"
#include <cmath>
#include <cstring>
#include <new>

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

namespace {

struct SampleCallbackPool {
    using Object = _TbSoundSampleCallback;
    static Object*& first() { return Object::mFirst_TbSoundSampleCallback; }
    static int32_t& current() { return Object::mCurrent_TbSoundSampleCallback; }
    static int32_t& low() { return Object::mLow_TbSoundSampleCallback; }
    static int32_t& high() { return Object::mHigh_TbSoundSampleCallback; }
    static int32_t& total() { return Object::mTotal_TbSoundSampleCallback; }
    static int32_t& level() { return Object::mLevel_TbSoundSampleCallback; }
    static Object*& link(Object* object) { return object->mSampleCallbackPoolLink; }
};

// The Mac's 32-bit signed arithmetic on the byte counts (wraps instead of overflowing).
int32_t Add32(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

int32_t Sub32(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

}  // namespace

void _TbSoundSampleCallback::ClearCache() { PoolClearCache<SampleCallbackPool>(); }

// ---------------------------------------------------------------- _TbSoundSampleCallback

void* _TbSoundSampleCallback::operator new(std::size_t size) { return PoolNew<SampleCallbackPool>(size); }
void _TbSoundSampleCallback::operator delete(void* object, std::size_t size) { PoolDelete<SampleCallbackPool>(object, size); }
void _TbSoundSampleCallback::TopUp() { PoolTopUp<SampleCallbackPool>(); }
void _TbSoundSampleCallback::SetLevel(int32_t level) { mLevel_TbSoundSampleCallback = level; }
// The user callback's SetInfo (not the pipe's), then the callback of the info when it carries one (flag 0x800).
TbError _TbSoundSampleCallback::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundUserCallback::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mFlags & 0x800) != 0) mCallback = info.mCallback;
    return TbError(0);
}

void _TbSoundSampleCallback::GetInfo(TbSoundSampleInfo& info) {
    TbSoundUserCallback::GetInfo(info);
    if ((info.mField18 & 0x800) != 0) info.SetCallback(mCallback);
}

// The guard detaches this pipe at the end of the call once a sample has finished (the destructor of the guard runs on both exits;
// the "nothing behind" exit never set it). The chunk is zeroed before every call, so a pipe that answers 1 without writing it
// delivers nothing; done starts at 0 and the loop test is a signed compare. mNext is read again for every round (GetNextSample
// replaces it).
int32_t _TbSoundSampleCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    CAutoDetach guard(this);
    int32_t chunk = 0;
    if (mNext == nullptr) return 1;
    done = 0;
    while (done < size) {
        chunk = 0;
        int32_t result = mNext->GetDataChunk(static_cast<uint8_t*>(buffer) + done, Sub32(size, done), chunk, true);
        if (result == 1) {
            guard = true;
            GetNextSample(static_cast<uint32_t>(Add32(chunk, done)));
        }
        done = Add32(done, chunk);
    }
    return 0;
}

CAutoDetach& CAutoDetach::operator=(bool detach) {
    mDetach = detach;
    return *this;
}

// The pipe detaches itself through the virtual slot +0x14 (the pipe's Detatch: it unlinks and deletes the pipe).
CAutoDetach::~CAutoDetach() {
    if (mDetach) mPipe->Detatch();
}
// ---------------------------------------------------------------- CAutoDetach

CAutoDetach::CAutoDetach(TbSoundSamplePipe* pipe) : mPipe(pipe), mDetach(false) {}

// The format byte comes from the old pipe (not tested for NULL: GetDataChunk only comes here with one behind). mNext is cleared
// BEFORE the callback runs, so that an Attatch by the callback does not release the old pipe; the old pipe is released after.
void _TbSoundSampleCallback::GetNextSample(uint32_t position) {
    uint8_t format = static_cast<TbSoundSampleCodec*>(mNext)->GetSampleFormat();
    TbSoundSamplePipe* old = mNext;
    mNext = nullptr;
    mCallback->SoundCallback(mHandle, mUserData);
    if (old != nullptr) old->Release();
    if (mNext == nullptr) {
        void* block = TbTimerCodec::operator new(sizeof(TbTimerCodec));
        TbTimerCodec* timer = nullptr;
        if (block != nullptr) timer = ::new (block) TbTimerCodec(format, position);
        mNext = timer;
    }
}

// AudioPlaceHolder.cpp: CAudioPlaceHolder, the base of every sound the game is playing (unit w79g2, "sound symbols shared", file
// AudioPlaceHolder.cpp of the library). See include/AudioPlaceHolder.h for the layout, the vtable and the flag bits.
//
// Conventions of this file. The sound manager is reached through the class static mpSoundManager (0x1003f360), never tested for
// NULL except where the listing tests it. Every command to the manager is built on the stack and handed to its virtual
// SystemCommand (slot +0x18); the answer is dropped unless the function says otherwise. The file words of Save and Load are the
// Mac's SamsUtilities swaps: the file is little-endian, so the host (little-endian) writes and reads the native words (the helpers of
// include/TbSoundBank.h do nothing here). The random numbers come from bfl::maths::Random and RangedRandomSlow (stand-ins), always
// given a COPY of mRandomSeed. All float arithmetic is single precision in the listing's order; the build has -ffp-contract=off.

void TbSndCallback::SoundCallback(void*, void*) {}

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

uint8_t TbSoundSampleCodec::GetSampleFormat() const {
    return mFormat;
}

_TbSoundSampleCallback::~_TbSoundSampleCallback() {}

// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).

extern "C" void __register_global_object(void* object, void* destructor, void* node);

uint32_t gCS_TbSoundSampleCallbackObject;   // 0x1003ec3c
uint32_t gCS_TbSoundSampleCallbackNode;     // 0x1003ec40
void* DestructStandInCallback(void* self, int16_t) { return self; }
void __sinit_TbSoundSampleCallback_cpp() {
    __register_global_object(&gCS_TbSoundSampleCallbackObject, reinterpret_cast<void*>(&DestructStandInCallback),
                             &gCS_TbSoundSampleCallbackNode);
}
