// Fakes and test entry point for unit w79d, the SystemCommand Execute bodies of the sound library
// (libtpw_unit_SoundCommands.dylib, driven by tests/test_SoundCommands.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/SystemCommand.cpp  the fourteen functions under test (13 Execute bodies and SetSampleInfo::SetChannelInfo)
// Real: the command constructors (w79b, src/sound/AudioPlaceHolder.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSysCommand.cpp), the info records and positions (w77), the device-module base and
// the handle values (w79a), the pipe base (w78) and CAudioComponentAccess's constructor (w79b).
// Fakes, each recording what the original's callee is given (the PowerPC side records the same from its stubs):
//   the access object (the core): GetSystemModule and GetSampleStore, and TbSoundCore::StopAndReleasePipe (w79b)
//   the device module: every slot the commands call (a TbSoundSystemModule subclass answering TbError from queues)
//   the sample store (w79c): GetValidChannelIndexFromHandle, both operator[], SetNumberOfChannelsToUse, StopAllChannels,
//       StopAllChannelsWithVolumeControl and the channel iterator
//   the channels (w79c): SetPriority, SetVolume, GetVolume, GetIndex, ReleasePipe and the virtual SetInfo
//   SamsUtilities::UDebug (the re-entry message)
//
// Labels: 1 the access object (the core), 2 the module, 3 the store, 4 to 7 the four channels, 8 the command's sample info, 9 the
// system info, 10 the listener position, 11 the front direction, 12 the top direction, 13 the listener velocity, 14 the speaker
// config. A channel is chosen by a selector: 0 NULL, 1 to 4 the channels 4 to 7.
//
// tpw_sx_run builds a world from the parameter words, runs one function and dumps the observable state.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include "calllog.h"
#include "platform/samsutils.h"
#include "snd_commands.h"

using namespace calllog;

namespace {

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

float bitsToFloat(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

// A float as recorded: its bits, with every NaN as one value (the sign of a generated NaN is the platform's).
int64_t FB(float value) {
    if (value != value) return 0x7FC00000;
    return fbits(value);
}

// The system info is 19 Mac words; the pointer words (+0x2c to +0x3c) are the labels of the case's objects.
TbSoundSystemInfo gSys;
TbSoundSampleInfo gSamp;
TbSoundPosition gPos[4];   // the listener position, the front and top directions, the listener velocity
alignas(16) uint8_t gConfigBytes[16];
alignas(16) uint8_t gStoreBytes[sizeof(TbSoundSampleStore) + 16];
alignas(16) uint8_t gChannelBytes[4][sizeof(TbSoundPipeChannel) + 16];
TbSoundPipeChannel* gChannel[4];
bool gFreshInfo;   // the info given to a channel or the module was built by the code under test: only its set words are defined

TbSoundSampleStore* Store() { return reinterpret_cast<TbSoundSampleStore*>(gStoreBytes); }

TbSoundPipeChannel* ChannelFor(int64_t selector) {
    if (selector < 1 || selector > 4) return nullptr;
    return gChannel[selector - 1];
}

void FillSys(const uint32_t* w) {
    gSys.mField0 = w[0];
    gSys.mField4 = w[1];
    gSys.mField8 = w[2];
    gSys.mFieldC = w[3];
    gSys.mField10 = w[4];
    gSys.mMaxBuffers = w[5];
    gSys.mMask = w[6];
    gSys.mBuffersToUse = w[7];
    gSys.mHWnd = Cookie(w[8]);
    gSys.mGuid = static_cast<NS_MacDoze::_GUID*>(Cookie(w[9]));
    gSys.mClassId = static_cast<NS_MacDoze::_GUID*>(Cookie(w[10]));
    gSys.mListenerPosition = &gPos[0];
    gSys.mFrontDir = &gPos[1];
    gSys.mTopDir = &gPos[2];
    gSys.mListenerVelocity = &gPos[3];
    gSys.mSpeakerConfig = reinterpret_cast<const TbSpeaker::Config*>(gConfigBytes);
    gSys.mField40 = w[16];
    gSys.mSamplesPlaying = w[17];
    gSys.mHardwareDevice = Cookie(w[18]);
}

void DumpSys(uint32_t* w) {
    w[0] = gSys.mField0;
    w[1] = gSys.mField4;
    w[2] = gSys.mField8;
    w[3] = gSys.mFieldC;
    w[4] = gSys.mField10;
    w[5] = gSys.mMaxBuffers;
    w[6] = gSys.mMask;
    w[7] = gSys.mBuffersToUse;
    w[8] = Low32(gSys.mHWnd);
    w[9] = Low32(gSys.mGuid);
    w[10] = Low32(gSys.mClassId);
    w[11] = static_cast<uint32_t>(lab(gSys.mListenerPosition));
    w[12] = static_cast<uint32_t>(lab(gSys.mFrontDir));
    w[13] = static_cast<uint32_t>(lab(gSys.mTopDir));
    w[14] = static_cast<uint32_t>(lab(gSys.mListenerVelocity));
    w[15] = static_cast<uint32_t>(lab(gSys.mSpeakerConfig));
    w[16] = gSys.mField40;
    w[17] = gSys.mSamplesPlaying;
    w[18] = Low32(gSys.mHardwareDevice);
}

// A sample info given to a callee: the words the code under test can have set. The command's own info (all of whose words
// the case defines) is recorded whole; an info the code built records only the words its constructor and setters define.
void RecInfo(const char* name, const TbSoundSampleInfo& info) {
    if (gFreshInfo) {
        ev(name, {lab(&info), info.mNumber, info.mField0C, info.mField10, info.mFlags, info.mPriority,
                  (info.mFlags & 0x40) != 0 ? info.mLoopCount : 0, (info.mFlags & 2) != 0 ? info.mVolume : 0});
    } else {
        ev(name, {lab(&info), info.mNumber, info.mUserId, info.mField0C, info.mField10, info.mFlags, info.mField18,
                  info.mVolume, info.mPriority, info.mLoopCount});
    }
}

void RecPos(const char* name, const TbSoundPosition& position) {
    ev(name, {lab(&position), FB(position.mX), FB(position.mY), FB(position.mZ)});
}

// ---- the device module ---------------------------------------------------------------------------------------------------

TbError Answer(const char* key) {
    return TbError(static_cast<int32_t>(ret(key, 0)));
}

class FakeModule : public TbSoundSystemModule {
public:
    FakeModule() : TbSoundSystemModule() {}
    void Process() override { ev("Mod.Process", {lab(this)}); }
    TbError SetOptions(uint32_t options) override {
        ev("Mod.SetOptions", {lab(this), options});
        return Answer("Mod.SetOptions");
    }
    void Close() override { ev("Mod.Close", {lab(this)}); }
    TbError StreamData(TbSoundPipeChannel* channel) override;   // below: it can start the command again (re-entry)
    TbError StopChannel(uint32_t channel) override {
        ev("Mod.StopChannel", {lab(this), channel});
        return Answer("Mod.StopChannel");
    }
    TbError PauseChannel(uint32_t channel) override {
        ev("Mod.PauseChannel", {lab(this), channel});
        return Answer("Mod.PauseChannel");
    }
    TbError RestartChannel(uint32_t channel) override {
        ev("Mod.RestartChannel", {lab(this), channel});
        return Answer("Mod.RestartChannel");
    }
    TbError PauseAll() override {
        ev("Mod.PauseAll", {lab(this)});
        return Answer("Mod.PauseAll");
    }
    TbError RestartAll() override {
        ev("Mod.RestartAll", {lab(this)});
        return Answer("Mod.RestartAll");
    }
    TbError SetSpeakerConfig(const TbSpeaker::Config& config) override {
        ev("Mod.SetSpeakerConfig", {lab(this), lab(&config)});
        return Answer("Mod.SetSpeakerConfig");
    }
    TbError Set3DDSPFunctionality(uint32_t channel, uint32_t options) override {
        ev("Mod.Set3DDSPFunctionality", {lab(this), channel, options});
        return Answer("Mod.Set3DDSPFunctionality");
    }
    TbError SetListenerVelocity(const TbSoundPosition& velocity) override {
        ev("Mod.SetListenerVelocity", {lab(this)});
        RecPos("Mod.SetListenerVelocity.pos", velocity);
        return Answer("Mod.SetListenerVelocity");
    }
    TbError SetListenerPosition(const TbSoundPosition& position) override {
        ev("Mod.SetListenerPosition", {lab(this)});
        RecPos("Mod.SetListenerPosition.pos", position);
        return Answer("Mod.SetListenerPosition");
    }
    TbError SetListenerDirection(const TbSoundPosition& front, const TbSoundPosition& top) override {
        ev("Mod.SetListenerDirection", {lab(this)});
        RecPos("Mod.SetListenerDirection.front", front);
        RecPos("Mod.SetListenerDirection.top", top);
        return Answer("Mod.SetListenerDirection");
    }
    TbError SetChannelInfo(uint32_t channel, TbSoundPipeChannel* pipe, TbSoundSampleInfo& info) override {
        ev("Mod.SetChannelInfo", {lab(this), channel, lab(pipe)});
        RecInfo("Mod.SetChannelInfo.info", info);
        return Answer("Mod.SetChannelInfo");
    }
    TbError SetChannelVolume(uint32_t channel, TbSoundSampleInfo& info) override {
        ev("Mod.SetChannelVolume", {lab(this), channel});
        RecInfo("Mod.SetChannelVolume.info", info);
        return Answer("Mod.SetChannelVolume");
    }
};

// ---- the access object (the core) ------------------------------------------------------------------------------------------

FakeModule* gModule;

class FakeAccess : public CAudioComponentAccess {
public:
    TbError GetSystemInfo(TbSoundSystemInfo&) override { return TbError(0); }
    TbError GetSampleInfo(void*, TbSoundSampleInfo&) override { return TbError(0); }
    void* NewSample(void*, TbSoundSampleInfo&) override { return nullptr; }
    void* NewControl(void*, TbSoundSampleInfo&) override { return nullptr; }
    TbError SystemCommand(const SystemCommand::Command&) override { return TbError(0); }
    TbSoundSystemModule* GetSystemModule() const override {
        ev("Access.GetSystemModule", {lab(this)});
        return gModule;
    }
    TbSoundSampleStore* GetSampleStore() const override {
        ev("Access.GetSampleStore", {lab(this)});
        return Store();
    }
};

FakeAccess* gAccess;

bool gReenter;   // the next StreamData runs Start::Execute again before it answers (the re-entry guard)
void* gReenterHandle;

TbError FakeModule::StreamData(TbSoundPipeChannel* channel) {
    ev("Mod.StreamData", {lab(this), lab(channel)});
    if (gReenter) {
        gReenter = false;
        SystemCommand::Start inner(gReenterHandle);
        TbError innerResult = inner.Execute(gAccess);
        ev("Inner.Start", {static_cast<int32_t>(innerResult)});
    }
    return Answer("Mod.StreamData");
}

}  // namespace

// ---- the fakes outside the unit ------------------------------------------------------------------------------------------------

// The core's StopAndReleasePipe (w79b) is called on the access object, downcast: it records the object, the channel and the flag.
TbError TbSoundCore::StopAndReleasePipe(TbSoundPipeChannel* channel, bool release) {
    ev("Core.StopAndReleasePipe", {lab(this), lab(channel), release});
    return Answer("Core.StopAndReleasePipe");
}

// The pipe base links the clock (the sample codecs use it; no command does).
uint32_t LbTime_GetClock() {
    return 0;
}

namespace SamsUtilities {
void UDebug(const char* format, ...) {
    ev("UDebug", {}, {format});
}
}  // namespace SamsUtilities

bool TbSoundSampleStore::GetValidChannelIndexFromHandle(const CUniqueHandle& handle, uint32_t& index) {
    ev("Store.GetValidChannelIndexFromHandle", {lab(this), handle.mValue});
    index = static_cast<uint32_t>(ret("Store.index", 0));
    return ret("Store.valid", 0) != 0;
}
TbSoundPipeChannel* TbSoundSampleStore::operator[](uint32_t index) const {
    ev("Store.at", {lab(this), index});
    return ChannelFor(ret("Store.at", 0));
}
TbSoundPipeChannel* TbSoundSampleStore::operator[](const CUniqueHandle& handle) const {
    ev("Store.atHandle", {lab(this), handle.mValue});
    return ChannelFor(ret("Store.atHandle", 0));
}
void TbSoundSampleStore::SetNumberOfChannelsToUse(const TbSoundSystemInfo& info) {
    ev("Store.SetNumberOfChannelsToUse", {lab(this), lab(&info)});
}
void TbSoundSampleStore::StopAllChannels(bool flag) {
    ev("Store.StopAllChannels", {lab(this), flag});
}
void TbSoundSampleStore::StopAllChannelsWithVolumeControl(uint32_t mask) {
    ev("Store.StopAllChannelsWithVolumeControl", {lab(this), mask});
}
TbSoundSampleStore::CChannelIterator::CChannelIterator(TbSoundSampleStore* store) {
    ev("It.ctor", {lab(store)});
    mCurrent = ChannelFor(ret("It.first", 0));
    mEnd = nullptr;
}
TbSoundPipeChannel* TbSoundSampleStore::CChannelIterator::operator++() {
    ev("It.next", {});
    mCurrent = ChannelFor(ret("It.next", 0));
    return mCurrent;
}
TbSoundSampleStore::CChannelIterator::operator TbSoundPipeChannel*() {
    ev("It.current", {});
    return mCurrent;
}

// The channel is a fake in this unit: its constructor and the virtuals the vtable names do nothing; SetInfo records.
TbSoundPipeChannel::TbSoundPipeChannel() : mServiceBuffer(static_cast<unsigned char*>(nullptr)) {}
TbSoundPipeChannel::~TbSoundPipeChannel() {}
TbSoundSamplePipe* TbSoundPipeChannel::Attatch(TbSoundSamplePipe*) { return nullptr; }
int32_t TbSoundPipeChannel::GetDataChunk(void*, int32_t, int32_t&, bool) { return 0; }
TbSoundSamplePipe* TbSoundPipeChannel::FindPipe(const CUniqueHandle&) { return nullptr; }
TbError TbSoundPipeChannel::SetInfo(const TbSoundSampleInfo& info) {
    ev("Chan.SetInfo", {lab(this)});
    RecInfo("Chan.SetInfo.info", info);
    return Answer("Chan.SetInfo");
}
void TbSoundPipeChannel::SetPriority(uint32_t priority) {
    ev("Chan.SetPriority", {lab(this), priority});
}
void TbSoundPipeChannel::SetVolume(uint32_t volume) {
    ev("Chan.SetVolume", {lab(this), volume});
}
uint32_t TbSoundPipeChannel::GetVolume() {
    ev("Chan.GetVolume", {lab(this)});
    return static_cast<uint32_t>(ret("Chan.GetVolume", 0));
}
uint32_t TbSoundPipeChannel::GetIndex() {
    ev("Chan.GetIndex", {lab(this)});
    return static_cast<uint32_t>(ret("Chan.GetIndex", 0));
}
bool TbSoundPipeChannel::ReleasePipe(bool flag) {
    ev("Chan.ReleasePipe", {lab(this), flag});
    return ret("Chan.ReleasePipe", 0) != 0;
}

// ---- the entry point -------------------------------------------------------------------------------------------------------

namespace {

// The parameter words of tpw_sx_run.
enum { P_HANDLE = 0, P_SINIT = 1, P_SINPROG = 2, P_MASK = 3, P_INDEX = 4, P_REENTER = 5, P_SAMP = 8, P_SYS = 16, P_POS = 40 };
// The result words.
enum { O_RESULT = 0, O_SAMP = 1, O_SYS = 6, O_POS = 25, O_STATIC = 37 };

enum Op {
    kPause = 0, kRestart, kPauseAll, kRestartAll, kSetSampleInfo, kSetChannelInfo, kStart, kStop, kStopLoop, kStopAll,
    kStopAllVolumeControl, kProcess, kSetSystemInfo, kUpdateAllVolumes
};

}  // namespace

extern "C" {

void tpw_sx_run(int32_t op, const uint32_t* p, uint32_t* out) {
    std::memset(out, 0, 64 * sizeof(uint32_t));
    tpw_cl_on = 0;

    // ---- the world, made with the log off
    if (gAccess == nullptr) {
        gAccess = new FakeAccess();
        gModule = new FakeModule();
        for (int i = 0; i < 4; ++i) {
            std::memset(gChannelBytes[i], 0xA5, sizeof gChannelBytes[i]);
            gChannel[i] = ::new (static_cast<void*>(gChannelBytes[i])) TbSoundPipeChannel();
        }
    }
    void* handle = Cookie(p[P_HANDLE]);
    std::memset(static_cast<void*>(&gSamp), 0xA5, sizeof gSamp);
    gSamp.mField0C = p[P_SAMP];
    gSamp.mFlags = p[P_SAMP + 1];
    gSamp.mField18 = p[P_SAMP + 2];
    gSamp.mVolume = p[P_SAMP + 3];
    gSamp.mPriority = p[P_SAMP + 4];
    FillSys(p + P_SYS);
    for (int i = 0; i < 4; ++i) {
        gPos[i].mX = bitsToFloat(p[P_POS + 3 * i]);
        gPos[i].mY = bitsToFloat(p[P_POS + 3 * i + 1]);
        gPos[i].mZ = bitsToFloat(p[P_POS + 3 * i + 2]);
    }
    SystemCommand::sStartInitialised = p[P_SINIT] != 0;
    SystemCommand::sStartInProgress = p[P_SINPROG] != 0;
    gReenter = p[P_REENTER] != 0;
    gReenterHandle = handle;
    gFreshInfo = op == kStopLoop || op == kUpdateAllVolumes;
    tpw_cl_label(1, gAccess);
    tpw_cl_label(2, gModule);
    tpw_cl_label(3, Store());
    for (int i = 0; i < 4; ++i) tpw_cl_label(4 + i, gChannel[i]);
    tpw_cl_label(8, &gSamp);
    tpw_cl_label(9, &gSys);
    for (int i = 0; i < 4; ++i) tpw_cl_label(10 + i, &gPos[i]);
    tpw_cl_label(14, gConfigBytes);

    // ---- the call
    tpw_cl_on = 1;
    using namespace SystemCommand;
    int64_t result = 0;
    switch (op) {
        case kPause: {
            Pause command(handle);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kRestart: {
            Restart command(handle);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kPauseAll: {
            PauseAll command;
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kRestartAll: {
            RestartAll command;
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kSetSampleInfo: {
            SetSampleInfo command(handle, gSamp);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kSetChannelInfo: {
            SetSampleInfo command(handle, gSamp);
            result = command.SetChannelInfo(gAccess, p[P_INDEX]);
            break;
        }
        case kStart: {
            Start command(handle);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kStop: {
            Stop command(handle);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kStopLoop: {
            StopLoop command(handle);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kStopAll: {
            StopAll command;
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kStopAllVolumeControl: {
            StopAllVolumeControl command(p[P_MASK]);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kProcess: {
            Process command;
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kSetSystemInfo: {
            SetSystemInfo command(gSys);
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        case kUpdateAllVolumes: {
            UpdateAllVolumes command;
            result = static_cast<const Command&>(command).Execute(gAccess);
            break;
        }
        default:
            break;
    }
    tpw_cl_on = 0;

    out[O_RESULT] = static_cast<uint32_t>(result);
    out[O_SAMP] = gSamp.mField0C;
    out[O_SAMP + 1] = gSamp.mFlags;
    out[O_SAMP + 2] = gSamp.mField18;
    out[O_SAMP + 3] = gSamp.mVolume;
    out[O_SAMP + 4] = gSamp.mPriority;
    DumpSys(out + O_SYS);
    for (int i = 0; i < 4; ++i) {
        out[O_POS + 3 * i] = static_cast<uint32_t>(FB(gPos[i].mX));
        out[O_POS + 3 * i + 1] = static_cast<uint32_t>(FB(gPos[i].mY));
        out[O_POS + 3 * i + 2] = static_cast<uint32_t>(FB(gPos[i].mZ));
    }
    out[O_STATIC] = SystemCommand::sStartInitialised;
    out[O_STATIC + 1] = SystemCommand::sStartInProgress;
}

}  // extern "C"
