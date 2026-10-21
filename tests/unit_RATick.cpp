// Test library for the ride animation tick and the quick vertex copy (unit v5308; libtpw_unit_RATick.dylib, tests/test_RATick.py), main app:
//   src/game/rides/RideAnim.cpp   RATick, QuickCopyUnmaskedMeshVertices
//
// AnimateNode and RAAnimateTextureFrames are recording fakes; gTPGlobals (defined in another file) is defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// gTPGlobals is another file's object whose constructors this library does not compile: its storage, zero at start, under its symbol.
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");

int32_t AnimateNode(sAnimTimeControl* control, tag_sM3D2Header* header, tag_sAnimHeader* animation, tag_sNode* node,
                    tag_sNodeAnim* nodeAnim) {
    ev("AnimateNode", {lab(control), lab(header), lab(animation), lab(node), lab(nodeAnim)});
    return static_cast<int32_t>(ret("AnimateNode"));
}

void RAAnimateTextureFrames(tag_sM3D2Header* header, tag_sAnimHeader* animation, sAnimTimeControl* control) {
    ev("RAAnimateTextureFrames", {lab(header), lab(animation), lab(control)});
}

extern "C" {

uint64_t tpw_ratick_addr(const char* name) {
    if (std::strcmp(name, "globals") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTPGlobalsStorage));
    return 0;
}

uint64_t tpw_ratick_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit word a Ptr32 field holds for the block at `address` (0: null).
uint32_t tpw_ratick_off(uint64_t address) {
    return address != 0 ? static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(address))) : 0u;
}

int32_t tpw_ratick_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNodeAnim));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertexCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mVertices));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mBox));
    out[n++] = static_cast<int64_t>(sizeof(sAlignedBox));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNodeAnim, mNode));
    out[n++] = static_cast<int64_t>(offsetof(tag_sAnimHeader, mNodeAnims));
    out[n++] = static_cast<int64_t>(offsetof(tag_sAnimHeader, mNodeAnimCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    return n;
}

uint32_t tpw_ratick_tick(uint64_t control, uint64_t mesh, uint64_t animation) {
    return static_cast<uint32_t>(RATick(reinterpret_cast<sAnimTimeControl*>(control), reinterpret_cast<tag_sMesh*>(mesh),
                                        reinterpret_cast<tag_sAnimHeader*>(animation)));
}

void tpw_ratick_quick(uint64_t from, uint64_t to, int32_t force) {
    QuickCopyUnmaskedMeshVertices(reinterpret_cast<tag_sMesh*>(from), reinterpret_cast<tag_sMesh*>(to), force);
}

}  // extern "C"
