// Fakes and test entry points for unit w80, the sound library's four small codecs (libtpw_unit_SoundCodecs.dylib, driven by
// tests/test_SoundCodecs.py in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundCodecs.cpp   _TbSoundSampleWAV, _TbSoundSampleWAVFile, _TbSoundCodecLoop, TbTimerCodec, CMemoryCopy::Copy
// Real: those, unit w78's classes (src/TbSoundPipe.cpp) and TbRefRelease. Fakes, each recording what the original's callee would
// be given (the PowerPC side records the same from its stubs):
//   FakeNode, a TbSoundSampleCodec that overrides every virtual slot and records it: the codec behind a loop (its
//       GetDataChunk answers the queues "Chunk" (the result) and "Got" (the bytes delivered); its destructor records the flag)
//   FakeFile, the open file of a WAVFile codec: Read, Seek and GetPos record and work on the case's file bytes (the queue
//       "Read" caps a read, "Seek" -1 fails a seek, "Pos" scripts the answers of GetPos)
//   FakeOwner, the owner a codec holds a reference on (records AddRef and Release)
//   the runtime heap (operator new and delete: calllog.cpp records "new" with the Mac's size and a label through the observer
//       below, "delete" for a labelled block) and the clock LbTime_GetClock (records "Clock", answers the queue)
//
// Sizes: the host objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are
// mapped: a class's host size is its Mac size (0x30 WAV and timer, 0x3c WAV file, 0x18 loop), the host sample codec (the
// FakeNode) is 0x2c, and any other size the test asks for is carried as the Mac size + 0x1000 on the host.
//
// Labels: 1 and 2 the owners, 4 to 6 the three sample headers, 7 the file, 100 and up the blocks the runtime made. A block the
// case allocates is filled with 0xA5 on both sides, so that the words a constructor leaves alone are compared too. The output
// buffer (gBuffer, 0x1000 bytes of 0xA5) is not labelled: a pointer into it is recorded as its offset.
//
// The case drives everything with tpw_sc_op and reads the state back with tpw_sc_slot, tpw_sc_pool, tpw_sc_block and
// tpw_sc_buffer.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "LbTime.h"
#include "TbSoundCodecs.h"
#include "calllog.h"

using namespace calllog;
using bfl::FileSystem::CFile;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr int kMaxBlocks = 4096;
void* gAllocated[kMaxBlocks];   // every block the runtime handed out in the case (freed at the end when not deleted)
int gAllocatedCount;

constexpr std::size_t kLiteral = 0x1000;

enum PoolClass { kPoolWAV = 0, kPoolWAVFile = 1, kPoolLoop = 2, kPoolTimer = 3, kPools = 4 };
const int kMacSizes[kPools] = {0x30, 0x3c, 0x18, 0x30};

// The Mac size of a host size (see the top of the file).
std::size_t MacSize(std::size_t size) {
    if (size == sizeof(_TbSoundSampleWAV)) return 0x30;
    if (size == sizeof(_TbSoundSampleWAVFile)) return 0x3c;
    if (size == sizeof(_TbSoundCodecLoop)) return 0x18;
    if (size == sizeof(TbTimerCodec)) return 0x30;
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

// Blocks the runtime frees are kept until the end of the case (see tpw_sc_end), so that no address is reused while a stale
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

// The members of the file class that nothing here calls but the fake's vtable needs (the library's own are imports).
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

namespace {

constexpr int kSlots = 6;
constexpr int kHeaders = 3;
constexpr int kHeaderBytes = 0x28 + 0x200;   // the file's 0x28-byte header, then the sample data
constexpr int kBufferBytes = 0x1000;
constexpr int kFileBytes = 0x400;
constexpr uint32_t kFill = 0xA5A5A5A5u;
enum Kind { kNone = 0, kWAV = 1, kWAVFile = 2, kLoop = 3, kTimer = 4, kFake = 5 };

// ---- the data -------------------------------------------------------------------------------------------------------------

alignas(16) uint8_t gHeaders[kHeaders][kHeaderBytes];
alignas(16) uint8_t gBuffer[kBufferBytes];
uint8_t gFileData[kFileBytes];

// The pattern of the sample data of header h (byte i after the 0x28 header bytes) and of the file (byte i).
uint8_t DataByte(int header, int index) {
    return static_cast<uint8_t>(index * 13 + header * 29 + 7);
}

uint8_t FileByte(int index) {
    return static_cast<uint8_t>(index * 5 + 1);
}

// The offset of a pointer in the output buffer, or -1 when it is not in it.
int64_t BufferOffset(const void* pointer) {
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    if (p >= gBuffer && p < gBuffer + kBufferBytes) return p - gBuffer;
    return -1;
}

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

// ---- the file -------------------------------------------------------------------------------------------------------------

int64_t gFilePosition;

// The open file of a WAVFile codec. Read copies what is left of the file (capped by the queue "Read": a value caps the count,
// -1 fails) into the output buffer; Seek answers the new position (the queue "Seek": -1 makes it fail); GetPos answers the
// position, or the next value of the queue "Pos" when there is one.
class FakeFile : public CFile {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("File.Read", {size, BufferOffset(dest)});
        const int64_t mode = ret("Read", -2);
        if (mode == -1) return -1;
        uint32_t wanted = size;
        if (mode >= 0 && static_cast<uint64_t>(mode) < wanted) wanted = static_cast<uint32_t>(mode);
        const int64_t left = gFilePosition < 0 ? 0 : static_cast<int64_t>(kFileBytes) - gFilePosition;
        const int64_t offset = BufferOffset(dest);
        const int64_t space = offset < 0 ? 0 : kBufferBytes - offset;
        int64_t count = wanted;
        if (count > left) count = left > 0 ? left : 0;
        if (count > space) count = space;
        if (count > 0) std::memcpy(dest, gFileData + gFilePosition, static_cast<std::size_t>(count));
        gFilePosition += count;
        return static_cast<int32_t>(count);
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("File.Seek", {offset, whence});
        if (ret("Seek", 0) == -1) return -1;
        int64_t target;
        switch (whence) {
            case 0: target = offset; break;
            case 1: target = gFilePosition + static_cast<int32_t>(offset); break;
            case 2: target = static_cast<int64_t>(kFileBytes) + static_cast<int32_t>(offset); break;
            default: return -1;
        }
        if (target < 0) return -1;
        gFilePosition = target;
        return static_cast<int32_t>(target);
    }
    int32_t GetPos() override {
        ev("File.GetPos");
        const int64_t scripted = ret("Pos", INT64_MIN);
        if (scripted != INT64_MIN) return static_cast<int32_t>(scripted);
        return static_cast<int32_t>(gFilePosition);
    }
};

FakeFile gFile;

// ---- the owners ---------------------------------------------------------------------------------------------------------------

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

TbSoundSamplePipe* gNodes[kSlots];
int gKinds[kSlots];
void* gBlocks[8];
int64_t gBlockSizes[8];   // the Mac sizes the raw blocks were asked for
int gBlockPools[8];       // the class whose operator new made them
int32_t gDtorFlag = 1;    // the flag the destructor call carries (a `delete this` of the class code is flag 1)

TbSoundSamplePipe* NodeBySlot(int64_t slot) {
    return slot >= 0 && slot < kSlots ? gNodes[slot] : nullptr;
}

// A data pointer named by a number: 0 is NULL, 0x10000 * h + offset is header h - 1's data (the bytes after its 0x28) + offset.
const uint8_t* DataPointer(int64_t value) {
    if (value == 0) return nullptr;
    const int header = static_cast<int>(value >> 16) - 1;
    if (header < 0 || header >= kHeaders) return nullptr;
    return gHeaders[header] + 0x28 + (value & 0xFFFF);
}

// Where a data start pointer points: 0 NULL, 0x10000 times the header number plus the offset from the header's start, or the
// low word.
int64_t DataStartWord(const uint8_t* pointer) {
    if (pointer == nullptr) return 0;
    for (int i = 0; i < kHeaders; ++i) {
        if (pointer >= gHeaders[i] && pointer < gHeaders[i] + kHeaderBytes) return 0x10000 * (i + 1) + (pointer - gHeaders[i]);
    }
    return Low32(pointer);
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

// ---- the four pools, as a table -----------------------------------------------------------------------------------------------

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
    POOL_ENTRY(_TbSoundSampleWAV, mFirst_TbSoundSampleWAV, mCurrent_TbSoundSampleWAV, mLow_TbSoundSampleWAV,
               mHigh_TbSoundSampleWAV, mTotal_TbSoundSampleWAV, mLevel_TbSoundSampleWAV, mWAVPoolLink),
    POOL_ENTRY(_TbSoundSampleWAVFile, mFirst_TbSoundSampleWAVFile, mCurrent_TbSoundSampleWAVFile, mLow_TbSoundSampleWAVFile,
               mHigh_TbSoundSampleWAVFile, mTotal_TbSoundSampleWAVFile, mLevel_TbSoundSampleWAVFile, mWAVFilePoolLink),
    POOL_ENTRY(_TbSoundCodecLoop, mFirst_TbSoundCodecLoop, mCurrent_TbSoundCodecLoop, mLow_TbSoundCodecLoop,
               mHigh_TbSoundCodecLoop, mTotal_TbSoundCodecLoop, mLevel_TbSoundCodecLoop, mLoopPoolLink),
    POOL_ENTRY(TbTimerCodec, mFirstTbTimerCodec, mCurrentTbTimerCodec, mLowTbTimerCodec, mHighTbTimerCodec,
               mTotalTbTimerCodec, mLevelTbTimerCodec, mTimerPoolLink),
};

std::size_t HostSize(int pool, int64_t macSize) {
    if (macSize == kMacSizes[pool]) return kPoolTable[pool].hostSize;
    if (macSize == 0) return 0;
    return static_cast<std::size_t>(macSize) + kLiteral;
}

// The pool a node of this kind belongs to, or -1 (the FakeNode is the runtime's).
int PoolOfKind(int kind) {
    switch (kind) {
        case kWAV: return kPoolWAV;
        case kWAVFile: return kPoolWAVFile;
        case kLoop: return kPoolLoop;
        case kTimer: return kPoolTimer;
        default: return -1;
    }
}

bool PooledIn(int pool, const void* object) {
    int steps = 0;
    for (void* idle = *kPoolTable[pool].first; idle != nullptr && steps < 64; idle = kPoolTable[pool].link(idle), ++steps) {
        if (idle == object) return true;
    }
    return false;
}

bool Alive(int slot) {
    if (gNodes[slot] == nullptr || lab(gNodes[slot]) <= 0) return false;
    const int pool = PoolOfKind(gKinds[slot]);
    return pool < 0 || !PooledIn(pool, gNodes[slot]);
}

// ---- the node that records every slot ------------------------------------------------------------------------------------------

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
        return nullptr;
    }
    void GetInfo(TbSoundSampleInfo& info) override { ev("Fake.GetInfo", {lab(this), lab(&info)}); }
    uint32_t GetSampleRate() override {
        ev("Fake.GetSampleRate", {lab(this)});
        return static_cast<uint32_t>(ret("Rate", 0));
    }
    // The codec behind a loop: the bytes it delivers and its answer come from the queues.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override {
        ev("Fake.GetDataChunk", {lab(this), BufferOffset(buffer), size, flag});
        done = static_cast<int32_t>(ret("Got", 0));
        return static_cast<int32_t>(ret("Chunk", 0));
    }
    void Restart() override { ev("Fake.Restart", {lab(this)}); }
    TbError SetInfo(const TbSoundSampleInfo& info) override {
        ev("Fake.SetInfo", {lab(this), lab(&info)});
        return TbError(0);
    }
    void* SetHandle(void* handle) override {
        ev("Fake.SetHandle", {lab(this), static_cast<int64_t>(Low32(handle))});
        return handle;
    }
    void Completed(bool done) override { ev("Fake.Completed", {lab(this), done}); }
    void SetDataStart(void* start) override { ev("Fake.SetDataStart", {lab(this), DataStartWord(static_cast<const uint8_t*>(start))}); }
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

// ---- building and dumping nodes ---------------------------------------------------------------------------------------------

TbSampleHeader* HeaderPointer(int64_t index) {
    return reinterpret_cast<TbSampleHeader*>(gHeaders[index >= 0 && index < kHeaders ? index : 0]);
}

// kind; a the header (WAV, WAV file) or the format byte (timer); b the owner (WAV, WAV file) or the milliseconds (timer);
// c the file offset of the data (WAV file).
TbSoundSamplePipe* MakeNode(int kind, int64_t a, int64_t b, int64_t c) {
    switch (kind) {
        case kWAV: return new _TbSoundSampleWAV(HeaderPointer(a), &gOwners[b & 1]);
        case kWAVFile: return new _TbSoundSampleWAVFile(&gFile, HeaderPointer(a), static_cast<uint32_t>(c), &gOwners[b & 1]);
        case kLoop: return new _TbSoundCodecLoop();
        case kTimer: return new TbTimerCodec(static_cast<uint8_t>(a), static_cast<uint32_t>(b));
        default: return new FakeNode();
    }
}

TbSoundSampleCodec* AsCodec(int slot) {
    return static_cast<TbSoundSampleCodec*>(NodeBySlot(slot));
}

// The link word of a node of this kind (its class's pool link), and the one of the class below (the WAV codec's, in a WAV file).
const void* OwnLink(int kind, const TbSoundSamplePipe* p) {
    switch (kind) {
        case kWAV: return static_cast<const _TbSoundSampleWAV*>(p)->mWAVPoolLink;
        case kWAVFile: return static_cast<const _TbSoundSampleWAVFile*>(p)->mWAVFilePoolLink;
        case kLoop: return static_cast<const _TbSoundCodecLoop*>(p)->mLoopPoolLink;
        case kTimer: return static_cast<const TbTimerCodec*>(p)->mTimerPoolLink;
        default: return nullptr;
    }
}

}  // namespace

extern "C" {

// Starts a case: the fixtures and their labels, the statics reset (the labels and queues were set by calllog's own entry).
void tpw_sc_begin() {
    gAllocatedCount = 0;
    std::memset(gNodes, 0, sizeof(gNodes));
    std::memset(gKinds, 0, sizeof(gKinds));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gBlockSizes, 0, sizeof(gBlockSizes));
    std::memset(gBlockPools, 0, sizeof(gBlockPools));
    std::memset(gHeaders, 0xA5, sizeof(gHeaders));
    std::memset(gBuffer, 0xA5, sizeof(gBuffer));
    for (int h = 0; h < kHeaders; ++h) {
        for (int i = 0; i < 0x200; ++i) gHeaders[h][0x28 + i] = DataByte(h, i);
    }
    for (int i = 0; i < kFileBytes; ++i) gFileData[i] = FileByte(i);
    gFilePosition = 0;
    gDtorFlag = 1;
    tpw_cl_label(1, &gOwners[0]);
    tpw_cl_label(2, &gOwners[1]);
    tpw_cl_label(4, gHeaders[0]);
    tpw_cl_label(5, gHeaders[1]);
    tpw_cl_label(6, gHeaders[2]);
    tpw_cl_label(7, &gFile);
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
void tpw_sc_end() {
    tpw_cl_on = 0;
    for (int i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] != nullptr) std::free(gAllocated[i]);
    }
    gAllocatedCount = 0;
    for (int p = 0; p < kPools; ++p) *kPoolTable[p].first = nullptr;
}

int32_t tpw_sc_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundSampleCodec));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(TbSampleHeader));
    }
}

// A sample header (the Mac's first eight bytes: size, rate, bits, flags), in the host's byte order.
void tpw_sc_set_header(int32_t index, uint32_t size, uint32_t rate, uint32_t bits, uint32_t flags) {
    TbSampleHeader* header = reinterpret_cast<TbSampleHeader*>(gHeaders[index]);
    header->mSize = static_cast<int32_t>(size);
    header->mRate = static_cast<uint16_t>(rate);
    header->mBits = static_cast<uint8_t>(bits);
    header->mFlags = static_cast<uint8_t>(flags);
}

void tpw_sc_pool_set(int32_t pool, int32_t current, int32_t low, int32_t high, int32_t total, int32_t level) {
    *kPoolTable[pool].current = current;
    *kPoolTable[pool].low = low;
    *kPoolTable[pool].high = high;
    *kPoolTable[pool].total = total;
    *kPoolTable[pool].level = level;
}

// One pool: the six statics and the chain of idle objects (labels; at most 12 shown, then -9).
void tpw_sc_pool(int32_t pool, int64_t* out) {
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
void tpw_sc_buffer(uint8_t* out) {
    std::memcpy(out, gBuffer, kBufferBytes);
}

// One slot: [status (0 empty, 1 gone, 2 alive), kind, next, previous, handle, base link class, owner, size, data start, time,
// position, format, own link class, the WAV codec's link class (a WAV file), file, file start].
void tpw_sc_slot(int32_t slot, int64_t* out) {
    std::memset(out, 0, 16 * sizeof(int64_t));
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
    if (kind != kLoop) {
        TbSoundSampleCodec* c = static_cast<TbSoundSampleCodec*>(p);
        out[6] = PW(c->mRefManager.mOwner);
        out[7] = c->mDataSize;
        out[8] = DataStartWord(c->mDataStart);
        out[9] = c->mTime;
        out[10] = c->mDataPosition;
        out[11] = c->mFormat;
    }
    out[12] = LinkClass(OwnLink(kind, p));
    if (kind == kWAVFile) {
        _TbSoundSampleWAVFile* f = static_cast<_TbSoundSampleWAVFile*>(p);
        out[13] = LinkClass(f->mWAVPoolLink);
        out[14] = PW(f->mFile);
        out[15] = f->mStart;
    }
}

// One raw block of the pool operations: [status (0 none, 1 gone, 2 alive, 3 idle in its pool), then, for a block that is a whole
// object of the class, mNext, mPrevious, mHandle, the base link class and the own link class].
void tpw_sc_block(int32_t index, int64_t* out) {
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
        out[5] = LinkClass(kPoolTable[pool].link(block));
    }
}

// The operations, in the order of OPS in tests/test_SoundCodecs.py (the test checks the count).
enum Op {
    OP_NEW, OP_DTOR, OP_GET_CHUNK, OP_RESTART, OP_SET_DATA_START, OP_ATTATCH, OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL,
    OP_CLEARCACHE, OP_POKE, OP_FILE_POS, OP_COPY, OP_COUNT
};

int32_t tpw_sc_op_count() { return OP_COUNT; }

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
int64_t tpw_sc_aux(int32_t index) { return gAux[index]; }

// A pointer result as the test compares it: the label of a node, 0 for NULL.
static int64_t Label(const void* pointer) {
    return pointer == nullptr ? 0 : lab(pointer);
}

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);

// One operation. Results come back as a 64-bit integer (labels for pointers); -99 means "not applicable" on both sides.
int64_t tpw_sc_op(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    const int64_t result = DoOp(op, a, b, c, d, e);
    Sweep();
    return result;
}

// The kind of the node a pointer names (kNone when it is none of the case's).
static int KindOfNode(const TbSoundSamplePipe* node) {
    for (int i = 0; i < kSlots; ++i) {
        if (node != nullptr && gNodes[i] == node) return gKinds[i];
    }
    return kNone;
}

// Whether the codec behind a loop can be driven without a fault or an endless loop: a WAV codec needs its data start at the
// start of a header's data, a size of 1 to 0x1e0, no timer running and a position in range; a WAV file codec the same
// without the data start; a timer or the fake need nothing.
static bool LoopTargetSafe(const TbSoundSamplePipe* next) {
    const int kind = KindOfNode(next);
    if (kind == kFake || kind == kTimer) return true;
    if (kind != kWAV && kind != kWAVFile) return false;
    const TbSoundSampleCodec* codec = static_cast<const TbSoundSampleCodec*>(next);
    if (codec->mTime != 0 || codec->mDataSize == 0 || codec->mDataSize > 0x1e0 || codec->mDataPosition > codec->mDataSize) return false;
    if (kind == kWAVFile) return true;
    for (int h = 0; h < kHeaders; ++h) {
        if (codec->mDataStart == gHeaders[h] + 0x28) return true;
    }
    return false;
}

// Whether a GetDataChunk at this slot copies a count that fits the buffers (the Mac and the host would both fault on a negative
// count or one beyond them); the test skips those operations on both sides.
static bool ChunkSafe(int kind, int slot, int64_t offset, int64_t size) {
    if (offset < 0 || offset > 0x800) return false;
    TbSoundSampleCodec* codec = AsCodec(slot);
    if (kind == kWAV) {
        if (codec->mTime != 0) return true;
        const int32_t left = static_cast<int32_t>(codec->mDataSize - codec->mDataPosition);
        const int32_t count = size < left ? static_cast<int32_t>(size) : left;
        if (count < 0 || offset + count > kBufferBytes) return false;
        const uint8_t* source = codec->mDataStart + codec->mDataPosition;
        const uint8_t* end = source + count;
        for (int h = 0; h < kHeaders; ++h) {
            if (source >= gHeaders[h] && end <= gHeaders[h] + kHeaderBytes) return true;
        }
        return count == 0;
    }
    if (kind == kTimer) return size >= 0 && offset + size <= kBufferBytes;
    if (kind == kLoop) {
        if (size < 0) return true;   // the signed loop test fails at once: no round, no call, no write
        if (offset + size > kBufferBytes) return false;
        return LoopTargetSafe(gNodes[slot]->mNext);
    }
    return true;
}

// Whether Restart can be called on this slot: a loop or a timer forwards it to the codec behind (which must be there and be
// a WAV, a WAV file or the fake).
static bool RestartSafe(int kind, int slot) {
    if (kind != kLoop && kind != kTimer) return true;
    const int next = KindOfNode(gNodes[slot]->mNext);
    return next == kWAV || next == kWAVFile || next == kFake;
}

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    gAux[0] = gAux[1] = 0;
    const bool needsNode = op == OP_DTOR || op == OP_GET_CHUNK || op == OP_RESTART || op == OP_SET_DATA_START ||
                           op == OP_ATTATCH || op == OP_POKE;
    TbSoundSamplePipe* pipe = (a >= 0 && a < kSlots) ? gNodes[a] : nullptr;
    int kind = kNone;
    if (needsNode) {
        if (a < 0 || a >= kSlots || !Alive(static_cast<int>(a))) return -99;
        kind = gKinds[a];
        if ((op == OP_GET_CHUNK || op == OP_RESTART) && kind == kNone) return -99;
        if (op == OP_GET_CHUNK && !ChunkSafe(kind, static_cast<int>(a), b, c)) return -99;
        if (op == OP_RESTART && !RestartSafe(kind, static_cast<int>(a))) return -99;
        if (op == OP_POKE && b >= 1 && kind == kLoop) return -99;
        if (op == OP_POKE && b == 6 && kind != kWAVFile) return -99;
    }
    switch (op) {
        case OP_NEW: {
            if (a < 0 || a >= kSlots || b < kWAV || b > kFake) return -99;
            TbSoundSamplePipe* made = MakeNode(static_cast<int>(b), c, d, e);
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
        case OP_GET_CHUNK: {
            // b the offset in the output buffer, c the size, d the stopped flag; the result of the virtual call
            int32_t done = 0x5555;
            const int32_t result = pipe->GetDataChunk(gBuffer + b, static_cast<int32_t>(c), done, d != 0);
            gAux[0] = done;
            return result;
        }
        case OP_RESTART:
            pipe->Restart();
            return 0;
        case OP_SET_DATA_START:
            pipe->SetDataStart(const_cast<uint8_t*>(DataPointer(b)));
            return 0;
        case OP_ATTATCH:
            if (b >= 0 && (b >= kSlots || !Alive(static_cast<int>(b)))) return -99;
            return Label(pipe->Attatch(NodeBySlot(b)));
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
        case OP_POKE: {
            // a slot, b the field, c the value
            TbSoundSampleCodec* codec = static_cast<TbSoundSampleCodec*>(pipe);
            switch (b) {
                case 0: pipe->mHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(c))); break;
                case 1: codec->mFormat = static_cast<uint8_t>(c); break;
                case 2: codec->mTime = static_cast<uint32_t>(c); break;
                case 3: codec->mDataPosition = static_cast<uint32_t>(c); break;
                case 4: codec->mDataSize = static_cast<uint32_t>(c); break;
                case 5: codec->mDataStart = DataPointer(c); break;
                case 6: static_cast<_TbSoundSampleWAVFile*>(pipe)->mStart = static_cast<uint32_t>(c); break;
                default: break;
            }
            return 0;
        }
        case OP_FILE_POS:
            gFilePosition = a;
            return 0;
        case OP_COPY: {
            // a the offset in the output buffer, b the offset in the file bytes, c the count
            if (a < 0 || a > 0x800 || b < 0 || b > 0x200 || c < 0 || c > 0x200) return -99;
            CMemoryCopy copier;
            copier.Copy(gBuffer + a, gFileData + b, static_cast<uint32_t>(c));
            return 0;
        }
        default:
            return -98;
    }
}

}  // extern "C"
