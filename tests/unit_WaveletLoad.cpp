// Unit w86: Bitmap::load_wavelet (src/Bitmap.cpp) over a scripted file and a fake wavelet
// decompressor, and the fast-decoder dispatcher CWaveletImp::Decompress 0x10039694
// (src/Wavelet_decode.cpp) over fake fast decoders. tests/test_WaveletLoad.py drives them.
//
// Fakes, each recording an event the PowerPC side records too (tests/test_WaveletLoad.py):
//   kind 1  operator new[]: size
//   kind 2  operator delete[]: ordinal of the block, hash of its bytes at that moment
//   kind 3  gFileRead: size, count, whether the handle is the one the case passed
//   kind 4  the fake CWavelet's Decompress: whether this is the fake, width, height, colour,
//           ordinal of the source block and a hash of its bytes, ordinal of the destination block
//           (0: NULL), the three float arguments as bits; it then writes a scripted pattern over
//           the whole destination block so the interleave has bytes to move
//   kind 5  CWaveletImp::DecompressColorFast: this, width, height, source, destination, 3 floats
//   kind 6  CWaveletImp::DecompressMonoFast: this, width, height, source, destination, 1 float
// New blocks are filled with 0xc3. The Nth new[] of a case answers NULL, and so does any new[]
// of more than 1 MiB (the PowerPC side's allocator does the same, so a size that wrapped to a
// huge number never has to be backed).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Bitmap.h"
#include "Wavelet.h"
#include "platform/samsutils.h"

// ---- the engine pieces the other sources of this unit need ---------------------------------
CWavelet::~CWavelet() {}
CWaveletImp::CWaveletImp() : mReferences(1), mBuffer(nullptr) {}
CWaveletImp::~CWaveletImp() {}
int32_t CWaveletImp::AddRef() { return ++mReferences; }
int32_t CWaveletImp::GetRefCount() { return mReferences; }
int32_t CWaveletImp::Release() { return --mReferences; }
int32_t CWaveletImp::Decompress_v12(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) {
    return 0;
}
uint8_t* gWaveletWorkspace = nullptr;
// A little-endian host has the file's words already (the PowerPC side swaps them for real).
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}

namespace {
constexpr int kMaxBlocks = 16;
constexpr int kMaxEvents = 64;
constexpr std::size_t kAllocLimit = 1u << 20;

struct Block {
    void* p;
    std::size_t size;
};
Block gBlocks[kMaxBlocks];
int gBlockCount = 0;
int gNewCalls = 0;
int gFailAt = 0;
int gDecompressCalls = 0;

const uint8_t* gStream = nullptr;
uint32_t gStreamLen = 0;
uint32_t gStreamPos = 0;
char gFileMarker;   // the handle the case's file is

uint32_t hash_bytes(const uint8_t* p, std::size_t n) {
    uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

// 1-based ordinal of the live block starting at p (0: not a block).
int32_t ordinal(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].p == p) return i + 1;
    }
    return 0;
}

const Block* find(const void* p) {
    if (p == nullptr) return nullptr;
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].p == p) return &gBlocks[i];
    }
    return nullptr;
}

uint32_t bits(float value) {
    uint32_t b;
    std::memcpy(&b, &value, sizeof b);
    return b;
}
}  // namespace

extern "C" {
int64_t tpw_wl_events[kMaxEvents][12];
int32_t tpw_wl_nevents = 0;
}

namespace {
template <typename... Values>
void event(int64_t kind, Values... values) {
    if (tpw_wl_nevents >= kMaxEvents) return;
    int64_t* e = tpw_wl_events[tpw_wl_nevents++];
    for (int i = 0; i < 12; ++i) e[i] = 0;
    e[0] = kind;
    int i = 1;
    ((e[i++] = static_cast<int64_t>(values)), ...);
}

// Frees what the last case left allocated and clears the log and counters.
void reset(int32_t failAt) {
    for (int i = 0; i < gBlockCount; ++i) std::free(gBlocks[i].p);
    gBlockCount = 0;
    tpw_wl_nevents = 0;
    gNewCalls = 0;
    gFailAt = failAt;
    gDecompressCalls = 0;
}
}  // namespace

void* operator new[](std::size_t size) {
    gNewCalls++;
    event(1, size);
    if (gNewCalls == gFailAt || size > kAllocLimit) return nullptr;
    void* p = std::malloc(size != 0 ? size : 1);
    std::memset(p, 0xc3, size);
    if (gBlockCount < kMaxBlocks) gBlocks[gBlockCount++] = Block{p, size};
    return p;
}

void operator delete[](void* p) noexcept {
    if (p == nullptr) return;
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].p != p) continue;
        event(2, i + 1, hash_bytes(static_cast<uint8_t*>(p), gBlocks[i].size));
        gBlocks[i].p = nullptr;
        break;
    }
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

namespace {
// The file: bytes from the case's stream, zeros past its end; answers the count.
uint32_t fakeRead(void* dest, uint32_t size, uint32_t count, void* file) {
    event(3, size, count, file == &gFileMarker ? 1 : 0);
    uint32_t n = size * count;
    uint8_t* out = static_cast<uint8_t*>(dest);
    for (uint32_t i = 0; i < n; ++i) {
        out[i] = gStreamPos < gStreamLen ? gStream[gStreamPos] : 0;
        gStreamPos++;
    }
    return count;
}

// The wavelet decompressor the loader calls (slot 0); the others are never reached.
class FakeWavelet : public CWavelet {
public:
    int32_t Decompress(uint32_t width, uint32_t height, bool colour, const uint8_t* source, uint8_t* dest, float a,
                       float b, float c) override {
        gDecompressCalls++;
        const Block* src = find(source);
        const Block* dst = find(dest);
        event(4, this == &instance(), width, height, colour, ordinal(source),
              src != nullptr ? hash_bytes(source, src->size) : 0, ordinal(dest), bits(a), bits(b), bits(c));
        if (dst != nullptr) {
            uint8_t* out = static_cast<uint8_t*>(dest);
            for (std::size_t j = 0; j < dst->size; ++j) {
                out[j] = static_cast<uint8_t>(j * 37 + gDecompressCalls * 101 + 5);
            }
        }
        return 0;
    }
    int32_t Decompress(int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) override {
        return 0;
    }
    int32_t Decompress(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float,
                       float, float, float) override {
        return 0;
    }
    int32_t AddRef() override { return 1; }
    int32_t GetRefCount() override { return 1; }
    int32_t Release() override { return 1; }
    static FakeWavelet& instance() {
        static FakeWavelet object;
        return object;
    }
};

CWaveletImp* Object() {
    static CWaveletImp* object = new CWaveletImp;
    return object;
}
}  // namespace

TbFileReadFn gFileRead = &fakeRead;

// The fast decoders (unit w87's): they record every argument.
void CWaveletImp::DecompressColorFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest, float a,
                                      float b, float c) {
    event(5, this == Object(), width, height, reinterpret_cast<uintptr_t>(source), reinterpret_cast<uintptr_t>(dest),
              bits(a), bits(b), bits(c));
}
void CWaveletImp::DecompressMonoFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest, float a) {
    event(6, this == Object(), width, height, reinterpret_cast<uintptr_t>(source), reinterpret_cast<uintptr_t>(dest),
              bits(a));
}

extern "C" {
// Clears the log, the allocation record and the counters; failAt is the 1-based new[] call that
// answers NULL (0: none).
void tpw_wl_reset(int32_t failAt) { reset(failAt); }

// Runs Bitmap::load_wavelet on a bitmap whose fields are desc (width, height, depth; NULL: all
// zero) and whose buffer holds init (initLen 0xffffffff: no buffer; the buffer is block 1 and
// counts as no new[] call), over the file stream. Answers the result; outDesc gets the bitmap's
// three fields and the buffer's length (0xffffffff: NULL), outData its bytes.
int32_t tpw_wl_load(const uint32_t* desc, const uint8_t* init, uint32_t initLen, const uint8_t* stream,
                    uint32_t streamLen, int32_t failAt, uint32_t* outDesc, uint8_t* outData, uint32_t outCap) {
    reset(0);
    Bitmap bitmap;
    if (desc != nullptr) {
        bitmap.mWidth = desc[0];
        bitmap.mHeight = desc[1];
        bitmap.mBytesPerPixel = desc[2];
    }
    if (initLen != 0xffffffffu) {
        bitmap.mData = new uint8_t[initLen];
        std::memcpy(bitmap.mData, init, initLen);
    }
    tpw_wl_nevents = 0;
    gNewCalls = 0;
    gFailAt = failAt;
    gStream = stream;
    gStreamLen = streamLen;
    gStreamPos = 0;
    int32_t result = bitmap.load_wavelet(&gFileMarker, &FakeWavelet::instance());
    outDesc[0] = bitmap.mWidth;
    outDesc[1] = bitmap.mHeight;
    outDesc[2] = bitmap.mBytesPerPixel;
    if (bitmap.mData == nullptr) {
        outDesc[3] = 0xffffffffu;
    } else {
        const Block* block = find(bitmap.mData);
        std::size_t n = block != nullptr ? block->size : 0;
        outDesc[3] = static_cast<uint32_t>(n);
        std::memcpy(outData, bitmap.mData, n < outCap ? n : outCap);
    }
    bitmap.mData = nullptr;   // the block table frees it
    return result;
}

// The dispatcher on the unit's decompressor object; the pointers are plain numbers (never used).
void tpw_wl_dispatch(int32_t colour, uint32_t width, uint32_t height, uint32_t source, uint32_t dest, float a, float b,
                     float c) {
    reset(0);
    Object()->Decompress(width, height, colour != 0, reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(source)),
                         reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(dest)), a, b, c);
}
}
