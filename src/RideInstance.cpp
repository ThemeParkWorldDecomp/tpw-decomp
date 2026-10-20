// A ride's working copy of its animation set (the main app's TPRide.cpp, TPEmbed.cpp, rsse.cpp and
// ui_ErrorDisplay.cpp ranges, as the inventory files them): InstanceRideAnimSet and what it calls
// (InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its sibling), and the
// small functions LoadRideType calls on a loaded set (CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads,
// GetFontPath, OverrideActiveChannels) with ERRORBox.
//
// The copies are made the way the original makes them: one block from the allocator (MReleaseAllocF), the
// pieces laid after each other in it, then every pointer that led into the original's tables made to lead
// into the copies. The mesh and animation records keep the file's layout (Ptr32 fields, arena offsets), so
// the original's arithmetic on addresses is arithmetic on offsets here. The game's own records (tag_sMesh,
// the embedded set and its records, tag_sRideAnimSet and its entries, tag_sDMeshSet) have native pointers
// and are larger than on the Mac, so the block's fixed parts are larger too (see InstanceMesh).
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "platform/samsutils.h"

sHeapInfo* gDefaultHeap;      // 0x102c6c50
uint32_t gPolySearchCount;    // 0x102c1e18

namespace {

// The node of a mesh header with index `index`: the full nodes first, then the mini nodes.
tag_sMiniNode* NodeByIndex(tag_sM3D2Header* header, uint32_t index) {
    if (index < header->mNodeCount) return header->mNodes.get() + index;
    return header->mMiniNodes.get() + (index - header->mNodeCount);
}

// Copies `bytes` from `from` to the end of the block being laid out (`cursor`), when both exist, and moves
// the cursor past them. The original guards every copy like this and moves the cursor inside the guard.
bool CopyAt(void*& cursor, const void* from, size_t bytes) {
    if (cursor == nullptr || from == nullptr || bytes == 0) return false;
    std::memcpy(cursor, from, bytes);
    cursor = static_cast<uint8_t*>(cursor) + bytes;
    return true;
}

}  // namespace

// 0x10039120 and 0x10039174, 44 bytes each: rounds a size or an address up to a multiple of four; what was
// added goes onto *moved.
void AlignTo32Bit(uint32_t& value, uint32_t* moved) {
    const uint32_t aligned = (value + 3) & ~3u;
    if (moved != nullptr) *moved += aligned - value;
    value = aligned;
}
void AlignTo32Bit(void*& address, uint32_t* moved) {
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(address) + 3) & ~static_cast<uintptr_t>(3);
    if (moved != nullptr) *moved += static_cast<uint32_t>(aligned - reinterpret_cast<uintptr_t>(address));
    address = reinterpret_cast<void*>(aligned);
}

// 0x1006fd3c, 32 bytes. The original is an empty variadic function: it spills the argument registers into
// the caller's frame and returns, so every message is dropped (and the debug routine next to it, UDebug,
// is empty too). The port writes the text to the log through UDebug, which the platform layer prints.
// Nothing else happens: no dialog, no exit.
void ERRORBox(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    SamsUtilities::UDebug("ERRORBox: %s", text);
}

// 0x100c609c, 32 bytes. Marks a set as animating through its channels: the word at 0x14 becomes 1 (a pointer
// field in the game's other uses, hence the cast) and the mesh header gets flag 0x80000.
void OverrideActiveChannels(tag_sRideAnimSet* set) {
    set->mF14 = reinterpret_cast<void*>(1);
    set->mHeads->mHeader->mFlags |= 0x80000;
}

// 0x101ffc4c and 0x101ffd08, 128 bytes each. Hides the nodes of the embedded entries that `mask` selects:
// the entry's word 8 shares a bit with it (the first) or is not equal to it (the second). Entry i belongs to
// node mF46 + i; the header is read again on every step, as the original does through the reference.
void DisableDataNodesRender(tag_sMesh*& mesh, uint32_t mask) {
    for (uint32_t i = 0; i < mesh->mHeader->mEmbeddedCount; ++i) {
        tag_sM3D2Header* header = mesh->mHeader;
        if ((mask & header->mEmbedded.get()[i].mF08) != 0) EmbeddedNode(header, i)->mFlags |= 0x80000010;
    }
}

void DisableDataNodesRenderExcept(tag_sMesh*& mesh, uint32_t mask) {
    for (uint32_t i = 0; i < mesh->mHeader->mEmbeddedCount; ++i) {
        tag_sM3D2Header* header = mesh->mHeader;
        if (mask != header->mEmbedded.get()[i].mF08) EmbeddedNode(header, i)->mFlags |= 0x80000010;
    }
}

// 0x10200b28, 152 bytes. The nodes of the embedded entries with flag 0x80 (the ride's heads) are hidden, and
// the mesh's embedded set gets flag 2 for each (TickRideHeads runs when the mesh has an embedded set).
void SetupRideHeads(tag_sMesh*& mesh) {
    const uint32_t count = mesh->mHeader->mEmbeddedCount;
    for (uint32_t i = 0; i < count; ++i) {
        tag_sM3D2Header* header = mesh->mHeader;
        if ((header->mEmbedded.get()[i].mFlags & 0x80) != 0) {
            EmbeddedNode(header, i)->mFlags |= 0x80000010;
            static_cast<sEmbeddedSet*>(mesh->mF28)->mFlags |= 2;
        }
    }
}

// 0x10067fdc, 236 bytes. Marks the mesh's embedded records (flag 8) for the entries that have one of the
// flags 0x580f00, or all of them when `all` is nonzero, and raises gPolySearchCount to the vertex count the
// mesh adds up to: the root node's, plus those of the nodes of the entries with flag 2. (A mini node has no
// vertex count: the original reads the two bytes after it, which the next record owns. The Mac reads them
// big-endian and the host little-endian, so they agree only where they are zero.)
void CorrectFaceFlags1(tag_sMesh* mesh, int32_t all) {
    tag_sM3D2Header* header = mesh->mHeader;
    uint32_t count = 0;
    if (mesh->mRootNode != nullptr) count += mesh->mRootNode->mVertexCount;
    const sEmbeddedData* entry = header->mEmbedded.get();
    sEmbeddedRecord* record = nullptr;
    if (mesh->mF28 != nullptr) record = static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i, ++entry) {
        if ((entry->mFlags & 2) != 0) count += static_cast<const tag_sNode*>(EmbeddedNode(header, i))->mVertexCount;
        if (record != nullptr && ((entry->mFlags & 0x580f00) != 0 || all != 0)) record->mFlags |= 8;
        if (record != nullptr) ++record;
    }
    if (count > gPolySearchCount) gPolySearchCount = count;
}

// 0x10067e1c, 388 bytes. Last corrections on a loaded set's mesh. Every node: a mini node (flag 0x200) is
// hidden (0x80000010); a node with flag 0x20000 or 0x40 gives the header flag 0x80; a full node's face sets
// whose texture has flag 0x80 get face set flag 2; a node whose children are all mini nodes without
// children of their own (and which has some) gets flag 0x20. Then the textures: one with flag 0x80 or 0x40
// gives the header flag 0x1000, and one whose info has flag 1 gets texture flag 0x40.
void CorrectNodeFlags2(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t i = 0; i < header->mAllNodeCount; ++i) {
        tag_sMiniNode* node = NodeByIndex(header, i);
        if ((node->mFlags & 0x200) != 0) node->mFlags |= 0x80000010;
        if ((node->mFlags & 0x20040) != 0) header->mFlags |= 0x80;
        if ((node->mFlags & 0x200) == 0) {
            tag_sNode* full = static_cast<tag_sNode*>(node);
            sFaceSet* faceSet = full->mFaceSets;
            for (uint32_t j = 0; j < full->mFaceSetCount; ++j, ++faceSet) {
                if (faceSet->mTexture != nullptr && (faceSet->mTexture->mF00 & 0x80) != 0) faceSet->mF0C[0] |= 2;
            }
        }
        bool onlyLeaves = true;
        for (tag_sNode* child = node->mF0C; child != nullptr; child = child->mF08) {
            if ((child->mFlags & 0x200) == 0 || child->mF0C != nullptr) {
                onlyLeaves = false;
                break;
            }
        }
        if (onlyLeaves && node->mF0C != nullptr) node->mFlags |= 0x20;
    }
    sTexture* texture = header->mTextures;
    sTextureInfo* info = header->mTextureInfos;
    for (uint32_t n = header->mTextureCount; n != 0; --n, ++texture, ++info) {
        if ((texture->mF00 & 0xc0) != 0) header->mFlags |= 0x1000;
        if ((info->mFlags & 1) != 0) texture->mF00 |= 0x40;
    }
}

// 0x101eeac8, 176 bytes. The folder of the game's fonts: the local path, a ':' unless it already ends in
// one, and "Data:Fonts". 0 (and `out` untouched) when there is no local path. For an empty path the original
// reads the byte before the string; here it counts as not a ':'.
int32_t GetFontPath(char* out) {
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    const size_t length = std::strlen(path);
    if (length != 0 && path[length - 1] == ':') std::sprintf(out, "%s%s", path, "Data:Fonts");
    else std::sprintf(out, "%s:%s", path, "Data:Fonts");
    return 1;
}

// 0x10062938, 540 bytes. Two passes over the node animations. The first adds up the block's size: the
// header, its node animations, and for each node animation with flag 0x1000 and not 0x4000 a morph set
// (0x34 bytes copied, though the record is 0x2c) and its key data (0x14 each). The second allocates it,
// copies the header (0x48 bytes) and the node animations, points the new header's last word at the block
// and gives each such node animation a copy of its morph set whose key data pointer leads to a copy of the
// key data. The flags argument and the heap are not used (the original substitutes the default heap for a
// missing one and does nothing with it).
int32_t InstanceAnim(tag_sAnimHeader* animation, sHeapInfo* heap, uint32_t flags, tag_sAnimHeader** output) {
    tag_sAnimHeader* copy = nullptr;
    tag_sNodeAnim* copyAnim = nullptr;
    void* cursor = nullptr;
    uint32_t size = static_cast<uint32_t>(animation->mNodeAnimCount) * 0x40 + 0x48;
    for (uint32_t pass = 0; pass < 2; ++pass) {
        tag_sNodeAnim* anim = animation->mNodeAnims;
        if (pass != 0) {
            void* block = MReleaseAllocF(size, 0);
            copy = static_cast<tag_sAnimHeader*>(block);
            std::memcpy(copy, animation, 0x48);
            cursor = static_cast<uint8_t*>(block) + 0x48;
            *output = copy;
            copy->mF44 = block;
            if (animation->mNodeAnims != nullptr) {
                copy->mNodeAnims = static_cast<tag_sNodeAnim*>(cursor);
                CopyAt(cursor, animation->mNodeAnims.get(), static_cast<size_t>(animation->mNodeAnimCount) * 0x40);
            }
            copyAnim = copy->mNodeAnims;
        }
        for (uint32_t n = animation->mNodeAnimCount; n != 0; --n) {
            if ((anim->mFlags & 0x1000) != 0 && (anim->mFlags & 0x4000) == 0) {
                if (pass != 0) {
                    copyAnim->mMorph = static_cast<sMorphSet*>(cursor);
                    CopyAt(cursor, anim->mMorph.get(), 0x34);
                    copyAnim->mMorph->mKeyData = static_cast<sCMKeyData*>(cursor);
                    if (cursor != nullptr) {
                        const sMorphSet* morph = anim->mMorph;
                        CopyAt(cursor, morph->mKeyData.get(), static_cast<size_t>(morph->mKeyDataCount) * 0x14);
                    }
                } else {
                    size += static_cast<uint32_t>(anim->mMorph->mKeyDataCount) * 0x14 + 0x34;
                }
            }
            ++anim;
            if (copyAnim != nullptr) ++copyAnim;
        }
    }
    return 1;
}

// 0x10062ba8, 536 bytes. An instance of a dynamic mesh set: the set's record and its entries in one block
// (0x44 and 0x30 each on the Mac), the entries copied; then an instance of the set's mesh (all nodes, no
// mask) with its engine mesh. The set takes four of the new header's table pointers, each entry after the
// first (when there is more than one) takes the matrix and face sets of the instance's node, and the shared
// record's instance count rises by one. The answer is InstanceMesh's, 1.
int32_t InstanceDynamicMeshSet(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags, tag_sDMeshSet** output) {
    if (heap == nullptr) heap = gDefaultHeap;
    uint32_t size = static_cast<uint32_t>(set->mCount) * sizeof(sDMeshEntry) + sizeof(tag_sDMeshSet) + 4;
    void* block = MReleaseAllocF(size, 0);
    void* cursor = block;
    AlignTo64Bit(cursor, nullptr);
    auto* copy = static_cast<tag_sDMeshSet*>(cursor);
    cursor = static_cast<uint8_t*>(cursor) + sizeof(tag_sDMeshSet);
    *copy = *set;
    copy->mAllocation = block;
    copy->mEntries = static_cast<sDMeshEntry*>(cursor);
    for (int32_t i = 0; i < static_cast<int32_t>(copy->mCount); ++i) copy->mEntries[i] = set->mEntries[i];
    const int32_t answer = InstanceMesh(set->mMesh, heap, flags, &copy->mMesh, 0);
    CreateEngineMesh(copy->mMesh);
    tag_sM3D2Header* header = copy->mMesh->mHeader;
    copy->mVertexSets = header->mVertexSets;
    copy->mPoints = header->mPoints;
    copy->mUVSets = header->mUVSets;
    copy->mFaces = header->mFaces;
    tag_sNode* node = header->mNodes;
    if (copy->mCount > 1) ++node;
    for (int32_t i = 0; i < static_cast<int32_t>(copy->mCount); ++i, ++node) {
        copy->mEntries[i].mMatrix = &node->mMatrix;
        copy->mEntries[i].mFaceSets = node->mFaceSets;
    }
    ++copy->mShared->mInstances;
    *output = copy;
    return answer;
}

// 0x10061b1c, 3,540 bytes. See the declaration for what the flags ask for.
//
// The block, in order: the game's record (0x40 bytes on the Mac, sizeof(tag_sMesh) here), a copy of the header
// (0xb8), the nodes and mini nodes, the points, the vertex sets, the faces, the face sets, the UV sets, the
// textures, the texture infos with one 0x16-byte name for each info with flag 2, and last the embedded set
// with its records and blocks. The block is aligned to eight bytes, and so is each table that is followed by
// another table of floats (the textures, infos and names are not). The size asked from the allocator is the
// total, plus what the alignments added (counted twice, as the original does), plus four.
//
// Quirks kept. (1) Flag 0x20 gives only the FIRST node the copied faces (the original's loop does not move
// on), so the other nodes keep the original's. (2) Without flag 1 the header copy still points at the
// original's nodes, and flags 0x20 or 0x4/0x100 then change the original's node records. (3) Flag 2 is
// added after flag 1 and never read. (4) The default heap is not used, and a missing header node
// (flag 1 absent) is read without a check. (5) The first word of each copied texture name loses bit 1 as a
// 32-bit word of the host's byte order, as the Mac clears it in its own.
// On the host the embedded set and its records have native pointers, so they are aligned to eight bytes
// first and take more room; the allocation grows by that room and eight bytes of slack.
int32_t InstanceMesh(tag_sMesh* mesh, sHeapInfo* heap, uint32_t flags, tag_sMesh** output, uint32_t mask) {
    tag_sM3D2Header* header = mesh->mHeader;
    const bool copyTextures = (flags & 0x200) != 0 || (header->mFlags & 0x1000) != 0 ||
                              ((header->mFlags & 8) != 0 && (gGameOptions & 0x2000) != 0);
    if (copyTextures) flags |= 0x40;
    if ((flags & 4) != 0 && (flags & 1) == 0) flags |= 3;
    if ((flags & 4) != 0) flags |= 0x100;
    if ((flags & 1) != 0) flags |= 2;

    // The size of the block.
    uint32_t moved = 0;
    uint32_t size = sizeof(tag_sMesh) + sizeof(tag_sM3D2Header);
    AlignTo64Bit(size, &moved);
    if ((flags & 1) != 0) {
        size += static_cast<uint32_t>(header->mNodeCount) * 0xa0 +
                (static_cast<uint32_t>(header->mAllNodeCount) - header->mNodeCount) * 0x58;
    } else if ((header->mF78->mFlags & 0x200) != 0) {
        size += 0x58;
    } else {
        size += 0xa0;
    }
    AlignTo64Bit(size, &moved);
    if ((flags & 4) != 0) {
        size += static_cast<uint32_t>(header->mVertexSetCount) * 0x30;
        AlignTo64Bit(size, &moved);
    }
    if ((flags & 8) != 0) {
        size += static_cast<uint32_t>(header->mPointCount) * 0xc;
        AlignTo64Bit(size, &moved);
    }
    if ((flags & 0x20) != 0) {
        size += static_cast<uint32_t>(header->mFaceCount) * 8;
        AlignTo64Bit(size, &moved);
    }
    if ((flags & 0x40) != 0) {
        size += static_cast<uint32_t>(header->mFaceSetCount) * 0x10;
        AlignTo64Bit(size, &moved);
    }
    uint32_t uvSets = 0;
    if ((flags & 0x90) != 0) {
        if ((flags & 0x80) != 0) {
            uvSets = header->mUVSetCount;
        } else {
            const tag_sNode* node = header->mNodes;
            for (uint32_t n = header->mNodeCount; n != 0; --n, ++node) {
                if ((node->mFlags & 0x80) != 0) uvSets += (node->mF5E >> 2) + 1;
            }
        }
    }
    size += uvSets * 0x20;
    AlignTo64Bit(size, &moved);
    if (copyTextures) size += static_cast<uint32_t>(header->mTextureCount) * 8;
    if ((header->mFlags & 0x18000) != 0) {
        uint32_t named = 2;
        if ((header->mFlags & 0x10000) != 0) {
            named = 0;
            const sTextureInfo* info = header->mTextureInfos;
            for (uint32_t i = 0; i < header->mTextureCount; ++i, ++info) {
                if ((info->mFlags & 2) != 0) ++named;
            }
        }
        size += static_cast<uint32_t>(header->mTextureCount) * 0x10 + named * 0x16;
    }
    if (header->mEmbeddedCount != 0) {
        const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(mesh->mF28);
        size += static_cast<uint32_t>(header->mEmbeddedCount) * sizeof(sEmbeddedRecord) +
                embedded->mBlockCount * 0x40 + sizeof(sEmbeddedSet) + 8;
    }

    // The block, the record and the header copy.
    void* raw = MReleaseAllocF(size + moved + 4, 0);
    void* cursor = raw;
    AlignTo64Bit(cursor, nullptr);
    auto* copy = static_cast<tag_sMesh*>(cursor);
    auto* newHeader = reinterpret_cast<tag_sM3D2Header*>(static_cast<uint8_t*>(cursor) + sizeof(tag_sMesh));
    cursor = reinterpret_cast<uint8_t*>(newHeader) + sizeof(tag_sM3D2Header);
    *copy = *mesh;
    std::memcpy(newHeader, header, sizeof(tag_sM3D2Header));
    copy->mAllocation = raw;
    copy->mF08 = mesh->mF08 != nullptr ? mesh->mF08 : mesh;
    copy->mHeader = newHeader;
    copy->mF00 &= ~1u;
    copy->mEngineMesh = nullptr;

    // The nodes. Links are offsets from the arena: the copies lie `delta` away from the originals, below
    // them (subtract) or above.
    if ((flags & 1) != 0) {
        if (header->mNodes.mOffset != 0) {
            if (newHeader->mNodeCount != 0) {
                newHeader->mNodes = static_cast<tag_sNode*>(cursor);
                CopyAt(cursor, header->mNodes.get(), static_cast<size_t>(newHeader->mNodeCount) * 0xa0);
            }
            const uint32_t miniCount = static_cast<uint32_t>(newHeader->mAllNodeCount) - newHeader->mNodeCount;
            if (miniCount != 0) {
                newHeader->mMiniNodes = static_cast<tag_sMiniNode*>(cursor);
                CopyAt(cursor, header->mMiniNodes.get(), static_cast<size_t>(miniCount) * 0x58);
            }
            const bool subtract = newHeader->mNodes.mOffset <= header->mNodes.mOffset;
            const uint32_t delta = subtract ? header->mNodes.mOffset - newHeader->mNodes.mOffset
                                            : newHeader->mNodes.mOffset - header->mNodes.mOffset;
            for (uint32_t i = 0; i < newHeader->mAllNodeCount; ++i) {
                tag_sMiniNode* node = NodeByIndex(newHeader, i);
                for (Ptr32<tag_sNode>* link : {&node->mF04, &node->mF08, &node->mF0C}) {
                    if (link->mOffset != 0) link->mOffset = subtract ? link->mOffset - delta : link->mOffset + delta;
                }
                if ((node->mFlags & 0x4000) != 0) copy->mRootNode = static_cast<tag_sNode*>(node);
            }
            if (newHeader->mF78.mOffset != 0) {
                newHeader->mF78.mOffset = subtract ? newHeader->mF78.mOffset - delta : newHeader->mF78.mOffset + delta;
            }
        }
    } else if (newHeader->mF78.mOffset != 0) {
        newHeader->mF78 = static_cast<tag_sNode*>(cursor);
        const tag_sNode* node = header->mF78;
        const size_t bytes = (node->mFlags & 0x200) != 0 ? 0x58 : 0xa0;
        CopyAt(cursor, node, bytes);
    }
    AlignTo64Bit(cursor, nullptr);

    if ((flags & 8) != 0) {
        newHeader->mPoints = static_cast<s3DPoint*>(cursor);
        CopyAt(cursor, header->mPoints.get(), static_cast<size_t>(newHeader->mPointCount) * 0xc);
        AlignTo64Bit(cursor, nullptr);
    }

    // The vertex sets, and the nodes' vertices and points as the same distance into the copies.
    if ((flags & 4) != 0 && (flags & 0x100) != 0 && header->mVertexSets.mOffset != 0) {
        newHeader->mVertexSets = static_cast<sVertexPosnSet*>(cursor);
        CopyAt(cursor, header->mVertexSets.get(), static_cast<size_t>(newHeader->mVertexSetCount) * 0x30);
        AlignTo64Bit(cursor, nullptr);
        tag_sNode* to = newHeader->mNodes;
        const tag_sNode* from = header->mNodes;
        for (uint32_t n = newHeader->mNodeCount; n != 0; --n, ++to, ++from) {
            if (to->mVertices.mOffset != 0) {
                const int32_t distance = static_cast<int32_t>(from->mVertices.mOffset - header->mVertexSets.mOffset);
                to->mVertices.mOffset = newHeader->mVertexSets.mOffset + (distance / 0x30) * 0x30;
            }
            if (to->mPoints.mOffset != 0) {
                const int32_t distance = static_cast<int32_t>(from->mPoints.mOffset - header->mPoints.mOffset);
                to->mPoints.mOffset = newHeader->mPoints.mOffset + (distance / 0xc) * 0xc;
            }
        }
    }

    // The faces: only the first node is given the copy.
    if ((flags & 0x20) != 0) {
        newHeader->mFaces = static_cast<sFace*>(cursor);
        CopyAt(cursor, header->mFaces.get(), static_cast<size_t>(newHeader->mFaceCount) * 8);
        AlignTo64Bit(cursor, nullptr);
        if (header->mNodeCount != 0) newHeader->mNodes.get()->mFaces = newHeader->mFaces;
    }

    // The face sets, and the nodes' pointers into them.
    if ((flags & 0x40) != 0) {
        newHeader->mFaceSets = static_cast<sFaceSet*>(cursor);
        CopyAt(cursor, header->mFaceSets.get(), static_cast<size_t>(newHeader->mFaceSetCount) * 0x10);
        AlignTo64Bit(cursor, nullptr);
        const tag_sNode* from = header->mNodes;
        tag_sNode* to = newHeader->mNodes;
        for (uint32_t n = header->mNodeCount; n != 0; --n, ++from, ++to) {
            const int32_t distance = static_cast<int32_t>(from->mFaceSets.mOffset - header->mFaceSets.mOffset);
            to->mFaceSets.mOffset = newHeader->mFaceSets.mOffset + (distance / 0x10) * 0x10;
        }
    }

    // The UV sets: all of them (0x80), or each node's own (0x10, for the nodes with flag 0x80).
    if ((flags & 0x90) != 0) {
        if ((flags & 0x80) != 0) {
            newHeader->mUVSets = static_cast<sUVSet*>(cursor);
            CopyAt(cursor, header->mUVSets.get(), static_cast<size_t>(newHeader->mUVSetCount) * 0x20);
            AlignTo64Bit(cursor, nullptr);
            tag_sNode* to = newHeader->mNodes;
            for (uint32_t n = header->mNodeCount; n != 0; --n, ++to) to->mUVs = newHeader->mUVSets;
        } else {
            const tag_sNode* from = header->mNodes;
            tag_sNode* to = newHeader->mNodes;
            for (uint32_t n = header->mNodeCount; n != 0; --n, ++from, ++to) {
                if ((from->mFlags & 0x80) == 0) continue;
                const size_t sets = (from->mF5E >> 2) + 1;
                to->mUVs = static_cast<sUVSet*>(cursor);
                CopyAt(cursor, from->mUVs.get(), sets * 0x20);
            }
        }
    }

    // The textures (the face sets' texture pointers move into the copy).
    if (copyTextures) {
        newHeader->mTextures = static_cast<sTexture*>(cursor);
        CopyAt(cursor, header->mTextures.get(), static_cast<size_t>(header->mTextureCount) * 8);
        tag_sNode* to = newHeader->mNodes;
        for (uint32_t n = header->mNodeCount; n != 0; --n, ++to) {
            sFaceSet* faceSet = to->mFaceSets;
            for (uint32_t k = to->mFaceSetCount; k != 0; --k, ++faceSet) {
                if (faceSet->mTexture.mOffset != 0) {
                    const int32_t distance = static_cast<int32_t>(faceSet->mTexture.mOffset - header->mTextures.mOffset);
                    faceSet->mTexture.mOffset = newHeader->mTextures.mOffset + (distance / 8) * 8;
                }
            }
        }
    }

    // The texture infos, and a name of 0x16 bytes for each info with flag 2.
    if ((header->mFlags & 0x18000) != 0) {
        newHeader->mTextureInfos = static_cast<sTextureInfo*>(cursor);
        CopyAt(cursor, header->mTextureInfos.get(), static_cast<size_t>(header->mTextureCount) * 0x10);
        const sTextureInfo* from = header->mTextureInfos;
        sTextureInfo* to = newHeader->mTextureInfos;
        for (uint32_t i = 0; i < header->mTextureCount; ++i, ++from, ++to) {
            if ((from->mFlags & 2) == 0) continue;
            to->mName = static_cast<char*>(cursor);
            CopyAt(cursor, from->mName.get(), 0x16);
            uint32_t word;
            std::memcpy(&word, to->mName.get(), 4);
            word &= ~2u;
            std::memcpy(to->mName.get(), &word, 4);
        }
    }

    // The embedded set, its records and its blocks.
    if (header->mEmbeddedCount != 0) {
        AlignTo64Bit(cursor, nullptr);
        const auto* from = static_cast<const sEmbeddedSet*>(mesh->mF28);
        auto* to = static_cast<sEmbeddedSet*>(cursor);
        copy->mF28 = to;
        if (from != nullptr) {
            *to = *from;
            cursor = to + 1;
        }
        to->mRecords = static_cast<sEmbeddedRecord*>(cursor);
        CopyAt(cursor, from->mRecords, static_cast<size_t>(header->mEmbeddedCount) * sizeof(sEmbeddedRecord));
        if (from->mBlockCount != 0) {
            to->mBlocks = static_cast<uint8_t*>(cursor);
            CopyAt(cursor, from->mBlocks, static_cast<size_t>(from->mBlockCount) * 0x40);
            uint8_t* block = to->mBlocks;
            sEmbeddedRecord* record = to->mRecords;
            for (uint32_t n = copy->mHeader->mEmbeddedCount; n != 0; --n, ++record) {
                if ((record->mFlags & 1) != 0) {
                    record->mBlock = block;
                    block += 0x40;
                }
            }
        }
    }

    *output = copy;
    tag_sMesh* instance = copy;
    if ((flags & 0x1000) != 0) DisableDataNodesRenderExcept(instance, mask);
    else DisableDataNodesRender(instance, mask);
    return 1;
}

// 0x10062e1c, 840 bytes. The block: the copy of the set (0xfc bytes on the Mac, sizeof(tag_sRideAnimSet)
// here), a copy of the time controls (0x38 each), the array of animation headers (4 bytes each on the Mac,
// a native pointer here) and the entries of all the groups (8 bytes each on the Mac, 16 here), the last two
// as many as there are animations. Each piece is aligned to four bytes (the block is aligned already, and
// all the sizes are multiples of eight, so nothing moves). The copy's word 0xe0 points at the copy itself
// (mInstanceSelf) and its first word at the original set. Each group's entries point at the copies'
// animations. The tests of the original that skip the animations never fire: flag 0x800 is cleared from the
// flags before it is tested.
int32_t InstanceRideAnimSet(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags, tag_sRideAnimSet** output,
                            uint32_t timeCount) {
    if (set == nullptr) return 0;
    const uint32_t meshFlags = flags & ~0x800u;
    tag_sMesh* mesh;
    InstanceMesh(set->mHeads, heap, meshFlags, &mesh, timeCount);
    tag_sDMeshSet* dynamicSet;
    if (set->mDynamicMeshSet != nullptr) InstanceDynamicMeshSet(set->mDynamicMeshSet, heap, 0x21df, &dynamicSet);
    else dynamicSet = nullptr;

    uint32_t moved = 0;
    uint32_t size = sizeof(tag_sRideAnimSet) + static_cast<uint32_t>(set->mTimeCount) * 0x38;
    AlignTo32Bit(size, &moved);
    const bool skipAnimations = (meshFlags & 0x800) != 0;
    if (!skipAnimations && set->mAnimationCount != 0) {
        size += set->mAnimationCount * static_cast<uint32_t>(sizeof(tag_sAnimHeader*)) +
                set->mAnimationCount * static_cast<uint32_t>(sizeof(sRideAnimEntry));
        AlignTo32Bit(size, &moved);
    }
    if (heap == nullptr) heap = gDefaultHeap;

    auto* copy = static_cast<tag_sRideAnimSet*>(MReleaseAllocF(size + moved, 0));
    *copy = *set;
    copy->mHeads = mesh;
    copy->mDynamicMeshSet = dynamicSet;
    copy->mInstanceSelf = copy;
    copy->mSelf = set;
    void* cursor = reinterpret_cast<uint8_t*>(copy) + sizeof(tag_sRideAnimSet);
    copy->mTimeControls = cursor;
    if (copy->mTimeControls != nullptr && set->mTimeControls != nullptr && set->mTimeCount != 0) {
        std::memcpy(copy->mTimeControls, set->mTimeControls, static_cast<size_t>(set->mTimeCount) * 0x38);
    }
    cursor = static_cast<uint8_t*>(cursor) + static_cast<size_t>(set->mTimeCount) * 0x38;
    AlignTo32Bit(cursor, nullptr);
    if (!skipAnimations && set->mAnimationCount != 0) {
        AlignTo32Bit(cursor, nullptr);
        copy->mAnimations = static_cast<tag_sAnimHeader**>(cursor);
        cursor = static_cast<uint8_t*>(cursor) + static_cast<size_t>(set->mAnimationCount) * sizeof(tag_sAnimHeader*);
        for (uint32_t i = 0; i < set->mAnimationCount; ++i) {
            if (set->mAnimations[i] != nullptr) InstanceAnim(set->mAnimations[i], heap, meshFlags, &copy->mAnimations[i]);
        }
        for (int32_t kind = 0; kind < 12; ++kind) {
            const sRideAnimGroup& group = set->mGroups[kind];
            if (group.mCount == 0) continue;
            if (cursor != nullptr && group.mEntries != nullptr) {
                std::memcpy(cursor, group.mEntries, static_cast<size_t>(group.mCount) * sizeof(sRideAnimEntry));
            }
            sRideAnimEntry* entries = static_cast<sRideAnimEntry*>(cursor);
            copy->mGroups[kind].mEntries = entries;
            cursor = entries + group.mCount;
            for (uint32_t i = 0; i < group.mCount; ++i) entries[i].mAnimation = copy->mAnimations[entries[i].mIndex];
        }
        AlignTo32Bit(cursor, nullptr);
    }
    if ((meshFlags & 0x2000) != 0) CreateEngineMesh(copy->mHeads);
    *output = copy;
    return 1;
}
