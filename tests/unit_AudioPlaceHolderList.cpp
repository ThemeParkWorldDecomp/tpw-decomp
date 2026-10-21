// Fakes and test entry point for unit w79g1, CAudioPlaceHolderList (libtpw_unit_AudioPlaceHolderList.dylib, driven by
// tests/test_AudioPlaceHolderList.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/AudioPlaceHolderList.cpp, src/sound/PlaceHolderTypes.cpp, src/sound/TbSysCommand.cpp  CAudioPlaceHolderList and its CMemBlock (the unit under test)
// Real: the sound lock (src/sound/AudioComponent.cpp, src/sound/ChannelState.cpp, src/sound/SystemCommand.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundCore.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundPipeChannel.cpp, src/sound/TbSoundSampleStore.cpp, src/sound/TbSoundSampleWAV.cpp) and the info and position records (w77). CAudioPlaceHolder (w79g2) is NOT linked: its
// members the list calls are defined below (the small ones as copies of the translation, which the PowerPC side runs as the
// original; the ones that need the sound manager, the map data or the file helpers as recording fakes), and the subclasses of
// unit w79g3 are the test class TestHolder<N>, constructed into the pool's slots by recording fakes of the CreateObject statics.
// Every fake records the same event on both sides (the PowerPC side records from emulator stubs): the slot's number, the sound
// lock's nesting count, and the arguments.
//
// The fakes of the placeholder: VolumeControlIsOn and ProcessPositionAndPriority answer the queues VC and PPP (default true);
// AllocateExtraData makes the extra data; Save writes the class id and the flags, Load reads the flags (through the file object's
// virtuals); the virtuals Release (the destructor), PlaySample, Stop, Update and UpdateParameter record (queues Play, Stop, Update).
// The file is a memory file (TestFile) whose virtuals Read, Write, Seek and GetPos record. The streamer's fix-up table calls write
// and read one marker word. The event's GetType answers the type the case gave.
//
// Slots are named by number: 0 NULL, 1 + 100 * block + index for the pool block `block` (the order of the pool allocations of the
// whole run; a standalone block of the CMemBlock cases counts) and 0xFFFFFFFF for any other pointer.
// Labels: 1 the list, 2 the category container, 3 the sound data, 4 the event, 5 the file, 7 the streamer, 9 and 10 the position and
// velocity arguments, 31.. the standalone blocks.
//
// tpw_apl_run runs a script of operations (8 words each, see the K_ enum shared with the Python file) and appends the answers, and
// on K_SNAP the state of the list, to the output words.
#define TPW_CALLLOG_EVENTS 16000
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "LbTime.h"
#include "PlaceHolderTypes.h"
#include "TbMapStreamer.h"
#include "bfl_FileSystem.h"
#include "calllog.h"

using namespace calllog;
using bfl::FileSystem::CFile;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr std::size_t kSlot = sizeof(CAudioPlaceHolder);
constexpr std::size_t kHeader = (sizeof(CAudioPlaceHolderList::CMemBlock) + alignof(CAudioPlaceHolder) - 1) / alignof(CAudioPlaceHolder) *
                                alignof(CAudioPlaceHolder);
constexpr int kMaxBlocks = 64;

bool gTrack;        // events are recorded only while the code under test runs
bool gInCtor;       // an array made now is the constructor's priority array
bool gFailNext;     // the next pool block fails (the allocation throws; the nothrow form answers NULL)
char* gBlocks[kMaxBlocks];   // the pool blocks (arrays made outside the constructor) in allocation order
int gBlockCount;
void* gOtherArrays[8];       // the priority arrays
int gOtherCount;

// The runtime's new: blocks are filled with 0xA5; arrays are recorded (kind 1 a pool block with its number and slot count, kind 2
// the priority array with its word count) and a failing block throws after recording.
void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!array || !on() || !gTrack) return;
    if (gInCtor) {
        if (gOtherCount < 8) gOtherArrays[gOtherCount++] = block;
        ev("new[]", {2, 0, static_cast<int64_t>(size / sizeof(void*))});
        return;
    }
    if (gFailNext) {
        gFailNext = false;
        ev("new[]", {1, -1, 0});
        throw std::bad_alloc();
    }
    const int index = gBlockCount;
    if (gBlockCount < kMaxBlocks) gBlocks[gBlockCount++] = static_cast<char*>(block);
    ev("new[]", {1, index, size >= kHeader ? static_cast<int64_t>((size - kHeader) / kSlot) : 0});
}

// Blocks the runtime frees are kept (no address is reused while a stale pointer to it is compared); the arrays record.
bool Released(void* p, bool array) {
    if (array && p != nullptr && on() && gTrack) {
        for (int i = 0; i < gBlockCount; ++i) {
            if (gBlocks[i] == p) {
                ev("delete[]", {1, i});
                return true;
            }
        }
        for (int i = 0; i < gOtherCount; ++i) {
            if (gOtherArrays[i] == p) {
                ev("delete[]", {2, 0});
                return true;
            }
        }
    }
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) Allocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) Released(p, array)
#include "calllog.cpp"

// The nothrow forms are the library's own in the C++ runtime, which would bypass the observer above: the same two through allocate.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return allocate(size, true);
    } catch (...) {
        return nullptr;
    }
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    release(p, true);
}

namespace {

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(uint32_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(value));
}

CAudioPlaceHolderList* gList;   // the list under test (NULL before K_NEW and after a deleting K_DELETE)

// The slot's number: 0 for NULL, 1 + 100 * block + index in the pool, 0xFFFFFFFF for anything else.
uint32_t SlotId(const void* pointer) {
    if (pointer == nullptr) return 0;
    const char* p = static_cast<const char*>(pointer);
    for (int b = 0; b < gBlockCount; ++b) {
        const char* data = gBlocks[b] + kHeader;
        if (p >= data && p < data + 100 * kSlot && static_cast<std::size_t>(p - data) % kSlot == 0) {
            return 1 + 100 * b + static_cast<uint32_t>(static_cast<std::size_t>(p - data) / kSlot);
        }
    }
    return 0xFFFFFFFFu;
}

// The nesting count of the sound lock (the list's critical section word).
int64_t Depth() {
    return gList != nullptr ? static_cast<int64_t>(gList->mCriticalSection.mValue) : -1;
}

// ---- the memory file -------------------------------------------------------------------------------------------------------

constexpr uint32_t kFileCap = 1 << 16;
uint8_t gFileBytes[kFileCap];
uint32_t gFileLen;
uint32_t gFilePos;

uint64_t FirstBytes(const void* data, uint32_t size) {
    uint64_t value = 0;
    std::memcpy(&value, data, size < 8 ? size : 8);
    return value;
}

class TestFile : public CFile {
public:
    int32_t Read(void* dest, uint32_t size) override {
        uint8_t* bytes = static_cast<uint8_t*>(dest);
        for (uint32_t i = 0; i < size; ++i) bytes[i] = gFilePos + i < gFileLen ? gFileBytes[gFilePos + i] : 0;
        ev("File.Read", {lab(this), size, static_cast<int64_t>(FirstBytes(dest, size)), Depth()});
        gFilePos += size;
        return static_cast<int32_t>(size);
    }
    int32_t Write(const void* source, uint32_t size) override {
        ev("File.Write", {lab(this), size, static_cast<int64_t>(FirstBytes(source, size)), Depth()});
        const uint8_t* bytes = static_cast<const uint8_t*>(source);
        for (uint32_t i = 0; i < size && gFilePos + i < kFileCap; ++i) {
            if (gFilePos + i >= gFileLen) {
                for (uint32_t j = gFileLen; j < gFilePos + i; ++j) gFileBytes[j] = 0;
                gFileLen = gFilePos + i + 1;
            }
            gFileBytes[gFilePos + i] = bytes[i];
        }
        gFilePos += size;
        return static_cast<int32_t>(size);
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("File.Seek", {lab(this), offset, whence, Depth()});
        if (whence == 0) {
            gFilePos = offset;
        } else if (whence == 1) {
            gFilePos += offset;
        } else {
            gFilePos = gFileLen + offset;
        }
        return 0;
    }
    int32_t GetPos() override {
        ev("File.GetPos", {lab(this), gFilePos, Depth()});
        return static_cast<int32_t>(gFilePos);
    }
};
TestFile gFile;

// ---- the event ---------------------------------------------------------------------------------------------------------------

// The sound library's event: GetType answers the type the case gave; the other two slots are not called by the list.
class TestEvent : public sound::TbAudioEvent {
public:
    uint32_t GetType() const override {
        ev("Event.GetType", {lab(this), Depth()});
        return mType;
    }
    uint32_t GetCategory() const override { return 0; }
    uint32_t GetEvent() const override { return 0; }
    uint32_t mType = 0;
};
TestEvent gEvent;

}  // namespace

// ---- the base class (w79g2): definitions the list's code and the test subclasses need ----------------------------------------

TbSndCallback::TbSndCallback() {}
void TbSndCallback::SoundCallback(void*, void*) {}
void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}

CAudioPlaceHolderExtraData::CAudioPlaceHolderExtraData() {
    mBounds.left = 0;
    mBounds.top = 0;
    mBounds.right = 0;
    mBounds.bottom = 0;
    mChild = nullptr;
    mParamId[0] = 0;
    std::memset(mParamValue, 0, 4);
}

// As the real constructor: the list's words (handle +0x38, links +0x40 and +0x44) and +0x52 .. +0x5a are left as they were.
CAudioPlaceHolder::CAudioPlaceHolder() : TbSndCallback(), mEvent(nullptr), mSound(nullptr), mSample(nullptr), mPosition(), mVelocity() {
    mEndTime = 0;
    mHandle = nullptr;
    mCatagory = nullptr;
    mFlags = 0;
    mPauseTime = 0;
    mMaxDistance = 0.0f;
    mLastSound = -1;
    mLastSample = -1;
    mExtra = nullptr;
}

CAudioPlaceHolder::~CAudioPlaceHolder() {
    mListHandle = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mListHandle) & 0xffff);
    if (mExtra != nullptr) delete mExtra;
}

void CAudioPlaceHolder::CompletedCallback(void*, void*, int32_t) {}
void CAudioPlaceHolder::Release() { this->~CAudioPlaceHolder(); }
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

// Copies of the translation (the PowerPC side runs the originals at 0x1001ccd0, 0x1001a7b8 and 0x10017e9c); GetHandle (0x1001c9b0)
// records the sound lock's nesting count (which is how the test sees that FindPlaceHolder takes the lock) and the slot's links.
void* CAudioPlaceHolder::GetHandle() const {
    ev("P.GetHandle", {SlotId(this), Depth(), SlotId(mNext), SlotId(mPrevious)});
    return mListHandle;
}
void* CAudioPlaceHolder::SetHandle(void* handle) {
    mListHandle = handle;
    return handle;
}
bool CAudioPlaceHolder::FinishedPlaying(uint32_t now) {
    if ((mFlags & kLooping) == 0 && now > mEndTime + mPauseTime) {
        mFlags &= ~kPlaying;
        return true;
    }
    return false;
}
CAudioPlaceHolder::CDistPriority::operator uint32_t() const {
    return (static_cast<uint32_t>(mDistance) << 16) | mBasePriority;
}

// The heavy members: recording fakes (the PowerPC side stubs the same addresses).
bool CAudioPlaceHolder::VolumeControlIsOn() {
    ev("P.VolumeControlIsOn", {SlotId(this), Depth()});
    return ret("VC", 1) != 0;
}

bool CAudioPlaceHolder::ProcessPositionAndPriority() {
    ev("P.ProcessPositionAndPriority", {SlotId(this), Depth()});
    return ret("PPP", 1) != 0;
}

void CAudioPlaceHolder::AllocateExtraData() {
    ev("P.AllocateExtraData", {SlotId(this), Depth()});
    if (mExtra == nullptr) mExtra = new CAudioPlaceHolderExtraData();
}

TbError CAudioPlaceHolder::Save(void* file, uint32_t now) {
    ev("P.Save", {SlotId(this), lab(file), now, Depth()});
    CFile* out = static_cast<CFile*>(file);
    const uint32_t classId = GetClassID();
    out->Write(&classId, 4);
    out->Write(&mFlags, 4);
    return TbError(0);
}

TbError CAudioPlaceHolder::Load(void* file, uint32_t now) {
    ev("P.Load", {SlotId(this), lab(file), now, Depth()});
    uint32_t flags = 0;
    static_cast<CFile*>(file)->Read(&flags, 4);
    mFlags = flags;
    return TbError(0);
}

// ---- the subclasses (w79g3): TestHolder<N> and the CreateObject statics -------------------------------------------------------

namespace {

// A subclass of the pool: no data members (it is constructed into a slot), the virtuals the list calls record. The class ids are
// the ones of CAudioPlaceHolderList::Load: 1 branching sentence, 2 linear sentence, 3 one-shot, 4 one-shot sentence, 5 sentence,
// 6 one-shot branching sentence; 7 list sentence and 8 shuffled sentence are the test's (Load makes neither).
template <int ID>
class TestHolder : public CAudioPlaceHolder {
public:
    ~TestHolder() override { ev("V.Release", {SlotId(this), Depth(), SlotId(mNext), SlotId(mPrevious)}); }
    TbError PlaySample(uint32_t now) override {
        ev("V.PlaySample", {SlotId(this), now, Depth()});
        return TbError(static_cast<int32_t>(ret("Play", 0)));
    }
    TbError Stop() override {
        ev("V.Stop", {SlotId(this), Depth()});
        return TbError(static_cast<int32_t>(ret("Stop", 0)));
    }
    TbError Update(const TbSoundPosition& position, const TbSoundPosition& velocity) override {
        ev("V.Update", {SlotId(this), lab(&position), lab(&velocity), Depth()});
        return TbError(static_cast<int32_t>(ret("Update", 0)));
    }
    void UpdateParameter(uint8_t id, uint32_t value) override { ev("V.UpdateParameter", {SlotId(this), id, value, Depth()}); }
    uint32_t GetClassID() override { return ID; }
};

// The slots that have been constructed (the snapshot looks at the extra data of those only: a slot that NewPlaceHolderMemory or
// MoveToUsedItemList moved to the used list without a CreateObject holds the 0xA5 filler).
bool gBuilt[kMaxBlocks * 100];

// The constructor of subclass `cls` into the slot: the class's vtable and the base constructor's writes.
void Construct(CAudioPlaceHolder* slot, int cls) {
    const uint32_t id = SlotId(slot);
    if (id != 0 && id != 0xFFFFFFFFu) gBuilt[id - 1] = true;
    switch (cls) {
        case 1: new (slot) TestHolder<1>; break;
        case 2: new (slot) TestHolder<2>; break;
        case 3: new (slot) TestHolder<3>; break;
        case 4: new (slot) TestHolder<4>; break;
        case 5: new (slot) TestHolder<5>; break;
        case 6: new (slot) TestHolder<6>; break;
        case 7: new (slot) TestHolder<7>; break;
        default: new (slot) TestHolder<8>; break;
    }
}

void Create4(const char* name, int cls, CCatagoryContainer* container, CAudioPlaceHolder* slot, void* sound, const sound::TbAudioEvent& event) {
    ev(name, {SlotId(slot), lab(container), lab(sound), lab(&event), Depth()});
    Construct(slot, cls);
}

void Create1(const char* name, int cls, CAudioPlaceHolder* slot) {
    ev(name, {SlotId(slot), Depth()});
    Construct(slot, cls);
}

}  // namespace

void CListSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.ListSentence", 7, c, s, sound, e);
}
void CPlaceHolderSentenceShuffle::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.SentenceShuffle", 8, c, s, sound, e);
}
void CPlaceHolderBranchingSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.BranchingSentence", 1, c, s, sound, e);
}
void CPlaceHolderLinearSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.LinearSentence", 2, c, s, sound, e);
}
void CPlaceHolderOneShot::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.OneShot", 3, c, s, sound, e);
}
void CPlaceHolderOneShotSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.OneShotSentence", 4, c, s, sound, e);
}
void CPlaceHolderSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.Sentence", 5, c, s, sound, e);
}
void CPlaceHolderOneShotBranchingSentence::CreateObject(CCatagoryContainer* c, CAudioPlaceHolder* s, void* sound, const sound::TbAudioEvent& e) {
    Create4("Create.OneShotBranchingSentence", 6, c, s, sound, e);
}
void CPlaceHolderBranchingSentence::CreateObject(CAudioPlaceHolder* s) { Create1("Load.BranchingSentence", 1, s); }
void CPlaceHolderLinearSentence::CreateObject(CAudioPlaceHolder* s) { Create1("Load.LinearSentence", 2, s); }
void CPlaceHolderOneShot::CreateObject(CAudioPlaceHolder* s) { Create1("Load.OneShot", 3, s); }
void CPlaceHolderOneShotSentence::CreateObject(CAudioPlaceHolder* s) { Create1("Load.OneShotSentence", 4, s); }
void CPlaceHolderSentence::CreateObject(CAudioPlaceHolder* s) { Create1("Load.Sentence", 5, s); }
void CPlaceHolderOneShotBranchingSentence::CreateObject(CAudioPlaceHolder* s) { Create1("Load.OneShotBranchingSentence", 6, s); }

// ---- the streamer, the clock and the file base -------------------------------------------------------------------------------

namespace {
alignas(16) unsigned char gStreamerBytes[sizeof(TbMapStreamer)];
constexpr uint32_t kFixupMarker = 0xFEEDF00Du;
}  // namespace

TbMapStreamer* TbMapStreamer::Instance() {
    ev("Streamer.Instance", {});
    return reinterpret_cast<TbMapStreamer*>(gStreamerBytes);
}

TbError TbMapStreamer::SaveCatgoryFixupTable(CFile* file) {
    ev("Streamer.SaveFixup", {lab(this), lab(file), Depth()});
    file->Write(&kFixupMarker, 4);
    return TbError(0);
}

TbError TbMapStreamer::LoadCatgoryFixupTable(CFile* file) {
    ev("Streamer.LoadFixup", {lab(this), lab(file), Depth()});
    uint32_t marker = 0;
    file->Read(&marker, 4);
    return TbError(0);
}

void TbMapStreamer::UnloadCatgoryFixupTable() {
    ev("Streamer.UnloadFixup", {lab(this), Depth()});
}

uint32_t LbTime_GetClock() {
    ev("Clock", {});
    return static_cast<uint32_t>(ret("Clock", 1000));
}

namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace bfl::FileSystem

// ---- the script runner -------------------------------------------------------------------------------------------------------

namespace {

// The operations (shared with tests/test_AudioPlaceHolderList.py). Eight words each: the code and seven arguments. A slot argument is
// a selector (kind << 24 | k): 1 the absolute number k, 2 the k-th free, 3 the k-th used, 4 the k-th used from the end; an operation
// whose slot does not resolve is skipped (0xDEAD). A handle argument is a pair (kind, value): 0 the value, 1 the handle of the k-th
// used, 2 of the k-th free, 3 the k-th used's with the generation's low bit flipped, 4 plus one, 5 its low 16 bits, 6 plus 0x10000;
// an unresolved one is 0.
enum Op {
    K_NEW = 0, K_DELETE, K_SNAP, K_SETSLOT, K_CONSTRUCT, K_POKE, K_FAILNEXT, K_SETCH, K_GETCH, K_PROCESS, K_REMOVE, K_SETBOUND,
    K_GETPH, K_STOPALL, K_UPDATE, K_UPDPARAM, K_CREATE, K_FIND, K_NEWMEM, K_MOVEUSED, K_FINDFREE, K_CREATEHANDLE, K_NEWHANDLEINDEX,
    K_ADDBLOCK, K_ADDFREE, K_ADDUSED, K_ADDEND, K_MOVEFREE, K_HANDLE2PTR, K_INITPRIO, K_ADDHI, K_SAVE, K_LOAD, K_FILEPOS,
    K_BLOCKNEW, K_BLOCKCREATE, K_BLOCKGETNEXT, K_BLOCKSETNEXT, K_BLOCKDATA, K_BLOCKINFO, K_BLOCKDEL, K_UNLINK
};
constexpr int kOpWords = 8;

CAudioPlaceHolder* Nth(CAudioPlaceHolder* head, uint32_t k) {
    uint32_t guard = 0;
    for (CAudioPlaceHolder* p = head; p != nullptr && guard < 100000; p = p->mNext, ++guard) {
        if (k-- == 0) return p;
    }
    return nullptr;
}

CAudioPlaceHolder* NthFromEnd(CAudioPlaceHolder* head, uint32_t k) {
    CAudioPlaceHolder* tail = head;
    uint32_t guard = 0;
    while (tail != nullptr && tail->mNext != nullptr && guard++ < 100000) tail = tail->mNext;
    for (; tail != nullptr && k > 0; --k) tail = tail->mPrevious;
    return tail;
}

bool ResolveSlot(uint32_t selector, CAudioPlaceHolder*& slot) {
    const uint32_t kind = selector >> 24;
    const uint32_t k = selector & 0xFFFFFF;
    slot = nullptr;
    if (gList == nullptr) return false;
    switch (kind) {
        case 1: {
            if (k == 0) return false;
            const uint32_t block = (k - 1) / 100;
            if (block >= static_cast<uint32_t>(gBlockCount)) return false;
            slot = reinterpret_cast<CAudioPlaceHolder*>(gBlocks[block] + kHeader + ((k - 1) % 100) * kSlot);
            return true;
        }
        case 2: slot = Nth(gList->mFree, k); break;
        case 3: slot = Nth(gList->mUsed, k); break;
        case 4: slot = NthFromEnd(gList->mUsed, k); break;
        default: break;
    }
    return slot != nullptr;
}

void* ResolveHandle(uint32_t kind, uint32_t value) {
    CAudioPlaceHolder* slot = nullptr;
    if (kind == 0) return Cookie(value);
    if (gList == nullptr) return nullptr;
    slot = kind == 2 ? Nth(gList->mFree, value) : Nth(gList->mUsed, value);
    if (slot == nullptr) return nullptr;
    const uint32_t handle = Low32(slot->mListHandle);
    switch (kind) {
        case 1: case 2: return Cookie(handle);
        case 3: return Cookie(handle ^ 0x10000u);
        case 4: return Cookie(handle + 1);
        case 5: return Cookie(handle & 0xffffu);
        case 6: return Cookie(handle + 0x10000u);
        default: return nullptr;
    }
}

struct Out {
    uint32_t* words;
    uint32_t count;
    uint32_t capacity;
    void put(uint32_t value) {
        if (count < capacity) words[count] = value;
        ++count;
    }
};

CAudioPlaceHolderList::CMemBlock* gBlockObjs[4];
TbSoundPosition gPosition(1.0f, 2.0f, 3.0f);
TbSoundPosition gVelocity(0.5f, 0.25f, 0.125f);
CEventData gSoundData;
alignas(16) unsigned char gContainer[16];

// The state of the list: the counters, the pool blocks, the free list, the used list and (when asked) the priority list.
void Snapshot(Out& out, bool priority) {
    out.put(0xB000);
    if (gList == nullptr) {
        out.put(0xDEAD);
        return;
    }
    out.put(gList->mBlockCount);
    out.put(gList->mNextSlotIndex);
    out.put(gList->mHandleGeneration);
    out.put(gList->mLowestPriority);
    out.put(gList->mNumberOfChannels);
    out.put(gList->mCriticalSection.mValue);
    uint32_t blocks = 0;
    for (auto* b = gList->mBlocks; b != nullptr && blocks < 64; b = b->mNext) ++blocks;
    out.put(blocks);
    uint32_t guard = 0;
    for (auto* b = gList->mBlocks; b != nullptr && guard++ < 64; b = b->mNext) {
        uint32_t id = 0xFFFFFFFFu;
        for (int i = 0; i < gBlockCount; ++i) {
            if (reinterpret_cast<char*>(b) == gBlocks[i]) id = static_cast<uint32_t>(i);
        }
        out.put(id);
        out.put(static_cast<uint32_t>(b->mSize / kSlot));
    }
    uint32_t n = 0;
    for (CAudioPlaceHolder* p = gList->mFree; p != nullptr && n < 2000; p = p->mNext) ++n;
    out.put(n);
    n = 0;
    for (CAudioPlaceHolder* p = gList->mFree; p != nullptr && n++ < 2000; p = p->mNext) {
        out.put(SlotId(p));
        out.put(Low32(p->mListHandle));
        out.put(SlotId(p->mPrevious));
        out.put(p->mFlags);
    }
    n = 0;
    for (CAudioPlaceHolder* p = gList->mUsed; p != nullptr && n < 2000; p = p->mNext) ++n;
    out.put(n);
    n = 0;
    for (CAudioPlaceHolder* p = gList->mUsed; p != nullptr && n++ < 2000; p = p->mNext) {
        out.put(SlotId(p));
        out.put(Low32(p->mListHandle));
        out.put(SlotId(p->mPrevious));
        out.put(p->mFlags);
        out.put(Low32(p->mHandle));
        out.put(static_cast<uint32_t>(p->mPriority));
        out.put(p->mEndTime);
        out.put(p->mPauseTime);
        out.put(Low32(p->mSample));
        const uint32_t id = SlotId(p);
        const bool built = id != 0xFFFFFFFFu && gBuilt[id - 1];
        out.put(built && p->mExtra != nullptr ? 1 : 0);
        if (built && p->mExtra != nullptr) {
            out.put(static_cast<uint32_t>(p->mExtra->mBounds.left));
            out.put(static_cast<uint32_t>(p->mExtra->mBounds.top));
            out.put(static_cast<uint32_t>(p->mExtra->mBounds.right));
            out.put(static_cast<uint32_t>(p->mExtra->mBounds.bottom));
        }
    }
    if (priority) {
        CAudioPlaceHolder** list = gList->mPriorityList;
        for (int i = 0; i < 31; ++i) out.put(SlotId(list[i]));
    }
}

}  // namespace

extern "C" {

// ops: nops * 8 words; the file's initial bytes; out: the answers.
void tpw_apl_run(const uint32_t* ops, uint32_t nops, const uint8_t* filein, uint32_t filelen, uint32_t* out_words, uint32_t* out_count,
                 uint32_t out_capacity) {
    Out out{out_words, 0, out_capacity};
    gList = nullptr;
    gBlockCount = 0;
    gOtherCount = 0;
    std::memset(gBuilt, 0, sizeof(gBuilt));
    gFailNext = false;
    gInCtor = false;
    for (auto& b : gBlockObjs) b = nullptr;
    gFileLen = filelen < kFileCap ? filelen : kFileCap;
    std::memcpy(gFileBytes, filein, gFileLen);
    gFilePos = 0;
    tpw_cl_label(5, &gFile);
    tpw_cl_label(7, gStreamerBytes);
    tpw_cl_label(9, &gPosition);
    tpw_cl_label(10, &gVelocity);
    tpw_cl_label(2, gContainer);
    tpw_cl_label(4, &gEvent);
    gTrack = true;
    for (uint32_t index = 0; index < nops; ++index) {
        const uint32_t* op = ops + index * kOpWords;
        const uint32_t a = op[1], b = op[2], c = op[3], d = op[4], e = op[5], f = op[6];
        out.put(0xA000 + index);
        CAudioPlaceHolder* slot = nullptr;
        switch (op[0]) {
            case K_NEW:
                gInCtor = true;
                gList = new CAudioPlaceHolderList(a);
                gInCtor = false;
                tpw_cl_label(1, gList);
                break;
            case K_DELETE:
                if (gList == nullptr) {
                    out.put(0xDEAD);
                    break;
                }
                if (static_cast<int16_t>(a) > 0) {
                    delete gList;
                } else {
                    gList->~CAudioPlaceHolderList();
                }
                gList = nullptr;
                break;
            case K_SNAP:
                Snapshot(out, a != 0);
                break;
            case K_SETSLOT:
                if (!ResolveSlot(a, slot)) {
                    out.put(0xDEAD);
                    break;
                }
                switch (b) {
                    case 0: slot->mFlags = c; break;
                    case 1: slot->mHandle = Cookie(c); break;
                    case 2:
                        slot->mPriority.mDistance = static_cast<uint16_t>(c >> 16);
                        slot->mPriority.mBasePriority = static_cast<uint16_t>(c);
                        break;
                    case 3: slot->mSample = reinterpret_cast<CSampleData*>(static_cast<uintptr_t>(c)); break;
                    case 4: slot->mEndTime = c; break;
                    case 5: slot->mPauseTime = c; break;
                    default: slot->mListHandle = Cookie(c); break;
                }
                break;
            case K_CONSTRUCT:
                if (!ResolveSlot(a, slot)) {
                    out.put(0xDEAD);
                    break;
                }
                Construct(slot, static_cast<int>(b));
                break;
            case K_POKE:
                if (gList == nullptr) {
                    out.put(0xDEAD);
                    break;
                }
                switch (a) {
                    case 0: gList->mHandleGeneration = b; break;
                    case 1: gList->mNextSlotIndex = b; break;
                    case 2: gList->mLowestPriority = b; break;
                    default: gList->mBlockCount = b; break;
                }
                break;
            case K_FAILNEXT:
                gFailNext = true;
                break;
            case K_SETCH:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                gList->SetNumberOfChannels(a);
                break;
            case K_GETCH:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(gList->GetNumberOfChannels());
                break;
            case K_PROCESS:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                gList->ProcessPlaceHolders();
                break;
            case K_REMOVE:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->RemovePlaceHolder(slot);
                break;
            case K_SETBOUND: {
                if (gList == nullptr) { out.put(0xDEAD); break; }
                TbRect rect;
                rect.left = static_cast<int32_t>(c);
                rect.top = static_cast<int32_t>(d);
                rect.right = static_cast<int32_t>(e);
                rect.bottom = static_cast<int32_t>(f);
                out.put(static_cast<uint32_t>(static_cast<int32_t>(gList->SetBoundingRectangle(ResolveHandle(a, b), rect))));
                break;
            }
            case K_GETPH: {
                if (gList == nullptr) { out.put(0xDEAD); break; }
                CAudioPlaceHolder* holder = reinterpret_cast<CAudioPlaceHolder*>(static_cast<uintptr_t>(0xA5A5A5A5u));
                out.put(Low32(gList->GetPlaceholderAndSoundHandle(ResolveHandle(a, b), holder)));
                out.put(SlotId(holder));
                break;
            }
            case K_STOPALL:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                gList->StopAllPlaceHolders(a);
                break;
            case K_UPDATE:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(gList->UpdatePlaceHolder(ResolveHandle(a, b), gPosition, gVelocity));
                break;
            case K_UPDPARAM:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(gList->UpdatePlaceHolderParameter(ResolveHandle(a, b), static_cast<uint8_t>(c), d));
                break;
            case K_CREATE: {
                if (gList == nullptr) { out.put(0xDEAD); break; }
                gSoundData.mFlags = static_cast<uint16_t>(b);
                tpw_cl_label(3, &gSoundData);
                gEvent.mType = c;
                out.put(Low32(gList->CreatePlaceHolder(a != 0 ? reinterpret_cast<CCatagoryContainer*>(gContainer) : nullptr, &gSoundData, gEvent)));
                break;
            }
            case K_FIND:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(SlotId(gList->FindPlaceHolder(ResolveHandle(a, b))));
                break;
            case K_NEWMEM: {
                if (gList == nullptr) { out.put(0xDEAD); break; }
                void* handle = Cookie(0xDEADBEEFu);
                slot = gList->NewPlaceHolderMemory(handle);
                out.put(SlotId(slot));
                out.put(Low32(handle));
                break;
            }
            case K_MOVEUSED:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->MoveToUsedItemList(slot);
                break;
            case K_FINDFREE:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(SlotId(gList->FindFreeItem(Cookie(a))));
                break;
            case K_CREATEHANDLE:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                out.put(Low32(gList->CreateHandle(slot)));
                break;
            case K_NEWHANDLEINDEX:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(gList->NewHandleIndex());
                break;
            case K_ADDBLOCK:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(static_cast<uint32_t>(static_cast<int32_t>(gList->AddNewBlock())));
                break;
            case K_ADDFREE:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->AddToFreeItemList(slot);
                break;
            case K_ADDUSED:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->AddToUsedItemList(slot);
                break;
            case K_ADDEND:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->AddToEndOfUsedItemList(slot);
                break;
            case K_MOVEFREE:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->MoveToFreeItemList(slot);
                break;
            case K_HANDLE2PTR:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(SlotId(gList->Handle2Ptr(ResolveHandle(a, b))));
                break;
            case K_INITPRIO:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                gList->InitialisePriorityList();
                break;
            case K_ADDHI:
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                gList->AddToHiPriorityList(slot);
                break;
            case K_SAVE:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(static_cast<uint32_t>(static_cast<int32_t>(gList->Save(&gFile))));
                break;
            case K_LOAD:
                if (gList == nullptr) { out.put(0xDEAD); break; }
                out.put(static_cast<uint32_t>(static_cast<int32_t>(gList->Load(&gFile, a != 0))));
                break;
            case K_FILEPOS:
                gFilePos = a;
                break;
            case K_BLOCKNEW:
                gBlockObjs[a & 3] = new CAudioPlaceHolderList::CMemBlock(b);
                tpw_cl_label(31 + static_cast<int>(a & 3), gBlockObjs[a & 3]);
                break;
            case K_BLOCKCREATE: {
                CAudioPlaceHolderList::CMemBlock* made = reinterpret_cast<CAudioPlaceHolderList::CMemBlock*>(static_cast<uintptr_t>(0xA5A5A5A5u));
                const TbError error = CAudioPlaceHolderList::CMemBlock::CreateObject(c != 0 ? nullptr : &made, b);
                out.put(static_cast<uint32_t>(static_cast<int32_t>(error)));
                if (c == 0 && static_cast<int32_t>(error) >= 0) {
                    gBlockObjs[a & 3] = made;
                    tpw_cl_label(31 + static_cast<int>(a & 3), made);
                }
                break;
            }
            case K_BLOCKGETNEXT:
                if (gBlockObjs[a & 3] == nullptr) { out.put(0xDEAD); break; }
                out.put(static_cast<uint32_t>(lab(gBlockObjs[a & 3]->GetNext())));
                break;
            case K_BLOCKSETNEXT:
                if (gBlockObjs[a & 3] == nullptr) { out.put(0xDEAD); break; }
                gBlockObjs[a & 3]->SetNext(b == 0 ? nullptr : gBlockObjs[(b - 1) & 3]);
                break;
            case K_BLOCKDATA:
                if (gBlockObjs[a & 3] == nullptr) { out.put(0xDEAD); break; }
                out.put(static_cast<char*>(gBlockObjs[a & 3]->Data()) - reinterpret_cast<char*>(gBlockObjs[a & 3]) == static_cast<std::ptrdiff_t>(kHeader) ? 1 : 0);
                break;
            case K_BLOCKINFO:
                if (gBlockObjs[a & 3] == nullptr) { out.put(0xDEAD); break; }
                out.put(b != 0 ? static_cast<uint32_t>(gBlockObjs[a & 3]->mSize / kSlot) : gBlockObjs[a & 3]->mSize);
                out.put(static_cast<uint32_t>(lab(gBlockObjs[a & 3]->mNext)));
                break;
            case K_BLOCKDEL:
                if (gBlockObjs[a & 3] == nullptr) { out.put(0xDEAD); break; }
                if (static_cast<int16_t>(b) > 0) {
                    delete gBlockObjs[a & 3];
                } else {
                    gBlockObjs[a & 3]->~CMemBlock();
                }
                gBlockObjs[a & 3] = nullptr;
                break;
            case K_UNLINK: {
                // test only: the slot leaves its list (b bit 0: 0 the free list, 1 the used list) with its links cleared (bit 1: left
                // as they were), so that an Add... function can be given a slot that is on no list
                if (!ResolveSlot(a, slot)) { out.put(0xDEAD); break; }
                CAudioPlaceHolder*& head = (b & 1) == 0 ? gList->mFree : gList->mUsed;
                if (slot->mPrevious != nullptr) {
                    slot->mPrevious->mNext = slot->mNext;
                } else {
                    head = slot->mNext;
                }
                if (slot->mNext != nullptr) slot->mNext->mPrevious = slot->mPrevious;
                if ((b & 2) == 0) {
                    slot->mNext = nullptr;
                    slot->mPrevious = nullptr;
                }
                break;
            }
            default:
                break;
        }
    }
    gTrack = false;
    tpw_cl_on = 0;
    out.put(0xF000);
    out.put(gFileLen);
    for (uint32_t i = 0; i < gFileLen; i += 4) {
        uint32_t word = 0;
        std::memcpy(&word, gFileBytes + i, gFileLen - i < 4 ? gFileLen - i : 4);
        out.put(word);
    }
    *out_count = out.count;
}

}  // extern "C"
