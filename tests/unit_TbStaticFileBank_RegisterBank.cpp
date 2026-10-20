// Test library for TbStaticFileBank::RegisterBank, TbSoundSampleStore::operator[] and three sound static initialisers (unit v51045).
// The callees outside the unit (the buffer allocation, the file's GetSize / Read / Close, the channel's FindPipe, the global
// object registration, the queue constructor) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbSoundBank.h"
#include "TbSoundSampleStore.h"
#include "TbSoundPipeChannel.h"
#include "CSprocketsModule.h"
#include "CMpegBase.h"
#include "snd_handles.h"

using namespace calllog;
using Pool = SamsUtilities::MpegFilePlayerPool;

namespace {
const uint8_t* gData = nullptr;
uint32_t gDataLen = 0;
}  // namespace

// ---- the pool's data (defined in TbMpegFilePlayer.cpp on the program)

template <>
Pool::Queue::UInterruptSafeQueue() {
    ev("QueueCtor", {lab(this)});
}
template <>
Pool::Queue Pool::gPoolQueue{};
template <>
Pool::StaticCleanup Pool::gPoolCleanup{};
template <>
Pool::StaticCleanup::~StaticCleanup() {}
// ---- fakes
void* AllocBuffer_(uint32_t bytes) {
    ev("Alloc", {static_cast<int64_t>(bytes)});
    if (ret("AllocFail") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(bytes < 4 ? 4 : bytes);
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), lab(destructor), lab(node)});
}

namespace {
class FakeAccess : public bfl::FileSystem::CAccessItem {
public:
    FakeAccess() {}
    ~FakeAccess() override {}
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {static_cast<int64_t>(size)});
        std::memcpy(dest, gData, size < gDataLen ? size : gDataLen);
        return static_cast<int32_t>(size);
    }
};
FakeAccess gAccess;
bfl::FileSystem::CFile gFile;
}  // namespace

namespace bfl {
namespace FileSystem {
CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return 0; }
int32_t CAccessItem::Write(const void*, uint32_t) { return 0; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return 0; }
int32_t CAccessItem::GetSize() { return 0; }
int32_t CAccessItem::GetPos() { return 0; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(0); }
int32_t CAccessItem::GetMacFileReference() { return 0; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

CFile::CFile() : mImplementation(nullptr) {}
CFile::CFile(const CFile& other) : mImplementation(other.mImplementation) {}
CFile& CFile::operator=(const CFile& other) {
    mImplementation = other.mImplementation;
    return *this;
}
CFile::~CFile() {}
void CFile::Close() { ev("Close", {}); }
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() {
    ev("GetSize", {});
    return static_cast<int32_t>(ret("GetSize"));
}
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace FileSystem
}  // namespace bfl


#include "platform/samsutils.h"
namespace SamsUtilities {
void _USwapBlock32(uint32_t*, int32_t) {}
UString::UString() : mLength(0), mText(nullptr) {}
UString::UString(const char*) : mLength(0), mText(nullptr) {}
UString::UString(const char*, uint32_t) : mLength(0), mText(nullptr) {}
UString::~UString() {}
}
#include "CCodecFactory.h"
CCodecFactory::CCodecFactory() : mLock() {}
CCodecFactory::~CCodecFactory() {}
TbSoundSamplePipe* CCodecFactory::CreateCodec(TbSoundPipeChannel*, TbSoundSampleInfo&) { return nullptr; }
TbSoundSamplePipe* CCodecFactory::CreateController(TbSoundSampleInfo&) { return nullptr; }
int32_t CCodecFactory::GetNextSampleFromBank(TbSampleHeader**, TbSoundSampleInfo&) { return 0; }
TbSoundSamplePipe* CCodecFactory::CreateCodecModifier(TbSoundSampleInfo&, TbSampleHeader*, TbSoundSamplePipe*) { return nullptr; }

bool TbSoundPipeChannel::ReleasePipe(bool) { return false; }
uint32_t TbSoundPipeChannel::ControlledBy(uint32_t) const { return 0; }
uint32_t TbSoundPipeChannel::GetPriority() { return 0; }
uint32_t TbSoundPipeChannel::GetSampleFlags() { return 0; }
void TbSoundPipeChannel::SetIndex(uint16_t) {}
void* TbSoundPipeChannel::InitChannelInfo(TbSoundSampleInfo&, uint32_t&) { return nullptr; }
TbError TbSoundPipeChannel::SetPipeInfo(void*, TbSoundSampleInfo&) { return TbError(0); }
TbSoundSamplePipe* TbSoundPipeChannel::GetChannelUsed() { return nullptr; }
TbSoundPipeChannel::~TbSoundPipeChannel() {}
uint32_t TbSoundPipeChannel::GetIndex() { return 0; }
void* TbSoundPipeChannel::GetHandle() { return nullptr; }
void TbSoundPipeChannel::GetChannelInfo(uint32_t&, uint32_t&) {}
uint32_t TbSoundPipeChannel::GetVolume() { return 0; }
TbSoundSamplePipe* TbSoundPipeChannel::Attatch(TbSoundSamplePipe*) { return nullptr; }
int32_t TbSoundPipeChannel::GetDataChunk(void*, int32_t, int32_t&, bool) { return 0; }
TbError TbSoundPipeChannel::SetInfo(const TbSoundSampleInfo&) { return TbError(0); }
TbSoundSamplePipe* TbSoundPipeChannel::FindPipe(const CUniqueHandle&) { return nullptr; }
TbSoundPipeChannel::TbSoundPipeChannel() : mServiceBuffer(static_cast<unsigned char*>(nullptr)) {}
TbSoundVolumeControl::TbSoundVolumeControl() : mVolumes(nullptr), mCount(0) {}
uint32_t TbSoundSystemInfo::GetBuffersToUse() const { return 0; }
void TbSoundPipeChannel::SetPriority(uint32_t) {}
void TbSoundPipeChannel::SetVolume(uint32_t) {}
uint32_t TbSoundPipeChannel::GetActualVolume() const { return 0; }
void TbSoundPipeChannel::SetSampleHeader(const TbSampleHeader&) {}
uint32_t TbSoundPipeChannel::GetNextHandle() { return 0; }
TbSampleHeader* TbSoundPipeChannel::GetSampleHeader() { return nullptr; }
uint32_t TbSoundPipeChannel::FillServiceBuffer() { return 0; }
bool TbSoundPipeChannel::StreamerCallback(void*, uint32_t, void*) { return false; }
void TbSoundSamplePipe::operator delete(void*, std::size_t) {}
void* TbSoundSamplePipe::operator new(std::size_t) { return nullptr; }
TbSoundSamplePipe::TbSoundSamplePipe() {}
TbSoundSamplePipe::~TbSoundSamplePipe() {}
TbSoundSamplePipe* TbSoundSamplePipe::Attatch(TbSoundSamplePipe*) { return nullptr; }
void TbSoundSamplePipe::Release() {}
void TbSoundSamplePipe::Detatch() {}
TbSoundSamplePipe* TbSoundSamplePipe::FindPipe(void*) { return nullptr; }
void TbSoundSamplePipe::GetInfo(TbSoundSampleInfo&) {}
uint32_t TbSoundSamplePipe::GetSampleRate() { return 0; }
int32_t TbSoundSamplePipe::GetDataChunk(void*, int32_t, int32_t&, bool) { return 0; }
void TbSoundSamplePipe::Restart() {}
TbError TbSoundSamplePipe::SetInfo(const TbSoundSampleInfo&) { return TbError(0); }
void* TbSoundSamplePipe::SetHandle(void*) { return nullptr; }
void TbSoundSamplePipe::Completed(bool) {}
void TbSoundSamplePipe::SetDataStart(void*) {}

// Never called here (TbSoundSampleStore.cpp's other members use it).
TbError TbSoundCore::StopAndReleasePipe(TbSoundPipeChannel*, bool) { return TbError(0); }

const char* TbFileBank::GetSampleName(uint32_t) { return nullptr; }
TbError TbFileBank::RetrieveSample(TbSampleHeader**, uint32_t) { return TbError(0); }
bool TbFileBank::IsType(uint32_t) const { return false; }
void TbFileBank::Deregister() {}
TbError TbFileBank::RegisterBank(const char*, uint32_t) { return TbError(0); }
TbFileBank::TbFileBank(TbRefRelease* owner, bfl::FileSystem::CFile* file) : TbBank(owner), mFile(*file) {}
TbFileBank::~TbFileBank() {}
TbBank::TbBank(TbRefRelease* owner) : mRefManager(owner) {}
TbBank::~TbBank() {}
int32_t TbBank::GetNumberOfSamples() { return 0; }
const char* TbBank::GetSampleName(uint32_t) { return nullptr; }
TbError TbBank::Initialise(TbRefRelease**) { return TbError(0); }
uint32_t TbBank::GetSampleLength(uint32_t) { return 0; }
uint32_t TbBank::GetSamplePosition(uint32_t) { return 0; }
bfl::FileSystem::CFile* TbBank::GetHandle() { return nullptr; }
bool TbBank::IsStreaming() const { return false; }
#include "TbRefRelease.h"
TbRefRelease::TbRefRelease() : mRefCount(1) {}
int32_t TbRefRelease::Release() { return 0; }
int32_t TbRefRelease::AddRef() { return ++mRefCount; }
TbRefRelease::~TbRefRelease() {}
// TbStaticFileBank's other members live in another file.
TbStaticFileBank::~TbStaticFileBank() {}
void TbStaticFileBank::Deregister() {}
bool TbStaticFileBank::IsType(uint32_t) const { return false; }

namespace {
struct TestBank : TbStaticFileBank {
    using TbStaticFileBank::TbStaticFileBank;
    void Deregister() override { ev("Deregister", {}); }
};

// A fake vtable for the channels: Itanium slots 0,1 destructors, 2 Attatch ... 13 SetDataStart, 14 FindPipe(const CUniqueHandle&).
void* gFakeVt[20];
TbSoundPipeChannel* gArray = nullptr;

TbSoundSamplePipe* FakeFindPipe(TbSoundPipeChannel* self, const CUniqueHandle& handle) {
    const int64_t index = (reinterpret_cast<uint8_t*>(self) - reinterpret_cast<uint8_t*>(gArray)) / static_cast<std::ptrdiff_t>(sizeof(TbSoundPipeChannel));
    ev("FindPipe", {index, static_cast<int64_t>(handle.mValue)});
    return ret("FindPipe") != 0 ? reinterpret_cast<TbSoundSamplePipe*>(1) : nullptr;
}
}  // namespace

extern "C" {

uint64_t tpw_rb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"1003f2a8", gSinit_1003f2a8}, {"10033d3c", gSinitDtor_10033d3c}, {"1003f2ac", gSinit_1003f2ac},
        {"1003f284", gSinit_1003f284}, {"10033d34", gSinitDtor_10033d34}, {"1003f288", gSinit_1003f288},
        {"1003e33c", &Pool::gPoolQueue}, {"1003e32a", &Pool::gPoolCleanup}, {"1003352c", gSinitDtor_1003352c},
        {"1003ebf8", gSinit_1003ebf8}, {"1003ebe8", gSinit_1003ebe8}, {"100336dc", gSinitDtor_100336dc},
        {"1003ebec", gSinit_1003ebec}, {"1003ebc8", gSinit_1003ebc8},
        {"g329", &gPoolQueueBuilt}, {"g328", &gPoolCleanupBuilt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbSoundPipeChannel));
    return n;
}

// RegisterBank on a fresh bank; out = mData, mCount, mCache.
int32_t tpw_rb_register(const uint8_t* data, uint32_t length, uint64_t* out) {
    gData = data;
    gDataLen = length;
    gFile.mImplementation = &gAccess;
    TestBank* bank = new TestBank(nullptr, &gFile);
    const TbError result = bank->RegisterBank(nullptr, 0);
    out[0] = reinterpret_cast<uintptr_t>(bank->mData);
    out[1] = bank->mCount;
    out[2] = reinterpret_cast<uintptr_t>(bank->mCache);
    return result;
}

// operator[](handle) on a store of n channels; answers the channel's index or -1.
int32_t tpw_rb_index(uint32_t handleValue, uint32_t n) {
    for (int i = 0; i < 20; i++) gFakeVt[i] = nullptr;
    gFakeVt[14] = reinterpret_cast<void*>(&FakeFindPipe);
    const size_t stride = sizeof(TbSoundPipeChannel);
    uint8_t* array = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(stride * n));
    for (uint32_t i = 0; i < n; i++) *reinterpret_cast<void***>(array + stride * i) = &gFakeVt[0];
    gArray = reinterpret_cast<TbSoundPipeChannel*>(array);
    TbSoundSampleStore* store = static_cast<TbSoundSampleStore*>(tpw::ArenaAllocZeroed(sizeof(TbSoundSampleStore)));
    store->mChannels.mData = gArray;
    CUniqueHandle handle;
    handle.mValue = handleValue;
    TbSoundPipeChannel* channel = (*store)[handle];
    if (channel == nullptr) return -1;
    return static_cast<int32_t>((reinterpret_cast<uint8_t*>(channel) - array) / static_cast<std::ptrdiff_t>(stride));
}

void tpw_rb_sinit_pipe() { __sinit_TbSoundSamplePipe_cpp(); }
void tpw_rb_sinit_mm() { __sinit_TbMMFileBank_cpp(); }
void tpw_rb_sinit_mp2() { __sinit_StreamableMpeg2_cpp(); }

}  // extern "C"
