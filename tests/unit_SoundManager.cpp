// Fakes and test entry points for unit w79e, the sound manager (libtpw_unit_SoundManager.dylib, driven by tests/test_SoundManager.py
// in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundManager.cpp  TbSoundManager (the unit under test)
// Real: the core (w79b: TbSoundCore, CAudioComponent and the SystemCommand constructors), the info records (w77), TbRefRelease
// (w78), the handle values (w79a) and the bullfrog strings. Fakes, each recording what the original's callee is given (the
// PowerPC side records the same from its stubs):
//   the device module CSprocketsModule (p-sound), the sample store (w79c) and the channel getters (w79c), as in the w79b unit
//   the command Execute bodies (w79d): SetSystemInfo, UpdateAllVolumes and Start record; the others are empty
//   TbMapStreamer::Instance and SetDroppable (the real streamer is w70's; the bank deque is raw storage built by the case)
//   the bullfrog storage and file (CFileStorage::OpenFile and FullPath record), CFileOperation::GetBankPath, TbSoundBankInfo's
//       file constructor and CBankFactory (GenerateBank records the bank info and answers a block from a queue)
//   FakeComponent, a CAudioComponent that records NewSample and NewControl (the manager's audio system)
//   HookedManager, a manager whose GetSystemInfo (slot +0x08) records and answers from queues
//
// Labels: 1 the manager, 3 the store, 5 the sample info, 8 the global copier, 10 and 15 the two audio components, 11..14 the four
// positions, 21.. the banks' storages, 31.. the bank containers, 41.. their cached banks, 100 and up the objects the code makes
// (the file, the bank the factory answers). "new" events are recorded only for the global copier (the Mac size 4 of the host's 8).
//
// tpw_sm_run drives one operation on a manager it builds from the parameter words; see the P_ and O_ enums.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "CSprocketsModule.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundManager.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "calllog.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

bool gTrack;   // "new" events are recorded only while the code under test runs

// The runtime's new: blocks are filled with 0xA5; the copier's is recorded with the Mac's size.
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    if (size == sizeof(CMemoryCopy::CStandardCopy)) ev("new", {4, newlab(block)});
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

float BitsToFloat(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

uint32_t FloatBits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

// A NaN compares as one value: the host's and the emulator's default NaNs differ in sign.
uint32_t Canon(uint32_t bits) {
    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0 ? 0x7FC00000u : bits;
}

const char* Text(const TbDynamicStringTemplate<char>& s) {
    return s.mData != nullptr ? s.mData : "";
}

// The words of a system info the constructor and the setters define (the others are stack garbage in the original): +0, +4,
// +8, the mask, the buffers to use, the window, +0x40, and the listener pointers when their mask bits say they were set.
void RecInfo(const char* name, int64_t first, const TbSoundSystemInfo& i) {
    const uint32_t mask = i.mMask;
    ev(name, {first, i.mField0, i.mField4, i.mField8, mask, i.mBuffersToUse, Low32(i.mHWnd), i.mField40,
              (mask & 4) != 0 ? lab(i.mListenerPosition) : 0, (mask & 8) != 0 ? lab(i.mFrontDir) : 0,
              (mask & 8) != 0 ? lab(i.mTopDir) : 0, (mask & 0x10) != 0 ? lab(i.mListenerVelocity) : 0});
}

// ---- the world -------------------------------------------------------------------------------------------------------------

TbSoundSampleInfo gSamp;
TbSoundPosition gPos[4];
uint32_t gOut;      // the uint32_t& argument of GetMaxNumberOfChannels and GetDirectSoundObject
void* gControl;     // the void*& argument of ConstructSample and PlaySample
std::string gPath;  // the manager's sound path after the operation

constexpr int kMaxBanks = 3;
CBankContainer* gBankPointers[kMaxBanks];
alignas(16) uint8_t gContainerBytes[kMaxBanks][sizeof(CBankContainer) + 16];
alignas(16) uint8_t gBankObjects[kMaxBanks][16];
alignas(16) uint8_t gStreamerBytes[sizeof(TbMapStreamer) + 16];

TbMapStreamer* Streamer() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

}  // namespace

// The string library's printf-style formatters are not used here (the bullfrog library supplies them on the Mac).
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- the fakes: the device module, the sample store and the channel getters (as in the w79b unit) -------------------------

CSprocketsModule::CSprocketsModule() {}
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
    return nullptr;
}
void* TbSoundSampleStore::CreateNewSample(void* handle, TbSoundSampleInfo& info, uint32_t& extra) {
    ev("Store.CreateNewSample", {lab(this), Low32(handle), lab(&info)});
    extra = 0;
    return nullptr;
}
void* TbSoundSampleStore::CreateNewControl(void* handle, TbSoundSampleInfo& info) {
    ev("Store.CreateNewControl", {lab(this), Low32(handle), lab(&info)});
    return nullptr;
}

uint32_t TbSoundPipeChannel::GetIndex() { return 0; }
bool TbSoundPipeChannel::ReleasePipe(bool) { return false; }
uint32_t TbSoundPipeChannel::GetActualVolume() const { return 0; }
uint32_t TbSoundPipeChannel::GetPriority() { return 0; }

// ---- the command bodies (w79d): three record what the manager handed to SystemCommand, the others are empty ----------------

namespace SystemCommand {

TbError SetSystemInfo::Execute(CAudioComponentAccess* access) const {
    RecInfo("SetSystemInfo.Execute", lab(access), *mInfo);
    return TbError(static_cast<int32_t>(ret("Exec.SetSystemInfo", 0)));
}
TbError UpdateAllVolumes::Execute(CAudioComponentAccess* access) const {
    ev("UpdateAllVolumes.Execute", {lab(access)});
    return TbError(static_cast<int32_t>(ret("Exec.UpdateAllVolumes", 0)));
}
TbError Start::Execute(CAudioComponentAccess* access) const {
    ev("Start.Execute", {lab(access), Low32(mHandle)});
    return TbError(static_cast<int32_t>(ret("Exec.Start", 0)));
}
TbError SetSampleInfo::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Stop::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Pause::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Restart::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopLoop::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopAllVolumeControl::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Process::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopAll::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError PauseAll::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError RestartAll::Execute(CAudioComponentAccess*) const { return TbError(0); }

}  // namespace SystemCommand

// ---- the streamer: Instance answers the raw storage the case built; SetDroppable (0x100231fc) is a stand-in that records ----

TbMapStreamer* TbMapStreamer::Instance() {
    return Streamer();
}
void TbMapStreamer::SetDroppable(bool droppable) {
    ev("Streamer.SetDroppable", {lab(this), droppable});
}

// ---- the bullfrog storage and file ----------------------------------------------------------------------------------------

namespace bfl::FileSystem {

CFileStorage::CFileStorage() : mAccess(nullptr) {}
CFileStorage::CFileStorage(const CFileStorage& other) : mAccess(other.mAccess) {}
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    mAccess = other.mAccess;
    return *this;
}
CFileStorage::~CFileStorage() {}
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
// The open records every argument; the answer is the queue's (negative fails).
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* extra) {
    ev("Storage.OpenFile", {lab(this), lab(&file), mode, Low32(extra)}, {name});
    return TbError(static_cast<int32_t>(ret("Storage.Open", 0)));
}
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(-1); }
// The root of the storage is written to the first string.
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>& path, const TbDynamicStringTemplate<char>& name) {
    ev("Storage.FullPath", {lab(this)}, {Text(name)});
    path = "R:";
    return TbError(static_cast<int32_t>(ret("Storage.FullPath", 0)));
}
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::CreateDir(const char*) { return TbError(-1); }
TbError CFileStorage::Delete(const char*) { return TbError(-1); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(-1); }

// The file records its construction and destruction (GetBank makes one on its stack).
CFile::CFile() : mImplementation(nullptr) { ev("File.ctor", {newlab(this)}); }
CFile::~CFile() {
    ev("File.dtor", {lab(this)});
    forget(this);
}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }

}  // namespace bfl::FileSystem

// ---- the bank pieces: GetBankPath, TbSoundBankInfo's file constructor and the factory ---------------------------------------

// w72's GetBankPath is a static function that appends to its third argument; the original is a member of an empty
// CFileOperation (this in r3) that appends to the string in r5. The fake records the storage, both strings and the number, and
// appends "HD.sdt" to that string when the queue answers nonzero (default 1).
int32_t CFileOperation::GetBankPath(bfl::FileSystem::CFileStorage& storage, TbDynamicStringTemplate<char>& path,
                                    const TbDynamicStringTemplate<char>& name, int32_t flags) {
    ev("GetBankPath", {lab(&storage), flags}, {Text(name), Text(path)});
    const int32_t answer = static_cast<int32_t>(ret("GetBankPath", 1));
    if (answer != 0) const_cast<TbDynamicStringTemplate<char>&>(name) += "HD.sdt";
    return answer;
}

TbSoundBankInfo::TbSoundBankInfo(uint32_t type, const char* path, bfl::FileSystem::CFile* file) {
    mType = type;
    mPath = path;
    mFile = file;
}

CBankFactory::CBankFactory() {}
TbError CBankFactory::ConstructBank(TbBank*&, const TbSoundBankInfo&, CAudioComponent*) { return TbError(-1); }
// Records the bank info (type, the file, the component, the path) and answers a new block (nonzero in the queue) or NULL.
Bank::CBank* CBankFactory::GenerateBank(const TbSoundBankInfo& info, CAudioComponent* component) {
    ev("Factory.GenerateBank", {info.mType, lab(info.mFile), lab(component)}, {info.mPath});
    if (ret("GenBank", 0) == 0) return nullptr;
    void* block = std::malloc(16);
    std::memset(block, 0xA5, 16);
    newlab(block);
    return static_cast<Bank::CBank*>(block);
}

// ---- the audio component the manager makes samples through -----------------------------------------------------------------

namespace {

class FakeComponent : public CAudioComponent {
public:
    FakeComponent() : CAudioComponent() {}
    TbError GetSystemInfo(TbSoundSystemInfo&) override {
        ev("Fake.GetSystemInfo", {lab(this)});
        return TbError(-77);
    }
    TbError GetSampleInfo(void*, TbSoundSampleInfo&) override {
        ev("Fake.GetSampleInfo", {lab(this)});
        return TbError(-77);
    }
    // The calls record the sample info's words the manager may have changed before them.
    void* NewSample(void* handle, TbSoundSampleInfo& info) override {
        ev("Fake.NewSample", {lab(this), Low32(handle), lab(&info), info.mField0C, info.mFlags});
        return Cookie(ret("Fake.NewSample", 0));
    }
    void* NewControl(void* handle, TbSoundSampleInfo& info) override {
        ev("Fake.NewControl", {lab(this), Low32(handle), lab(&info), info.mField0C, info.mFlags});
        return Cookie(ret("Fake.NewControl", 0));
    }
    TbError SystemCommand(const SystemCommand::Command&) override {
        ev("Fake.SystemCommand", {lab(this)});
        return TbError(-77);
    }
    TbSoundSystemModule* GetSystemModule() const override {
        ev("Fake.GetSystemModule", {lab(this)});
        return nullptr;
    }
    TbSoundSampleStore* GetSampleStore() const override {
        ev("Fake.GetSampleStore", {lab(this)});
        return nullptr;
    }
};

// A manager whose GetSystemInfo (slot +0x08) records the request and answers from queues: the error, bits OR-ed into the mask,
// the max-buffers word and the word at +0 (the IDirectSound slot).
class HookedManager : public TbSoundManager {
public:
    TbError GetSystemInfo(TbSoundSystemInfo& info) override {
        RecInfo("Hook.GetSystemInfo", lab(this), info);
        info.mMask |= static_cast<uint32_t>(ret("Hook.mask", 0));
        info.mMaxBuffers = static_cast<uint32_t>(ret("Hook.max", 0));
        info.mField0 = static_cast<uint32_t>(ret("Hook.f0", 0));
        return TbError(static_cast<int32_t>(ret("Hook.err", 0)));
    }
};

FakeComponent* gComponents[2];

enum Param {
    P_MODE = 0, P_OPTS = 1, P_DIST = 2, P_ON = 3, P_VOL = 4, P_DROP = 8, P_AUDIO = 9, P_LISTENER = 10, P_NBANKS = 12,
    P_FLAGS = 13, P_CACHED = 16, P_STORE = 19, P_A = 20, P_INFO0C = 28, P_INFOFLAGS = 29, P_CONTROL = 30, P_POS = 32
};
enum Out {
    O_RESULT = 0, O_STATE = 1, O_X = 14, O_INFO = 22, O_CONTROL = 24, O_BANKS = 25, O_OUT = 28
};
enum Op {
    kCtor = 0, kDtor, kInit, kGetSoundPath, kSetListenerPosition, kGetListenerPosition, kSetAudioSystem, kSetDistance,
    kGetDistance, kIsQSound, kGetRolloff, kGetVolumeControl, kGetVolume, kVolumeIsOn, kUpdateVolume, kToggleVolume, kSetQSound,
    kOrientation, kGetMax, kGetDirect, kGetBankData, kIsBankDroppable, kSetDroppable, kGetBank, kConstruct, kPlay
};

// The manager's members as 13 words: options, distance bits, the listener and audio labels, the four control bytes, the four
// volume words, whether the volume control points at the volumes, its count and the droppable byte. `tail` false leaves the
// last three out (an index that ran past the volumes wrote into them).
void DumpState(TbSoundManager* m, uint32_t* w, bool tail) {
    w[0] = m->mOptions;
    w[1] = Canon(FloatBits(m->mDistanceMultiplier));
    w[2] = static_cast<uint32_t>(lab(m->mListenerPosition));
    w[3] = static_cast<uint32_t>(lab(m->mAudioSystem));
    w[4] = m->mControlOn[0] | (m->mControlOn[1] << 8) | (m->mControlOn[2] << 16) | (static_cast<uint32_t>(m->mControlOn[3]) << 24);
    for (int i = 0; i < 4; ++i) w[5 + i] = Canon(FloatBits(m->mVolumes[i]));
    if (tail) {
        w[9] = m->mVolumeControl.mVolumes == m->mVolumes;
        w[10] = m->mVolumeControl.mCount;
        w[11] = m->mDroppable;
    }
    gPath = Text(m->mSoundPath);
}

}  // namespace

extern "C" {

int32_t tpw_sm_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundManager));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSystemInfo));
        case 2: return static_cast<int32_t>(sizeof(CBankContainer));
    }
    return 0;
}

const char* tpw_sm_path() {
    return gPath.c_str();
}

// One operation. p: the parameter words (P_ enum), names: five 64-byte texts (the three bank names, the Init argument, the path
// the manager starts with), out: the results (O_ enum).
void tpw_sm_run(int32_t op, const uint32_t* p, const char* names, uint32_t* out) {
    std::memset(out, 0, 40 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    gPath.clear();

    // ---- the world, made with the log off
    gSamp.mField0C = p[P_INFO0C];
    gSamp.mFlags = p[P_INFOFLAGS];
    for (int i = 0; i < 4; ++i) {
        gPos[i].mX = BitsToFloat(p[P_POS + 3 * i]);
        gPos[i].mY = BitsToFloat(p[P_POS + 3 * i + 1]);
        gPos[i].mZ = BitsToFloat(p[P_POS + 3 * i + 2]);
        tpw_cl_label(11 + i, &gPos[i]);
    }
    tpw_cl_label(5, &gSamp);
    gOut = 0xA5A5A5A5u;
    gControl = Cookie(p[P_CONTROL]);

    alignas(16) static uint8_t rawBytes[sizeof(TbSoundManager) + 64];
    TbSoundManager* m;
    if (op == kCtor) {
        std::memset(rawBytes, 0xA5, sizeof rawBytes);
        m = reinterpret_cast<TbSoundManager*>(rawBytes);
        CMemoryCopy::_mpCopy = nullptr;
    } else {
        m = p[P_MODE] == 1 ? new HookedManager() : new TbSoundManager();
        tpw_cl_label(8, CMemoryCopy::_mpCopy);
        m->mOptions = p[P_OPTS];
        m->mDistanceMultiplier = BitsToFloat(p[P_DIST]);
        for (int i = 0; i < 4; ++i) {
            m->mControlOn[i] = static_cast<uint8_t>(p[P_ON] >> (8 * i));
            m->mVolumes[i] = BitsToFloat(p[P_VOL + i]);
        }
        m->mDroppable = static_cast<uint8_t>(p[P_DROP]);
        if (p[P_LISTENER] != 0) m->mListenerPosition = &gPos[(p[P_LISTENER] - 1) & 3];
        if (p[P_AUDIO] != 0) {
            if (gComponents[p[P_AUDIO] - 1] == nullptr) gComponents[p[P_AUDIO] - 1] = new FakeComponent();
            tpw_cl_label(p[P_AUDIO] == 1 ? 10 : 15, gComponents[p[P_AUDIO] - 1]);
            m->mAudioSystem = gComponents[p[P_AUDIO] - 1];
        }
        if (p[P_STORE] != 0) {
            m->mStore = new TbSoundSampleStore(m, 0);
            tpw_cl_label(3, m->mStore);
        }
        m->mSoundPath = names + 64 * 4;
    }
    tpw_cl_label(1, m);
    // the second audio component is a separate object for SetAudioSystem to receive
    if (op == kSetAudioSystem) {
        for (int i = 0; i < 2; ++i) {
            if (gComponents[i] == nullptr) gComponents[i] = new FakeComponent();
            tpw_cl_label(i == 0 ? 10 : 15, gComponents[i]);
        }
    }

    // the bank deque: raw storage of the streamer, containers built over 0xA5 with the members GetBank reads
    const uint32_t banks = p[P_NBANKS];
    CResize<CBankContainer*>* deque = new (&Streamer()->mBanks) CResize<CBankContainer*>();
    deque->mData = gBankPointers;
    deque->mSize = banks;
    tpw_cl_label(9, Streamer());
    for (uint32_t i = 0; i < banks && i < kMaxBanks; ++i) {
        auto* c = reinterpret_cast<CBankContainer*>(gContainerBytes[i]);
        std::memset(gContainerBytes[i], 0xA5, sizeof gContainerBytes[i]);
        new (&c->mName) TbDynamicStringTemplate<char>(names + 64 * i);
        new (&c->mStorage) bfl::FileSystem::CFileStorage();
        c->mFlags = p[P_FLAGS + i];
        std::memset(gBankObjects[i], 0xA5, 16);
        c->mBank = p[P_CACHED + i] != 0 ? reinterpret_cast<TbBank*>(gBankObjects[i]) : nullptr;
        gBankPointers[i] = c;
        tpw_cl_label(31 + i, c);
        tpw_cl_label(21 + i, &c->mStorage);
        if (p[P_CACHED + i] != 0) tpw_cl_label(41 + i, gBankObjects[i]);
    }
    const uint32_t a0 = p[P_A], a1 = p[P_A + 1], a2 = p[P_A + 2], a3 = p[P_A + 3];

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    int64_t result = 0;
    uint32_t x[8] = {};
    bool tail = true;
    switch (op) {
        case kCtor:
            new (m) TbSoundManager();
            x[0] = static_cast<uint32_t>(lab(CMemoryCopy::_mpCopy));
            x[1] = 1;   // the vptrs, the vtable slots and the bytes beyond the object are checked on the PowerPC side
            break;
        case kDtor:
            if (static_cast<int16_t>(a0) > 0) {
                delete m;
            } else {
                m->~TbSoundManager();
            }
            x[0] = static_cast<uint32_t>(lab(CMemoryCopy::_mpCopy));
            break;
        case kInit:
            result = static_cast<int32_t>(m->Init(names + 64 * 3));
            break;
        case kGetSoundPath:
            result = &m->GetSoundPath() == &m->mSoundPath;
            break;
        case kSetListenerPosition:
            m->SetListenerPosition(gPos[a0 & 3]);
            break;
        case kGetListenerPosition:
            result = lab(m->GetListenerPosition());
            break;
        case kSetAudioSystem:
            m->SetAudioSystem(a0 == 0 ? nullptr : gComponents[a0 - 1]);
            break;
        case kSetDistance:
            m->SetDistanceMapping(BitsToFloat(a0));
            break;
        case kGetDistance:
            result = Canon(FloatBits(m->GetDistanceMultiplier()));
            break;
        case kIsQSound:
            result = m->IsQSoundOn();
            break;
        case kGetRolloff:
            result = Canon(FloatBits(m->GetRolloffFactorToMinMaxDistance(BitsToFloat(a0), BitsToFloat(a1))));
            break;
        case kGetVolumeControl:
            result = m->GetVolumeControl() == &m->mVolumeControl;
            break;
        case kGetVolume:
            if (static_cast<uint8_t>(a0) >= 4) {
                // an index past the four volumes reads the member after them: a1 is put there first (as on the PowerPC side)
                std::memcpy(reinterpret_cast<char*>(m->mVolumes) + 4 * static_cast<uint8_t>(a0), &a1, 4);
                tail = false;
            }
            result = Canon(FloatBits(m->GetVolume(static_cast<uint8_t>(a0))));
            break;
        case kVolumeIsOn:
            result = m->VolumeControlIsOn(static_cast<uint8_t>(a0));
            break;
        case kUpdateVolume:
            result = static_cast<int32_t>(m->UpdateVolumeControl(static_cast<uint8_t>(a0), BitsToFloat(a1)));
            if (static_cast<uint8_t>(a0) >= 4) {
                tail = false;
                x[0] = Canon(FloatBits(m->mVolumes[static_cast<uint8_t>(a0)]));
            }
            break;
        case kToggleVolume:
            result = static_cast<int32_t>(m->ToggleVolumeControl(static_cast<uint8_t>(a0), a1));
            break;
        case kSetQSound:
            result = static_cast<int32_t>(m->SetQSoundInfo(a0, a1, a2));
            break;
        case kOrientation:
            result = static_cast<int32_t>(m->SetListenerOrientation(gPos[0], gPos[1], gPos[2], gPos[3]));
            break;
        case kGetMax:
            gOut = a0;
            result = static_cast<int32_t>(m->GetMaxNumberOfChannels(gOut));
            break;
        case kGetDirect:
            gOut = a0;
            result = static_cast<int32_t>(m->GetDirectSoundObject(gOut));
            break;
        case kGetBankData:
            result = lab(m->GetBankData(a0));
            break;
        case kIsBankDroppable:
            result = m->IsBankDroppable(a0);
            break;
        case kSetDroppable:
            m->SetDroppable(a0 != 0);
            break;
        case kGetBank:
            result = lab(m->GetBank(a0));
            break;
        case kConstruct:
            result = Low32(m->ConstructSample(Cookie(a0), gControl, gSamp));
            break;
        case kPlay:
            result = Low32(m->PlaySample(gSamp, gControl));
            break;
        default:
            break;
    }
    (void)a2;
    (void)a3;
    gTrack = false;

    out[O_RESULT] = static_cast<uint32_t>(result);
    if (op != kDtor) DumpState(m, out + O_STATE, tail);
    std::memcpy(out + O_X, x, sizeof x);
    out[O_INFO] = gSamp.mField0C;
    out[O_INFO + 1] = gSamp.mFlags;
    out[O_CONTROL] = Low32(gControl);
    for (uint32_t i = 0; i < banks && i < kMaxBanks; ++i) {
        out[O_BANKS + i] = static_cast<uint32_t>(lab(reinterpret_cast<CBankContainer*>(gContainerBytes[i])->mBank));
    }
    out[O_OUT] = gOut;
}

}  // extern "C"
