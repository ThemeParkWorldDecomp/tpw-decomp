// Test library for CSprocketsModule's constructor, Close, Tick, StreamData, SetChannelInfo and SetListenerDirection (unit v5997;
// libtpw_unit_CSprocketsModule_SetListenerDirection_2.dylib, tests/test_CSprocketsModule_SetListenerDirection_2.py), "sound symbols shared":
//   src/TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_CSprocketsModule_SetListenerDirection_2.cpp
//
// The callees outside the unit are fakes that record what the PowerPC stubs record. The vtable of CSprocketsModule is emitted by
// the unit's source (the destructor is its key function), so every other override is a dummy here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbMpegBase.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include "calllog.h"
#include "platform/arena.h"
#include "../platform/sound_module.h"

using namespace calllog;

// 0x1002b374 (100) is not translated: the key function of the class, so the vtable lands here for the test library. In the
// launcher platform/sound_module.cpp owns it.
CSprocketsModule::~CSprocketsModule() {}

namespace {

alignas(16) unsigned char gModuleMem[64];
alignas(16) unsigned char gInfoMem[256];
CSprocketsChannel* gChannelArray[8];
bool gHasArray = false;
void* gLastAlloc = nullptr;

int64_t I(int64_t value) { return value; }

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// A double as two unsigned halves, every NaN made one value.
void CanonD(double value, int64_t& hi, int64_t& lo) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    if (((bits >> 52) & 0x7ff) == 0x7ff && (bits & 0xfffffffffffffull) != 0) bits = 0x7ff8000000000000ull;
    hi = static_cast<int64_t>(bits >> 32);
    lo = static_cast<int64_t>(bits & 0xffffffffull);
}

uint32_t KindOf(const SamsUtilities::UCommand& command) {
    uint32_t kind;
    std::memcpy(&kind, command.mStorage, 4);
    return kind;
}

void SetKind(SamsUtilities::UCommand& command, uint32_t kind) { std::memcpy(command.mStorage, &kind, 4); }

uint32_t PipeTag(const void* pipe) {
    uint32_t tag;
    std::memcpy(&tag, static_cast<const char*>(pipe) + 0x70, 4);
    return tag;
}

CSprocketsModule* Module() { return reinterpret_cast<CSprocketsModule*>(gModuleMem); }

void Vec3(const char* name, const SamsUtilities::UCommand& command, double x, double y, double z) {
    int64_t xh, xl, yh, yl, zh, zl;
    CanonD(x, xh, xl);
    CanonD(y, yh, yl);
    CanonD(z, zh, zl);
    ev(name, {I(KindOf(command)), xh, xl, yh, yl, zh, zl});
}

}  // namespace

// ---- the pools and the import
void _TbSoundSampleMPEG2::TopUp() { ev("TopUp", {1}); }
void _TbSoundSampleCallback::TopUp() { ev("TopUp", {2}); }
void CMpegBase::TopUp() { ev("TopUp", {3}); }
void _TbSoundSampleMPEG2File::TopUp() { ev("TopUp", {4}); }
void TbTimerCodec::TopUp() { ev("TopUp", {5}); }
void TbSoundUserCallback::TopUp() { ev("TopUp", {6}); }
void TbSoundSamplePipe::TopUp() { ev("TopUp", {7}); }
void _TbSoundCodecLoop::TopUp() { ev("TopUp", {8}); }
void _TbSoundSampleCompletedCallback::TopUp() { ev("TopUp", {9}); }

template <>
void SamsUtilities::MpegFilePlayerPool::ClearPool(uint8_t all) {
    ev("ClearPool", {I(all)});
}
template <>
void SamsUtilities::MpegFilePlayerPool::MaintainPoolLevel() {
    ev("MaintainPoolLevel", {0});
}

void SoundTickImport_() { ev("Idle", {0}); }
void DisposeBuffer_(void* buffer) { ev("DisposeBuffer", {I(gHasArray && buffer == static_cast<void*>(gChannelArray))}); }

void* SamsUtilities::UAllocatorMake(int32_t size) {
    ev("Alloc", {I(size)});
    if (ret("AllocNull") != 0) {
        gLastAlloc = nullptr;
        return nullptr;
    }
    gLastAlloc = tpw::ArenaAllocZeroed(size < 8 ? 8 : static_cast<uint32_t>(size));
    return gLastAlloc;
}

// ---- the speaker and the base class
CSprocketsSpeaker::CSprocketsSpeaker() { ev("SpeakerCtor", {I(static_cast<void*>(this) == gLastAlloc)}); }

void TbSpeaker::ConfigSetModuleSpeaker(Module* speaker) {
    void* p = static_cast<void*>(speaker);
    ev("SetModuleSpeaker", {p == nullptr ? 0 : (p == gLastAlloc ? 1 : 2)});
}

// The base class's member (0x1001493c, src/snd_handles.cpp is not compiled here); the destructor only runs if a module is destroyed.
TbSpeaker::AutoDelete::~AutoDelete() {}

// ---- the other overrides: dummies, only for the vtable the unit's source emits
TbError CSprocketsModule::SetOptions(uint32_t) { return TbError(0); }
TbError CSprocketsModule::Init(const TbSoundSystemInfo&) { return TbError(0); }
TbError CSprocketsModule::StopChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::StopAll() { return TbError(0); }
TbError CSprocketsModule::PauseChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::RestartChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::PauseAll() { return TbError(0); }
TbError CSprocketsModule::RestartAll() { return TbError(0); }
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config&) { return TbError(0); }
TbError CSprocketsModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(0); }
TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(0); }
TbError CSprocketsModule::GetPosition(uint32_t, TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerVelocity(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetChannelVolume(uint32_t, TbSoundSampleInfo&) { return TbError(0); }

CSprocketsChannel** CSprocketsModule::Channels() const { return gHasArray ? gChannelArray : nullptr; }

// ---- the pipe
uint32_t TbSoundPipeChannel::GetIndex() {
    ev("GetIndex", {I(PipeTag(this))});
    return static_cast<uint32_t>(ret("GetIndex"));
}
uint32_t TbSoundPipeChannel::GetSampleFlags() {
    ev("GetSampleFlags", {I(PipeTag(this))});
    return static_cast<uint32_t>(ret("GetSampleFlags"));
}
TbSampleHeader* TbSoundPipeChannel::GetSampleHeader() {
    ev("GetSampleHeader", {I(PipeTag(this))});
    return reinterpret_cast<TbSampleHeader*>(reinterpret_cast<char*>(this) + 0x40);
}

void TbSoundPosition::Get(float& x, float& y, float& z) const {
    ev("PositionGet", {CanonBits(mX), CanonBits(mY), CanonBits(mZ)});
    x = mX;
    y = mY;
    z = mZ;
}

// ---- the host channel (the inline members SetPipe, Set3D and Is3D run for real)
CSprocketsChannel::CSprocketsChannel(tpw::SoundDevice* device, uint32_t index)
    : mDevice(device), mIndex(index), mPipe(nullptr), mChunk(0), mIs3D(0), mLocation{}, mVelocity{} {}
CSprocketsChannel::~CSprocketsChannel() { ev("ChannelDtor", {I(mIndex), 1}); }
void CSprocketsChannel::Play() { ev("Play", {I(mIndex)}); }
bool CSprocketsChannel::GetDescription() const {
    ev("GetDescription", {I(mIndex)});
    return ret("GetDescription") != 0;
}
void CSprocketsChannel::SetDescription(const TbSampleHeader& header) {
    ev("SetDescription", {I(mIndex), I(static_cast<uint32_t>(header.mSize)), I(static_cast<uint32_t>(header.mField8)), I(header.mFieldC)});
}

// ---- the commands
namespace SamsUtilities {
UCommandListenerPosition::UCommandListenerPosition(uint32_t on) {
    SetKind(*this, 4);
    ev("ListenerPosition", {I(on)});
}
UCommandPreroll::UCommandPreroll(uint32_t on) {
    SetKind(*this, 1);
    ev("Preroll", {I(on)});
}
UCommandPosition::UCommandPosition(uint32_t on) {
    SetKind(*this, 3);
    ev("Position", {I(on)});
}
UCommandVolume::UCommandVolume(double volume) {
    SetKind(*this, 2);
    int64_t hi, lo;
    CanonD(volume, hi, lo);
    ev("Volume", {hi, lo});
}
void UCommand::SetImmediate(uint8_t immediate) { ev("SetImmediate", {I(KindOf(*this)), I(immediate)}); }
void UCommand3dPosition::SetLocation(double x, double y, double z) { Vec3("SetLocation", *this, x, y, z); }
void UCommand3dPosition::SetVelocity(double x, double y, double z) { Vec3("SetVelocity", *this, x, y, z); }
void UCommand3dPosition::SetOrientation(double x, double y, double z) { Vec3("SetOrientation", *this, x, y, z); }
void UCommand3dPosition::SetUpVector(double x, double y, double z) { Vec3("SetUpVector", *this, x, y, z); }

// UStatus's default constructor and assignment are translated in unit v51053; plain copies here.
UStatus::UStatus() { mValue = 0; }
UStatus& UStatus::operator=(const UStatus& other) {
    mValue = other.mValue;
    return *this;
}

int32_t UStatus::IfOk(TbLegacyError ok, TbLegacyError failed) const {
    ev("IfOk", {I(mValue), I(ok.mValue), I(failed.mValue)});
    return static_cast<int32_t>(static_cast<uint32_t>(ret("IfOk")));
}
bool UStatus::IsSet() const {
    ev("IsSet", {I(mValue)});
    return ret("IsSet") != 0;
}

UStatus SendToChannel(CSprocketsChannel* channel, UCommand& command) {
    ev("Send", {I(channel->Index()), I(KindOf(command))});
    UStatus status;
    status.mValue = static_cast<int32_t>(static_cast<uint32_t>(ret("Send")));
    return status;
}
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_csm_addr(const char* name) {
    if (std::strcmp(name, "module") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gModuleMem));
    return 0;
}

uint64_t tpw_csm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_csm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CSprocketsModule));
    out[n++] = static_cast<int64_t>(sizeof(TbSampleHeader));
    return n;
}

// The world: the module's counts and the array of eight channels (a bit of mask: the channel exists).
void tpw_csm_reset(int32_t buffers, int32_t hasArray, uint32_t mask) {
    Module()->mBuffers = static_cast<uint32_t>(buffers);
    Module()->mF008 = 0x11111111u;
    Module()->mRate = 0x22222222u;
    gLastAlloc = nullptr;
    gHasArray = hasArray != 0;
    for (int i = 0; i < 8; i++) {
        gChannelArray[i] = nullptr;
        if (gHasArray && ((mask >> i) & 1) != 0) gChannelArray[i] = new CSprocketsChannel(nullptr, static_cast<uint32_t>(i));
    }
}

void tpw_csm_chan_init(int32_t index, uint32_t is3D, uint64_t pipe) {
    if (gChannelArray[index] == nullptr) return;
    gChannelArray[index]->Set3D(static_cast<uint8_t>(is3D));
    gChannelArray[index]->SetPipe(reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(pipe)));
}

void tpw_csm_chan_state(int32_t index, int64_t* out) {
    out[0] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(gChannelArray[index]->Pipe()));
    out[1] = gChannelArray[index]->Is3D();
}

void tpw_csm_module_words(int64_t* out) {
    out[0] = Module()->mBuffers;
    out[1] = Module()->mF008;
    out[2] = Module()->mRate;
}

uint64_t tpw_csm_info(uint32_t field0C, uint32_t flags, uint32_t volume, uint64_t position, uint64_t velocity) {
    std::memset(gInfoMem, 0, sizeof(gInfoMem));
    TbSoundSampleInfo* info = reinterpret_cast<TbSoundSampleInfo*>(gInfoMem);
    info->mField0C = field0C;
    info->mFlags = flags;
    info->mVolume = volume;
    info->mPosition = reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(position));
    info->mVelocity = reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(velocity));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gInfoMem));
}

uint32_t tpw_csm_info_flags() { return reinterpret_cast<TbSoundSampleInfo*>(gInfoMem)->mFlags; }

// ---- the functions
int32_t tpw_csm_ctor() {
    CSprocketsModule* module = new (gModuleMem) CSprocketsModule();
    return module == Module() ? 1 : 0;
}
void tpw_csm_Close() { Module()->CSprocketsModule::Close(); }
void tpw_csm_Tick() { Module()->CSprocketsModule::Tick(); }
int32_t tpw_csm_StreamData(uint64_t pipe) {
    return Module()->CSprocketsModule::StreamData(reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(pipe)));
}
int32_t tpw_csm_SetChannelInfo(uint32_t channel, uint64_t pipe, uint64_t info) {
    return Module()->CSprocketsModule::SetChannelInfo(channel, reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(pipe)),
                                                      *reinterpret_cast<TbSoundSampleInfo*>(static_cast<uintptr_t>(info)));
}
int32_t tpw_csm_SetListenerDirection(uint64_t front, uint64_t top) {
    return Module()->CSprocketsModule::SetListenerDirection(*reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(front)),
                                                            *reinterpret_cast<const TbSoundPosition*>(static_cast<uintptr_t>(top)));
}

}  // extern "C"
