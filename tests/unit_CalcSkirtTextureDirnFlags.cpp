// Test library for CalcSkirtTextureDirnFlags (unit v5042; libtpw_unit_CalcSkirtTextureDirnFlags.dylib, tests/test_CalcSkirtTextureDirnFlags.py), main app:
//   src/game/park/SkirtDeform.cpp   CalcSkirtTextureDirnFlags, gSkirtCurrentCell, gSkirtCells
//
// The callees outside the unit are recording fakes: LLCC_GetRideBlendInfo (answers gTestBlend), MReleaseAllocF (a zeroed block),
// GetNodeWorldVertPosnsIntoGeomBuffer, StripRideWrapping, ERRORBox and CTRACE. gScratchGeomBuffer and gHeightfieldHeader are
// defined here (other files define them in the program).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"

using namespace calllog;

void* gScratchGeomBuffer;
tag_sM3D2Header* gHeightfieldHeader;
void* gTestBlend;

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

void* LLCC_GetRideBlendInfo(void* shape) {
    ev("LLCC_GetRideBlendInfo", {lab(shape)});
    return gTestBlend;
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {static_cast<int64_t>(size), static_cast<int64_t>(flags)});
    return tpw::ArenaAllocZeroed(size);
}

void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node) { ev("GetNodeWorldVertPosnsIntoGeomBuffer", {lab(node)}); }

void StripRideWrapping(tag_sRideAnimSet* set, tag_sNode* node, float amount, void* data, s3DPoint* point) {
    ev("StripRideWrapping", {lab(set), lab(node), CanonBits(amount), lab(data), lab(point)});
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t a = va_arg(args, int32_t);
    int32_t b = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(static_cast<uint32_t>(a)), static_cast<int64_t>(static_cast<uint32_t>(b))},
       {format});
}

void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    int64_t words[6];
    for (int i = 0; i < 3; ++i) {
        double value = va_arg(args, double);
        uint64_t bits;
        std::memcpy(&bits, &value, 8);
        words[2 * i] = static_cast<int64_t>(bits >> 32);
        words[2 * i + 1] = static_cast<int64_t>(bits & 0xffffffffu);
    }
    va_end(args);
    ev("ERRORBox", {words[0], words[1], words[2], words[3], words[4], words[5]}, {format, name});
}

extern "C" {

uint64_t tpw_skirt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"cur", &gSkirtCurrentCell},
        {"cells", gSkirtCells},
        {"geom", &gScratchGeomBuffer},
        {"hfh", &gHeightfieldHeader},
        {"blend", &gTestBlend},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_skirt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word of a host address.
uint32_t tpw_skirt_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_skirt_layout(int64_t* out) {
    alignas(16) static char set_buf[512];
    alignas(16) static char mesh_buf[512];
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(set_buf);
    auto* mesh = reinterpret_cast<tag_sMesh*>(mesh_buf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&set->mHeads) - set_buf;
    out[n++] = reinterpret_cast<char*>(&mesh->mF00) - mesh_buf;
    out[n++] = reinterpret_cast<char*>(&mesh->mHeader) - mesh_buf;
    out[n++] = reinterpret_cast<char*>(&mesh->mRootNode) - mesh_buf;
    out[n++] = reinterpret_cast<char*>(&mesh->mSkirtVertices) - mesh_buf;
    out[n++] = static_cast<int64_t>(sizeof(SkirtCell));
    out[n++] = static_cast<int64_t>(sizeof(gSkirtCells));
    return n;
}

void tpw_skirt_call(uint64_t set, uint64_t shape, int32_t width, int32_t height, int32_t force) {
    CalcSkirtTextureDirnFlags(reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)),
                              reinterpret_cast<RideShape*>(static_cast<uintptr_t>(shape)), width, height, force);
}

}  // extern "C"
