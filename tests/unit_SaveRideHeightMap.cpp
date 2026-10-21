// Test library for SaveRideHeightMap (unit v5644; libtpw_unit_SaveRideHeightMap.dylib, tests/test_SaveRideHeightMap.py), main app:
//   src/game/park/TPHField.cpp
//
// The callees outside the unit are fakes recording what the real ones read: MReleaseAllocF, MReleaseFreeF, CTRACE, fopen, fwrite, fclose,
// and (built with the port fixes on, unit v5L44) tpw::GamePathForWriting, the port's user-folder write path.
// memcpy is the real one on both sides.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "TPEngine.h"
#include "Debug.h"
#include "m3d2.h"

using namespace calllog;

namespace {
void* gBlock = nullptr;
}

// The user folder's copy of a game path, faked: "<user>/" and the path with its ':' made '/' (test_SaveRideHeightMap.user_path).
std::string tpw::GamePathForWriting(const char* gamePath) {
    ev("GamePathForWriting", std::initializer_list<int64_t>{}, {gamePath});
    std::string host = std::string("<user>/") + gamePath;
    for (char& c : host)
        if (c == ':') c = '/';
    return host;
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("Alloc", {size, flags});
    gBlock = tpw::ArenaAllocZeroed(size);
    return gBlock;
}

void* MReleaseFreeF(void* block) {
    ev("Free", {block == gBlock ? 1 : 0});
    return nullptr;
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* text = va_arg(args, const char*);
    va_end(args);
    ev("CTRACE", {level}, {format, text});
}

extern "C" {

FILE* fopen(const char* path, const char* mode) {
    ev("fopen", std::initializer_list<int64_t>{}, {path, mode});
    return reinterpret_cast<FILE*>(static_cast<uintptr_t>(ret("fopen")));
}

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* file) {
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    uint64_t h = 0xcbf29ce484222325ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 0x100000001b3ull; };
    uint32_t w;
    uint16_t s;
    std::memcpy(&w, b, 4); mix(w);
    std::memcpy(&w, b + 4, 4); mix(w);
    std::memcpy(&s, b + 8, 2); mix(s);
    std::memcpy(&s, b + 10, 2); mix(s);
    for (int i = 12; i < 48; i += 4) { std::memcpy(&w, b + i, 4); mix(w); }
    for (size_t i = 48; i < size * count; ++i) mix(b[i]);
    ev("fwrite", {static_cast<int64_t>(size), static_cast<int64_t>(count), static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)),
                  static_cast<int64_t>(h & 0x3fffffffffffffffull)});
    return static_cast<size_t>(ret("fwrite"));
}

int fclose(FILE* file) {
    ev("fclose", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    return 0;
}

uint64_t tpw_srhm_addr(const char*) { return 0; }

// 1 when the library is built with the port fixes on (the .hmp goes to the user folder's copy of its path), else 0.
int32_t tpw_srhm_portfix() {
    return 0;
}

// The arena offset a Ptr32 word holds for a host address.
uint32_t tpw_srhm_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

uint64_t tpw_srhm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_srhm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mCells));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mMin));
    out[n++] = static_cast<int64_t>(sizeof(sRideHeightMap));
    return n;
}

uint32_t tpw_srhm_save(uint64_t map, uint64_t path) {
    SaveRideHeightMap(reinterpret_cast<sRideHeightMap*>(static_cast<uintptr_t>(map)),
                      reinterpret_cast<char*>(static_cast<uintptr_t>(path)));
    return 0;
}

}  // extern "C"
