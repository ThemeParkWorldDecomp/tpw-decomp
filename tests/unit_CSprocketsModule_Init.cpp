// Test library for CSprocketsModule::Init (unit v5L24; libtpw_unit_CSprocketsModule_Init.dylib, tests/test_CSprocketsModule_Init.py),
// "sound symbols shared":
//   src/CSprocketsModule_Init.cpp
// with src/TbSoundSystemModule.cpp, whose Init runs for real on both sides (the PowerPC side runs the original 0x10012340).
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: the array allocation (AllocBuffer_), the
// sound library's allocator, the channel constructor, the three command constructors, SetImmediate, the channel send,
// TbSpeaker::Stereo's constructor, SetSpeakerConfig (the virtual call), SetPoolKeepFull and Tick. The vtable of CSprocketsModule
// is emitted here (the destructor is its key function), so the other overrides are dummies; the virtual SetSpeakerConfig
// reaches the recording one through it.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbSoundInfo.h"
#include "TbSoundPipeChannel.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

namespace {

alignas(16) unsigned char gModuleMem[64];
alignas(16) unsigned char gInfoMem[256];
CSprocketsChannel** gArray = nullptr;   // the module's mChannels (Channels() and SetChannels)

int64_t I(int64_t value) { return value; }

int64_t Bits(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    return bits;
}

CSprocketsModule* Module() { return reinterpret_cast<CSprocketsModule*>(gModuleMem); }

void* Fill(uint32_t size) {
    void* p = tpw::ArenaAllocZeroed(size < 8 ? 8 : size);
    std::memset(p, 0xa5, size);
    return p;
}

}  // namespace

// ---- the class: its key function, the constructor and the dummy overrides (the vtable lands here)
CSprocketsModule::~CSprocketsModule() {}
CSprocketsModule::CSprocketsModule() {}
TbError CSprocketsModule::SetOptions(uint32_t) { return TbError(0); }
void CSprocketsModule::Close() {}
TbError CSprocketsModule::StreamData(TbSoundPipeChannel*) { return TbError(0); }
TbError CSprocketsModule::StopChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::StopAll() { return TbError(0); }
TbError CSprocketsModule::PauseChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::RestartChannel(uint32_t) { return TbError(0); }
TbError CSprocketsModule::PauseAll() { return TbError(0); }
TbError CSprocketsModule::RestartAll() { return TbError(0); }
TbError CSprocketsModule::Set3DDSPFunctionality(uint32_t, uint32_t) { return TbError(0); }
TbError CSprocketsModule::SetSpeedOfSound(float) { return TbError(0); }
TbError CSprocketsModule::GetPosition(uint32_t, TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerVelocity(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerPosition(const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetListenerDirection(const TbSoundPosition&, const TbSoundPosition&) { return TbError(0); }
TbError CSprocketsModule::SetChannelInfo(uint32_t, TbSoundPipeChannel*, TbSoundSampleInfo&) { return TbError(0); }
TbError CSprocketsModule::SetChannelVolume(uint32_t, TbSoundSampleInfo&) { return TbError(0); }

// The virtual call of Init (Mac slot +0x54): the configuration's label; the answer is dropped.
TbError CSprocketsModule::SetSpeakerConfig(const TbSpeaker::Config& config) {
    ev("SetSpeakerConfig", {I(lab(&config))});
    return TbError(static_cast<int32_t>(ret("Config")));
}

void CSprocketsModule::Tick() { ev("Tick", {}); }
CSprocketsChannel** CSprocketsModule::Channels() const { return gArray; }
void CSprocketsModule::SetChannels(CSprocketsChannel** channels) { gArray = channels; }

// The base class's member (0x1001493c; src/snd_handles.cpp is not compiled here).
TbSpeaker::AutoDelete::~AutoDelete() {}

template <>
void SamsUtilities::MpegFilePlayerPool::SetPoolKeepFull(uint8_t flag) {
    ev("SetPoolKeepFull", {I(flag)});
}

// ---- the allocations. The sizes are recorded in the Mac's terms: the array in 4-byte slots, the channel as 0x4c when it is the
// native object's size; any other size is marked (0x10000000) so that it never matches the Mac's.
void* AllocBuffer_(uint32_t bytes) {
    ev("AllocBuffer", {I(bytes % sizeof(void*) == 0 ? bytes / sizeof(void*) : (0x10000000u | bytes))});
    if (ret("AllocBufferNull") != 0) return nullptr;
    void* p = Fill(bytes);
    newlab(p);
    return p;
}

void* SamsUtilities::UAllocatorMake(int32_t size) {
    ev("Alloc", {I(size == static_cast<int32_t>(sizeof(CSprocketsChannelMac)) ? 0x4c : (0x10000000 | size))});
    if (ret("AllocNull") != 0) return nullptr;
    void* p = Fill(static_cast<uint32_t>(size));
    newlab(p);
    return p;
}

CSprocketsChannelMac::CSprocketsChannelMac() { ev("ChannelCtor", {I(lab(this))}); }

void TbSpeaker::StereoConstruct(void* stereo) { ev("Stereo", {I(newlab(stereo))}); }

// ---- the commands
namespace SamsUtilities {
UCommandSoundDistance::UCommandSoundDistance(double distance) { ev("SoundDistance", {I(newlab(this)), Bits(distance)}); }
UCommandListenerScale::UCommandListenerScale(double x, double y, double z) {
    ev("ListenerScale", {I(newlab(this)), Bits(x), Bits(y), Bits(z)});
}
UCommandCPULoad::UCommandCPULoad(double load) { ev("CPULoad", {I(newlab(this)), Bits(load)}); }
void UCommand::SetImmediate(uint8_t immediate) { ev("SetImmediate", {I(lab(this)), I(immediate)}); }

UStatus SendToChannel(CSprocketsChannelMac* channel, UCommand& command) {
    ev("Send", {I(lab(channel)), I(lab(&command))});
    return UStatus(static_cast<int32_t>(ret("Send")));
}
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_csi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_csi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CSprocketsModule));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

// The world: a module in place with its words; mChannels starts as a pointer that is no label.
void tpw_csi_reset(uint32_t buffers, uint32_t f008, uint32_t rate, uint64_t channels) {
    CSprocketsModule* module = new (gModuleMem) CSprocketsModule();
    module->mBuffers = buffers;
    module->mF008 = f008;
    module->mRate = rate;
    gArray = reinterpret_cast<CSprocketsChannel**>(static_cast<uintptr_t>(channels));
}

// The module's words and the array: out[0..2] mBuffers, mF008, mRate; out[3] the array's label; out[4..] the label of each of
// the first n slots (only when the array is there).
void tpw_csi_state(int32_t n, int64_t* out) {
    out[0] = Module()->mBuffers;
    out[1] = Module()->mF008;
    out[2] = Module()->mRate;
    out[3] = tpw_cl_lab(gArray);
    for (int32_t i = 0; i < n; i++) out[4 + i] = gArray != nullptr ? tpw_cl_lab(gArray[i]) : 0;
}

int32_t tpw_csi_Init(uint32_t mask, uint32_t maxBuffers, uint32_t buffersToUse, uint32_t field0C, uint32_t field10) {
    std::memset(gInfoMem, 0, sizeof(gInfoMem));
    TbSoundSystemInfo* info = reinterpret_cast<TbSoundSystemInfo*>(gInfoMem);
    info->mMask = mask;
    info->mMaxBuffers = maxBuffers;
    info->mBuffersToUse = buffersToUse;
    info->mFieldC = field0C;
    info->mField10 = field10;
    return Module()->CSprocketsModule::Init(*info).mValue;
}

}  // extern "C"
