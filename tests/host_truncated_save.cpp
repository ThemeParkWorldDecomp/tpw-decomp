// host_truncated_save.cpp: a park file's packed block cut short or damaged. The rest of a park file after its header is one
// "BILZ" block (the 0x1c-byte header, then the zlib stream of the modules), and CSaveLoad::Load unpacked it with
// LbZLib_MemDecompress, which hands zlib an input length of 0xc0000000: a block that ends early (a save cut short when the game
// was closed while writing it) is read on past its end, and Load did not look at the answer, so it read the modules out of a
// buffer only partly unpacked and crashed. With the port's fixes (parkload and truncsave, src/game/save/SaveLoad.cpp) Load
// drops a file with less than a block header left, and unpacks with LbZLib_PortMemDecompressChecked (src/bullfrog/LbZLib.cpp),
// which gives zlib only the bytes the block has and answers the block's unpacked size only for a whole block, else 0. This
// program checks that answer with the host's zlib, sized as Load sizes it (the header's unpacked size): each block it unpacks
// ends at a page with no access behind it, so a read (or the header's swap mark written) past the block's end faults here.
//   - a good block unpacks to its size, byte for byte (the round trip of LbZLib_MemCompress);
//   - the block cut at every boundary of its header, inside zlib's own header, in the middle of the stream, without the
//     checksum and with only its last byte missing answers 0;
//   - garbage (random bytes, a good header with a random stream, a good block with bytes changed, a wrong unpacked size, a
//     wrong window size) answers anything but the size the header gives.
// With a park file's path as its argument it also cuts that file's block at many points (every byte near either end) and
// checks the whole of it still unpacks. Run by ctest ("truncated_save") in a build with the port's fixes; a build without
// them has no such call. Exits nonzero on a failure.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "bfl_ZLib.h"

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    if (value >= 0)
        std::printf("FAILED: %s (%ld)\n", what, value);
    else
        std::printf("FAILED: %s\n", what);
    ++gFailures;
}

size_t PageSize() {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
}

// A copy of `size` bytes whose last byte is the last one before a page with no access: the block's own memory and nothing after it.
class Guarded {
public:
    Guarded(const uint8_t* bytes, size_t size) {
        const size_t page = PageSize();
        mLength = (size + page - 1) / page * page + page;
#if defined(_WIN32)
        mBase = static_cast<uint8_t*>(VirtualAlloc(nullptr, mLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        DWORD old = 0;
        VirtualProtect(mBase + mLength - page, page, PAGE_NOACCESS, &old);
#else
        mBase = static_cast<uint8_t*>(mmap(nullptr, mLength, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
        mprotect(mBase + mLength - page, page, PROT_NONE);
#endif
        mData = mBase + mLength - page - size;
        if (size != 0) std::memcpy(mData, bytes, size);
    }
    ~Guarded() {
#if defined(_WIN32)
        VirtualFree(mBase, 0, MEM_RELEASE);
#else
        munmap(mBase, mLength);
#endif
    }
    uint8_t* data() { return mData; }

private:
    uint8_t* mBase;
    uint8_t* mData;
    size_t mLength;
};

uint32_t Word(const uint8_t* at) {
    return static_cast<uint32_t>(at[0]) | static_cast<uint32_t>(at[1]) << 8 | static_cast<uint32_t>(at[2]) << 16 |
           static_cast<uint32_t>(at[3]) << 24;
}
void SetWord(uint8_t* at, uint32_t word) {
    for (int i = 0; i < 4; ++i) at[i] = static_cast<uint8_t>(word >> (8 * i));
}

// The first `length` bytes of `block` unpacked from guarded memory into a buffer of `room` bytes, told the unpacked size
// the whole block's header gives as Load is (no more than `room`); the answer of the call.
uint32_t Unpack(const std::vector<uint8_t>& block, size_t length, std::vector<uint8_t>& out, size_t room) {
    Guarded source(block.data(), length);
    out.assign(room, 0xee);
    const uint32_t declared = block.size() >= 8 ? Word(block.data() + 4) : 0;
    const uint32_t size = declared <= room ? declared : static_cast<uint32_t>(room);
    return LbZLib_PortMemDecompressChecked(source.data(), static_cast<uint32_t>(length), out.data(), size);
}

// What a park's modules look like to zlib: runs of words and small counts, names, and stretches that do not pack.
std::vector<uint8_t> Modules(uint32_t size, uint32_t seed) {
    std::mt19937 random(seed);
    std::vector<uint8_t> data(size);
    for (uint32_t at = 0; at < size;) {
        const uint32_t run = 16 + random() % 2000;
        const uint32_t kind = random() % 3;
        for (uint32_t i = 0; i < run && at < size; ++i, ++at) {
            if (kind == 0) data[at] = static_cast<uint8_t>(i % 4 == 0 ? random() % 8 : 0);
            if (kind == 1) data[at] = static_cast<uint8_t>("PART MESS RIDE SHOP "[i % 20]);
            if (kind == 2) data[at] = static_cast<uint8_t>(random());
        }
    }
    return data;
}

std::vector<uint8_t> Pack(const std::vector<uint8_t>& modules) {
    std::vector<uint8_t> block(modules.size());
    std::vector<uint8_t> source = modules;
    const uint32_t packed = LbZLib_MemCompress(source.data(), block.data(), static_cast<uint32_t>(source.size()), nullptr);
    block.resize(packed);
    return block;
}

void Synthetic() {
    const std::vector<uint8_t> modules = Modules(300000, 17);
    const std::vector<uint8_t> block = Pack(modules);
    Check(block.size() > 0x1c + 64 && block.size() < modules.size(), "the modules pack", static_cast<long>(block.size()));
    Check(Word(block.data()) == kZLibTag && Word(block.data() + 4) == modules.size() && Word(block.data() + 8) == block.size(),
          "the block's header gives its tag and both sizes");
    const size_t size = block.size();
    std::vector<uint8_t> out;

    // the round trip: the whole block, from guarded memory and into a buffer of the exact size
    Check(Unpack(block, size, out, modules.size()) == modules.size(), "a good block unpacks to its size");
    Check(out == modules, "a good block unpacks to the bytes packed");
    Check(Unpack(block, size, out, modules.size() + 64) == modules.size() &&
              std::memcmp(out.data(), modules.data(), modules.size()) == 0 && out[modules.size()] == 0xee,
          "a good block writes nothing past its size");

    // cut short: in the block's header, in zlib's header, in the stream, without the checksum, without the last byte
    std::vector<size_t> cuts = {0, 1, 3, 4, 8, 0xc, 0x10, 0x17, 0x18, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x40, 0x100,
                                size / 4, size / 3, size / 2, size * 3 / 4, size - 64, size - 8, size - 5, size - 4, size - 3,
                                size - 2, size - 1};
    std::mt19937 random(5);
    for (int i = 0; i < 64; ++i) cuts.push_back(0x1c + random() % (size - 0x1c));
    for (size_t cut : cuts) Check(Unpack(block, cut, out, modules.size()) == 0, "a block cut short answers 0", static_cast<long>(cut));

    // garbage: random bytes of the block's length
    for (uint32_t seed = 0; seed < 8; ++seed) {
        std::mt19937 bytes(100 + seed);
        std::vector<uint8_t> garbage(size);
        for (uint8_t& b : garbage) b = static_cast<uint8_t>(bytes());
        Check(Unpack(garbage, size, out, modules.size()) == 0, "random bytes answer 0", seed);
        // the same with a good header in front
        std::memcpy(garbage.data(), block.data(), 0x1c);
        Check(Unpack(garbage, size, out, modules.size()) != modules.size(), "a good header with a random stream does not unpack",
              seed);
        // a good header and zlib's two header bytes, then random bytes
        std::memcpy(garbage.data(), block.data(), 0x1e);
        Check(Unpack(garbage, size, out, modules.size()) != modules.size(), "zlib's header with a random stream does not unpack",
              seed);
    }
    // a good block with bytes changed: one byte, a run of bytes, the checksum's last byte
    for (size_t at : {size_t{0x1e}, size_t{0x40}, size / 5, size / 2, size - 100, size - 5, size - 1}) {
        std::vector<uint8_t> changed = block;
        changed[at] ^= 0x5a;
        Check(Unpack(changed, size, out, modules.size()) != modules.size(), "a block with a byte changed does not unpack",
              static_cast<long>(at));
        std::vector<uint8_t> run = block;
        for (size_t i = at; i < std::min(size, at + 40); ++i) run[i] = static_cast<uint8_t>(i * 31);
        Check(Unpack(run, size, out, modules.size()) != modules.size(), "a block with a run of bytes changed does not unpack",
              static_cast<long>(at));
    }
    // a header that gives the wrong unpacked size, either way (the answer is then not the size the header gives)
    {
        std::vector<uint8_t> small = block;
        SetWord(small.data() + 4, static_cast<uint32_t>(modules.size() - 1));
        Check(Unpack(small, size, out, modules.size()) != modules.size() - 1, "a header one byte short does not unpack");
        std::vector<uint8_t> large = block;
        SetWord(large.data() + 4, static_cast<uint32_t>(modules.size() + 1));
        Check(Unpack(large, size, out, modules.size() + 1) != modules.size() + 1, "a header one byte over does not unpack");
    }
    // the tag and the window size
    {
        std::vector<uint8_t> tag = block;
        tag[0] ^= 1;
        Check(Unpack(tag, size, out, modules.size()) == 0, "a block with another tag answers 0");
        for (uint32_t bits : {0u, 7u, 16u, 31u, 47u, 0xffffffffu}) {
            std::vector<uint8_t> window = block;
            SetWord(window.data() + 0xc, bits);
            Check(Unpack(window, size, out, modules.size()) == 0, "a block with a window size zlib 1.1.3 refuses answers 0", bits);
        }
    }
    // a small block, whole and cut at every byte
    {
        std::vector<uint8_t> tiny(256);
        for (size_t i = 0; i < tiny.size(); ++i) tiny[i] = static_cast<uint8_t>("PART MESS RIDE SHOP "[i % 20]);
        const std::vector<uint8_t> small = Pack(tiny);
        Check(Unpack(small, small.size(), out, tiny.size()) == tiny.size() && out == tiny, "a small block unpacks to its bytes");
        for (size_t cut = 0; cut < small.size(); ++cut)
            Check(Unpack(small, cut, out, tiny.size()) == 0, "a small block cut short answers 0", static_cast<long>(cut));
    }
}

// The block of a park file: the "BILZ" header whose packed size reaches exactly to the file's end.
void ParkFile(const char* path) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        Check(false, "the park file opens");
        return;
    }
    std::vector<uint8_t> bytes;
    uint8_t chunk[65536];
    for (size_t n; (n = std::fread(chunk, 1, sizeof chunk, file)) > 0;) bytes.insert(bytes.end(), chunk, chunk + n);
    std::fclose(file);
    size_t start = 0;
    for (size_t at = 0; at + 0x1c <= bytes.size(); ++at) {
        if (Word(&bytes[at]) == kZLibTag && Word(&bytes[at + 8]) == bytes.size() - at) {
            start = at;
            break;
        }
    }
    Check(start != 0, "the park file has its block");
    if (start == 0) return;
    const std::vector<uint8_t> block(bytes.begin() + static_cast<long>(start), bytes.end());
    const size_t size = block.size();
    const uint32_t unpacked = Word(&block[4]);
    std::printf("%s: header %zu bytes, block %zu bytes, unpacks to %u\n", path, start, size, unpacked);
    std::vector<uint8_t> out;
    Check(Unpack(block, size, out, unpacked) == unpacked, "the park file's block unpacks to its size");
    const std::vector<uint8_t> whole = out;
    std::vector<uint8_t> again;
    Check(Unpack(block, size, again, unpacked) == unpacked && again == whole, "the park file's block unpacks the same twice");
    int cuts = 0;
    for (size_t cut = 0; cut < size; cut += (cut < 0x400 || cut + 0x1000 > size) ? 1 : 97) {
        Check(Unpack(block, cut, out, unpacked) == 0, "the park file cut short answers 0", static_cast<long>(start + cut));
        ++cuts;
    }
    std::printf("%s: %d cuts, from %zu to %zu bytes\n", path, cuts, start, bytes.size() - 1);
}

}  // namespace

int main(int argc, char** argv) {
    Synthetic();
    for (int i = 1; i < argc; ++i) ParkFile(argv[i]);
    std::printf("%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
