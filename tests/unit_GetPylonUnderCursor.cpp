// Test library for GetPylonUnderCursor (unit v5759; libtpw_unit_GetPylonUnderCursor.dylib, tests/test_GetPylonUnderCursor.py), main app:
//   src/game/rides/CoasterPylon.cpp   GetPylonUnderCursor
//
// No callees. The globals of other files the function reads are defined here. The world (instances, pylons, height field header)
// is built through the tpw_gpc_* helpers because the host's structs hold native pointers.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterInstances;
tag_sM3D2Header* gHeightfieldHeader;
s3DPoint gPointerWorldPosn;

extern "C" {

uint64_t tpw_gpc_inst() { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(sizeof(tag_sCoasterInstance))); }
uint64_t tpw_gpc_pylon(uint32_t flags, uint32_t id) {
    sCoasterPylon* p = static_cast<sCoasterPylon*>(tpw::ArenaAllocZeroed(sizeof(sCoasterPylon)));
    p->mFlags = flags;
    p->mF08 = id;
    return reinterpret_cast<uintptr_t>(p);
}
void tpw_gpc_pylon_next(uint64_t p, uint64_t next) {
    reinterpret_cast<sCoasterPylon*>(p)->mNext = reinterpret_cast<sCoasterPylon*>(next);
}
void tpw_gpc_inst_set(uint64_t i, uint64_t next, uint64_t pylons) {
    tag_sCoasterInstance* inst = reinterpret_cast<tag_sCoasterInstance*>(i);
    inst->mNext = reinterpret_cast<tag_sCoasterInstance*>(next);
    inst->mPylons = reinterpret_cast<sCoasterPylon*>(pylons);
}
void tpw_gpc_globals(uint64_t head, uint32_t stride, uint32_t xbits, uint32_t zbits) {
    gCoasterInstances = reinterpret_cast<tag_sCoasterInstance*>(head);
    gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    sHField* hf = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    hf->mF18[0] = stride;
    gHeightfieldHeader->mHField.set(hf);
    std::memcpy(&gPointerWorldPosn.x, &xbits, 4);
    std::memcpy(&gPointerWorldPosn.z, &zbits, 4);
}

// mask: 1 pass a, 2 pass b, 4 pass c
uint64_t tpw_gpc_call(int32_t mask, uint32_t* out) {
    return reinterpret_cast<uintptr_t>(
        GetPylonUnderCursor((mask & 1) ? out : nullptr, (mask & 2) ? out + 1 : nullptr, (mask & 4) ? out + 2 : nullptr));
}

}  // extern "C"
