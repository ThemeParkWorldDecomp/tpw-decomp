// RATick 0x100c4e20 and QuickCopyUnmaskedMeshVertices 0x100c4f18 (unit v5308).
#include <cstdint>
#include <cstring>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "rsse.h"

// 0x100c4e20, 172 bytes: runs every node animation of `animation` on the mesh's header at the control's frame, then
// (animation flag 2, engine flag 8 clear) steps its animated textures. Always answers 1. A NULL animation does nothing.
int32_t RATick(sAnimTimeControl* control, tag_sMesh* mesh, tag_sAnimHeader* animation) {
    if (animation != nullptr) {
        if (animation->mNodeAnimCount != 0) {
            tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
            for (int32_t i = 0; i < static_cast<int32_t>(animation->mNodeAnimCount); ++i, ++nodeAnim) {
                AnimateNode(control, mesh->mHeader, animation, nodeAnim->mNode.get(), nodeAnim);
            }
        }
        if ((animation->mF00 & 2) != 0 && (gTPGlobals.mEngineFlags & 8) == 0) {
            RAAnimateTextureFrames(mesh->mHeader, animation, control);
        }
    }
    return 1;
}

// 0x100c4f18, 240 bytes: for each of `to`'s full nodes (the count of `to`'s header) that has vertices (mVertexCount)
// and either flag 0x200000 or `force` nonzero: copies `from`'s node's vertices (when both have a vertex block and
// the SOURCE node's (mVertexCount + 3) >> 2 sets are not zero) and always the bounding box. Both meshes' node arrays
// are walked in step.
void QuickCopyUnmaskedMeshVertices(tag_sMesh* from, tag_sMesh* to, int32_t force) {
    tag_sNode* dest = to->mHeader->mNodes.get();
    tag_sNode* source = from->mHeader->mNodes.get();
    for (uint32_t left = to->mHeader->mNodeCount; left != 0; --left, ++dest, ++source) {
        if (dest->mVertexCount == 0) continue;
        if ((dest->mFlags & 0x200000) == 0 && force == 0) continue;
        if (dest->mVertices != nullptr && source->mVertices != nullptr) {
            int32_t sets = (static_cast<int32_t>(source->mVertexCount) + 3) >> 2;
            if (sets != 0) std::memcpy(dest->mVertices.get(), source->mVertices.get(), static_cast<size_t>(sets) * 0x30);
        }
        std::memcpy(&dest->mBox, &source->mBox, sizeof(sAlignedBox));
    }
}
