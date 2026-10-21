// Test library for the small Sound Sprockets pieces of CSprocketsModule.cpp (unit v51107; libtpw_unit_SamsUtilities_UCommand_SetImmediate.dylib,
// tests/test_SamsUtilities_UCommand_SetImmediate.py), main app:
//   src/sound/CSprocketsModule.cpp, src/sound/SprocketsChannel.cpp   the flag helpers, the command constructors, the channel accessors, ...
//
// Callees outside the unit: UManager<UAllocator>::Allocate and the base destructor TbSoundSystemModule::~TbSoundSystemModule are recording
// fakes. The base class's other virtuals are empty definitions here so that CSprocketsModule's vtable links.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbGraphicFile.h"

using namespace calllog;

namespace {
uint64_t gDtorObject = 0;
}

// ---- the fakes
template <>
void SamsUtilities::UManager<SamsUtilities::UAllocator>::Allocate() {
    ev("Allocate", {0});
    mObject = (ret("Allocate") & 1) ? tpw::ArenaAllocZeroed(8) : nullptr;
}

TbSoundSystemModule::~TbSoundSystemModule() {
    ev("BaseDtor", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) == gDtorObject), 0});
}

// CSprocketsModule's vtable is emitted by the destructor translated here, so the overrides it names must link.
TbError CSprocketsModule::RestartAll() { return TbError(0); }
TbError CSprocketsModule::Init(const TbSoundSystemInfo&) { return TbError(0); }
void CSprocketsModule::Close() {}
TbError CSprocketsModule::StreamData(TbSoundPipeChannel*) { return TbError(0); }
TbError CSprocketsModule::StopChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::StopAll() { return TbError(0); }
TbError CSprocketsModule::PauseChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::RestartChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::PauseAll() { return TbError(0); }
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config&) { return TbError(0); }
TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(0); }
TbError CSprocketsModule::GetPosition(uint32_t, TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerVelocity(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerDirection(const TbSoundPosition&, const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetChannelInfo(uint32_t, TbSoundPipeChannel*, TbSoundSampleInfo&) { return TbError(0); }
TbError CSprocketsModule::SetChannelVolume(uint32_t, TbSoundSampleInfo&) { return TbError(0); }

// The base's vtable is emitted by the recording destructor above: its other virtuals are empty definitions.
void TbSoundSystemModule::Process() {}
uint32_t TbSoundSystemModule::GetDirectSoundObject(IDirectSound*&) { return 0; }
void TbSoundSystemModule::GetDirectSoundBuffer(uint32_t, IDirectSoundBuffer*&) {}
void* TbSoundSystemModule::GetSystemObject(uint32_t) { return nullptr; }
TbError TbSoundSystemModule::SetOptions(uint32_t) { return TbError(0); }
TbError TbSoundSystemModule::Init(const TbSoundSystemInfo&) { return TbError(0); }
TbError TbSoundSystemModule::StopAll() { return TbError(0); }
TbError TbSoundSystemModule::PauseChannel(uint32_t) { return TbError(0); }
TbError TbSoundSystemModule::RestartChannel(uint32_t) { return TbError(0); }
TbError TbSoundSystemModule::PauseAll() { return TbError(0); }
TbError TbSoundSystemModule::RestartAll() { return TbError(0); }
bool TbSoundSystemModule::IsChannelPlaying(uint32_t) { return false; }
bool TbSoundSystemModule::SupportsEnvironmentalEffects() { return false; }
TbError TbSoundSystemModule::SetLoPass(uint32_t, float) { return TbError(0); }
TbError TbSoundSystemModule::SetDistanceFactor(uint32_t, const TbSoundDistance&) { return TbError(0); }
TbError TbSoundSystemModule::SetSpeakerConfig(const TbSpeaker::Config&) { return TbError(0); }
TbError TbSoundSystemModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(0); }
TbError TbSoundSystemModule::SetSpeedOfSound(float) { return TbError(0); }
TbError TbSoundSystemModule::SetPanRate(int32_t) { return TbError(0); }
TbError TbSoundSystemModule::SetPosition(uint32_t, const TbSoundPosition&, int32_t) { return TbError(0); }
TbError TbSoundSystemModule::GetPosition(uint32_t, TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::SetVelocity(uint32_t, const TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::GetVelocity(uint32_t, TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::SetListenerVelocity(const TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::SetListenerPosition(const TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::SetListenerDirection(const TbSoundPosition&, const TbSoundPosition&) { return TbError(0); }
TbError TbSoundSystemModule::SetPitch(uint32_t, uint32_t, int32_t) { return TbError(0); }
TbError TbSoundSystemModule::SetFrequency(uint32_t, uint32_t, float) { return TbError(0); }
TbError TbSoundSystemModule::SetChannelInfo(uint32_t, TbSoundPipeChannel*, TbSoundSampleInfo&) { return TbError(0); }
TbError TbSoundSystemModule::SetReverbEnvironment(uint32_t, float, float, float) { return TbError(0); }
TbError TbSoundSystemModule::SetReverbPosition(const TbSoundPosition&) { return TbError(0); }

TbSpeaker::AutoDelete::~AutoDelete() {}   // the base's member, a Mac import not under test

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_v51107_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// HOST_LAYOUT: the host offsets of the fields the test writes by name.
int32_t tpw_v51107_layout(int64_t* out) {
    using namespace SamsUtilities;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mPipe));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, m3D));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mFlag49));
    out[n++] = static_cast<int64_t>(offsetof(CSprocketsChannelMac, mDescription));
    out[n++] = static_cast<int64_t>(offsetof(UReferenceCounter<USoundDescription>, mRest));
    out[n++] = static_cast<int64_t>(sizeof(UReferenceCounter<USoundDescription>));
    out[n++] = static_cast<int64_t>(offsetof(UReferenceCounted, mCount));
    return n;
}

// One entry for every function; the object is a raw block of the Mac's words.
uint64_t tpw_v51107_call(int32_t op, uint64_t obj, uint32_t a, uint32_t b, uint32_t c, uint32_t d, double dbl) {
    using namespace SamsUtilities;
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    gDtorObject = obj;
    switch (op) {
        case 0: static_cast<UCommand*>(p)->SetImmediate(static_cast<uint8_t>(a)); return 0;
        case 1: UHasFlags_ClearFlag(p, a); return 0;
        case 2: UHasFlags_SetFlag(p, a); return 0;
        case 3: UHasFlags_Construct(p); return 0;
        case 4: UHasFlags_ConstructWith(p, a); return 0;
        case 5: UCommand_Construct(static_cast<UCommand*>(p)); return 0;
        case 6: UCommand_ConstructFlags(static_cast<UCommand*>(p), a); return 0;
        case 7: return reinterpret_cast<uintptr_t>(static_cast<USoundDescription*>(*static_cast<UReferenceCounter<USoundDescription>*>(p)));
        case 8: new (p) UCommandPlay(); return 0;
        case 9: static_cast<CSprocketsChannelMac*>(p)->SetPipe(reinterpret_cast<void*>(static_cast<uintptr_t>(a))); return 0;
        case 10: return reinterpret_cast<uintptr_t>(static_cast<CSprocketsChannelMac*>(p)->GetDescription());
        case 11: return static_cast<UReferenceCounted*>(p)->AddReference();
        case 12:
            USoundDescriptionCounter_Make(static_cast<UReferenceCounter<USoundDescription>*>(p),
                                          reinterpret_cast<USoundDescription*>(static_cast<uintptr_t>((static_cast<uint64_t>(b) << 32) | a)));
            return 0;
        case 13: return static_cast<CSprocketsChannelMac*>(p)->Is3D();
        case 14: new (p) CSprocketsSpeaker(); return 0;
        case 15: TbSpeaker::ModuleConstruct(p); return 0;
        case 16: static_cast<CSprocketsModule*>(p)->CSprocketsModule::~CSprocketsModule(); return 0;
        case 17: TbSpeaker::StereoConstruct(p); return 0;
        case 18: TbSpeaker::ConfigConstruct(p); return 0;
        case 19: new (p) UCommandSoundDistance(dbl); return 0;
        case 20: return reinterpret_cast<uintptr_t>((*static_cast<UManager<UAllocator>*>(p)).operator->());
        case 21: return static_cast<uint32_t>(static_cast<int32_t>(static_cast<CSprocketsModule*>(p)->CSprocketsModule::SetOptions(a)));
        case 22: return static_cast<uint32_t>(static_cast<int32_t>(static_cast<CSprocketsModule*>(p)->CSprocketsModule::Set3DDSPFunctionality(a, b)));
        case 23: return static_cast<uint32_t>(static_cast<const UStatus*>(p)->IfOk(TbLegacyError{static_cast<int32_t>(a)}, TbLegacyError{static_cast<int32_t>(b)}));
        case 24: new (p) UCommandStop(); return 0;
        case 25: TbRect_Construct(static_cast<TbRect*>(p), static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<int32_t>(c), static_cast<int32_t>(d)); return 0;
        case 26: new (p) UCommandPause(a); return 0;
    }
    return 0;
}

}  // extern "C"
