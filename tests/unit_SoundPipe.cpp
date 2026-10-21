// Fakes and test entry points for unit w78, the sound library's sample pipe base (libtpw_unit_SoundPipe.dylib, driven by
// tests/test_SoundPipe.py in the program "sound symbols shared"; see calllog.h):
//   src/sound/StreamableMpeg2.cpp, src/sound/TbSoundCodecLoop.cpp, src/sound/TbSoundPipe.cpp, src/sound/TbSoundSampleCallback.cpp, src/sound/TbSoundSampleCodec.cpp, src/sound/TbSoundSamplePipe.cpp, src/sound/TbSoundSystemInfo.cpp   TbSoundSamplePipe (the pool, the chain, the forwards), TbSoundSPCodec, TbSoundSampleCodec
// Real: those and TbRefRelease. Fakes, each recording what the original's callee would be given (the PowerPC side records
// the same from its stubs):
//   FakeNode, a TbSoundSampleCodec that overrides every virtual slot and records it (the chain's tail, and a codec whose
//       GetCodec, GetSampleRate and SetSampleRate answer the case's queues, so that the virtual dispatch of the real code is
//       checked); its destructor records the flag and, when positive, the block is deleted (a "delete" event follows)
//   FakeOwner, the owner a codec holds a reference on (records AddRef and Release)
//   the runtime heap (operator new and delete: calllog.cpp records "new" with the Mac's size and a label through the
//       observer below, "delete" for a labelled block) and the clock LbTime_GetClock (records "Clock", answers the queue)
//
// Sizes: the host's pipe is 40 bytes and its sample codec 80, the Mac's 0x14 and 0x2c: the sizes recorded and the sizes the
// pool operations are given are the Mac's (MacSize and HostSize). The Mac sizes 0x28 and 0x48 are not used by the test.
//
// Labels: 1 and 2 the owners, 3 the sample info, 4 to 6 the three sample headers, 100 and up the blocks the runtime made.
// A block the case allocates is filled with 0xA5 on both sides, so that the words a constructor leaves alone are compared too.
//
// The case drives everything with tpw_sp_op and reads the state back with tpw_sp_slot and tpw_sp_pool.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "LbTime.h"
#include "TbSoundPipe.h"
#include "calllog.h"

using namespace calllog;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr int kMaxBlocks = 4096;
void* gAllocated[kMaxBlocks];   // every block the runtime handed out in the case (freed at the end when not deleted)
int gAllocatedCount;

std::size_t MacSize(std::size_t size) {
    if (size == sizeof(TbSoundSamplePipe)) return 0x14;
    if (size == sizeof(TbSoundSampleCodec)) return 0x2c;
    return size;
}

std::size_t HostSize(int64_t size) {
    if (size == 0x14) return sizeof(TbSoundSamplePipe);
    if (size == 0x2c) return sizeof(TbSoundSampleCodec);
    return static_cast<std::size_t>(static_cast<uint32_t>(size));
}

// The runtime's new: a "new" event of the Mac's size and the block's label, the block filled with 0xA5.
void SoundAllocated(std::size_t size, bool, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on()) return;
    if (gAllocatedCount < kMaxBlocks) gAllocated[gAllocatedCount++] = block;
    ev("new", {static_cast<int64_t>(MacSize(size)), newlab(block)});
}

// Blocks the runtime frees are kept until the end of the case (see tpw_sp_end), so that no address is reused while a stale
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

namespace {

constexpr int kSlots = 6;
constexpr int kHeaderBytes = 0x60;
constexpr uint32_t kFill = 0xA5A5A5A5u;
enum Kind { kNone = 0, kPipe = 1, kSP = 2, kCodec = 3, kDefault = 4, kFake = 5 };

// ---- the owners -----------------------------------------------------------------------------------------------------------

class FakeOwner : public TbRefRelease {
public:
    int32_t Release() override {
        ev("Owner.Release", {lab(this)});
        return 0;
    }
    int32_t AddRef() override {
        ev("Owner.AddRef", {lab(this)});
        return 0;
    }
};

FakeOwner gOwners[2];
alignas(16) uint8_t gHeaders[3][kHeaderBytes];
alignas(16) uint8_t gInfoBytes[sizeof(TbSoundSampleInfo) + 16];
TbSoundSampleInfo* Info() { return reinterpret_cast<TbSoundSampleInfo*>(gInfoBytes); }

TbSoundSamplePipe* gNodes[kSlots];
int gKinds[kSlots];
void* gBlocks[8];
int64_t gBlockSizes[8];   // the Mac sizes the raw blocks were asked for
int32_t gDtorFlag = 1;   // the flag the destructor call carries (a `delete this` of the class code is flag 1)

TbSoundSamplePipe* NodeBySlot(int64_t slot) {
    return slot >= 0 && slot < kSlots ? gNodes[slot] : nullptr;
}

void* Cookie(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

// A pointer field as the test compares it: 0, the label of the object, the fill word, or -1.
int64_t PW(const void* pointer) {
    if (pointer == nullptr) return 0;
    const int32_t label = lab(pointer);
    if (label > 0) return label;
    if (Low32(pointer) == kFill) return kFill;
    return -1;
}

// ---- the node that records every slot ------------------------------------------------------------------------------------

// A queue value that names a node: -1 NULL, -2 the node's own mNext (what the real GetCodec answers), otherwise a slot.
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
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t&, bool flag) override {
        ev("Fake.GetDataChunk", {lab(this), static_cast<int64_t>(Low32(buffer)), size, flag});
        return static_cast<int32_t>(ret("Chunk", 0));
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
        return static_cast<const uint8_t*>(Cookie(ret("DataStart", 0)));
    }
    void SetDataPosition(uint32_t position) override { ev("Fake.SetDataPosition", {lab(this), position}); }
    uint32_t GetDataPosition() override {
        ev("Fake.GetDataPosition", {lab(this)});
        return static_cast<uint32_t>(ret("DataPosition", 0));
    }
    uint32_t GetDataSize() override {
        ev("Fake.GetDataSize", {lab(this)});
        return static_cast<uint32_t>(ret("DataSize", 0));
    }
    void SetSampleRate(uint16_t rate) override { ev("Fake.SetSampleRate", {lab(this), rate}); }
    uint32_t GetBitRate() override {
        ev("Fake.GetBitRate", {lab(this)});
        return static_cast<uint32_t>(ret("BitRate", 0));
    }
    uint32_t GetChannels() override {
        ev("Fake.GetChannels", {lab(this)});
        return static_cast<uint32_t>(ret("Channels", 0));
    }
    TbSoundSPCodec* GetCodec() override {
        ev("Fake.GetCodec", {lab(this)});
        return static_cast<TbSoundSPCodec*>(QueuedNode("Codec", this));
    }
    bool SetTime(bool stopped, uint32_t bytes) override {
        ev("Fake.SetTime", {lab(this), stopped, bytes});
        return ret("SetTime", 0) != 0;
    }
};

static_assert(sizeof(FakeNode) == sizeof(TbSoundSampleCodec), "the fake is the size of a sample codec");

// ---- building and dumping nodes ---------------------------------------------------------------------------------------------

TbSoundSamplePipe* MakeNode(int kind, int header, int owner) {
    switch (kind) {
        case kPipe: return new TbSoundSamplePipe();
        case kSP: return new TbSoundSPCodec();
        case kCodec: return new TbSoundSampleCodec(reinterpret_cast<TbSampleHeader*>(gHeaders[header]), &gOwners[owner]);
        case kDefault: return new TbSoundSampleCodec();
        default: return new FakeNode();
    }
}

TbSoundSampleCodec* AsCodec(int64_t slot) {
    return static_cast<TbSoundSampleCodec*>(NodeBySlot(slot));
}

// Where a data start pointer points: 0x10000 times the header number plus the offset in it, or as a pointer field.
int64_t DataStartWord(const uint8_t* pointer) {
    for (int i = 0; i < 3; ++i) {
        if (pointer >= gHeaders[i] && pointer < gHeaders[i] + kHeaderBytes) return 0x10000 * (i + 1) + (pointer - gHeaders[i]);
    }
    return Low32(pointer);
}

// 0 NULL, 1 the fill word, 2 anything else: what the pool link of a node that is not idle holds.
int64_t LinkClass(const void* pointer) {
    if (pointer == nullptr) return 0;
    return Low32(pointer) == kFill ? 1 : 2;
}

bool Pooled(const TbSoundSamplePipe* object) {
    int steps = 0;
    for (const TbSoundSamplePipe* idle = TbSoundSamplePipe::mFirstTbSoundSamplePipe; idle != nullptr && steps < 64;
         idle = idle->mPoolLink, ++steps) {
        if (idle == object) return true;
    }
    return false;
}

bool Alive(int slot) {
    return gNodes[slot] != nullptr && lab(gNodes[slot]) > 0 && !Pooled(gNodes[slot]);
}

}  // namespace

extern "C" {

// Starts a case: the fixtures and their labels, the statics reset (the labels and queues were set by calllog's own entry).
void tpw_sp_begin() {
    gAllocatedCount = 0;
    std::memset(gNodes, 0, sizeof(gNodes));
    std::memset(gKinds, 0, sizeof(gKinds));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gBlockSizes, 0, sizeof(gBlockSizes));
    std::memset(gHeaders, 0xA5, sizeof(gHeaders));
    std::memset(gInfoBytes, 0xA5, sizeof(gInfoBytes));
    gDtorFlag = 1;
    tpw_cl_label(1, &gOwners[0]);
    tpw_cl_label(2, &gOwners[1]);
    tpw_cl_label(3, gInfoBytes);
    tpw_cl_label(4, gHeaders[0]);
    tpw_cl_label(5, gHeaders[1]);
    tpw_cl_label(6, gHeaders[2]);
    TbSoundSamplePipe::mFirstTbSoundSamplePipe = nullptr;
    TbSoundSamplePipe::mCurrentTbSoundSamplePipe = 0;
    TbSoundSamplePipe::mLowTbSoundSamplePipe = 0;
    TbSoundSamplePipe::mHighTbSoundSamplePipe = 0;
    TbSoundSamplePipe::mTotalTbSoundSamplePipe = 0;
    TbSoundSamplePipe::mLevelTbSoundSamplePipe = 10;
}

// Ends it: every block the case allocated is freed now (the runtime's delete kept them), without running anything.
void tpw_sp_end() {
    tpw_cl_on = 0;
    for (int i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] != nullptr) std::free(gAllocated[i]);
    }
    gAllocatedCount = 0;
    TbSoundSamplePipe::mFirstTbSoundSamplePipe = nullptr;
}

int32_t tpw_sp_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundSamplePipe));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSampleCodec));
        case 2: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(TbSampleHeader));
    }
}

// A sample header (the Mac's first eight bytes: size, rate, bits, flags), in the host's byte order.
void tpw_sp_set_header(int32_t index, uint32_t size, uint32_t rate, uint32_t bits, uint32_t flags) {
    TbSampleHeader* header = reinterpret_cast<TbSampleHeader*>(gHeaders[index]);
    header->mSize = static_cast<int32_t>(size);
    header->mRate = static_cast<uint16_t>(rate);
    header->mBits = static_cast<uint8_t>(bits);
    header->mFlags = static_cast<uint8_t>(flags);
}

// The second flags word of the sample info (the word at +0xc on the Mac).
void tpw_sp_set_info(uint32_t flags) {
    Info()->mField0C = flags;
}

uint32_t tpw_sp_get_info() {
    return Info()->mField0C;
}

void tpw_sp_pool_set(int32_t current, int32_t low, int32_t high, int32_t total, int32_t level) {
    TbSoundSamplePipe::mCurrentTbSoundSamplePipe = current;
    TbSoundSamplePipe::mLowTbSoundSamplePipe = low;
    TbSoundSamplePipe::mHighTbSoundSamplePipe = high;
    TbSoundSamplePipe::mTotalTbSoundSamplePipe = total;
    TbSoundSamplePipe::mLevelTbSoundSamplePipe = level;
}

// The six statics and the chain of idle objects (labels; -9 ends the chain; at most 40 shown).
void tpw_sp_pool(int64_t* out) {
    out[0] = PW(TbSoundSamplePipe::mFirstTbSoundSamplePipe);
    out[1] = TbSoundSamplePipe::mCurrentTbSoundSamplePipe;
    out[2] = TbSoundSamplePipe::mLowTbSoundSamplePipe;
    out[3] = TbSoundSamplePipe::mHighTbSoundSamplePipe;
    out[4] = TbSoundSamplePipe::mTotalTbSoundSamplePipe;
    out[5] = TbSoundSamplePipe::mLevelTbSoundSamplePipe;
    int n = 0;
    for (TbSoundSamplePipe* o = TbSoundSamplePipe::mFirstTbSoundSamplePipe; o != nullptr && n < 40; o = o->mPoolLink) {
        out[6 + n++] = PW(o);
    }
    out[6 + n] = -9;
}

// One slot: [status (0 empty, 1 gone, 2 alive), kind, next, previous, handle, link class, owner, size, data start, time,
// position, format].
void tpw_sp_slot(int32_t slot, int64_t* out) {
    std::memset(out, 0, 16 * sizeof(int64_t));
    if (gNodes[slot] == nullptr) return;
    if (!Alive(slot)) {
        out[0] = 1;
        return;
    }
    TbSoundSamplePipe* p = gNodes[slot];
    out[0] = 2;
    out[1] = gKinds[slot];
    out[2] = PW(p->mNext);
    out[3] = PW(p->mPrevious);
    out[4] = Low32(p->mHandle);
    out[5] = LinkClass(p->mPoolLink);
    if (gKinds[slot] >= kCodec) {
        TbSoundSampleCodec* c = static_cast<TbSoundSampleCodec*>(p);
        out[6] = PW(c->mRefManager.mOwner);
        out[7] = c->mDataSize;
        out[8] = DataStartWord(c->mDataStart);
        out[9] = c->mTime;
        out[10] = c->mDataPosition;
        out[11] = c->mFormat;
    }
}

// One raw block of the pool operations: [status (0 none, 1 gone, 2 alive, 3 idle in the pool), then, for a block that can hold a
// pipe, mNext, mPrevious, mHandle and the link class read as a pipe's fields].
void tpw_sp_block(int32_t index, int64_t* out) {
    std::memset(out, 0, 8 * sizeof(int64_t));
    void* block = gBlocks[index];
    if (block == nullptr) return;
    if (lab(block) <= 0) {
        out[0] = 1;
        return;
    }
    TbSoundSamplePipe* p = static_cast<TbSoundSamplePipe*>(block);
    if (Pooled(p)) {
        out[0] = 3;
        return;
    }
    out[0] = 2;
    if (gBlockSizes[index] >= 0x14 && gBlockSizes[index] != 0x15) {
        out[1] = PW(p->mNext);
        out[2] = PW(p->mPrevious);
        out[3] = Low32(p->mHandle);
        out[4] = LinkClass(p->mPoolLink);
    }
}

// The operations, in the order of OPS in tests/test_SoundPipe.py (the test checks the names).
enum Op {
    OP_NEW, OP_DTOR, OP_SET_HANDLE, OP_GET_DATA_CHUNK, OP_PIPE_GET_SAMPLE_RATE, OP_PIPE_RESTART, OP_PIPE_SET_DATA_START,
    OP_PIPE_GET_INFO, OP_PIPE_SET_INFO, OP_PIPE_COMPLETED, OP_FIND_PIPE, OP_ATTATCH, OP_ATTACH_REPLACE, OP_RELEASE, OP_DETATCH,
    OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL, OP_CLEARCACHE,
    OP_SP_GET_CODEC, OP_SP_RESTART, OP_SP_GET_DATA_START, OP_SP_SET_DATA_POSITION, OP_SP_GET_DATA_POSITION, OP_SP_GET_DATA_SIZE,
    OP_SP_SET_SAMPLE_RATE, OP_SP_GET_SAMPLE_RATE, OP_SP_GET_BIT_RATE, OP_SP_GET_CHANNELS, OP_SP_GET_INFO, OP_SP_SET_INFO,
    OP_SC_GET_DATA_POSITION, OP_SC_GET_DATA_START, OP_SC_GET_DATA_SIZE, OP_SC_GET_INFO, OP_SC_GET_SAMPLE_FORMAT, OP_SC_SET_INFO,
    OP_SC_SET_FORMAT_FROM_HEADER, OP_SC_SET_SAMPLE_RATE, OP_SC_GET_SAMPLE_RATE, OP_SC_GET_BIT_RATE, OP_SC_GET_CHANNELS,
    OP_SC_SET_TIME, OP_SC_IS_TIME_COMPLETE, OP_SC_SET_DATA_POSITION, OP_SC_TIMED,
    OP_POKE, OP_COUNT
};

int32_t tpw_sp_op_count() { return OP_COUNT; }

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
int64_t tpw_sp_aux(int32_t index) { return gAux[index]; }

// A pointer result as the test compares it: the label of a node, 0 for NULL.
static int64_t Label(const void* pointer) {
    return pointer == nullptr ? 0 : lab(pointer);
}

// One operation. Results come back as a 64-bit integer (labels for pointers); -99 means "not applicable" on both sides.
static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d);
int64_t tpw_sp_op(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    const int64_t result = DoOp(op, a, b, c, d);
    Sweep();
    return result;
}

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    gAux[0] = gAux[1] = 0;
    const bool needsNode = op != OP_NEW && op != OP_NEWOP && op != OP_DELETEOP && op != OP_TOPUP && op != OP_SETLEVEL &&
                           op != OP_CLEARCACHE;
    if (needsNode) {
        if (a < 0 || a >= kSlots || !Alive(static_cast<int>(a))) return -99;
        const int kind = gKinds[a];
        if (op >= OP_SP_GET_CODEC && op <= OP_SP_SET_INFO && kind < kSP) return -99;
        if (op >= OP_SC_GET_DATA_POSITION && op <= OP_SC_TIMED && kind < kCodec) return -99;
        if (op == OP_POKE && b > 0 && kind < kCodec) return -99;
    }
    if (op == OP_NEW && (a < 0 || a >= kSlots)) return -99;
    TbSoundSamplePipe* pipe = (a >= 0 && a < kSlots) ? gNodes[a] : nullptr;
    switch (op) {
        case OP_NEW: {
            TbSoundSamplePipe* made = MakeNode(static_cast<int>(b), static_cast<int>(c), static_cast<int>(d));
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
            }
            gDtorFlag = 1;
            gNodes[a] = nullptr;
            gKinds[a] = kNone;
            return 0;
        }
        case OP_SET_HANDLE:
            return Low32(pipe->TbSoundSamplePipe::SetHandle(Cookie(b)));
        case OP_GET_DATA_CHUNK: {
            int32_t done = 0x5555;
            const int32_t result = pipe->TbSoundSamplePipe::GetDataChunk(Cookie(b), static_cast<int32_t>(c), done, d != 0);
            gAux[0] = done;
            return result;
        }
        case OP_PIPE_GET_SAMPLE_RATE:
            return pipe->TbSoundSamplePipe::GetSampleRate();
        case OP_PIPE_RESTART:
            pipe->TbSoundSamplePipe::Restart();
            return 0;
        case OP_PIPE_SET_DATA_START:
            pipe->TbSoundSamplePipe::SetDataStart(Cookie(b));
            return 0;
        case OP_PIPE_GET_INFO:
            pipe->TbSoundSamplePipe::GetInfo(*Info());
            return 0;
        case OP_PIPE_SET_INFO:
            return static_cast<int32_t>(pipe->TbSoundSamplePipe::SetInfo(*Info()));
        case OP_PIPE_COMPLETED:
            pipe->TbSoundSamplePipe::Completed(b != 0);
            return 0;
        case OP_FIND_PIPE:
            return Label(pipe->TbSoundSamplePipe::FindPipe(Cookie(b)));
        case OP_ATTATCH:
            if (b >= 0 && (b >= kSlots || !Alive(static_cast<int>(b)))) return -99;
            return Label(pipe->TbSoundSamplePipe::Attatch(NodeBySlot(b)));
        case OP_ATTACH_REPLACE:
            if (b < 0 || b >= kSlots || !Alive(static_cast<int>(b))) return -99;
            return Label(pipe->TbSoundSamplePipe::AttachReplace(NodeBySlot(b), NodeBySlot(c)));
        case OP_RELEASE:
            pipe->TbSoundSamplePipe::Release();
            return 0;
        case OP_DETATCH:
            pipe->TbSoundSamplePipe::Detatch();
            return 0;
        case OP_NEWOP: {
            void* block = TbSoundSamplePipe::operator new(HostSize(b));
            gBlocks[a] = block;
            gBlockSizes[a] = b;
            return lab(block);
        }
        case OP_DELETEOP:
            TbSoundSamplePipe::operator delete(a < 0 ? nullptr : gBlocks[a], HostSize(b));
            if (a >= 0 && c != 0) gBlocks[a] = nullptr;
            return 0;
        case OP_TOPUP:
            TbSoundSamplePipe::TopUp();
            return 0;
        case OP_SETLEVEL:
            TbSoundSamplePipe::SetLevel(static_cast<int32_t>(a));
            return 0;
        case OP_CLEARCACHE:
            TbSoundSamplePipe::ClearCache();
            return 0;
        // ---- the SP codec's versions, called on a node that is an SP codec or a sample codec
        case OP_SP_GET_CODEC:
            return Label(static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetCodec());
        case OP_SP_RESTART:
            static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::Restart();
            return 0;
        case OP_SP_GET_DATA_START:
            return DataStartWord(static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetDataStart());
        case OP_SP_SET_DATA_POSITION:
            static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::SetDataPosition(static_cast<uint32_t>(b));
            return 0;
        case OP_SP_GET_DATA_POSITION:
            return static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetDataPosition();
        case OP_SP_GET_DATA_SIZE:
            return static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetDataSize();
        case OP_SP_SET_SAMPLE_RATE:
            static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::SetSampleRate(static_cast<uint16_t>(b));
            return 0;
        case OP_SP_GET_SAMPLE_RATE:
            return static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetSampleRate();
        case OP_SP_GET_BIT_RATE:
            return static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetBitRate();
        case OP_SP_GET_CHANNELS:
            return static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetChannels();
        case OP_SP_GET_INFO:
            static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::GetInfo(*Info());
            return 0;
        case OP_SP_SET_INFO:
            return static_cast<int32_t>(static_cast<TbSoundSPCodec*>(pipe)->TbSoundSPCodec::SetInfo(*Info()));
        // ---- the sample codec's own versions
        case OP_SC_GET_DATA_POSITION:
            return AsCodec(a)->TbSoundSampleCodec::GetDataPosition();
        case OP_SC_GET_DATA_START:
            return DataStartWord(AsCodec(a)->TbSoundSampleCodec::GetDataStart());
        case OP_SC_GET_DATA_SIZE:
            return AsCodec(a)->TbSoundSampleCodec::GetDataSize();
        case OP_SC_GET_INFO:
            AsCodec(a)->TbSoundSampleCodec::GetInfo(*Info());
            return 0;
        case OP_SC_GET_SAMPLE_FORMAT:
            return AsCodec(a)->GetSampleFormat();
        case OP_SC_SET_INFO:
            return static_cast<int32_t>(AsCodec(a)->TbSoundSampleCodec::SetInfo(*Info()));
        case OP_SC_SET_FORMAT_FROM_HEADER:
            AsCodec(a)->SetSampleFormatFromHeader(reinterpret_cast<TbSampleHeader*>(gHeaders[b]));
            return 0;
        case OP_SC_SET_SAMPLE_RATE:
            AsCodec(a)->TbSoundSampleCodec::SetSampleRate(static_cast<uint16_t>(b));
            return 0;
        case OP_SC_GET_SAMPLE_RATE:
            return AsCodec(a)->TbSoundSampleCodec::GetSampleRate();
        case OP_SC_GET_BIT_RATE:
            return AsCodec(a)->TbSoundSampleCodec::GetBitRate();
        case OP_SC_GET_CHANNELS:
            return AsCodec(a)->TbSoundSampleCodec::GetChannels();
        case OP_SC_SET_TIME:
            return AsCodec(a)->TbSoundSampleCodec::SetTime(b != 0, static_cast<uint32_t>(c));
        case OP_SC_IS_TIME_COMPLETE:
            return AsCodec(a)->IsTimeComplete();
        case OP_SC_SET_DATA_POSITION:
            AsCodec(a)->TbSoundSampleCodec::SetDataPosition(static_cast<uint32_t>(b));
            return 0;
        case OP_SC_TIMED:
            return AsCodec(a)->Timed();
        case OP_POKE: {
            // a slot, b the field, c the value
            TbSoundSampleCodec* codec = static_cast<TbSoundSampleCodec*>(pipe);
            switch (b) {
                case 0: pipe->mHandle = Cookie(c); break;
                case 1: codec->mFormat = static_cast<uint8_t>(c); break;
                case 2: codec->mTime = static_cast<uint32_t>(c); break;
                case 3: codec->mDataPosition = static_cast<uint32_t>(c); break;
                case 4: codec->mDataSize = static_cast<uint32_t>(c); break;
                case 5: codec->mDataStart = static_cast<const uint8_t*>(Cookie(c)); break;
                default: break;
            }
            return 0;
        }
        default:
            return -98;
    }
}

}  // extern "C"
