// DynamicMeshSet.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "DynamicMesh.h"
#include "Game.h"
#include "GameSys.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include <cstdio>
#include <cstring>

// Leaves the park's second-stage set-up calls (main app; unit w112a): the coaster track extruder's storage, the object instance
// table's set-up, the texture set loader and freer of the dynamic meshes (TPAdvisor.cpp range of the inventory) and the two
// static texture sets with their entries (initialised data of the image).

// 0x1003013c (8 bytes): a stub that loads nothing and always answers 1.
int32_t DMLoadTextures(sDTextureSet* set) { return 1; }

// 0x10030178 (20 bytes): clears bit 0 of the set's first word. (The original answers 1; no caller reads it.)
void DMFreeTextures(sDTextureSet* set) { set->mFlags &= 0xfffffffeu; }

// ---- The static texture sets: initialised data (entry: file name, flags, +8, +0xc) ----
sDTextureEntry gRideHoardingTextureEntries[4] = {
    {const_cast<char*>("Closed.tga"), 0, 1, 0},
    {const_cast<char*>("Hoarding.tga"), 0, 1, 0},
    {const_cast<char*>("Condemn.tga"), 0, 1, 0},
    {const_cast<char*>("Upgrade.tga"), 0, 1, 0},
};
sDTextureEntry gDynamicCellTextureEntries[20] = {
    {const_cast<char*>("blue.tga"), 7, 0, 0},
    {const_cast<char*>("red.tga"), 7, 0, 0},
    {const_cast<char*>("orange.tga"), 7, 0, 0},
    {const_cast<char*>("m_enter.tga"), 7, 0, 0},
    {const_cast<char*>("m_exit.tga"), 7, 0, 0},
    {const_cast<char*>("m_direct.tga"), 7, 0, 0},
    {const_cast<char*>("m_front.tga"), 7, 0, 0},
    {const_cast<char*>("m_inout.tga"), 7, 0, 0},
    {const_cast<char*>("m_link.tga"), 7, 0, 0},
    {const_cast<char*>("m_break.tga"), 7, 0, 0},
    {const_cast<char*>("m_cross.tga"), 7, 0, 0},
    {const_cast<char*>("m_end.tga"), 7, 0, 0},
    {const_cast<char*>("m_nocash.tga"), 7, 0, 0},
    {const_cast<char*>("m_erase.tga"), 7, 0, 0},
    {const_cast<char*>("red.tga"), 7, 0, 0},
    {const_cast<char*>("gby_sur1.tga"), 3, 0, 0},
    {const_cast<char*>("gby_sur2.tga"), 3, 0, 0},
    {const_cast<char*>("gby_sur3.tga"), 3, 0, 0},
    {const_cast<char*>("gte_lgo1.tga"), 3, 0, 0},
    {const_cast<char*>("gte_lgo1.tga"), 3, 0, 0},
};
// (count, entries, flags, folder)
sDTextureSet gRideHoardingTextures = {4, gRideHoardingTextureEntries, 0, nullptr};
sDTextureSet gDynamicCellTextures = {20, gDynamicCellTextureEntries, 0, "Data:Generic:Dynamic:Textures"};

// The dynamic cell meshes (main application; unit w112b). The Mac files are analyser.cpp for the class (CDynamicMesh: constructor
// 0x100e1610, destructor 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10, DC_CreateSquareFace 0x100e210c), TPAdvisor.cpp
// for DMCreateMesh 0x100301c0 and the two DMEnableMeshRender 0x1003088c and 0x10030928, rsse.cpp for CorrectDTexturePaths 0x100b8710 and
// LLCellSystem.cpp for LLCC_SystemGetMode 0x100914f4.
//
// A mesh is N squares of four vertices and two faces each, drawn cell by cell by DrawAll (untranslated, the next unit). DMCreateMesh lays the
// whole mesh image out in one block of the engine allocator in the mesh file's 32-bit layout (Ptr32 fields; the block comes from the arena);
// the CDynamicMesh itself, its texture set, entry and mesh set are native records that point into the block.

// 0x100301c0 (1,668 bytes). The block holds, in this order after the 0xb8 byte header: the nodes (a root first when there are several
// entries), the texture records, the texture infos (only for a fresh texture set; otherwise the set's existing infos are used), the
// vertex sets, points, UV sets, faces, face sets and last the index buffer. The entries' counts are summed in 16 bits, and each array's
// size is computed from the sums; an empty array leaves its header field zero. The allocator's answer is not tested.
int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    const uint32_t nodeCount = set->mCount == 1 ? 1u : set->mCount + 1u;
    set->mShared = &textures->mUse;
    set->mTextureCount = 0;
    set->mVertexCount = 0;
    set->mFaceSetCount = 0;
    set->mFaceCount = 0;
    set->mUVCount = 0;
    set->mPointCount = 0;
    const sDMeshEntry* entry = set->mEntries;
    for (uint32_t i = 0; i < set->mCount; ++i) {
        set->mVertexCount += entry->mVertexCount;
        set->mFaceCount += entry->mFaceCount;
        set->mFaceSetCount += entry->mFaceSetCount;
        set->mUVCount += entry->mUVCount;
        set->mPointCount += entry->mPointCount;
        if ((entry->mFlags & 4) == 0) set->mPointCount += entry->mVertexCount;
        ++entry;
    }
    set->mTextureCount += textures->mCount;
    const bool fresh = (textures->mFlags & 1) == 0;
    uint32_t textureBytes;
    if (fresh) {
        textures->mUse.mInstances = 1;
        set->mF00 |= 0x8000;
        textureBytes = static_cast<uint32_t>(set->mTextureCount) * 0x18;
    } else {
        textures->mUse.mInstances += 1;
        textureBytes = static_cast<uint32_t>(set->mTextureCount) << 3;
    }
    const uint32_t vertexSets = (static_cast<uint32_t>(set->mVertexCount) + 3) >> 2;
    const uint32_t uvSets = (static_cast<uint32_t>(set->mUVCount) + 3) >> 2;
    const uint32_t size = nodeCount * 0xa0 + vertexSets * 0x30 + static_cast<uint32_t>(set->mPointCount) * 0xc + uvSets * 0x20 +
                          textureBytes + static_cast<uint32_t>(set->mFaceSetCount) * 0x10 + static_cast<uint32_t>(set->mFaceCount) * 8 +
                          static_cast<uint32_t>(set->mVertexCount) * 2 + 0x4b8;
    uint8_t* block = static_cast<uint8_t*>(MReleaseAllocF(size, 1));
    set->mBlock20 = block;
    tag_sM3D2Header* header = reinterpret_cast<tag_sM3D2Header*>(block);
    set->mMesh = CreateMeshFromM3D2Header(header);
    if (name != nullptr) std::strncpy(header->mName, name, 0x14);

    // The arrays, one after the other.
    uint8_t* cursor = block + 0xb8;
    header->mNodes = reinterpret_cast<tag_sNode*>(cursor);
    cursor += nodeCount * 0xa0;
    header->mF78 = header->mNodes;
    if (set->mTextureCount != 0) {
        header->mTextures = reinterpret_cast<sTexture*>(cursor);
        cursor += static_cast<uint32_t>(set->mTextureCount) * 8;
        if (fresh) {
            header->mTextureInfos = reinterpret_cast<sTextureInfo*>(cursor);
            cursor += static_cast<uint32_t>(set->mTextureCount) * 0x10;
        } else {
            header->mTextureInfos = textures->mInfos;
        }
    }
    if (set->mVertexCount != 0) {
        header->mVertexSets = reinterpret_cast<sVertexPosnSet*>(cursor);
        cursor += vertexSets * 0x30;
    }
    if (set->mPointCount != 0) {
        header->mPoints = reinterpret_cast<s3DPoint*>(cursor);
        cursor += static_cast<uint32_t>(set->mPointCount) * 0xc;
    }
    if (set->mUVCount != 0) {
        header->mUVSets = reinterpret_cast<sUVSet*>(cursor);
        cursor += uvSets * 0x20;
    }
    if (set->mFaceCount != 0) {
        header->mFaces = reinterpret_cast<sFace*>(cursor);
        cursor += static_cast<uint32_t>(set->mFaceCount) * 8;
    }
    sFaceSet* faceSets = nullptr;
    if (set->mFaceSetCount != 0) {
        faceSets = reinterpret_cast<sFaceSet*>(cursor);
        cursor += static_cast<uint32_t>(set->mFaceSetCount) * 0x10;
    }
    // The index buffer is the last array, so the cursor is not moved past it here.
    uint16_t* indices = set->mVertexCount != 0 ? reinterpret_cast<uint16_t*>(cursor) : nullptr;

    if ((flags & 2) != 0) header->mFlags |= 0x200000;
    header->mFaceSets = faceSets;
    header->mFaceSetCount = set->mFaceSetCount;
    header->mAllNodeCount = static_cast<uint16_t>(nodeCount);
    header->mNodeCount = header->mAllNodeCount;
    header->mTextureCount = set->mTextureCount;
    header->mVertexSetCount = static_cast<uint16_t>(vertexSets);
    header->mFaceCount = set->mFaceCount;
    header->mUVSetCount = static_cast<uint16_t>(uvSets);
    header->mPointCount = set->mPointCount;
    set->mVertexSets = header->mVertexSets;
    set->mPoints = header->mPoints;
    set->mUVSets = header->mUVSets;
    set->mFaces = header->mFaces;
    textures->mTextures = header->mTextures;
    textures->mInfos = header->mTextureInfos;
    textures->mFlags |= 1;

    // With several entries the first node is a root (identity matrix) whose children are the entries' nodes, linked in a row; each of
    // those links back to the root. A single entry uses node 0 alone.
    tag_sNode* node = header->mNodes;
    if (set->mCount > 1) {
        SetMatrixIdentity(&node->mMatrix);
        node->mF0C = header->mNodes.get() + 1;
        for (uint32_t i = 0; i < set->mCount; ++i) {
            tag_sNode* nodes = header->mNodes;
            if (i < static_cast<uint32_t>(set->mCount) - 1) nodes[i + 1].mF08 = nodes + (i + 2);
            nodes[i + 1].mF04 = node;
        }
        node += 1;
    }
    header->mFlags |= 0x21;
    if ((set->mF00 & 0x1000) != 0) header->mFlags |= 0x200;

    sDMeshEntry* item = set->mEntries;
    for (uint32_t i = 0; i < set->mCount; ++i) {
        node->mFlags |= 0x80800;
        node->mVertexCount = item->mVertexCount;
        node->mF5C = item->mFaceCount;
        node->mF5E = item->mUVCount;
        node->mFaceSetCount = item->mFaceSetCount;
        node->mVertices = header->mVertexSets;
        node->mPoints = header->mPoints;
        node->mUVs = header->mUVSets;
        node->mFaces = header->mFaces;
        if (node->mVertexCount == 0) {
            node->mF94 = nullptr;
        } else {
            node->mF94 = indices;
            if ((flags & 1) != 0) {
                for (uint32_t k = 0; k < node->mVertexCount; ++k) indices[k] = static_cast<uint16_t>(k);
            }
            indices += node->mVertexCount;
        }
        node->mFaceSets = faceSets;
        if (faceSets != nullptr) faceSets += item->mFaceSetCount;
        std::memcpy(&node->mBox, item->mBox, sizeof item->mBox);
        item->mMatrix = &node->mMatrix;
        item->mFaceSets = node->mFaceSets;
        ++node;
        ++item;
    }

    // The texture infos (only a fresh set makes them) and the texture records.
    for (uint32_t i = 0; i < textures->mCount; ++i) {
        if (fresh) {
            sTextureInfo* info = header->mTextureInfos.get() + i;
            info->mFrames = 1;
            info->mName = static_cast<char*>(MReleaseAllocF(0x14, 1));
            std::strncpy(info->mName.get(), textures->mEntries[i].mName, 0x14);
        }
        sTexture* texture = header->mTextures.get() + i;
        texture->mF00 |= 1;
        if ((textures->mEntries[i].mFlags & 1) != 0) texture->mF00 |= 2;
        if ((textures->mEntries[i].mFlags & 2) != 0) texture->mF00 |= 0x30;
    }

    set->mF00 |= 0x2000;
    if ((flags & 1) != 0) set->mF00 |= 8;
    CorrectDTexturePaths(textures, set);
    CreateEngineMesh(set->mMesh);
    return 1;
}

// ---- TPAdvisor.cpp

// 0x1003088c (100 bytes): the mesh goes into the draw lists, over the set's matrix when it has one. Answers 1.
int32_t DMEnableMeshRender(tag_sDMeshSet* set) {
    if (set->mMatrix != nullptr) {
        EnableMeshDraw(set->mMesh, set->mMatrix, 0);
    } else {
        EnableMeshDraw(set->mMesh);
    }
    set->mF00 |= 0x4000;
    return 1;
}

// 0x10030928 (84 bytes): the same over a range of the map. Answers 1.
int32_t DMEnableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to) {
    EnableMeshDraw(set->mMesh, from, to);
    set->mF00 |= 0x4000;
    return 1;
}

void CorrectDTexturePaths(sDTextureSet* textures, tag_sDMeshSet* set);

// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)

// 0x100309c8. Answers 1.
int32_t DMDisableMeshRender(tag_sDMeshSet* set) {
    DisableMeshDraw(set->mMesh);
    set->mF00 &= ~0x4000u;
    return 1;
}

// 0x10030a48. Answers 1.
int32_t DMDisableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to) {
    DisableMeshDraw(set->mMesh, from, to);
    set->mF00 &= ~0x4000u;
    return 1;
}

// 0x10030ae8. The texture info names are freed only when this was the last instance of the shared record (the count is unsigned, so an
// already empty record wraps and keeps them). Answers 1.
int32_t DMFreeMesh(tag_sDMeshSet* set) {
    if ((set->mF00 & 0x4000) != 0) DisableMeshDraw(set->mMesh);
    DestroyEngineMesh(set->mMesh);
    --set->mShared->mInstances;
    if (set->mShared->mInstances == 0) {
        for (uint32_t i = 0; i < set->mMesh->mHeader->mTextureCount; ++i) {
            sTextureInfo* info = set->mMesh->mHeader->mTextureInfos.get() + i;
            MReleaseFreeF(info->mName.get());
            info->mName = nullptr;
        }
    }
    MReleaseFreeF(set->mBlock20);
    set->mBlock20 = nullptr;
    set->mF00 &= ~0x2000u;
    return 1;
}
