// Test library for GetCrossSectionFromParams (unit v5208; libtpw_unit_GetCrossSectionFromParams.dylib), main app:
//   src/game/rides/CoasterExtrude.cpp
// No callees and no globals.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

extern "C" {

uint64_t tpw_gcs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gcs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(sCoasterOrientation, mF04));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterOrientation, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterType, mTable));
    out[n++] = static_cast<int64_t>(offsetof(sCoasterType, mCount));
    out[n++] = static_cast<int64_t>(sizeof(sCoasterCrossSectionEntry));
    return n;
}

int32_t tpw_gcs_a(uint64_t type, uint64_t orientation, float angle) {
    return GetCrossSectionFromParams(reinterpret_cast<sCoasterType*>(static_cast<uintptr_t>(type)), 0.0f,
                                     reinterpret_cast<sCoasterOrientation*>(static_cast<uintptr_t>(orientation)), angle);
}

}  // extern "C"
