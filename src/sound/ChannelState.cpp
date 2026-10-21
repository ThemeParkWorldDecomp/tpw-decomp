// ChannelState.cpp: name proven by __sinit_ChannelState_cpp in the sound library.

#include "CSprocketsModule.h"
#include "snd_handles.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstring>
#include <new>

void* TbSpeaker::AutoDelete::_mpSpeaker = nullptr;
uint32_t CUniqueHandle::_mBitsPerCount = 26;
CChannelState CChannelState::mChannelState;

TbSpeaker::AutoDelete::~AutoDelete() {
    if (_mpSpeaker != nullptr) {
        ::operator delete(_mpSpeaker);
        _mpSpeaker = nullptr;
    }
}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

// ---- the speaker -----------------------------------------------------------------------------------------------------------
namespace TbSpeaker {
// 0x100149e8 (12): stores the module in _mpSpeaker (0x1003f348).
void ConfigSetModuleSpeaker(Module* speaker) { AutoDelete::_mpSpeaker = speaker; }

// Each calls a virtual slot of the module _mpSpeaker points to (no test for NULL), through the glue 0x1002f154.
Stereo::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->StereoValue(); }
Headphones::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->HeadphonesValue(); }
Mono::operator int() const { return static_cast<Module*>(AutoDelete::_mpSpeaker)->MonoValue(); }
}  // namespace TbSpeaker

// TbSoundCore.cpp: the sound library's core (unit w79b, sound symbols shared). See include/TbSoundCore.h for the layouts, the
// vtables and the calling convention. The sample store (unit w79c), the device module (CSprocketsModule, unit p-sound) and the
// command bodies (unit w79d) are other units'.

// ---------------------------------------------------------------- the copier

CMemoryCopy::CCopy* CMemoryCopy::_mpCopy = nullptr; // 0x1003f34c

CMemoryCopyInit::CMemoryCopyInit() {
    CMemoryCopy::_mpCopy = new CMemoryCopy::CStandardCopy();
}
CMemoryCopy::CStandardCopy::CStandardCopy() : CCopy() {}

CMemoryCopy::CCopy::CCopy() {}

void CMemoryCopy::CStandardCopy::Copy(void* dest, const void* source, uint32_t count) {
    std::memcpy(dest, source, count);
}

// ---- __sinit_ChannelState_cpp 0x10014d1c ------------------------------------------------------------------------------------
void __sinit_ChannelState_cpp() { new (&CChannelState::mChannelState) CChannelState(); }

const char* NameText(const TbDynamicStringTemplate<char>& name);
__attribute__((weak)) const char* NameText(const TbDynamicStringTemplate<char>& name) { return name.mData; }

void TbRectConstruct(TbRect* rect) {
    rect->left = 0;
    rect->top = 0;
    rect->right = 0;
    rect->bottom = 0;
}

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

CAtomicValue::operator int32_t() const { return mValue; }
CChannelState::CChannelState() : mCount() {}
