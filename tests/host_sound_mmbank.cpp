// host_sound_mmbank.cpp: a streaming bank (TbMMFileBank) whose offsets point at music data instead of a sample's header, as
// Theme Park Inc's retail Music/MusicHD.sdt banks of Arabian Nights (24 samples from the 104th on) and Polar Zone (6 from the
// 102nd) do. RegisterBank caches the first min(size, 0x2000) bytes of every sample; the header's size is read from the music
// data, and a negative one passes the original's signed cap and goes to new[] as unsigned, a block of 2 to 4 GB. The Mac's
// 32-bit heap never had one (NULL); a 64-bit heap hands them out, and Windows commits them (12 to 20 GB at the park's start,
// Discord, 2026-11-12). The port's fix (bankcache, src/sound/TbMMFileBank.cpp) caps the size unsigned, so such a sample
// caches 0x2000 bytes like any long one.
//
// RegisterBank and the bank's other functions are the real ones, cut from src/sound/TbMMFileBank.cpp (CMakeLists.txt,
// tpw_host_sound_mmbank); the file (a synthetic bank in memory), the bank's base and new[] are the test's. new[] records every
// request and refuses any above 16 MB, so a build without the fix reports the 2 to 4 GB requests instead of making them. Run
// by ctest ("sound_mmbank") in a build with the port's fixes; a build without them makes the program, which fails. Exits
// nonzero on a failure.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "TbSoundBank.h"

using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;

namespace {

int gFailures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++gFailures;
    }
}

// Every new[] of the program: the largest request, and the blocks not yet given back.
constexpr std::size_t kRefused = 16u << 20;
std::size_t gLargest = 0;
int gLive = 0;

void* Allocate(std::size_t size) {
    if (size > gLargest) gLargest = size;
    if (size > kRefused) return nullptr;
    void* block = std::malloc(size != 0 ? size : 1);
    if (block != nullptr) ++gLive;
    return block;
}

void Free(void* block) {
    if (block == nullptr) return;
    --gLive;
    std::free(block);
}

// The bank file in memory, read through the CFile's access object as the game's files are.
class MemoryAccess : public CAccessItem {
public:
    explicit MemoryAccess(const std::vector<uint8_t>& bytes) : mBytes(bytes) {}
    int32_t Read(void* dest, uint32_t size) override {
        const int64_t left = static_cast<int64_t>(mBytes.size()) - mPosition;
        const int64_t count = left <= 0 ? 0 : (static_cast<int64_t>(size) < left ? static_cast<int64_t>(size) : left);
        if (count > 0) std::memcpy(dest, mBytes.data() + mPosition, static_cast<std::size_t>(count));
        mPosition += count;
        return static_cast<int32_t>(count);
    }
    int32_t Seek(int32_t offset, uint32_t whence) override {
        if (whence == 0) mPosition = static_cast<uint32_t>(offset);
        else if (whence == 1) mPosition += offset;
        else mPosition = static_cast<int64_t>(mBytes.size()) + offset;
        return static_cast<int32_t>(mPosition);
    }
    int32_t GetSize() override { return static_cast<int32_t>(mBytes.size()); }
    int32_t GetPos() override { return static_cast<int32_t>(mPosition); }

    const std::vector<uint8_t>& mBytes;
    int64_t mPosition = 0;
};

}  // namespace

void* operator new[](std::size_t size) {
    void* block = Allocate(size);
    if (block == nullptr) throw std::bad_alloc();
    return block;
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return Allocate(size); }
void operator delete[](void* block) noexcept { Free(block); }
void operator delete[](void* block, std::size_t) noexcept { Free(block); }

// The access object's and the file's out-of-line members (bullfrog imports), the bank's base (src/sound/TbBank.cpp) and the
// bank's constructor (src/sound/TbSoundBankInfo.cpp): only what the bank's functions reach.
namespace bfl {
namespace FileSystem {
CAccessItem::CAccessItem() : mNext(nullptr), mReferences(1) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(0); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
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
void CFile::Close() {}
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    return mImplementation != nullptr ? mImplementation->Seek(static_cast<int32_t>(offset), whence) : -1;
}
int32_t CFile::GetSize() { return mImplementation != nullptr ? mImplementation->GetSize() : -1; }
int32_t CFile::GetPos() { return mImplementation != nullptr ? mImplementation->GetPos() : -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }
}  // namespace FileSystem
}  // namespace bfl

TbBank::TbBank(TbRefRelease* owner) : mRefManager(owner) {}
TbBank::~TbBank() {}
int32_t TbBank::GetNumberOfSamples() { return static_cast<int32_t>(mCount); }
uint32_t TbBank::GetSampleLength(uint32_t) { return 0; }
const char* TbBank::GetSampleName(uint32_t) { return nullptr; }
uint32_t TbBank::GetSamplePosition(uint32_t) { return 0; }
CFile* TbBank::GetHandle() { return nullptr; }
bool TbBank::IsStreaming() const { return false; }
TbError TbBank::Initialise(TbRefRelease** out) {
    *out = this;
    return TbError(0);
}
TbMMFileBank::TbMMFileBank(TbRefRelease* owner, CFile* file) : TbBank(owner), mFile(*file) {}

// The offsets' swap (platform/sams_core.cpp): the bank file is little-endian, as the host is, so it changes nothing.
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}

namespace {

// A sample of the bank: its offset, the size its header gives (for a music-data "header", what the music data holds at +4)
// and the bytes after the 0x28 header that the file holds.
struct Sample {
    uint32_t offset;
    int32_t size;
    uint32_t stored;
    bool header;   // false: no header at the offset, only music data
};

uint8_t DataByte(uint32_t at) { return static_cast<uint8_t>(at * 7u + (at >> 8) + 1u); }

void Put32(std::vector<uint8_t>& bytes, uint32_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, 4); }

// The bank file: the count, the offsets, then each sample (its 0x28-byte header and its bytes). A sample with no header is
// music data from its offset on, with `size` where the header's size would be, as the retail banks have.
std::vector<uint8_t> MakeBank(const std::vector<Sample>& samples) {
    uint32_t end = 0;
    for (const Sample& s : samples) end = s.offset + 0x28 + s.stored > end ? s.offset + 0x28 + s.stored : end;
    std::vector<uint8_t> bytes(end);
    for (uint32_t i = 0; i < end; ++i) bytes[i] = DataByte(i);
    Put32(bytes, 0, static_cast<uint32_t>(samples.size()));
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Sample& s = samples[i];
        Put32(bytes, static_cast<uint32_t>(4 + i * 4), s.offset);
        if (s.header) {
            std::memset(bytes.data() + s.offset, 0, 0x28);
            Put32(bytes, s.offset, 0x28);
            bytes[s.offset + 0x18] = 0x22;   // 22050 per second
            bytes[s.offset + 0x19] = 0x56;
            bytes[s.offset + 0x1a] = 0x10;   // 16-bit
            bytes[s.offset + 0x1b] = 0x25;   // MPEG (0x20), stereo
        }
        Put32(bytes, s.offset + 4, static_cast<uint32_t>(s.size));
    }
    return bytes;
}

// Registers the bank and checks each sample's header and cache: the size as the file gives it, and a cache of the first
// min(size, 0x2000) bytes (0x2000 for a negative size), as the file holds them.
void RegisterAndCheck(const char* name, const std::vector<Sample>& samples) {
    const std::vector<uint8_t> bytes = MakeBank(samples);
    MemoryAccess access(bytes);
    CFile file;
    file.mImplementation = &access;
    gLargest = 0;
    gLive = 0;
    {
        TbMMFileBank bank(nullptr, &file);
        Check(static_cast<int32_t>(bank.RegisterBank(name, 0)) == 0, "the bank registers");
        Check(bank.mCount == samples.size(), "the bank's count is the file's");
        std::printf("  %s: %zu samples, largest new[] %zu bytes\n", name, samples.size(), gLargest);
        Check(gLargest <= 0x2000, "no new[] above a sample's 0x2000-byte cache");
        for (uint32_t i = 0; i < samples.size(); ++i) {
            const Sample& s = samples[i];
            TbSampleHeader* header = nullptr;
            Check(static_cast<int32_t>(bank.RetrieveSample(&header, i + 1)) == 0 && header != nullptr, "the sample is there");
            if (header == nullptr) continue;
            Check(header->mSize == s.size, "the header keeps the file's size");
            Check(bank.GetSamplePosition(i + 1) == s.offset, "the sample's position is its offset");
            Check(header->mData != nullptr, "the sample has its cache");
            if (header->mData == nullptr) continue;
            const uint32_t cached = static_cast<uint32_t>(s.size) < 0x2000u ? static_cast<uint32_t>(s.size) : 0x2000u;
            const uint32_t compared = cached < s.stored ? cached : s.stored;
            Check(std::memcmp(header->mData, bytes.data() + s.offset + 0x28, compared) == 0,
                  "the cache holds the sample's first bytes");
        }
    }
    Check(gLive == 0, "every block is given back when the bank goes");
}

}  // namespace

int main() {
    std::printf("sound mmbank: a streaming bank whose offsets point at music data\n");
    // The bank as Theme Park World's own are: every offset at a header, sizes below and above the cache.
    RegisterAndCheck("good", {{0x14, 0x100, 0x100, true}, {0x13c, 0x3000, 0x3000, true}});
    // Arabian Nights' MusicHD.sdt from its 104th sample on: music data at the offset, with the size word negative (the values
    // at +4 of the 104th and 105th samples) or 0x80000000, and the last one close to the end of the file.
    RegisterAndCheck("music data", {{0x1c, 0x100, 0x100, true},
                                    {0x144, -875872534, 0x2400, false},
                                    {0x256c, -702889504, 0x2400, false},
                                    {0x4994, static_cast<int32_t>(0x80000000u), 0x2400, false},
                                    {0x6dbc, 0x7fffffff, 0x2400, false},
                                    {0x91e4, -1, 0x100, false}});
    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", gFailures);
        return 1;
    }
    std::printf("sound mmbank: OK\n");
    return 0;
}
