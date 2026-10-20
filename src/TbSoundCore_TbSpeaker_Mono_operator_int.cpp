// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.
#include "TbSoundCore.h"
#include "TbSoundCodecs.h"
#include <cstring>
#include <new>
#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "snd_handles.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1002eefc here

// ---- the speaker -----------------------------------------------------------------------------------------------------------
namespace TbSpeaker {
// 0x100149e8 (12): stores the module in _mpSpeaker (0x1003f348).
void ConfigSetModuleSpeaker(Module* speaker) { AutoDelete::_mpSpeaker = speaker; }

// Each calls a virtual slot of the module _mpSpeaker points to (no test for NULL), through the glue 0x1002f154.
Stereo::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->StereoValue(); }
Headphones::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->HeadphonesValue(); }
Mono::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->MonoValue(); }
}  // namespace TbSpeaker

// ---- __sinit_ChannelState_cpp 0x10014d1c ------------------------------------------------------------------------------------
void __sinit_ChannelState_cpp() { new (&CChannelState::mChannelState) CChannelState(); }

// ---- Bank::CBank ------------------------------------------------------------------------------------------------------------
namespace Bank {
bool CBank::IsType(uint32_t type) const {
    if (mBank == nullptr) return false;
    return mBank->IsType(type);
}
uint32_t CBank::GetSampleLength(uint32_t index) const {
    if (mBank == nullptr) return 0;
    return mBank->GetSampleLength(index);
}
const char* CBank::GetSampleName(uint32_t index) const {
    if (mBank == nullptr) return nullptr;
    return mBank->GetSampleName(index);
}
}  // namespace Bank

CRefManager::CRefManager() : mOwner(nullptr) {}

bool TbStaticFileBank::IsType(uint32_t type) const { return type == 3; }

const char* NameText(const TbDynamicStringTemplate<char>& name);
__attribute__((weak)) const char* NameText(const TbDynamicStringTemplate<char>& name) { return name.mData; }

void TbRectConstruct(TbRect* rect) {
    rect->left = 0;
    rect->top = 0;
    rect->right = 0;
    rect->bottom = 0;
}

// ---- the static cleanup objects --------------------------------------------------------------------------------------------
uint32_t gCS_TbSoundUserCallbackObject;    // 0x1003f338
uint32_t gCS_TbSoundUserCallbackNode;      // 0x1003f33c
uint32_t gCS_TbTimerCodecObject;           // 0x1003f314
uint32_t gCS_TbTimerCodecNode;             // 0x1003f318
uint32_t gCS_TbSoundSampleWAVFileObject;   // 0x1003f2f0
uint32_t gCS_TbSoundSampleWAVFileNode;     // 0x1003f2f4

_CS_TbTimerCodec* _CS_TbTimerCodec::Destruct(_CS_TbTimerCodec* self, int16_t deleteFlag) {
    if (self != nullptr) {
        TbTimerCodec::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

_CS__TbSoundSampleWAVFile* _CS__TbSoundSampleWAVFile::Destruct(_CS__TbSoundSampleWAVFile* self, int16_t deleteFlag) {
    if (self != nullptr) {
        _TbSoundSampleWAVFile::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

_CS__TbSoundSampleWAV* _CS__TbSoundSampleWAV::Destruct(_CS__TbSoundSampleWAV* self, int16_t deleteFlag) {
    if (self != nullptr) {
        _TbSoundSampleWAV::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

// Stand-in: 0x10013f2c is not translated.
_CS_TbSoundUserCallback* DestructUserCallbackCleanup(_CS_TbSoundUserCallback* self, int16_t) { return self; }

void __sinit_UserCallback_cpp() {
    __register_global_object(&gCS_TbSoundUserCallbackObject, reinterpret_cast<void*>(&DestructUserCallbackCleanup),
                             &gCS_TbSoundUserCallbackNode);
}
void __sinit_TimerCodec_cpp() {
    __register_global_object(&gCS_TbTimerCodecObject, reinterpret_cast<void*>(&_CS_TbTimerCodec::Destruct), &gCS_TbTimerCodecNode);
}
void __sinit_TbSoundSampleWAVFile_cpp() {
    __register_global_object(&gCS_TbSoundSampleWAVFileObject, reinterpret_cast<void*>(&_CS__TbSoundSampleWAVFile::Destruct),
                             &gCS_TbSoundSampleWAVFileNode);
}

// ---- math ------------------------------------------------------------------------------------------------------------------
namespace tbstd {
float powf(float a, float b) { return static_cast<float>(MslPow(a, b)); }   // frsp of the runtime's pow
float fabs(float x) { return fabsf(x); }
float fabsf(float x) { return static_cast<float>(fabs(static_cast<double>(x))); }
double fabs(double x) { return __builtin_fabs(x); }
}  // namespace tbstd
