// Fakes and test entry point for unit w79c, the sound library's pipe channel and sample store (libtpw_unit_SoundChannel.dylib,
// driven by tests/test_SoundChannel.py in the program "sound symbols shared"; see calllog.h):
//   src/TbSoundPipeChannel.cpp   TbSoundPipeChannel
//   src/TbSoundSampleStore.cpp   TbSoundSampleStore, CChannelIterator, CUsedChannelIterator, CAutoArrayPtr<TbSoundPipeChannel>
// Real (linked): the pipe base (src/TbSoundPipe.cpp), the sample info records (src/TbSoundInfo.cpp), the handle and critical
// section values (src/snd_handles.cpp, included here to reach its cached mask). Fakes, each recording what the original's
// callee would be given (the PowerPC side records the same from its stubs):
//   FakePipe, a TbSoundSamplePipe that records every virtual slot (the codec behind a channel); its answers come from queues
//       (Chunk, Done, SetInfoErr, FindPipe, Attatch), its destructor is silent and nothing deletes it
//   TbSoundCore::StopAndReleasePipe (unit w79b), CCodecFactory (unit w80b: constructor, destructor, CreateCodec, CreateController)
//   CvtDblUl (the runtime's double to unsigned long, unit w72), memset (the length can be a huge unsigned count: the fake records
//       destination, byte and length and writes only inside the case's buffers) and the runtime heap (below)
// Heap: operator new and delete record "new" / "new[]" with the Mac's size and a label (blocks are filled with 0xA5, so the words
// a constructor leaves alone are compared) and "delete" for a labelled block; the blocks live until the end of the case. The
// service buffer's 0x1220-byte allocation is returned at an address whose low five bits the case chooses ("skews"), so that the
// 32-byte alignment of the aligned buffer is checked for every value; the channel array is sized in Mac bytes (count * 0x60 + 8).
//
// Only the target channel's service buffer is a region (a pointer near another channel's block would be named by the heap's layout).
// Labels: 1 the core, 2 the store (or the array holder), 3 the sample info, 4 and 5 the two volume controls the info can point
// at, 6 the data buffer, 7 the system info, 8 and 9 the two float arrays of the channels' own volume controls, 10 + i the
// channels, 20 + j the fake pipes, 30 the array block, 40 + i the service buffers of the channels, 100 and up what the case
// allocates. A case is a flat array of 64-bit words (see tpw_sc_run); the answer is another, and the data buffer's bytes.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CCodecFactory.h"
#include "TbSoundBank.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "calllog.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

namespace {

constexpr uint32_t kFill = 0xA5A5A5A5u;
constexpr int kMaxAllocs = 4096;
constexpr int kDataBytes = 0x6000;   // the data buffer: the region proper is the middle 0x2000, the rest is slack
constexpr int kDataStart = 0x2000;
constexpr int kDataSize = 0x2000;

bool gSetup;              // while the world is built nothing is recorded or labelled
int gSkewCount;
int64_t gSkews[8];
int gSkewNext;
bool gSkewActive;
bool gShowDone;           // set around the StreamerCallback op: the pipe fake then also logs the done word it is handed
int64_t gMacArrayBytes;   // the Mac size of the channel array allocation of the case
void* gArrayRaw;          // the block the channel array was allocated in (set while building)
void* gAllocPtr[kMaxAllocs];
void* gAllocReal[kMaxAllocs];
int gAllocCount;

}  // namespace

// The runtime's malloc for the case: every block is remembered (and freed at the end), the service buffer's block gets the
// skew of the case. (The unit's calllog.cpp is compiled below with malloc turned into this.)
namespace std {
void* SkewMalloc(size_t size) {
    void* real;
    void* given;
    if (size == 0x1220 && gSkewActive) {
        const int64_t skew = gSkewNext < gSkewCount ? gSkews[gSkewNext] : 0;
        ++gSkewNext;
        real = ::malloc(size + 96);
        const uintptr_t base = (reinterpret_cast<uintptr_t>(real) + 31) & ~static_cast<uintptr_t>(31);
        given = reinterpret_cast<void*>(base + static_cast<uintptr_t>(skew & 31));
    } else {
        real = ::malloc(size);
        given = real;
    }
    if (gSkewActive && gAllocCount < kMaxAllocs) {
        gAllocPtr[gAllocCount] = given;
        gAllocReal[gAllocCount] = real;
        ++gAllocCount;
    }
    return given;
}
}  // namespace std

namespace {

void SoundAllocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (gSetup) {
        if (array && size != 0x1220) gArrayRaw = block;
        return;
    }
    if (!calllog::on()) return;
    const int64_t macSize = array && size != 0x1220 ? gMacArrayBytes : static_cast<int64_t>(size);
    calllog::ev(array ? "new[]" : "new", {macSize, calllog::newlab(block)});
}

// Freed blocks are kept until the end of the case (the address is never reused while a stale pointer is compared).
bool KeepFreed(void*, bool) {
    return true;
}

}  // namespace
#define TPW_ALLOCATION_OBSERVER(size, array, p) SoundAllocated(size, array, p)
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#define malloc(n) SkewMalloc(n)
#include "calllog.cpp"
#undef malloc

#include <cstdio>

// ---- the regions the memset fake may write into -------------------------------------------------------------------------------

namespace {

struct Region {
    int label;
    uintptr_t start;
    int64_t size;
};
Region gRegions[16];
int gRegionCount;

void AddRegion(int label, const void* start, int64_t size) {
    gRegions[gRegionCount++] = Region{label, reinterpret_cast<uintptr_t>(start), size};
}

// The channels' lock words (the first eight): every recorded callee event ends with their sum, so that a call made outside the
// lock (or a lock not released) shows in the log.
uint32_t* gLockWords[8];
int gLockCount;

int64_t Lock() {
    uint32_t sum = 0;
    for (int i = 0; i < gLockCount; ++i) sum += *gLockWords[i];
    return sum;
}

// A pointer into a region as the test compares it: the region's label times a million plus the offset (up to 0x2000 outside is
// still named), -1 for anything else.
int64_t Ref(const void* pointer) {
    const int64_t address = static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer));
    for (int i = 0; i < gRegionCount; ++i) {
        const int64_t offset = address - static_cast<int64_t>(gRegions[i].start);
        if (offset >= 0 && offset < gRegions[i].size) return gRegions[i].label * 1000000LL + offset;
    }
    for (int i = 0; i < gRegionCount; ++i) {
        const int64_t offset = address - static_cast<int64_t>(gRegions[i].start);
        if (offset >= -0x2000 && offset < gRegions[i].size + 0x2000) return gRegions[i].label * 1000000LL + offset;
    }
    return -1;
}

}  // namespace

// The memset import (the translated sources call it unqualified; the macro below points them here).
static void* SoundMemset(void* destination, int value, size_t count) {
    calllog::ev("memset", {Ref(destination), value & 0xff, static_cast<int64_t>(count), Lock()});
    const int64_t address = static_cast<int64_t>(reinterpret_cast<uintptr_t>(destination));
    for (int i = 0; i < gRegionCount; ++i) {
        const int64_t offset = address - static_cast<int64_t>(gRegions[i].start);
        if (offset >= 0 && static_cast<int64_t>(count) <= gRegions[i].size - offset) {
            std::memset(destination, value, count);
            return destination;
        }
    }
    return destination;
}
#define memset SoundMemset

// ---- the callees outside the unit ---------------------------------------------------------------------------------------------

namespace {

int64_t Low32(const void* pointer) {
    return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)));
}

// A pointer field as the test compares it: 0, the label of the object, the fill word, or -1.
int64_t PW(const void* pointer) {
    if (pointer == nullptr) return 0;
    const int32_t label = calllog::lab(pointer);
    if (label > 0) return label;
    if (Low32(pointer) == kFill) return kFill;
    return -1;
}

uint32_t RealCvt(double value) {
    if (value < 0.0) return 0;
    if (!(value < 4294967296.0)) return 0xffffffffu;
    const bool low = value < 2147483648.0;
    const double part = low ? value : value - 2147483648.0;
    const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(part));
    return low ? word : word + 0x80000000u;
}

int64_t DoubleBits(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    return bits;
}

TbSoundSamplePipe* gPipeObjects[4];
int gPipeCount;

TbSoundSamplePipe* Node(int64_t index) {
    return index >= 0 && index < gPipeCount ? gPipeObjects[index] : nullptr;
}

// The codec behind a channel: records every slot; the queue values name a fake pipe (-1 NULL).
class FakePipe : public TbSoundSamplePipe {
public:
    ~FakePipe() override {}
    TbSoundSamplePipe* Attatch(TbSoundSamplePipe* pipe) override {
        calllog::ev("Pipe.Attatch", {calllog::lab(this), calllog::lab(pipe), Lock()});
        return Node(calllog::ret("Attatch", -1));
    }
    void Release() override { calllog::ev("Pipe.Release", {calllog::lab(this), Lock()}); }
    void Detatch() override { calllog::ev("Pipe.Detatch", {calllog::lab(this), Lock()}); }
    TbSoundSamplePipe* FindPipe(void* handle) override {
        calllog::ev("Pipe.FindPipe", {calllog::lab(this), Low32(handle), Lock()});
        const int64_t value = calllog::ret("FindPipe", -1);
        if (value == -2) return this;
        return Node(value);
    }
    void GetInfo(TbSoundSampleInfo& info) override { calllog::ev("Pipe.GetInfo", {calllog::lab(this), calllog::lab(&info), Lock()}); }
    uint32_t GetSampleRate() override {
        calllog::ev("Pipe.GetSampleRate", {calllog::lab(this), Lock()});
        return 0;
    }
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override {
        calllog::ev("Pipe.GetDataChunk", {calllog::lab(this), Ref(buffer), size, flag, Lock()});
        // the done word the caller hands in (only for the StreamerCallback op: elsewhere it is the caller's uninitialised stack)
        if (gShowDone) calllog::ev("Pipe.GetDataChunk.done", {calllog::lab(this), static_cast<uint32_t>(done), Lock()});
        done = static_cast<int32_t>(calllog::ret("Done", 0));
        return static_cast<int32_t>(calllog::ret("Chunk", 0));
    }
    void Restart() override { calllog::ev("Pipe.Restart", {calllog::lab(this), Lock()}); }
    TbError SetInfo(const TbSoundSampleInfo& info) override {
        calllog::ev("Pipe.SetInfo", {calllog::lab(this), calllog::lab(&info), Lock()});
        return TbError(static_cast<int32_t>(calllog::ret("SetInfoErr", 0)));
    }
    void* SetHandle(void* handle) override {
        calllog::ev("Pipe.SetHandle", {calllog::lab(this), Low32(handle), Lock()});
        return handle;
    }
    void Completed(bool done) override { calllog::ev("Pipe.Completed", {calllog::lab(this), done, Lock()}); }
    void SetDataStart(void* start) override { calllog::ev("Pipe.SetDataStart", {calllog::lab(this), Low32(start), Lock()}); }
};

}  // namespace

// The core's stop (unit w79b): recorded with the core, the channel and the flag; the answer comes from the queue "StopErr".
TbError TbSoundCore::StopAndReleasePipe(TbSoundPipeChannel* channel, bool flag) {
    calllog::ev("Core.StopAndReleasePipe", {calllog::lab(this), calllog::lab(channel), flag, Lock()});
    return TbError(static_cast<int32_t>(calllog::ret("StopErr", 0)));
}

// The codec factory (unit w80b): a stack temporary; the pipes it makes are the queues' fake pipes.
CCodecFactory::CCodecFactory() { calllog::ev("Factory.ctor", {Lock()}); }
CCodecFactory::~CCodecFactory() { calllog::ev("Factory.dtor", {Lock()}); }
TbSoundSamplePipe* CCodecFactory::CreateCodec(TbSoundPipeChannel* channel, TbSoundSampleInfo& info) {
    calllog::ev("Factory.CreateCodec", {calllog::lab(channel), calllog::lab(&info), Lock()});
    return Node(calllog::ret("Codec", -1));
}
TbSoundSamplePipe* CCodecFactory::CreateController(TbSoundSampleInfo& info) {
    calllog::ev("Factory.CreateController", {calllog::lab(&info), Lock()});
    return Node(calllog::ret("Control", -1));
}

// The clock the pipe base asks for (only the codecs read it; no case reaches it).
#include "LbTime.h"
uint32_t LbTime_GetClock() {
    calllog::ev("Clock");
    return 0;
}

// The runtime's double to unsigned long (unit w72): recorded with the double's bits.
uint32_t CvtDblUl(double value) {
    calllog::ev("CvtDblUl", {DoubleBits(value), Lock()});
    return RealCvt(value);
}

#include "../src/TbSoundPipeChannel.cpp"
#include "../src/TbSoundSampleStore.cpp"
#include "../src/snd_handles.cpp"
#undef memset

// ---- the case ----------------------------------------------------------------------------------------------------------------

namespace {

// The case's words (the order is tests/test_SoundChannel.py's SPEC_*).
enum {
    S_OP = 0, S_M = 1, S_USED = 2, S_TARGET = 3, S_PIPES = 4, S_EXTRA = 5, S_NSKEW = 6, S_SKEW = 7, S_A = 16, S_INFO = 24,
    S_VOL = 40, S_SYS = 50, S_GLOBALS = 51, S_CH = 64, S_CHW = 24, S_PIPE = 256, S_SIZE = 272
};
// The answer's words.
enum { O_RET = 0, O_SLOT = 1, O_PIPE = 5, O_CHANNEL = 6, O_SEQ = 7, O_DUMP = 16, O_STORE = 304, O_PIPES = 310, O_INFO = 326, O_STATE = 342 };
constexpr int kDumpWords = 32;

alignas(16) unsigned char gData[kDataBytes];
alignas(16) unsigned char gInfoBytes[sizeof(TbSoundSampleInfo) + 16];
alignas(16) unsigned char gSysBytes[sizeof(TbSoundSystemInfo) + 16];
alignas(16) unsigned char gPipeBytes[4][sizeof(FakePipe)];
alignas(16) unsigned char gChannelBytes[sizeof(TbSoundPipeChannel)];
alignas(16) unsigned char gStoreBytes[sizeof(TbSoundSampleStore)];
alignas(16) unsigned char gIterBytes[sizeof(TbSoundSampleStore::CUsedChannelIterator)];
alignas(16) unsigned char gHolderBytes[sizeof(CAutoArrayPtr<TbSoundPipeChannel>)];
// The core is only an address the store keeps and the fake StopAndReleasePipe labels. The real TbSoundCore (w79b) has a
// constructor and a vtable this unit does not link, so it is raw storage here.
alignas(16) unsigned char gCoreBytes[sizeof(TbSoundCore)];
TbSoundCore* const gCore = reinterpret_cast<TbSoundCore*>(gCoreBytes);
float gFloatsA[4];
float gFloatsB[4];
TbSoundVolumeControl gControlA;
TbSoundVolumeControl gControlB;

// The class of a pointer into the case's volume tables: 0 NULL, 1 the first, 2 the second, the fill word, -1.
int64_t VolClass(const void* pointer, const void* first, const void* second) {
    if (pointer == nullptr) return 0;
    if (pointer == first) return 1;
    if (pointer == second) return 2;
    if (Low32(pointer) == kFill) return kFill;
    return -1;
}

int64_t LinkClass(const void* pointer) {
    if (pointer == nullptr) return 0;
    return Low32(pointer) == kFill ? 1 : 2;
}

// One channel as the test compares it: every word of the Mac layout (pointers as labels or classes).
void DumpChannel(const TbSoundPipeChannel* c, int64_t* o) {
    std::memset(o, 0, sizeof(int64_t) * kDumpWords);
    o[0] = PW(c->mNext);
    o[1] = PW(c->mPrevious);
    o[2] = Low32(c->TbSoundSamplePipe::mHandle);
    o[3] = LinkClass(c->mPoolLink);
    o[4] = c->mInUse;
    o[5] = c->mVolume;
    o[6] = c->mSampleFlags;
    o[7] = c->mFlags;
    o[8] = c->mBufferSize;
    o[9] = c->mControlMask;
    o[10] = c->mField2C;
    o[11] = c->mHandle.mValue;
    o[12] = c->mServiceResult;
    o[13] = VolClass(c->mVolumeControl.mVolumes, gFloatsA, gFloatsB);
    o[14] = c->mVolumeControl.mCount;
    o[15] = static_cast<uint32_t>(c->mSampleHeader.mSize);
    o[16] = (static_cast<uint32_t>(c->mSampleHeader.mRate) << 16) | (static_cast<uint32_t>(c->mSampleHeader.mBits) << 8) |
            c->mSampleHeader.mFlags;
    o[17] = static_cast<uint32_t>(c->mSampleHeader.mField8);
    o[18] = c->mSampleHeader.mFieldC;
    o[19] = Low32(c->mSampleHeader.mData);
    const unsigned char* data = c->mServiceBuffer.mData;
    o[20] = PW(data);
    const bool dataSet = data != nullptr && Low32(data) != kFill;
    if (c->mAlignedBuffer == nullptr) o[21] = 0;
    else if (Low32(c->mAlignedBuffer) == kFill) o[21] = kFill;
    else if (!dataSet) o[21] = -1;
    else o[21] = static_cast<int64_t>(c->mAlignedBuffer - data);
    o[22] = dataSet ? static_cast<int64_t>(reinterpret_cast<uintptr_t>(data) & 31) : -1;
    o[23] = c->mLock.mValue;
}

void DumpStore(const TbSoundSampleStore* s, int64_t* o) {
    o[0] = PW(s->mCore);
    o[1] = s->mMaxChannels;
    o[2] = s->mChannelsToUse;
    o[3] = static_cast<TbSoundPipeChannel*>(s->mChannels) != nullptr ? 1 : 0;
}

void DumpInfo(const TbSoundSampleInfo* i, int64_t* o) {
    o[0] = i->mNumber;
    o[1] = i->mUserId;
    o[2] = i->mField0C;
    o[3] = i->mField10;
    o[4] = i->mFlags;
    o[5] = i->mField18;
    o[6] = i->mVolume;
    o[7] = i->mPriority;
    o[8] = VolClass(i->mVolumeControl, &gControlA, &gControlB);
    o[9] = i->mVolumeControlIndex;
    o[10] = i->mStreamingBufferSize;
    o[11] = i->mPan;
    o[12] = static_cast<uint32_t>(i->mPitch);
    o[13] = i->mLoopCount;
    o[14] = Low32(i->mBank);
    o[15] = calllog::fbits(i->mFrequency);
}

void* HandleArg(int64_t value) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

// The index of a channel of the store (-1 for NULL, -2 for a pointer outside the array).
int64_t ChannelIndex(const TbSoundSampleStore* store, const TbSoundPipeChannel* channel) {
    if (channel == nullptr) return -1;
    const TbSoundPipeChannel* first = static_cast<TbSoundPipeChannel*>(store->mChannels);
    if (channel < first) return -2;
    return channel - first;
}

}  // namespace

extern "C" {

int32_t tpw_sc_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundPipeChannel));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSampleStore));
        case 2: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(FakePipe));
    }
}

// The runtime's conversion, for the PowerPC side's stub of 0x1002e988 (the same function as the host's).
uint32_t tpw_sc_cvt(double value) {
    return RealCvt(value);
}

// Runs one case. sp is the case (S_SIZE words), out the answer (350 words) and data the data buffer's bytes after the case.
void tpw_sc_run(const int64_t* sp, int64_t* out, uint8_t* data) {
    std::memset(out, 0, sizeof(int64_t) * 350);
    const int op = static_cast<int>(sp[S_OP]);
    const int M = static_cast<int>(sp[S_M]);
    const int target = static_cast<int>(sp[S_TARGET]);
    const int64_t* a = sp + S_A;
    gSetup = true;
    gSkewActive = true;
    gAllocCount = 0;
    gSkewNext = 0;
    gSkewCount = static_cast<int>(sp[S_NSKEW]);
    for (int i = 0; i < 8; ++i) gSkews[i] = sp[S_SKEW + i];
    gMacArrayBytes = static_cast<int64_t>(M) * 0x60 + 8;
    gArrayRaw = nullptr;
    gRegionCount = 0;
    CChannelState::mChannelState.mCount.mValue = static_cast<int32_t>(sp[S_GLOBALS + 3]);
    gHandleMaskInitialized = static_cast<uint8_t>(sp[S_GLOBALS]);
    gHandleIndexMask = static_cast<uint32_t>(sp[S_GLOBALS + 1]);
    CUniqueHandle::_mBitsPerCount = static_cast<uint32_t>(sp[S_GLOBALS + 2]);

    // The fixtures: data buffer, sample info, system info, volume tables, fake pipes.
    std::memset(gData, 0xA5, sizeof gData);
    for (int i = 0; i < 4; ++i) {
        gFloatsA[i] = 0.0f;
        gFloatsB[i] = 0.0f;
        const uint32_t bitsA = static_cast<uint32_t>(sp[S_VOL + i]);
        const uint32_t bitsB = static_cast<uint32_t>(sp[S_VOL + 4 + i]);
        std::memcpy(&gFloatsA[i], &bitsA, 4);
        std::memcpy(&gFloatsB[i], &bitsB, 4);
    }
    gControlA.mVolumes = gFloatsA;
    gControlA.mCount = 4;
    gControlB.mVolumes = gFloatsB;
    gControlB.mCount = 2;
    std::memset(gInfoBytes, 0xA5, sizeof gInfoBytes);
    auto* info = reinterpret_cast<TbSoundSampleInfo*>(gInfoBytes);
    const int64_t* w = sp + S_INFO;
    info->mNumber = static_cast<uint32_t>(w[10]);
    info->mUserId = static_cast<uint32_t>(w[11]);
    info->mField0C = static_cast<uint32_t>(w[0]);
    info->mField10 = static_cast<uint32_t>(w[1]);
    info->mFlags = static_cast<uint32_t>(w[2]);
    info->mField18 = static_cast<uint32_t>(w[7]);
    info->mVolume = static_cast<uint32_t>(w[3]);
    info->mPriority = static_cast<uint32_t>(w[4]);
    info->mVolumeControl = w[5] == 1 ? &gControlA : w[5] == 2 ? &gControlB : nullptr;
    info->mVolumeControlIndex = static_cast<uint32_t>(w[6]);
    info->mStreamingBufferSize = static_cast<uint32_t>(w[8]);
    info->mPan = static_cast<uint32_t>(w[9]);
    std::memset(gSysBytes, 0xA5, sizeof gSysBytes);
    reinterpret_cast<TbSoundSystemInfo*>(gSysBytes)->mBuffersToUse = static_cast<uint32_t>(sp[S_SYS]);
    gPipeCount = static_cast<int>(sp[S_PIPES]);
    for (int j = 0; j < gPipeCount; ++j) {
        std::memset(gPipeBytes[j], 0xA5, sizeof gPipeBytes[j]);
        gPipeObjects[j] = ::new (gPipeBytes[j]) FakePipe;
    }

    const bool standaloneChannel = op == 0 || op == 1;
    const bool standaloneStore = op == 30;
    const bool holderOnly = op >= 49;
    TbSoundSampleStore* store = nullptr;
    TbSoundPipeChannel* alone = nullptr;
    CAutoArrayPtr<TbSoundPipeChannel>* holder = nullptr;
    if (op == 1) {
        alone = new TbSoundPipeChannel();
    } else if (op == 50 || op == 51) {
        holder = new CAutoArrayPtr<TbSoundPipeChannel>(static_cast<uint32_t>(a[1]));
        // op 50 with a[2] set: the holder's pointer is NULL (the array leaks), the destructor's NULL test is then taken
        if (op == 50 && a[2] != 0) holder->mData = nullptr;
    } else if (!standaloneChannel && !standaloneStore && !holderOnly) {
        store = new TbSoundSampleStore(gCore, static_cast<uint32_t>(M));
        store->mChannelsToUse = static_cast<uint32_t>(sp[S_USED]);
    }
    TbSoundPipeChannel* channels = store != nullptr ? static_cast<TbSoundPipeChannel*>(store->mChannels) : nullptr;

    // The channels' own state and the pipes' links.
    if (store != nullptr) {
        for (int i = 0; i < M && i < 8; ++i) {
            TbSoundPipeChannel* c = &channels[i];
            const int64_t* v = sp + S_CH + i * S_CHW;
            c->mInUse = static_cast<uint8_t>(v[0]);
            c->mVolume = static_cast<uint32_t>(v[1]);
            c->mSampleFlags = static_cast<uint32_t>(v[2]);
            c->mFlags = static_cast<uint32_t>(v[3]);
            c->mBufferSize = static_cast<uint32_t>(v[4]);
            c->mControlMask = static_cast<uint32_t>(v[5]);
            c->mField2C = static_cast<uint32_t>(v[6]);
            c->mHandle.mValue = static_cast<uint32_t>(v[7]);
            c->mServiceResult = static_cast<uint32_t>(v[8]);
            c->mVolumeControl.mVolumes = v[9] == 1 ? gFloatsA : v[9] == 2 ? gFloatsB : nullptr;
            c->mVolumeControl.mCount = static_cast<uint32_t>(v[10]);
            c->mSampleHeader.mSize = static_cast<int32_t>(v[11]);
            c->mSampleHeader.mRate = static_cast<uint16_t>(v[12] >> 16);
            c->mSampleHeader.mBits = static_cast<uint8_t>(v[12] >> 8);
            c->mSampleHeader.mFlags = static_cast<uint8_t>(v[12]);
            c->mSampleHeader.mField8 = static_cast<int32_t>(v[13]);
            c->mSampleHeader.mFieldC = static_cast<uint32_t>(v[14]);
            c->mSampleHeader.mData = HandleArg(v[15]);
            c->mNext = Node(v[16]);
            c->mPrevious = Node(v[17]);
            c->TbSoundSamplePipe::mHandle = HandleArg(v[18]);
            c->mLock.mValue = static_cast<uint32_t>(v[19]);
        }
    }
    for (int j = 0; j < gPipeCount; ++j) {
        const int64_t* v = sp + S_PIPE + j * 4;
        TbSoundSamplePipe* pipe = gPipeObjects[j];
        pipe->TbSoundSamplePipe::mHandle = HandleArg(v[0]);
        pipe->mNext = Node(v[1]);
        pipe->mPrevious = v[2] == -2 && channels != nullptr ? &channels[target] : Node(v[2]);
    }

    // Labels and regions.
    tpw_cl_label(1, gCore);
    tpw_cl_label(3, info);
    tpw_cl_label(4, &gControlA);
    tpw_cl_label(5, &gControlB);
    tpw_cl_label(6, gData + kDataStart);
    tpw_cl_label(7, gSysBytes);
    tpw_cl_label(8, gFloatsA);
    tpw_cl_label(9, gFloatsB);
    AddRegion(6, gData + kDataStart, kDataSize);
    for (int j = 0; j < gPipeCount; ++j) tpw_cl_label(20 + j, gPipeObjects[j]);
    if (store != nullptr) {
        tpw_cl_label(2, store);
        tpw_cl_label(30, gArrayRaw);
        for (int i = 0; i < M && i < 8; ++i) {
            tpw_cl_label(10 + i, &channels[i]);
            tpw_cl_label(40 + i, channels[i].mServiceBuffer.mData);
            if (i == target) AddRegion(40 + i, channels[i].mServiceBuffer.mData, 0x1220);
        }
    }
    if (alone != nullptr) {
        tpw_cl_label(9, alone);
        tpw_cl_label(40, alone->mServiceBuffer.mData);
        AddRegion(40, alone->mServiceBuffer.mData, 0x1220);
    }
    if (holder != nullptr) {
        tpw_cl_label(2, holder);
        tpw_cl_label(30, gArrayRaw);
    }
    gLockCount = 0;
    if (store != nullptr) {
        for (int i = 0; i < M && i < 8; ++i) gLockWords[gLockCount++] = &channels[i].mLock.mValue;
    }
    gSetup = false;
    tpw_cl_n = 0;

    // The operation.
    int64_t ret = 0;
    uint32_t slot[4] = {kFill, kFill, kFill, kFill};
    TbSoundSamplePipe* pipeOut = nullptr;
    TbSoundPipeChannel* channelOut = nullptr;
    TbSoundPipeChannel* c = channels != nullptr ? &channels[target] : alone;
    TbSoundSystemInfo* sys = reinterpret_cast<TbSoundSystemInfo*>(gSysBytes);
    unsigned char* buffer = gData + kDataStart + a[2];
    CUniqueHandle handle;
    handle.mValue = static_cast<uint32_t>(a[0]);
    int64_t* seq = out + O_SEQ;
    const TbSoundSampleStore* dumpStore = store;
    switch (op) {
        case 0: {
            std::memset(gChannelBytes, 0xA5, sizeof gChannelBytes);
            tpw_cl_label(9, gChannelBytes);
            alone = ::new (gChannelBytes) TbSoundPipeChannel();
            c = alone;
            break;
        }
        case 1:
            if (static_cast<int16_t>(a[0]) > 0) delete alone;
            else alone->~TbSoundPipeChannel();
            break;
        case 2: ret = c->GetNextHandle(); break;
        case 3: c->SetBufferSize(static_cast<uint32_t>(a[0])); break;
        case 4: ret = c->GetBufferSize(); break;
        case 5: {
            TbSampleHeader header;
            std::memset(&header, 0xA5, sizeof header);
            header.mSize = static_cast<int32_t>(a[0]);
            header.mRate = static_cast<uint16_t>(a[1] >> 16);
            header.mBits = static_cast<uint8_t>(a[1] >> 8);
            header.mFlags = static_cast<uint8_t>(a[1]);
            header.mField8 = static_cast<int32_t>(a[2]);
            header.mFieldC = static_cast<uint32_t>(a[3]);
            header.mData = HandleArg(a[4]);
            c->SetSampleHeader(header);
            break;
        }
        case 6: c->SetPriority(static_cast<uint32_t>(a[0])); break;
        case 7: ret = c->GetPriority(); break;
        case 8: c->SetVolume(static_cast<uint32_t>(a[0])); break;
        case 9: ret = c->GetActualVolume(); break;
        case 10: ret = c->GetSampleFlags(); break;
        case 11: ret = c->GetSampleHeader() == &c->mSampleHeader ? 0x40 : -1; break;
        case 12: ret = c->ControlledBy(static_cast<uint32_t>(a[0])); break;
        case 13: ret = Low32(c->GetHandle()); break;
        case 14: ret = c->GetIndex(); break;
        case 15: c->SetIndex(static_cast<uint16_t>(a[0])); break;
        case 16: ret = static_cast<uint32_t>(c->SetPipeInfo(HandleArg(a[0]), *info).mValue); break;
        case 17: c->GetChannelInfo(slot[1], slot[2]); break;
        case 18: ret = Low32(c->InitChannelInfo(*info, slot[1])); break;
        case 19: ret = static_cast<uint32_t>(c->SetInfo(*info).mValue); break;
        case 20: ret = PW(c->Attatch(Node(a[0]))); break;
        case 21: ret = PW(c->FindPipe(handle)); break;
        case 22: {
            int32_t done = static_cast<int32_t>(kFill);
            ret = static_cast<uint32_t>(c->GetDataChunk(buffer, static_cast<int32_t>(a[1]), done, a[3] != 0));
            slot[1] = static_cast<uint32_t>(done);
            break;
        }
        case 23: ret = c->FillServiceBuffer(); break;
        case 24: {
            gShowDone = true;
            TbSoundSamplePipe* pipe = a[1] == 0 ? static_cast<TbSoundSamplePipe*>(c) : Node(a[1] - 1);
            ret = TbSoundPipeChannel::StreamerCallback(buffer, static_cast<uint32_t>(a[0]), pipe) ? 1 : 0;
            gShowDone = false;
            break;
        }
        case 25: ret = PW(c->GetChannelUsed()); break;
        case 26: ret = c->ReleasePipe(a[0] != 0) ? 1 : 0; break;
        case 27: ret = c->GetVolume(); break;
        case 30: {
            std::memset(gStoreBytes, 0xA5, sizeof gStoreBytes);
            tpw_cl_label(2, gStoreBytes);
            gSkewActive = true;
            store = ::new (gStoreBytes) TbSoundSampleStore(gCore, static_cast<uint32_t>(M));
            dumpStore = store;
            channels = static_cast<TbSoundPipeChannel*>(store->mChannels);
            break;
        }
        case 31:
            if (static_cast<int16_t>(a[0]) > 0) delete store;
            else store->~TbSoundSampleStore();
            break;
        case 32: ret = static_cast<uint32_t>(store->Init(static_cast<uint32_t>(a[0])).mValue); break;
        case 33: store->SetNumberOfChannelsToUse(*sys); break;
        case 34: ret = store->GetNumberOfUsedChannels(); break;
        case 35: ret = store->GetMaxNumberOfChannels(); break;
        case 36: ret = ChannelIndex(store, (*store)[static_cast<uint32_t>(a[0])]); break;
        case 37: ret = ChannelIndex(store, (*store)[handle]); break;
        case 38: {
            channelOut = store->FindChannelAndPipe(handle, pipeOut);
            ret = ChannelIndex(store, channelOut);
            break;
        }
        case 39: ret = store->GetValidChannelIndexFromHandle(handle, slot[1]) ? 1 : 0; break;
        case 40:
        case 41: {
            TbSoundSampleStore::CChannelIterator* it = nullptr;
            if (op == 40) it = ::new (gIterBytes) TbSoundSampleStore::CChannelIterator(store);
            else it = ::new (gIterBytes) TbSoundSampleStore::CUsedChannelIterator(store);
            seq[0] = ChannelIndex(store, static_cast<TbSoundPipeChannel*>(*it));
            for (int k = 1; k <= 6; ++k) seq[k] = ChannelIndex(store, ++(*it));
            seq[7] = (it->mEnd - static_cast<TbSoundPipeChannel*>(store->mChannels));
            seq[8] = (it->mCurrent - static_cast<TbSoundPipeChannel*>(store->mChannels));
            break;
        }
        case 42: ret = static_cast<uint32_t>(store->ReleaseAllPipes()); break;
        case 43: store->StopAllChannels(a[0] != 0); break;
        case 44: store->StopAllChannelsWithVolumeControl(static_cast<uint32_t>(a[0])); break;
        case 45: ret = ChannelIndex(store, store->GetFreeChannel(*info)); break;
        case 46: {
            TbSoundSamplePipe* pipe = store->GetPipesFromHandle(HandleArg(a[0]), &channelOut, *info);
            ret = PW(pipe);
            break;
        }
        case 47: ret = Low32(store->CreateNewSample(HandleArg(a[0]), *info, slot[1])); break;
        case 48: ret = Low32(store->CreateNewControl(HandleArg(a[0]), *info)); break;
        case 49: {
            std::memset(gHolderBytes, 0xA5, sizeof gHolderBytes);
            tpw_cl_label(2, gHolderBytes);
            holder = ::new (gHolderBytes) CAutoArrayPtr<TbSoundPipeChannel>(static_cast<uint32_t>(a[1]));
            ret = static_cast<TbSoundPipeChannel*>(*holder) != nullptr ? 1 : 0;
            break;
        }
        case 50:
            if (static_cast<int16_t>(a[0]) > 0) delete holder;
            else holder->~CAutoArrayPtr<TbSoundPipeChannel>();
            break;
        case 51: ret = static_cast<TbSoundPipeChannel*>(*holder) == holder->mData ? 1 : 0; break;
    }
    if (op >= 49) {
        out[O_STORE + 3] = holder->mData != nullptr ? 1 : 0;
    }

    // The answer.
    out[O_RET] = ret;
    for (int k = 0; k < 4; ++k) out[O_SLOT + k] = slot[k];
    out[O_PIPE] = PW(pipeOut);
    out[O_CHANNEL] = op == 46 ? PW(channelOut) : ChannelIndex(store, channelOut);
    if (alone != nullptr || c != nullptr) {
        if (standaloneChannel) DumpChannel(alone, out + O_DUMP);
    }
    if (dumpStore != nullptr && channels != nullptr) {
        for (int i = 0; i < M && i < 8; ++i) DumpChannel(&channels[i], out + O_DUMP + i * kDumpWords);
        const int64_t extra = sp[S_EXTRA];
        if (extra >= 0 && extra < M) DumpChannel(&channels[extra], out + O_DUMP + 8 * kDumpWords);
        DumpStore(dumpStore, out + O_STORE);
    }
    for (int j = 0; j < gPipeCount; ++j) {
        int64_t* o = out + O_PIPES + j * 4;
        o[0] = PW(gPipeObjects[j]->mNext);
        o[1] = PW(gPipeObjects[j]->mPrevious);
        o[2] = Low32(gPipeObjects[j]->TbSoundSamplePipe::mHandle);
        o[3] = LinkClass(gPipeObjects[j]->mPoolLink);
    }
    DumpInfo(info, out + O_INFO);
    out[O_STATE] = CChannelState::mChannelState.mCount.mValue;
    out[O_STATE + 1] = gHandleMaskInitialized;
    out[O_STATE + 2] = gHandleIndexMask;
    out[O_STATE + 3] = CUniqueHandle::_mBitsPerCount;
    std::memcpy(data, gData, kDataBytes);

    // The case's blocks go back to the system.
    tpw_cl_on = 0;
    gSkewActive = false;
    for (int i = 0; i < gAllocCount; ++i) ::free(gAllocReal[i]);
    gAllocCount = 0;
}

}  // extern "C"
