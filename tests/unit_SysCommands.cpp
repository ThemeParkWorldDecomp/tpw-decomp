// Fakes and test entry points for unit w79h, the Execute bodies of the sound library's system commands (libtpw_unit_SysCommands.dylib,
// driven by tests/test_SysCommands.py in the program "sound symbols shared"; see calllog.h and cmake/units/SysCommands.cmake):
//   src/TbSysCommand.cpp  every TbSysCommand::<Class>::Execute and the accessors of SetValue, GetValue and Serialise (under test)
// Real: the audio object of w79f (src/TbAudioBody.cpp: GetSoundManager, GetPlaceHolderList; its virtuals StopHandle and
// DeregisterCatagoryBanks are hooked below to record), the info and sample-info records (w77), the library's command constructors
// (w79b: SystemCommand::Process, StopAll, PauseAll, RestartAll, SetSystemInfo, SetSampleInfo), the component chain (CAudioComponent).
// Fakes, each recording what the original's callee is given (the PowerPC side records the same from its stubs):
//   TbSoundManager (w79e) and the TbSoundCore under it: GetSystemInfo and SystemCommand (the virtuals the commands reach) and every
//       member a command calls: SetDistanceMapping, GetMaxNumberOfChannels, SetQSoundInfo, SetDroppable, UpdateVolumeControl,
//       ToggleVolumeControl, GetDirectSoundObject, GetVolume
//   CAudioPlaceHolderList (w79g1): the members the commands call and the ones TbAudioBody links; CAudioPlaceHolder (w79g2): SetVolume,
//       SetVolumeAndPitch and a fake placeholder whose Pause and Restart record
//   TpBody::Tick, Save and Load (untranslated), CvtDblUl, the streamer and the event mapper (linked by TbAudioBody, never called)
//
// Labels: 1 the body, 2 the manager, 4 the list, 5 the placeholder, 7 the sample data, 9 the command, 10 the speaker config,
// 11 the command's out word.
//
// tpw_sc_run drives one operation on a world it builds from the parameter words; see the P_ and O_ enums. The operation numbers are the
// order of the table in tests/test_SysCommands.py.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "calllog.h"
#include "snd_commands.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

float BitsToFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

// 0 for the commands that send a system info built by the default constructor, 1 for the speaker-config constructor's (the fake
// manager records the words each of them defines).
int gInfoKind;

// The world of the running operation.
CAudioPlaceHolder* gHolder;
CSampleData* gSample;

}  // namespace

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- the library's command classes: Execute is the core's (w79d); the manager here is a fake, so none runs -------------------------

TbError SystemCommand::SetSystemInfo::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::SetSampleInfo::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::Process::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::StopAll::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::PauseAll::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::RestartAll::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::UpdateAllVolumes::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::Stop::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::Pause::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::Restart::Execute(CAudioComponentAccess*) const { return TbError(-77); }
TbError SystemCommand::Start::Execute(CAudioComponentAccess*) const { return TbError(-77); }

// ---- the core and the manager: fakes (w79e, w79b) -----------------------------------------------------------------------------

namespace {

// The words a system info defines, for the kind of constructor the command used.
void RecInfo(const char* name, int64_t first, const TbSoundSystemInfo& i) {
    if (gInfoKind == 0) {
        ev(name, {first, i.mField0, i.mField4, i.mField8, i.mMask, i.mBuffersToUse, Low32(i.mHWnd), i.mField40});
    } else {
        ev(name, {first, i.mField4, i.mField8, i.mMask, lab(i.mSpeakerConfig)});
    }
}

}  // namespace

TbSoundCore::TbSoundCore() : CAudioComponent(), mModule(nullptr), mStore(nullptr), mCopyInit() {}
TbSoundCore::~TbSoundCore() {}
TbError TbSoundCore::Init(const TbSoundSystemInfo&) { return TbError(-77); }
// The device query (slot +8 of the manager's table): the request word of the info (+8) is recorded, then the case's mask and
// sample count are written into it.
TbError TbSoundCore::GetSystemInfo(TbSoundSystemInfo& info) {
    ev("Mgr.GetSystemInfo", {lab(this), info.mField8});
    info.mMask = static_cast<uint32_t>(ret("Mgr.Info.mask", 0));
    info.mSamplesPlaying = static_cast<uint32_t>(ret("Mgr.Info.playing", 0));
    return TbError(static_cast<int32_t>(ret("Mgr.Info", 0)));
}
TbError TbSoundCore::GetSampleInfo(void*, TbSoundSampleInfo&) { return TbError(-77); }
void* TbSoundCore::NewSample(void*, TbSoundSampleInfo&) { return nullptr; }
void* TbSoundCore::NewControl(void*, TbSoundSampleInfo&) { return nullptr; }
// The command channel (slot +0x18): the class of the command and the words it defines are recorded.
TbError TbSoundCore::SystemCommand(const SystemCommand::Command& command) {
    int64_t kind = -1;
    if (dynamic_cast<const ::SystemCommand::Process*>(&command) != nullptr) {
        kind = 1;
    } else if (dynamic_cast<const ::SystemCommand::StopAll*>(&command) != nullptr) {
        kind = 2;
    } else if (dynamic_cast<const ::SystemCommand::PauseAll*>(&command) != nullptr) {
        kind = 3;
    } else if (dynamic_cast<const ::SystemCommand::RestartAll*>(&command) != nullptr) {
        kind = 4;
    } else if (auto* set = dynamic_cast<const ::SystemCommand::SetSystemInfo*>(&command)) {
        RecInfo("Mgr.SetSystemInfo", lab(this), *set->mInfo);
        kind = 5;
    } else if (auto* sample = dynamic_cast<const ::SystemCommand::SetSampleInfo*>(&command)) {
        // the volume and the pitch only when the flags say they were set (the constructor leaves the others as they were)
        const TbSoundSampleInfo& info = *sample->mInfo;
        ev("Mgr.SetSampleInfo", {lab(this), Low32(sample->mHandle), info.mFlags, (info.mFlags & 2) != 0 ? info.mVolume : 0,
                                 (info.mFlags & 0x20) != 0 ? static_cast<uint32_t>(info.mPitch) : 0});
        kind = 6;
    }
    ev("Mgr.Command", {lab(this), kind});
    return TbError(static_cast<int32_t>(ret("Mgr.Command", 0)));
}
TbSoundSystemModule* TbSoundCore::GetSystemModule() const { return nullptr; }
TbSoundSampleStore* TbSoundCore::GetSampleStore() const { return nullptr; }
CMemoryCopyInit::CMemoryCopyInit() {}
CMemoryCopyInit::~CMemoryCopyInit() {}

TbSoundManager::TbSoundManager() : TbSoundCore() {
    mListenerPosition = nullptr;
    mAudioSystem = nullptr;
}
TbSoundManager::~TbSoundManager() {}
TbError TbSoundManager::Init(const char*) { return TbError(-77); }
float TbSoundManager::GetRolloffFactorToMinMaxDistance(float, float) { return 0.0f; }
TbDynamicStringTemplate<char>& TbSoundManager::GetSoundPath() { return mSoundPath; }
void TbSoundManager::SetListenerPosition(const TbSoundPosition&) {}
void TbSoundManager::SetAudioSystem(CAudioComponent*) {}
bool TbSoundManager::VolumeControlIsOn(uint8_t) { return false; }
TbError TbSoundManager::SetListenerOrientation(const TbSoundPosition&, const TbSoundPosition&, const TbSoundPosition&,
                                               const TbSoundPosition&) {
    return TbError(-77);
}

void TbSoundManager::SetDistanceMapping(float multiplier) {
    ev("Mgr.SetDistanceMapping", {lab(this), fbits(multiplier)});
}
TbError TbSoundManager::GetMaxNumberOfChannels(uint32_t& out) {
    ev("Mgr.GetMaxNumberOfChannels", {lab(this), lab(&out), out});
    out = static_cast<uint32_t>(ret("Mgr.Max.value", 0));
    return TbError(static_cast<int32_t>(ret("Mgr.Max", 0)));
}
TbError TbSoundManager::SetQSoundInfo(uint32_t enable, uint32_t extended, uint32_t third) {
    ev("Mgr.SetQSoundInfo", {lab(this), enable, extended, third});
    return TbError(static_cast<int32_t>(ret("Mgr.QSound", 0)));
}
void TbSoundManager::SetDroppable(bool on) {
    ev("Mgr.SetDroppable", {lab(this), on ? 1 : 0});
}
TbError TbSoundManager::UpdateVolumeControl(uint8_t index, float volume) {
    ev("Mgr.UpdateVolumeControl", {lab(this), index, fbits(volume)});
    return TbError(static_cast<int32_t>(ret("Mgr.Update", 0)));
}
TbError TbSoundManager::ToggleVolumeControl(uint8_t index, uint32_t on) {
    ev("Mgr.ToggleVolumeControl", {lab(this), index, on});
    return TbError(static_cast<int32_t>(ret("Mgr.Toggle", 0)));
}
TbError TbSoundManager::GetDirectSoundObject(uint32_t& out) {
    ev("Mgr.GetDirectSoundObject", {lab(this), lab(&out), out});
    out = static_cast<uint32_t>(ret("Mgr.DS.value", 0));
    return TbError(static_cast<int32_t>(ret("Mgr.DS", 0)));
}
float TbSoundManager::GetVolume(uint8_t index) {
    ev("Mgr.GetVolume", {lab(this), index});
    return BitsToFloat(static_cast<uint32_t>(ret("Mgr.Volume", 0)));
}

// The runtime's double to unsigned long conversion (w72's, linked in the launcher): the double's two halves are recorded
uint32_t CvtDblUl(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("CvtDblUl", {static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xFFFFFFFFu)});
    return static_cast<uint32_t>(ret("CvtDblUl", 0));
}

// ---- the streamer and the mapper (linked by TbAudioBody; no command reaches them) -----------------------------------------------

namespace {
alignas(16) uint8_t gStreamerBytes[sizeof(TbMapStreamer) + 16];
}  // namespace

TbMapStreamer* TbMapStreamer::Instance() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}
void TbMapStreamer::SetSoundManager(TbSoundManager*) {}
TbError TbMapStreamer::RegisterCatagory(const char*, const char*, uint32_t&) { return TbError(-77); }
TbError TbMapStreamer::RemoveCatagory(uint32_t) { return TbError(-77); }
CEventData* TbEventMapper::MapEvent(CCatagoryData*, const sound::TbAudioEvent&) { return nullptr; }

// ---- the placeholder list and the placeholders ----------------------------------------------------------------------------------

// A placeholder the list finds: Pause and Restart (the virtuals the commands call) record and answer from the queues.
class FakeHolder : public CAudioPlaceHolder {
public:
    TbError Pause() override {
        ev("Holder.Pause", {lab(this)});
        return TbError(static_cast<int32_t>(ret("Holder.Pause", 0)));
    }
    TbError Restart() override {
        ev("Holder.Restart", {lab(this)});
        return TbError(static_cast<int32_t>(ret("Holder.Restart", 0)));
    }
};

CAudioPlaceHolder::~CAudioPlaceHolder() {}
TbSndCallback::TbSndCallback() {}
void TbSndCallback::SoundCallback(void*, void*) {}
void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}
CAudioPlaceHolder::CAudioPlaceHolder() : mEvent(nullptr), mSound(nullptr), mSample(nullptr), mEndTime(0), mHandle(nullptr),
    mCatagory(nullptr), mListHandle(nullptr), mFlags(0), mNext(nullptr), mPrevious(nullptr), mPauseTime(0), mMaxDistance(0.0f),
    mLastSound(-1), mLastSample(-1), mVolumeControlIndex(0), mFade(0.0f), mVolume(0), mPitch(0), mExtra(nullptr) {}
void CAudioPlaceHolder::CompletedCallback(void*, void*, int32_t) {}
void CAudioPlaceHolder::Release() {}
TbError CAudioPlaceHolder::Init(CCatagoryContainer*, void*, const TbSoundPosition&, float) { return TbError(0); }
TbError CAudioPlaceHolder::PlaySample(uint32_t) { return TbError(0); }
TbError CAudioPlaceHolder::Stop() { return TbError(0); }
TbError CAudioPlaceHolder::Pause() { return TbError(0); }
TbError CAudioPlaceHolder::Restart() { return TbError(0); }
float CAudioPlaceHolder::ChangeDistPriority() { return 0.0f; }
TbError CAudioPlaceHolder::Update(const TbSoundPosition&, const TbSoundPosition&) { return TbError(0); }
void CAudioPlaceHolder::UpdateParameter(uint8_t, uint32_t) {}
void CAudioPlaceHolder::UpdateParameterDepandants() {}
bool CAudioPlaceHolder::ChooseRandomSound(bool) { return false; }
CSampleData* CAudioPlaceHolder::ChooseRandomSample(bool) { return nullptr; }
bool CAudioPlaceHolder::GetParameterValue(uint32_t, uint32_t&) { return false; }
bool CAudioPlaceHolder::FillSampleInfo(uint32_t, TbSoundSampleInfo&) { return false; }
bool CAudioPlaceHolder::FillBasicSampleInfo(TbSoundSampleInfo&) { return false; }
int32_t CAudioPlaceHolder::GetPsuedoTime() { return 0; }
uint32_t CAudioPlaceHolder::GetClassID() { return 0; }
uint32_t CAudioPlaceHolder::CDistPriority::GetBasePriority() const { return mBasePriority; }
void CAudioPlaceHolder::SetSoundManager(TbSoundManager*) {}
void CAudioPlaceHolder::SetVolume(uint32_t volume) {
    ev("Holder.SetVolume", {lab(this), volume});
}
void CAudioPlaceHolder::SetVolumeAndPitch(uint32_t volume, int32_t pitch) {
    ev("Holder.SetVolumeAndPitch", {lab(this), volume, static_cast<uint32_t>(pitch)});
}

// The list: the constructor gives the array no data (its destructor then frees nothing); the sound lock's two members are defined
// here (the real ones are in src/snd_handles.cpp, which this unit does not link).
CAutoCriticalSection::CAutoCriticalSection() : mValue(0) {}
CAutoCriticalSection::~CAutoCriticalSection() = default;
CAudioPlaceHolderList::CAudioPlaceHolderList(uint32_t) : mPriorityList(nullptr) {}
CAudioPlaceHolderList::~CAudioPlaceHolderList() {}
CAudioPlaceHolder* CAudioPlaceHolderList::FindPlaceHolder(void* handle) {
    ev("List.FindPlaceHolder", {lab(this), Low32(handle)});
    return ret("List.Find", 0) != 0 ? gHolder : nullptr;
}
void* CAudioPlaceHolderList::CreatePlaceHolder(CCatagoryContainer*, void*, const sound::TbAudioEvent&) { return nullptr; }
uint32_t CAudioPlaceHolderList::UpdatePlaceHolder(void*, const TbSoundPosition&, const TbSoundPosition&) { return 0; }
uint32_t CAudioPlaceHolderList::UpdatePlaceHolderParameter(void*, uint8_t, uint32_t) { return 0; }
void CAudioPlaceHolderList::ProcessPlaceHolders() {
    ev("List.ProcessPlaceHolders", {lab(this)});
}
void CAudioPlaceHolderList::StopAllPlaceHolders(uint32_t unused) {
    ev("List.StopAllPlaceHolders", {lab(this), unused});
}
uint32_t CAudioPlaceHolderList::GetNumberOfChannels() const {
    ev("List.GetNumberOfChannels", {lab(this)});
    return static_cast<uint32_t>(ret("List.GetCh", 0));
}
uint32_t CAudioPlaceHolderList::SetNumberOfChannels(uint32_t channels) {
    ev("List.SetNumberOfChannels", {lab(this), channels});
    return static_cast<uint32_t>(ret("List.SetCh", 0));
}
TbError CAudioPlaceHolderList::SetBoundingRectangle(void* handle, const TbRect& rect) {
    ev("List.SetBoundingRectangle", {lab(this), Low32(handle), static_cast<uint32_t>(rect.left), static_cast<uint32_t>(rect.top),
                                     static_cast<uint32_t>(rect.right), static_cast<uint32_t>(rect.bottom)});
    return TbError(static_cast<int32_t>(ret("List.SetRect", 0)));
}
void* CAudioPlaceHolderList::GetPlaceholderAndSoundHandle(void* handle, CAudioPlaceHolder*& holder) {
    ev("List.GetPlaceholderAndSoundHandle", {lab(this), Low32(handle)});
    holder = ret("List.GetPH.holder", 0) != 0 ? gHolder : nullptr;
    return Cookie(ret("List.GetPH.sample", 0));
}

// ---- TpBody (untranslated): the audio object's own Tick, Save and Load ---------------------------------------------------------

void TpBody::Tick() {
    ev("TpBody.Tick", {lab(this)});
}
TbError TpBody::Save(void* file) const {
    ev("TpBody.Save", {lab(this), Low32(file)});
    return TbError(static_cast<int32_t>(ret("TpBody.Save", 0)));
}
TbError TpBody::Load(void* file, uint8_t flag) const {
    ev("TpBody.Load", {lab(this), Low32(file), flag});
    return TbError(static_cast<int32_t>(ret("TpBody.Load", 0)));
}

// ---- the body ----------------------------------------------------------------------------------------------------------------------

namespace {

// The real body with its two virtuals the commands call replaced by recording ones (the Mac table's slots +0x34 and +0x38).
class HookedBody : public TbAudioBody {
public:
    TbError StopHandle(void* handle) const override {
        ev("Body.StopHandle", {lab(this), Low32(handle)});
        return TbError(static_cast<int32_t>(ret("Body.StopHandle", 0)));
    }
    TbError DeregisterCatagoryBanks(uint32_t category) const override {
        ev("Body.DeregisterCatagoryBanks", {lab(this), category});
        return TbError(static_cast<int32_t>(ret("Body.Dereg", 0)));
    }
};

enum Param {
    P_OP = 0, P_W = 1 /* five command words */, P_LIST = 8, P_SAMPLE = 9, P_LENGTH = 10
};
enum Out {
    O_RESULT = 0, O_OUTWORD = 1, O_HIGH = 2
};
enum Op {
    kProcess, kStopAll, kStopAllWithFade, kPauseAll, kRestartAll, kSetDistanceMapping, kSetBoundingRectangle, kSetNumberOfChannels,
    kGetMaxNumberOfChannels, kGetNumberOfPlayingChannels, kFlushCatagory, kSetSpeakerConfig, kSetFades, kSetHalfRate, kSetQSoundInfo,
    kSetEAX, kSetDroppable, kSetGeometryInfo, kStopHandle, kSetHandleVolume, kSetHandleVolumeAndPitch, kStopHandleWithFade,
    kSetMasterVolume, kToggleMaster, kSetSpeechVolume, kToggleSpeech, kSetUser1Volume, kToggleUser1, kSetUser2Volume, kToggleUser2,
    kSetSFXVolume, kToggleSFX, kSetMusicVolume, kToggleMusic, kGetDirectSoundObject, kGetSampleLength, kGetSFXVolume, kGetMusicVolume,
    kTick, kSave, kLoad, kPauseHandle, kRestartHandle, kSetPresetReverb, kRegisterObject, kDeleteObject, kDeleteAllObjects,
    kAccSetValueSys, kAccSetValueFloat, kAccSetValueHandle, kAccGetValueSys, kAccSerialiseHandle
};

alignas(16) uint8_t gConfigBytes[16];
alignas(16) uint8_t gSampleBytes[sizeof(CSampleData) + 16];

}  // namespace

extern "C" {

// One operation. p: the parameter words (P_ enum), out: the results (O_ enum).
void tpw_sc_run(const uint32_t* p, uint32_t* out) {
    const int32_t op = static_cast<int32_t>(p[P_OP]);
    std::memset(out, 0, 16 * sizeof(uint32_t));
    tpw_cl_on = 0;

    // ---- the world, made with the log off
    auto* body = new HookedBody();
    tpw_cl_label(1, body);
    tpw_cl_label(4, body->mPlaceHolders.mData);
    if (p[P_LIST] == 0) body->mPlaceHolders.mData = nullptr;
    auto* manager = new TbSoundManager();
    tpw_cl_label(2, manager);
    body->mManager = manager;
    gHolder = new FakeHolder();
    tpw_cl_label(5, gHolder);
    std::memset(gSampleBytes, 0xA5, sizeof gSampleBytes);
    gSample = reinterpret_cast<CSampleData*>(gSampleBytes);
    gSample->mF08 = p[P_LENGTH];
    tpw_cl_label(7, gSample);
    gHolder->mSample = p[P_SAMPLE] != 0 ? gSample : nullptr;
    int32_t outWord = static_cast<int32_t>(0xA5A5A5A5u);
    tpw_cl_label(11, &outWord);
    auto* config = reinterpret_cast<TbSpeaker::Config*>(gConfigBytes);
    tpw_cl_label(10, config);
    gInfoKind = op == kSetSpeakerConfig ? 1 : 0;

    const int32_t w0 = static_cast<int32_t>(p[P_W]);
    const int32_t w1 = static_cast<int32_t>(p[P_W + 1]);
    const int32_t w2 = static_cast<int32_t>(p[P_W + 2]);
    const int32_t w3 = static_cast<int32_t>(p[P_W + 3]);
    const int32_t w4 = static_cast<int32_t>(p[P_W + 4]);

    // ---- the call
    tpw_cl_on = 1;
    int32_t result = 0;
    bool accessor = false;
    using namespace TbSysCommand;
    switch (op) {
#define EXEC(cls, ...) \
    case k##cls: { \
        const cls command{__VA_ARGS__}; \
        result = command.Execute(body); \
        break; \
    }
        EXEC(Process)
        EXEC(StopAll)
        EXEC(StopAllWithFade)
        EXEC(PauseAll)
        EXEC(RestartAll)
        EXEC(SetDistanceMapping, BitsToFloat(p[P_W]))
        EXEC(SetBoundingRectangle, w0, w1, w2, w3, w4)
        EXEC(SetNumberOfChannels, w0)
        EXEC(GetMaxNumberOfChannels, &outWord)
        EXEC(GetNumberOfPlayingChannels, &outWord)
        EXEC(FlushCatagory, static_cast<uint32_t>(w0))
        EXEC(SetSpeakerConfig, config)
        EXEC(SetFades, w0)
        EXEC(SetHalfRate, w0)
        EXEC(SetQSoundInfo, w0, w1, w2)
        EXEC(SetEAX, w0)
        EXEC(SetDroppable, w0)
        EXEC(SetGeometryInfo, w0)
        EXEC(StopHandle, w0)
        EXEC(SetHandleVolume, w0, w1)
        EXEC(SetHandleVolumeAndPitch, w0, w1, w2)
        EXEC(StopHandleWithFade, w0)
        EXEC(SetMasterVolume, w0)
        EXEC(ToggleMaster, w0)
        EXEC(SetSpeechVolume, w0)
        EXEC(ToggleSpeech, w0)
        EXEC(SetUser1Volume, w0)
        EXEC(ToggleUser1, w0)
        EXEC(SetUser2Volume, w0)
        EXEC(ToggleUser2, w0)
        EXEC(SetSFXVolume, w0)
        EXEC(ToggleSFX, w0)
        EXEC(SetMusicVolume, w0)
        EXEC(ToggleMusic, w0)
        EXEC(GetDirectSoundObject, &outWord)
        EXEC(GetSampleLength, &outWord, w1)
        EXEC(GetSFXVolume, &outWord)
        EXEC(GetMusicVolume, &outWord)
        EXEC(Tick)
        EXEC(Save, Cookie(p[P_W]))
        EXEC(Load, Cookie(p[P_W]), static_cast<uint8_t>(p[P_W + 1]))
        EXEC(PauseHandle, w0)
        EXEC(RestartHandle, w0)
        EXEC(SetPresetReverb, w0)
        EXEC(RegisterObject, w0)
        EXEC(DeleteObject, w0)
        EXEC(DeleteAllObjects, w0)
#undef EXEC
        case kAccSetValueSys: {
            const ToggleMaster command(w0);
            result = static_cast<int32_t>(command.GetSysValue());
            accessor = true;
            break;
        }
        case kAccSetValueFloat: {
            const SetDistanceMapping command(BitsToFloat(p[P_W]));
            result = static_cast<int32_t>(fbits(command.GetFloatSysValue()));
            accessor = true;
            break;
        }
        case kAccSetValueHandle: {
            const StopHandle command(w0);
            const void* handle = command.GetTbHandleSysValue();
            result = static_cast<int32_t>(Low32(handle));
            out[O_HIGH] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle) >> 32);
            accessor = true;
            break;
        }
        case kAccGetValueSys: {
            const GetSFXVolume command(&outWord);
            result = lab(command.GetSysValue());
            accessor = true;
            break;
        }
        case kAccSerialiseHandle: {
            const Save command(Cookie(p[P_W]));
            result = static_cast<int32_t>(Low32(command.GetHandle()));
            accessor = true;
            break;
        }
    }
    tpw_cl_on = 0;
    (void)accessor;
    out[O_RESULT] = static_cast<uint32_t>(result);
    out[O_OUTWORD] = static_cast<uint32_t>(outWord);
}

}  // extern "C"
