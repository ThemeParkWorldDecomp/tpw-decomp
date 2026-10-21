// Test library for the ride height polygon search (unit v5228; libtpw_unit_GetRideHeightAtPoint.dylib, tests/test_GetRideHeightAtPoint.py), main app:
//   src/game/park/TPHField.cpp   DoNodePolySearch, GetHeightFromPolySearch, GetRideHeightAtPoint, gRideCtx, gRideFlags
//
// Callees outside the unit: MathLib::__fpclassifyf and DetermineHFieldHeightQuick, recording fakes answering from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "Ptr32.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gPolySearchTable;

extern Ptr32<uint8_t> gRideCtxRef;
extern uint32_t gRideFlags;
struct sEmbeddedDataNodeParams;
float DoNodePolySearch(tag_sNode* node, sEmbeddedDataNodeParams* params, float x, float z, s3DPoint* pt, int flag);
float GetHeightFromPolySearch(float x, float z, int a, int b);
void GetRideHeightAtPoint(float x, float z, float& y);

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

namespace MathLib {
int __fpclassifyf(float x) {
    ev("fpclassifyf", {CanonBits(x)});
    return static_cast<int>(ret("fpclassifyf"));
}
}  // namespace MathLib

float DetermineHFieldHeightQuick(float x, float z) {
    ev("DetermineHFieldHeightQuick", {CanonBits(x), CanonBits(z)});
    uint32_t bits = static_cast<uint32_t>(ret("DetermineHFieldHeightQuick"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

extern "C" {

uint64_t tpw_rh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ctx", &gRideCtxRef},
        {"flags", &gRideFlags},
        {"table", &gPolySearchTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_rh_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_rh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gRideCtxRef));
    out[n++] = static_cast<int64_t>(sizeof(gRideFlags));
    out[n++] = static_cast<int64_t>(sizeof(gPolySearchTable));
    return n;
}

float tpw_rh_node(uint64_t node, uint64_t params, float x, float z, uint64_t pt, int32_t flag) {
    return DoNodePolySearch(reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(node)),
                            reinterpret_cast<sEmbeddedDataNodeParams*>(static_cast<uintptr_t>(params)), x, z,
                            reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(pt)), flag);
}

float tpw_rh_height(float x, float z, int32_t a, int32_t b) { return GetHeightFromPolySearch(x, z, a, b); }

void tpw_rh_ride(float x, float z, uint64_t out) { GetRideHeightAtPoint(x, z, *reinterpret_cast<float*>(static_cast<uintptr_t>(out))); }

}  // extern "C"
