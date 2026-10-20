// Fakes and test entry point for unit w79g4, the placeholder helpers (libtpw_unit_PlaceHolderHelpers.dylib, driven by
// tests/test_PlaceHolderHelpers.py in the program "sound symbols shared"; see calllog.h):
//   src/PlaceHolderHelpers.cpp  CMapReference, CPlaceHolderOneShotThrowaway, CPlaceHolderOneShotSentenceElement,
//                               CPlaceHolderOneShotBranchingSentenceElement (the unit under test)
//   include/TbMapStreamer.h     sound::TbAudioEvent (the constructor and the four getters, inline)
//   include/PlaceHolderHelpers.h  TbAudioEventList::ListItem and, with include/snd_containers.h, the CAutoArrayPtr<ListItem> instance
// The base class is REAL (src/AudioPlaceHolder.cpp, unit w79g2, run as the original's CAudioPlaceHolder on the PowerPC side), so the
// Element classes are tested over the real base functions and the real vtable slots. Real as well: the info records and positions
// (w77: TbSoundInfo.cpp), TbSoundBank.cpp (w72), TbRefRelease (w78), CAudioComponent and the command constructors (w79b). Fakes that
// record what they are given (the PowerPC side records the same from its stubs): the sound manager and the core under it (w79e is not
// linked), the commands' Execute bodies (w79d), the category container, TbMapStreamer::Instance (the streamer is a block of bytes with
// the category table in it) and FixUpCatagoryIndex, bfl::maths::Random and RangedRandomSlow, LbTime_GetClock, LbFile_Read and
// LbFile_Write. CPlaceHolderOneShot (unit w79g3's) is a fake here: the base constructor and nothing else, which is what the original's
// constructor does besides its vtable, and an answer of the base for PlaySample and GetClassID.
//
// Labels: 1 the subject placeholder, 2 the manager, 3 the volume control, 4 the category container, 5 the sample info, 6 the parent
// placeholder (in the map reference world: the holder), 7 the streamer, 9 the array of a CAutoArrayPtr, 10 its holder, 11..13 the
// banks, 15 the TbAudioEvent, 16 the category data, 17 the subject's extra data, 18 and 19 the parent's position and velocity, 21..
// the events, 31.. the sounds (flat), 40.. the category containers of the map reference world, 71.. the samples (flat), 100 and up
// the objects made.
//
// tpw_ph_run drives one operation on a world it builds from the parameter words (the P_ enum, shared with the Python file).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
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
#include "snd_containers.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wunused-parameter"

namespace {

bool gTrack;   // "new" events are recorded only while the code under test runs

// The runtime's new: blocks are filled with 0xA5; the extra data is recorded with the Mac's size.
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    if (size == sizeof(CAudioPlaceHolderExtraData)) ev("new", {0x20, newlab(block)});
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


// ---- the category container and the streamer ----------------------------------------------------------------------------------

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

FakeContainer* gContainer;

}  // namespace

// The streamer is a block of bytes of which only the category table (the deque at Mac +0x1c) is ever made.
TbMapStreamer* TbMapStreamer::Instance() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

uint32_t TbMapStreamer::FixUpCatagoryIndex(uint32_t index) {
    ev("FixUp", {lab(this), index});
    return static_cast<uint32_t>(ret("FixUp", 0));
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

// CPlaceHolderOneShot (unit w79g3's, not linked here): the base's constructor and an answer of the base for each override (the inline
// stand-in of this unit's time; nothing of this unit calls PlaySample).
CPlaceHolderOneShot::CPlaceHolderOneShot() : CAudioPlaceHolder() {}
CPlaceHolderOneShot::~CPlaceHolderOneShot() {}
TbError CPlaceHolderOneShot::PlaySample(uint32_t now) { return CAudioPlaceHolder::PlaySample(now); }
uint32_t CPlaceHolderOneShot::GetClassID() { return CAudioPlaceHolder::GetClassID(); }

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


// ---- the parameter and result words (shared with tests/test_PlaceHolderHelpers.py) --------------------------------------------

namespace {

enum Param {
    P_OP = 0, P_MGR = 1, P_SEED = 2, P_BRANCHING = 6, P_A = 8, P_STATE = 16, P_PSTATE = 52, P_INFO = 88, P_CAT = 100, P_IMG = 128
};
// The preset (and dumped) state of a placeholder, 34 words from P_STATE / P_PSTATE / O_STATE / O_PSTATE.
enum State {
    S_EVENT = 0, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME, S_PRIORITY, S_HANDLE, S_CATAGORY,
    S_LISTHANDLE, S_FLAGS, S_NEXT, S_PREVIOUS, S_PAUSETIME, S_MAXDIST, S_LASTSOUND, S_LASTSAMPLE, S_VCINDEX, S_FADE, S_VOLUME,
    S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP, S_RIGHT, S_BOTTOM, S_CHILD, S_PARENT, S_COUNT
};
enum Out {
    O_R = 0, O_R2 = 1, O_R3 = 2, O_R4 = 3, O_R5 = 4, O_STATE = 8, O_PSTATE = 44, O_CAT = 80, O_INFO = 84, O_DATA = 112,
    O_STATIC = 130, O_ARRAY = 140, O_BLOCK = 200
};
enum Op {
    kMapRefCtor = 0, kGetSoundIds, kGetSoundPointer, kElementCtor, kBranchingCtor, kThrowawayCtor, kElementDtor, kBranchingDtor,
    kThrowawayDtor, kInit, kFillSampleInfo, kUpdate, kGetParameterValue, kChooseRandomSound, kElementClassID, kBranchingClassID,
    kThrowawayClassID, kThrowawayFill, kEventCtor, kEventGetType, kEventGetCategory, kEventGetEvent, kEventGetPosition,
    kListItemCtor, kArrayCtor, kArrayConvert, kArrayDtor
};

constexpr uint32_t kFill = 0xA5A5A5A5u;

// ---- the data image ---------------------------------------------------------------------------------------------------------

// The map data of a case is one block of the arena: the test gives its fields (position, size, value; a pointer field holds a
// position in the block and is relocated into an arena offset) and the positions that carry labels.
std::map<int, void*> gLabelPtr;
std::vector<CEventData*> gEvents;   // label 21 + index
uint8_t* gBlock;

void FreeImage() {
    if (gBlock != nullptr) tpw::ArenaFree(gBlock);
    gBlock = nullptr;
    gLabelPtr.clear();
    gEvents.clear();
}

// img: [size, nfields, nlabels, (position, size | pointer << 8, value) * nfields, (label, position) * nlabels]
void BuildImage(const uint32_t* img) {
    FreeImage();
    const uint32_t size = img[0];
    const uint32_t fields = img[1];
    const uint32_t labels = img[2];
    gBlock = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size != 0 ? size : 16));
    const uint32_t base = tpw::ArenaOffset(gBlock);
    const uint32_t* w = img + 3;
    for (uint32_t i = 0; i < fields; ++i, w += 3) {
        uint32_t value = w[2];
        if ((w[1] >> 8) != 0) value += base;
        std::memcpy(gBlock + w[0], &value, w[1] & 0xff);   // little-endian host: the low bytes
    }
    for (uint32_t i = 0; i < labels; ++i, w += 2) {
        gLabelPtr[static_cast<int>(w[0])] = gBlock + w[1];
        tpw_cl_label(static_cast<int>(w[0]), gBlock + w[1]);
        if (w[0] >= 21 && w[0] < 31) gEvents.push_back(reinterpret_cast<CEventData*>(gBlock + w[1]));
    }
}

void* PtrOf(uint32_t label) {
    auto it = gLabelPtr.find(static_cast<int>(label));
    return it != gLabelPtr.end() ? it->second : nullptr;
}
CEventData* EventOf(uint32_t label) { return static_cast<CEventData*>(label >= 21 && label < 31 ? PtrOf(label) : nullptr); }
CSoundData* SoundOf(uint32_t label) { return static_cast<CSoundData*>(label >= 31 && label < 71 ? PtrOf(label) : nullptr); }
CSampleData* SampleOf(uint32_t label) { return static_cast<CSampleData*>(label >= 71 ? PtrOf(label) : nullptr); }

uint32_t LabelOf(const void* p) { return static_cast<uint32_t>(lab(p)); }

// A 32-bit word that is an event's address (the sample info's user id): the event's label, else -1.
int64_t EventWordLabel(uint32_t word) {
    if (word == 0) return 0;
    for (std::size_t i = 0; i < gEvents.size(); ++i) {
        if (Low32(gEvents[i]) == word) return lab(gEvents[i]);
    }
    return -1;
}

// ---- the placeholders -------------------------------------------------------------------------------------------------------

void* NewFilled(std::size_t size) {
    return ::operator new(size);   // the observer fills the block with 0xA5
}

CAudioPlaceHolder* gParent;

// The preset: pointer words are labels (0 NULL), the container word is 0 or 1, the parent word is 0 or 1 (the parent placeholder).
void Preset(CAudioPlaceHolder* h, const uint32_t* s, bool subject) {
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
        if (subject) tpw_cl_label(17, h->mExtra);   // so that the destructor's delete is recorded
        std::memcpy(h->mExtra->mParamId, &s[S_IDS], 4);
        std::memcpy(h->mExtra->mParamValue, &s[S_VALUES], 4);
        h->mExtra->mBounds.left = static_cast<int32_t>(s[S_LEFT]);
        h->mExtra->mBounds.top = static_cast<int32_t>(s[S_TOP]);
        h->mExtra->mBounds.right = static_cast<int32_t>(s[S_RIGHT]);
        h->mExtra->mBounds.bottom = static_cast<int32_t>(s[S_BOTTOM]);
        h->mExtra->mParent = s[S_PARENT] != 0 ? gParent : nullptr;
    }
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
        o[S_CHILD] = LabelOf(h->mExtra->mChild);
        o[S_PARENT] = LabelOf(h->mExtra->mParent);
    }
}

// The sample info as the Mac lays it out in words: the fields the constructor and the setters define and the untouched rest
// (0xA5 fill on both sides). A position or velocity pointer that is not a labelled object (the dead locals of a non-positional
// Element's FillSampleInfo) dumps as -1.
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

TbSoundManager* gManager;

}  // namespace

// ---- the entry point ---------------------------------------------------------------------------------------------------------

extern "C" {

// One operation. p: the parameter words (P_ enum), out: the results (O_ enum).
void tpw_ph_run(int32_t op, const uint32_t* p, uint32_t* out) {
    std::memset(out, 0, 1024 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;

    // ---- the world, made with the log off
    const uint32_t* state = p + P_STATE;
    const uint32_t* a = p + P_A;
    BuildImage(p + P_IMG);
    if (gContainer == nullptr) gContainer = new FakeContainer();
    if (gManager == nullptr) gManager = new TbSoundManager();
    gContainer->mRefCount = 1;
    gContainer->mData = a[6] != 0 ? nullptr : static_cast<CCatagoryData*>(PtrOf(16));
    tpw_cl_label(4, gContainer);
    tpw_cl_label(2, gManager);
    tpw_cl_label(3, &gVolumeControl);
    for (int i = 0; i < 3; ++i) {
        std::memset(gBankBytes[i], 0xA5, 16);
        tpw_cl_label(11 + i, gBankBytes[i]);
    }
    tpw_cl_label(7, gStreamerBytes);
    CAudioPlaceHolder::mpSoundManager = p[P_MGR] != 0 ? gManager : nullptr;
    CAudioPlaceHolder::mRandomSeed = static_cast<int32_t>(p[P_SEED]);

    // the parent placeholder (the real constructor, then the preset), made for every placeholder operation
    gParent = nullptr;
    const bool mapOp = op >= kMapRefCtor && op <= kGetSoundPointer;
    const bool placeholderOp = op >= kElementCtor && op <= kThrowawayFill;
    if (placeholderOp) {
        gParent = new (NewFilled(sizeof(CAudioPlaceHolder))) CAudioPlaceHolder();
        Preset(gParent, p + P_PSTATE, false);
        tpw_cl_label(6, gParent);
        tpw_cl_label(18, &gParent->mPosition);
        tpw_cl_label(19, &gParent->mVelocity);
    }

    // the subject: constructed with the real constructor of its class (the constructor operations do it under the log), then preset
    CAudioPlaceHolder* subject = nullptr;
    CPlaceHolderOneShotThrowaway* throwaway = nullptr;
    CPlaceHolderOneShotSentenceElement* element = nullptr;
    CPlaceHolderOneShotBranchingSentenceElement* branching = nullptr;
    void* memory = nullptr;
    const bool constructorOp = op == kElementCtor || op == kBranchingCtor || op == kThrowawayCtor;
    if (placeholderOp) {
        memory = NewFilled(sizeof(CPlaceHolderOneShotBranchingSentenceElement));
        if (!constructorOp) {
            switch (op) {
                case kBranchingDtor:
                case kChooseRandomSound:
                case kBranchingClassID:
                    branching = new (memory) CPlaceHolderOneShotBranchingSentenceElement(gParent);
                    element = branching;
                    throwaway = branching;
                    break;
                case kThrowawayDtor:
                case kThrowawayClassID:
                case kThrowawayFill:
                    throwaway = new (memory) CPlaceHolderOneShotThrowaway();
                    break;
                default:
                    // the Element operations also run on a Branching element (it inherits them) when P_BRANCHING is set
                    if (p[P_BRANCHING] != 0 && (op == kInit || op == kFillSampleInfo || op == kUpdate || op == kGetParameterValue)) {
                        branching = new (memory) CPlaceHolderOneShotBranchingSentenceElement(gParent);
                        element = branching;
                    } else {
                        element = new (memory) CPlaceHolderOneShotSentenceElement(gParent);
                    }
                    throwaway = element;
                    break;
            }
            subject = throwaway;
            Preset(subject, state, true);
            tpw_cl_label(1, subject);
        }
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

    // the map reference world: the category table in the streamer, the containers and the holder
    CResize<CCatagoryContainer*>* deque = nullptr;
    std::vector<FakeContainer*> containers;
    CAudioPlaceHolder* holder = nullptr;
    alignas(16) static uint8_t smallBlock[64];
    std::memset(smallBlock, 0xA5, sizeof smallBlock);
    if (mapOp) {
        deque = ::new (&TbMapStreamer::Instance()->mCatagories) CResize<CCatagoryContainer*>();
        const uint32_t count = a[4];
        if (count != 0) {
            deque->mData = new CCatagoryContainer*[count];
            deque->mSize = count;
        }
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t present = p[P_CAT + 2 * i];
            const uint32_t data = p[P_CAT + 2 * i + 1];
            FakeContainer* container = nullptr;
            if (present != 0) {
                container = new FakeContainer();
                container->mData = data != 0 ? reinterpret_cast<CCatagoryData*>(gBlock + data) : nullptr;
                tpw_cl_label(40 + static_cast<int>(i), container);
            }
            deque->mData[i] = container;
            containers.push_back(container);
        }
        holder = new (NewFilled(sizeof(CAudioPlaceHolder))) CAudioPlaceHolder();
        holder->mEvent = a[0] != 0 ? reinterpret_cast<CEventData*>(gBlock + a[0]) : nullptr;
        holder->mSound = a[1] != 0 ? reinterpret_cast<CSoundData*>(gBlock + a[1]) : nullptr;
        holder->mSample = a[2] != 0 ? reinterpret_cast<CSampleData*>(gBlock + a[2]) : nullptr;
        tpw_cl_label(6, holder);
    }

    // the array and event worlds
    alignas(16) static uint8_t holderBytes[32];
    std::memset(holderBytes, 0xA5, sizeof holderBytes);
    typedef CAutoArrayPtr<TbAudioEventList::ListItem> ItemArray;
    ItemArray* items = nullptr;
    alignas(16) static uint8_t eventBytes[sizeof(sound::TbAudioEvent) + 16];
    std::memset(eventBytes, 0xA5, sizeof eventBytes);
    sound::TbAudioEvent* audioEvent = nullptr;
    if (op >= kEventGetType && op <= kEventGetPosition) {
        audioEvent = new (eventBytes) sound::TbAudioEvent(a[0], a[1], TbSoundPosition(BitsToFloat(a[2]), BitsToFloat(a[3]), BitsToFloat(a[4])));
        tpw_cl_label(15, audioEvent);
    }
    if (op == kArrayConvert || op == kArrayDtor) {
        // a[7] = 1: a holder over NULL (the pointer constructor), else the counted constructor with a[0] elements
        items = a[7] != 0 ? new ItemArray(static_cast<TbAudioEventList::ListItem*>(nullptr)) : new ItemArray(a[0]);
        tpw_cl_label(9, items->mData);
        tpw_cl_label(10, items);
    }

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    int64_t result = 0;
    uint32_t second = 0;
    bool dumpSubject = placeholderOp;
    switch (op) {
        case kMapRefCtor:
            new (smallBlock) CMapReference();
            for (int i = 0; i < 4; ++i) out[O_BLOCK + i] = smallBlock[i];
            break;
        case kGetSoundIds: {
            CMapReference reference;
            int32_t category = static_cast<int32_t>(kFill);
            int32_t event = static_cast<int32_t>(kFill);
            int32_t sound = static_cast<int32_t>(kFill);
            int32_t sample = static_cast<int32_t>(kFill);
            reference.GetSoundIds(category, event, sound, sample, holder);
            out[O_R] = static_cast<uint32_t>(category);
            out[O_R2] = static_cast<uint32_t>(event);
            out[O_R3] = static_cast<uint32_t>(sound);
            out[O_R4] = static_cast<uint32_t>(sample);
            break;
        }
        case kGetSoundPointer: {
            CMapReference reference;
            result = LabelOf(reference.GetSoundPointer(a[3], holder));
            break;
        }
        case kElementCtor:
        case kBranchingCtor:
        case kThrowawayCtor: {
            if (op == kElementCtor) {
                element = new (memory) CPlaceHolderOneShotSentenceElement(gParent);
                subject = element;
            } else if (op == kBranchingCtor) {
                branching = new (memory) CPlaceHolderOneShotBranchingSentenceElement(gParent);
                subject = branching;
            } else {
                throwaway = new (memory) CPlaceHolderOneShotThrowaway();
                subject = throwaway;
            }
            tpw_cl_label(1, subject);
            // the vtable is checked through the class id (a virtual call); bit 8 stands for the PowerPC side's check of its vptr
            result = subject->GetClassID() | 0x300;
            break;
        }
        case kElementDtor:
            if (static_cast<int16_t>(a[0]) > 0) delete element;
            else element->~CPlaceHolderOneShotSentenceElement();
            break;
        case kBranchingDtor:
            if (static_cast<int16_t>(a[0]) > 0) delete branching;
            else branching->~CPlaceHolderOneShotBranchingSentenceElement();
            break;
        case kThrowawayDtor:
            if (static_cast<int16_t>(a[0]) > 0) delete throwaway;
            else throwaway->~CPlaceHolderOneShotThrowaway();
            break;
        case kInit:
            result = element->Init(EventOf(a[0]), a[1]);
            break;
        case kFillSampleInfo:
            result = element->CPlaceHolderOneShotSentenceElement::FillSampleInfo(a[0], *info);
            break;
        case kUpdate:
            element->Update();
            break;
        case kGetParameterValue: {
            uint32_t value = kFill;
            result = element->CPlaceHolderOneShotSentenceElement::GetParameterValue(a[0], value);
            second = value;
            break;
        }
        case kChooseRandomSound:
            result = branching->CPlaceHolderOneShotBranchingSentenceElement::ChooseRandomSound(a[0] != 0);
            break;
        case kElementClassID:
            result = element->CPlaceHolderOneShotSentenceElement::GetClassID();
            break;
        case kBranchingClassID:
            result = branching->CPlaceHolderOneShotBranchingSentenceElement::GetClassID();
            break;
        case kThrowawayClassID:
            result = throwaway->CPlaceHolderOneShotThrowaway::GetClassID();
            break;
        case kThrowawayFill:
            result = throwaway->CPlaceHolderOneShotThrowaway::FillSampleInfo(a[0], *info);
            break;
        case kEventCtor: {
            new (eventBytes) sound::TbAudioEvent(a[0], a[1], TbSoundPosition(BitsToFloat(a[2]), BitsToFloat(a[3]), BitsToFloat(a[4])));
            sound::TbAudioEvent* made = reinterpret_cast<sound::TbAudioEvent*>(eventBytes);
            out[O_BLOCK] = made->mCategory;
            out[O_BLOCK + 1] = made->mEvent;
            out[O_BLOCK + 2] = FloatBits(made->mPosition.mX);
            out[O_BLOCK + 3] = FloatBits(made->mPosition.mY);
            out[O_BLOCK + 4] = FloatBits(made->mPosition.mZ);
            out[O_BLOCK + 5] = FloatBits(made->mW);
            result = 0x300;   // the vtable and the bytes past the object are checked on the PowerPC side
            break;
        }
        case kEventGetType:
            result = audioEvent->sound::TbAudioEvent::GetType();
            break;
        case kEventGetCategory:
            result = audioEvent->sound::TbAudioEvent::GetCategory();
            break;
        case kEventGetEvent:
            result = audioEvent->sound::TbAudioEvent::GetEvent();
            break;
        case kEventGetPosition: {
            const TbSoundPosition& position = audioEvent->sound::TbAudioEvent::GetPosition();
            result = &position == &audioEvent->mPosition ? 1 : 0;
            out[O_BLOCK] = FloatBits(position.mX);
            out[O_BLOCK + 1] = FloatBits(position.mY);
            out[O_BLOCK + 2] = FloatBits(position.mZ);
            break;
        }
        case kListItemCtor: {
            new (smallBlock) TbAudioEventList::ListItem();
            for (int i = 0; i < 8; ++i) out[O_BLOCK + i] = smallBlock[i];
            break;
        }
        case kArrayCtor: {
            items = new (holderBytes) ItemArray(a[0]);
            tpw_cl_label(9, items->mData);
            result = items->mData != nullptr ? 1 : 0;
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(items->mData);
            for (uint32_t i = 0; i < a[0] * 8 && i < 128; ++i) out[O_ARRAY + i / 4] |= static_cast<uint32_t>(bytes[i]) << (8 * (i % 4));
            break;
        }
        case kArrayConvert:
            result = LabelOf(static_cast<TbAudioEventList::ListItem*>(*items));
            break;
        case kArrayDtor:
            if (static_cast<int16_t>(a[1]) > 0) delete items;
            else items->~ItemArray();
            break;
        default:
            break;
    }
    gTrack = false;
    tpw_cl_on = 0;

    out[O_R] = op == kGetSoundIds ? out[O_R] : static_cast<uint32_t>(result);
    out[O_R5] = second;
    if (op == kGetSoundIds) out[O_R5] = 0;
    if (dumpSubject && subject != nullptr) Dump(subject, out + O_STATE);
    if (placeholderOp && gParent != nullptr) Dump(gParent, out + O_PSTATE);
    out[O_CAT] = static_cast<uint32_t>(gContainer->mRefCount);
    DumpInfo(*info, out + O_INFO);
    // the first eight sounds' volume and pitch words (GetRandomVolume and GetRandomPitch swap a minimum and a maximum in place)
    for (int i = 0; i < 8; ++i) {
        CSoundData* sound = SoundOf(31 + i);
        if (sound == nullptr) break;
        out[O_DATA + 2 * i] = sound->mF0C[0] | (sound->mF0C[1] << 8) | (sound->mF0C[2] << 16) | (static_cast<uint32_t>(sound->mF0C[3]) << 24);
        out[O_DATA + 2 * i + 1] = static_cast<uint32_t>(sound->mF10) | (static_cast<uint32_t>(sound->mF12) << 16);
    }
    out[O_STATIC] = LabelOf(CAudioPlaceHolder::mpSoundManager);
    out[O_STATIC + 1] = static_cast<uint32_t>(CAudioPlaceHolder::mRandomSeed);

    // the map reference world is taken down
    if (deque != nullptr) {
        delete[] deque->mData;
        deque->mData = nullptr;
        deque->mSize = 0;
    }
}

}  // extern "C"
