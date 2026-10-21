// Fakes and test entry point for unit w79g3, the placeholder subclasses (libtpw_unit_PlaceHolderTypes.dylib, driven by
// tests/test_PlaceHolderTypes.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/ListSentence.cpp, src/sound/PlaceHolderSentence.cpp, src/sound/PlaceHolderTypes.cpp  CPlaceHolderOneShot, ...OneShotSentence, ...OneShotBranchingSentence, CPlaceHolderSentence and its
//                             Linear, Branching, Shuffle (with CListShuffle) and CListSentence (with CListData) subclasses (under test)
// Real as well: the placeholder base CAudioPlaceHolder (w79g2, src/sound/AudioPlaceHolder.cpp, src/sound/AudioPlaceHolderList.cpp, src/sound/TbAudioBody.cpp, src/sound/TbSoundSampleCallback.cpp), the one-shot sentence Elements (w79g4,
// src/sound/AudioPlaceHolder.cpp, src/sound/PlaceHolderSentence.cpp, src/sound/TbMapStreamer.cpp), the info records and positions (w77), TbSoundBank.cpp (w72), TbRefRelease (w78), CAudioComponent and
// the command constructors (w79b). On the PowerPC side every one of them runs as the original, so a subclass runs over the real
// base functions and the real vtable slots. Fakes that record what they are given (the PowerPC side records the same from its
// stubs): the sound manager and the core under it (w79e is not linked: getters, Rolloff, PlaySample, ConstructSample, SystemCommand),
// the commands' Execute bodies (w79d), the category container, TbMapStreamer::Instance (a block of bytes with the category table in
// it) and FixUpCatagoryIndex, TbEventMapper::MapEvent, bfl::maths::Random and RangedRandomSlow, LbTime_GetClock, LbFile_Read and
// LbFile_Write.
//
// Labels: 1 the subject, 2 the manager, 3 the volume control, 4 the category container, 5 the sample info, 6 spare, 7 the streamer,
// 8 the subject's mHandle field (the manager's "control"), 9 the array of a CListData, 10 the standalone list data or shuffle,
// 11..13 the banks, 14 the event list, 16 the category data, 17 the subject's extra data, 21.. the events, 31.. the sounds (flat),
// 40..43 the category containers, 50..53 the chained children, 55..58 their extra data, 71.. the samples (flat), 480..483 the
// children's mHandle fields, 150 and up the objects made.
//
// tpw_pt_run drives one operation on a world it builds from the parameter words (the P_ enum, shared with the Python file).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "AudioPlaceHolder.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
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

// The runtime's new: blocks are filled with 0xA5; the objects the code makes are recorded with the Mac's size (the extra data 0x20,
// a placeholder 0x60, the list data 0xc, the shuffle 0x10).
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    if (size == sizeof(CAudioPlaceHolderExtraData)) ev("new", {0x20, newlab(block)});
    else if (size == sizeof(CAudioPlaceHolder)) ev("new", {0x60, newlab(block)});
    else if (size == sizeof(CListSentence::CListData)) ev("new", {0xc, newlab(block)});
    else if (size == sizeof(CPlaceHolderSentenceShuffle::CListShuffle)) ev("new", {0x10, newlab(block)});
}

// Blocks the runtime frees are kept (no address is reused while a stale pointer to it is compared).
bool KeepFreed(void*, bool) {
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) Allocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

static_assert(sizeof(CAudioPlaceHolderExtraData) != sizeof(CAudioPlaceHolder), "the allocation observer tells the objects apart by size");
static_assert(sizeof(CListSentence::CListData) != sizeof(CAudioPlaceHolderExtraData), "the allocation observer tells the objects apart by size");
static_assert(sizeof(CPlaceHolderSentenceShuffle::CListShuffle) != sizeof(CListSentence::CListData), "the allocation observer tells the objects apart by size");
static_assert(sizeof(CPlaceHolderSentenceShuffle::CListShuffle) != sizeof(CAudioPlaceHolderExtraData), "the allocation observer tells the objects apart by size");
static_assert(sizeof(CPlaceHolderOneShotSentenceElement) == sizeof(CAudioPlaceHolder), "a subclass adds no data member");
static_assert(sizeof(CPlaceHolderOneShotSentence) == sizeof(CAudioPlaceHolder), "a subclass adds no data member");
static_assert(sizeof(CListSentence) == sizeof(CAudioPlaceHolder), "a subclass adds no data member");

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
// The one call the base placeholder makes: the command is executed (the Execute fakes below record its words).
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

// ---- the command bodies (w79d) and the sample info records -------------------------------------------------------------------

namespace {

int64_t EventWordLabel(uint32_t word);

// The sample info as the command (or the manager's PlaySample and ConstructSample) receives it: the words the constructor and the
// setters define (flag-gated), the positions it points to by value.
void RecInfo(const char* name, int64_t first, const TbSoundSampleInfo& i, bool labelledOnly = false) {
    const uint32_t flags = i.mFlags;
    // a position that is not a labelled object may be a dead local of the caller (a non-positional Element's): its words are not read
    const bool readPosition = i.mPosition != nullptr && (!labelledOnly || lab(i.mPosition) > 0);
    const bool readVelocity = i.mVelocity != nullptr && (!labelledOnly || lab(i.mVelocity) > 0);
    ev(name, {first, i.mNumber, lab(i.mBank), i.mField0C, i.mField10, flags, i.mPriority, (flags & 2) != 0 ? i.mVolume : 0,
              (flags & 0x20) != 0 ? static_cast<uint32_t>(i.mPitch) : 0, (flags & 4) != 0 ? EventWordLabel(i.mUserId) : 0,
              (flags & 0x40) != 0 ? i.mLoopCount : 0, (flags & 0x2000) != 0 ? lab(i.mCompletedCallback) : 0});
    ev(std::string(name).append(".more").c_str(),
       {(flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mA)) : 0, (flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mB)) : 0,
        (flags & 0x200) != 0 ? Canon(FloatBits(i.mDistance.mC)) : 0, (flags & 0x1000) != 0 ? Low32(i.mUserData) : 0,
        (flags & 0x40000) != 0 ? lab(i.mVolumeControl) : 0, (flags & 0x40000) != 0 ? i.mVolumeControlIndex : 0});
    ev(std::string(name).append(".pos").c_str(),
       {lab(i.mPosition), readPosition ? Canon(FloatBits(i.mPosition->mX)) : 0, readPosition ? Canon(FloatBits(i.mPosition->mY)) : 0,
        readPosition ? Canon(FloatBits(i.mPosition->mZ)) : 0, lab(i.mVelocity), readVelocity ? Canon(FloatBits(i.mVelocity->mX)) : 0,
        readVelocity ? Canon(FloatBits(i.mVelocity->mY)) : 0, readVelocity ? Canon(FloatBits(i.mVelocity->mZ)) : 0});
}

// The sample callback (flag 0x800 of the info's flags) as its own event: the Sentence classes make themselves the callback.
void RecInfoCallback(const char* name, const TbSoundSampleInfo& i) {
    ev(std::string(name).append(".callback").c_str(), {(i.mFlags & 0x800) != 0 ? lab(i.mCallback) : 0});
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

// The two manager calls the subclasses make (w79e's, recording here): PlaySample starts a sample (the control the placeholder passes
// is its own mHandle field; the fake writes a control of its own), ConstructSample makes the next sample of a sentence.
void* TbSoundManager::PlaySample(TbSoundSampleInfo& info, void*& control) {
    ev("Mgr.PlaySample", {lab(this), lab(&control)});
    RecInfo("Mgr.PlaySample.info", 0, info, true);
    RecInfoCallback("Mgr.PlaySample.info", info);
    control = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("PlayControl", 0x4321))));
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("PlaySample", 0))));
}

void* TbSoundManager::ConstructSample(void* handle, void*& control, TbSoundSampleInfo& info) {
    ev("Mgr.ConstructSample", {lab(this), Low32(handle)});
    RecInfo("Mgr.ConstructSample.info", 0, info, true);
    RecInfoCallback("Mgr.ConstructSample.info", info);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("ConstructSample", 0))));
}

// ---- the category container, the streamer and the event mapper ------------------------------------------------------------------

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

CEventData* EventOf(uint32_t label);

}  // namespace

// The streamer is a block of bytes of which only the category table (the deque at Mac +0x1c) is ever made.
TbMapStreamer* TbMapStreamer::Instance() {
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

uint32_t TbMapStreamer::FixUpCatagoryIndex(uint32_t index) {
    ev("FixUp", {lab(this), index});
    return static_cast<uint32_t>(ret("FixUp", 0));
}

// The event mapper (w70's real one reads the map data): records the category data, the event's two words and its position, answers
// the event of the queue "Mapped" (a label 21.., else NULL).
CEventData* TbEventMapper::MapEvent(CCatagoryData* data, const sound::TbAudioEvent& event) {
    ev("MapEvent", {lab(data), event.mCategory, event.mEvent, Canon(FloatBits(event.mPosition.mX)), Canon(FloatBits(event.mPosition.mY)),
                    Canon(FloatBits(event.mPosition.mZ))});
    return EventOf(static_cast<uint32_t>(ret("Mapped", 0)));
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

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("File.Write", {lab(file), length});
    if (transferred != nullptr) *transferred = length;
    return TbError(static_cast<int32_t>(ret("FileErr", 0)));
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    std::memset(buffer, 0, length);
    ev("File.Read", {lab(file), length});
    if (transferred != nullptr) *transferred = length;
    return TbError(static_cast<int32_t>(ret("FileErr", 0)));
}

// ---- the parameter and result words (shared with tests/test_PlaceHolderTypes.py) ------------------------------------------------

namespace {

enum Param {
    P_OP = 0, P_MGR = 1, P_SEED = 2, P_KIND = 3, P_NCHILD = 4, P_NCAT = 5, P_BKIND = 6, P_A = 8, P_STATE = 24, P_CHILD = 60, P_INFO = 198,
    P_LIST = 202, P_CAT = 222, P_EVT = 232, P_SHUF = 240, P_IMG = 256
};
// The preset (and dumped) state of a placeholder, 34 words from P_STATE / P_CHILD / O_STATE.
enum State {
    S_EVENT = 0, S_SOUND, S_SAMPLE, S_POSX, S_POSY, S_POSZ, S_VELX, S_VELY, S_VELZ, S_ENDTIME, S_PRIORITY, S_HANDLE, S_CATAGORY,
    S_LISTHANDLE, S_FLAGS, S_NEXT, S_PREVIOUS, S_PAUSETIME, S_MAXDIST, S_LASTSOUND, S_LASTSAMPLE, S_VCINDEX, S_FADE, S_VOLUME,
    S_PITCH, S_EXTRA, S_IDS, S_VALUES, S_LEFT, S_TOP, S_RIGHT, S_BOTTOM, S_CHILD, S_PARENT, S_COUNT
};
enum Out {
    O_R = 0, O_STATE = 8, O_CHILD = 44, O_CAT = 184, O_INFO = 186, O_STATIC = 210, O_SOUNDS = 214, O_AUX = 336, O_SLOT = 360, O_BLOCK = 392
};
enum Kind { kOS = 0, kOSS, kOSBS, kSEN, kLIN, kBRA, kSHU, kLST, kKinds };
enum Op {
    kCtor = 0, kDtor = 8, kClassId = 16, kCreate1 = 24, kCreate4 = 32, kGetElement = 40, kLDCtor, kLDDtor, kShCtor, kShInit, kShShuffle,
    kShNext, kSetListInfo, kChoose, kAssign, kUpdParam, kPlay, kStop, kUpdate, kUpdDep, kFirstChild, kProcess, kAddElement, kRemove,
    kCallback, kBasic
};

// ---- the data image ---------------------------------------------------------------------------------------------------------

// The map data of a case is one block of the arena: the test gives its fields (position, size, value; a pointer field holds a
// position in the block and is relocated into an arena offset) and the positions that carry labels.
std::map<int, void*> gLabelPtr;
std::vector<CEventData*> gEvents;   // label 21 + index
uint8_t* gBlock;
uint8_t* gPad;   // 128 KiB allocated before the image, so that the image's arena offsets have bits above the low 16 (a record moved byte by byte shows)

void FreeImage() {
    if (gBlock != nullptr) tpw::ArenaFree(gBlock);
    if (gPad != nullptr) tpw::ArenaFree(gPad);
    gBlock = nullptr;
    gPad = nullptr;
    gLabelPtr.clear();
    gEvents.clear();
}

// img: [size, nfields, nlabels, (position, size | pointer << 8, value) * nfields, (label, position) * nlabels]
void BuildImage(const uint32_t* img) {
    FreeImage();
    const uint32_t size = img[0];
    const uint32_t fields = img[1];
    const uint32_t labels = img[2];
    gPad = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x20000));
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
CEventData* EventOfImpl(uint32_t label) { return static_cast<CEventData*>(label >= 21 && label < 31 ? PtrOf(label) : nullptr); }
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

}  // namespace

namespace {
CEventData* EventOf(uint32_t label) { return EventOfImpl(label); }
}  // namespace

// ---- the placeholders -------------------------------------------------------------------------------------------------------

namespace {

void* NewFilled(std::size_t size) {
    return ::operator new(size);   // the observer fills the block with 0xA5
}

// The preset: pointer words are labels (0 NULL), the container word is 0 or 1. The class's own constructor may have made the extra
// data (the sentences' do): then it is updated in place and its +0x18 and +0x1c words are left alone.
void Preset(CAudioPlaceHolder* h, const uint32_t* s, int extraLabel) {
    h->mEvent = EventOfImpl(s[S_EVENT]);
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
        if (h->mExtra == nullptr) h->mExtra = new CAudioPlaceHolderExtraData();
        if (extraLabel != 0) tpw_cl_label(extraLabel, h->mExtra);   // so that the destructor's delete is recorded
        std::memcpy(h->mExtra->mParamId, &s[S_IDS], 4);
        std::memcpy(h->mExtra->mParamValue, &s[S_VALUES], 4);
        h->mExtra->mBounds.left = static_cast<int32_t>(s[S_LEFT]);
        h->mExtra->mBounds.top = static_cast<int32_t>(s[S_TOP]);
        h->mExtra->mBounds.right = static_cast<int32_t>(s[S_RIGHT]);
        h->mExtra->mBounds.bottom = static_cast<int32_t>(s[S_BOTTOM]);
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

// The chain of children from the placeholder's extra data: up to four, each as its label and its 34 state words.
uint32_t DumpChain(CAudioPlaceHolder* h, uint32_t* o) {
    if (h->mExtra == nullptr) return 0;
    uint32_t i = 0;
    for (CAudioPlaceHolder* c = h->mExtra->mChild; c != nullptr && i < 8; c = c->mExtra != nullptr ? c->mExtra->mChild : nullptr, ++i) {
        if (i < 3) {
            o[i * 35] = LabelOf(c) | (c->GetClassID() << 16);   // the label and, through the vtable, the class id
            Dump(c, o + i * 35 + 1);
        }
    }
    return i;
}

// The sample info as the Mac lays it out in words: the fields the constructor and the setters define and the untouched rest
// (0xA5 fill on both sides). A position or velocity pointer that is not a labelled object dumps as -1.
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

// The sound records of event 21 (up to 12), as ten words each: the fields the shuffle moves, with the pointers as positions in the block.
void DumpSounds(uint32_t* o) {
    CEventData* event = EventOfImpl(21);
    if (event == nullptr) return;
    const uint32_t base = tpw::ArenaOffset(gBlock);
    uint32_t count = event->mSoundCount < 12 ? event->mSoundCount : 12;
    for (uint32_t i = 0; i < count; ++i) {
        const CSoundData& s = event->mSounds.get()[i];
        uint32_t* w = o + i * 10;
        w[0] = s.mSampleCount;
        w[1] = s.mBranchCount;
        w[2] = s.mSamples.mOffset != 0 ? s.mSamples.mOffset - base : 0;
        w[3] = s.mF0C[0] | (s.mF0C[1] << 8) | (s.mF0C[2] << 16) | (static_cast<uint32_t>(s.mF0C[3]) << 24);
        w[4] = s.mF10 | (static_cast<uint32_t>(s.mF12) << 16);
        w[5] = s.mF16 | (static_cast<uint32_t>(s.mF18) << 16);
        w[6] = s.mF1A | (static_cast<uint32_t>(s.mF1C) << 16);
        w[7] = s.mF1E;
        w[8] = Canon(FloatBits(s.mF22));
        w[9] = s.mBranches.mOffset != 0 ? s.mBranches.mOffset - base : 0;
    }
}

TbSoundManager* gManager;
alignas(16) uint8_t gListBytes[sizeof(TbAudioEventList) + 16];
TbAudioEventList::ListItem gItems[8];
alignas(16) uint8_t gInfoBytes[sizeof(TbSoundSampleInfo) + 16];

// A subject of the given kind, constructed in `memory` by the class's own constructor.
CAudioPlaceHolder* Construct(int kind, void* memory, const TbAudioEventList* list) {
    switch (kind) {
        case kOS: return new (memory) CPlaceHolderOneShot();
        case kOSS: return new (memory) CPlaceHolderOneShotSentence();
        case kOSBS: return new (memory) CPlaceHolderOneShotBranchingSentence();
        case kSEN: return new (memory) CPlaceHolderSentence();
        case kLIN: return new (memory) CPlaceHolderLinearSentence();
        case kBRA: return new (memory) CPlaceHolderBranchingSentence();
        case kSHU: return new (memory) CPlaceHolderSentenceShuffle();
        default: return new (memory) CListSentence(list);
    }
}

}  // namespace

// ---- the entry point ---------------------------------------------------------------------------------------------------------

extern "C" {

// One operation. p: the parameter words (P_ enum), out: the results (O_ enum).
void tpw_pt_run(int32_t op, const uint32_t* p, uint32_t* out) {
    std::memset(out, 0, 1024 * sizeof(uint32_t));
    tpw_cl_on = 0;
    gTrack = false;

    // ---- the world, made with the log off
    const int kind = static_cast<int>(p[P_KIND]);
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

    // the category table of the streamer (for CListSentence): containers whose data is the image's category data or NULL
    CResize<CCatagoryContainer*>* deque = ::new (&TbMapStreamer::Instance()->mCatagories) CResize<CCatagoryContainer*>();
    const uint32_t ncat = p[P_NCAT];
    std::vector<FakeContainer*> containers;
    if (ncat != 0) {
        deque->mData = new CCatagoryContainer*[ncat];
        deque->mSize = ncat;
    }
    for (uint32_t i = 0; i < ncat; ++i) {
        const uint32_t present = p[P_CAT + 2 * i];
        const uint32_t data = p[P_CAT + 2 * i + 1];
        FakeContainer* container = nullptr;
        if (present != 0) {
            container = new FakeContainer();
            container->mData = data != 0 ? static_cast<CCatagoryData*>(PtrOf(16)) : nullptr;
            tpw_cl_label(40 + static_cast<int>(i), container);
        }
        deque->mData[i] = container;
        containers.push_back(container);
    }

    // the event list (and the event of the CreateObject statics): category, event, position, the float, and the list items
    std::memset(gListBytes, 0xA5, sizeof gListBytes);
    TbAudioEventList* list = new (gListBytes) TbAudioEventList();
    list->mCategory = p[P_EVT];
    list->mEvent = p[P_EVT + 1];
    list->mPosition.mX = BitsToFloat(p[P_EVT + 2]);
    list->mPosition.mY = BitsToFloat(p[P_EVT + 3]);
    list->mPosition.mZ = BitsToFloat(p[P_EVT + 4]);
    list->mW = BitsToFloat(p[P_EVT + 5]);
    list->mCount = p[P_LIST];
    for (uint32_t i = 0; i < 8; ++i) {
        gItems[i].mCategory = p[P_LIST + 1 + 2 * i];
        gItems[i].mEvent = p[P_LIST + 2 + 2 * i];
    }
    list->mItems = gItems;
    tpw_cl_label(14, list);

    // the sample info
    std::memset(gInfoBytes, 0xA5, sizeof gInfoBytes);
    TbSoundSampleInfo* info = new (gInfoBytes) TbSoundSampleInfo();
    if (p[P_INFO + 2] != 0) {
        info->mField0C = p[P_INFO];
        info->mFlags = p[P_INFO + 1];
    }
    tpw_cl_label(5, info);

    // the subject
    const bool kindOp = op < kGetElement;
    const bool createOp = (op >= kCreate1 && op < kGetElement);
    const bool standalone = op >= kLDCtor && op <= kShNext;   // list data and shuffle operations: no subject
    CAudioPlaceHolder* subject = nullptr;
    CAudioPlaceHolder* slot = nullptr;
    void* ctorMemory = op < kDtor ? NewFilled(sizeof(CAudioPlaceHolder)) : nullptr;   // made before the call is observed
    std::vector<CAudioPlaceHolder*> children;
    CListSentence::CListData* data = nullptr;
    CPlaceHolderSentenceShuffle::CListShuffle* shuffle = nullptr;
    (void)kindOp;
    if (createOp) {
        // a fresh slot with the list's words set: the subclass constructor must leave them alone
        slot = static_cast<CAudioPlaceHolder*>(NewFilled(sizeof(CAudioPlaceHolder)));
        slot->mListHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(state[S_LISTHANDLE]));
        slot->mNext = reinterpret_cast<CAudioPlaceHolder*>(static_cast<uintptr_t>(state[S_NEXT]));
        slot->mPrevious = reinterpret_cast<CAudioPlaceHolder*>(static_cast<uintptr_t>(state[S_PREVIOUS]));
        if (a[7] != 0) slot = nullptr;
    } else if (!standalone && !(op >= kCtor && op < kDtor)) {
        subject = Construct(kind, NewFilled(sizeof(CAudioPlaceHolder)), list);
        Preset(subject, state, 17);
        if (kind == kLST) {
            // the list data and its array are labelled so that their deletion shows
            CListSentence::CListData* made = reinterpret_cast<CListSentence::CListData*>(subject->mExtra->mParent);
            tpw_cl_label(10, made);
            tpw_cl_label(9, static_cast<TbAudioEventList::ListItem*>(made->mItems));
        }
        tpw_cl_label(1, subject);
        tpw_cl_label(8, &subject->mHandle);
        tpw_cl_label(18, &subject->mPosition);
        tpw_cl_label(19, &subject->mVelocity);
        // the chain of children: Elements (or branching Elements) of the subject, each preset
        CAudioPlaceHolder* previous = subject;
        for (uint32_t i = 0; i < p[P_NCHILD] && i < 4; ++i) {
            void* memory = NewFilled(sizeof(CAudioPlaceHolder));
            CAudioPlaceHolder* child = p[P_BKIND] != 0 ? static_cast<CAudioPlaceHolder*>(new (memory) CPlaceHolderOneShotBranchingSentenceElement(subject))
                                                         : static_cast<CAudioPlaceHolder*>(new (memory) CPlaceHolderOneShotSentenceElement(subject));
            Preset(child, p + P_CHILD + 34 * i, 55 + static_cast<int>(i));
            tpw_cl_label(50 + static_cast<int>(i), child);
            tpw_cl_label(480 + static_cast<int>(i), &child->mHandle);
            tpw_cl_label(490 + 2 * static_cast<int>(i), &child->mPosition);
            tpw_cl_label(491 + 2 * static_cast<int>(i), &child->mVelocity);
            previous->mExtra->mChild = child;
            previous = child;
            children.push_back(child);
        }
        // the shuffle of a CPlaceHolderSentenceShuffle (words: present, event label, seed, index, toggle, step 0, step 1)
        if (kind == kSHU && p[P_SHUF] != 0) {
            shuffle = static_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(NewFilled(sizeof(CPlaceHolderSentenceShuffle::CListShuffle)));
            shuffle->mEvent = EventOfImpl(p[P_SHUF + 1]);
            shuffle->mSeed = p[P_SHUF + 2];
            shuffle->mIndex = p[P_SHUF + 3];
            shuffle->mToggle = static_cast<uint8_t>(p[P_SHUF + 4]);
            shuffle->mStep[0] = static_cast<uint8_t>(p[P_SHUF + 5]);
            shuffle->mStep[1] = static_cast<uint8_t>(p[P_SHUF + 6]);
            subject->mExtra->mParent = reinterpret_cast<CAudioPlaceHolder*>(shuffle);
            tpw_cl_label(10, shuffle);
        }
        if (kind == kLST && p[P_SHUF] != 0) {
            data = reinterpret_cast<CListSentence::CListData*>(subject->mExtra->mParent);
            data->mIndex = p[P_SHUF + 3];
        }
    }
    // the standalone shuffle and list data
    alignas(16) static uint8_t shuffleBytes[sizeof(CPlaceHolderSentenceShuffle::CListShuffle) + 16];
    alignas(16) static uint8_t dataBytes[sizeof(CListSentence::CListData) + 16];
    if (standalone) {
        std::memset(shuffleBytes, 0xA5, sizeof shuffleBytes);
        std::memset(dataBytes, 0xA5, sizeof dataBytes);
        if (op == kShInit || op == kShShuffle || op == kShNext) {
            shuffle = reinterpret_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(shuffleBytes);
            shuffle->mEvent = EventOfImpl(p[P_SHUF + 1]);
            shuffle->mSeed = p[P_SHUF + 2];
            shuffle->mIndex = p[P_SHUF + 3];
            shuffle->mToggle = static_cast<uint8_t>(p[P_SHUF + 4]);
            shuffle->mStep[0] = static_cast<uint8_t>(p[P_SHUF + 5]);
            shuffle->mStep[1] = static_cast<uint8_t>(p[P_SHUF + 6]);
            tpw_cl_label(10, shuffle);
        }
        if (op == kLDDtor) {
            // a list data made by the real constructor, on the heap (the deleting form frees it)
            data = new CListSentence::CListData(*list);
            tpw_cl_label(10, data);
            tpw_cl_label(9, static_cast<TbAudioEventList::ListItem*>(data->mItems));
        }
    }

    // ---- the call
    tpw_cl_on = 1;
    gTrack = true;
    int64_t result = 0;
    uint32_t second = 0;
    bool dumpSubject = subject != nullptr;
    CAudioPlaceHolder* returned = nullptr;
    switch (op) {
        case kCtor + kOS: case kCtor + kOSS: case kCtor + kOSBS: case kCtor + kSEN: case kCtor + kLIN: case kCtor + kBRA:
        case kCtor + kSHU: case kCtor + kLST: {
            subject = Construct(kind, ctorMemory, list);
            tpw_cl_label(1, subject);
            // the vtable is checked through the class id (a virtual call); bit 8 stands for the PowerPC side's check of its vptr
            result = subject->GetClassID() | 0x300;
            dumpSubject = true;
            break;
        }
        case kDtor + kOS: case kDtor + kOSS: case kDtor + kOSBS: case kDtor + kSEN: case kDtor + kLIN: case kDtor + kBRA:
        case kDtor + kSHU: case kDtor + kLST:
            if (static_cast<int16_t>(a[0]) > 0) delete subject;
            else subject->~CAudioPlaceHolder();
            dumpSubject = false;
            break;
        case kClassId + kOS: case kClassId + kOSS: case kClassId + kOSBS: case kClassId + kSEN: case kClassId + kLIN: case kClassId + kBRA:
        case kClassId + kSHU: case kClassId + kLST:
            result = subject->GetClassID();
            break;
        case kCreate1 + kOS: CPlaceHolderOneShot::CreateObject(slot); break;
        case kCreate1 + kOSS: CPlaceHolderOneShotSentence::CreateObject(slot); break;
        case kCreate1 + kOSBS: CPlaceHolderOneShotBranchingSentence::CreateObject(slot); break;
        case kCreate1 + kSEN: CPlaceHolderSentence::CreateObject(slot); break;
        case kCreate1 + kLIN: CPlaceHolderLinearSentence::CreateObject(slot); break;
        case kCreate1 + kBRA: CPlaceHolderBranchingSentence::CreateObject(slot); break;
        case kCreate1 + kSHU: CPlaceHolderSentenceShuffle::CreateObject(slot); break;
        case kCreate1 + kLST: CListSentence::CreateObject(slot, list); break;
        case kCreate4 + kOS: CPlaceHolderOneShot::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kOSS: CPlaceHolderOneShotSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kOSBS: CPlaceHolderOneShotBranchingSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kSEN: CPlaceHolderSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kLIN: CPlaceHolderLinearSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kBRA: CPlaceHolderBranchingSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kSHU: CPlaceHolderSentenceShuffle::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kCreate4 + kLST: CListSentence::CreateObject(gContainer, slot, EventOfImpl(a[0]), *list); break;
        case kGetElement:
            returned = static_cast<CPlaceHolderOneShotSentence*>(subject)->GetElement();
            result = LabelOf(returned);
            break;
        case kLDCtor:
            data = new (dataBytes) CListSentence::CListData(*list);
            tpw_cl_label(10, data);
            tpw_cl_label(9, static_cast<TbAudioEventList::ListItem*>(data->mItems));
            break;
        case kLDDtor:
            if (static_cast<int16_t>(a[0]) > 0) delete data;
            else data->~CListData();
            break;
        case kShCtor:
            shuffle = new (shuffleBytes) CPlaceHolderSentenceShuffle::CListShuffle(EventOfImpl(a[0]));
            tpw_cl_label(10, shuffle);
            break;
        case kShInit:
            shuffle->Init(EventOfImpl(a[0]));
            break;
        case kShShuffle:
            shuffle->Shuffle();
            break;
        case kShNext:
            returned = nullptr;
            result = LabelOf(shuffle->GetNextSound(a[0]));
            break;
        case kSetListInfo:
            static_cast<CPlaceHolderSentenceShuffle*>(subject)->SetListInfo(EventOfImpl(a[0]));
            shuffle = reinterpret_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(subject->mExtra->mParent);
            break;
        case kChoose:
            result = subject->ChooseRandomSound(a[0] != 0);
            break;
        case kAssign:
            result = static_cast<CPlaceHolderBranchingSentence*>(subject)->AssignSoundToNextBranch(a[0]);
            break;
        case kUpdParam:
            subject->UpdateParameter(static_cast<uint8_t>(a[0]), a[1]);
            break;
        case kPlay:
            result = static_cast<int32_t>(subject->PlaySample(a[0]));
            break;
        case kStop:
            result = static_cast<int32_t>(subject->Stop());
            break;
        case kUpdate: {
            TbSoundPosition position(BitsToFloat(a[0]), BitsToFloat(a[1]), BitsToFloat(a[2]));
            TbSoundPosition velocity(BitsToFloat(a[3]), BitsToFloat(a[4]), BitsToFloat(a[5]));
            result = static_cast<int32_t>(subject->Update(position, velocity));
            break;
        }
        case kUpdDep:
            subject->UpdateParameterDepandants();
            break;
        case kFirstChild:
            returned = static_cast<CPlaceHolderOneShotSentence*>(subject)->GetFirstChild(a[0]);
            result = LabelOf(returned);
            break;
        case kProcess:
            static_cast<CPlaceHolderOneShotSentence*>(subject)->ProcessChildPlaceHolders(a[0]);
            break;
        case kAddElement: {
            CAudioPlaceHolder* parent = a[1] == 0 ? subject : (a[1] <= children.size() ? children[a[1] - 1] : nullptr);
            result = static_cast<CPlaceHolderOneShotSentence*>(subject)->AddNewElementTo(parent, a[0]);
            break;
        }
        case kRemove:
            static_cast<CPlaceHolderOneShotSentence*>(subject)->RemoveCompleteChildPlaceHolders(a[0]);
            break;
        case kCallback:
            subject->SoundCallback(reinterpret_cast<void*>(static_cast<uintptr_t>(a[0])), reinterpret_cast<void*>(static_cast<uintptr_t>(a[1])));
            break;
        case kBasic:
            result = subject->FillBasicSampleInfo(*info);
            break;
        default:
            break;
    }
    gTrack = false;
    tpw_cl_on = 0;

    out[O_R] = static_cast<uint32_t>(result);
    out[O_R + 1] = second;
    if (dumpSubject && subject != nullptr) {
        Dump(subject, out + O_STATE);
        out[O_R + 2] = DumpChain(subject, out + O_CHILD);
        if (kind == kLST && subject->mExtra != nullptr && subject->mExtra->mParent != nullptr && op != kSetListInfo) {
            CListSentence::CListData* d = reinterpret_cast<CListSentence::CListData*>(subject->mExtra->mParent);
            out[O_AUX] = LabelOf(d);
            out[O_AUX + 1] = d->mCount;
            out[O_AUX + 2] = d->mIndex;
            const TbAudioEventList::ListItem* items = d->mItems;
            for (uint32_t i = 0; i < d->mCount && i < 8; ++i) {
                out[O_AUX + 3 + 2 * i] = items[i].mCategory;
                out[O_AUX + 4 + 2 * i] = items[i].mEvent;
            }
        }
    }
    if (returned != nullptr) {
        out[O_CHILD + 3 * 35] = LabelOf(returned) | (returned->GetClassID() << 16);
        Dump(returned, out + O_CHILD + 3 * 35 + 1);
    }
    if (dumpSubject && subject != nullptr && kind == kSHU && subject->mExtra != nullptr && subject->mExtra->mParent != nullptr) {
        shuffle = reinterpret_cast<CPlaceHolderSentenceShuffle::CListShuffle*>(subject->mExtra->mParent);
    }
    if (shuffle != nullptr) {
        out[O_AUX + 12] = LabelOf(shuffle);
        out[O_AUX + 13] = LabelOf(shuffle->mEvent);
        out[O_AUX + 14] = shuffle->mSeed;
        out[O_AUX + 15] = shuffle->mIndex;
        out[O_AUX + 16] = shuffle->mStep[0];
        out[O_AUX + 17] = shuffle->mStep[1];
        out[O_AUX + 18] = shuffle->mToggle;
    }
    if (data != nullptr && op >= kLDCtor && op <= kLDDtor && op == kLDCtor) {
        out[O_AUX] = LabelOf(data);
        out[O_AUX + 1] = data->mCount;
        out[O_AUX + 2] = data->mIndex;
        const TbAudioEventList::ListItem* items = data->mItems;
        for (uint32_t i = 0; i < data->mCount && i < 8; ++i) {
            out[O_AUX + 3 + 2 * i] = items[i].mCategory;
            out[O_AUX + 4 + 2 * i] = items[i].mEvent;
        }
    }
    if (createOp && slot != nullptr) {
        // the slot after CreateObject: the list's words, the class id through the vtable and the first words
        out[O_SLOT] = LabelOf(slot);
        out[O_SLOT + 1] = Low32(slot->mListHandle);
        out[O_SLOT + 2] = Low32(slot->mNext);
        out[O_SLOT + 3] = Low32(slot->mPrevious);
        // the object is whole when it was constructed: ask its class id (a virtual call); a slot never constructed would crash, so only
        // when the case says the construction happened (a[5] says the Python side expects an object)
        if (a[5] != 0) {
            out[O_R] = 0x300;   // the vptr and the bytes past the object are checked on the PowerPC side
            out[O_SLOT + 4] = slot->GetClassID();
            Dump(slot, out + O_SLOT + 8);
        }
    }
    DumpSounds(out + O_SOUNDS);
    out[O_CAT] = static_cast<uint32_t>(gContainer->mRefCount);
    DumpInfo(*info, out + O_INFO);
    out[O_STATIC] = LabelOf(CAudioPlaceHolder::mpSoundManager);
    out[O_STATIC + 1] = static_cast<uint32_t>(CAudioPlaceHolder::mRandomSeed);

    // the streamer's table is taken down
    delete[] deque->mData;
    deque->mData = nullptr;
    deque->mSize = 0;
}

}  // extern "C"
