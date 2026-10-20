// Unit v51112: small functions of the sound program's AudioPlaceHolder, TbSoundSample*, CSprocketsModule ranges (see the header block).
#include "AudioPlaceHolder.h"

#include <cmath>
#include <new>

#include "LbTime.h"
#include "TbMpegBase.h"
#include "TbSoundCodecs.h"
#include "bfl_FileSystem.h"
#include "snd_containers.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// ---- random ------------------------------------------------------------------------------------------------------------------

// LbMaths_RangedRandomSlow__FRlUl 0x10018c88 (56): divwu by zero is undefined on the hardware; the emulator answers 0, so the
// remainder is the random value itself.
uint32_t LbMaths_RangedRandomSlow(int32_t& seed, uint32_t range) {
    const uint32_t value = static_cast<uint32_t>(LbMaths_Random(seed));
    const uint32_t quotient = range != 0 ? value / range : 0;
    return value - quotient * range;
}

namespace bfl::maths {
// 0x10018c30 (32)
uint32_t RangedRandomSlow(int32_t& seed, uint32_t range) { return LbMaths_RangedRandomSlow(seed, range); }
// 0x10019ad0 (32)
uint32_t Random(int32_t& seed) { return static_cast<uint32_t>(LbMaths_Random(seed)); }
}  // namespace bfl::maths

namespace SamsUtilities::UBinary {
// RotateRight<long> 0x10018d6c (20): sraw | slw (shift counts use six bits: from 32 up, sraw fills with the sign and slw gives 0).
template <>
int32_t RotateRight<int32_t>(int32_t value, int32_t amount) {
    const uint32_t right = static_cast<uint32_t>(amount) & 0x3f;
    const uint32_t left = (0x20u - static_cast<uint32_t>(amount)) & 0x3f;
    const int32_t a = right > 31 ? (value < 0 ? -1 : 0) : (value >> right);
    const uint32_t b = left > 31 ? 0u : (static_cast<uint32_t>(value) << left);
    return static_cast<int32_t>(static_cast<uint32_t>(a) | b);
}
}  // namespace SamsUtilities::UBinary

// ---- LB_MIN / LB_MAX ---------------------------------------------------------------------------------------------------------

const uint32_t& LbMaxULong(const uint32_t& a, const uint32_t& b) {
    if (a > b) return a;
    return b;
}
const float& LbMinFloat(const float& a, const float& b) {
    if (a < b) return a;
    return b;
}
const uint32_t& LbMinUInt(const uint32_t& a, const uint32_t& b) {
    if (a < b) return a;
    return b;
}
const int32_t& LbMaxLong(const int32_t& a, const int32_t& b) {
    if (a > b) return a;
    return b;
}
const int32_t& LbMinInt(const int32_t& a, const int32_t& b) {
    if (a < b) return a;
    return b;
}

TbRect* TbRectAssign(TbRect* dest, const TbRect* source) {
    dest->left = source->left;
    dest->top = source->top;
    dest->right = source->right;
    dest->bottom = source->bottom;
    return dest;
}

void* OperatorNewPlacementSound(uint32_t, void* where) { return where; }

namespace TbSoundSampleStoreSound {
TbSoundPipeChannel* IteratorIncrement(ChannelIterator* self, uint32_t elementSize) {
    uint8_t* next = reinterpret_cast<uint8_t*>(self->mCurrent) + elementSize;
    self->mCurrent = reinterpret_cast<TbSoundPipeChannel*>(next);
    if (next < reinterpret_cast<uint8_t*>(self->mEnd)) return self->mCurrent;
    return nullptr;
}
}  // namespace TbSoundSampleStoreSound

void* CFormatToWaveFormatEx(CFormat* self) {
    self->FillWaveFormatEx();
    return reinterpret_cast<uint8_t*>(self) + 4;
}

namespace tbstd {
float logf(float x) { return static_cast<float>(::log(static_cast<double>(x))); }
}  // namespace tbstd

// ---- CAutoArrayPtr<CAudioPlaceHolder*> ---------------------------------------------------------------------------------------

// The runtime's heap glue (new[] 0x1002ebfc, delete[] 0x1002ebcc, delete 0x1002ea94): faked by the test library.
extern "C" void* SoundHeapNewPointers(uint32_t count);
extern "C" void SoundHeapDeleteArray(void* block);
extern "C" void SoundHeapDelete(void* block);

// Lead 2026-10-20 (v51112): the host bodies of that glue and of the seed's (0x1002ebfc is operator new[], 0x1002ebcc operator
// delete[], 0x1002ea94 operator delete and 0x1002eaac LbTime_GetClock, as src/AudioPlaceHolderList.cpp says). They are weak,
// so the test library's recording fakes take their place there; the launcher has no other definition of these unmangled names.
extern "C" __attribute__((weak)) void* SoundHeapNewPointers(uint32_t count) { return ::operator new[](count * sizeof(void*)); }
extern "C" __attribute__((weak)) void SoundHeapDeleteArray(void* block) { ::operator delete[](block); }
extern "C" __attribute__((weak)) void SoundHeapDelete(void* block) { ::operator delete(block); }
extern "C" __attribute__((weak)) int32_t MslSeedGlue1002eaac() { return static_cast<int32_t>(LbTime_GetClock()); }

CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderCtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, uint32_t count) {
    self->mData = static_cast<CAudioPlaceHolder**>(SoundHeapNewPointers(count));
    return self;
}

CAutoArrayPtr<CAudioPlaceHolder*>* CAutoArrayPtrPlaceHolderDtor(CAutoArrayPtr<CAudioPlaceHolder*>* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mData != nullptr) SoundHeapDeleteArray(self->mData);
        if (deleteFlag > 0) SoundHeapDelete(self);
    }
    return self;
}

CAudioPlaceHolder** CAutoArrayPtrPlaceHolderGet(const CAutoArrayPtr<CAudioPlaceHolder*>* self) { return self->mData; }

// ---- files -------------------------------------------------------------------------------------------------------------------

int32_t CFileWriteMain(bfl::FileSystem::CFile* self, const void* source, uint32_t size) {
    if (self->mImplementation == nullptr) return -1;
    return self->mImplementation->Write(source, size);
}

namespace bfl::FileSystem {
// 0x10020c50 (104)
CDiscFileStorage* CDiscFileStorage::DestroySound(CDiscFileStorage* self, int16_t deleteFlag) {
    if (self) {
        self->CFileStorage::~CFileStorage();
        if (deleteFlag > 0) SoundHeapDelete(self);
    }
    return self;
}
}  // namespace bfl::FileSystem

// ---- static initialisers -----------------------------------------------------------------------------------------------------

uint32_t gCS_TbSoundSampleWAVObject;        // 0x1003f2cc
uint32_t gCS_TbSoundSampleWAVNode;          // 0x1003f2d0
uint32_t gCS_TbSoundSampleMPEG2Object;      // 0x1003f260
uint32_t gCS_TbSoundSampleMPEG2Node;        // 0x1003f264
uint32_t gCS_TbSoundSampleCallbackObject;   // 0x1003ec3c
uint32_t gCS_TbSoundSampleCallbackNode;     // 0x1003ec40
uint32_t gCS_TbSoundCodecLoopObject;        // 0x1003ec18
uint32_t gCS_TbSoundCodecLoopNode;          // 0x1003ec1c

// Stand-ins: the real destructors (0x1000ed30, 0x1000d608, 0x1000b590) are not translated; only their addresses are registered.
void* DestructStandInMPEG2(void* self, int16_t) { return self; }
void* DestructStandInCallback(void* self, int16_t) { return self; }
void* DestructStandInCodecLoop(void* self, int16_t) { return self; }

void __sinit_TbSoundSampleWAV_cpp() {
    __register_global_object(&gCS_TbSoundSampleWAVObject, reinterpret_cast<void*>(&_CS__TbSoundSampleWAV::Destruct),
                             &gCS_TbSoundSampleWAVNode);
}
void __sinit_TbSoundSampleMPEG2_cpp() {
    __register_global_object(&gCS_TbSoundSampleMPEG2Object, reinterpret_cast<void*>(&DestructStandInMPEG2),
                             &gCS_TbSoundSampleMPEG2Node);
}
void __sinit_TbSoundSampleCallback_cpp() {
    __register_global_object(&gCS_TbSoundSampleCallbackObject, reinterpret_cast<void*>(&DestructStandInCallback),
                             &gCS_TbSoundSampleCallbackNode);
}
void __sinit_TbSoundCodecLoop_cpp() {
    __register_global_object(&gCS_TbSoundCodecLoopObject, reinterpret_cast<void*>(&DestructStandInCodecLoop),
                             &gCS_TbSoundCodecLoopNode);
}

void __sinit_AudioPlaceHolder_cpp() { CAudioPlaceHolder::mRandomSeed = MslSeedGlue1002eaac(); }
