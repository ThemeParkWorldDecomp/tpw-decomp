// RideHeads differential library: TickRideHeads (src/TPEmbed_heads.cpp) with the matrix functions it calls linked for
// real; the script setters record every argument (floats as bits). The mesh, its header with full nodes and mini nodes,
// the embedded set and the head blocks are built here from the case's numbers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "TPCamera.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "calllog.h"
#include "fake_strings.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
sMatrix* gCurrentCameraMatrix = nullptr;   // 0x102b5af0

// ---- the fakes: the script variable setters, recording every argument
void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SCR_SetScriptPosition", {handle, fbits(x), fbits(y), fbits(z), mode});
}

void SCR_SetVariable(int32_t handle, uint32_t index, int32_t value) {
    ev("SCR_SetVariable", {handle, index, value});
}

void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    ev("SCR_SetFloatVariable", {handle, index, fbits(value)});
}

namespace {

constexpr int kMaxRecords = 4;

std::vector<void*> gBlocks;   // arena blocks of the case

// A zeroed arena block that lives until the next tpw_rh_reset.
void* Block(size_t bytes) {
    void* p = tpw::ArenaAllocZeroed(bytes);
    gBlocks.push_back(p);
    return p;
}

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

}  // namespace

extern "C" {

// Releases the previous case's header and node arrays.
void tpw_rh_reset() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
}

// Builds the sine table of `size` entries with BuildGeometryTables and copies size, mask, scale bits and the entries to
// out (3 + size words), for the PowerPC side to put in its own TPGlobals.
void tpw_rh_table(uint32_t size, uint32_t* out) {
    BuildGeometryTables(size);
    out[0] = gTPGlobals.mSinTableSize;
    out[1] = gTPGlobals.mSinTableMask;
    std::memcpy(&out[2], &gTPGlobals.mSinTableScale, 4);
    std::memcpy(&out[3], gTPGlobals.mSinTable, 4 * size);
}

// GetLMatrixRotationAroundZAxisYOnly of the sixteen words.
float tpw_rh_zrot(const uint32_t* words) {
    sMatrix matrix;
    std::memcpy(&matrix, words, sizeof matrix);
    return GetLMatrixRotationAroundZAxisYOnly(&matrix);
}

// One TickRideHeads call. p: table size, the byte flag, the float static (bits), the camera's sixteen words, the set's
// flags, the header's embedded count, its full node count, its mini node count, its first embedded node (mF46), the flags
// of every node (full then mini), then for each of `embedded count` records: flags, script handle, whether it has a block,
// the block's sixteen words. out: the float static (bits), the byte flag, then the sixteen words of each record's block.
int32_t tpw_rh_run(const int64_t* p, int64_t* out) {
    const uint32_t tableSize = static_cast<uint32_t>(*p++);
    BuildGeometryTables(tableSize);
    gRideHeadsVariable8Set = static_cast<int8_t>(*p++);
    const uint32_t staticBits = static_cast<uint32_t>(*p++);
    gRideHeadsVariable8 = FromBits(staticBits);
    static sMatrix camera;
    uint32_t words[16];
    for (uint32_t& w : words) w = static_cast<uint32_t>(*p++);
    std::memcpy(&camera, words, sizeof camera);
    gCurrentCameraMatrix = &camera;
    const uint32_t setFlags = static_cast<uint32_t>(*p++);
    const uint32_t count = static_cast<uint32_t>(*p++);
    const uint32_t fullNodes = static_cast<uint32_t>(*p++);
    const uint32_t miniNodes = static_cast<uint32_t>(*p++);
    const uint32_t first = static_cast<uint32_t>(*p++);
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    auto* nodes = static_cast<uint8_t*>(Block(0xA0 * (fullNodes + 1)));
    auto* minis = static_cast<uint8_t*>(Block(0x58 * (miniNodes + 1)));
    std::memset(nodes, 0xA5, 0xA0 * (fullNodes + 1));
    std::memset(minis, 0xA5, 0x58 * (miniNodes + 1));
    header->mNodeCount = static_cast<uint16_t>(fullNodes);
    header->mF46 = static_cast<uint16_t>(first);
    header->mEmbeddedCount = static_cast<uint16_t>(count);
    header->mNodes = reinterpret_cast<tag_sNode*>(nodes);
    header->mMiniNodes = reinterpret_cast<tag_sMiniNode*>(minis);
    for (uint32_t i = 0; i < fullNodes + miniNodes; ++i) {
        const uint32_t flags = static_cast<uint32_t>(*p++);
        std::memcpy(i < fullNodes ? nodes + 0xA0 * i : minis + 0x58 * (i - fullNodes), &flags, 4);
    }
    static uint32_t blocks[kMaxRecords][16];
    static sEmbeddedRecord records[kMaxRecords];
    for (uint32_t i = 0; i < count; ++i) {
        records[i].mFlags = static_cast<uint32_t>(*p++);
        records[i].mF0C = static_cast<uint32_t>(*p++);
        const bool hasBlock = *p++ != 0;
        for (uint32_t& w : blocks[i]) w = static_cast<uint32_t>(*p++);
        records[i].mBlock = hasBlock ? reinterpret_cast<uint8_t*>(blocks[i]) : nullptr;
        records[i].mF08 = 0;
        records[i].mF10 = 0;
    }
    static sEmbeddedSet set;
    set.mFlags = setFlags;
    set.mRecords = records;
    static tag_sMesh mesh;
    std::memset(&mesh, 0, sizeof mesh);
    mesh.mHeader = header;
    mesh.mF28 = &set;
    tag_sMesh* meshPointer = &mesh;
    TickRideHeads(meshPointer);
    int n = 0;
    std::memcpy(&out[n], &gRideHeadsVariable8, 4);
    out[n++] &= 0xFFFFFFFF;
    out[n++] = static_cast<uint8_t>(gRideHeadsVariable8Set);
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t w : blocks[i]) out[n++] = w;
    }
    return n;
}

}  // extern "C"
