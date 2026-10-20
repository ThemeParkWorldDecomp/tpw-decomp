// The embedded data of a mesh (the main app's TPEmbed.cpp range): TPSetupEmbeddedHeirarchy
// (0x101ff674) and TPInitialiseEmbeddedData (0x101ff7e4). A mesh's embedded records are the
// header's sEmbeddedData entries (file layout, include/m3d2.h); the set built for them and its
// records use native pointers (include/TPEngine.h).
#include "TPEngine.h"

// 0x101ff674, 280 bytes. Sets up a node and the nodes below it. The entry of a node is its mF50
// less the header's first entry (mF46), and only the entries in range count. An in-range entry
// with flag 0x30 makes the result 0. A node with no child and an in-range entry gets flag 0x20 on
// the matching record of the mesh's set. Each child is set up in turn (the result is 0 if any of
// them is 0), and a node whose result is 1 gets flag 0x40000000.
int32_t TPSetupEmbeddedHeirarchy(tag_sMesh* mesh, tag_sNode* node, tag_sM3D2Header* header) {
    const uint32_t first = header->mF46;
    const uint32_t index = node->mF50;
    const bool inRange = first <= index && index - first < static_cast<uint32_t>(header->mEmbeddedCount);
    int32_t result = 1;
    if (inRange && (header->mEmbedded.get()[index - first].mFlags & 0x30) != 0) result = 0;
    if (inRange && node->mF0C.get() == nullptr) {
        sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
        set->mRecords[index - first].mFlags |= 0x20;
    }
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get()) {
        if (TPSetupEmbeddedHeirarchy(mesh, child, header) == 0) result = 0;
    }
    if (result != 0) node->mFlags |= 0x40000000;
    return result;
}

// 0x101ff7e4, 568 bytes. Builds the mesh's embedded data: the sEmbeddedSet at mesh->mF28 with
// one record per embedded entry of the header (mEmbeddedCount). Entry i belongs to node mF46 + i
// (full nodes first, then mini nodes). An entry with flag 0x30 sets 0x20000000 on its node, flag
// 1 on its record and -1 in the record's word 0xc; the record also gets 0x10 when the node has
// 0x400. With flag 0x40040 the node's mF04 link is flagged 0x10000000, read without a null check
// as the original does. Flag 8 of an entry sets 0x40 on its node, and flag 0x10000 sets 0x20000.
// The set takes a block array with one 0x40-byte block per node flagged 0x20000000 (counted as
// the entries are visited), and the records with flag 1 take those blocks in order. The header's
// root node is then set up with TPSetupEmbeddedHeirarchy, and the set gets flag 8 when an entry
// has flag 2.
void TPInitialiseEmbeddedData(tag_sMesh* mesh) {
    tag_sM3D2Header* header = mesh->mHeader;
    const uint32_t count = header->mEmbeddedCount;
    if (count == 0) return;
    auto* set = static_cast<sEmbeddedSet*>(MReleaseAllocF(sizeof(sEmbeddedSet), 1));
    set->mRecords = static_cast<sEmbeddedRecord*>(MReleaseAllocF(count * sizeof(sEmbeddedRecord), 1));
    mesh->mF28 = set;
    uint32_t flaggedNodes = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t index = header->mF46 + i;
        tag_sMiniNode* node;
        if (index < header->mNodeCount) {
            node = header->mNodes.get() + index;
        } else {
            node = header->mMiniNodes.get() + (index - header->mNodeCount);
        }
        sEmbeddedData& entry = header->mEmbedded.get()[i];
        sEmbeddedRecord& record = set->mRecords[i];
        if ((entry.mFlags & 0x30) != 0) {
            node->mFlags |= 0x20000000;
            record.mFlags |= 1;
            record.mF0C = 0xffffffffu;
            if ((node->mFlags & 0x400) != 0) record.mFlags |= 0x10;
            if ((entry.mFlags & 0x40040) != 0) node->mF04->mFlags |= 0x10000000;
        }
        if ((entry.mFlags & 8) != 0) node->mFlags |= 0x40;
        if ((entry.mFlags & 0x10000) != 0) node->mFlags |= 0x20000;
        if ((node->mFlags & 0x20000000) != 0) ++flaggedNodes;
    }
    if (flaggedNodes != 0) {
        set->mFlags |= 1;
        set->mBlockCount = flaggedNodes;
        set->mBlocks = static_cast<uint8_t*>(MReleaseAllocF(flaggedNodes << 6, 1));
    }
    TPSetupEmbeddedHeirarchy(mesh, header->mF78.get(), header);
    uint8_t* block = set->mBlocks;
    for (uint32_t i = 0; i < count; ++i) {
        sEmbeddedRecord& record = set->mRecords[i];
        if ((record.mFlags & 1) != 0) {
            record.mBlock = block;
            block += 0x40;
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        if ((header->mEmbedded.get()[i].mFlags & 2) != 0) set->mFlags |= 8;
    }
}
