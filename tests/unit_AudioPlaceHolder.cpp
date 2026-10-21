// Fakes and test entry points for unit w79g2, CAudioPlaceHolder (libtpw_unit_AudioPlaceHolder.dylib, driven by
// tests/test_AudioPlaceHolder.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/AudioPlaceHolder.cpp, src/sound/AudioPlaceHolderList.cpp, src/sound/TbAudioBody.cpp, src/sound/TbSoundSampleCallback.cpp  CAudioPlaceHolder, CDistPriority, CAudioPlaceHolderExtraData, TbSndCallback (the unit under test)
// Real: the info records and positions (w77: TbSoundInfo.cpp), TbSoundSampleInfo::SetBank and CvtDblUl (w72: TbSoundBank.cpp),
// TbRefRelease (w78), CAudioComponent (w79b), the command constructors (w79b). Everything else the code calls is a fake that
// records what it is given (the PowerPC side records the same from its stubs):
//   the sound manager: w79e's TbSoundManager is NOT linked (it needs the streamer, the storage and the bank factory); this file
//       defines the members the placeholder calls (GetListenerPosition, GetDistanceMultiplier, IsQSoundOn, GetVolumeControl,
//       VolumeControlIsOn, GetBank, the virtuals GetRolloffFactorToMinMaxDistance and SystemCommand) over a fake TbSoundCore. A
//       command handed to SystemCommand is executed by the commands' Execute fakes below, which record its words.
//   the category container (a fake CCatagoryContainer whose AddRef and Release record), TbMapStreamer::Instance, the free function
//       TbMapStreamer::FixUpCatagoryIndex, CMapReference (w79g4), the two sentence Element constructors (w79g4), bfl::maths::Random
//       and RangedRandomSlow, LbTime_GetClock, LbFile_Read and LbFile_Write (a memory file).
// Virtuals of the placeholder (the test class TestHolder) can be replaced by recording fakes through a hook mask.
//
// Labels: 1 the placeholder, 2 the manager, 3 the volume control, 4 the category container, 5 the sample info, 6 the child
// placeholder, 7 the streamer, 8 the listener position, 9 and 10 the position and velocity arguments, 11..13 the banks, 14 the
// file, 16 the category data, 21.. the events, 31.. the sounds (flat), 71.. the samples (flat), 100 and up the objects made.
//
// tpw_ap_run drives one operation on a world it builds from the parameter words (the P_ enum, shared with the Python file).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "AudioPlaceHolder.h"
#include "PlaceHolderHelpers.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include "LbFile.h"
#include "LbTime.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wunused-parameter"

namespace {

bool gTrack;   // "new" events are recorded only while the code under test runs

// The runtime's new: blocks are filled with 0xA5; the extra data and the sentence elements are recorded with the Mac's size.
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    if (size == sizeof(CAudioPlaceHolderExtraData)) ev("new", {0x20, newlab(block)});
    else if (size == sizeof(CPlaceHolderOneShotSentenceElement)) ev("new", {0x60, newlab(block)});
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

// ---- the parameter and result words (shared with tests/test_AudioPlaceHolder.py) ---------------------------------------------

enum Param {
    P_OP = 0, P_MGR = 1, P_HOOK = 2, P_CLASS = 3, P_CHILDCLASS = 4, P_SEED = 5, P_FILEN = 6, P_STATE = 8, P_A = 48, P_LISTENER = 56,
    P_ARGPOS = 60, P_CHILD = 72, P_INFO = 88, P_SPEC = 128
};
// The preset (and dumped) state of the placeholder, 32 words from P_STATE / O_STATE.
enum State {
    S_EVENT = 0, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME, S_PRIORITY, S_HANDLE, S_CATAGORY,
    S_LISTHANDLE, S_FLAGS, S_NEXT, S_PREVIOUS, S_PAUSETIME, S_MAXDIST, S_LASTSOUND, S_LASTSAMPLE, S_VCINDEX, S_FADE, S_VOLUME,
    S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP, S_RIGHT, S_BOTTOM, S_CHILD, S_PARENT, S_COUNT
};
enum Out {
    O_R = 0, O_R2 = 1, O_R3 = 2, O_STATE = 8, O_CAT = 48, O_INFO = 52, O_FILE = 96, O_DATA = 224, O_STATIC = 256, O_EXTRA = 260
};
enum Hook {
    H_DTOR = 1, H_INIT = 2, H_SOUND = 4, H_SAMPLE = 8, H_PARAM = 0x10, H_BASIC = 0x20, H_PSEUDO = 0x40, H_CLASS = 0x80,
    H_DIST = 0x100, H_DEP = 0x200
};
enum Op {
    kSetSoundManager = 0, kGetBasePriority, kCtor, kPriorityCtor, kDtor, kChangeDistPriority, kSetDistance, kSetBasePriority,
    kSetPriorityAndVolume, kPriorityToWord, kPriorityAssign, kUpdateParameter, kAllocateExtraData, kExtraCtor,
    kUpdateDependants, kUpdate, kCompletedCallback, kRelease, kSetFlagsFromEvent, kInitControls, kInit, kGetParameterValue,
    kGetRandomVolume, kGetRandomPitch, kFillBasic, kGetPsuedoTime, kFillSampleInfo, kGetVolume, kGetMaxDistance,
    kChooseRandomSample, kGetNextSample, kVolumeControlIsOn, kChooseRandomSound, kGetNextSound, kSetVolume,
    kSetVolumeAndFrequency, kConvertFrequency, kSetVolumeAndPitch, kProcessPosition, kInitFade, kSetOutOfRange, kStop, kPause,
    kRestart, kFinishedPlaying, kSave, kWriteMagic, kGetClassID, kLoad, kReadMagic, kModifyPosition, kPlaySample, kGetHandle,
    kSetHandle, kSoundCallback, kCallbackCtor, kBaseCompleted
};

constexpr uint32_t kFill = 0xA5A5A5A5u;

// ---- the world -------------------------------------------------------------------------------------------------------------

// The placeholder under test (and the child's), with the virtuals a hook mask can replace by recording fakes.
class TestHolder : public CAudioPlaceHolder {
public:
    TestHolder(uint32_t hooks, uint32_t classId, bool child) : mHooks(hooks), mClassId(classId), mChild(child) {}
    ~TestHolder() override {
        if ((mHooks & H_DTOR) != 0) ev("V.dtor", {lab(this)});
    }
    TbError Init(CCatagoryContainer* container, void* event, const TbSoundPosition& position, float maxDistance) override {
        if ((mHooks & H_INIT) == 0) return CAudioPlaceHolder::Init(container, event, position, maxDistance);
        ev("V.Init", {lab(this), lab(container), lab(event), lab(&position), Canon(FloatBits(maxDistance))});
        return TbError(static_cast<int32_t>(ret("V.Init", 0)));
    }
    bool ChooseRandomSound(bool rotate) override {
        if ((mHooks & H_SOUND) == 0) return CAudioPlaceHolder::ChooseRandomSound(rotate);
        ev("V.ChooseRandomSound", {lab(this), rotate});
        return ret("V.ChooseRandomSound", 0) != 0;
    }
    CSampleData* ChooseRandomSample(bool rotate) override;
    bool GetParameterValue(uint32_t mask, uint32_t& value) override {
        if ((mHooks & H_PARAM) == 0) return CAudioPlaceHolder::GetParameterValue(mask, value);
        ev("V.GetParameterValue", {lab(this), mask});
        value = static_cast<uint32_t>(ret("V.GetParameterValue.out", 0));
        return ret("V.GetParameterValue", 0) != 0;
    }
    bool FillBasicSampleInfo(TbSoundSampleInfo& info) override {
        if ((mHooks & H_BASIC) == 0) return CAudioPlaceHolder::FillBasicSampleInfo(info);
        ev("V.FillBasicSampleInfo", {lab(this), lab(&info)});
        return ret("V.FillBasicSampleInfo", 0) != 0;
    }
    int32_t GetPsuedoTime() override {
        if ((mHooks & H_PSEUDO) == 0) return CAudioPlaceHolder::GetPsuedoTime();
        ev("V.GetPsuedoTime", {lab(this)});
        return static_cast<int32_t>(ret("V.GetPsuedoTime", 0));
    }
    uint32_t GetClassID() override {
        if ((mHooks & H_CLASS) == 0 && !mChild) return CAudioPlaceHolder::GetClassID();
        ev("V.GetClassID", {lab(this)});
        return mClassId;
    }
    float ChangeDistPriority() override {
        if ((mHooks & H_DIST) == 0) return CAudioPlaceHolder::ChangeDistPriority();
        ev("V.ChangeDistPriority", {lab(this)});
        return BitsToFloat(static_cast<uint32_t>(ret("V.ChangeDistPriority", 0)));
    }
    void UpdateParameterDepandants() override {
        if ((mHooks & H_DEP) == 0) return CAudioPlaceHolder::UpdateParameterDepandants();
        ev("V.UpdateParameterDepandants", {lab(this)});
    }

    uint32_t mHooks;
    uint32_t mClassId;
    bool mChild;
};

}  // namespace

// ---- the fakes: the sound manager ---------------------------------------------------------------------------------------------
// TbSoundCore's members, none of them real here (the manager is a recording fake).

CMemoryCopyInit::CMemoryCopyInit() {}
CMemoryCopyInit::~CMemoryCopyInit() {}
TbSoundCore::TbSoundCore() : CAudioComponent(), mModule(nullptr), mStore(nullptr) {}
TbSoundCore::~TbSoundCore() {}
TbError TbSoundCore::Init(const TbSoundSystemInfo&) { return TbError(-77); }
TbError TbSoundCore::GetSystemInfo(TbSoundSystemInfo&) { return TbError(-77); }
TbError TbSoundCore::GetSampleInfo(void*, TbSoundSampleInfo&) { return TbError(-77); }
void* TbSoundCore::NewSample(void*, TbSoundSampleInfo&) { return nullptr; }
void* TbSoundCore::NewControl(void*, TbSoundSampleInfo&) { return nullptr; }
TbSoundSystemModule* TbSoundCore::GetSystemModule() const { return nullptr; }
TbSoundSampleStore* TbSoundCore::GetSampleStore() const { return nullptr; }
// The one call the placeholder makes: the command is executed (the Execute fakes below record its words).
TbError TbSoundCore::SystemCommand(const ::SystemCommand::Command& command) {
    ev("Mgr.SystemCommand", {lab(this)});
    return command.Execute(this);
}

TbSoundManager::TbSoundManager() : TbSoundCore() {}
TbSoundManager::~TbSoundManager() {}
TbError TbSoundManager::Init(const char*) { return TbError(-77); }
float TbSoundManager::GetRolloffFactorToMinMaxDistance(float min, float max) {
    ev("Mgr.Rolloff", {lab(this), Canon(FloatBits(min)), Canon(FloatBits(max))});
    return BitsToFloat(static_cast<uint32_t>(ret("Rolloff", 0)));
}

namespace {

TbSoundPosition gListener;
TbSoundVolumeControl gVolumeControl;
alignas(16) uint8_t gBankBytes[3][16];
alignas(16) uint8_t gStreamerBytes[sizeof(TbMapStreamer) + 16];

}  // namespace

const TbSoundPosition* TbSoundManager::GetListenerPosition() {
    ev("Mgr.GetListenerPosition", {lab(this)});
    return ret("ListenerOn", 0) != 0 ? &gListener : nullptr;
}
float TbSoundManager::GetDistanceMultiplier() {
    ev("Mgr.GetDistanceMultiplier", {lab(this)});
    return BitsToFloat(static_cast<uint32_t>(ret("DistMul", 0x3F800000)));
}
uint32_t TbSoundManager::IsQSoundOn() {
    ev("Mgr.IsQSoundOn", {lab(this)});
    return static_cast<uint32_t>(ret("QSound", 0));
}
TbSoundVolumeControl* TbSoundManager::GetVolumeControl() {
    ev("Mgr.GetVolumeControl", {lab(this)});
    return &gVolumeControl;
}
bool TbSoundManager::VolumeControlIsOn(uint8_t mask) {
    ev("Mgr.VolumeControlIsOn", {lab(this), mask});
    return ret("VCOn", 1) != 0;
}
TbBank* TbSoundManager::GetBank(uint32_t index) {
    ev("Mgr.GetBank", {lab(this), index});
    const int64_t answer = ret("Bank", 0);
    return answer >= 1 && answer <= 3 ? reinterpret_cast<TbBank*>(gBankBytes[answer - 1]) : nullptr;
}

// ---- the command bodies (w79d): each records the words of its command; the answer is the queue's -------------------------------

namespace {

int64_t EventWordLabel(uint32_t word);

// The sample info as the command receives it: the words the constructor and the setters define (flag-gated), the positions it
// points to by value.
void RecInfo(const char* name, int64_t first, const TbSoundSampleInfo& i) {
    const uint32_t flags = i.mFlags;
    ev(name, {first, i.mNumber, lab(i.mBank), i.mField0C, i.mField10, flags, i.mPriority, (flags & 2) != 0 ? i.mVolume : 0,
              (flags & 0x20) != 0 ? static_cast<uint32_t>(i.mPitch) : 0, (flags & 4) != 0 ? EventWordLabel(i.mUserId) : 0,
              (flags & 0x40) != 0 ? i.mLoopCount : 0, (flags & 0x2000) != 0 ? lab(i.mCompletedCallback) : 0});
    ev(std::string(name).append(".more").c_str(),
       {(flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mA)) : 0, (flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mB)) : 0,
        (flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mC)) : 0, (flags & 0x1000) != 0 ? Low32(i.mUserData) : 0,
        (flags & 0x40000) != 0 ? lab(i.mVolumeControl) : 0, (flags & 0x40000) != 0 ? i.mVolumeControlIndex : 0});
    ev(std::string(name).append(".pos").c_str(),
       {lab(i.mPosition), i.mPosition != nullptr ? Canon(FloatBits(i.mPosition->mX)) : 0,
        i.mPosition != nullptr ? Canon(FloatBits(i.mPosition->mY)) : 0, i.mPosition != nullptr ? Canon(FloatBits(i.mPosition->mZ)) : 0,
        lab(i.mVelocity), i.mVelocity != nullptr ? Canon(FloatBits(i.mVelocity->mX)) : 0,
        i.mVelocity != nullptr ? Canon(FloatBits(i.mVelocity->mY)) : 0, i.mVelocity != nullptr ? Canon(FloatBits(i.mVelocity->mZ)) : 0});
}

}  // namespace

namespace SystemCommand {

TbError SetSampleInfo::Execute(CAudioComponentAccess*) const {
    RecInfo("Cmd.SetSampleInfo", Low32(mHandle), *mInfo);
    return TbError(static_cast<int32_t>(ret("SysErr", 0)));
}
TbError Stop::Execute(CAudioComponentAccess*) const {
    ev("Cmd.Stop", {Low32(mHandle)});
    return TbError(static_cast<int32_t>(ret("SysErr", 0)));
}
TbError Pause::Execute(CAudioComponentAccess*) const {
    ev("Cmd.Pause", {Low32(mHandle)});
    return TbError(static_cast<int32_t>(ret("SysErr", 0)));
}
TbError Restart::Execute(CAudioComponentAccess*) const {
    ev("Cmd.Restart", {Low32(mHandle)});
    return TbError(static_cast<int32_t>(ret("SysErr", 0)));
}
TbError SetSystemInfo::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopLoop::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopAllVolumeControl::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Start::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError UpdateAllVolumes::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError Process::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError StopAll::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError PauseAll::Execute(CAudioComponentAccess*) const { return TbError(0); }
TbError RestartAll::Execute(CAudioComponentAccess*) const { return TbError(0); }

}  // namespace SystemCommand

// ---- the category container, the streamer and the map reference -------------------------------------------------------------

CCatagoryContainer::CCatagoryContainer(uint32_t index) : mData(nullptr), mIndex(index), mElements(nullptr) {}
CCatagoryContainer::~CCatagoryContainer() {}

namespace {

// A container whose AddRef and Release record (the count stays in the base; Release never deletes).
class FakeContainer : public CCatagoryContainer {
public:
    FakeContainer() : CCatagoryContainer(0) {}
    int32_t AddRef() override {
        ++mRefCount;
        ev("Cat.AddRef", {lab(this), mRefCount});
        return mRefCount;
    }
    int32_t Release() override {
        --mRefCount;
        ev("Cat.Release", {lab(this), mRefCount});
        return mRefCount;
    }
};

}  // namespace

TbMapStreamer* TbMapStreamer::Instance() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

uint32_t TbMapStreamer::FixUpCatagoryIndex(uint32_t index) {
    ev("FixUp", {lab(this), index});
    return static_cast<uint32_t>(ret("FixUp", 0));
}

namespace {
FakeContainer* gContainer;
}

CMapReference::CMapReference() {
    ev("MapRef.ctor", {});
}
void CMapReference::GetSoundIds(int32_t& category, int32_t& event, int32_t& sound, int32_t& sample, CAudioPlaceHolder* holder) {
    ev("MapRef.GetSoundIds", {lab(holder)});
    category = static_cast<int32_t>(ret("Ids", 0));
    event = static_cast<int32_t>(ret("Ids", 0));
    sound = static_cast<int32_t>(ret("Ids", 0));
    sample = static_cast<int32_t>(ret("Ids", 0));
}
CCatagoryContainer* CMapReference::GetSoundPointer(uint32_t category, CAudioPlaceHolder* holder) {
    ev("MapRef.GetSoundPointer", {category, lab(holder)});
    return ret("SoundPointer", 0) != 0 ? gContainer : nullptr;
}

// The elements are plain placeholders here (their own behaviour is unit w79g4's): the class chain of include/PlaceHolderHelpers.h
// (Element, Throwaway, w79g3's CPlaceHolderOneShot) with every override answering the base's, so that the vtables of the classes
// (which the constructors below make) are defined and a virtual call on a child reaches the base function, as the PowerPC side's
// nested base constructor leaves it.
// CPlaceHolderOneShot (unit w79g3's, not linked here): the base's constructor and an answer of the base for each override, so that the
// children behave as plain placeholders in this unit's cases.
CPlaceHolderOneShot::CPlaceHolderOneShot() : CAudioPlaceHolder() {}
CPlaceHolderOneShot::~CPlaceHolderOneShot() {}
TbError CPlaceHolderOneShot::PlaySample(uint32_t now) { return CAudioPlaceHolder::PlaySample(now); }
uint32_t CPlaceHolderOneShot::GetClassID() { return CAudioPlaceHolder::GetClassID(); }
CPlaceHolderOneShotThrowaway::CPlaceHolderOneShotThrowaway() : CPlaceHolderOneShot() {}
CPlaceHolderOneShotThrowaway::~CPlaceHolderOneShotThrowaway() {}
bool CPlaceHolderOneShotThrowaway::FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) {
    return CAudioPlaceHolder::FillSampleInfo(now, info);
}
uint32_t CPlaceHolderOneShotThrowaway::GetClassID() {
    return CAudioPlaceHolder::GetClassID();
}
namespace {
bool gMakingBranching;   // set while a Branching element's constructor runs the Element's, which then records nothing
CAudioPlaceHolder* MarkBranching(CAudioPlaceHolder* parent) {
    gMakingBranching = true;
    return parent;
}
}  // namespace
CPlaceHolderOneShotSentenceElement::CPlaceHolderOneShotSentenceElement(CAudioPlaceHolder* parent) : CPlaceHolderOneShotThrowaway() {
    if (!gMakingBranching) ev("Element.ctor", {lab(this), lab(parent)});
    gMakingBranching = false;
}
CPlaceHolderOneShotSentenceElement::~CPlaceHolderOneShotSentenceElement() {}
bool CPlaceHolderOneShotSentenceElement::FillSampleInfo(uint32_t now, TbSoundSampleInfo& info) {
    return CAudioPlaceHolder::FillSampleInfo(now, info);
}
bool CPlaceHolderOneShotSentenceElement::GetParameterValue(uint32_t mask, uint32_t& value) {
    return CAudioPlaceHolder::GetParameterValue(mask, value);
}
uint32_t CPlaceHolderOneShotSentenceElement::GetClassID() {
    return CAudioPlaceHolder::GetClassID();
}
CPlaceHolderOneShotBranchingSentenceElement::CPlaceHolderOneShotBranchingSentenceElement(CAudioPlaceHolder* parent)
    : CPlaceHolderOneShotSentenceElement(MarkBranching(parent)) {
    ev("BranchingElement.ctor", {lab(this), lab(parent)});
}
CPlaceHolderOneShotBranchingSentenceElement::~CPlaceHolderOneShotBranchingSentenceElement() {}
bool CPlaceHolderOneShotBranchingSentenceElement::ChooseRandomSound(bool rotate) {
    return CAudioPlaceHolder::ChooseRandomSound(rotate);
}
uint32_t CPlaceHolderOneShotBranchingSentenceElement::GetClassID() {
    return CAudioPlaceHolder::GetClassID();
}

// ---- TbSoundBank.cpp's two outside callees (never reached here) ---------------------------------------------------------------

namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::CFile(const CFile&) : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace bfl::FileSystem

void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}
SamsUtilities::UString::UString(const char*) {}
SamsUtilities::UString::~UString() {}
// The string library's printf-style formatters are not used here (the bullfrog library supplies them on the Mac).
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- the random numbers, the clock and the file -----------------------------------------------------------------------------

namespace bfl::maths {
uint32_t Random(int32_t& seed) {
    ev("Random", {static_cast<uint32_t>(seed)});
    return static_cast<uint32_t>(ret("Random", 0));
}
uint32_t RangedRandomSlow(int32_t& seed, uint32_t range) {
    ev("RangedRandom", {static_cast<uint32_t>(seed), range});
    return static_cast<uint32_t>(ret("RangedRandom", 0));
}
}  // namespace bfl::maths

uint32_t LbTime_GetClock() {
    ev("Clock", {});
    return static_cast<uint32_t>(ret("Clock", 0));
}

namespace {
std::vector<uint8_t> gFileOut;   // reserved before the code runs, so that no allocation happens while it is observed
const uint8_t* gFileIn;
uint32_t gFileInSize;
uint32_t gFileInPos;
}  // namespace

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint64_t value = 0;
    std::memcpy(&value, buffer, length < 8 ? length : 8);
    ev("File.Write", {lab(file), length, static_cast<int64_t>(value)});
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gFileOut.insert(gFileOut.end(), bytes, bytes + length);
    if (transferred != nullptr) *transferred = length;
    return TbError(static_cast<int32_t>(ret("FileErr", 0)));
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    uint8_t* bytes = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < length; ++i) {
        bytes[i] = gFileInPos < gFileInSize ? gFileIn[gFileInPos] : 0;
        ++gFileInPos;
    }
    uint64_t value = 0;
    std::memcpy(&value, buffer, length < 8 ? length : 8);
    ev("File.Read", {lab(file), length, static_cast<int64_t>(value)});
    if (transferred != nullptr) *transferred = length;
    return TbError(static_cast<int32_t>(ret("FileErr", 0)));
}

// ---- the test class's sample virtual (needs the world's sample labels) ---------------------------------------------------------

namespace {
std::vector<CSampleData*> gSamples;   // the flat samples, label 71 + index
}

CSampleData* TestHolder::ChooseRandomSample(bool rotate) {
    if ((mHooks & H_SAMPLE) == 0) return CAudioPlaceHolder::ChooseRandomSample(rotate);
    ev("V.ChooseRandomSample", {lab(this), rotate});
    const int64_t answer = ret("V.ChooseRandomSample", 0);
    return answer > 0 && answer <= static_cast<int64_t>(gSamples.size()) ? gSamples[answer - 1] : nullptr;
}

// ---- the world's data: events, sounds and samples in the arena --------------------------------------------------------------

namespace {

std::vector<CEventData*> gEventArrays;
std::vector<CSoundData*> gSoundArrays;
std::vector<CSampleData*> gSampleArrays;
std::vector<CSoundData*> gSounds;   // the flat sounds, label 31 + index
std::vector<CEventData*> gEvents;   // label 21 + index
CCatagoryData* gCatagoryData;

void FreeWorld() {
    for (auto* p : gEventArrays) tpw::ArenaFree(p);
    for (auto* p : gSoundArrays) tpw::ArenaFree(p);
    for (auto* p : gSampleArrays) tpw::ArenaFree(p);
    if (gCatagoryData != nullptr) tpw::ArenaFree(gCatagoryData);
    gEventArrays.clear();
    gSoundArrays.clear();
    gSampleArrays.clear();
    gSounds.clear();
    gSamples.clear();
    gEvents.clear();
    gCatagoryData = nullptr;
}

// spec: [nEvents] then per event [id, nSounds, f0c, flags | f12 << 16], then per sound [nSamples, bytes, f10 | f12 << 16,
// f16 | f18 << 16, f1a | f1c << 16, f1e, f22 bits], then per sample [f00, f04, f08, bank].
void BuildWorld(const uint32_t* spec, uint32_t vcIndex) {
    FreeWorld();
    const uint32_t nEvents = spec[0];
    const uint32_t* w = spec + 1;
    auto* events = static_cast<CEventData*>(tpw::ArenaAllocZeroed(sizeof(CEventData) * (nEvents != 0 ? nEvents : 1)));
    gEventArrays.push_back(events);
    int sampleLabel = 0, soundLabel = 0;
    for (uint32_t e = 0; e < nEvents; ++e) {
        CEventData& ev = events[e];
        const uint32_t nSounds = w[1];
        ev.mID = w[0];
        ev.mSoundCount = nSounds;
        ev.mF0C = w[2];
        ev.mFlags = static_cast<uint16_t>(w[3]);
        ev.mF12 = static_cast<uint16_t>(w[3] >> 16);
        w += 4;
        auto* sounds = static_cast<CSoundData*>(tpw::ArenaAllocZeroed(sizeof(CSoundData) * (nSounds != 0 ? nSounds : 1)));
        gSoundArrays.push_back(sounds);
        ev.mSounds.set(sounds);
        tpw_cl_label(21 + static_cast<int>(e), &ev);
        gEvents.push_back(&ev);
        for (uint32_t s = 0; s < nSounds; ++s) {
            CSoundData& sd = sounds[s];
            const uint32_t nSamples = w[0] & 0xffff;
            sd.mSampleCount = w[0];
            sd.mF0C[0] = static_cast<uint8_t>(w[1]);
            sd.mF0C[1] = static_cast<uint8_t>(w[1] >> 8);
            sd.mF0C[2] = static_cast<uint8_t>(w[1] >> 16);
            sd.mF0C[3] = static_cast<uint8_t>(w[1] >> 24);
            sd.mF10 = static_cast<uint16_t>(w[2]);
            sd.mF12 = static_cast<uint16_t>(w[2] >> 16);
            sd.mF16 = static_cast<uint16_t>(w[3]);
            sd.mF18 = static_cast<uint16_t>(w[3] >> 16);
            sd.mF1A = static_cast<uint16_t>(w[4]);
            sd.mF1C = static_cast<uint16_t>(w[4] >> 16);
            sd.mF1E = w[5];
            sd.mF22 = BitsToFloat(w[6]);
            w += 7;
            auto* samples = static_cast<CSampleData*>(tpw::ArenaAllocZeroed(sizeof(CSampleData) * (nSamples != 0 ? nSamples : 1)));
            gSampleArrays.push_back(samples);
            sd.mSamples.set(samples);
            tpw_cl_label(31 + soundLabel, &sd);
            gSounds.push_back(&sd);
            ++soundLabel;
            for (uint32_t m = 0; m < nSamples; ++m) {
                samples[m].mF00 = w[0];
                samples[m].mF04 = w[1];
                samples[m].mF08 = w[2];
                samples[m].mBankIndex = static_cast<uint16_t>(w[3]);
                w += 4;
                tpw_cl_label(71 + sampleLabel, &samples[m]);
                gSamples.push_back(&samples[m]);
                ++sampleLabel;
            }
        }
    }
    gCatagoryData = static_cast<CCatagoryData*>(tpw::ArenaAllocZeroed(sizeof(CCatagoryData)));
    gCatagoryData->mEvents.set(events);
    gCatagoryData->mEventCount = nEvents;
    gCatagoryData->mF08 = static_cast<uint16_t>(vcIndex);
    tpw_cl_label(16, gCatagoryData);
}

// A pointer word of the preset: 0 NULL, else the label's object.
CEventData* EventOf(uint32_t label) { return label >= 21 && label - 21 < gEvents.size() ? gEvents[label - 21] : nullptr; }
CSoundData* SoundOf(uint32_t label) { return label >= 31 && label - 31 < gSounds.size() ? gSounds[label - 31] : nullptr; }
CSampleData* SampleOf(uint32_t label) { return label >= 71 && label - 71 < gSamples.size() ? gSamples[label - 71] : nullptr; }

void Preset(CAudioPlaceHolder* h, const uint32_t* s) {
    h->mEvent = EventOf(s[S_EVENT]);
    h->mSound = SoundOf(s[S_SOUND]);
    h->mSample = SampleOf(s[S_SAMPLE]);
    h->mPosition.mX = BitsToFloat(s[S_POSX]);
    h->mPosition.mY = BitsToFloat(s[S_POSY]);
    h->mPosition.mZ = BitsToFloat(s[S_POSZ]);
    h->mVelocity.mX = BitsToFloat(s[S_VELX]);
    h->mVelocity.mY = BitsToFloat(s[S_VELY]);
    h->mVelocity.mZ = BitsToFloat(s[S_VELZ]);
    h->mEndTime = s[S_ENDTIME];
    h->mPriority = s[S_PRIORITY];
    h->mHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(s[S_HANDLE]));
    h->mCatagory = s[S_CATAGORY] != 0 ? gContainer : nullptr;
    h->mListHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(s[S_LISTHANDLE]));
    h->mFlags = s[S_FLAGS];
    h->mPauseTime = s[S_PAUSETIME];
    h->mMaxDistance = BitsToFloat(s[S_MAXDIST]);
    h->mLastSound = static_cast<int8_t>(s[S_LASTSOUND]);
    h->mLastSample = static_cast<int8_t>(s[S_LASTSAMPLE]);
    h->mVolumeControlIndex = static_cast<uint8_t>(s[S_VCINDEX]);
    h->mFade = BitsToFloat(s[S_FADE]);
    h->mVolume = static_cast<uint16_t>(s[S_VOLUME]);
    h->mPitch = static_cast<int16_t>(s[S_PITCH]);
    if (s[S_EXTRA] != 0) {
        h->mExtra = new CAudioPlaceHolderExtraData();
        tpw_cl_label(17, h->mExtra);   // so that the destructor's delete is recorded
        std::memcpy(h->mExtra->mParamId, &s[S_IDS], 4);
        std::memcpy(h->mExtra->mParamValue, &s[S_VALUES], 4);
        h->mExtra->mBounds.left = static_cast<int32_t>(s[S_LEFT]);
        h->mExtra->mBounds.top = static_cast<int32_t>(s[S_TOP]);
        h->mExtra->mBounds.right = static_cast<int32_t>(s[S_RIGHT]);
        h->mExtra->mBounds.bottom = static_cast<int32_t>(s[S_BOTTOM]);
        h->mExtra->mParent = reinterpret_cast<CAudioPlaceHolder*>(static_cast<uintptr_t>(s[S_PARENT]));
    }
}

uint32_t LabelOf(const void* p) { return static_cast<uint32_t>(lab(p)); }

// A 32-bit word that is an event's address (the sample info's user id): the event's label, else -1.
int64_t EventWordLabel(uint32_t word) {
    if (word == 0) return 0;
    for (std::size_t i = 0; i < gEvents.size(); ++i) {
        if (Low32(gEvents[i]) == word) return lab(gEvents[i]);
    }
    return -1;
}

void Dump(CAudioPlaceHolder* h, uint32_t* o) {
    o[S_EVENT] = LabelOf(h->mEvent);
    o[S_SOUND] = LabelOf(h->mSound);
    o[S_SAMPLE] = LabelOf(h->mSample);
    o[S_POSX] = Canon(FloatBits(h->mPosition.mX));
    o[S_POSY] = Canon(FloatBits(h->mPosition.mY));
    o[S_POSZ] = Canon(FloatBits(h->mPosition.mZ));
    o[S_VELX] = Canon(FloatBits(h->mVelocity.mX));
    o[S_VELY] = Canon(FloatBits(h->mVelocity.mY));
    o[S_VELZ] = Canon(FloatBits(h->mVelocity.mZ));
    o[S_ENDTIME] = h->mEndTime;
    o[S_PRIORITY] = h->mPriority;
    o[S_HANDLE] = Low32(h->mHandle);
    o[S_CATAGORY] = LabelOf(h->mCatagory);
    o[S_LISTHANDLE] = Low32(h->mListHandle);
    o[S_FLAGS] = h->mFlags;
    o[S_NEXT] = Low32(h->mNext);
    o[S_PREVIOUS] = Low32(h->mPrevious);
    o[S_PAUSETIME] = h->mPauseTime;
    o[S_MAXDIST] = Canon(FloatBits(h->mMaxDistance));
    o[S_LASTSOUND] = static_cast<uint8_t>(h->mLastSound);
    o[S_LASTSAMPLE] = static_cast<uint8_t>(h->mLastSample);
    o[S_VCINDEX] = h->mVolumeControlIndex;
    o[S_FADE] = Canon(FloatBits(h->mFade));
    o[S_VOLUME] = h->mVolume;
    o[S_PITCH] = static_cast<uint16_t>(h->mPitch);
    o[S_EXTRA] = h->mExtra != nullptr ? 1 : 0;
    if (h->mExtra != nullptr) {
        std::memcpy(&o[S_IDS], h->mExtra->mParamId, 4);
        std::memcpy(&o[S_VALUES], h->mExtra->mParamValue, 4);
        o[S_LEFT] = static_cast<uint32_t>(h->mExtra->mBounds.left);
        o[S_TOP] = static_cast<uint32_t>(h->mExtra->mBounds.top);
        o[S_RIGHT] = static_cast<uint32_t>(h->mExtra->mBounds.right);
        o[S_BOTTOM] = static_cast<uint32_t>(h->mExtra->mBounds.bottom);
        o[S_CHILD] = h->mExtra->mChild != nullptr && lab(h->mExtra->mChild) <= 0 ? Low32(h->mExtra->mChild) : LabelOf(h->mExtra->mChild);
        o[S_PARENT] = Low32(h->mExtra->mParent);
    }
}

// The sample info as the Mac lays it out in words: the fields the constructor and the setters define and the untouched rest
// (0xA5 fill on both sides).
void DumpInfo(const TbSoundSampleInfo& i, uint32_t* o) {
    int n = 0;
    o[n++] = i.mNumber;
    o[n++] = LabelOf(i.mBank);
    o[n++] = (i.mFlags & 4) != 0 ? static_cast<uint32_t>(EventWordLabel(i.mUserId)) : i.mUserId;
    o[n++] = i.mField0C;
    o[n++] = i.mField10;
    o[n++] = i.mFlags;
    o[n++] = i.mField18;
    o[n++] = i.mVolume;
    o[n++] = Canon(FloatBits(i.mFrequency));
    o[n++] = static_cast<uint32_t>(i.mPitch);
    o[n++] = i.mPriority;
    o[n++] = LabelOf(i.mPosition);
    o[n++] = LabelOf(i.mVelocity);
    o[n++] = Canon(FloatBits(i.mDistance.mA));
    o[n++] = Canon(FloatBits(i.mDistance.mB));
    o[n++] = Canon(FloatBits(i.mDistance.mC));
    o[n++] = i.mLoopCount;
    o[n++] = LabelOf(i.mCallback);
    o[n++] = LabelOf(i.mCompletedCallback);
    o[n++] = Low32(i.mUserData);
    o[n++] = LabelOf(i.mVolumeControl);
    o[n++] = i.mVolumeControlIndex;
    o[n++] = i.mStreamingBufferSize;
    o[n++] = i.mPan;
}

}  // namespace

// ---- the entry point ---------------------------------------------------------------------------------------------------------

namespace {

TbSoundManager* gManager;

void* Cookie(uint32_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(value));
}

void* NewFilled(std::size_t size) {
    return ::operator new(size);   // the observer fills the block with 0xA5
}

}  // namespace

extern "C" {

// One operation. p: the parameter words (P_ enum), fileIn: the bytes LbFile_Read serves, out: the results (O_ enum).
void tpw_ap_run(int32_t op, const uint32_t* p, const uint8_t* fileIn, uint32_t* out) {
    std::memset(out, 0, 512 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;
    gFileOut.clear();
    gFileOut.reserve(4096);
    gFileIn = fileIn;
    gFileInSize = p[P_FILEN];
    gFileInPos = 0;

    // ---- the world, made with the log off
    const uint32_t* state = p + P_STATE;
    const uint32_t* a = p + P_A;
    BuildWorld(p + P_SPEC, 0);
    if (gContainer == nullptr) gContainer = new FakeContainer();
    if (gManager == nullptr) gManager = new TbSoundManager();
    gContainer->mRefCount = 1;
    gContainer->mData = a[6] != 0 ? nullptr : gCatagoryData;
    gCatagoryData->mF08 = static_cast<uint16_t>(a[7]);   // the volume control index Init reads
    tpw_cl_label(4, gContainer);
    tpw_cl_label(2, gManager);
    gListener.mX = BitsToFloat(p[P_LISTENER]);
    gListener.mY = BitsToFloat(p[P_LISTENER + 1]);
    gListener.mZ = BitsToFloat(p[P_LISTENER + 2]);
    tpw_cl_label(8, &gListener);
    static TbSoundPosition argPos[2];
    for (int i = 0; i < 2; ++i) {
        argPos[i].mX = BitsToFloat(p[P_ARGPOS + 3 * i]);
        argPos[i].mY = BitsToFloat(p[P_ARGPOS + 3 * i + 1]);
        argPos[i].mZ = BitsToFloat(p[P_ARGPOS + 3 * i + 2]);
        tpw_cl_label(9 + i, &argPos[i]);
    }
    tpw_cl_label(3, &gVolumeControl);
    for (int i = 0; i < 3; ++i) {
        std::memset(gBankBytes[i], 0xA5, 16);
        tpw_cl_label(11 + i, gBankBytes[i]);
    }
    tpw_cl_label(7, gStreamerBytes);
    void* const file = Cookie(0x7C0FF000u);
    tpw_cl_label(14, file);
    CAudioPlaceHolder::mpSoundManager = p[P_MGR] != 0 ? gManager : nullptr;
    CAudioPlaceHolder::mRandomSeed = static_cast<int32_t>(p[P_SEED]);

    // the placeholder: the constructor (a TestHolder), then the preset state; the constructor under test gets bare 0xA5 memory
    void* memory = NewFilled(sizeof(TestHolder));
    TestHolder* h = nullptr;
    CAudioPlaceHolder* plain = nullptr;
    if (op == kCtor) {
        plain = reinterpret_cast<CAudioPlaceHolder*>(memory);
    } else {
        h = new (memory) TestHolder(p[P_HOOK], p[P_CLASS], false);
        Preset(h, state);
        if (state[S_CHILD] != 0 && h->mExtra != nullptr) {
            void* childMemory = NewFilled(sizeof(TestHolder));
            TestHolder* child = new (childMemory) TestHolder(0, p[P_CHILDCLASS], true);
            const uint32_t* c = p + P_CHILD;
            child->mFlags = c[0];
            child->mPosition.mX = BitsToFloat(c[1]);
            child->mPosition.mY = BitsToFloat(c[2]);
            child->mPosition.mZ = BitsToFloat(c[3]);
            child->mMaxDistance = BitsToFloat(c[4]);
            child->mListHandle = Cookie(c[5]);
            tpw_cl_label(6, child);
            h->mExtra->mChild = child;
        }
        tpw_cl_label(1, h);
    }
    // the sample info
    alignas(16) static uint8_t infoBytes[sizeof(TbSoundSampleInfo) + 16];
    std::memset(infoBytes, 0xA5, sizeof infoBytes);
    TbSoundSampleInfo* info = new (infoBytes) TbSoundSampleInfo();
    if (p[P_INFO + 2] != 0) {
        info->mField0C = p[P_INFO];
        info->mFlags = p[P_INFO + 1];
    }
    tpw_cl_label(5, info);
    // a block for the constructors of the small classes
    alignas(16) static uint8_t block[256];
    std::memset(block, 0xA5, sizeof block);

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    int64_t result = 0;
    uint32_t second = 0;
    bool dumpHolder = true;
    switch (op) {
        case kSetSoundManager:
            CAudioPlaceHolder::SetSoundManager(a[0] == 0 ? nullptr : a[0] == 1 ? gManager : reinterpret_cast<TbSoundManager*>(Cookie(a[0])));
            break;
        case kGetBasePriority:
            result = h->mPriority.GetBasePriority();
            break;
        case kCtor:
            new (plain) CAudioPlaceHolder();
            result = 1;   // the vptr is checked on the PowerPC side
            break;
        case kPriorityCtor:
            new (&h->mPriority) CAudioPlaceHolder::CDistPriority();
            break;
        case kDtor:
            if (static_cast<int16_t>(a[0]) > 0) {
                delete h;
            } else {
                static_cast<CAudioPlaceHolder*>(h)->~CAudioPlaceHolder();
            }
            break;
        case kChangeDistPriority:
            result = Canon(FloatBits(h->CAudioPlaceHolder::ChangeDistPriority()));
            break;
        case kSetDistance:
            h->mPriority.SetDistance(static_cast<uint16_t>(a[0]));
            break;
        case kSetBasePriority:
            h->mPriority.SetBasePriority(static_cast<uint16_t>(a[0]));
            break;
        case kSetPriorityAndVolume:
            h->SetPriorityAndVolume(a[0]);
            break;
        case kPriorityToWord:
            result = static_cast<uint32_t>(h->mPriority);
            break;
        case kPriorityAssign:
            h->mPriority = a[0];
            break;
        case kUpdateParameter:
            h->CAudioPlaceHolder::UpdateParameter(static_cast<uint8_t>(a[0]), a[1]);
            break;
        case kAllocateExtraData:
            h->AllocateExtraData();
            break;
        case kExtraCtor: {
            auto* extra = new (block) CAudioPlaceHolderExtraData();
            std::memcpy(&out[O_EXTRA], extra->mParamId, 4);
            std::memcpy(&out[O_EXTRA + 1], extra->mParamValue, 4);
            out[O_EXTRA + 2] = static_cast<uint32_t>(extra->mBounds.left);
            out[O_EXTRA + 3] = static_cast<uint32_t>(extra->mBounds.top);
            out[O_EXTRA + 4] = static_cast<uint32_t>(extra->mBounds.right);
            out[O_EXTRA + 5] = static_cast<uint32_t>(extra->mBounds.bottom);
            out[O_EXTRA + 6] = Low32(extra->mChild);
            out[O_EXTRA + 7] = Low32(extra->mParent);
            result = 1;
            dumpHolder = false;
            break;
        }
        case kUpdateDependants:
            h->CAudioPlaceHolder::UpdateParameterDepandants();
            break;
        case kUpdate:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::Update(argPos[0], argPos[1]));
            break;
        case kCompletedCallback:
            h->CAudioPlaceHolder::CompletedCallback(Cookie(a[0]), Cookie(a[1]), static_cast<int32_t>(a[2]));
            break;
        case kRelease:
            h->CAudioPlaceHolder::Release();
            break;
        case kSetFlagsFromEvent:
            h->SetFlagsFromEvent(BitsToFloat(a[0]));
            break;
        case kInitControls:
            h->InitParamatisedControls();
            break;
        case kInit:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::Init(a[0] == 4 ? gContainer : static_cast<CCatagoryContainer*>(Cookie(a[0])),
                                                                     EventOf(a[1]), argPos[0], BitsToFloat(a[2])));
            break;
        case kGetParameterValue: {
            uint32_t value = kFill;
            result = h->CAudioPlaceHolder::GetParameterValue(a[0], value);
            second = value;
            break;
        }
        case kGetRandomVolume:
            result = h->GetRandomVolume();
            break;
        case kGetRandomPitch:
            result = h->GetRandomPitch();
            break;
        case kFillBasic:
            result = h->CAudioPlaceHolder::FillBasicSampleInfo(*info);
            break;
        case kGetPsuedoTime:
            result = h->CAudioPlaceHolder::GetPsuedoTime();
            break;
        case kFillSampleInfo:
            result = h->CAudioPlaceHolder::FillSampleInfo(a[0], *info);
            break;
        case kGetVolume:
            result = h->GetVolume();
            break;
        case kGetMaxDistance:
            result = Canon(FloatBits(h->GetMaxDistance()));
            break;
        case kChooseRandomSample:
            result = LabelOf(h->CAudioPlaceHolder::ChooseRandomSample(a[0] != 0));
            break;
        case kGetNextSample: {
            CSampleData* sample = SampleOf(a[2]);
            h->GetNextSample(a[0] != 0, static_cast<int8_t>(a[1]), sample);
            result = LabelOf(sample);
            break;
        }
        case kVolumeControlIsOn:
            result = h->VolumeControlIsOn();
            break;
        case kChooseRandomSound:
            result = h->CAudioPlaceHolder::ChooseRandomSound(a[0] != 0);
            break;
        case kGetNextSound: {
            CSoundData* sound = SoundOf(a[2]);
            h->GetNextSound(a[0] != 0, static_cast<int8_t>(a[1]), sound);
            result = LabelOf(sound);
            break;
        }
        case kSetVolume:
            h->SetVolume(a[0]);
            break;
        case kSetVolumeAndFrequency:
            h->SetVolumeAndFrequency(a[0], BitsToFloat(a[1]));
            break;
        case kConvertFrequency:
            result = h->ConvertFrequencyToPitchIndex(BitsToFloat(a[0]));
            break;
        case kSetVolumeAndPitch:
            h->SetVolumeAndPitch(a[0], static_cast<int32_t>(a[1]));
            break;
        case kProcessPosition:
            result = h->ProcessPositionAndPriority();
            break;
        case kInitFade:
            h->InitFade();
            break;
        case kSetOutOfRange:
            h->SetOutOfRange();
            break;
        case kStop:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::Stop());
            break;
        case kPause:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::Pause());
            break;
        case kRestart:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::Restart());
            break;
        case kFinishedPlaying:
            result = h->FinishedPlaying(a[0]);
            break;
        case kSave:
            result = static_cast<int32_t>(h->Save(file, a[0]));
            break;
        case kWriteMagic:
            h->WriteMagicHeader(file);
            break;
        case kGetClassID:
            result = h->CAudioPlaceHolder::GetClassID();
            break;
        case kLoad:
            result = static_cast<int32_t>(h->Load(file, a[0]));
            break;
        case kReadMagic:
            result = h->ReadMagicHeader(file);
            break;
        case kModifyPosition:
            h->ModifyPositionForBoundingRectangle();
            break;
        case kPlaySample:
            result = static_cast<int32_t>(h->CAudioPlaceHolder::PlaySample(a[0]));
            break;
        case kGetHandle:
            result = Low32(h->GetHandle());
            break;
        case kSetHandle:
            result = Low32(h->SetHandle(Cookie(a[0])));
            break;
        case kSoundCallback:
            h->CAudioPlaceHolder::SoundCallback(Cookie(a[0]), Cookie(a[1]));
            break;
        case kCallbackCtor:
            new (block) TbSndCallback();
            result = 1;   // the vptr is checked on the PowerPC side
            dumpHolder = false;
            break;
        case kBaseCompleted: {
            TbSndCallback callback;
            callback.TbSndCallback::CompletedCallback(Cookie(a[0]), Cookie(a[1]), static_cast<int32_t>(a[2]));
            dumpHolder = false;
            break;
        }
        default:
            break;
    }
    gTrack = false;
    tpw_cl_on = 0;

    out[O_R] = static_cast<uint32_t>(result);
    out[O_R2] = second;
    CAudioPlaceHolder* subject = op == kCtor ? plain : h;
    if (dumpHolder && subject != nullptr) Dump(subject, out + O_STATE);
    out[O_CAT] = static_cast<uint32_t>(gContainer->mRefCount);
    DumpInfo(*info, out + O_INFO);
    out[O_FILE] = static_cast<uint32_t>(gFileOut.size());
    for (std::size_t i = 0; i < gFileOut.size() && i < 480; ++i) out[O_FILE + 1 + i / 4] |= static_cast<uint32_t>(gFileOut[i]) << (8 * (i % 4));
    for (std::size_t i = 0; i < gSounds.size() && i < 8; ++i) {
        const CSoundData& sd = *gSounds[i];
        out[O_DATA + 2 * i] = sd.mF0C[0] | (sd.mF0C[1] << 8) | (sd.mF0C[2] << 16) | (static_cast<uint32_t>(sd.mF0C[3]) << 24);
        out[O_DATA + 2 * i + 1] = static_cast<uint32_t>(sd.mF10) | (static_cast<uint32_t>(sd.mF12) << 16);
    }
    out[O_STATIC] = LabelOf(CAudioPlaceHolder::mpSoundManager);
    out[O_STATIC + 1] = static_cast<uint32_t>(CAudioPlaceHolder::mRandomSeed);
}

}  // extern "C"
