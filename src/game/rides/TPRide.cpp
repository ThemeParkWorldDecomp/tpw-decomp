// TPRide.cpp: name proven by __sinit_TPRide_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CBalanceTable.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "helpers/TPInstanceHelpers.h"
#include "helpers/TPQuickloadHelpers.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainWindowCallback.h"
#include "MapWho.h"
#include "OptionsBalance.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/ltms.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Quickload.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "tb_ddraw.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "TbDynamicStringTemplate.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "tpw_probe.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "Wavelet.h"
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// The ride animation set's set-up, the main app's rsse.cpp range: RAInitType (0x100c3430),
// CorrectNodeFlags1 (0x10067cf4), ResetAnimTimeStats (0x100c5a90), RAInitNodeAnimNodes
// (0x100c32ac) and InitRideAnimSet (0x100c7768). LoadRideAnimSet (src/RideAnimLoad.cpp)
// calls them. The mesh files' records keep their 32-bit layout (include/m3d2.h); the set
// and its time controls use native pointers (include/TPEngine.h).

// 0x10067cf4, 236 bytes. Corrects the node flags of every animation in the set. A node
// animation with flag 0x1000 marks the header (0x2000). One with 0x10000 flags each node it
// names (0x80), and the texture of each of the node's face sets (0x8030 in the texture's
// first word). A node index past the full nodes names a mini node. For such a node the
// original reads the face count and the face set pointer from the record after it, which
// lie past the mini node's first 0x58 bytes. This reads the same bytes in host order, so the
// two agree only where those bytes are zero.
void CorrectNodeFlags1(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t a = 0; a < set->mAnimationCount; ++a) {
        tag_sAnimHeader* animation = set->mAnimations[a];
        tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
        for (uint32_t n = animation->mNodeAnimCount; n != 0; --n, ++nodeAnim) {
            const uint32_t index = nodeAnim->mF14;
            tag_sNode* node;
            if (index < header->mNodeCount) {
                node = header->mNodes.get() + index;
            } else {
                node = reinterpret_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
            }
            if ((nodeAnim->mFlags & 0x1000) != 0) header->mFlags |= 0x2000;
            if ((nodeAnim->mFlags & 0x10000) != 0) {
                sFaceSet* faceSets = node->mFaceSets.get();
                for (uint32_t f = 0; f < node->mFaceSetCount; ++f) {
                    node->mFlags |= 0x80;
                    sTexture* texture = faceSets[f].mTexture.get();
                    if (texture != nullptr) texture->mF00 |= 0x8030;
                }
            }
        }
    }
}

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

uint32_t gPolySearchCount;    // 0x102c1e18

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

// The quickload indexes (decompiled from the main app of SimTheme Park 2.04b3:
// 0x1006736c..0x10068248). The inventory files this range under TPRide.cpp; the original
// file name is unknown (see Quickload.h).
//
// A qickload.txt (sic) lists the files of its folder, separated by white space. The game
// loads the list of the current mesh folder and of its theme texture folder for each search
// path, and DoesFileExist / FindFilePath (TPConfig.cpp) consult them before the disk.

extern sQuickloadIndex* gQuickloadTextureIndexes;

extern sQuickloadIndex* gQuickloadIndexes;

extern int32_t gQuickloadInitialised;

extern int32_t gQuickloadEnabled;

extern int32_t gQuickloadPathCount;

void LoadQuickloadIndex(sQuickloadIndex* index, void* data, uint32_t size);

void LoadQuickloadIndexes(const char* folder, int32_t path);

void FreeQuickloadIndexes();

int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    char path[0x400];
    char textureFolder[0x400];
    int32_t index;
    FreeQuickloadIndexes();
    // The original passes `file` alone as sprintf's format; file names have no '%'.
    if (folder != nullptr) std::snprintf(path, sizeof(path), "%s:%s", folder, file);
    else std::snprintf(path, sizeof(path), "%s", file);
    char* root = FindFilePath(path, 0, &index);
    if (index != -1) {
        LoadQuickloadIndexes(folder, index);
        SetFileAccessIndex(index);
        std::strcpy(textureFolder, folder);
        const std::size_t length = std::strlen(textureFolder);
        char* end = textureFolder + length;
        if (end[-1] == ':') std::snprintf(end, sizeof(textureFolder) - length, "%s", gThemeTextureFolder);
        else std::snprintf(end, sizeof(textureFolder) - length, ":%s", gThemeTextureFolder);
        if (root != nullptr) {
            SetNewMeshPaths(root, folder, textureFolder, sharedTextureFolder);
            return 1;
        }
    }
    CTRACE(5, "Could not find ride '%s' - check registry paths and quickload indexes\n", path);
    return 0;
}

// SetRideMobile 0x10068280: marks ride `ride` as mobile: the mesh header gets flag 0x20, the header's node
// (mF78) gets bits 0x80800 in its first word, and the anim set gets flag 0x1000000. No NULL checks, like the original.

void SetRideMobile(uint32_t& ride) {
    tag_sRideAnimSet* set = gRideAnimSets[ride];
    tag_sM3D2Header* header = set->mHeads->mHeader;
    header->mFlags |= 0x20;
    uint32_t* word = reinterpret_cast<uint32_t*>(set->mHeads->mHeader->mF78.get());
    *word |= 0x80800;
    set->mFlags |= 0x1000000;
}

// The engine start-up glue of the main app (0x10059c70..0x10068d90 in part; the inventory
// files the range under TPRide.cpp, and the original file name is unknown): the options
// table and the options dialog turned into engine flags, the window, the OpenGL context and
// the engine's start, the engine's Mac* hooks, the per-frame engine calls, the memory hooks,
// the procedural texturing switches and the scratch geometry buffer.
//
// The rest of the range (heightfield, ride height maps, hoardings, instancing, meshes,
// quickload) is not translated yet.

uint32_t gScratchGeomVertices;
void* gScratchGeomBuffer;

void* MReleaseAllocF(uint32_t size, uint32_t flags);

void* MReleaseFreeF(void* block);

// 0x100682fc: keeps the largest counts any loaded mesh needs.
void UpdateScratchGeomBufferSize(tag_sMesh* mesh) {
    const tag_sNode* node = mesh->mHeader->mNodes;
    for (uint16_t n = mesh->mHeader->mNodeCount; n != 0; --n, ++node) {
        if (node->mF5C > gScratchGeomVertices) gScratchGeomVertices = node->mF5C;
        if (node->mVertexCount > gScratchGeomVertices) gScratchGeomVertices = node->mVertexCount;
        if (node->mF5E > gScratchGeomVertices) gScratchGeomVertices = node->mF5E;
    }
    if (mesh->mHeader->mAllNodeCount > gScratchGeomVertices) gScratchGeomVertices = mesh->mHeader->mAllNodeCount;
    if (static_cast<uint32_t>(mesh->mHeader->mUVSetCount) << 2 > gMaxMeshFaces4)
        gMaxMeshFaces4 = static_cast<uint32_t>(mesh->mHeader->mUVSetCount) << 2;
    if (mesh->mHeader->mFaceCount > gMaxMeshF3E) gMaxMeshF3E = mesh->mHeader->mFaceCount;
    if (mesh->mHeader->mNodeCount > gMaxMeshNodes) gMaxMeshNodes = mesh->mHeader->mNodeCount;
}

// The original two-pass ride-animation loader. Native records grow with their pointers;
// mesh and animation headers themselves retain their on-disk 32-bit layout.

int32_t LoadRideAnimSet(char* name, uint32_t flags, tag_sRideAnimSet** output, char* loadedName, uint32_t timeCount) {
    tag_sRideAnimSet local{};
    char filename[1040], stem[1024];
    char* path = GetMeshPath();
    if (path == nullptr || (flags & 0x200) != 0) std::sprintf(filename, "%s", name);
    else std::sprintf(filename, "%s%s", path, name);
    char* dot = std::strrchr(filename, '.');
    if (dot == nullptr || dot == filename || dot[1] == ':') std::strcat(filename, ".md2");
    std::strcpy(stem, name);
    dot = std::strrchr(stem, '.');
    // The original's comparison is against the other buffer, so a leading dot here is
    // also removed. Keep that behaviour rather than correcting its filename handling.
    if (dot != nullptr && dot[1] != ':') *dot = '\0';
    local.mHeads = LoadMesh(filename, nullptr, 1, gTPGlobals.mEngineFlags & 8, 0);
    if (local.mHeads == nullptr) return 0;
    UpdateScratchGeomBufferSize(local.mHeads);
    tag_sM3D2Header* header = local.mHeads->mHeader;
    if ((flags & 0x20000) == 0 && (header->mFlags & 4) != 0) {
        ERRORBox("Ride %s has relative animation incorrectly set\nStripping flag, animation will not work!\n", header->mName);
        header->mFlags = (header->mFlags & ~4u) | 0x1000000;
    }
    if (loadedName != nullptr) std::strcpy(loadedName, filename);
    if ((flags & 8) != 0) {
        for (uint32_t i = 0; i < header->mNodeCount; ++i) {
            tag_sNode& node = header->mNodes.get()[i];
            if (NS_MacDoze::_stricmp(node.mName.get(), "Sky") == 0) node.mFlags |= 0x4000000;
        }
    }
    if ((flags & 0x100) != 0) local.mFlags |= 8;
    if ((flags & 0x10) != 0) header->mFlags |= 0x800;
    if ((flags & 0x80) != 0) header->mFlags |= 0x100;
    if ((flags & 0x800) != 0) header->mFlags |= 0x20000;
    if ((flags & 0x1000) != 0) local.mFlags |= 0x100000;
    if ((flags & 0x2000) != 0) local.mFlags |= 0x2000000;
    if ((flags & 0x4000) != 0) header->mFlags |= 0x200000;
    if ((flags & 0x8000) != 0) local.mFlags |= 0x10000000;
    if ((flags & 0x800000) != 0) local.mFlags |= 0x20000000;

    constexpr char kinds[13] = "CDILSMEUWBRO";
    sRideAnimEntry* nextEntry = nullptr;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass != 0) {
            local.mAnimations = static_cast<tag_sAnimHeader**>(MReleaseAllocF(local.mAnimationCount * sizeof(tag_sAnimHeader*), 1));
            local.mAnimationHeaders = static_cast<tag_sM3D2Header**>(MReleaseAllocF(local.mAnimationCount * sizeof(tag_sM3D2Header*), 1));
            nextEntry = static_cast<sRideAnimEntry*>(MReleaseAllocF(local.mAnimationCount * sizeof(sRideAnimEntry), 1));
            local.mEntryAllocation = nextEntry;
            local.mAnimationCount = 0;
        }
        for (int32_t kind = 0; kind < 12; ++kind) {
            sRideAnimGroup& group = local.mGroups[kind];
            const uint32_t firstPassCount = group.mCount;
            group.mCount = 0;
            bool numbered = pass == 0 || (group.mFlags & 2) != 0;
            while (true) {
                if (numbered) std::sprintf(filename, "%s%s%c%u.md2", path, stem, kinds[kind], group.mCount + 1);
                else std::sprintf(filename, "%s%s%c.md2", path, stem, kinds[kind]);
                tag_sM3D2Header* animationHeader = nullptr;
                bool found;
                if (pass != 0) {
                    found = false;
                    if ((group.mFlags & 3) != 0) {
                        animationHeader = LoadM3D2Header(filename, 0);
                        if (animationHeader == nullptr || animationHeader->mAnim == nullptr) {
                            if (animationHeader != nullptr) DestroyMeshHeader(animationHeader);
                        } else found = true;
                    }
                } else found = DoesFileExist(filename, gQuickloadEnabled == 0 ? 0 : 2) != 0;
                if (pass != 0 && group.mCount == 0) group.mFlags &= ~3u;
                if (!found) {
                    if (numbered && group.mCount == 0) { numbered = false; continue; }
                    break;
                }
                if (pass != 0) {
                    if (group.mEntries == nullptr) group.mEntries = nextEntry;
                    ++nextEntry;
                    local.mAnimations[local.mAnimationCount] = animationHeader->mAnim;
                    local.mAnimationHeaders[local.mAnimationCount] = animationHeader;
                    group.mEntries[group.mCount] = {local.mAnimationCount, animationHeader->mAnim};
                }
                group.mFlags |= numbered ? 2 : 1;
                ++group.mCount;
                ++local.mAnimationCount;
                if (group.mCount == firstPassCount || !numbered) break;
            }
        }
    }
    *output = static_cast<tag_sRideAnimSet*>(MReleaseAllocF(sizeof(tag_sRideAnimSet) + timeCount * 0x38, 1));
    local.mTimeControls = reinterpret_cast<uint8_t*>(*output) + sizeof(tag_sRideAnimSet);
    local.mTimeCount = static_cast<uint16_t>(timeCount);
    for (uint32_t i = 0; i < local.mAnimationCount; ++i) RAInitType(local.mHeads, local.mAnimations[i]);
    if (local.mHeads != nullptr && local.mHeads->mHeader != nullptr) {
        if ((flags & 0x20) != 0) {
            header->mFlags |= 0x20;
            header->mF78->mFlags |= 0x80800;
            local.mFlags |= 0x1000000;
            OverrideActiveChannels(&local);
        }
        if ((flags & 0x40) != 0) {
            header->mFlags |= 0x80;
            for (uint32_t i = 0; i < header->mNodeCount; ++i) header->mNodes.get()[i].mFlags |= 0x20000;
        }
    }
    if ((flags & 0x400) != 0) StripRideWrapping(&local, nullptr, 0.02f, nullptr, nullptr);
    CorrectNodeFlags1(&local);
    if ((flags & 8) == 0) CreateMeshTypeEngineMesh(local.mHeads);
    if ((flags & 10) != 0) {
        InitRideAnimSet(&local, 1, flags & 0x40000, 0);
        if ((flags & 8) == 0) CreateEngineMesh(local.mHeads);
        if ((flags & 6) == 6) local.mFlags |= 4;
        if ((flags & 8) != 0 && header->mHField != nullptr) {
            header->mFlags |= 0x400;
            header->mFA8[3] = 0;
            header->mFA8[2] = static_cast<uint8_t>(header->mHField->mF18[0] - 1);
            header->mFA8[1] = 0;
            header->mFA8[0] = static_cast<uint8_t>(header->mHField->mF18[1] - 1);
        }
    }
    TPInitialiseEmbeddedData(local.mHeads);
    if (*output != nullptr) **output = local;
    if ((flags & 10) != 0) (*output)->mSelf = *output;
    return 1;
}

// 0x10068d14
void CreateScratchGeomBuffer(int32_t large) {
    const uint32_t vertices = large != 0 ? 0x400 : gScratchGeomVertices;
    gScratchGeomBuffer = MReleaseAllocF(vertices * 12, 1);
}

// 0x10068d90: the pointer is left as it was.
void DestroyScratchGeomBuffer() { MReleaseFreeF(gScratchGeomBuffer); }

// Ride mesh helpers of the TPRide file: face normals of the root node, the blended base textures,
// the node's world vertex positions (0x100694c8, 0x10069770, 0x10068dec).

// 0x10237cd8: a glue import taking a float in f1 and answering an int in r3 (the float to int step).
int32_t FloatToIntGlue(float value);

namespace {

uint16_t Half(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

}  // namespace

// Puts the node's vertex positions, moved by the node's matrix combined with those of its
// descendants (the chain of mF04 links), into the scratch geometry buffer: x, y, z per vertex.
void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node) {
    sMatrix m;
    std::memcpy(&m, &node->mMatrix, sizeof m);
    for (tag_sNode* child = node->mF04.get(); child != nullptr; child = child->mF04.get())
        CombineMatricesNoWDest1st(&m, &child->mMatrix);
    float* out = static_cast<float*>(gScratchGeomBuffer);
    const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
    for (uint32_t i = 0; i < node->mVertexCount; ++i) {
        const float* p = reinterpret_cast<const float*>(verts + (i >> 2) * 0x30 + (i & 3) * 4);
        float x = p[0], y = p[4], z = p[8];
        out[0] = x;
        out[1] = y;
        out[2] = z;
        float ax = ppc_fmadds(x, m.m[0][0], y * m.m[1][0]);
        ax = ppc_fmadds(z, m.m[2][0], ax);
        float ay = ppc_fmadds(x, m.m[0][1], y * m.m[1][1]);
        ay = ppc_fmadds(z, m.m[2][1], ay);
        float az = ppc_fmadds(x, m.m[0][2], y * m.m[1][2]);
        az = ppc_fmadds(z, m.m[2][2], az);
        out[0] = m.m[3][0] + ax;
        out[1] = m.m[3][1] + ay;
        out[2] = m.m[3][2] + az;
        out += 3;
    }
}

// Ride types and ride instances (main app of SimTheme Park 2.04b3; the inventory's TPRide.cpp
// range): LoadRideType 0x10068fd0 (1,216 bytes), InstanceRide 0x10069b24 (1,496),
// DestroyRideInstance 0x1006a3ec (464) and DestroyAllRideTypes 0x1006a9f0 (84).
//
// A ride type is a ride animation set loaded from a mesh file (LoadRideAnimSet), numbered 1 up
// in gRideTypes. An instance is a copy of a type's set (InstanceRideAnimSet), numbered 1 up in
// gRideAnimSets. The lobby makes every mesh of the island one of each. Everything the four
// functions call besides the library's string functions belongs to other files.

tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
uint32_t gRideFirstFree;
uint32_t gRideCountA;
uint32_t gRideCountB;
tag_sRideAnimSet* gRideTypes[kMaxRideTypes];
uint32_t gRideTypeCount;

namespace {

// The flags LoadRideType hands to LoadRideAnimSet for the flags it is given.
struct sFlagMap {
    uint32_t mRequest;
    uint32_t mLoad;
};
constexpr sFlagMap kFlagMap[] = {
    {0x10, 0x2},        {0x20, 0x10},      {0x40, 0x20},       {0x2, 0x40},     {0x100, 0x1},
    {0x200, 0x100},     {0x4, 0x80},       {0x4000, 0x400},    {0x10000, 0x800}, {0x40000, 0x1000},
    {0x100000, 0x2000}, {0x200000, 0x4000}, {0x400000, 0x8000}, {0x1000000, 0x20000},
};

// True when the text has no extension of its own: no '.' at all, or a '.' that a ':' follows
// (a folder called ".", as in "disc:.:name").
bool NeedsExtension(const char* dot) { return dot == nullptr || dot[1] == ':'; }

}  // namespace

// Loads the ride mesh `name` of `folder` and records it as the next ride type. Returns the type's
// number, or 0 when the folder is not set up or the mesh does not load. The size (width and depth
// in cells) comes from `a` (two ints) when `d` or `e` is 0.
// The original stores the loader's flag 1 into the set LoadRideAnimSet made even when that call
// failed, through a pointer it never set (stack garbage); here a failed load that left no set is
// not stored to.
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c,
                      const char16_t* sign1, const char16_t* sign2, int32_t d, int32_t e) {
    tpw_probe::Hit(tpw_probe::kLoadRideType);
    if (d == 0) d = a != nullptr ? static_cast<int32_t*>(a)[0] : 0;
    if (e == 0) e = a != nullptr ? static_cast<int32_t*>(a)[1] : 0;
    uint32_t loadFlags = 0;
    for (const sFlagMap& map : kFlagMap) {
        if ((flags & map.mRequest) != 0) loadFlags |= map.mLoad;
    }
    if (c == 0) c = 1;
    char file[1024];
    std::strcpy(file, name);
    if (NeedsExtension(std::strrchr(file, '.'))) std::strcat(file, ".md2");
    char shared[1024];
    BuildSharedTexturePath(shared);
    int32_t loaded = SetupDirectoryForMeshLoad(file, folder, shared);
    tag_sRideAnimSet* set = nullptr;
    char loadedName[1026];
    if (loaded == 1) {
        loaded = LoadRideAnimSet(name, loadFlags, &set, loadedName, c);
        if (set != nullptr) set->mFlags |= 1;
    }
    if (loaded == 1 && (flags & 0x20000) != 0) {
        // The "p" variant of the mesh, kept in the set (+0xd0).
        char variant[1024];
        std::snprintf(variant, sizeof variant, "p%s", name);
        LoadRideAnimSet(variant, 0, &set->mFD0, nullptr, 1);
    }
    if (loaded != 1) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed, 1000); return 0; }
    set->mFF8 = id;
    set->mFF0 = d;
    set->mFF4 = e;
    if ((flags & 1) != 0) CalculateSkirtVertices(set, gHeightfieldHeader, a, d, e);
    if ((flags & 0x2000) != 0) {
        tag_sMesh* mesh = set->mHeads;
        if (mesh->mRootNode != nullptr) SetBaseBlendFlags(mesh, mesh->mRootNode, 1);
        else SetBaseBlendFlags(mesh, mesh->mHeader->mF78.get(), 1);
        if (set->mFD0 != nullptr) {
            mesh = set->mFD0->mHeads;
            SetBaseBlendFlags(mesh, mesh->mHeader->mF78.get(), 1);
        }
    }
    CorrectFaceFlags1(set->mHeads, flags & 0x400000);
    if ((flags & 8) != 0 && b != nullptr) GenerateHoardingMeshes(gGame.GetCurrentThemeName().mData, set, b);
    if ((flags & 0x80) != 0) SetupRideHeads(set->mHeads);
    if ((flags & 0x400) != 0) {
        // The height map file: the mesh's file name with ".hmp" in place of its extension.
        char* dot = std::strrchr(loadedName, '.');
        if (NeedsExtension(dot)) std::strcat(loadedName, ".hmp");
        else std::strcpy(dot, ".hmp");
        GetRideHeightMap(set, d, e, loadedName, a, flags & 0x800);
        if (set->mFD0 != nullptr) set->mFD0->mHeightMap = set->mHeightMap;
    }
    char fontPath[1024];
    if (GetFontPath(fontPath) == 1) {
        if (sign1 == nullptr) {
            const char16_t* typeName = gRideDatabase.GetTypeName(static_cast<uint16_t>(id));
            GenerateRideSign(set, folder, name, fontPath, typeName, nullptr);
        } else {
            GenerateRideSign(set, folder, name, fontPath, sign1, sign2);
        }
    } else {
        ERRORBox("Ridename texture generator: Could not find local path\n");
    }
    CorrectNodeFlags2(set);
    ++gRideTypeCount;
    gRideTypes[gRideTypeCount] = set;
    return gRideTypeCount;
}

// Works out the normal of every face of the root node's face sets, turned by `rotation` (0, 90,
// 180, 270 degrees; any other value leaves it as is). A face whose normal's y is below 0.999
// (or is NaN) gets its flag bit 0 set and its normal appended to the scratch geometry buffer;
// `result` counts them.
void CalculateRootNodeFaceNormals(tag_sNode* node, int32_t rotation, int32_t& result) {
    result = 0;
    const uint8_t* set = reinterpret_cast<const uint8_t*>(node->mFaceSets.get());    float* out = static_cast<float*>(gScratchGeomBuffer);
    for (uint32_t sets = node->mFaceSetCount; sets != 0; --sets, set += 0x10) {
        uint8_t* face = reinterpret_cast<uint8_t*>(node->mFaces.get()) + static_cast<uint32_t>(Half(set + 4)) * 8;
        const uint16_t* map = node->mF94.get();
        const uint8_t* verts = reinterpret_cast<const uint8_t*>(node->mVertices.get());
        for (uint32_t n = Half(set + 6); n != 0; --n, face += 8) {
            uint32_t ia = map[Half(face + 2)];
            uint32_t ib = map[Half(face + 4)];
            uint32_t ic = map[Half(face + 6)];
            const float* pa = reinterpret_cast<const float*>(verts + (ia >> 2) * 0x30 + (ia & 3) * 4);
            const float* pb = reinterpret_cast<const float*>(verts + (ib >> 2) * 0x30 + (ib & 3) * 4);
            const float* pc = reinterpret_cast<const float*>(verts + (ic >> 2) * 0x30 + (ic & 3) * 4);
            float x0 = pa[0], y0 = pa[4], z0 = pa[8];
            float f3 = x0 - pb[0];
            float f5 = z0 - pb[8];
            float f4 = y0 - pb[4];
            float f6 = x0 - pc[0];
            float f7 = y0 - pc[4];
            float f8 = z0 - pc[8];
            float t0 = f4 * f6;
            float t2 = f5 * f7;
            float t1 = f3 * f8;
            float nz = ppc_fmsubs(f3, f7, t0);
            float nx = ppc_fmsubs(f4, f8, t2);
            float ny = ppc_fmsubs(f5, f6, t1);
            float sy = ny * ny;
            float sz = nz * nz;
            double sum = ppc_fmadd(static_cast<double>(nx), static_cast<double>(nx), static_cast<double>(sy));
            double total = static_cast<double>(sz) + sum;
            float len = static_cast<float>(std::sqrt(total));
            nx = nx / len;
            ny = ny / len;
            nz = nz / len;
            if (rotation == 0xb4) {
                nx = nx * -1.0f;
                nz = nz * -1.0f;
            } else if (rotation == 0x5a) {
                float old = nx;
                nx = nz;
                nz = -old;
            } else if (rotation == 0x10e) {
                float old = nx;
                nx = -nz;
                nz = old;
            }
            if (ny < 0.999f) {   // fcmpo then bge: a NaN skips the face
                // Bit 0 of the face's 16-bit index-and-flag word. The Mac's stb to byte 1 is the low byte of
                // its big-endian word; on the host the word is native, so the flag goes on the word itself.
                uint16_t word;
                std::memcpy(&word, face, 2);
                word = static_cast<uint16_t>(word | 1);
                std::memcpy(face, &word, 2);
                out[0] = nx;
                out[1] = ny;
                out[2] = nz;
                out += 3;
                ++result;
            }
        }
    }
}

// Copies the blend flags of the ride mesh's skirt records onto the height field's cells under the
// ride, turned by the mesh's rotation (mF10), then asks the engine mesh for its base.
// With a rotation other than 0, 90, 180, 270 the original uses stale registers; not reproduced.
void GetBlendedBaseTextures(tag_sRideAnimSet* set) {
    tag_sMesh* mesh = set->mHeads;
    sHField* field = gHeightfieldHeader->mHField.get();
    tag_sNode* root = mesh->mHeader->mF78.get();
    const uint8_t* map = static_cast<const uint8_t*>(set->mSelf->mHeightMap);
    int32_t rotation = mesh->mF10;
    const uint8_t* src = static_cast<const uint8_t*>(mesh->mSkirtVertices);

    int32_t baseX = FloatToIntGlue(root->mMatrix.m[3][0] / field->mF08[2]);
    int32_t baseZ = FloatToIntGlue(root->mMatrix.m[3][2] / field->mF08[3]);
    if (rotation == 0xb4) {
        baseX -= 1;
        baseZ -= 1;
    } else if (rotation == 0x5a) {
        baseZ -= 1;
    } else if (rotation == 0x10e) {
        baseX -= 1;
    }
    int32_t stride = static_cast<int32_t>(field->mF18[0]);
    uint8_t* cells = reinterpret_cast<uint8_t*>(field->mQuads.get());
    int32_t x = 0, z = 0;
    for (uint32_t i = 0; i < Half(map + 0xa); ++i) {
        for (uint32_t j = 0; j < Half(map + 8); ++j) {
            int32_t ii = static_cast<int32_t>(i), jj = static_cast<int32_t>(j);
            if (rotation == 0) {
                x = baseX + jj;
                z = baseZ + ii;
            } else if (rotation == 0x5a) {
                x = baseX + ii;
                z = baseZ - jj;
            } else if (rotation == 0xb4) {
                x = baseX - jj;
                z = baseZ - ii;
            } else if (rotation == 0x10e) {
                x = baseX - ii;
                z = baseZ + jj;
            }
            uint32_t flags = Half(src);
            if ((flags & 0x80) != 0) {
                uint32_t value = flags & 0x40;
                uint8_t* cell = cells + static_cast<int32_t>((x + z * stride) * 4);
                uint32_t dir = flags & 0x38;
                if (rotation == 0) {
                    value |= dir;
                } else if (rotation == 0x5a) {
                    if (dir == 0) value |= 8;
                    else if (dir == 8) value |= 0x10;
                    else if (dir == 0x10) value |= 0x20;
                } else if (rotation == 0xb4) {
                    if (dir == 0) value |= 0x10;
                    else if (dir == 8) value |= 0x20;
                    else if (dir == 0x20) value |= 8;
                } else if (rotation == 0x10e) {
                    if (dir == 0) value |= 0x20;
                    else if (dir == 0x10) value |= 8;
                    else if (dir == 0x20) value |= 0x10;
                }
                uint16_t old = Half(cell);
                uint16_t merged = static_cast<uint16_t>(value | 0x281 | (old & 6));
                std::memcpy(cell, &merged, 2);
                std::memcpy(cell + 2, src + 2, 2);
            }
            src += 4;
        }
    }
    if (mesh->mEngineMesh->GenerateRideBase(set->mFC0.mF02, set->mFC0.mF00, set->mFC4.mF02, set->mFC4.mF00) == 0)
        ERRORBox("Could not blend base for ride %s, engine function returned 0\n", mesh->mHeader->mName);
}

// The ride mesh's frame callback and animation queries (SimTheme Park 2.04b3).
// TPRide.cpp: AnimateRideCallback 0x10069a98, ProcessRideAnim 0x1005b24c,
// HandleActiveHoardings 0x100619d4. rsse.cpp: GetCurrentAnimStats 0x100c7930.

// 0x10069a98, 80 bytes.
void AnimateRideCallback(tag_sM3D2Header* header) {
    tag_sRideAnimSet* set = header->mRideSet.get();
    if (set != nullptr) {
        ProcessRideAnim(set, 1);
        set->mFlags |= 0x200000;
    }
}

void ProcessRideAnim(tag_sRideAnimSet* set, int32_t animate);

void HandleActiveHoardings(tag_sRideAnimSet* set, float step);

// Makes an instance of ride type `type` (its "p" variant with flag 0x400 when it has one) and
// puts it in gRideAnimSets: at number `f`, or the first free one when `f` is 0xffffffff, which also
// keeps the three counters (see TPEngine.h). Returns the number, or 0 for no such type or an
// instance that could not be made. The flags: 1 and 2 and 0x80 become set flags 4, 0x4000 and
// 0x8000; 0x26 places the ride from `params` (2 also sets the world's bounding area, 4 the
// hoardings' heights, 8 the skirt deformation, with a height from GetRideRequiredYOffset when
// params has an object); 0x100 turns the mesh's drawing on; 0x200 makes a coaster of it.
// `a` is the number of time controls, `b` is kept in the set (+0xd4), `c` and `g` go to InstanceCoaster.
// The original also reads params+0xc when `d` or `e` is 0 and then drops it. With flag 8 but none
// of 0x26 the height it passes on is whatever the caller left in f31 (0 here), and with 0x200 or
// 8 but without 0x26 the cell it passes on is stack garbage (0 here).
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c,
                      int32_t d, int32_t e, uint32_t f, int32_t g) {
    tpw_probe::Hit(tpw_probe::kInstanceRide);
    if (type == 0) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    tag_sRideAnimSet* typeSet = gRideTypes[type];
    if (typeSet == nullptr) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    uint32_t instanceFlags = (flags & 0x800) != 0 ? 0x231f : 0x211f;
    tag_sRideAnimSet* instance = nullptr;
    int32_t made;
    if ((flags & 0x400) != 0 && typeSet->mFD0 != nullptr) {
        made = InstanceRideAnimSet(typeSet->mFD0, nullptr, instanceFlags, &instance, a);
    } else {
        made = InstanceRideAnimSet(typeSet, nullptr, instanceFlags, &instance, a);
    }
    if (made != 1) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    instance->mFlags &= ~1u;
    instance->mFE4 = flags;
    instance->mFD4 = b;
    // The number is index + 1.
    uint32_t index;
    if (f == 0xffffffff) {
        index = gRideFirstFree;
        gRideAnimSets[index + 1] = instance;
        if (index == gRideCountB) {
            ++gRideFirstFree;
            ++gRideCountB;
        } else {
            // A hole below the end was filled: look for the next one.
            uint32_t next = index + 1;
            while (next <= gRideCountB + gRideCountA) {
                if (gRideAnimSets[next + 1] == nullptr) {
                    gRideFirstFree = next;
                    break;
                }
                ++next;
            }
            --gRideCountA;
            ++gRideCountB;
        }
    } else {
        index = f - 1;
        gRideAnimSets[f] = instance;
    }
    if ((flags & 1) != 0) instance->mFlags |= 4;
    if ((flags & 2) != 0) instance->mFlags |= 0x4000;
    if ((flags & 0x80) != 0) instance->mFlags |= 0x8000;
    InitRideAnimSet(instance, 1, 0, 0);
    if (IsValidAnimation(instance, 0, 0) != 0) {
        SetCurRideAnim(instance, 0, 0, 0, 1.0f, 0);
        SetCurRideAnim(instance, 0xd, 0, 0, 1.0f, 0);
    }
    int32_t x = 0;
    int32_t z = 0;
    int32_t cellX = 0;
    int32_t cellZ = 0;
    float y = 0.0f;
    if ((flags & 0x26) != 0) {
        x = params->mX;
        z = params->mZ;
        const int32_t rotation = params->mRotation;
        void* object = params->mF0C;
        instance->mFE8 = params->mX;
        instance->mFEC = params->mZ;
        if (gHeightfieldHeader != nullptr && gHeightfieldHeader->mHField != nullptr) {
            CorrectRidePosnForRotation(&x, &z, rotation, &cellX, &cellZ, instance->mFF0, instance->mFF4);
        }
        if (object != nullptr && (flags & 8) != 0) {
            y = GetRideRequiredYOffset(cellX, cellZ, object, instance->mFF0, instance->mFF4, flags, rotation);
        } else {
            sHField* field = gHeightfieldHeader->mHField;
            const uint32_t at = static_cast<uint32_t>(x) + static_cast<uint32_t>(z) * (field->mF18[0] + 1);
            y = field->mHeights.get()[at];
        }
        SetRidePositionAndRotation(instance, cellX, cellZ, x, z, rotation, y, flags, instance->mFF0, instance->mFF4);
    }
    if ((flags & 8) != 0) {
        DoMeshSkirtDeform(instance->mHeads, gHeightfieldHeader, y, 0);
        if ((gGameOptions & 0x4000) != 0 && (instance->mHeads->mF00 & 0x40) != 0) GetBlendedBaseTextures(instance);
    }
    if ((flags & 2) != 0) SetWorldBoundingArea(instance->mHeads->mHeader, instance->mHeads->mHeader);
    if ((flags & 4) != 0) SetHoardingYOffsets(instance);
    tag_sMesh* mesh = instance->mHeads;
    if (mesh->mRootNode != nullptr) {
        int32_t calculated = 0;
        CalculateRootNodeFaceNormals(mesh->mRootNode, mesh->mF10, calculated);
        if (calculated != 0) mesh->mF30 = mesh->mRootNode->mPoints.get() + mesh->mRootNode->mVertexCount;
    }
    if ((flags & 0x100) != 0) {
        if ((instance->mFlags & 0x4000) == 0) EnableMeshDraw(instance->mHeads);
        else EnableMeshDraw(instance->mHeads, instance->mFC0, instance->mFC4);
    }
    if (instance->mHeads->mF28 != nullptr && (instance->mFlags & 0x20000000) == 0) TPProcessEmbeddedData(instance->mHeads);
    if ((flags & 0x200) != 0 && IsCoasterType(type) != 0) InstanceCoaster(type, index + 1, cellX, cellZ, g, c);
    // The header remembers its instance, and the engine mesh calls AnimateRideCallback every frame.
    instance->mHeads->mHeader->mRideSet = instance;
    if (instance->mHeads->mEngineMesh != nullptr) instance->mHeads->mEngineMesh->mCallback = AnimateRideCallback;
    if ((instance->mFlags & 0x1000000) != 0) OverrideActiveChannels(instance);
    if ((instance->mFlags & 0x2000000) != 0) StartFlashingObject(instance);
    tag_sM3D2Header* header = instance->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    for (uint32_t left = header->mNodeCount; left != 0; --left, ++node) node->mFlags |= 0x10000;
    header->mFlags |= 0x40000;
    RecalculateInvalidFaceNormals(instance->mHeads->mHeader);
    return index + 1;
}

// 0x1006a140: finds the existing instance (or checks the given one) and marks it restored. `a`, `f` are unused.
uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f) {
    int32_t width = d;
    if (width == 0) {
        if (params != nullptr && params->mF0C != nullptr) width = static_cast<const int32_t*>(params->mF0C)[0];
        else width = 0;
    }
    int32_t depth = e;
    if (depth == 0) {
        if (params != nullptr && params->mF0C != nullptr) depth = static_cast<const int32_t*>(params->mF0C)[1];
        else depth = 0;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(gRideTypes[type]->mFF8);
    RideInfo* info = gRideDatabase.GetRideInfo(id);

    tag_sRideAnimSet* set = nullptr;
    bool found = false;
    if (instance != 0) {
        found = true;
        set = gRideAnimSets[instance];
    } else {
        for (uint32_t i = 1; i <= gRideCountB + gRideCountA; ++i) {
            set = gRideAnimSets[i];
            if (set == nullptr) continue;
            uint32_t k = gRideTypeCount;
            for (; k != 0; --k)
                if (set->mSelf == gRideTypes[k]) break;
            if ((set->mFlags & 0x10000) != 0) continue;
            if (k != type) continue;
            if (set->mFE4 != flags) continue;
            if (params == nullptr) {
                found = true;
                instance = i;
                break;
            }
            if (set->mFE8 != params->mX) continue;
            if (set->mFEC != params->mZ) continue;
            if (set->mHeads->mF10 != params->mRotation) continue;
            if (params->mF0C == nullptr) {
                found = true;
                instance = i;
                break;
            }
            if (info != nullptr) {
                // The record's Shape slot (the Mac's +0x18) against the params' shape: whole pointers, by name (the Mac's compare is of
                // its 32-bit pointers; the host's record is laid out wider and its slot holds a native pointer).
                if (info->Value(kRideSlot_Shape).mShape != params->mF0C) continue;
                found = true;
                instance = i;
                break;
            }
            if (width != set->mFF0) continue;
            if (depth != set->mFF4) continue;
            found = true;
            instance = i;
            break;
        }
    }
    if (!found) return 0;
    if ((set->mFlags & 0x40000) != 0) SetCoasterCObject(instance, c);
    set->mFlags |= 0x10000;
    return instance;
}

// Takes instance `instance` out of the game: what points at it forgets it, its animation set is
// destroyed and its number is given back (the three counters, see TPEngine.h).
void DestroyRideInstance(uint32_t instance) {
    if (instance == 0) return;
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    if (set == nullptr) {
        CTRACE(5, "Cannot find a ride to destroy at slot %d - returning\n", instance);
        return;
    }
    if (gHighlightedRide == set) UnhighlightRide();
    if (set == gPotentialHighlightedRide) gPotentialHighlightedRide = nullptr;
    if (IsRideFirstPersonRide(instance) != 0) StopRidingRideIn1stPerson();
    if ((set->mFlags & 0x4000000) != 0) StopFlashingObject(set);
    DestroyCoasterInstanceFromEntranceID(instance);
    DestroyRideAnimSetInstance(set, nullptr, 0);
    gRideAnimSets[instance] = nullptr;
    const uint32_t index = instance - 1;
    if (gRideCountA == 0) {
        if (index == gRideCountB - 1) {
            --gRideFirstFree;
            --gRideCountB;
        } else {
            gRideCountA = 1;
            gRideFirstFree = index;
            --gRideCountB;
        }
    } else if (index == gRideCountB + gRideCountA - 1) {
        // The last number goes: the free numbers that now end the range go with it.
        --gRideCountB;
        while (gRideCountA != 0 && gRideAnimSets[gRideCountB + gRideCountA] == nullptr) --gRideCountA;
    } else {
        ++gRideCountA;
        --gRideCountB;
        if (index < gRideFirstFree) gRideFirstFree = index;
    }
}

// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)

int32_t DMFreeMesh(tag_sDMeshSet* set);

void DestroyRideHeightMap(tag_sRideAnimSet* set);

void DestroySkirtVertices(tag_sRideAnimSet* set);

void DestroyMeshHeader(tag_sM3D2Header* header);

void DestroyMesh(tag_sMesh* mesh);

// 0x1006a5e8. Mode 0 frees the height map block, any other mode only clears the pointer (the "p" variant shares its type's).
void DestroyRideAnimSet(tag_sRideAnimSet* set, int32_t mode) {
    for (uint32_t i = 0; i < set->mAnimationCount; ++i) DestroyMeshHeader(set->mAnimationHeaders[i]);
    MReleaseFreeF(set->mAnimations);
    set->mAnimations = nullptr;
    MReleaseFreeF(set->mAnimationHeaders);
    set->mAnimationHeaders = nullptr;
    MReleaseFreeF(set->mEntryAllocation);
    set->mEntryAllocation = nullptr;
    if (set->mDynamicMeshSet != nullptr) {
        DMFreeMesh(set->mDynamicMeshSet);
        MReleaseFreeF(set->mDynamicMeshSet->mEntries);
        set->mDynamicMeshSet->mEntries = nullptr;
        MReleaseFreeF(set->mDynamicMeshSet);
        set->mDynamicMeshSet = nullptr;
    }
    if (set->mHoardingGeometry != nullptr) {
        MReleaseFreeF(set->mHoardingGeometry->mBlock);
        set->mHoardingGeometry->mBlock = nullptr;
        MReleaseFreeF(set->mHoardingGeometry);
        set->mHoardingGeometry = nullptr;
    }
    if (mode == 0) {
        DestroyRideHeightMap(set);
    } else {
        set->mHeightMap = nullptr;
    }
    DestroyMesh(set->mHeads);
    MReleaseFreeF(set);
}

// 0x1006a768. The type's "p" variant goes first (DestroyRideAnimSet's body inline with mode 1), then the skirt, the sign and the type's
// own set with mode 0. A type number without a set crashes here as on the Mac.
void DestroyRideType(uint32_t type) {
    tag_sRideAnimSet* set = gRideTypes[type];
    tag_sRideAnimSet* variant = set->mFD0;
    if (variant != nullptr) {
        DestroyRideAnimSet(variant, 1);
        set->mFD0 = nullptr;
    }
    DestroySkirtVertices(set);
    DestroyRideSign(set);
    DestroyRideAnimSet(set, 0);
}

// Destroys every ride type, last first, and forgets them.
void DestroyAllRideTypes() {
    for (uint32_t type = gRideTypeCount; type != 0; --type) DestroyRideType(type);
    gRideTypeCount = 0;
}

// 0x1006aa70: traces the time control's fields (nothing for a control of kind 0xc).
void OutputAnimTimeStats(sAnimTimeControl* control, tag_sRideAnimSet* set) {
    if (control->mF04 == 0xc) return;
    char buffer[256];
    int32_t diff = static_cast<int32_t>(reinterpret_cast<uintptr_t>(control) - reinterpret_cast<uintptr_t>(set->mTimeControls));
    int64_t magic = static_cast<int32_t>(0x92492493u);
    int32_t high = static_cast<int32_t>((magic * diff) >> 32);
    int32_t t = (high + diff) >> 5;
    int32_t index = t + static_cast<int32_t>(static_cast<uint32_t>(t) >> 31);
    const char* name = reinterpret_cast<const char*>(set->mHeads->mHeader) + 0x18;
    sprintf(buffer, "interesting AnimTimeControl %d/%d to ride instance %s\n", index, static_cast<int32_t>(set->mTimeCount), name);
    CTRACE(5, buffer);
    sprintf(buffer, "Flags                : %d\n", control->mFlags);
    CTRACE(5, buffer);
    sprintf(buffer, "AnimID               : %d\n", control->mF04);
    CTRACE(5, buffer);
    sprintf(buffer, "SubAnim              : %d\n", control->mF08);
    CTRACE(5, buffer);
    sprintf(buffer, "StartAnimTime        : %d\n", control->mF10);
    CTRACE(5, buffer);
    sprintf(buffer, "AnimTime             : %d\n", control->mF14);
    CTRACE(5, buffer);
    sprintf(buffer, "NoPauseAnimTime      : %d\n", control->mF18);
    CTRACE(5, buffer);
    sprintf(buffer, "TotalAnimFrames      : %.2f\n", static_cast<double>(control->mF1C));
    CTRACE(5, buffer);
    sprintf(buffer, "AnimFrame            : %.2f\n", static_cast<double>(control->mF20));
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredAnimID       : %d\n", control->mF24);
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredSubAnim      : %d\n", control->mF28);
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredFlags        : %d\n\n", control->mF2C);
    CTRACE(5, buffer);
}

// SaveRides 0x1006acf8 (2308 bytes): writes the ride anim sets to the save file. Two passes over the sets: the first adds up the size
// of the record block, the second fills it (every word swapped to the file's little-endian form the way the original does, field by field).

// 0x1029d050: the word the compiler keeps for the TbError comparison of the second write (0 in the binary).
uint32_t gSaveRidesTbErrorZero = 0;

namespace {

template <class T>
void Store(uint8_t* where, T value) {
    std::memcpy(where, &value, sizeof value);
}

// A word of the file: swapped, then stored (the original stores it and swaps it in place).
void PutWord(uint8_t* where, uint32_t value) {
    SamsUtilities::USwapInplace32<uint32_t>(value);
    Store(where, value);
}
void PutHalf(uint8_t* where, uint16_t value) {
    SamsUtilities::USwapInplace16<uint16_t>(value);
    Store(where, value);
}
void PutFloat(uint8_t* where, float value) {
    TPCoasterSwap::USwapInplaceFloat(value);
    Store(where, value);
}


}  // namespace

int32_t SaveRides(void* file) {
    uint32_t countB = gRideCountB;
    uint32_t countA = gRideCountA;
    uint32_t firstFree = gRideFirstFree;
    uint32_t total = 0xc;
    uint8_t* block = nullptr;
    uint8_t* p = nullptr;
    for (uint32_t pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            block = static_cast<uint8_t*>(MReleaseAllocF(total, 1));
            PutWord(block, countB);
            PutWord(block + 4, countA);
            PutWord(block + 8, firstFree);
            p = block + 12;
        }
        for (uint32_t slot = 0; slot < countB + countA; ++slot) {
            tag_sRideAnimSet* set = gRideAnimSets[slot + 1];
            if (pass == 0) {
                total += 1;
            } else if (set != nullptr) {
                *p++ = 1;
            } else {
                *p++ = 0;
            }
            if (set == nullptr) continue;
            if (pass == 0) {
                total += 0x36;
                total += set->mTimeCount * 0x2c;
                total += set->mHeads->mHeader->mEmbeddedCount * 8;
                total += set->mHeads->mHeader->mAllNodeCount * 4;
                continue;
            }
            PutWord(p + 0x00, set->mFF8);
            PutWord(p + 0x04, set->mFE8);
            PutWord(p + 0x08, set->mFEC);
            PutWord(p + 0x0c, set->mFF0);
            PutWord(p + 0x10, set->mFF4);
            PutWord(p + 0x14, set->mFE4);
            PutWord(p + 0x18, set->mFD4);
            uint32_t bits = 0;
            const uint32_t flags = set->mFlags;
            if (flags & 0x20) bits |= 1;
            if (flags & 0x40) bits |= 2;
            if (flags & 0x80) bits |= 4;
            if (flags & 0x100) bits |= 8;
            if (flags & 0x200) bits |= 0x10;
            if (flags & 0x400) bits |= 0x20;
            if (flags & 0x800) bits |= 0x40;
            if (flags & 0x40000) bits |= 0x80;
            if (flags & 0x8000000) bits |= 0x100;
            PutWord(p + 0x1c, bits);
            PutHalf(p + 0x20, set->mF0C);
            PutFloat(p + 0x22, set->mHoardingLevel);
            p += 0x26;

            tag_sMesh* mesh = set->mHeads;
            tag_sM3D2Header* header = mesh->mHeader;
            PutWord(p + 0, static_cast<uint32_t>(mesh->mF10));
            PutHalf(p + 4, header->mAllNodeCount);
            PutHalf(p + 6, header->mEmbeddedCount);
            sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(mesh->mF28);
            if (embedded != nullptr) {
                PutWord(p + 8, embedded->mFlags);
                PutWord(p + 0xc, embedded->mF08);
                p += 0x10;
                sEmbeddedRecord* record = embedded->mRecords;
                for (uint32_t i = 0; i < mesh->mHeader->mEmbeddedCount; ++i) {
                    PutWord(p, record->mFlags);
                    PutWord(p + 4, record->mF0C);
                    ++record;
                    p += 8;
                }
            } else {
                // Words 8 and 0xc are swapped as they stand in the block (zeros from the allocator).
                uint32_t w;
                std::memcpy(&w, p + 8, 4);
                PutWord(p + 8, w);
                std::memcpy(&w, p + 0xc, 4);
                PutWord(p + 0xc, w);
                p += 0x10;
            }

            for (uint32_t i = 0; i < header->mAllNodeCount; ++i) {
                const uint8_t* node;
                if (i < header->mNodeCount) {
                    node = reinterpret_cast<const uint8_t*>(header->mNodes.get()) + i * 0xa0;
                } else {
                    node = reinterpret_cast<const uint8_t*>(header->mMiniNodes.get()) + (i - header->mNodeCount) * 0x58;
                }
                const uint32_t nflags = *reinterpret_cast<const uint32_t*>(node);
                uint32_t out = 0;
                if (nflags & 0x10) out |= 1;
                if (nflags & 0x20) out |= 2;
                if (nflags & 0x8000) out |= 4;
                if (nflags & 0x10000) out |= 8;
                if (nflags & 0x40000) out |= 0x20;
                if (nflags & 0x80000) out |= 0x40;
                if (nflags & 0x200000) out |= 0x80;
                if (nflags & 0x400000) out |= 0x100;
                if (nflags & 0x100000) out |= 0x800;
                if (nflags & 0x20000000) out |= 0x200;
                if (nflags & 0x80000000u) out |= 0x400;
                PutWord(p, out);
                p += 4;
            }

            sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
            for (uint32_t i = 0; i < set->mTimeCount; ++i) {
                if (control->mF04 != 0xc) CTRACE(5, "Saving anim time control\n");
                OutputAnimTimeStats(control, set);
                PutWord(p + 0x00, control->mFlags);
                PutWord(p + 0x04, control->mF04);
                PutWord(p + 0x08, control->mF08);
                PutWord(p + 0x0c, static_cast<uint32_t>(control->mF10));
                Store(p + 0x10, static_cast<uint32_t>(control->mF14));   // not swapped: the original has no call for it
                PutWord(p + 0x14, static_cast<uint32_t>(control->mF18));
                PutFloat(p + 0x18, control->mF0C);
                PutWord(p + 0x1c, control->mF24);
                PutWord(p + 0x20, control->mF28);
                PutWord(p + 0x24, control->mF2C);
                PutFloat(p + 0x28, control->mF30);
                ++control;
                p += 0x2c;
            }
        }
    }
    uint32_t length = total;
    SamsUtilities::USwapInplace32<uint32_t>(length);
    uint32_t written = 0;
    LbFile_Write(file, &length, 4, &written);
    if (written >> 2 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    TbError result = LbFile_Write(file, block, total, nullptr);
    if (result.Succeeded() != (gSaveRidesTbErrorZero == 0)) return 0;
    MReleaseFreeF(block);
    return 1;
}

// RestoreAnimTime 0x1006b6f4 (288 bytes): copies a saved time control back, re-finds the animation and restores its stats.

bool RestoreAnimTime(tag_sRideAnimSet* set, sAnimTimeControl* control, sAnimTimeControlSave* save) {
    control->mFlags = save->mF00;
    control->mF04 = save->mF04;
    control->mF08 = save->mF08;
    control->mF10 = static_cast<int32_t>(save->mF0C);
    control->mF14 = static_cast<int32_t>(save->mF10);
    control->mF18 = static_cast<int32_t>(save->mF14);
    control->mF0C = save->mF18;
    if (control->mFlags & 4) control->mFlags |= 0x10;
    control->mF24 = save->mF1C;
    control->mF28 = save->mF20;
    control->mF2C = save->mF24;
    control->mF30 = save->mF28;
    uint32_t b = control->mF08;
    uint32_t a = control->mF04;
    tag_sRideAnimSet* local = set;
    tag_sAnimHeader* animation = nullptr;
    if (IsValidAnimation(local, a, b) != 0) animation = local->mGroups[a].mEntries[b].mAnimation;
    if (animation != nullptr) RestoreAnimTimeStats(control, animation);
    return control->mF04 == 0;
}

// LoadRides 0x1006b878 (2200 bytes, unit v5890): reads the rides module of a save: every ride instance is made again with InstanceRide, its
// flags, hoardings, embedded data, node flags and animation time controls restored from the file.

int32_t gLoadRidesTbErrorZero = 0;      // 0x1029d054
int32_t gLoadRidesShape[2] = {0, 0};    // 0x102c6d04

namespace {

// The save file is little endian: the Mac swaps each word where it lies (the swaps are real there); a little-endian host leaves them as
// they are, a big-endian one (or TPW_SWAP_FILE_WORDS) swaps. The file's words are not aligned, so they go through memcpy.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
constexpr bool kSwapFileWords = true;
#else
constexpr bool kSwapFileWords = false;
#endif

void Swap32At(uint8_t* p) {
    if (!kSwapFileWords) return;
    uint32_t v;
    std::memcpy(&v, p, 4);
    v = __builtin_bswap32(v);
    std::memcpy(p, &v, 4);
}

void Swap16At(uint8_t* p) {
    if (!kSwapFileWords) return;
    uint16_t v;
    std::memcpy(&v, p, 2);
    v = __builtin_bswap16(v);
    std::memcpy(p, &v, 2);
}

uint32_t Word(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

int32_t SWord(const uint8_t* p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

uint16_t Half_2(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}


}  // namespace

int32_t LoadRides(void* file) {
    uint32_t sizeWord = 0;
    uint32_t transferred = 0;

    LbFile_Read(file, &sizeWord, 4, &transferred);
    if (transferred >> 2 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    Swap32At(reinterpret_cast<uint8_t*>(&sizeWord));
    uint8_t* block = static_cast<uint8_t*>(MReleaseAllocF(sizeWord, 0));

    TbError error = LbFile_Read(file, block, sizeWord, nullptr);
    // The compiler's comparison with a constant TbError: (error >= 0) == (zero word == 0) is the success.
    if (error.Succeeded() != (gLoadRidesTbErrorZero == 0)) return 0;

    uint8_t* cursor = block;
    std::memcpy(&gRideCountB, cursor, 4);
    cursor += 4;
    std::memcpy(&gRideCountA, cursor, 4);
    cursor += 4;
    std::memcpy(&gRideFirstFree, cursor, 4);
    cursor += 4;
    Swap32At(reinterpret_cast<uint8_t*>(&gRideCountB));
    Swap32At(reinterpret_cast<uint8_t*>(&gRideCountA));
    Swap32At(reinterpret_cast<uint8_t*>(&gRideFirstFree));

    TickRideAnims();

    for (uint32_t index = 0; index < gRideCountB + gRideCountA; index++) {
        uint8_t marker = *cursor++;
        if (marker != 1) continue;

        uint8_t* rec = cursor;          // r30: the ride's record
        cursor += 0x26;
        uint8_t* hdr = cursor;          // r31: the node and embedded counts and the embedded words
        cursor += 0x10;

        Swap32At(rec + 0x00);
        Swap32At(rec + 0x04);
        Swap32At(rec + 0x08);
        Swap32At(rec + 0x0c);
        Swap32At(rec + 0x10);
        Swap32At(rec + 0x14);
        Swap32At(rec + 0x18);
        Swap32At(rec + 0x1c);
        Swap16At(rec + 0x20);
        Swap32At(rec + 0x22);

        Swap32At(hdr + 0x00);
        Swap16At(hdr + 0x04);
        Swap16At(hdr + 0x06);
        Swap32At(hdr + 0x08);
        Swap32At(hdr + 0x0c);

        uint32_t typeNumber = Word(rec);
        uint32_t typeIndex = gRideTypeCount;
        for (;;) {
            if (typeIndex == 0) break;
            if (typeNumber == gRideTypes[typeIndex]->mFF8) break;
            typeIndex--;
        }

        uint32_t rotation = Word(hdr);
        int32_t e = SWord(rec + 0x10);
        int32_t d = SWord(rec + 0xc);
        int32_t z = SWord(rec + 8);
        int32_t x = SWord(rec + 4);

        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = static_cast<uint16_t>(typeNumber);
        RideInfo* info = gRideDatabase.GetRideInfo(id);

        sFixedRideParams params;
        params.mX = x;
        params.mZ = z;
        params.mRotation = static_cast<int32_t>(rotation);
        if (info != nullptr) {
            params.mF0C = info->Value(kRideSlot_Shape).mShape;
        } else if (d != 0 && e != 0) {
            gLoadRidesShape[0] = d;
            gLoadRidesShape[1] = e;
            params.mF0C = gLoadRidesShape;
        } else {
            params.mF0C = nullptr;
        }

        uint32_t number = InstanceRide(typeIndex, Word(rec + 0x14), &params, 0, SWord(rec + 0x18), nullptr, SWord(rec + 0xc),
                                       SWord(rec + 0x10), index + 1, 1);
        tag_sRideAnimSet* set = gRideAnimSets[number];
        tag_sMesh* mesh = set->mHeads;
        tag_sM3D2Header* header = mesh->mHeader;

        bool same;
        if (Half_2(hdr + 6) == header->mEmbeddedCount && Half_2(hdr + 4) == header->mAllNodeCount) {
            same = true;
        } else {
            same = false;
            ERRORBox("Node/Embedded node count changed on mesh %s since save\n"
                     "Cannot safely restore node data - expect some (hopefully minor) problems with this ride",
                     header->mName);
        }

        set->mFlags &= 0xf7fff01fu;
        uint32_t saved = Word(rec + 0x1c);
        if ((saved & 1) != 0) set->mFlags |= 0x20;
        if ((saved & 2) != 0) set->mFlags |= 0x40;
        if ((saved & 4) != 0) set->mFlags |= 0x80;
        if ((saved & 8) != 0) set->mFlags |= 0x100;
        if ((saved & 0x10) != 0) set->mFlags |= 0x200;
        if ((saved & 0x20) != 0) set->mFlags |= 0x400;
        if ((saved & 0x40) != 0) set->mFlags |= 0x800;
        if ((saved & 0x80) != 0) set->mFlags |= 0x40000;
        if ((saved & 0x100) != 0) set->mFlags |= 0x8000000;
        set->mF0C = Half_2(rec + 0x20);
        std::memcpy(&set->mHoardingLevel, rec + 0x22, 4);
        ReactivateHoardings(set);

        sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(mesh->mF28);
        if (embedded != nullptr && same) {
            embedded->mFlags = Word(hdr + 8);
            embedded->mF08 = Word(hdr + 0xc);
            sEmbeddedRecord* records = embedded->mRecords;
            for (uint32_t i = 0; i < set->mHeads->mHeader->mEmbeddedCount; i++) {
                Swap32At(cursor);
                Swap32At(cursor + 4);
                records->mFlags = Word(cursor);
                records->mF0C = Word(cursor + 4);
                records++;
                cursor += 8;
            }
        } else {
            cursor += static_cast<uint32_t>(Half_2(hdr + 6)) * 8;
        }

        if (same) {
            for (uint32_t i = 0; i < header->mAllNodeCount; i++) {
                uint32_t* nodeFlags;
                if (i < header->mNodeCount) {
                    nodeFlags = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + i * 0xa0);
                } else {
                    nodeFlags = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) +
                                                            (i - header->mNodeCount) * 0x58);
                }
                *nodeFlags &= 0x5f827fcfu;
                Swap32At(cursor);
                uint32_t w = Word(cursor);
                if ((w & 1) != 0) *nodeFlags |= 0x10;
                if ((w & 2) != 0) *nodeFlags |= 0x20;
                if ((w & 4) != 0) *nodeFlags |= 0x8000;
                if ((w & 8) != 0) *nodeFlags |= 0x10000;
                if ((w & 0x20) != 0) *nodeFlags |= 0x40000;
                if ((w & 0x40) != 0) *nodeFlags |= 0x80000;
                if ((w & 0x80) != 0) *nodeFlags |= 0x200000;
                if ((w & 0x100) != 0) *nodeFlags |= 0x400000;
                if ((w & 0x800) != 0) *nodeFlags |= 0x100000;
                if ((w & 0x200) != 0) *nodeFlags |= 0x20000000;
                if ((w & 0x400) != 0) *nodeFlags |= 0x80000000u;
                cursor += 4;
            }
        } else {
            cursor += static_cast<uint32_t>(Half_2(hdr + 4)) * 4;
        }

        sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
        int32_t restored = 0;
        for (uint32_t i = 0; i < set->mTimeCount; i++) {
            Swap32At(cursor + 0x00);
            Swap32At(cursor + 0x04);
            Swap32At(cursor + 0x08);
            Swap32At(cursor + 0x0c);
            Swap32At(cursor + 0x14);
            Swap32At(cursor + 0x18);
            Swap32At(cursor + 0x1c);
            Swap32At(cursor + 0x20);
            Swap32At(cursor + 0x24);
            Swap32At(cursor + 0x28);
            if (Word(cursor + 4) != 0xc) CTRACE(5, "Restoring anim time control\n");
            restored += RestoreAnimTime(set, control, reinterpret_cast<sAnimTimeControlSave*>(cursor));
            OutputAnimTimeStats(control, set);
            control++;
            cursor += 0x2c;
        }
        if (restored == 0) StaticifyCreateOnlyAnimNodes(set);

        for (uint32_t i = 0; i < set->mTimeCount; i++) {
            if (static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF04 < 0xc) set->mFlags |= 0x400000;
        }
        if ((set->mFlags & 0x1000000) != 0) {
            OverrideActiveChannels(set);
        } else {
            GetNumActiveChannels(set);
        }
    }

    MReleaseFreeF(block);
    return 1;
}

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
const char16_t kRideW[] = u"gms.dat";              // 0x1023d8e8
}  // namespace

alignas(16) unsigned char gV51077RideW1edc[sizeof(W)];     // 0x102d1edc
unsigned char gV51077RideNode1ef0[16];                      // 0x102d1ef0
void __sinit_TPRide_cpp() {
    void* object = new (static_cast<void*>(gV51077RideW1edc)) W(kRideW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077RideNode1ef0);
}
