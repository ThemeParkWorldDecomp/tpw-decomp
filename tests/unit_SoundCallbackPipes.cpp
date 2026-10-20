// Fakes and test entry points for unit w79i, the sound library's sample callback pipes (libtpw_unit_SoundCallbackPipes.dylib,
// driven by tests/test_SoundCallbackPipes.py in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundSampleCallback.cpp   TbSoundUserCallback, _TbSoundSampleCallback, _TbSoundSampleCompletedCallback, CAutoDetach,
//                                   their pools and the three cleanup objects
// Real: those, unit w78's pipe base (src/TbSoundPipe.cpp), unit w77's sample info (src/TbSoundInfo.cpp), unit w80's timer codec
// (src/TbSoundCodecs.cpp) and TbRefRelease. Fakes, each recording what the original's callee would be given (the PowerPC side
// records the same from its stubs):
//   FakeNode, a TbSoundSampleCodec that overrides every virtual slot and records it: the pipe behind a callback pipe. Its
//       GetDataChunk answers the queue "Step" (the result in the high 32 bits, the bytes delivered in the low 32 bits, 0x7fffffff
//       meaning "all that was asked"; the last value repeats, so a script ends it with the default 0x7fffffff) and refuses to answer
//       1 for a request over 0x1000 bytes or a negative one, and delivers 0 to all that was asked when it does answer 1 (the timer
//       codec that follows a finished sample would write that many)
//   FakeCallback, the TbSndCallback of a pipe (two of them, labels 7 and 8): SoundCallback records its arguments and the caller's
//       mNext (which GetNextSample cleared first), then attaches the FakeNode of the slot the queue "Attach" names (-1: none) behind
//       the caller (the Sample pipe with a NULL mNext and the handle it was given), as a placeholder's override does
//   the runtime heap (operator new and delete: calllog.cpp records "new" with the Mac's size and a label through the observer
//       below, "delete" for a labelled block), the clock LbTime_GetClock (records "Clock", answers the queue), and the open file
//       class the timer's translation unit names (never called)
//
// Sizes: the host objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are mapped:
// a class's host size is its Mac size (0x1c the user callback, 0x24 the sample and the completed callback, 0x30 the timer, 0x14 the
// pipe, 0x2c the FakeNode) and any other size the test asks for is carried as the Mac size + 0x1000 on the host.
//
// Labels: 1 and 2 reserved, 3 the sample info, 7 and 8 the callbacks, 100 and up the blocks the runtime made. A block the case
// allocates is filled with 0xA5 on both sides, so that the words a constructor leaves alone are compared too. The output buffer
// (gBuffer, 0x10000 bytes of 0xA5, the pointer the pipes are given in the middle of it) is not labelled: a pointer into it is
// recorded as its offset from that pointer.
//
// The case drives everything with tpw_cb_op and reads the state back with tpw_cb_slot, tpw_cb_pool, tpw_cb_block, tpw_cb_info
// and tpw_cb_buffer.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AudioPlaceHolder.h"
#include "LbTime.h"
#include "TbSoundCodecs.h"
#include "TbSoundSampleCallback.h"
#include "calllog.h"

using namespace calllog;
using bfl::FileSystem::CFile;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr int kMaxBlocks = 4096;
void* gAllocated[kMaxBlocks];   // every block the runtime handed out in the case (freed at the end when not deleted)
int gAllocatedCount;

constexpr std::size_t kLiteral = 0x1000;

enum PoolClass { kPoolUser = 0, kPoolSample = 1, kPoolCompleted = 2, kPoolTimer = 3, kPoolPipe = 4, kPools = 5 };
const int kMacSizes[kPools] = {0x1c, 0x24, 0x24, 0x30, 0x14};

class FakeNode;

// The Mac size of a host size (see the top of the file).
std::size_t MacSize(std::size_t size) {
    if (size == sizeof(TbSoundUserCallback)) return 0x1c;
    if (size == sizeof(_TbSoundSampleCallback)) return 0x24;
    if (size == sizeof(_TbSoundSampleCompletedCallback)) return 0x24;
    if (size == sizeof(TbTimerCodec)) return 0x30;
    if (size == sizeof(TbSoundSamplePipe)) return 0x14;
    if (size == sizeof(TbSoundSampleCodec)) return 0x2c;
    if (size >= kLiteral && size < 2 * kLiteral) return size - kLiteral;
    return size;
}

// The host size for a Mac size asked of the operations of one class.
std::size_t HostSize(int pool, int64_t macSize);

// The runtime's new: a "new" event of the Mac's size and the block's label, the block filled with 0xA5.
void SoundAllocated(std::size_t size, bool, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on()) return;
    if (gAllocatedCount < kMaxBlocks) gAllocated[gAllocatedCount++] = block;
    ev("new", {static_cast<int64_t>(MacSize(size)), newlab(block)});
}

// Blocks the runtime frees are kept until the end of the case (see tpw_cb_end), so that no address is reused while a stale
// pointer to it is compared: the PowerPC side's heap does not reuse addresses either.
bool KeepFreed(void*, bool) {
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) SoundAllocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

// The clock the sound library asks for (the import LbTime_GetClock__Fv): the queue "Clock".
uint32_t LbTime_GetClock() {
    ev("Clock");
    return static_cast<uint32_t>(ret("Clock", 0));
}

// The members of the file class that nothing here calls but the timer's translation unit needs (the library's own are imports).
#include "fake_file_slots.h"

CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) {
    return -1;
}
int32_t CFile::GetSize() {
    return -1;
}

// The callback base (include/AudioPlaceHolder.h; the placeholder's unit defines it): empty here, the fake overrides it.
TbSndCallback::TbSndCallback() {}
void TbSndCallback::SoundCallback(void*, void*) {}
void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}

namespace {

constexpr int kSlots = 6;
constexpr int kBufferBytes = 0x10000;
constexpr int kBufferBase = 0x8000;   // the pointer the pipes are given is gBuffer + kBufferBase + the op's offset
constexpr int64_t kOutside = -(int64_t(1) << 40);
constexpr uint32_t kFill = 0xA5A5A5A5u;
constexpr int64_t kFillStep = 0x7fffffff;   // result 0, all that was asked
enum Kind { kNone = 0, kUser = 1, kSample = 2, kCompleted = 3, kFake = 4, kPipe = 5, kTimer = 6 };

alignas(16) uint8_t gBuffer[kBufferBytes];
alignas(16) uint8_t gInfoBytes[sizeof(TbSoundSampleInfo) + 16];
TbSoundSampleInfo* Info() { return reinterpret_cast<TbSoundSampleInfo*>(gInfoBytes); }

// The offset of a pointer in the output buffer from the pointer the pipes are given, or kOutside.
int64_t BufferOffset(const void* pointer) {
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    if (p >= gBuffer && p < gBuffer + kBufferBytes) return (p - gBuffer) - kBufferBase;
    return kOutside;
}

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

// A pointer field as the test compares it: 0, the label of the object, the fill word, or -1.
int64_t PW(const void* pointer) {
    if (pointer == nullptr) return 0;
    const int32_t label = lab(pointer);
    if (label > 0) return label;
    if (Low32(pointer) == kFill) return kFill;
    return -1;
}

// 0 NULL, 1 the fill word, 2 anything else: what a pool link word of a node that is not idle holds.
int64_t LinkClass(const void* pointer) {
    if (pointer == nullptr) return 0;
    return Low32(pointer) == kFill ? 1 : 2;
}

// ---- the four pools of the unit and the timer's and the pipe's ---------------------------------------------------------------

struct PoolDesc {
    void** first;
    int32_t* current;
    int32_t* low;
    int32_t* high;
    int32_t* total;
    int32_t* level;
    void* (*link)(void*);
    std::size_t hostSize;
    void* (*opNew)(std::size_t);
    void (*opDelete)(void*, std::size_t);
    void (*topUp)();
    void (*setLevel)(int32_t);
    void (*clearCache)();
};

#define POOL_ENTRY(CLASS, FIRST, CUR, LOW, HIGH, TOTAL, LEVEL, LINKFIELD)                                              \
    {reinterpret_cast<void**>(&CLASS::FIRST), &CLASS::CUR, &CLASS::LOW, &CLASS::HIGH, &CLASS::TOTAL, &CLASS::LEVEL,    \
     [](void* object) -> void* { return static_cast<CLASS*>(object)->LINKFIELD; }, sizeof(CLASS),                     \
     &CLASS::operator new, &CLASS::operator delete, &CLASS::TopUp, &CLASS::SetLevel, &CLASS::ClearCache}

const PoolDesc kPoolTable[kPools] = {
    POOL_ENTRY(TbSoundUserCallback, mFirstTbSoundUserCallback, mCurrentTbSoundUserCallback, mLowTbSoundUserCallback,
               mHighTbSoundUserCallback, mTotalTbSoundUserCallback, mLevelTbSoundUserCallback, mUserCallbackPoolLink),
    POOL_ENTRY(_TbSoundSampleCallback, mFirst_TbSoundSampleCallback, mCurrent_TbSoundSampleCallback, mLow_TbSoundSampleCallback,
               mHigh_TbSoundSampleCallback, mTotal_TbSoundSampleCallback, mLevel_TbSoundSampleCallback, mSampleCallbackPoolLink),
    POOL_ENTRY(_TbSoundSampleCompletedCallback, mFirst_TbSoundSampleCompletedCallback, mCurrent_TbSoundSampleCompletedCallback,
               mLow_TbSoundSampleCompletedCallback, mHigh_TbSoundSampleCompletedCallback, mTotal_TbSoundSampleCompletedCallback,
               mLevel_TbSoundSampleCompletedCallback, mCompletedCallbackPoolLink),
    POOL_ENTRY(TbTimerCodec, mFirstTbTimerCodec, mCurrentTbTimerCodec, mLowTbTimerCodec, mHighTbTimerCodec, mTotalTbTimerCodec,
               mLevelTbTimerCodec, mTimerPoolLink),
    POOL_ENTRY(TbSoundSamplePipe, mFirstTbSoundSamplePipe, mCurrentTbSoundSamplePipe, mLowTbSoundSamplePipe,
               mHighTbSoundSamplePipe, mTotalTbSoundSamplePipe, mLevelTbSoundSamplePipe, mPoolLink),
};

std::size_t HostSize(int pool, int64_t macSize) {
    if (macSize == kMacSizes[pool]) return kPoolTable[pool].hostSize;
    if (macSize == 0) return 0;
    return static_cast<std::size_t>(macSize) + kLiteral;
}

bool PooledIn(int pool, const void* object) {
    int steps = 0;
    for (void* idle = *kPoolTable[pool].first; idle != nullptr && steps < 64; idle = kPoolTable[pool].link(idle), ++steps) {
        if (idle == object) return true;
    }
    return false;
}

bool PooledAny(const void* object) {
    for (int pool = 0; pool < kPools; ++pool) {
        if (PooledIn(pool, object)) return true;
    }
    return false;
}

// The pool a node of this kind belongs to, or -1 (the FakeNode is the runtime's).
int PoolOfKind(int kind) {
    switch (kind) {
        case kUser: return kPoolUser;
        case kSample: return kPoolSample;
        case kCompleted: return kPoolCompleted;
        case kTimer: return kPoolTimer;
        case kPipe: return kPoolPipe;
        default: return -1;
    }
}

// ---- the world -----------------------------------------------------------------------------------------------------------------

TbSoundSamplePipe* gNodes[kSlots];
int gKinds[kSlots];
void* gBlocks[8];
int64_t gBlockSizes[8];   // the Mac sizes the raw blocks were asked for
int gBlockPools[8];       // the class whose operator new made them
int32_t gDtorFlag = 1;    // the flag the destructor call carries (a `delete this` of the class code is flag 1)

TbSoundSamplePipe* NodeBySlot(int64_t slot) {
    return slot >= 0 && slot < kSlots ? gNodes[slot] : nullptr;
}

// A node exists, has its label and is not in its pool.
bool Alive(int slot) {
    if (gNodes[slot] == nullptr || lab(gNodes[slot]) <= 0) return false;
    const int pool = PoolOfKind(gKinds[slot]);
    return pool < 0 || !PooledIn(pool, gNodes[slot]);
}

// ---- the node that records every slot ------------------------------------------------------------------------------------------

// A queue value that names a node: -1 NULL, -2 the node's own mNext, otherwise a slot.
TbSoundSamplePipe* QueuedNode(const char* key, TbSoundSamplePipe* self) {
    const int64_t value = ret(key, -1);
    if (value == -2) return self->mNext;
    return NodeBySlot(value);
}

class FakeNode : public TbSoundSampleCodec {
public:
    FakeNode() : TbSoundSampleCodec() {}
    ~FakeNode() override { ev("Fake.~", {lab(this), gDtorFlag}); }

    TbSoundSamplePipe* Attatch(TbSoundSamplePipe* pipe) override {
        ev("Fake.Attatch", {lab(this), lab(pipe)});
        return nullptr;
    }
    void Release() override { ev("Fake.Release", {lab(this)}); }
    void Detatch() override { ev("Fake.Detatch", {lab(this)}); }
    TbSoundSamplePipe* FindPipe(void* handle) override {
        ev("Fake.FindPipe", {lab(this), static_cast<int64_t>(Low32(handle))});
        return QueuedNode("FindPipe", this);
    }
    void GetInfo(TbSoundSampleInfo& info) override { ev("Fake.GetInfo", {lab(this), lab(&info)}); }
    uint32_t GetSampleRate() override {
        ev("Fake.GetSampleRate", {lab(this)});
        return static_cast<uint32_t>(ret("Rate", 0));
    }
    // The pipe behind a callback pipe: the scripted result and bytes delivered (see the top of the file).
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override {
        ev("Fake.GetDataChunk", {lab(this), BufferOffset(buffer), size, flag, done});
        const int64_t step = ret("Step", kFillStep);
        int32_t result = static_cast<int32_t>(step >> 32);
        int32_t chunk = static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(step) & 0xFFFFFFFFu));
        if (chunk == 0x7fffffff) chunk = size;
        if (result == 1 && (size > 0x1000 || size < 0)) result = 0;
        if (result == 1) chunk = chunk < 0 ? 0 : (chunk > size ? size : chunk);
        done = chunk;
        return result;
    }
    void Restart() override { ev("Fake.Restart", {lab(this)}); }
    TbError SetInfo(const TbSoundSampleInfo& info) override {
        ev("Fake.SetInfo", {lab(this), lab(&info)});
        return TbError(static_cast<int32_t>(ret("SetInfoErr", 0)));
    }
    void* SetHandle(void* handle) override {
        ev("Fake.SetHandle", {lab(this), static_cast<int64_t>(Low32(handle))});
        return handle;
    }
    void Completed(bool done) override { ev("Fake.Completed", {lab(this), done}); }
    void SetDataStart(void* start) override { ev("Fake.SetDataStart", {lab(this), static_cast<int64_t>(Low32(start))}); }
    const uint8_t* GetDataStart() override {
        ev("Fake.GetDataStart", {lab(this)});
        return nullptr;
    }
    void SetDataPosition(uint32_t position) override { ev("Fake.SetDataPosition", {lab(this), position}); }
    uint32_t GetDataPosition() override {
        ev("Fake.GetDataPosition", {lab(this)});
        return 0;
    }
    uint32_t GetDataSize() override {
        ev("Fake.GetDataSize", {lab(this)});
        return 0;
    }
    void SetSampleRate(uint16_t rate) override { ev("Fake.SetSampleRate", {lab(this), rate}); }
    uint32_t GetBitRate() override {
        ev("Fake.GetBitRate", {lab(this)});
        return 0;
    }
    uint32_t GetChannels() override {
        ev("Fake.GetChannels", {lab(this)});
        return 0;
    }
    TbSoundSPCodec* GetCodec() override {
        ev("Fake.GetCodec", {lab(this)});
        return nullptr;
    }
    bool SetTime(bool stopped, uint32_t bytes) override {
        ev("Fake.SetTime", {lab(this), stopped, bytes});
        return false;
    }
};

static_assert(sizeof(FakeNode) == sizeof(TbSoundSampleCodec), "the fake is the size of a sample codec");
static_assert(sizeof(TbSoundUserCallback) != sizeof(TbSoundSamplePipe), "the user callback is not the size of a pipe");
static_assert(sizeof(_TbSoundSampleCallback) != sizeof(TbSoundUserCallback), "the sample callback is not the size of the user callback");
static_assert(sizeof(TbTimerCodec) != sizeof(_TbSoundSampleCallback), "the timer is not the size of a callback");

// What kind a node is: kNone for NULL, for a node that is dead (no label, or idle in a pool) and for anything unknown.
int TypeOf(const TbSoundSamplePipe* node) {
    if (node == nullptr || lab(node) <= 0 || PooledAny(node)) return kNone;
    if (dynamic_cast<const FakeNode*>(node) != nullptr) return kFake;
    if (dynamic_cast<const TbTimerCodec*>(node) != nullptr) return kTimer;
    if (dynamic_cast<const _TbSoundSampleCallback*>(node) != nullptr) return kSample;
    if (dynamic_cast<const _TbSoundSampleCompletedCallback*>(node) != nullptr) return kCompleted;
    if (dynamic_cast<const TbSoundUserCallback*>(node) != nullptr) return kUser;
    return kPipe;
}

// The callback of the sample pipe that called SoundCallback: the first alive sample pipe with nothing behind it and the handle.
TbSoundSamplePipe* CallerOf(void* handle) {
    for (int i = 0; i < kSlots; ++i) {
        if (gNodes[i] == nullptr || TypeOf(gNodes[i]) != kSample) continue;
        if (gNodes[i]->mNext == nullptr && gNodes[i]->mHandle == handle) return gNodes[i];
    }
    return nullptr;
}

// The TbSndCallback of a pipe. SoundCallback records the arguments and the caller's mNext (GetNextSample clears it first), then
// attaches the FakeNode the queue "Attach" names behind the caller, as the placeholder's override does with its new codec.
class FakeCallback : public TbSndCallback {
public:
    void SoundCallback(void* sample, void* userData) override {
        TbSoundSamplePipe* caller = CallerOf(sample);
        ev("Cb.Sound", {lab(this), static_cast<int64_t>(Low32(sample)), static_cast<int64_t>(Low32(userData)),
                        caller != nullptr ? PW(caller->mNext) : -7});
        TbSoundSamplePipe* node = NodeBySlot(ret("Attach", -1));
        if (caller != nullptr && node != nullptr && TypeOf(node) == kFake) {
            caller->mNext = node;
            node->mPrevious = caller;
        }
    }
    void CompletedCallback(void* sample, void* userData, int32_t reason) override {
        ev("Cb.Completed", {lab(this), static_cast<int64_t>(Low32(sample)), static_cast<int64_t>(Low32(userData)), reason});
    }
};

FakeCallback gCallbacks[2];

// A callback named by a code: 0 and 1 the fakes, 2 a pointer that is none of them, anything else NULL.
TbSndCallback* CallbackFor(int64_t code) {
    if (code == 0 || code == 1) return &gCallbacks[code];
    if (code == 2) return reinterpret_cast<TbSndCallback*>(static_cast<uintptr_t>(0x1234));
    return nullptr;
}

// ---- building and dumping nodes ---------------------------------------------------------------------------------------------

// kind; the arguments: user callback c the user data (d != 0: the constructor without one), sample callback c the callback code,
// completed callback c the callback code and d the user data, timer c the format and d the milliseconds.
TbSoundSamplePipe* MakeNode(int kind, int64_t c, int64_t d) {
    switch (kind) {
        case kUser:
            if (d != 0) return new TbSoundUserCallback();
            return new TbSoundUserCallback(Cookie(c));
        case kSample: return new _TbSoundSampleCallback(CallbackFor(c));
        case kCompleted: return new _TbSoundSampleCompletedCallback(CallbackFor(c), Cookie(d));
        case kPipe: return new TbSoundSamplePipe();
        case kTimer: return new TbTimerCodec(static_cast<uint8_t>(c), static_cast<uint32_t>(d));
        default: return new FakeNode();
    }
}

}  // namespace

extern "C" {

// Starts a case: the fixtures and their labels, the statics reset (the labels and queues were set by calllog's own entry).
void tpw_cb_begin() {
    gAllocatedCount = 0;
    std::memset(gNodes, 0, sizeof(gNodes));
    std::memset(gKinds, 0, sizeof(gKinds));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gBlockSizes, 0, sizeof(gBlockSizes));
    std::memset(gBlockPools, 0, sizeof(gBlockPools));
    std::memset(gBuffer, 0xA5, sizeof(gBuffer));
    std::memset(gInfoBytes, 0xA5, sizeof(gInfoBytes));
    gDtorFlag = 1;
    tpw_cl_label(3, gInfoBytes);
    tpw_cl_label(7, &gCallbacks[0]);
    tpw_cl_label(8, &gCallbacks[1]);
    for (int p = 0; p < kPools; ++p) {
        *kPoolTable[p].first = nullptr;
        *kPoolTable[p].current = 0;
        *kPoolTable[p].low = 0;
        *kPoolTable[p].high = 0;
        *kPoolTable[p].total = 0;
        *kPoolTable[p].level = 10;
    }
}

// Ends it: every block the case allocated is freed now (the runtime's delete kept them), without running anything.
void tpw_cb_end() {
    tpw_cl_on = 0;
    for (int i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] != nullptr) std::free(gAllocated[i]);
    }
    gAllocatedCount = 0;
    for (int p = 0; p < kPools; ++p) *kPoolTable[p].first = nullptr;
}

int32_t tpw_cb_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundSamplePipe));
        case 1: return static_cast<int32_t>(sizeof(TbSoundUserCallback));
        case 2: return static_cast<int32_t>(sizeof(_TbSoundSampleCallback));
        case 3: return static_cast<int32_t>(sizeof(_TbSoundSampleCompletedCallback));
        case 4: return static_cast<int32_t>(sizeof(TbTimerCodec));
        case 5: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(TbSoundSampleCodec));
    }
}

void tpw_cb_pool_set(int32_t pool, int32_t current, int32_t low, int32_t high, int32_t total, int32_t level) {
    *kPoolTable[pool].current = current;
    *kPoolTable[pool].low = low;
    *kPoolTable[pool].high = high;
    *kPoolTable[pool].total = total;
    *kPoolTable[pool].level = level;
}

// One pool: the six statics and the chain of idle objects (labels; at most 12 shown, then -9).
void tpw_cb_pool(int32_t pool, int64_t* out) {
    const PoolDesc& desc = kPoolTable[pool];
    out[0] = PW(*desc.first);
    out[1] = *desc.current;
    out[2] = *desc.low;
    out[3] = *desc.high;
    out[4] = *desc.total;
    out[5] = *desc.level;
    int n = 0;
    for (void* o = *desc.first; o != nullptr && n < 12; o = desc.link(o)) out[6 + n++] = PW(o);
    out[6 + n] = -9;
}

// The output buffer.
void tpw_cb_buffer(uint8_t* out) {
    std::memcpy(out, gBuffer, kBufferBytes);
}

// The five words of the sample info the pipes read and write: the flags (+0x14), the request word (+0x18), the callback (+0x44),
// the completed callback (+0x48) and the user data (+0x4c).
void tpw_cb_info(int64_t* out) {
    TbSoundSampleInfo* info = Info();
    out[0] = info->mFlags;
    out[1] = info->mField18;
    out[2] = PW(info->mCallback);
    out[3] = PW(info->mCompletedCallback);
    out[4] = Low32(info->mUserData);
}

// One slot: [status (0 empty, 1 gone, 2 alive), kind, next, previous, handle, base link class, user link class, user data, own
// link class, callback, size, time, position, format, owner, data start class, timer link class, kind of what is behind (0 none,
// 1 a timer codec, 2 anything else), and for a timer behind: format, time, position, size, link class, its next].
void tpw_cb_slot(int32_t slot, int64_t* out) {
    std::memset(out, 0, 24 * sizeof(int64_t));
    if (gNodes[slot] == nullptr) return;
    if (!Alive(slot)) {
        out[0] = 1;
        return;
    }
    TbSoundSamplePipe* p = gNodes[slot];
    const int kind = gKinds[slot];
    out[0] = 2;
    out[1] = kind;
    out[2] = PW(p->mNext);
    out[3] = PW(p->mPrevious);
    out[4] = Low32(p->mHandle);
    out[5] = LinkClass(p->mPoolLink);
    if (kind >= kUser && kind <= kCompleted) {
        TbSoundUserCallback* u = static_cast<TbSoundUserCallback*>(p);
        out[6] = LinkClass(u->mUserCallbackPoolLink);
        out[7] = Low32(u->mUserData);
    }
    if (kind == kSample) {
        _TbSoundSampleCallback* s = static_cast<_TbSoundSampleCallback*>(p);
        out[8] = LinkClass(s->mSampleCallbackPoolLink);
        out[9] = PW(s->mCallback);
    }
    if (kind == kCompleted) {
        _TbSoundSampleCompletedCallback* c = static_cast<_TbSoundSampleCompletedCallback*>(p);
        out[8] = LinkClass(c->mCompletedCallbackPoolLink);
        out[9] = PW(c->mCallback);
    }
    if (kind == kFake || kind == kTimer) {
        TbSoundSampleCodec* c = static_cast<TbSoundSampleCodec*>(p);
        out[10] = c->mDataSize;
        out[11] = c->mTime;
        out[12] = c->mDataPosition;
        out[13] = c->mFormat;
        out[14] = PW(c->mRefManager.mOwner);
        out[15] = LinkClass(c->mDataStart);
    }
    if (kind == kTimer) out[16] = LinkClass(static_cast<TbTimerCodec*>(p)->mTimerPoolLink);
    if (p->mNext != nullptr) {
        const TbTimerCodec* t = dynamic_cast<const TbTimerCodec*>(p->mNext);
        out[17] = t != nullptr ? 1 : 2;
        if (t != nullptr) {
            out[18] = t->mFormat;
            out[19] = t->mTime;
            out[20] = t->mDataPosition;
            out[21] = t->mDataSize;
            out[22] = LinkClass(t->mTimerPoolLink);
            out[23] = PW(t->mNext);
        }
    }
}

// One raw block of the pool operations: [status (0 none, 1 gone, 2 alive, 3 idle in its pool), then, for a block that is a whole
// object of the class, mNext, mPrevious, mHandle, the base link class, the user link class and the own link class].
void tpw_cb_block(int32_t index, int64_t* out) {
    std::memset(out, 0, 8 * sizeof(int64_t));
    void* block = gBlocks[index];
    if (block == nullptr) return;
    if (lab(block) <= 0) {
        out[0] = 1;
        return;
    }
    const int pool = gBlockPools[index];
    if (PooledIn(pool, block)) {
        out[0] = 3;
        return;
    }
    out[0] = 2;
    if (gBlockSizes[index] == kMacSizes[pool]) {
        TbSoundSamplePipe* p = static_cast<TbSoundSamplePipe*>(block);
        out[1] = PW(p->mNext);
        out[2] = PW(p->mPrevious);
        out[3] = Low32(p->mHandle);
        out[4] = LinkClass(p->mPoolLink);
        if (pool == kPoolUser || pool == kPoolSample || pool == kPoolCompleted) {
            out[5] = LinkClass(static_cast<TbSoundUserCallback*>(p)->mUserCallbackPoolLink);
        }
        if (pool != kPoolUser) out[6] = LinkClass(kPoolTable[pool].link(block));
    }
}

// The operations, in the order of OPS in tests/test_SoundCallbackPipes.py (the test checks the count).
enum Op {
    OP_NEW, OP_DTOR, OP_GET_CHUNK, OP_GET_NEXT, OP_SET_INFO, OP_GET_INFO, OP_GUARD, OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL,
    OP_CLEARCACHE, OP_CS_DTOR, OP_POKE, OP_LINK, OP_INFO, OP_COUNT
};

int32_t tpw_cb_op_count() { return OP_COUNT; }

// A slot whose node was freed (it has no label, or it went to the pool) is emptied after every operation.
static void Sweep() {
    for (int i = 0; i < kSlots; ++i) {
        if (gNodes[i] != nullptr && !Alive(i)) {
            gNodes[i] = nullptr;
            gKinds[i] = kNone;
        }
    }
}

static int64_t gAux[4];
int64_t tpw_cb_aux(int32_t index) { return gAux[index]; }

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d);

// One operation. Results come back as a 64-bit integer (labels for pointers); -99 means "not applicable" on both sides.
int64_t tpw_cb_op(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    const int64_t result = DoOp(op, a, b, c, d);
    Sweep();
    return result;
}

// Whether a GetDataChunk on this node can be driven without a fault or an endless loop (the mirror is ChunkSafe in the test):
// a sample pipe needs a fixture callback and a fake or timer codec behind it (or nothing), and a timer needs a size in range
// (the fake answers 1 only for those); a completed callback forwards to what is behind it.
static bool ChunkSafe(const TbSoundSamplePipe* node, int64_t size, int depth) {
    switch (TypeOf(node)) {
        case kUser:
        case kPipe:
        case kFake:
            return true;
        case kTimer:
            return size >= 0 && size <= 0x1000;
        case kSample: {
            const _TbSoundSampleCallback* s = static_cast<const _TbSoundSampleCallback*>(node);
            if (s->mNext == nullptr) return true;
            const int next = TypeOf(s->mNext);
            if (next != kFake && next != kTimer) return false;
            if (lab(s->mCallback) != 7 && lab(s->mCallback) != 8) return false;
            return next != kTimer || size <= 0x1000;
        }
        case kCompleted: {
            if (depth > 5) return false;
            if (node->mNext == nullptr) return true;
            return ChunkSafe(node->mNext, size, depth + 1);
        }
        default:
            return false;
    }
}

// Whether the pipes behind this one are all alive (the chain functions call through them), within six links.
static bool ChainAlive(const TbSoundSamplePipe* node) {
    const TbSoundSamplePipe* next = node->mNext;
    for (int depth = 0; next != nullptr; ++depth) {
        if (TypeOf(next) == kNone || depth >= 6) return false;
        next = next->mNext;
    }
    return true;
}

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    gAux[0] = gAux[1] = 0;
    const bool needsNode = op == OP_DTOR || op == OP_GET_CHUNK || op == OP_GET_NEXT || op == OP_SET_INFO || op == OP_GET_INFO ||
                           op == OP_GUARD || op == OP_POKE || op == OP_LINK;
    TbSoundSamplePipe* pipe = (a >= 0 && a < kSlots) ? gNodes[a] : nullptr;
    int kind = kNone;
    if (needsNode) {
        if (a < 0 || a >= kSlots || !Alive(static_cast<int>(a))) return -99;
        kind = gKinds[a];
        if (op == OP_GET_CHUNK) {
            if (b < 0 || b > 0x800 || !ChunkSafe(pipe, c, 0)) return -99;
        }
        if ((op == OP_SET_INFO || op == OP_GET_INFO) && !ChainAlive(pipe)) return -99;
        if (op == OP_GET_NEXT) {
            if (kind != kSample) return -99;
            _TbSoundSampleCallback* s = static_cast<_TbSoundSampleCallback*>(pipe);
            const int next = TypeOf(s->mNext);
            if (next != kFake && next != kTimer) return -99;
            if (lab(s->mCallback) != 7 && lab(s->mCallback) != 8) return -99;
        }
        if (op == OP_POKE) {
            if (b == 3 && (kind < kUser || kind > kCompleted)) return -99;
            if (b == 4 && kind != kSample && kind != kCompleted) return -99;
            if (b >= 5 && kind != kFake && kind != kTimer) return -99;
        }
        if (op == OP_LINK) {
            if (b >= kSlots || b < -1) return -99;
            if (b >= 0 && (!Alive(static_cast<int>(b)) || b == a)) return -99;
        }
    }
    switch (op) {
        case OP_NEW: {
            if (a < 0 || a >= kSlots || b < kUser || b > kTimer) return -99;
            TbSoundSamplePipe* made = MakeNode(static_cast<int>(b), c, d);
            for (int i = 0; i < kSlots; ++i) {
                if (gNodes[i] == made) gNodes[i] = nullptr;
            }
            gNodes[a] = made;
            gKinds[a] = static_cast<int>(b);
            return lab(made);
        }
        case OP_DTOR: {
            gDtorFlag = static_cast<int16_t>(b);
            if (static_cast<int16_t>(b) > 0) {
                delete pipe;
            } else {
                pipe->~TbSoundSamplePipe();
                forget(pipe);   // destroyed but not freed: the object is dead to the test (its vptr is the base's now)
            }
            gDtorFlag = 1;
            gNodes[a] = nullptr;
            gKinds[a] = kNone;
            return 0;
        }
        case OP_GET_CHUNK: {
            // b the offset in the output buffer, c the size, d the flag; the result of the virtual call
            int32_t done = 0x5555;
            const int32_t result = pipe->GetDataChunk(gBuffer + kBufferBase + b, static_cast<int32_t>(c), done, d != 0);
            gAux[0] = done;
            return result;
        }
        case OP_GET_NEXT:
            static_cast<_TbSoundSampleCallback*>(pipe)->GetNextSample(static_cast<uint32_t>(b));
            return 0;
        case OP_SET_INFO:
            return static_cast<int32_t>(pipe->SetInfo(*Info()));
        case OP_GET_INFO:
            pipe->GetInfo(*Info());
            return 0;
        case OP_GUARD: {
            // b the mode: 0 construct and destroy, 1 set the flag then destroy (the pipe detaches), 2 set it and clear it again
            CAutoDetach guard(pipe);
            gAux[0] = guard.mDetach;
            if (b == 1) guard = true;
            if (b == 2) {
                guard = true;
                guard = false;
            }
            gAux[1] = guard.mDetach;
            return lab(guard.mPipe);
        }
        case OP_NEWOP: {
            // a the block index, b the class, c the Mac size
            if (a < 0 || a >= 8 || b < 0 || b >= kPools) return -99;
            void* block = kPoolTable[b].opNew(HostSize(static_cast<int>(b), c));
            gBlocks[a] = block;
            gBlockSizes[a] = c;
            gBlockPools[a] = static_cast<int>(b);
            return lab(block);
        }
        case OP_DELETEOP: {
            // a the block index (-1: NULL), b the class, c the Mac size, d forgets the block
            if (b < 0 || b >= kPools) return -99;
            kPoolTable[b].opDelete(a < 0 ? nullptr : gBlocks[a], HostSize(static_cast<int>(b), c));
            if (a >= 0 && d != 0) gBlocks[a] = nullptr;
            return 0;
        }
        case OP_TOPUP:
            if (a < 0 || a >= kPools) return -99;
            kPoolTable[a].topUp();
            return 0;
        case OP_SETLEVEL:
            if (a < 0 || a >= kPools) return -99;
            kPoolTable[a].setLevel(static_cast<int32_t>(b));
            return 0;
        case OP_CLEARCACHE:
            if (a < 0 || a >= kPools) return -99;
            kPoolTable[a].clearCache();
            return 0;
        case OP_CS_DTOR: {
            // a which cleanup object (0 user callback, 1 sample callback, 2 completed callback), b the flag (positive: the deleting form)
            if (a < 0 || a > 2) return -99;
            if (a == 0) {
                _CS_TbSoundUserCallback* object = new _CS_TbSoundUserCallback();
                if (static_cast<int16_t>(b) > 0) delete object; else object->~_CS_TbSoundUserCallback();
            } else if (a == 1) {
                _CS__TbSoundSampleCallback* object = new _CS__TbSoundSampleCallback();
                if (static_cast<int16_t>(b) > 0) delete object; else object->~_CS__TbSoundSampleCallback();
            } else {
                _CS__TbSoundSampleCompletedCallback* object = new _CS__TbSoundSampleCompletedCallback();
                if (static_cast<int16_t>(b) > 0) delete object; else object->~_CS__TbSoundSampleCompletedCallback();
            }
            return 0;
        }
        case OP_POKE: {
            // a slot, b the field, c the value
            switch (b) {
                case 0: pipe->mHandle = Cookie(c); break;
                case 3: static_cast<TbSoundUserCallback*>(pipe)->mUserData = Cookie(c); break;
                case 4:
                    if (kind == kSample) static_cast<_TbSoundSampleCallback*>(pipe)->mCallback = CallbackFor(c);
                    else static_cast<_TbSoundSampleCompletedCallback*>(pipe)->mCallback = CallbackFor(c);
                    break;
                case 5: static_cast<TbSoundSampleCodec*>(pipe)->mFormat = static_cast<uint8_t>(c); break;
                case 6: static_cast<TbSoundSampleCodec*>(pipe)->mTime = static_cast<uint32_t>(c); break;
                case 7: static_cast<TbSoundSampleCodec*>(pipe)->mDataPosition = static_cast<uint32_t>(c); break;
                case 8: static_cast<TbSoundSampleCodec*>(pipe)->mDataSize = static_cast<uint32_t>(c); break;
                default: break;
            }
            return 0;
        }
        case OP_LINK:
            // a the slot, b the slot behind it (-1: nothing): the two chain fields only
            if (b < 0) {
                pipe->mNext = nullptr;
            } else {
                pipe->mNext = gNodes[b];
                gNodes[b]->mPrevious = pipe;
            }
            return 0;
        case OP_INFO: {
            // a the field, b the value: 0 the flags, 1 the request word, 2 the callback (a code), 3 the user data, 4 the completed
            // callback (a code)
            TbSoundSampleInfo* info = Info();
            switch (a) {
                case 0: info->mFlags = static_cast<uint32_t>(b); break;
                case 1: info->mField18 = static_cast<uint32_t>(b); break;
                case 2: info->mCallback = CallbackFor(b); break;
                case 3: info->mUserData = Cookie(b); break;
                case 4: info->mCompletedCallback = CallbackFor(b); break;
                default: return -99;
            }
            return 0;
        }
        default:
            return -98;
    }
}

}  // extern "C"
