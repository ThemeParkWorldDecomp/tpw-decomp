// Unit w83: the Bitmap pixel methods (colour_fill, copy_to, swizzle_for_gimex, alphablt,
// shadowblend, colourblt, descimate) over bitmaps the test builds from plain buffers.
//
// The only callees are memcpy (native) and the array operators new[] and delete[]: this file
// replaces both. They record events the PowerPC side records too (kind 1: new[] with its size;
// kind 2: delete[] with the allocation's ordinal and a hash of the block's bytes at that moment),
// fill new blocks with 0xc3 so that reading a byte the code never wrote is the same on both
// sides, and can make the Nth new[] of a run answer NULL (the tests build with -fcheck-new).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Bitmap.h"
#include "Wavelet.h"
#include "platform/samsutils.h"

// Bitmap::load's callees (not under test here, but src/Bitmap.cpp links them).
TbFileReadFn gFileRead = nullptr;
void SamsUtilities::_USwapBlock32(uint32_t*, int32_t) {}

namespace {
constexpr int kMaxBlocks = 16;
constexpr int kMaxEvents = 64;
struct Block {
    void* p;
    std::size_t size;
};
Block gBlocks[kMaxBlocks];
int gBlockCount = 0;
int gNewCalls = 0;
int gFailAt = 0;

uint32_t hash_bytes(const uint8_t* p, std::size_t n) {
    uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

void event(int32_t kind, int32_t a, int32_t b);
}  // namespace

extern "C" {
int32_t tpw_bm_events[kMaxEvents][3];
int32_t tpw_bm_nevents = 0;

// Clears the event log and the allocation record; failAt is the 1-based new[] call that answers NULL (0: none).
void tpw_bm_reset(int32_t failAt) {
    tpw_bm_nevents = 0;
    gBlockCount = 0;
    gNewCalls = 0;
    gFailAt = failAt;
}
}

namespace {
void event(int32_t kind, int32_t a, int32_t b) {
    if (tpw_bm_nevents >= kMaxEvents) return;
    tpw_bm_events[tpw_bm_nevents][0] = kind;
    tpw_bm_events[tpw_bm_nevents][1] = a;
    tpw_bm_events[tpw_bm_nevents][2] = b;
    tpw_bm_nevents++;
}
}  // namespace

void* operator new[](std::size_t size) {
    gNewCalls++;
    event(1, static_cast<int32_t>(size), 0);
    if (gNewCalls == gFailAt) return nullptr;
    void* p = std::malloc(size != 0 ? size : 1);
    std::memset(p, 0xc3, size);
    if (gBlockCount < kMaxBlocks) gBlocks[gBlockCount++] = Block{p, size};
    return p;
}

void operator delete[](void* p) noexcept {
    if (p == nullptr) return;
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].p != p) continue;
        event(2, i + 1, static_cast<int32_t>(hash_bytes(static_cast<uint8_t*>(p), gBlocks[i].size)));
        gBlocks[i].p = nullptr;
        break;
    }
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }

namespace {
// Detaches the buffers so the Bitmap destructor never frees the test's memory.
struct View {
    Bitmap bitmap;
    View(const uint32_t* desc, uint8_t* data) {
        if (desc != nullptr) {
            bitmap.mWidth = desc[0];
            bitmap.mHeight = desc[1];
            bitmap.mBytesPerPixel = desc[2];
        }
        bitmap.mData = data;
    }
    ~View() { bitmap.mData = nullptr; }
};
}  // namespace

extern "C" {
// op: 0 colour_fill(args r, g, b, a), 1 copy_to, 2 swizzle_for_gimex, 3 alphablt(args x, y),
// 4 shadowblend, 5 colourblt(args alpha, r, g, b), 6 descimate. desc: width, height, depth.
int32_t tpw_bm_run(int32_t op, const uint32_t* srcDesc, uint8_t* srcData, const uint32_t* dstDesc, uint8_t* dstData,
                   const uint32_t* args) {
    View src(srcDesc, srcData);
    View dst(dstDesc, dstData);
    switch (op) {
    case 0:
        src.bitmap.colour_fill(static_cast<uint8_t>(args[0]), static_cast<uint8_t>(args[1]),
                               static_cast<uint8_t>(args[2]), static_cast<uint8_t>(args[3]));
        return 0;
    case 1: src.bitmap.copy_to(dst.bitmap); return 0;
    case 2: src.bitmap.swizzle_for_gimex(); return 0;
    case 3: src.bitmap.alphablt(dst.bitmap, args[0], args[1]); return 0;
    case 4: src.bitmap.shadowblend(dst.bitmap); return 0;
    case 5:
        src.bitmap.colourblt(dst.bitmap, static_cast<uint8_t>(args[0]), static_cast<uint8_t>(args[1]),
                             static_cast<uint8_t>(args[2]), static_cast<uint8_t>(args[3]));
        return 0;
    case 6: return src.bitmap.descimate(dst.bitmap);
    }
    return -1;
}
}
