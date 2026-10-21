// Test library for the park darkening and the poly search vertices (unit v5902; libtpw_unit_RecalculateParkDarkening.dylib,
// tests/test_RecalculateParkDarkening.py), main app:
//   src/game/park/TPHField.cpp   RecalculateParkDarkening, UpdateHFieldDarkeningNormals, SetupMeshForPolySearchSimple
//
// The callees outside the unit (IsFlag, SetMatrixIdentity, CombineMatricesNoWDest2nd, GetNodeWorldVertPosnsIntoGeomBuffer,
// GetNodePositionSimple, FloatToCell, UpdateLights) are recording fakes that answer from the test's queues. The globals of other
// files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "tb_resource_system.h"

using namespace calllog;

// ---- globals of other files, defined here
void* gPolySearchTable;
int32_t gHeightfieldHeight;
int32_t gHeightfieldWidth;
sHFieldQuad* gHeightfieldQuads;
tag_sM3D2Header* gHeightfieldHeader;
void* gScratchGeomBuffer;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CSystem::CSystem() {}
CSystem gei_System;

static_assert(sizeof(tag_sNode) == 0xa0, "node size");
static_assert(sizeof(tag_sMiniNode) == 0x58, "mini node size");
static_assert(sizeof(sEmbeddedData) == 0x14, "embedded data size");

namespace {

int64_t Chk(const uint32_t* words) {
    uint32_t h = 0;
    for (int i = 0; i < 16; ++i) h = h * 31u + words[i];
    return h;
}

int64_t NodeId(const void* node) { return *reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(node) + 0x50); }

}  // namespace

// ---- the fakes
int32_t CMapCell::IsFlag(uint16_t flag) const {
    int64_t index = (reinterpret_cast<const uint8_t*>(this) - reinterpret_cast<const uint8_t*>(gGameWorld) - 0x2d8) / 0x44;
    ev("IsFlag", {index, flag});
    return static_cast<int32_t>(ret("IsFlag"));
}

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {});
    uint32_t* w = reinterpret_cast<uint32_t*>(matrix);
    for (uint32_t i = 0; i < 16; ++i) w[i] = 0x3f000000u + i * 0x10000u;
}

void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) {
    const uint32_t* s = reinterpret_cast<const uint32_t*>(source);
    uint32_t* d = reinterpret_cast<uint32_t*>(destination);
    ev("CombineMatricesNoWDest2nd", {NodeId(reinterpret_cast<const uint8_t*>(source) - 0x10), Chk(s), Chk(d)});
    for (int i = 0; i < 16; ++i) d[i] = d[i] * 3u + s[i] + 1u;
}

void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node, sMatrix* matrix) {
    ev("GetNodeWorldVertPosnsIntoGeomBuffer", {NodeId(node), Chk(reinterpret_cast<const uint32_t*>(matrix))});
}

void GetNodePositionSimple(tag_sNode* node, s3DPoint* out, int32_t unused) {
    ev("GetNodePositionSimple", {NodeId(node), unused});
    uint32_t bits[3] = {static_cast<uint32_t>(ret("px")), static_cast<uint32_t>(ret("py")), static_cast<uint32_t>(ret("pz"))};
    std::memcpy(out, bits, 12);
}

int32_t FloatToCell(double value) {
    int64_t bits;
    std::memcpy(&bits, &value, 8);
    ev("FloatToCell", {bits});
    return static_cast<int32_t>(ret("FloatToCell"));
}

void CSystem::UpdateLights() { ev("UpdateLights", {}); }

extern "C" {

uint64_t tpw_rpd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gHeightfieldHeader", &gHeightfieldHeader}, {"gHeightfieldWidth", &gHeightfieldWidth},
        {"gHeightfieldHeight", &gHeightfieldHeight}, {"gHeightfieldQuads", &gHeightfieldQuads},
        {"gScratchGeomBuffer", &gScratchGeomBuffer}, {"gPolySearchTable", &gPolySearchTable},
        {"gGameWorld", &gGameWorld},                 {"gParkDarkeningFlag", &gParkDarkeningFlag},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rpd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_rpd_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host offsets of the native-pointer structs: mesh header, rotation, set pointer, root node; set records; record size, record mF10.
int32_t tpw_rpd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF10));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mRootNode));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mF10));
    return n;
}

void tpw_rpd_recalc() { RecalculateParkDarkening(); }

void tpw_rpd_update(uint64_t node, const uint32_t* words) {
    sMatrix m;
    std::memcpy(&m, words, sizeof m);
    UpdateHFieldDarkeningNormals(reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(node)), m);
}

void tpw_rpd_setup(uint64_t mesh, int32_t flag) {
    SetupMeshForPolySearchSimple(reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh)), flag);
}

}  // extern "C"
