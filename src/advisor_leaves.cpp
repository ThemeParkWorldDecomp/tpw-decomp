// The small functions the advisor calls (main app of SimTheme Park 2.04b3), each from a range of its own:
//   GetDeferredAnim 0x100c7a74 and GetAnimLength 0x100c7bb4 (rsse.cpp, next to the animation tick),
//   SetMeshInUISpace 0x1006553c (TPRide.cpp, the mesh draw switches),
//   FindDataNodeIndexByID 0x101ffdc8 and FindDataNodeIndexByIDAdvisorTempHack 0x101fff90 (TPEmbed.cpp),
//   FindDataNodeIndexByDisableCode 0x1020017c (TPEmbed.cpp, unit v5L21).
// They are here and not in the files of their neighbours so that the advisor's unit library links without the rest of
// those files. (ScaleLMatrixNoTrans is in src/rsse_matrix.cpp and SND_DuckVolume in src/TPSound.cpp.)
#include "LLCellSystem.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "rsse.h"

// A time control is 0x38 bytes (sAnimTimeControl); words 0x24 and 0x28 hold the animation queued behind the playing one.
void GetDeferredAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    const sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[control];
    if (kind != nullptr) *kind = time.mF24;
    if (index != nullptr) *index = time.mF28;
}

// The length of an animation header is in frames (word 8) at 30 a second. The product is a 32-bit unsigned number
// (the original's `mulli` then a multiply-high by 0x88888889 and a shift, which is the division by 30).
uint32_t GetAnimLength(tag_sRideAnimSet* set, uint32_t kind, uint32_t index) {
    if (kind == 0xc) return 0;
    const sRideAnimGroup& group = set->mGroups[kind];
    if ((group.mFlags & 3) == 0 || group.mCount <= index) return 0;
    const uint32_t frames = group.mEntries[index].mAnimation->mF08;
    return (frames * 1000u) / 30u;
}

void SetMeshInUISpace(tag_sMesh*& mesh) { mesh->mHeader->mFlags |= 0x200; }

// A mask with none of the bits the entries use is replaced by the one the original falls back to.
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    if ((mask & 0x3da1f83) == 0) mask = 0x3da1f82;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        const sEmbeddedData& entry = header->mEmbedded[i];
        if (entry.mF04 == id && (mask & entry.mFlags) != 0) return i;
    }
    return 0xffffffffu;
}

// As FindDataNodeIndexByID, but the id is the entry's third word (0x08) and a code with none of the bits the entries use
// is replaced by 1. The original reloads the header on every pass; nothing in the loop writes it.
int32_t FindDataNodeIndexByDisableCode(tag_sRideAnimSet* set, uint32_t code, uint32_t id) {
    if ((code & 0x3da1f82) == 0) code = 1;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        const sEmbeddedData& entry = header->mEmbedded[i];
        if (entry.mF08 == id && (code & entry.mFlags) != 0) return static_cast<int32_t>(i);
    }
    return -1;
}

// Quirks: the nodes are all taken as full nodes (0xa0 bytes), however many of the mAllNodeCount are mini nodes, so a name
// that is not found reads on past the full nodes; the id picks the name from a jump table over -1 to 5 (0 and anything
// else answer 0 straight away), and the first argument is ignored.
uint32_t FindDataNodeIndexByIDAdvisorTempHack(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t i = 0; i < header->mAllNodeCount; ++i) {
        const char* wanted;
        switch (static_cast<int32_t>(id)) {
        case -1: wanted = "bug head"; break;
        case 1: wanted = "mouth - normal"; break;
        case 2: wanted = "mouth - aah"; break;
        case 3: wanted = "mouth - eee"; break;
        case 4: wanted = "mouth - ooh"; break;
        case 5: wanted = "mouth - sss"; break;
        default: return 0;
        }
        if (NS_MacDoze::_stricmp(header->mNodes[i].mName, wanted) == 0) return i;
    }
    return 0;
}
