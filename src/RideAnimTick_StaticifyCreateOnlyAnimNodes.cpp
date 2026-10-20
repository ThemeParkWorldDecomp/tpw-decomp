#include "TPEngine.h"
#include "m3d2.h"
#include "tb_resource_system.h"

// 0x100c6b54, 144 bytes. Every node (or mini node) of the set's mesh with flag 0x100 loses flag 0x800;
// if there was one, the view is marked changed. The set loses flag 0x40000000.
void StaticifyCreateOnlyAnimNodes(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    bool changed = false;
    for (uint32_t i = 0; i < header->mAllNodeCount; i++) {
        uint32_t* flags;
        if (i < header->mNodeCount) flags = &header->mNodes.get()[i].mFlags;
        else flags = &header->mMiniNodes.get()[i - header->mNodeCount].mFlags;
        if ((*flags & 0x100) != 0) {
            changed = true;
            *flags &= ~0x800u;
        }
    }
    if (changed) gei_System.mFlags |= CSystem::kViewChanged;
    set->mFlags &= ~0x40000000u;
}
