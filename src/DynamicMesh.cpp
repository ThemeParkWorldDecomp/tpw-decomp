// The dynamic cell meshes (main application; unit w112b). The Mac files are analyser.cpp for the class (CDynamicMesh: constructor
// 0x100e1610, destructor 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10, DC_CreateSquareFace 0x100e210c), TPAdvisor.cpp
// for DMCreateMesh 0x100301c0 and the two DMEnableMeshRender 0x1003088c and 0x10030928, rsse.cpp for CorrectDTexturePaths 0x100b8710 and
// LLCellSystem.cpp for LLCC_SystemGetMode 0x100914f4.
//
// A mesh is N squares of four vertices and two faces each, drawn cell by cell by DrawAll (untranslated, the next unit). DMCreateMesh lays the
// whole mesh image out in one block of the engine allocator in the mesh file's 32-bit layout (Ptr32 fields; the block comes from the arena);
// the CDynamicMesh itself, its texture set, entry and mesh set are native records that point into the block.
#include "DynamicMesh.h"

#include <cstdio>
#include <cstring>

#include "Game.h"
#include "GameSys.h"
#include "LLCellSystem.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"


// The records the other units hold by value or allocate (src/RideInstance.cpp, include/ui_DrawObject.h) keep their host sizes.
static_assert(sizeof(tag_sDMeshSet) == 112 && sizeof(sDMeshEntry) == 56, "the host sizes of the mesh set and its entry");

// ---- globals (all zero at start, like the Mac's bss)
float gDynamicCellUV[10][3];
float gDynamicCellF9BC0;
float gDynamicCellF9BC4;
char gDynamicCellTexturePath[0x400];
int32_t gLLCCDB50;
int32_t gLLCCDB54;

namespace {

// The strings of the Mac's string area at 0x1023ff4d the constructor uses.
const char kTexturesFolder[] = "Data:Generic:Dynamic:Textures";   // 0x1024001f
const char kSmallTexturesFolder[] = "Data:Generic:Dynamic:STexture";   // 0x102400a3
const char kLevelsFolder[] = "Data:Levels:";                      // 0x102400c1
const char kSmallLevelFolder[] = ":Dynamic:STexture";             // 0x102400ce
const char kLevelFolder[] = ":Dynamic:Textures";                  // 0x102400e0

}  // namespace

// ---- LLCellSystem.cpp

// 0x100914f4 (12 bytes): the cell system's mode.
int32_t LLCC_SystemGetMode() { return gLLCC480C; }

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

// ---- rsse.cpp

// 0x100b8710 (324 bytes). Two tries: the texture folder and the first texture's name as they are, then (after FindFilePath finds nothing)
// with the name's last extension replaced by ".wct". FindFilePath answers the search path the file is in, not a file name; with none found
// the last name tried is the root. The Mac's sprintf has no bound; this one stops at the buffer's size.
void CorrectDTexturePaths(sDTextureSet* textures, tag_sDMeshSet* set) {
    char buffer[1032];
    char* root = nullptr;
    for (uint32_t tries = 2; tries != 0; --tries) {
        const char* folder = textures->mFolder;
        const char* name = textures->mEntries[0].mName;
        const size_t length = std::strlen(folder);
        if (folder[static_cast<ptrdiff_t>(length) - 1] == ':') {
            std::snprintf(buffer, sizeof buffer, "%s%s", folder, name);
        } else {
            std::snprintf(buffer, sizeof buffer, "%s:%s", folder, name);
        }
        if (tries == 1) {
            char* dot = std::strrchr(buffer, '.');
            if (dot == nullptr) {
                std::strcat(buffer, ".wct");
            } else {
                std::strcpy(dot, ".wct");
            }
        }
        root = FindFilePath(buffer, 0, nullptr);
        if (root != nullptr) break;
    }
    if (root == nullptr) root = buffer;
    SetNewMeshPaths(root, nullptr, const_cast<char*>(textures->mFolder), const_cast<char*>(textures->mSharedFolder));
    OverrideTexturePaths(set->mMesh->mHeader, 1, 0);
}

// ---- analyser.cpp: CDynamicMesh

// 0x100e1610 (716 bytes). Bytes the constructor leaves as the heap had them: the entry's +2 and +0xe and the set's +6.
CDynamicMesh::CDynamicMesh(uint16_t sizeA, uint16_t sizeB, uint32_t* ok, const char* name, DYN_Type type, float minX, float minY,
                           float minZ, float maxX, float maxY, float maxZ) {
    *ok = 1;
    mType = type;
    mTextures.mFlags = 0;
    mTextures.mCount = sizeB;
    mTextures.mFolder = UseSmallTextures() ? kSmallTexturesFolder : kTexturesFolder;
    std::strcpy(gDynamicCellTexturePath, kLevelsFolder);
    std::strcat(gDynamicCellTexturePath, gGame.GetCurrentThemeName().mData);
    std::strcat(gDynamicCellTexturePath, UseSmallTextures() ? kSmallLevelFolder : kLevelFolder);
    mTextures.mSharedFolder = gDynamicCellTexturePath;
    mTextures.mEntries = gDynamicCellTextureEntries;

    mMeshSet.mF00 = 3;
    mMeshSet.mCount = 1;
    mMeshSet.mEntries = &mItem;
    mMeshSet.mMatrix = nullptr;
    mMeshSet.mVertexSets = nullptr;
    mMeshSet.mPoints = nullptr;
    mMeshSet.mUVSets = nullptr;
    mMeshSet.mFaces = nullptr;
    mMeshSet.mBlock20 = nullptr;
    mMeshSet.mMesh = nullptr;
    mMeshSet.mTextureCount = 0;
    mMeshSet.mVertexCount = 0;
    mMeshSet.mFaceSetCount = 0;
    mMeshSet.mFaceCount = 0;
    mMeshSet.mUVCount = 0;
    mMeshSet.mPointCount = 0;
    mMeshSet.mAllocation = nullptr;
    mMeshSet.mF38 = 0;
    mMeshSet.mShared = nullptr;

    mItem.mFlags = 3;
    mItem.mFaceCount = static_cast<uint16_t>(sizeA << 1);
    mItem.mVertexCount = static_cast<uint16_t>(sizeA << 2);
    mItem.mUVCount = mItem.mVertexCount;
    mItem.mPointCount = 0;
    mItem.mFaceSetCount = sizeB;
    mMaxSquares = sizeA;
    SetBoundingBox(minX, minY, minZ, maxX, maxY, maxZ);

    *ok = static_cast<uint32_t>(DMCreateMesh(&mMeshSet, &mTextures, name, 3));
    if (*ok == 1) {
        SetMatrixIdentity(mItem.mMatrix);
        mVertexCount = 0;
        mFaceCount = 0;
        mFaceCursor = mMeshSet.mFaces;
        for (int32_t i = 0; i < static_cast<int32_t>(sizeA); ++i) DC_CreateSquareFace(0.0f, 0.0f, 0.0f, 0, 0xff, -1, 1.0f, 0, 0, 0);
        for (int32_t i = 0; i < static_cast<int32_t>(sizeB); ++i) {
            sFaceSet* faceSet = mItem.mFaceSets + i;
            faceSet->mTexture = mTextures.mTextures + i;
            faceSet->mFirstFace = 0;
            faceSet->mFaceCount = 0;
        }
        DMEnableMeshRender(&mMeshSet);
    }
}

// 0x100e191c (104 bytes). The Mac's NULL test and its operator delete (for a positive flag) are C++'s `delete`.
CDynamicMesh::~CDynamicMesh() {
    DMDisableMeshRender(&mMeshSet);
    DMFreeMesh(&mMeshSet);
}

// 0x100e19b0 (28 bytes).
void CDynamicMesh::SetBoundingBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    mItem.mBox[0] = minX;
    mItem.mBox[1] = minY;
    mItem.mBox[2] = minZ;
    mItem.mBox[3] = maxX;
    mItem.mBox[4] = maxY;
    mItem.mBox[5] = maxZ;
}

// 0x100e1f10 (444 bytes). With followTerrain the vertex's height is the terrain's plus a wave: the sine table at (z + (gDynamicCellF9DA0 + x))
// times the table's scale (the listing adds the phase to x first), halved after adding 1 for the point's y, and added to the height as it
// is. Without it the point is the one given (or straight up) and the height is y unchanged. The positions are stored four to a group (x, y and
// z arrays of four), the texture coordinates likewise.
void CDynamicMesh::DC_SetVertex(float x, float y, float z, float u, float v, int32_t followTerrain, s3DPoint* point) {
    s3DPoint* entry = mMeshSet.mPoints + mVertexCount;
    if (followTerrain != 0) {
        const float height = DetermineHFieldHeightQuick(x, z);
        y = y + height;
        const float phase = gDynamicCellF9DA0 + x;
        const float place = z + phase;
        const float scaled = gTPGlobals.mSinTableScale * place;
        const uint32_t index = gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)));
        const float wave = gTPGlobals.mSinTable[index];
        entry->x = 0.0f;
        entry->y = (1.0f + wave) * 0.5f;
        entry->z = 0.0f;
        y = y + wave;
    } else if (point == nullptr) {
        entry->x = 0.0f;
        entry->y = 1.0f;
        entry->z = 0.0f;
    } else {
        entry->x = point->x;
        entry->y = point->y;
        entry->z = point->z;
    }
    float* positions = reinterpret_cast<float*>(mMeshSet.mVertexSets + (mVertexCount >> 2)) + (mVertexCount & 3);
    positions[0] = x;
    positions[4] = y;
    positions[8] = z;
    float* uvs = reinterpret_cast<float*>(mMeshSet.mUVSets + (mVertexCount >> 2)) + (mVertexCount & 3);
    uvs[0] = u;
    uvs[4] = v;
    mVertexCount = mVertexCount + 1;
}

// 0x100e210c (1,576 bytes). A face's corner order (which table entry each vertex takes) comes from `rotation`, the geometry from `side`; the
// map's scroll lag is taken off the position for textures below 13 while the cell system is in mode 4, 0xe or 0x13. Both faces use the vertex
// counter as it was at entry.
void CDynamicMesh::DC_CreateSquareFace(float x, float y, float z, uint8_t side, uint8_t rotation, int32_t texture, float, int32_t large,
                                       uint8_t, int32_t keepPosition) {
    const uint32_t first = mVertexCount;
    const float size = large != 0 ? 20.0f : 10.0f;
    if (keepPosition == 0 && gDynamicCellF9BD4 == 0 && texture < 0xd &&
        (LLCC_SystemGetMode() == 4 || LLCC_SystemGetMode() == 0xe || LLCC_SystemGetMode() == 0x13)) {
        const float originX = static_cast<float>(gLLCCDB54);
        const float originZ = static_cast<float>(gLLCCDB50);
        x = ppc_fnmsubs(10.0f, originX, x);
        z = ppc_fnmsubs(10.0f, originZ, z);
        x = x + gDynamicCellF9BC4;
        z = z + gDynamicCellF9BC0;
    }
    const int32_t followTerrain = (texture == -1 || texture >= 0xf) ? 0 : 1;
    int32_t base = 0;
    if (texture < 0xf && texture >= 8) base = 5;
    int32_t a, b, c, d;
    if (rotation == 0x10) {
        a = base + 3;
        b = base + 2;
        c = base + 1;
        d = base;
    } else if (rotation == 4) {
        a = base + 2;
        b = base;
        c = base + 3;
        d = base + 1;
    } else if (rotation == 0x40) {
        a = base + 1;
        b = base + 3;
        c = base;
        d = base + 2;
    } else {
        a = base;
        b = base + 1;
        c = base + 2;
        d = base + 3;
    }
    const float(*uv)[3] = gDynamicCellUV;
    if (side == 4) {
        const float right = x + size;
        DC_SetVertex(right, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(right, y, z + size, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(right, top, z, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(right, top, z + size, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 1) {
        DC_SetVertex(x, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x, top, z, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x + size, top, z, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0) {
        DC_SetVertex(x, y, z, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        DC_SetVertex(x, y, z + size, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x + size, y, z + size, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0x40) {
        DC_SetVertex(x, y, z + size, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x, y, z, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x, top, z + size, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x, top, z, uv[d][0], uv[d][1], followTerrain, nullptr);
    } else if (side == 0x10) {
        const float back = z + size;
        DC_SetVertex(x + size, y, back, uv[a][0], uv[a][1], followTerrain, nullptr);
        DC_SetVertex(x, y, back, uv[b][0], uv[b][1], followTerrain, nullptr);
        const float top = 10.0f + y;
        DC_SetVertex(x + size, top, back, uv[c][0], uv[c][1], followTerrain, nullptr);
        DC_SetVertex(x, top, back, uv[d][0], uv[d][1], followTerrain, nullptr);
    }
    // Both faces, whatever the side (an unknown side made no vertices, so its faces name the next square's).
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first + 1);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first + 3);
    mFaceCursor += 1;
    mFaceCount += 1;
    mFaceCursor->mF02[0] = static_cast<uint16_t>(first);
    mFaceCursor->mF02[1] = static_cast<uint16_t>(first + 2);
    mFaceCursor->mF02[2] = static_cast<uint16_t>(first + 3);
    mFaceCursor += 1;
    mFaceCount += 1;
}
