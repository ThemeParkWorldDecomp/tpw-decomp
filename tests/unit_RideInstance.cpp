// Test library for src/engine/EngineUtil.cpp, src/game/Debug.cpp, src/game/TPEmbed.cpp, src/game/TPInstance.cpp, src/game/rides/RideAnim.cpp, src/game/rides/RidePlacement.cpp, src/game/rides/TPRide.cpp (libtpw_unit_RideInstance.dylib; see tests/test_RideInstance.py):
// InstanceRideAnimSet, InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its
// sibling, CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads, GetFontPath, OverrideActiveChannels,
// ERRORBox and AlignTo32Bit.
//
// The twelve functions call each other for real here (InstanceRideAnimSet runs the real InstanceMesh and
// InstanceAnim). What stays outside is a fake: the allocator (hands out blocks of one pool inside the arena,
// recording what it was asked for), the 64-bit alignment (the engine's, 0x10039078), CreateEngineMesh, the
// debug routine and the game's local path. The records the functions work on are built by the test
// (mesh file records in the arena, from a Python description that also builds the PowerPC side's copy);
// the layout of the game's own records is reported by tpw_ri_layout so the test never guesses it.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Debug.h"
#include "FS_System.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "calllog.h"
#include "fake_strings.h"
#include "platform/arena.h"

using namespace calllog;

uint32_t gGameOptions;

CSYS_TPFileInfo gTPFileInfo;
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    ev("GetLocalPath");
    return mLocalPath;
}

namespace {

constexpr size_t kMargin = 0x100;          // every block has this much room on both sides (labels land in it)
constexpr size_t kPoolSize = 8u << 20;

uint8_t* gPool;
size_t gTop;
std::vector<uint8_t*> gReserved;           // blocks the next allocations take, in order
std::vector<std::pair<uint8_t*, uint32_t>> gAllocations;   // what the fake handed out: address, size asked
uint32_t gMisalign;                        // added to every address the fake allocator answers
uint8_t gFill = 0xa5;                      // what the fake allocator's blocks hold before the function writes them

// A block of the pool, poisoned with 0xa5, with room around it. Returns the address of its first byte.
uint8_t* PoolBlock(size_t size) {
    size_t start = (gTop + 0xf) & ~size_t{0xf};
    gTop = start + size + 2 * kMargin;
    uint8_t* block = gPool + start + kMargin;
    std::memset(block - kMargin, 0xa5, size + 2 * kMargin);
    return block;
}

}  // namespace

// ---- fakes
// The game's allocator: the next reserved block, or a new one of the pool. Recorded as (index, size, flags);
// the test brings the host's sizes to the Mac's (the records the functions lay out in the block are larger
// here). The block's label is 200 + index, on the address it answers and on that address aligned to 8.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    const int32_t index = static_cast<int32_t>(gAllocations.size());
    uint8_t* block;
    if (!gReserved.empty()) {
        block = gReserved.front();
        gReserved.erase(gReserved.begin());
    } else {
        block = PoolBlock(size + 0x10);
    }
    block += gMisalign;
    std::memset(block, gFill, size + 0x10);
    gAllocations.push_back({block, size});
    ev("Alloc", {index, size, flags});
    tpw_cl_label(200 + index, block);
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(block) + 7) & ~uintptr_t{7};
    if (aligned != reinterpret_cast<uintptr_t>(block)) tpw_cl_label(200 + index, reinterpret_cast<void*>(aligned));
    return block;
}

// The engine's alignments (0x10039078, 0x100390cc): up to a multiple of eight, what moved onto *moved.
void AlignTo64Bit(uint32_t& value, uint32_t* moved) {
    const uint32_t aligned = (value + 7) & ~7u;
    if (moved != nullptr) *moved += aligned - value;
    value = aligned;
}
void AlignTo64Bit(void*& address, uint32_t* moved) {
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(address) + 7) & ~uintptr_t{7};
    if (moved != nullptr) *moved += static_cast<uint32_t>(aligned - reinterpret_cast<uintptr_t>(address));
    address = reinterpret_cast<void*>(aligned);
}

void CreateEngineMesh(tag_sMesh* mesh) { ev("CreateEngineMesh", {lab(mesh)}); }

void SamsUtilities::UDebug(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ev("UDebug", {}, {text});
}

// ---- the layout of the game's own records, as JSON
namespace {

std::string gLayout;

// One record's JSON object: its size and the offset of each member that is added.
struct Layout {
    std::string text;
    Layout(const char* name, size_t size) {
        text = std::string("\"") + name + "\":{\"size\":" + std::to_string(size) + ",";
    }
    Layout& field(const char* name, size_t offset) {
        text += std::string("\"") + name + "\":" + std::to_string(offset) + ",";
        return *this;
    }
    std::string done() { return text + "\"end\":0}"; }
};

}  // namespace

// Adds the member `member` of the record type `Record` (set by the line before the chain).
#define FLD(member) .field(#member, offsetof(Record, member))

extern "C" {

// One JSON object: for each record the host lays out natively, its size and the offset of each member.
const char* tpw_ri_layout() {
    std::string out = "{";
    {
        using Record = tag_sMesh;
        out += Layout("mesh", sizeof(Record))
            FLD(mF00) FLD(mHeader) FLD(mF08) FLD(mF0C) FLD(mF10) FLD(mF14) FLD(mEngineMesh) FLD(mTypeEngineMesh)
            FLD(mAllocation) FLD(mF28) FLD(mRootNode) FLD(mF30) FLD(mEngineMeshes) FLD(mF38)
            .done() + ",";
    }
    {
        using Record = sEmbeddedSet;
        out += Layout("eset", sizeof(Record)) FLD(mFlags) FLD(mRecords) FLD(mF08) FLD(mBlockCount) FLD(mBlocks)
            .done() + ",";
    }
    {
        using Record = sEmbeddedRecord;
        out += Layout("erec", sizeof(Record)) FLD(mFlags) FLD(mBlock) FLD(mF08) FLD(mF0C) FLD(mF10).done() + ",";
    }
    {
        using Record = tag_sRideAnimSet;
        out += Layout("set", sizeof(Record))
            FLD(mSelf) FLD(mFlags) FLD(mHeads) FLD(mF0C) FLD(mTimeCount) FLD(mTimeControls) FLD(mF14) FLD(mGroups)
            FLD(mAnimationCount) FLD(mAnimations) FLD(mDynamicMeshSet) FLD(mFC0) FLD(mFC4) FLD(mFC8) FLD(mFCC)
            FLD(mFD0) FLD(mFD4) FLD(mEntryAllocation) FLD(mAnimationHeaders) FLD(mFE0) FLD(mFE4) FLD(mFE8)
            FLD(mFEC) FLD(mFF0) FLD(mFF4) FLD(mFF8) FLD(mInstanceSelf)
            .field("group", sizeof(sRideAnimGroup))
            .field("entry", sizeof(sRideAnimEntry))
            .field("entry_animation", offsetof(sRideAnimEntry, mAnimation))
            .done() + ",";
    }
    {
        using Record = tag_sDMeshSet;
        out += Layout("dmesh", sizeof(Record))
            FLD(mF00) FLD(mCount) FLD(mF06) FLD(mEntries) FLD(mF0C) FLD(mVertexSets) FLD(mPoints) FLD(mUVSets)
            FLD(mFaces) FLD(mF20) FLD(mMesh) FLD(mF28) FLD(mF2C) FLD(mF30) FLD(mAllocation) FLD(mF38) FLD(mShared)
            .done() + ",";
    }
    {
        using Record = sDMeshEntry;
        out += Layout("dentry", sizeof(Record)) FLD(mF00) FLD(mMatrix) FLD(mFaceSets).done() + ",";
    }
    {
        using Record = sDMeshShared;
        out += Layout("shared", sizeof(Record)) FLD(mF00) FLD(mInstances).done();
    }
    out += "}";
    gLayout = out;
    return gLayout.c_str();
}

uint64_t tpw_ri_base() {
    tpw::ArenaAlloc(16);
    return reinterpret_cast<uint64_t>(tpw::gArenaBase);
}

// Starts a case: a new pool, no reserved blocks, no allocations recorded, the globals cleared. `fill` is the byte
// the allocator's blocks hold at first (0xa5 shows what a function leaves unwritten; 0 is what zeroed memory holds).
void tpw_ri_begin(uint32_t misalign, uint32_t fill) {
    if (gPool != nullptr) tpw::ArenaFree(gPool);
    gPool = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(kPoolSize));
    gTop = 0;
    gReserved.clear();
    gAllocations.clear();
    gMisalign = misalign;
    gFill = static_cast<uint8_t>(fill);
    gGameOptions = 0;
    gDefaultHeap = nullptr;
    gPolySearchCount = 0;
}

// A block for the test's records (poisoned, with room around it).
uint64_t tpw_ri_block(uint32_t size) { return reinterpret_cast<uint64_t>(PoolBlock(size)); }

// A block the next allocation answers, so that it can lie below the records built after it.
uint64_t tpw_ri_reserve(uint32_t size) {
    uint8_t* block = PoolBlock(size + 0x10);
    gReserved.push_back(block);
    return reinterpret_cast<uint64_t>(block);
}

// The i-th block the fake allocator answered: its address (the misalignment included) and the size asked for.
int32_t tpw_ri_allocation(int32_t index, uint64_t* address, uint32_t* size) {
    if (index < 0 || index >= static_cast<int32_t>(gAllocations.size())) return 0;
    *address = reinterpret_cast<uint64_t>(gAllocations[index].first);
    *size = gAllocations[index].second;
    return 1;
}

void tpw_ri_set_globals(uint32_t options, uint64_t heap) {
    gGameOptions = options;
    gDefaultHeap = reinterpret_cast<sHeapInfo*>(heap);
}
uint32_t tpw_ri_poly_search_count() { return gPolySearchCount; }
void tpw_ri_set_poly_search_count(uint32_t value) { gPolySearchCount = value; }

// The game's local path (a null pointer for none).
void tpw_ri_set_local_path(const char* text) {
    gTPFileInfo.mLocalPath.mData = const_cast<char*>(text);
}

// ---- the functions under test
int32_t tpw_ri_instance_mesh(tag_sMesh* mesh, uint64_t heap, uint32_t flags, tag_sMesh** output, uint32_t mask) {
    return InstanceMesh(mesh, reinterpret_cast<sHeapInfo*>(heap), flags, output, mask);
}
int32_t tpw_ri_instance_anim(tag_sAnimHeader* animation, uint64_t heap, uint32_t flags, tag_sAnimHeader** output) {
    return InstanceAnim(animation, reinterpret_cast<sHeapInfo*>(heap), flags, output);
}
int32_t tpw_ri_instance_dmesh(tag_sDMeshSet* set, uint64_t heap, uint32_t flags, tag_sDMeshSet** output) {
    return InstanceDynamicMeshSet(set, reinterpret_cast<sHeapInfo*>(heap), flags, output);
}
int32_t tpw_ri_instance_set(tag_sRideAnimSet* set, uint64_t heap, uint32_t flags, tag_sRideAnimSet** output,
                            uint32_t timeCount) {
    return InstanceRideAnimSet(set, reinterpret_cast<sHeapInfo*>(heap), flags, output, timeCount);
}
void tpw_ri_disable(tag_sMesh* mesh, uint32_t mask) { DisableDataNodesRender(mesh, mask); }
void tpw_ri_disable_except(tag_sMesh* mesh, uint32_t mask) { DisableDataNodesRenderExcept(mesh, mask); }
void tpw_ri_setup_heads(tag_sMesh* mesh) { SetupRideHeads(mesh); }
void tpw_ri_correct_face_flags(tag_sMesh* mesh, int32_t all) { CorrectFaceFlags1(mesh, all); }
void tpw_ri_correct_node_flags(tag_sRideAnimSet* set) { CorrectNodeFlags2(set); }
void tpw_ri_override_channels(tag_sRideAnimSet* set) { OverrideActiveChannels(set); }
int32_t tpw_ri_font_path(char* out) { return GetFontPath(out); }
// ERRORBox is variadic; a call through ctypes cannot pass a variable argument list on every host, so
// these pass a text and a number in either order.
void tpw_ri_error_box_text_first(const char* format, const char* text, int32_t number) { ERRORBox(format, text, number); }
void tpw_ri_error_box_number_first(const char* format, int32_t number, const char* text) { ERRORBox(format, number, text); }
// AlignTo32Bit, both forms: the aligned value, and the amount that moved added to *moved.
uint32_t tpw_ri_align32_value(uint32_t value, uint32_t* moved) {
    AlignTo32Bit(value, moved);
    return value;
}
uint64_t tpw_ri_align32_address(uint64_t address, uint32_t* moved) {
    void* pointer = reinterpret_cast<void*>(address);
    AlignTo32Bit(pointer, moved);
    return reinterpret_cast<uint64_t>(pointer);
}

}  // extern "C"
