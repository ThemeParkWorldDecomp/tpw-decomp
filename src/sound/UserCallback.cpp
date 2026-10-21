// UserCallback.cpp: name proven by __sinit_UserCallback_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "CSprocketsModule.h"
#include "helpers/TbSoundSampleCallbackHelpers.h"
#include "snd_handles.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstring>
#include <new>

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

namespace {

// The three pools have the allocation code of the other pooled classes (see src/TbSoundSampleMPEG2.cpp, which keeps its own copy of
// these templates): a separate listing per class that differs only in the statics, the size and the offset of the link.

struct UserCallbackPool {
    using Object = TbSoundUserCallback;
    static Object*& first() { return Object::mFirstTbSoundUserCallback; }
    static int32_t& current() { return Object::mCurrentTbSoundUserCallback; }
    static int32_t& low() { return Object::mLowTbSoundUserCallback; }
    static int32_t& high() { return Object::mHighTbSoundUserCallback; }
    static int32_t& total() { return Object::mTotalTbSoundUserCallback; }
    static int32_t& level() { return Object::mLevelTbSoundUserCallback; }
    static Object*& link(Object* object) { return object->mUserCallbackPoolLink; }
};

}  // namespace

void TbSoundUserCallback::ClearCache() { PoolClearCache<UserCallbackPool>(); }

// ---------------------------------------------------------------- TbSoundUserCallback

void* TbSoundUserCallback::operator new(std::size_t size) { return PoolNew<UserCallbackPool>(size); }
void TbSoundUserCallback::operator delete(void* object, std::size_t size) { PoolDelete<UserCallbackPool>(object, size); }
void TbSoundUserCallback::TopUp() { PoolTopUp<UserCallbackPool>(); }
void TbSoundUserCallback::SetLevel(int32_t level) { mLevelTbSoundUserCallback = level; }
// The chain's answer first (the pipe's own SetInfo, not a virtual call); a negative one is -1, else the user data is taken from
// the info when it carries it.
TbError TbSoundUserCallback::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundSamplePipe::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mFlags & 0x1000) != 0) mUserData = info.mUserData;
    return TbError(0);
}

// The info's request word (+0x18, which include/TbSoundInfo.h calls mField18) asks for the user data with 0x1000.
void TbSoundUserCallback::GetInfo(TbSoundSampleInfo& info) {
    if ((info.mField18 & 0x1000) != 0) info.SetUserData(mUserData);
}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1002eefc here

// ---- the static cleanup objects --------------------------------------------------------------------------------------------
uint32_t gCS_TbSoundUserCallbackObject;    // 0x1003f338
uint32_t gCS_TbSoundUserCallbackNode;      // 0x1003f33c
_CS_TbSoundUserCallback* DestructUserCallbackCleanup(_CS_TbSoundUserCallback* self, int16_t);

void __sinit_UserCallback_cpp() {
    __register_global_object(&gCS_TbSoundUserCallbackObject, reinterpret_cast<void*>(&DestructUserCallbackCleanup),
                             &gCS_TbSoundUserCallbackNode);
}
