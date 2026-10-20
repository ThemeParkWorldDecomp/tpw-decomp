// StripRideWrapping (main app, 0x101ee150, 236 bytes; the inventory files it under ui_ErrorDisplay.cpp; unit w75).
#include "TPEngine.h"
#include "m3d2.h"

// Strips the wrapping of one node (`node` given) or of every node of the set's mesh header: node indices run over the header's node
// count at +0x42; an index below the full-node count at +0x44 is in the full nodes (stride 0xa0), the others are mini nodes (stride
// 0x58, counted from the difference); a node with flag 0x200 is skipped when walking (a node given is always done). All calls share
// one counter word, which starts at 0 here. The compares are unsigned (the counts are 16-bit).
void StripRideWrapping(tag_sRideAnimSet* set, tag_sNode* node, float amount, void* data, s3DPoint* point) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    int32_t counter = 0;
    if (node != nullptr) {
        StripNodeWrapping(header, node, amount, data, point, counter);
        return;
    }
    for (uint32_t index = 0; index < header->mAllNodeCount; ++index) {
        tag_sNode* current;
        if (index < header->mNodeCount)
            current = header->mNodes.get() + index;
        else
            current = static_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
        if ((current->mFlags & 0x200) == 0) StripNodeWrapping(header, current, amount, data, point, counter);
    }
}
