// Fakes and test entry points for unit w80b, the sound library's codec factory, bank file wrapper and MPEG sample codecs
// (libtpw_unit_SoundCodecFactory.dylib, driven by tests/test_SoundCodecFactory.py in the program "sound symbols shared"; see
// calllog.h):
//   src/CCodecFactory.cpp          CCodecFactory (the four functions, constructor, destructor) and _CFileAccess
//   src/TbSoundSampleMPEG2.cpp     _TbSoundSampleMPEG2, _TbSoundSampleMPEG2File, their pools and the two cleanup objects
// Real: those, the WAV codecs, the loop and the pipe base of units w80 and w78, the critical sections of w79a, and Bank::CBank
// with the bank base TbBank (unit w72: RetrieveSample, the file conversion and the three members unit w80b added). Fakes, each
// recording what the original's callee would be given (the PowerPC side records the same from its stubs):
//   FakeBank, a TbBank whose AddRef, Release, RetrieveSample, GetSamplePosition, GetHandle and IsStreaming record and answer from
//       the queues "RetErr" and "Hdr" (the header index), "Pos" and "Stream"
//   the channel's SetSampleHeader, GetNextHandle (the queue "Handle") and ReleasePipe (it records the flag); SetBufferSize is
//       inline here and a real function of the Mac (the test compares the channel's buffer size word)
//   CMpegBase (constructor, destructor, Open, Read, Reset, Close, operator new) and the file player (Open, the pooled wrapper's
//       operator new and constructor): Open answers the queue "Open" (-1 fails) and fills the rate, length and layer from the
//       queues "Rate", "Total" and "Layer"; Read writes a counting pattern into what it is given
//   the two callback pipes (constructors and operator new) over the SP codec's behaviour
//   the file the bank gives (FakeFile: Read, Seek and GetPos record and work on the case's bytes) and the default file of a
//       _CFileAccess (Own.*), the semaphore and its lock (Sema.*, SemaLock.*)
//   the runtime heap (operator new and delete: calllog.cpp records "new" with the Mac's size and a label through the observer
//       below, "delete" for a labelled block) and the clock LbTime_GetClock
//
// Sizes: the host objects are larger than the Mac's (native pointers), so the sizes the heap and the pool operations see are
// mapped (MacSize below); any other size the test asks for is carried as the Mac size + 0x1000 on the host.
//
// Labels: 1 and 2 the banks, 3 and 4 the Bank::CBank handles on them, 5 a handle without a bank, 7 the bank's file, 8 and 9 the
// two sample infos, 10 the channel, 40 to 42 the sample headers; 100 and up the blocks the runtime made.
// A block the case allocates is filled with 0xA5 on both sides. The output buffer (gBuffer, 0x1000 bytes of 0xA5) is not
// labelled: a pointer into it is recorded as its offset (the two scratch buffers of the MPEG codec as 0x10000 and 0x20000).
//
// The case drives everything with tpw_cf_op and reads the state back with the getters below.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CCodecFactory.h"
#include "CMpegBase.h"
#include "TbDynamicStringTemplate.h"
#include "LbTime.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include "calllog.h"
#include "platform/samsutils.h"

using namespace calllog;
using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;

#pragma clang diagnostic ignored "-Winvalid-offsetof"

// The string constructors the real strings need and nothing here calls.
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbStaticStringTemplate<char>::Format(const char*, ...) {}

namespace {

constexpr int kMaxBlocks = 4096;
void* gAllocated[kMaxBlocks];   // every block the runtime handed out in the case (freed at the end when not deleted)
int gAllocatedCount;

constexpr std::size_t kLiteral = 0x1000;

// The pools: the order of the Python test's table.
enum PoolClass { kPoolWAV = 0, kPoolWAVFile = 1, kPoolLoop = 2, kPoolMPEG2 = 3, kPoolMPEG2File = 4, kPools = 5 };
const int kMacSizes[kPools] = {0x30, 0x3c, 0x18, 0x40, 0x50};

// The Mac size of a host size (see the top of the file).
std::size_t MacSize(std::size_t size) {
    if (size == sizeof(_TbSoundSampleWAV)) return 0x30;
    if (size == sizeof(_TbSoundSampleWAVFile)) return 0x3c;
    if (size == sizeof(_TbSoundCodecLoop)) return 0x18;
    if (size == sizeof(_TbSoundSampleMPEG2)) return 0x40;
    if (size == sizeof(_TbSoundSampleMPEG2File)) return 0x50;
    if (size == sizeof(CMpegBase)) return 0x24f0;
    if (size == sizeof(SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>)) return 0x6d64;
    if (size == sizeof(_TbSoundSampleCallback)) return 0x24;
    if (size == sizeof(_CFileAccess)) return 0x18;
    if (size >= kLiteral && size < 2 * kLiteral) return size - kLiteral;
    return size;
}

std::size_t HostSize(int pool, int64_t macSize);

// The factory whose critical section word the "new" event and the channel's GetNextHandle and ReleasePipe events also carry, set
// only while an OP_CREATE_CODEC runs (0 otherwise): a lock scope that ends too early or late then shows in the log.
CCodecFactory* gLockFactory = nullptr;
int64_t LockWord() { return gLockFactory != nullptr ? static_cast<int64_t>(static_cast<uint32_t>(gLockFactory->mLock.mValue)) : 0; }

// The runtime's new: a "new" event of the Mac's size and the block's label (and the lock word), the block filled with 0xA5.
void SoundAllocated(std::size_t size, bool, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on()) return;
    if (gAllocatedCount < kMaxBlocks) gAllocated[gAllocatedCount++] = block;
    ev("new", {static_cast<int64_t>(MacSize(size)), newlab(block), LockWord()});
}

// Blocks the runtime frees are kept until the end of the case (see tpw_cf_end), so that no address is reused while a stale
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

// The nothrow forms of operator new (the bank code allocates with them; no case calls those functions).
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return std::calloc(1, size != 0 ? size : 1);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return std::calloc(1, size != 0 ? size : 1);
}

// The platform's swap and strings, which the bank code (real) links against and no case runs.
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}

SamsUtilities::UString::UString(const char*) {}

SamsUtilities::UString::~UString() {}

#define TPW_REAL_CFILE_MEMBERS
#include "fake_file_slots.h"

namespace {

constexpr int kSlots = 6;
constexpr int kHeaders = 3;
constexpr int kHeaderBytes = 0x28 + 0x200;   // the file's 0x28-byte header, then the sample data
constexpr int kBufferBytes = 0x1000;
constexpr int kFileBytes = 0x400;
constexpr int kBanks = 2;
constexpr int kCBanks = 3;
constexpr int kInfos = 2;
constexpr int kFactories = 2;
constexpr int kAccesses = 2;
constexpr int kRowWords = 26;
constexpr int kRows = 4;
constexpr uint32_t kFill = 0xA5A5A5A5u;
enum Kind { kNone = 0, kMPEG2 = 1, kMPEG2File = 2, kChain = 3 };

// ---- the data -------------------------------------------------------------------------------------------------------------

alignas(16) uint8_t gHeaders[kHeaders][kHeaderBytes];
alignas(16) uint8_t gBuffer[kBufferBytes];
uint8_t gFileData[kFileBytes];   // the bank's file (FakeFile)
uint8_t gOwnData[kFileBytes];    // the default file of a _CFileAccess
int64_t gFilePosition;
int64_t gOwnPosition;
uint32_t gPattern;               // the counter of the decoder's pattern

uint8_t FileByte(int index) {
    return static_cast<uint8_t>(index * 5 + 1);
}

uint8_t OwnByte(int index) {
    return static_cast<uint8_t>(index * 7 + 3);
}

uint32_t Low32(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

// A pointer into a buffer the decoder or a file writes: the output buffer's offset, 0x10000 and 0x20000 for the codec's two
// scratch buffers (plus the offset), or -1.
int64_t BufferOffset(const void* pointer) {
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    if (p >= gBuffer && p < gBuffer + kBufferBytes) return p - gBuffer;
    if (p >= _TbSoundSampleMPEG2::sSetInfoScratch && p < _TbSoundSampleMPEG2::sSetInfoScratch + 0x300) {
        return 0x10000 + (p - _TbSoundSampleMPEG2::sSetInfoScratch);
    }
    if (p >= _TbSoundSampleMPEG2::sRestartScratch && p < _TbSoundSampleMPEG2::sRestartScratch + 0x300) {
        return 0x20000 + (p - _TbSoundSampleMPEG2::sRestartScratch);
    }
    return -1;
}

// Where a data pointer points: 0 NULL, 0x10000 times the header number plus the offset from the header's start, or the low word.
int64_t DataStartWord(const void* pointer) {
    if (pointer == nullptr) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    for (int i = 0; i < kHeaders; ++i) {
        if (p >= gHeaders[i] && p < gHeaders[i] + kHeaderBytes) return 0x10000 * (i + 1) + (p - gHeaders[i]);
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

// The index of a header the case made, or -1.
int64_t HeaderIndex(const void* pointer) {
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    for (int i = 0; i < kHeaders; ++i) {
        if (p == gHeaders[i]) return i;
    }
    return -1;
}

}  // namespace

// ---- the files --------------------------------------------------------------------------------------------------------------

namespace {

// The bank's file: Read copies what is left of the file (capped by the queue "Read": a value caps the count, -1 fails) into the
// output buffer; Seek answers the new position (the queue "Seek": -1 makes it fail); GetPos answers the position, or the next
// value of the queue "Pos" when there is one.
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

// The default file every _CFileAccess holds: the same behaviour over its own bytes, recorded as Own.* (queues "OwnRead",
// "OwnSeek" and "OwnPos"). It is the base CFile, whose implementation object is this access item.
class OwnAccess : public CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("Own.Read", {size, BufferOffset(dest)});
        const int64_t mode = ret("OwnRead", -2);
        if (mode == -1) return -1;
        uint32_t wanted = size;
        if (mode >= 0 && static_cast<uint64_t>(mode) < wanted) wanted = static_cast<uint32_t>(mode);
        const int64_t left = gOwnPosition < 0 ? 0 : static_cast<int64_t>(kFileBytes) - gOwnPosition;
        const int64_t offset = BufferOffset(dest);
        const int64_t space = offset < 0 ? 0 : kBufferBytes - offset;
        int64_t count = wanted;
        if (count > left) count = left > 0 ? left : 0;
        if (count > space) count = space;
        if (count > 0) std::memcpy(dest, gOwnData + gOwnPosition, static_cast<std::size_t>(count));
        gOwnPosition += count;
        return static_cast<int32_t>(count);
    }
};

OwnAccess gOwnAccess;

}  // namespace

CFile::CFile() : mImplementation(&gOwnAccess) {}

// The copy a _CFileAccess makes of the file it is given: it reads through the default file's access item (Own.*).
CFile::CFile(const CFile& other) : mImplementation(&gOwnAccess) {
    ev("Own.copy", {lab(&other)});
}

CFile::~CFile() {
    ev("Own.dtor");
}

void CFile::Close() {}

int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("Own.Seek", {offset, whence});
    if (ret("OwnSeek", 0) == -1) return -1;
    int64_t target;
    switch (whence) {
        case 0: target = offset; break;
        case 1: target = gOwnPosition + static_cast<int32_t>(offset); break;
        case 2: target = static_cast<int64_t>(kFileBytes) + static_cast<int32_t>(offset); break;
        default: return -1;
    }
    if (target < 0) return -1;
    gOwnPosition = target;
    return static_cast<int32_t>(target);
}

int32_t CFile::GetSize() {
    return -1;
}

int32_t CFile::GetPos() {
    ev("Own.GetPos");
    const int64_t scripted = ret("OwnPos", INT64_MIN);
    if (scripted != INT64_MIN) return static_cast<int32_t>(scripted);
    return static_cast<int32_t>(gOwnPosition);
}

// ---- the semaphore and its lock ----------------------------------------------------------------------------------------------

_LbSemaphore::_LbSemaphore() {
    ev("Sema.ctor");
}

_LbSemaphore::~_LbSemaphore() {
    ev("Sema.dtor");
}

int32_t _LbSemaLock::Lock(int32_t wait) {
    ev("SemaLock.Lock", {mState, mWaiting, mLocked, wait});
    return ret("SemaLock.Lock", 1);
}

_LbSemaLock::~_LbSemaLock() {
    ev("SemaLock.dtor", {mState, mWaiting, mLocked});
}

// ---- the decoder -----------------------------------------------------------------------------------------------------------

namespace {

// What the fake Open leaves in the decoder: the stream, the rate (CMpegBase +0x248c), the length (+0x24b4) and the layer
// (+0x2494) from the queues "Rate", "Total" and "Layer". The answer is the queue "Open" (-1 fails and leaves the fields).
int32_t FakeOpen(CMpegBase* decoder, const void* data) {
    const int32_t answer = static_cast<int32_t>(ret("Open", 0));
    if (answer != -1) {
        decoder->mData = static_cast<const uint8_t*>(data);
        decoder->mSampleRate2 = static_cast<int32_t>(ret("Rate", 0x5622));
        decoder->mTotalBytes = static_cast<int32_t>(ret("Total", 0x400));
        decoder->mLayer = static_cast<uint8_t>(ret("Layer", 2));
    }
    return answer;
}

}  // namespace

CMpegBase::CMpegBase() {
    ev("Mpeg.ctor", {lab(this)});
}

CMpegBase::~CMpegBase() {
    ev("Mpeg.dtor", {lab(this), 1});
}

void* CMpegBase::operator new(std::size_t size) {
    return ::operator new(size);
}

void CMpegBase::operator delete(void* object, std::size_t) {
    ::operator delete(object);
}

void CMpegBase::SeekBits(int32_t) {}

int32_t CMpegBase::DecodeHeader() {
    return 0;
}

int32_t CMpegBase::GetHeader() {
    return 0;
}

int32_t CMpegBase::Open(void* data, int32_t size) {
    ev("Mpeg.Open", {lab(this), DataStartWord(data), size});
    return FakeOpen(this, data);
}

int32_t CMpegBase::Close() {
    ev("Mpeg.Close", {lab(this)});
    return 0;
}

void CMpegBase::Reset() {
    ev("Mpeg.Reset", {lab(this)});
}

// Writes a counting pattern (the count of bytes asked for, capped by the room in the buffer it is given) and answers the queue
// "ReadRet" (default: the count).
int32_t CMpegBase::Read(void* out, int32_t bytes) {
    ev("Mpeg.Read", {lab(this), BufferOffset(out), bytes});
    const int64_t offset = BufferOffset(out);
    int64_t room = 0;
    if (offset >= 0 && offset < 0x10000) room = kBufferBytes - offset;
    if (offset >= 0x10000 && offset < 0x20000) room = 0x300 - (offset - 0x10000);
    if (offset >= 0x20000) room = 0x300 - (offset - 0x20000);
    int64_t count = bytes < 0 ? 0 : bytes;
    if (count > room) count = room;
    for (int64_t i = 0; i < count; ++i) static_cast<uint8_t*>(out)[i] = static_cast<uint8_t>(gPattern++);
    return static_cast<int32_t>(ret("ReadRet", bytes));
}

int32_t CMpegMacFilePlayerUnpooled::Open(CFile* file, int32_t start, int32_t size, char* data) {
    ev("Player.Open", {lab(this), lab(file), start, size, DataStartWord(data)});
    return FakeOpen(this, data);
}

// The player's other virtuals and the pool's operator delete (unit w80c's real header declares them): the vtable of the pooled
// wrapper needs the first three and its deleting destructor the last; the fake Open never reaches them, and the delete is the
// decoder fake's, as it was before the pool had one of its own.
void CMpegMacFilePlayerUnpooled::SeekBits(int32_t) {}
int32_t CMpegMacFilePlayerUnpooled::DecodeHeader() {
    return 0;
}
int32_t CMpegMacFilePlayerUnpooled::GetHeader() {
    return 0;
}

template <>
void SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::operator delete(void* object, std::size_t size) {
    CMpegBase::operator delete(object, size);
}

template <>
SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::UPooled() {
    ev("Player.ctor", {lab(this)});
}

template <>
void* SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>::operator new(std::size_t size) {
    return ::operator new(size);
}

// ---- the callbacks ----------------------------------------------------------------------------------------------------------

_TbSoundSampleCallback::_TbSoundSampleCallback(TbSndCallback* callback) {
    ev("Callback.ctor", {lab(this), Low32(callback)});
}

void* _TbSoundSampleCallback::operator new(std::size_t size) {
    return ::operator new(size);
}

_TbSoundSampleCompletedCallback::_TbSoundSampleCompletedCallback(TbSndCallback* callback, void* userData) {
    ev("Completed.ctor", {lab(this), Low32(callback), Low32(userData)});
}

void* _TbSoundSampleCompletedCallback::operator new(std::size_t size) {
    return ::operator new(size);
}

// The rest of the callback pipes' members (unit w79i, include/TbSoundSampleCallback.h): this test keeps the pipes' behaviour of
// before that unit (the pipe's own virtuals, the runtime's delete, no pools), so the vtables the constructors above store have
// these fakes in the slots the pipes override.
TbSoundUserCallback::TbSoundUserCallback() {}

TbSoundUserCallback::TbSoundUserCallback(void*) {}

TbSoundUserCallback::~TbSoundUserCallback() {}

TbError TbSoundUserCallback::SetInfo(const TbSoundSampleInfo& info) {
    return TbSoundSamplePipe::SetInfo(info);
}

void TbSoundUserCallback::GetInfo(TbSoundSampleInfo& info) {
    TbSoundSamplePipe::GetInfo(info);
}

void TbSoundUserCallback::operator delete(void* object, std::size_t) {
    ::operator delete(object);
}

_TbSoundSampleCallback::~_TbSoundSampleCallback() {}

TbError _TbSoundSampleCallback::SetInfo(const TbSoundSampleInfo& info) {
    return TbSoundSamplePipe::SetInfo(info);
}

void _TbSoundSampleCallback::GetInfo(TbSoundSampleInfo& info) {
    TbSoundSamplePipe::GetInfo(info);
}

int32_t _TbSoundSampleCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) {
    return TbSoundSamplePipe::GetDataChunk(buffer, size, done, flag);
}

void _TbSoundSampleCallback::operator delete(void* object, std::size_t) {
    ::operator delete(object);
}

_TbSoundSampleCompletedCallback::~_TbSoundSampleCompletedCallback() {}

int32_t _TbSoundSampleCompletedCallback::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) {
    return TbSoundSamplePipe::GetDataChunk(buffer, size, done, flag);
}

void _TbSoundSampleCompletedCallback::operator delete(void* object, std::size_t) {
    ::operator delete(object);
}

// ---- the channel -------------------------------------------------------------------------------------------------------------

void TbSoundPipeChannel::SetSampleHeader(const TbSampleHeader& header) {
    ev("Chan.SetSampleHeader", {lab(this), HeaderIndex(&header)});
}

uint32_t TbSoundPipeChannel::GetNextHandle() {
    ev("Chan.GetNextHandle", {lab(this), LockWord()});
    return static_cast<uint32_t>(ret("Handle", 0x1234));
}

bool TbSoundPipeChannel::ReleasePipe(bool flag) {
    ev("Chan.ReleasePipe", {lab(this), flag, LockWord()});
    return ret("RelRet", 0) != 0;
}

// ---- the banks ----------------------------------------------------------------------------------------------------------------

namespace {

// A bank whose answers are the case's. Its owner is nothing; AddRef and Release record (the codecs keep a reference).
class FakeBank : public TbBank {
public:
    FakeBank() : TbBank(nullptr) {}
    int32_t Release() override {
        ev("Bank.Release", {lab(this)});
        return 0;
    }
    int32_t AddRef() override {
        ev("Bank.AddRef", {lab(this)});
        return 0;
    }
    TbError RegisterBank(const char*, uint32_t) override { return TbError(0); }
    // The queue "RetErr" gives the error (a negative one leaves the header unset), "Hdr" the header's index.
    TbError RetrieveSample(TbSampleHeader** out, uint32_t index) override {
        ev("Bank.Retrieve", {lab(this), index});
        const int32_t error = static_cast<int32_t>(ret("RetErr", 0));
        if (error >= 0) {
            const int64_t which = ret("Hdr", 0);
            *out = reinterpret_cast<TbSampleHeader*>(gHeaders[which >= 0 && which < kHeaders ? which : 0]);
        }
        return TbError(error);
    }
    uint32_t GetSamplePosition(uint32_t index) override {
        ev("Bank.Position", {lab(this), index});
        return static_cast<uint32_t>(ret("Pos", 0x100));
    }
    CFile* GetHandle() override {
        ev("Bank.GetHandle", {lab(this)});
        return &gFile;
    }
    bool IsType(uint32_t) const override { return false; }
    bool IsStreaming() const override {
        ev("Bank.IsStreaming", {lab(this)});
        return ret("Stream", 0) != 0;
    }
    void Deregister() override {}
};

}  // namespace

// ---- the world ------------------------------------------------------------------------------------------------------------------

namespace {

TbSoundSamplePipe* gNodes[kSlots];
int gKinds[kSlots];
FakeBank* gBanks[kBanks];
Bank::CBank* gCBanks[kCBanks];
alignas(16) uint8_t gChannelStorage[sizeof(TbSoundPipeChannel) + 16];
TbSoundPipeChannel* gChannel;
alignas(16) uint8_t gInfoStorage[kInfos][sizeof(TbSoundSampleInfo)];
CCodecFactory* gFactories[kFactories];
_CFileAccess* gAccesses[kAccesses];
void* gBlocks[8];
int64_t gBlockSizes[8];   // the Mac sizes the raw blocks were asked for
int gBlockPools[8];       // the pool whose operator new made them
void* gCleanup[2];        // the objects the cleanup-destructor operation made

TbSoundSampleInfo& Info(int index) {
    return *reinterpret_cast<TbSoundSampleInfo*>(gInfoStorage[index]);
}

// ---- the pools, as a table -------------------------------------------------------------------------------------------------------

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
    POOL_ENTRY(_TbSoundSampleMPEG2, mFirst_TbSoundSampleMPEG2, mCurrent_TbSoundSampleMPEG2, mLow_TbSoundSampleMPEG2,
               mHigh_TbSoundSampleMPEG2, mTotal_TbSoundSampleMPEG2, mLevel_TbSoundSampleMPEG2, mMPEG2PoolLink),
    POOL_ENTRY(_TbSoundSampleMPEG2File, mFirst_TbSoundSampleMPEG2File, mCurrent_TbSoundSampleMPEG2File,
               mLow_TbSoundSampleMPEG2File, mHigh_TbSoundSampleMPEG2File, mTotal_TbSoundSampleMPEG2File,
               mLevel_TbSoundSampleMPEG2File, mMPEG2FilePoolLink),
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

bool PooledAnywhere(const void* object) {
    for (int p = 0; p < kPools; ++p) {
        if (PooledIn(p, object)) return true;
    }
    return false;
}

// A node exists: it has its label (a block the runtime freed loses it) and is not idle in a pool.
bool NodeAlive(const void* node) {
    return node != nullptr && lab(node) > 0 && !PooledAnywhere(node);
}

bool Alive(int slot) {
    return NodeAlive(gNodes[slot]);
}

// ---- the dump of one pipe --------------------------------------------------------------------------------------------------------

// The kind of node a pipe is: 1 WAV, 2 WAV file, 3 MPEG2, 4 MPEG2 file, 5 loop, 6 a callback, 0 anything else.
int NodeType(TbSoundSamplePipe* p) {
    if (dynamic_cast<_TbSoundSampleMPEG2File*>(p) != nullptr) return 4;
    if (dynamic_cast<_TbSoundSampleMPEG2*>(p) != nullptr) return 3;
    if (dynamic_cast<_TbSoundSampleWAVFile*>(p) != nullptr) return 2;
    if (dynamic_cast<_TbSoundSampleWAV*>(p) != nullptr) return 1;
    if (dynamic_cast<_TbSoundCodecLoop*>(p) != nullptr) return 5;
    if (dynamic_cast<_TbSoundSampleCallback*>(p) != nullptr) return 6;
    if (dynamic_cast<_TbSoundSampleCompletedCallback*>(p) != nullptr) return 6;
    return 0;
}

// One pipe as kRowWords words: [type, label, next, previous, handle, base link class, owner, size, data start, time, position,
// format, own link class, the base codec's link class (the file kinds), flags / file, total / start, decoder label, decoder
// layer, rate, length, stream, field +0x38 class, file (MPEG2 file), start, data].
void NodeRow(TbSoundSamplePipe* p, int64_t* out) {
    std::memset(out, 0, kRowWords * sizeof(int64_t));
    const int type = NodeType(p);
    out[0] = type;
    out[1] = lab(p);
    out[2] = PW(p->mNext);
    out[3] = PW(p->mPrevious);
    out[4] = Low32(p->mHandle);
    out[5] = LinkClass(p->mPoolLink);
    if (type >= 1 && type <= 4) {
        TbSoundSampleCodec* c = static_cast<TbSoundSampleCodec*>(p);
        out[6] = PW(c->mRefManager.mOwner);
        out[7] = c->mDataSize;
        out[8] = DataStartWord(c->mDataStart);
        out[9] = c->mTime;
        out[10] = c->mDataPosition;
        out[11] = c->mFormat;
    }
    switch (type) {
        case 1: out[12] = LinkClass(static_cast<_TbSoundSampleWAV*>(p)->mWAVPoolLink); break;
        case 2: {
            _TbSoundSampleWAVFile* f = static_cast<_TbSoundSampleWAVFile*>(p);
            out[12] = LinkClass(f->mWAVFilePoolLink);
            out[13] = LinkClass(f->mWAVPoolLink);
            out[14] = PW(f->mFile);
            out[15] = f->mStart;
            break;
        }
        case 5: out[12] = LinkClass(static_cast<_TbSoundCodecLoop*>(p)->mLoopPoolLink); break;
        default: break;
    }
    if (type == 3 || type == 4) {
        _TbSoundSampleMPEG2* m = static_cast<_TbSoundSampleMPEG2*>(p);
        out[12] = LinkClass(m->mMPEG2PoolLink);
        out[14] = m->mFlags;
        out[15] = m->mTotalBytes;
        out[16] = PW(m->mMpeg);
        if (m->mMpeg != nullptr && lab(m->mMpeg) > 0) {
            out[17] = m->mMpeg->mLayer;
            out[18] = m->mMpeg->mSampleRate2;
            out[19] = m->mMpeg->mTotalBytes;
            out[20] = DataStartWord(m->mMpeg->mData);
        }
        if (type == 4) {
            _TbSoundSampleMPEG2File* f = static_cast<_TbSoundSampleMPEG2File*>(p);
            out[13] = LinkClass(f->mMPEG2FilePoolLink);
            out[22] = PW(f->mFile);
            out[23] = f->mStart;
            out[24] = DataStartWord(f->mData);
        }
    }
}

// ---- building nodes ------------------------------------------------------------------------------------------------------------------

TbSampleHeader* HeaderPointer(int64_t index) {
    return reinterpret_cast<TbSampleHeader*>(gHeaders[index >= 0 && index < kHeaders ? index : 0]);
}

// The owner of a codec made by hand: one of the two banks (the original calls AddRef on it without a test for NULL).
TbRefRelease* OwnerPointer(int64_t which) {
    return gBanks[which & 1];
}

}  // namespace

extern "C" {

// Starts a case: the fixtures and their labels, the statics reset (the labels and queues were set by calllog's own entry).
void tpw_cf_begin() {
    gAllocatedCount = 0;
    std::memset(gNodes, 0, sizeof(gNodes));
    std::memset(gKinds, 0, sizeof(gKinds));
    std::memset(gBlocks, 0, sizeof(gBlocks));
    std::memset(gBlockSizes, 0, sizeof(gBlockSizes));
    std::memset(gBlockPools, 0, sizeof(gBlockPools));
    std::memset(gFactories, 0, sizeof(gFactories));
    std::memset(gAccesses, 0, sizeof(gAccesses));
    std::memset(gCleanup, 0, sizeof(gCleanup));
    std::memset(gHeaders, 0xA5, sizeof(gHeaders));
    std::memset(gBuffer, 0xA5, sizeof(gBuffer));
    std::memset(_TbSoundSampleMPEG2::sSetInfoScratch, 0xA5, 0x300);
    std::memset(_TbSoundSampleMPEG2::sRestartScratch, 0xA5, 0x300);
    for (int i = 0; i < kFileBytes; ++i) {
        gFileData[i] = FileByte(i);
        gOwnData[i] = OwnByte(i);
    }
    gFilePosition = 0;
    gOwnPosition = 0;
    gPattern = 0;
    for (int h = 0; h < kHeaders; ++h) {
        for (int i = 0; i < 0x200; ++i) gHeaders[h][0x28 + i] = static_cast<uint8_t>(i * 13 + h * 29 + 7);
    }
    for (int p = 0; p < kPools; ++p) {
        *kPoolTable[p].first = nullptr;
        *kPoolTable[p].current = 0;
        *kPoolTable[p].low = 0;
        *kPoolTable[p].high = 0;
        *kPoolTable[p].total = 0;
        *kPoolTable[p].level = 10;
    }
    // The world objects (made with the log off, like the PowerPC side's fixtures): two banks and the handles on them, a handle
    // without a bank, the channel and the infos, all filled with 0xA5 where the code does not write.
    for (int i = 0; i < kBanks; ++i) {
        gBanks[i] = new FakeBank();
        tpw_cl_label(1 + i, gBanks[i]);
    }
    for (int i = 0; i < 2; ++i) {
        gCBanks[i] = new Bank::CBank(gBanks[i]);
        tpw_cl_label(3 + i, gCBanks[i]);
    }
    gCBanks[2] = new Bank::CBank(nullptr);
    tpw_cl_label(5, gCBanks[2]);
    tpw_cl_label(7, &gFile);
    std::memset(gChannelStorage, 0xA5, sizeof(gChannelStorage));
    gChannel = reinterpret_cast<TbSoundPipeChannel*>(gChannelStorage);
    tpw_cl_label(10, gChannel);
    for (int i = 0; i < kInfos; ++i) {
        std::memset(gInfoStorage[i], 0xA5, sizeof(gInfoStorage[i]));
        tpw_cl_label(8 + i, &Info(i));
    }
    tpw_cl_label(40, gHeaders[0]);
    tpw_cl_label(41, gHeaders[1]);
    tpw_cl_label(42, gHeaders[2]);
}

// Ends it: every block the case allocated is freed now (the runtime's delete kept them), without running anything.
void tpw_cf_end() {
    tpw_cl_on = 0;
    for (int i = 0; i < gAllocatedCount; ++i) {
        if (gAllocated[i] != nullptr) std::free(gAllocated[i]);
    }
    gAllocatedCount = 0;
    for (int p = 0; p < kPools; ++p) *kPoolTable[p].first = nullptr;
    for (int i = 0; i < kBanks; ++i) {
        std::free(static_cast<void*>(gBanks[i]));
        gBanks[i] = nullptr;
    }
    for (int i = 0; i < kCBanks; ++i) {
        std::free(static_cast<void*>(gCBanks[i]));
        gCBanks[i] = nullptr;
    }
}

int32_t tpw_cf_sizes(int32_t which) {
    switch (which) {
        case 0: return static_cast<int32_t>(sizeof(TbSoundSampleCodec));
        case 1: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(TbSampleHeader));
    }
}

// A sample header (the Mac's first 0x14 bytes), in the host's byte order; `data` is the offset of the cached data word in the
// header's block (after the 0x28 bytes), or -1 for NULL.
void tpw_cf_set_header(int32_t index, uint32_t size, uint32_t rate, uint32_t bits, uint32_t flags, uint32_t field8, uint32_t fieldC,
                       int32_t data) {
    TbSampleHeader* header = reinterpret_cast<TbSampleHeader*>(gHeaders[index]);
    header->mSize = static_cast<int32_t>(size);
    header->mRate = static_cast<uint16_t>(rate);
    header->mBits = static_cast<uint8_t>(bits);
    header->mFlags = static_cast<uint8_t>(flags);
    header->mField8 = static_cast<int32_t>(field8);
    header->mFieldC = fieldC;
    header->mData = data < 0 ? nullptr : gHeaders[index] + 0x28 + data;
}

// A sample info: the number, the bank (0 none, 1 and 2 the handles on the banks, 3 the handle without a bank), the flags, the loop
// count, the callback, the completion callback, its user data and the streaming buffer size. The other words are the fill.
void tpw_cf_set_info(int32_t index, uint32_t number, int32_t bank, uint32_t flags, uint32_t loop, uint32_t callback,
                     uint32_t completed, uint32_t userData, uint32_t bufferSize) {
    TbSoundSampleInfo& info = Info(index);
    info.mNumber = number;
    info.mBank = bank >= 1 && bank <= 3 ? gCBanks[bank - 1] : nullptr;
    info.mFlags = flags;
    info.mLoopCount = loop;
    info.mCallback = reinterpret_cast<TbSndCallback*>(static_cast<uintptr_t>(callback));
    info.mCompletedCallback = reinterpret_cast<TbSndCallback*>(static_cast<uintptr_t>(completed));
    info.mUserData = reinterpret_cast<void*>(static_cast<uintptr_t>(userData));
    info.mStreamingBufferSize = bufferSize;
}

void tpw_cf_pool_set(int32_t pool, int32_t current, int32_t low, int32_t high, int32_t total, int32_t level) {
    *kPoolTable[pool].current = current;
    *kPoolTable[pool].low = low;
    *kPoolTable[pool].high = high;
    *kPoolTable[pool].total = total;
    *kPoolTable[pool].level = level;
}

// One pool: the six statics and the chain of idle objects (labels; at most 12 shown, then -9).
void tpw_cf_pool(int32_t pool, int64_t* out) {
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

// The output buffer and the two scratch buffers of the MPEG codec.
void tpw_cf_buffer(uint8_t* out) {
    std::memcpy(out, gBuffer, kBufferBytes);
}

void tpw_cf_scratch(uint8_t* out) {
    std::memcpy(out, _TbSoundSampleMPEG2::sSetInfoScratch, 0x300);
    std::memcpy(out + 0x300, _TbSoundSampleMPEG2::sRestartScratch, 0x300);
}

// One slot: [status (0 empty, -1 gone, else the number of rows), kind, then up to four rows of kRowWords words: the node and the
// ones behind it].
void tpw_cf_slot(int32_t slot, int64_t* out) {
    std::memset(out, 0, (2 + kRows * kRowWords) * sizeof(int64_t));
    if (gNodes[slot] == nullptr) return;
    if (!Alive(slot)) {
        out[0] = -1;
        return;
    }
    out[1] = gKinds[slot];
    int rows = 0;
    for (TbSoundSamplePipe* node = gNodes[slot]; NodeAlive(node) && rows < kRows; node = node->mNext) {
        NodeRow(node, out + 2 + rows * kRowWords);
        ++rows;
    }
    out[0] = rows;
}

// One raw block of the pool operations: [status (0 none, 1 gone, 2 alive, 3 idle in its pool), then, for a block that is a whole
// object of the class, mNext, mPrevious, mHandle, the base link class and the own link class].
void tpw_cf_block(int32_t index, int64_t* out) {
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

// The channel: [the streaming byte, the buffer size, the in-use byte].
void tpw_cf_channel(int64_t* out) {
    out[0] = gChannel->mStreaming;
    out[1] = gChannel->mBufferSize;
    out[2] = gChannel->mInUse;
}

// A sample info: [number, bank label, flags, loop count, callback, completion callback, user data, streaming buffer size].
void tpw_cf_info(int32_t index, int64_t* out) {
    TbSoundSampleInfo& info = Info(index);
    out[0] = info.mNumber;
    out[1] = PW(info.mBank);
    out[2] = info.mFlags;
    out[3] = info.mLoopCount;
    out[4] = Low32(info.mCallback);
    out[5] = Low32(info.mCompletedCallback);
    out[6] = Low32(info.mUserData);
    out[7] = info.mStreamingBufferSize;
}

// A factory: [alive, the lock word].
void tpw_cf_factory(int32_t index, int64_t* out) {
    out[0] = out[1] = 0;
    CCodecFactory* factory = gFactories[index];
    if (factory == nullptr || lab(factory) <= 0) return;
    out[0] = 1;
    out[1] = factory->mLock.mValue;
}

// A file wrapper: [alive, position, start, the semaphore word's class].
void tpw_cf_access(int32_t index, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = 0;
    _CFileAccess* access = gAccesses[index];
    if (access == nullptr || lab(access) <= 0) return;
    out[0] = 1;
    out[1] = access->mPosition;
    out[2] = access->mStart;
    out[3] = LinkClass(reinterpret_cast<const void*>(static_cast<uintptr_t>(access->mSemaphore.mState)));
}

// The positions of the two files.
void tpw_cf_files(int64_t* out) {
    out[0] = gFilePosition;
    out[1] = gOwnPosition;
}

// The operations, in the order of OPS in tests/test_SoundCodecFactory.py (the test checks the count).
enum Op {
    OP_NEW, OP_DTOR, OP_RELEASE, OP_GET_CHUNK, OP_RESTART, OP_SET_DATA_START, OP_SET_INFO, OP_OPEN, OP_CLOSE, OP_GET_BLOCK,
    OP_SET_NEW_POSITION, OP_SET_POSITION, OP_FILE_READ, OP_FILE_SEEK, OP_NEWOP, OP_DELETEOP, OP_TOPUP, OP_SETLEVEL,
    OP_CLEARCACHE, OP_CS_DTOR, OP_POKE, OP_FACTORY_NEW, OP_FACTORY_DTOR, OP_CREATE_CODEC, OP_CREATE_CONTROLLER, OP_GET_NEXT,
    OP_MODIFIER, OP_ACCESS_NEW, OP_ACCESS_READ, OP_ACCESS_SEEK, OP_ACCESS_SEEK_START, OP_ACCESS_DTOR, OP_INFO_SET, OP_FILE_POS,
    OP_CBANK, OP_CHANNEL_POKE, OP_COUNT
};

int32_t tpw_cf_op_count() { return OP_COUNT; }

// A slot whose node was freed (it has no label, or it went to a pool) is emptied after every operation.
static void Sweep() {
    for (int i = 0; i < kSlots; ++i) {
        if (gNodes[i] != nullptr && !Alive(i)) {
            gNodes[i] = nullptr;
            gKinds[i] = kNone;
        }
    }
}

static int64_t gAux[4];
int64_t tpw_cf_aux(int32_t index) { return gAux[index]; }

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);

// One operation. Results come back as a 64-bit integer (labels for pointers); -99 means "not applicable" on both sides.
int64_t tpw_cf_op(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    const int64_t result = DoOp(op, a, b, c, d, e);
    Sweep();
    return result;
}

// A pointer result as the test compares it: the label of a node, 0 for NULL.
static int64_t Label(const void* pointer) {
    return pointer == nullptr ? 0 : lab(pointer);
}

// Stores a node a function answered in a slot (the pipe is the factory's chain head: its kind is the chain's).
static void Keep(int64_t slot, TbSoundSamplePipe* pipe) {
    if (slot < 0 || slot >= kSlots || pipe == nullptr) return;
    gNodes[slot] = pipe;
    const int type = NodeType(pipe);
    gKinds[slot] = type == 3 ? kMPEG2 : type == 4 ? kMPEG2File : kChain;
}

static bool IsMpeg(int kind) { return kind == kMPEG2 || kind == kMPEG2File; }

// Whether SetInfo can run down a chain: a loop or a callback hands it to the pipe behind (which the original does not test), and
// the codecs answer it.
static bool SetInfoSafe(TbSoundSamplePipe* node) {
    for (int depth = 0; depth < 8; ++depth) {
        if (!NodeAlive(node)) return false;
        const int type = NodeType(node);
        if (type >= 1 && type <= 4) return true;
        if (type != 5 && type != 6) return false;
        node = node->mNext;
    }
    return false;
}

// Whether the decoder of an MPEG node exists (the original dereferences it without a test in these operations).
static bool HasDecoder(int slot) {
    _TbSoundSampleMPEG2* m = dynamic_cast<_TbSoundSampleMPEG2*>(gNodes[slot]);
    return m != nullptr && m->mMpeg != nullptr && lab(m->mMpeg) > 0;
}

// Whether a GetDataChunk or GetMpegBlock of this size and offset writes inside the output buffer (the decoder fake clamps it too, but
// the test also keeps the chunk size sane).
static bool ChunkSafe(int64_t offset, int64_t size) {
    return offset >= 0 && offset <= 0x800 && size >= 0 && offset + size <= kBufferBytes;
}

static int64_t DoOp(int32_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    gAux[0] = gAux[1] = 0;
    const bool needsNode = op == OP_DTOR || op == OP_RELEASE || op == OP_GET_CHUNK || op == OP_RESTART || op == OP_SET_DATA_START ||
                           op == OP_SET_INFO || op == OP_OPEN || op == OP_CLOSE || op == OP_GET_BLOCK ||
                           op == OP_SET_NEW_POSITION || op == OP_SET_POSITION || op == OP_FILE_READ || op == OP_FILE_SEEK ||
                           op == OP_POKE;
    TbSoundSamplePipe* pipe = (a >= 0 && a < kSlots) ? gNodes[a] : nullptr;
    int kind = kNone;
    if (needsNode) {
        if (a < 0 || a >= kSlots || !Alive(static_cast<int>(a))) return -99;
        kind = gKinds[a];
        const int slot = static_cast<int>(a);
        const bool mpeg = IsMpeg(kind);
        // The operations of the MPEG codecs only run on those.
        if ((op == OP_GET_CHUNK || op == OP_RESTART || op == OP_SET_DATA_START || op == OP_OPEN || op == OP_CLOSE ||
             op == OP_GET_BLOCK || op == OP_SET_NEW_POSITION || op == OP_SET_POSITION || op == OP_FILE_READ || op == OP_FILE_SEEK ||
             op == OP_POKE) && !mpeg) {
            return -99;
        }
        if ((op == OP_FILE_READ || op == OP_FILE_SEEK) && kind != kMPEG2File) return -99;
        if (op == OP_GET_CHUNK) {
            if (!ChunkSafe(b, c)) return -99;
            _TbSoundSampleMPEG2* m = static_cast<_TbSoundSampleMPEG2*>(pipe);
            if (m->mTime == 0 && !HasDecoder(slot)) return -99;
        }
        if (op == OP_RESTART && !HasDecoder(slot)) return -99;
        if (op == OP_SET_DATA_START && !HasDecoder(slot)) return -99;
        if (op == OP_GET_BLOCK) {
            if (!ChunkSafe(b, c)) return -99;
            if (kind == kMPEG2 && !HasDecoder(slot)) return -99;
        }
        // (the files clamp a read to the room in the buffer, so a huge count is safe)
        if ((op == OP_FILE_READ) && (b < 0 || b > 0x800 || static_cast<_TbSoundSampleMPEG2File*>(pipe)->mFile == nullptr)) return -99;
        if (op == OP_FILE_SEEK && static_cast<_TbSoundSampleMPEG2File*>(pipe)->mFile == nullptr) return -99;
        if (op == OP_POKE && b == 6 && kind != kMPEG2File) return -99;
        if (op == OP_SET_INFO && (c < 0 || c >= kInfos || !SetInfoSafe(pipe))) return -99;
    }
    switch (op) {
        case OP_NEW: {
            // a the slot, b the kind (1 MPEG2, 2 MPEG2 file), c the header, d the owner (0, 1 a bank, 2 none), e the file offset
            // of the data (a file with file 1 is the bank's)
            if (a < 0 || a >= kSlots || b < kMPEG2 || b > kMPEG2File) return -99;
            TbSoundSamplePipe* made;
            if (b == kMPEG2) {
                made = new _TbSoundSampleMPEG2(HeaderPointer(c), OwnerPointer(d));
            } else {
                made = new _TbSoundSampleMPEG2File((d & 4) != 0 ? nullptr : &gFile, HeaderPointer(c), static_cast<uint32_t>(e),
                                                    OwnerPointer(d & 3));
            }
            for (int i = 0; i < kSlots; ++i) {
                if (gNodes[i] == made) gNodes[i] = nullptr;
            }
            gNodes[a] = made;
            gKinds[a] = static_cast<int>(b);
            return lab(made);
        }
        case OP_DTOR: {
            // b the flag of the destructor call (positive: the deleting form)
            if (static_cast<int16_t>(b) > 0) {
                delete pipe;
            } else {
                pipe->~TbSoundSamplePipe();
            }
            gNodes[a] = nullptr;
            gKinds[a] = kNone;
            return 0;
        }
        case OP_RELEASE:
            pipe->Release();
            return 0;
        case OP_GET_CHUNK: {
            // b the offset in the output buffer, c the size, d the stopped flag
            int32_t done = 0x5555;
            const int32_t result = pipe->GetDataChunk(gBuffer + b, static_cast<int32_t>(c), done, d != 0);
            gAux[0] = done;
            return result;
        }
        case OP_RESTART:
            pipe->Restart();
            return 0;
        case OP_SET_DATA_START: {
            // b the data pointer (0 NULL, else 0x10000 * (header + 1) + offset from the header's start)
            const uint8_t* start = nullptr;
            if (b != 0) {
                const int64_t header = (b >> 16) - 1;
                if (header >= 0 && header < kHeaders) start = gHeaders[header] + (b & 0xFFFF);
            }
            pipe->SetDataStart(const_cast<uint8_t*>(start));
            return 0;
        }
        case OP_SET_INFO: {
            const TbError result = pipe->SetInfo(Info(static_cast<int>(c)));
            return static_cast<int32_t>(result);
        }
        case OP_OPEN: {
            const TbError result = static_cast<_TbSoundSampleMPEG2*>(pipe)->OpenMpegFile();
            return static_cast<int32_t>(result);
        }
        case OP_CLOSE:
            static_cast<_TbSoundSampleMPEG2*>(pipe)->CloseMpegFile();
            return 0;
        case OP_GET_BLOCK:
            return static_cast<_TbSoundSampleMPEG2*>(pipe)->GetMpegBlock(gBuffer + b, static_cast<int32_t>(c));
        case OP_SET_NEW_POSITION:
            static_cast<_TbSoundSampleMPEG2*>(pipe)->SetNewDataPosition();
            return 0;
        case OP_SET_POSITION:
            static_cast<_TbSoundSampleMPEG2*>(pipe)->SetDataPosition(static_cast<uint32_t>(b));
            return 0;
        case OP_FILE_READ:
            return static_cast<_TbSoundSampleMPEG2File*>(pipe)->Read(gBuffer + b, static_cast<uint32_t>(c));
        case OP_FILE_SEEK:
            return static_cast<_TbSoundSampleMPEG2File*>(pipe)->Seek(static_cast<int32_t>(b));
        case OP_NEWOP: {
            // a the block index, b the pool, c the Mac size
            if (a < 0 || a >= 8 || b < 0 || b >= kPools) return -99;
            void* block = kPoolTable[b].opNew(HostSize(static_cast<int>(b), c));
            gBlocks[a] = block;
            gBlockSizes[a] = c;
            gBlockPools[a] = static_cast<int>(b);
            return lab(block);
        }
        case OP_DELETEOP: {
            // a the block index (-1: NULL), b the pool, c the Mac size, d forgets the block
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
            // a which cleanup object (0 MPEG2, 1 MPEG2 file), b the flag (positive: the deleting form)
            if (a < 0 || a > 1) return -99;
            if (a == 0) {
                _CS__TbSoundSampleMPEG2* object = new _CS__TbSoundSampleMPEG2();
                if (static_cast<int16_t>(b) > 0) delete object; else object->~_CS__TbSoundSampleMPEG2();
            } else {
                _CS__TbSoundSampleMPEG2File* object = new _CS__TbSoundSampleMPEG2File();
                if (static_cast<int16_t>(b) > 0) delete object; else object->~_CS__TbSoundSampleMPEG2File();
            }
            return 0;
        }
        case OP_POKE: {
            // a the slot, b the field, c the value
            _TbSoundSampleMPEG2* m = static_cast<_TbSoundSampleMPEG2*>(pipe);
            switch (b) {
                case 0: pipe->mHandle = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(c))); break;
                case 1: m->mFormat = static_cast<uint8_t>(c); break;
                case 2: m->mTime = static_cast<uint32_t>(c); break;
                case 3: m->mDataPosition = static_cast<uint32_t>(c); break;
                case 4: m->mDataSize = static_cast<uint32_t>(c); break;
                case 5: m->mTotalBytes = static_cast<uint32_t>(c); break;
                case 6: static_cast<_TbSoundSampleMPEG2File*>(pipe)->mStart = static_cast<uint32_t>(c); break;
                case 7: m->mFlags = static_cast<uint32_t>(c); break;
                case 8: {
                    // the decoder's layer
                    if (!HasDecoder(static_cast<int>(a))) return -99;
                    m->mMpeg->mLayer = static_cast<uint8_t>(c);
                    break;
                }
                default: break;
            }
            return 0;
        }
        case OP_FACTORY_NEW: {
            if (a < 0 || a >= kFactories) return -99;
            gFactories[a] = new CCodecFactory();
            return lab(gFactories[a]);
        }
        case OP_FACTORY_DTOR: {
            // a the factory, b the flag
            if (a < 0 || a >= kFactories || gFactories[a] == nullptr || lab(gFactories[a]) <= 0) return -99;
            if (static_cast<int16_t>(b) > 0) {
                delete gFactories[a];
            } else {
                gFactories[a]->~CCodecFactory();
            }
            return 0;
        }
        case OP_CREATE_CODEC: {
            // a the factory, b the channel (0 none, 1 the channel, 2 none although the bank step succeeds), c the info, d the slot
            // that keeps the answer
            if (a < 0 || a >= kFactories || gFactories[a] == nullptr || lab(gFactories[a]) <= 0) return -99;
            if (c < 0 || c >= kInfos || d < 0 || d >= kSlots) return -99;
            // without a channel the call only returns when the bank step fails (the original calls GetNextHandle on NULL)
            if (b == 0 && !(Info(static_cast<int>(c)).mNumber == 0 || Info(static_cast<int>(c)).mBank == nullptr)) return -99;
            gLockFactory = gFactories[a];
            TbSoundSamplePipe* made = gFactories[a]->CreateCodec(b == 1 ? gChannel : nullptr, Info(static_cast<int>(c)));
            gLockFactory = nullptr;
            Keep(d, made);
            return Label(made);
        }
        case OP_CREATE_CONTROLLER: {
            // a the factory, b the info, c the slot that keeps the answer
            if (a < 0 || a >= kFactories || gFactories[a] == nullptr || lab(gFactories[a]) <= 0) return -99;
            if (b < 0 || b >= kInfos || c < 0 || c >= kSlots) return -99;
            TbSoundSamplePipe* made = gFactories[a]->CreateController(Info(static_cast<int>(b)));
            Keep(c, made);
            return Label(made);
        }
        case OP_GET_NEXT: {
            // a the factory, b the info; aux 0: which header it answered (-1 unset, -2 other)
            if (a < 0 || a >= kFactories || gFactories[a] == nullptr || lab(gFactories[a]) <= 0) return -99;
            if (b < 0 || b >= kInfos) return -99;
            TbSampleHeader* out = nullptr;
            const int32_t result = gFactories[a]->GetNextSampleFromBank(&out, Info(static_cast<int>(b)));
            gAux[0] = out == nullptr ? -1 : HeaderIndex(out) >= 0 ? HeaderIndex(out) : -2;
            return result;
        }
        case OP_MODIFIER: {
            // a the factory, b the info, c the header, d the slot of the pipe, e the slot that keeps the answer
            if (a < 0 || a >= kFactories || gFactories[a] == nullptr || lab(gFactories[a]) <= 0) return -99;
            if (b < 0 || b >= kInfos || d < 0 || d >= kSlots || e < 0 || e >= kSlots || !Alive(static_cast<int>(d))) return -99;
            TbSoundSamplePipe* made =
                gFactories[a]->CreateCodecModifier(Info(static_cast<int>(b)), HeaderPointer(c), gNodes[d]);
            Keep(e, made);
            return Label(made);
        }
        case OP_ACCESS_NEW: {
            // a the slot, b the constructor (0 the file only, 1 with a start), c the start; the file argument is the bank's file
            if (a < 0 || a >= kAccesses || b < 0 || b > 1) return -99;
            gAccesses[a] = b == 0 ? new _CFileAccess(&gFile) : new _CFileAccess(&gFile, static_cast<uint32_t>(c));
            return lab(gAccesses[a]);
        }
        case OP_ACCESS_READ: {
            // a the slot, b the offset in the output buffer, c the count
            if (a < 0 || a >= kAccesses || gAccesses[a] == nullptr || lab(gAccesses[a]) <= 0) return -99;
            if (b < 0 || b > 0x800) return -99;
            return gAccesses[a]->Read(gBuffer + b, static_cast<uint32_t>(c));
        }
        case OP_ACCESS_SEEK:
            if (a < 0 || a >= kAccesses || gAccesses[a] == nullptr || lab(gAccesses[a]) <= 0) return -99;
            gAccesses[a]->Seek(static_cast<int32_t>(b));
            return 0;
        case OP_ACCESS_SEEK_START:
            if (a < 0 || a >= kAccesses || gAccesses[a] == nullptr || lab(gAccesses[a]) <= 0) return -99;
            gAccesses[a]->SeekToDataStart();
            return 0;
        case OP_ACCESS_DTOR: {
            // a the slot, b the flag
            if (a < 0 || a >= kAccesses || gAccesses[a] == nullptr || lab(gAccesses[a]) <= 0) return -99;
            if (static_cast<int16_t>(b) > 0) {
                delete gAccesses[a];
            } else {
                gAccesses[a]->~_CFileAccess();
            }
            return 0;
        }
        case OP_INFO_SET: {
            // a the info, b the field, c the value
            if (a < 0 || a >= kInfos) return -99;
            TbSoundSampleInfo& info = Info(static_cast<int>(a));
            switch (b) {
                case 0: info.mNumber = static_cast<uint32_t>(c); break;
                case 1: info.mBank = c >= 1 && c <= 3 ? gCBanks[c - 1] : nullptr; break;
                case 2: info.mFlags = static_cast<uint32_t>(c); break;
                case 3: info.mLoopCount = static_cast<uint32_t>(c); break;
                case 4: info.mCallback = reinterpret_cast<TbSndCallback*>(static_cast<uintptr_t>(static_cast<uint32_t>(c))); break;
                case 5:
                    info.mCompletedCallback = reinterpret_cast<TbSndCallback*>(static_cast<uintptr_t>(static_cast<uint32_t>(c)));
                    break;
                case 6: info.mUserData = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(c))); break;
                case 7: info.mStreamingBufferSize = static_cast<uint32_t>(c); break;
                default: break;
            }
            return 0;
        }
        case OP_FILE_POS:
            // a the bank's file position, b the default file's
            gFilePosition = a;
            gOwnPosition = b;
            return 0;
        case OP_CBANK: {
            // a the handle (0 and 1 on the banks, 2 without a bank), b what (0 IsStreaming, 1 GetSamplePosition(c), 2 the bank as a
            // reference counted object, answered as a label)
            if (a < 0 || a >= kCBanks) return -99;
            Bank::CBank* cbank = gCBanks[a];
            if (b == 0) return cbank->IsStreaming() ? 1 : 0;
            if (b == 1) return cbank->GetSamplePosition(static_cast<uint32_t>(c));
            if (b == 2) return Label(cbank->GetBankAsRefRelease());
            return -99;
        }
        case OP_CHANNEL_POKE: {
            // a the field (0 the streaming byte, 1 the buffer size), b the value
            if (a == 0) gChannel->mStreaming = static_cast<uint8_t>(b);
            if (a == 1) gChannel->mBufferSize = static_cast<uint32_t>(b);
            return 0;
        }
        default:
            return -98;
    }
}

}  // extern "C"
