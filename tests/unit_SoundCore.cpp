// Fakes and test entry points for unit w79b, the sound library's core (libtpw_unit_SoundCore.dylib, driven by
// tests/test_SoundCore.py in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundCore.cpp   TbSoundCore, CAudioComponentAccess, CAudioComponent, CModuleFactory, CMemoryCopyInit and the copier
//                         classes; src/snd_commands.cpp the SystemCommand constructors
// Real: those, the info records (w77), TbRefRelease (w78), the device-module base and the handle values (w79a).
// Fakes, each recording what the original's callee is given (the PowerPC side records the same from its stubs):
//   CSprocketsModule (p-sound): the device module GenerateModule makes; every slot the core calls records and answers a queue
//   TbSoundSampleStore (the real header is w79c's; the definitions here are fakes): constructor, destructor, Init, ReleaseAllPipes, the two counts, operator[],
//       CreateNewSample and CreateNewControl
//   TbSoundPipeChannel (w79c): GetIndex, ReleasePipe, GetActualVolume, GetPriority (the object is raw storage)
//   the command Execute bodies (w79d), and TestCommand, a command that records its Execute
//   FakeComponent, a CAudioComponent that overrides every virtual slot and records it (the tail of a chain)
//
// Sizes: the Mac's new sizes are recorded (the host's module is 24 bytes, the store 16, the copier 8: the first two are the
// Mac's already, the copier is mapped from 8 to 4). Labels: 1 the core, 2 the module, 3 the store, 4 the channel, 5 the sample
// info, 6 the system info, 7 the test command, 8 the global copier of the world, 9 an access object, 11 and up the chain nodes,
// 100 and up the blocks the code under test makes. A block the case allocates is filled with 0xA5 on both sides.
//
// tpw_sc_run drives one operation of the core, the factory, the copier or a command constructor on a world it builds from the
// parameter words; tpw_sc_chain_* drive the component chain in scripts, with the state of every node after each operation.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <typeinfo>

#include "CSprocketsModule.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "calllog.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

bool gTrack;   // "new" events are recorded only while the code under test runs

std::size_t MacSize(std::size_t size) {
    if (size == sizeof(CMemoryCopy::CStandardCopy)) return 4;
    return size;   // the module (24) and the store (16) are the Mac's sizes already
}

// The runtime's new: a "new" event of the Mac's size and the block's label, the block filled with 0xA5.
void Allocated(std::size_t size, bool, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack) return;
    ev("new", {static_cast<int64_t>(MacSize(size)), newlab(block)});
}

// Blocks the runtime frees are kept (no address is reused while a stale pointer to it is compared).
bool KeepFreed(void*, bool) {
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) Allocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

namespace {

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

// ---- the system info as 19 Mac words --------------------------------------------------------------------------------------

constexpr int kSysWords = 19;

void FillSys(TbSoundSystemInfo& info, const uint32_t* w) {
    info.mField0 = w[0];
    info.mField4 = w[1];
    info.mField8 = w[2];
    info.mFieldC = w[3];
    info.mField10 = w[4];
    info.mMaxBuffers = w[5];
    info.mMask = w[6];
    info.mBuffersToUse = w[7];
    info.mHWnd = Cookie(w[8]);
    info.mGuid = static_cast<NS_MacDoze::_GUID*>(Cookie(w[9]));
    info.mClassId = static_cast<NS_MacDoze::_GUID*>(Cookie(w[10]));
    info.mListenerPosition = static_cast<const TbSoundPosition*>(Cookie(w[11]));
    info.mFrontDir = static_cast<const TbSoundPosition*>(Cookie(w[12]));
    info.mTopDir = static_cast<const TbSoundPosition*>(Cookie(w[13]));
    info.mListenerVelocity = static_cast<const TbSoundPosition*>(Cookie(w[14]));
    info.mSpeakerConfig = static_cast<const TbSpeaker::Config*>(Cookie(w[15]));
    info.mField40 = w[16];
    info.mSamplesPlaying = w[17];
    info.mHardwareDevice = Cookie(w[18]);
}

void DumpSys(const TbSoundSystemInfo& info, uint32_t* w) {
    w[0] = info.mField0;
    w[1] = info.mField4;
    w[2] = info.mField8;
    w[3] = info.mFieldC;
    w[4] = info.mField10;
    w[5] = info.mMaxBuffers;
    w[6] = info.mMask;
    w[7] = info.mBuffersToUse;
    w[8] = Low32(info.mHWnd);
    w[9] = Low32(info.mGuid);
    w[10] = Low32(info.mClassId);
    w[11] = Low32(info.mListenerPosition);
    w[12] = Low32(info.mFrontDir);
    w[13] = Low32(info.mTopDir);
    w[14] = Low32(info.mListenerVelocity);
    w[15] = Low32(info.mSpeakerConfig);
    w[16] = info.mField40;
    w[17] = info.mSamplesPlaying;
    w[18] = Low32(info.mHardwareDevice);
}

// A system info passed to a fake is recorded as two events of its Mac words.
void RecInfo(const std::string& name, const TbSoundSystemInfo& info) {
    uint32_t w[kSysWords];
    DumpSys(info, w);
    ev((name + ".a").c_str(), {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9]});
    ev((name + ".b").c_str(), {w[10], w[11], w[12], w[13], w[14], w[15], w[16], w[17], w[18]});
}

// ---- the world -------------------------------------------------------------------------------------------------------------

alignas(16) uint8_t gChannelBytes[sizeof(TbSoundPipeChannel) + 16];
TbSoundPipeChannel* Channel() { return reinterpret_cast<TbSoundPipeChannel*>(gChannelBytes); }

TbSoundSystemInfo gSys;
TbSoundSampleInfo gSamp;
alignas(16) uint8_t gAccessBytes[16];
CAudioComponentAccess* Access() { return reinterpret_cast<CAudioComponentAccess*>(gAccessBytes); }

}  // namespace

// The pointer the next Module.Close call clears when the queue "Module.Close.clear" answers non-zero (the module drops its owner's
// pointer from inside Close, the case the owner's second NULL test after Close is for); set only by the Close and CloseModule ops.
TbSoundSystemModule** gOwnerSlot = nullptr;

// ---- the fakes ---------------------------------------------------------------------------------------------------------------

// The module every case uses: derived from the base (not from CSprocketsModule) with no members, so that it has the same size
// as CSprocketsModule and can be built in its place by the constructor fake below (the "FakeModule-like object in place").
class FakeModule : public TbSoundSystemModule {
public:
    FakeModule() : TbSoundSystemModule() {}
    ~FakeModule() override { ev("Module.~", {lab(this)}); }
    TbError Init(const TbSoundSystemInfo& info) override {
        ev("Module.Init", {lab(this)});
        RecInfo("Module.Init.info", info);
        return TbError(static_cast<int32_t>(ret("Module.Init", 0)));
    }
    void Close() override {
        ev("Module.Close", {lab(this)});
        if (gOwnerSlot != nullptr && ret("Module.Close.clear", 0) != 0) *gOwnerSlot = nullptr;
    }
    TbError StreamData(TbSoundPipeChannel*) override {
        ev("Module.StreamData", {lab(this)});
        return TbError(-77);
    }
    TbError StopChannel(uint32_t channel) override {
        ev("Module.StopChannel", {lab(this), channel});
        return TbError(static_cast<int32_t>(ret("Module.StopChannel", 0)));
    }
    TbError StopAll() override {
        ev("Module.StopAll", {lab(this)});
        return TbError(static_cast<int32_t>(ret("Module.StopAll", 0)));
    }
    bool SupportsEnvironmentalEffects() override {
        ev("Module.Supports", {lab(this)});
        return ret("Module.Supports", 1) != 0;
    }
    TbError SetChannelVolume(uint32_t, TbSoundSampleInfo&) override {
        ev("Module.SetChannelVolume", {lab(this)});
        return TbError(-77);
    }
};
static_assert(sizeof(FakeModule) == sizeof(CSprocketsModule), "FakeModule replaces a CSprocketsModule in place");

// The constructor p-sound will provide: here it records and turns the block into a FakeModule.
CSprocketsModule::CSprocketsModule() {
    ev("CSprocketsModule", {lab(this)});
    new (static_cast<void*>(this)) FakeModule();
}
void CSprocketsModule::Close() {}
TbError CSprocketsModule::StreamData(TbSoundPipeChannel*) { return TbError(-77); }
TbError CSprocketsModule::StopChannel(uint32_t) { return TbError(-77); }
TbError CSprocketsModule::SetChannelVolume(uint32_t, TbSoundSampleInfo&) { return TbError(-77); }
#include "sprockets_module_fakes.h"

// The fake store's member mChannels is the real header's CAutoArrayPtr<TbSoundPipeChannel> (always null here); its destructor
// would pull in TbSoundPipeChannel's, which this unit does not link, so the instance is a no-op in this unit only.
template<> CAutoArrayPtr<TbSoundPipeChannel>::~CAutoArrayPtr() {}

TbSoundSampleStore::TbSoundSampleStore(TbSoundCore* core, uint32_t maxChannels)
    : mChannels(static_cast<TbSoundPipeChannel*>(nullptr)) {
    // The real store is 0x10 bytes on the Mac but 24 on the host, the module's size: the "new" event just logged for this
    // block (the allocation observer cannot tell the two by size) is rewritten to the Mac's size.
    if (on() && tpw_cl_n > 0 && tpw_cl_n <= tpw_cl_capacity) {
        TpwCallEvent& last = tpw_cl_log[tpw_cl_n - 1];
        if (std::strcmp(last.name, "new") == 0 && last.nints >= 2 && last.ints[1] == lab(this)) last.ints[0] = 0x10;
    }
    ev("Store.ctor", {lab(this), lab(core), maxChannels});
}
TbSoundSampleStore::~TbSoundSampleStore() {
    ev("Store.~", {lab(this)});
}
TbError TbSoundSampleStore::Init(uint32_t channelsToUse) {
    ev("Store.Init", {lab(this), channelsToUse});
    return TbError(static_cast<int32_t>(ret("Store.Init", 0)));
}
int32_t TbSoundSampleStore::ReleaseAllPipes() {
    ev("Store.ReleaseAllPipes", {lab(this)});
    return 0;
}
uint32_t TbSoundSampleStore::GetNumberOfUsedChannels() {
    ev("Store.GetNumberOfUsedChannels", {lab(this)});
    return static_cast<uint32_t>(ret("Store.Used", 0));
}
uint32_t TbSoundSampleStore::GetMaxNumberOfChannels() {
    ev("Store.GetMaxNumberOfChannels", {lab(this)});
    return static_cast<uint32_t>(ret("Store.Max", 0));
}
TbSoundPipeChannel* TbSoundSampleStore::operator[](const CUniqueHandle& handle) const {
    ev("Store.index", {lab(this), handle.mValue});
    return ret("Store.index", 0) != 0 ? Channel() : nullptr;
}
void* TbSoundSampleStore::CreateNewSample(void* handle, TbSoundSampleInfo& info, uint32_t& extra) {
    ev("Store.CreateNewSample", {lab(this), Low32(handle), lab(&info)});
    extra = static_cast<uint32_t>(ret("Store.out", 0));
    return Cookie(ret("Store.sample", 0));
}
void* TbSoundSampleStore::CreateNewControl(void* handle, TbSoundSampleInfo& info) {
    ev("Store.CreateNewControl", {lab(this), Low32(handle), lab(&info)});
    return Cookie(ret("Store.control", 0));
}

uint32_t TbSoundPipeChannel::GetIndex() {
    ev("Channel.GetIndex", {lab(this)});
    return static_cast<uint32_t>(ret("Channel.index", 0));
}
bool TbSoundPipeChannel::ReleasePipe(bool release) {
    ev("Channel.ReleasePipe", {lab(this), release});
    return ret("Channel.release", 0) != 0;
}
uint32_t TbSoundPipeChannel::GetActualVolume() const {
    ev("Channel.GetActualVolume", {lab(this)});
    return static_cast<uint32_t>(ret("Channel.volume", 0));
}
uint32_t TbSoundPipeChannel::GetPriority() {
    ev("Channel.GetPriority", {lab(this)});
    return static_cast<uint32_t>(ret("Channel.priority", 0));
}

// The command bodies are unit w79d's: each records its name, the access object and the words it would read.
namespace SystemCommand {

TbError SetSystemInfo::Execute(CAudioComponentAccess* access) const {
    ev("SetSystemInfo.Execute", {lab(access)});
    RecInfo("SetSystemInfo.info", *mInfo);
    return TbError(static_cast<int32_t>(ret("Exec.SetSystemInfo", 0)));
}
TbError SetSampleInfo::Execute(CAudioComponentAccess* access) const {
    ev("SetSampleInfo.Execute", {lab(access), Low32(mHandle), lab(mInfo)});
    return TbError(static_cast<int32_t>(ret("Exec.SetSampleInfo", 0)));
}
#define HANDLE_EXECUTE(Class) \
    TbError Class::Execute(CAudioComponentAccess* access) const { \
        ev(#Class ".Execute", {lab(access), Low32(mHandle)}); \
        return TbError(0); \
    }
HANDLE_EXECUTE(Stop)
HANDLE_EXECUTE(Pause)
HANDLE_EXECUTE(Restart)
HANDLE_EXECUTE(Start)
#define PLAIN_EXECUTE(Class) \
    TbError Class::Execute(CAudioComponentAccess* access) const { \
        ev(#Class ".Execute", {lab(access), 0}); \
        return TbError(0); \
    }
PLAIN_EXECUTE(UpdateAllVolumes)
PLAIN_EXECUTE(Process)
PLAIN_EXECUTE(StopAll)
PLAIN_EXECUTE(PauseAll)
PLAIN_EXECUTE(RestartAll)

}  // namespace SystemCommand

namespace {

// A command that records its Execute (the test hands it to SystemCommand).
class TestCommand : public SystemCommand::Command {
public:
    TbError Execute(CAudioComponentAccess* access) const override {
        ev("Command.Execute", {lab(this), lab(access)});
        return TbError(static_cast<int32_t>(ret("Exec", 0)));
    }
};
TestCommand gTestCommand;

// The commands with no constructor of their own to test: a derived class that is concrete.
class PlainCommand : public SystemCommand::Command {
public:
    TbError Execute(CAudioComponentAccess*) const override { return TbError(0); }
};
class PlainHandle : public SystemCommand::HandleCommand {
public:
    using HandleCommand::HandleCommand;
    TbError Execute(CAudioComponentAccess*) const override { return TbError(0); }
};
class PlainCopy : public CMemoryCopy::CCopy {
public:
    void Copy(void*, const void*, uint32_t) override {}
};
class PlainAccess : public CAudioComponentAccess {
public:
    TbError GetSystemInfo(TbSoundSystemInfo&) override { return TbError(0); }
    TbError GetSampleInfo(void*, TbSoundSampleInfo&) override { return TbError(0); }
    void* NewSample(void*, TbSoundSampleInfo&) override { return nullptr; }
    void* NewControl(void*, TbSoundSampleInfo&) override { return nullptr; }
    TbError SystemCommand(const SystemCommand::Command&) override { return TbError(0); }
    TbSoundSystemModule* GetSystemModule() const override { return nullptr; }
    TbSoundSampleStore* GetSampleStore() const override { return nullptr; }
};

// ---- the component that records every slot --------------------------------------------------------------------------------

class FakeComponent : public CAudioComponent {
public:
    FakeComponent() : CAudioComponent() {}
    ~FakeComponent() override { ev("Fake.~", {lab(this)}); }
    TbError Init(const TbSoundSystemInfo& info) override {
        ev("Fake.Init", {lab(this)});
        RecInfo("Fake.Init.info", info);
        return TbError(static_cast<int32_t>(ret("Fake.Init", 0)));
    }
    TbError GetSystemInfo(TbSoundSystemInfo& info) override {
        ev("Fake.GetSystemInfo", {lab(this), lab(&info)});
        return TbError(static_cast<int32_t>(ret("Fake.GetSystemInfo", 0)));
    }
    TbError GetSampleInfo(void* handle, TbSoundSampleInfo& info) override {
        ev("Fake.GetSampleInfo", {lab(this), Low32(handle), lab(&info)});
        return TbError(static_cast<int32_t>(ret("Fake.GetSampleInfo", 0)));
    }
    void* NewSample(void* handle, TbSoundSampleInfo& info) override {
        ev("Fake.NewSample", {lab(this), Low32(handle), lab(&info)});
        return Cookie(ret("Fake.NewSample", 0));
    }
    void* NewControl(void* handle, TbSoundSampleInfo& info) override {
        ev("Fake.NewControl", {lab(this), Low32(handle), lab(&info)});
        return Cookie(ret("Fake.NewControl", 0));
    }
    TbError SystemCommand(const SystemCommand::Command& command) override {
        ev("Fake.SystemCommand", {lab(this), lab(&command)});
        return TbError(static_cast<int32_t>(ret("Fake.SystemCommand", 0)));
    }
    TbSoundSystemModule* GetSystemModule() const override {
        ev("Fake.GetSystemModule", {lab(this)});
        return static_cast<TbSoundSystemModule*>(Cookie(ret("Fake.GetSystemModule", 0)));
    }
    TbSoundSampleStore* GetSampleStore() const override {
        ev("Fake.GetSampleStore", {lab(this)});
        return static_cast<TbSoundSampleStore*>(Cookie(ret("Fake.GetSampleStore", 0)));
    }
};

}  // namespace


// ---- entry points: the core, the factory, the copier and the command constructors ------------------------------------------

namespace {

// The parameter words of tpw_sc_run.
enum { P_HAS_MODULE = 0, P_HAS_STORE = 1, P_HANDLE = 2, P_FLAG = 3, P_CHANNEL = 4, P_COUNT = 5, P_SYS = 8, P_SAMP = 27 };
// The result words.
enum { O_RESULT = 0, O_SYS = 1, O_SAMP = 20, O_MODULE = 25, O_STORE = 26, O_X = 27 };

int64_t PW(const void* pointer) {
    return lab(pointer);
}

void DumpSamp(const TbSoundSampleInfo& info, uint32_t* out) {
    out[0] = info.mField0C;
    out[1] = info.mFlags;
    out[2] = info.mField18;
    out[3] = info.mVolume;
    out[4] = info.mPriority;
}

enum Op {
    kCtor = 0, kDtor, kModify, kInit, kClose, kGetSystemInfo, kStopAndRelease, kGetSampleInfo, kNewSample, kNewControl,
    kSystemCommand, kGetModule, kGetStore,
    kGenerate = 20, kCloseModule, kCopyInitCtor, kStandardCtor, kCopyCtor, kStandardCopy, kAccessCtor, kComponentCtor
};

// The command constructors in the order of the python table.
enum CommandKind {
    kCmdBase = 0, kCmdHandle, kCmdSetSystemInfo, kCmdSetSampleInfo, kCmdStop, kCmdPause, kCmdRestart, kCmdStart,
    kCmdUpdateAllVolumes, kCmdProcess, kCmdStopAll, kCmdPauseAll, kCmdRestartAll
};

}  // namespace

extern "C" {

int32_t tpw_sc_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(CSprocketsModule));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSampleStore));
        case 2: return static_cast<int32_t>(sizeof(CMemoryCopy::CStandardCopy));
        case 3: return static_cast<int32_t>(sizeof(TbSoundCore));
        case 4: return static_cast<int32_t>(sizeof(CAudioComponent));
    }
    return 0;
}

void tpw_sc_run(int32_t op, const uint32_t* p, uint32_t* out) {
    std::memset(out, 0, 64 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    const bool hasModule = p[P_HAS_MODULE] != 0;
    const bool hasStore = p[P_HAS_STORE] != 0;
    void* handle = Cookie(p[P_HANDLE]);
    const int32_t flag = static_cast<int32_t>(p[P_FLAG]);

    // ---- the world, made with the log off
    FillSys(gSys, p + P_SYS);
    gSamp.mField0C = p[P_SAMP];
    gSamp.mFlags = p[P_SAMP + 1];
    gSamp.mField18 = p[P_SAMP + 2];
    gSamp.mVolume = p[P_SAMP + 3];
    gSamp.mPriority = p[P_SAMP + 4];
    CChannelState::mChannelState.mCount.mValue = static_cast<int32_t>(p[P_COUNT]);
    tpw_cl_label(4, Channel());
    tpw_cl_label(5, &gSamp);
    tpw_cl_label(6, &gSys);
    tpw_cl_label(7, &gTestCommand);
    tpw_cl_label(9, Access());

    alignas(16) static uint8_t coreBytes[sizeof(TbSoundCore) + 16];
    TbSoundCore* core = nullptr;
    TbSoundSystemModule* factoryModule = nullptr;
    TbSoundSampleStore* store = nullptr;
    if (op <= kGetStore) {
        if (op == kCtor) {
            std::memset(coreBytes, 0xA5, sizeof coreBytes);
            core = reinterpret_cast<TbSoundCore*>(coreBytes);
            CMemoryCopy::_mpCopy = nullptr;
        } else {
            core = new TbSoundCore();
            tpw_cl_label(8, CMemoryCopy::_mpCopy);
            if (hasModule) {
                core->mModule = new FakeModule();
                tpw_cl_label(2, core->mModule);
            }
            if (hasStore) {
                core->mStore = new TbSoundSampleStore(core, 0);
                tpw_cl_label(3, core->mStore);
            }
        }
        tpw_cl_label(1, core);
    } else if (op == kGenerate || op == kCloseModule) {
        if (hasModule) {
            factoryModule = new FakeModule();
            tpw_cl_label(2, factoryModule);
        }
        store = new TbSoundSampleStore(nullptr, 0);
        tpw_cl_label(3, store);
    } else if (op == kCopyInitCtor) {
        // with hasModule, a copier from an earlier init is replaced without being freed
        CMemoryCopy::_mpCopy = hasModule ? new CMemoryCopy::CStandardCopy() : nullptr;
        tpw_cl_label(8, CMemoryCopy::_mpCopy);
    }

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    uint32_t x[8] = {};
    int64_t result = 0;
    switch (op) {
        case kCtor:
            new (core) TbSoundCore();
            x[0] = static_cast<uint32_t>(PW(CMemoryCopy::_mpCopy));
            x[1] = dynamic_cast<CMemoryCopy::CStandardCopy*>(CMemoryCopy::_mpCopy) != nullptr;
            x[2] = 0xA5A5A5A5u;   // the Mac word at +0x20 is not written by the constructor
            x[3] = core->mId;
            x[4] = static_cast<uint32_t>(core->mRefCount);
            x[5] = static_cast<uint32_t>(PW(core->mNext));
            x[6] = static_cast<uint32_t>(PW(core->mPrevious));
            x[7] = 1;   // the vptrs and the vtable slots are checked on the PowerPC side
            break;
        case kDtor:
            if (flag > 0) {
                delete core;
            } else {
                core->~TbSoundCore();
            }
            x[0] = static_cast<uint32_t>(PW(CMemoryCopy::_mpCopy));
            break;
        case kModify:
            core->ModifyMaxBuffers(gSys);
            break;
        case kInit:
            result = static_cast<int32_t>(core->Init(gSys));
            break;
        case kClose:
            gOwnerSlot = &core->mModule;
            core->Close();
            gOwnerSlot = nullptr;
            break;
        case kGetSystemInfo:
            result = static_cast<int32_t>(core->GetSystemInfo(gSys));
            break;
        case kStopAndRelease:
            result = static_cast<int32_t>(core->StopAndReleasePipe(p[P_CHANNEL] != 0 ? Channel() : nullptr, flag != 0));
            break;
        case kGetSampleInfo:
            result = static_cast<int32_t>(core->GetSampleInfo(handle, gSamp));
            break;
        case kNewSample:
            result = Low32(core->NewSample(handle, gSamp));
            break;
        case kNewControl:
            result = Low32(core->NewControl(handle, gSamp));
            break;
        case kSystemCommand:
            result = static_cast<int32_t>(core->SystemCommand(gTestCommand));
            break;
        case kGetModule:
            result = PW(core->GetSystemModule());
            break;
        case kGetStore:
            result = PW(core->GetSampleStore());
            break;
        case kGenerate: {
            CModuleFactory factory;
            result = static_cast<int32_t>(factory.GenerateModule(factoryModule, gSys, store));
            x[0] = static_cast<uint32_t>(PW(factoryModule));
            break;
        }
        case kCloseModule: {
            CModuleFactory factory;
            gOwnerSlot = &factoryModule;
            factory.CloseModule(factoryModule);
            gOwnerSlot = nullptr;
            x[0] = static_cast<uint32_t>(PW(factoryModule));
            break;
        }
        case kCopyInitCtor: {
            alignas(16) static uint8_t bytes[16];
            std::memset(bytes, 0xA5, sizeof bytes);
            new (bytes) CMemoryCopyInit();
            x[0] = static_cast<uint32_t>(PW(CMemoryCopy::_mpCopy));
            x[1] = dynamic_cast<CMemoryCopy::CStandardCopy*>(CMemoryCopy::_mpCopy) != nullptr;
            x[2] = 0xA5A5A5A5u;
            break;
        }
        case kStandardCtor: {
            alignas(16) static uint8_t bytes[16];
            std::memset(bytes, 0xA5, sizeof bytes);
            CMemoryCopy::CCopy* copier = new (bytes) CMemoryCopy::CStandardCopy();
            x[0] = typeid(*copier) == typeid(CMemoryCopy::CStandardCopy);
            x[1] = 1;   // the Mac word at +4 is not written
            break;
        }
        case kCopyCtor: {
            alignas(16) static uint8_t bytes[16];
            new (bytes) PlainCopy();
            x[0] = 1;   // CCopy has no state; the Mac's vptr is checked on the PowerPC side
            break;
        }
        case kAccessCtor: {
            alignas(16) static uint8_t bytes[16];
            new (bytes) PlainAccess();
            x[0] = 1;
            break;
        }
        case kComponentCtor: {
            alignas(16) static uint8_t bytes[sizeof(CAudioComponent) + 16];
            std::memset(bytes, 0xA5, sizeof bytes);
            auto* component = new (bytes) CAudioComponent();
            x[0] = static_cast<uint32_t>(PW(component->mNext));
            x[1] = static_cast<uint32_t>(PW(component->mPrevious));
            x[2] = component->mId;
            x[3] = static_cast<uint32_t>(component->mRefCount);
            x[4] = 1;   // the vptrs and the vtable slots are checked on the PowerPC side
            break;
        }
        case kStandardCopy: {
            alignas(16) static uint8_t dest[48];
            alignas(16) static uint8_t source[48];
            alignas(16) static uint8_t bytes[16];
            std::memset(dest, 0xEE, sizeof dest);
            uint32_t seed = p[P_HANDLE];
            for (auto& b : source) {
                seed = seed * 1664525u + 1013904223u;
                b = static_cast<uint8_t>(seed >> 24);
            }
            CMemoryCopy::CCopy* copier = new (bytes) CMemoryCopy::CStandardCopy();
            copier->Copy(dest + p[P_COUNT], source, static_cast<uint32_t>(flag));
            std::memcpy(out + O_X, dest, sizeof dest);
            break;
        }
        default:
            break;
    }
    gTrack = false;

    out[O_RESULT] = static_cast<uint32_t>(result);
    DumpSys(gSys, out + O_SYS);
    DumpSamp(gSamp, out + O_SAMP);
    if (op <= kGetStore && op != kDtor) {
        out[O_MODULE] = static_cast<uint32_t>(PW(core->mModule));
        out[O_STORE] = static_cast<uint32_t>(PW(core->mStore));
    }
    if (op != kStandardCopy) std::memcpy(out + O_X, x, sizeof x);
}

// The copier initialiser's destructor: the object and its copier are made with the log off.
void tpw_sc_copyinit_dtor(int32_t flag, uint32_t* out) {
    std::memset(out, 0, 8 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    auto* init = new CMemoryCopyInit();
    tpw_cl_label(1, init);
    tpw_cl_label(8, CMemoryCopy::_mpCopy);
    tpw_cl_on = 1;
    if (flag > 0) {
        delete init;
    } else {
        init->~CMemoryCopyInit();
    }
    out[0] = static_cast<uint32_t>(PW(CMemoryCopy::_mpCopy));
    out[1] = CMemoryCopy::_mpCopy != nullptr;
}

// One command constructor, then its Execute through the base pointer. out: the handle word, the info label, the answer.
void tpw_sc_command(int32_t kind, uint32_t handleWord, uint32_t* out) {
    std::memset(out, 0, 8 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    uint32_t words[kSysWords];
    for (int i = 0; i < kSysWords; ++i) words[i] = 0x100u + static_cast<uint32_t>(i);
    FillSys(gSys, words);
    tpw_cl_label(5, &gSamp);
    tpw_cl_label(6, &gSys);
    tpw_cl_label(9, Access());
    tpw_cl_on = 1;
    alignas(16) static uint8_t bytes[64];
    std::memset(bytes, 0xA5, sizeof bytes);
    void* handle = Cookie(handleWord);
    using namespace SystemCommand;
    const Command* command = nullptr;
    switch (kind) {
        case kCmdBase: {
            auto* c = new (bytes) PlainCommand();
            command = c;
            break;
        }
        case kCmdHandle: {
            auto* c = new (bytes) PlainHandle(handle);
            out[0] = Low32(c->mHandle);
            command = c;
            break;
        }
        case kCmdSetSystemInfo: {
            auto* c = new (bytes) SetSystemInfo(gSys);
            out[1] = static_cast<uint32_t>(PW(c->mInfo));
            command = c;
            break;
        }
        case kCmdSetSampleInfo: {
            auto* c = new (bytes) SetSampleInfo(handle, gSamp);
            out[0] = Low32(c->mHandle);
            out[1] = static_cast<uint32_t>(PW(c->mInfo));
            command = c;
            break;
        }
        case kCmdStop: {
            auto* c = new (bytes) Stop(handle);
            out[0] = Low32(c->mHandle);
            command = c;
            break;
        }
        case kCmdPause: {
            auto* c = new (bytes) Pause(handle);
            out[0] = Low32(c->mHandle);
            command = c;
            break;
        }
        case kCmdRestart: {
            auto* c = new (bytes) Restart(handle);
            out[0] = Low32(c->mHandle);
            command = c;
            break;
        }
        case kCmdStart: {
            auto* c = new (bytes) Start(handle);
            out[0] = Low32(c->mHandle);
            command = c;
            break;
        }
        case kCmdUpdateAllVolumes: command = new (bytes) UpdateAllVolumes(); break;
        case kCmdProcess: command = new (bytes) Process(); break;
        case kCmdStopAll: command = new (bytes) SystemCommand::StopAll(); break;
        case kCmdPauseAll: command = new (bytes) PauseAll(); break;
        case kCmdRestartAll: command = new (bytes) RestartAll(); break;
    }
    out[2] = static_cast<uint32_t>(static_cast<int32_t>(command->Execute(Access())));
    out[3] = 1;   // the vtable and the Execute address are checked on the PowerPC side
}

// ---- the component chain ---------------------------------------------------------------------------------------------------

namespace {
constexpr int kNodes = 6;
CAudioComponent* gNodes[kNodes];
int gKind[kNodes];          // 1 a real CAudioComponent, 2 a FakeComponent
bool gDestroyed[kNodes];    // destroyed by an explicit destructor call (no delete)
CAudioComponent* Node(int32_t slot) {
    return slot >= 0 && slot < kNodes ? gNodes[slot] : nullptr;
}
}  // namespace

void tpw_sc_chain_begin() {
    tpw_cl_on = 0;
    gTrack = false;
    for (int i = 0; i < kNodes; ++i) {
        gNodes[i] = nullptr;
        gKind[i] = 0;
        gDestroyed[i] = false;
    }
    uint32_t words[kSysWords];
    for (int i = 0; i < kSysWords; ++i) words[i] = 0x100u + static_cast<uint32_t>(i);
    FillSys(gSys, words);
    tpw_cl_label(5, &gSamp);
    tpw_cl_label(6, &gSys);
    tpw_cl_label(7, &gTestCommand);
    tpw_cl_on = 1;
}

// op: 0 new (a slot, b kind), 1 Chain(a, b), 2 Init, 3 UnchainAndRelease, 4 GetComponent(a, id b), 5 set mId, 6 a forwarder
// (b which, c the handle), 7 Release, 8 AddRef, 9 destructor (b the flag). skip: the python side judged the call unsafe.
void tpw_sc_chain_op(int32_t op, int32_t a, int32_t b, int32_t c, int32_t skip, int64_t* out) {
    out[0] = 0;
    out[1] = 0;
    if (skip != 0) {
        out[0] = -99;
        return;
    }
    CAudioComponent* node = Node(a);
    void* handle = Cookie(static_cast<uint32_t>(c));
    switch (op) {
        case 0: {
            tpw_cl_on = 0;
            gNodes[a] = b == 2 ? new FakeComponent() : new CAudioComponent();
            gKind[a] = b;
            gDestroyed[a] = false;
            tpw_cl_label(11 + a, gNodes[a]);
            tpw_cl_on = 1;
            out[0] = lab(gNodes[a]);
            break;
        }
        case 1:
            out[0] = static_cast<int32_t>(node->Chain(Node(b)));
            break;
        case 2:
            out[0] = static_cast<int32_t>(node->Init(gSys));
            break;
        case 3:
            out[0] = node->UnchainAndRelease();
            break;
        case 4:
            out[0] = lab(node->GetComponent(static_cast<uint32_t>(b)));
            break;
        case 5:
            node->mId = static_cast<uint32_t>(b);
            break;
        case 6:
            switch (b) {
                case 0: out[0] = static_cast<int32_t>(node->GetSystemInfo(gSys)); break;
                case 1: out[0] = static_cast<int32_t>(node->GetSampleInfo(handle, gSamp)); break;
                case 2: out[0] = Low32(node->NewSample(handle, gSamp)); break;
                case 3: out[0] = Low32(node->NewControl(handle, gSamp)); break;
                case 4: out[0] = static_cast<int32_t>(node->SystemCommand(gTestCommand)); break;
                case 5: out[0] = Low32(node->GetSystemModule()); break;
                case 6: out[0] = Low32(node->GetSampleStore()); break;
            }
            break;
        case 7:
            out[0] = node->Release();
            break;
        case 8:
            out[0] = node->AddRef();
            break;
        case 9:
            if (static_cast<int16_t>(b) > 0) {
                delete node;
            } else {
                node->~CAudioComponent();
                gDestroyed[a] = true;
            }
            break;
    }
}

// The state of a node after an operation: 0 never made, 1 gone, 2 alive; the kind, the links as labels, mId, the count.
void tpw_sc_chain_slot(int32_t slot, int64_t* out) {
    for (int i = 0; i < 6; ++i) out[i] = 0;
    CAudioComponent* node = gNodes[slot];
    if (node == nullptr) return;
    if (gDestroyed[slot] || lab(node) <= 0) {
        out[0] = 1;
        return;
    }
    out[0] = 2;
    out[1] = gKind[slot];
    out[2] = lab(node->mNext);
    out[3] = lab(node->mPrevious);
    out[4] = node->mId;
    out[5] = node->mRefCount;
}

}  // extern "C"
