// TPEmbed.cpp: name proven by __sinit_TPEmbed_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "GameSys.h"
#include "LbDrawPrimitives.h"
#include "LbSCBack.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "UI_pump.h"
#include "World.h"
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Unit v5962: the embedded-entry counters (0x101fff0c, 0x101ffe74), EnableRideHead (0x10200230), bthread_cond (0x10200e34, 0x10200ea8,
// 0x10200f30) and CalculateOrientFromTrackMorph (0x101fec54).

// Name invented, only the address is used (0x1029fda8).
uint32_t gRideHeadSpawnBlock = 0;

__attribute__((weak)) NS_MacDoze::InternalHandle* NS_MacDoze::CreateEvent(void*, int32_t, int32_t, const char*) { return nullptr; }
__attribute__((weak)) uint32_t NS_MacDoze::WaitForSingleObject(NS_MacDoze::InternalHandle*, uint32_t) { return 0; }
__attribute__((weak)) void bthread_callee::OperatorDelete(void* p) { ::operator delete(p); }

// 0x101fec54: the matrix of a track morph node: the third row is the direction from points to position (the normal's own turn when that is
// not a number), the second the normal, the first their cross product; then the morph's three angles (looked up in the sine table, the
// cosine taken half a turn on) turn it about z, y and x.
void CalculateOrientFromTrackMorph(s3DPoint* points, s3DPoint* position, s3DPoint* normal, sTrackMorphingParent* morph, sMatrix* matrix) {
    const uint8_t* angles = reinterpret_cast<const uint8_t*>(morph) - morph->mBack;   // three floats: x, y, z
    float dy = position->y - points->y;
    float dx = position->x - points->x;
    float dz = position->z - points->z;
    double sum =ppc_fmadd(dx, dx, static_cast<double>(dy * dy));
    double length = std::sqrt(static_cast<double>(dz * dz) + sum);
    float len = static_cast<float>(length);
    float fx = dx / len;
    float fy = dy / len;
    float fz = dz / len;
    if (std::isnan(fx)) {
        fx = normal->z;
        fy = normal->x;
        fz = normal->y;
    }
    float* m = &matrix->m[0][0];
    m[0] = ppc_fmsubs(normal->y, fz, normal->z * fy);
    m[1] = ppc_fmsubs(normal->z, fx, normal->x * fz);
    m[2] = ppc_fmsubs(normal->x, fy, normal->y * fx);
    m[4] = normal->x;
    m[5] = normal->y;
    m[6] = normal->z;
    m[8] = fx;
    m[9] = fy;
    m[10] = fz;
    sMatrix turn;
    SetMatrixIdentity(&turn);
    const float halfTurn = 1.5707963705062866f;
    float angle = reinterpret_cast<const float*>(angles)[2];
    int32_t s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    int32_t c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    ZRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    angle = reinterpret_cast<const float*>(angles)[1];
    s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    YRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    angle = reinterpret_cast<const float*>(angles)[0];
    s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    XRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    CombineMatricesNoWNoTransDest2nd(&turn, matrix);
}

// The ride animation tick of the main app (SimTheme Park 2.04b3): what moves a ride set's time
// controls on and applies the animations they select to the nodes of the set's mesh. Eight
// functions of three ranges of the inventory (rsse.cpp, TPRide.cpp, TPEmbed.cpp):
//   TickRideAnimSet 0x100c6a20, DoAnimateRideSet 0x100c6c2c, RAAnimateTextureFrames 0x100c3678,
//   AnimateNode 0x100c45d4, GetAnimFrameData 0x100c34bc, IsMeshDrawEnabled 0x10065364,
//   TPProcessEmbeddedData 0x101ffb0c and DoWorldTransformsForEmbeddedData 0x101fef24.
// AnimateRideSet (src/RideTick.cpp) calls TickRideAnimSet, DoAnimateRideSet and
// TPProcessEmbeddedData; InstanceRide (src/RideTypes.cpp) calls the last as well.

// 0x101fef24, 1,744 bytes. See TPEngine.h. A node without flag 0x20000000 only multiplies its own
// matrix into the running one. A node with it has an embedded record (the node's mF50 less the
// header's mF46): if the record is complete (flag 0x20 with none of 0xe set it is skipped) the
// node's matrix is multiplied in, and when the entry has flag 0x40 or 0x40000 and the node's
// parent has flag 0x200000 the matrix's translation becomes the point the entry's morph record
// gives: the triangle (a face of the parent) carried through `world`, its corners blended by the
// record's weights a and b, and moved c along the face's normal (also carried). With entry flag
// 0x40000 the rotation comes from CalculateOrientFromTrackMorph. The matrix then goes in the
// record's block. After it, a node with flags 0x10000000 and 0x10000 has its normals calculated
// again, and the nodes below it are walked with the matrix as it now is (children with flag
// 0x40000000 are skipped), each handed that matrix as the world matrix of its parent.
void DoWorldTransformsForEmbeddedData(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sMatrix* world,
                                      sEmbeddedRecord* records) {
    if ((node->mFlags & 0x20000000) == 0) {
        CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    } else {
        const int32_t index = static_cast<int32_t>(node->mF50) - static_cast<int32_t>(header->mF46);
        sEmbeddedRecord& record = records[index];
        const sEmbeddedData& entry = header->mEmbedded.get()[index];
        if ((record.mFlags & 0x20) == 0 || (record.mFlags & 0xe) != 0) {
            CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
            // (The parent is not checked for null: the original faults on a missing one.)
            if ((entry.mFlags & 0x40040) != 0 && (node->mF04->mFlags & 0x200000) != 0) {
                tag_sNode* parent = node->mF04.get();
                sTrackMorphingParent* morph = entry.mMorphParent.get();
                const sFace* face = parent->mFaces.get() + morph->mF00;
                // The face's three corners, carried through the parent's world matrix.
                s3DPoint corner[3];
                for (int k = 0; k < 3; ++k) {
                    const uint16_t word = parent->mF94.get()[face->mF02[k]];
                    const sVertexPosnSet& set = parent->mVertices.get()[word >> 2];
                    const uint32_t lane = word & 3;
                    corner[k] = {set.x[lane], set.y[lane], set.z[lane]};
                    VectorThroughLMatrixNoW(world, &corner[k]);
                }
                // The face's normal, carried through the rotation only.
                const s3DPoint normalIn = parent->mPoints.get()[(face->mIndexAndFlag >> 1) & 0x7fff];
                const sMatrix& w = *world;
                float nx = normalIn.y * w.m[1][0];
                float ny = normalIn.y * w.m[1][1];
                float nz = normalIn.y * w.m[1][2];
                nx = ppc_fmadds(normalIn.x, w.m[0][0], nx);
                ny = ppc_fmadds(normalIn.x, w.m[0][1], ny);
                nz = ppc_fmadds(normalIn.x, w.m[0][2], nz);
                nx = ppc_fmadds(normalIn.z, w.m[2][0], nx);
                ny = ppc_fmadds(normalIn.z, w.m[2][1], ny);
                nz = ppc_fmadds(normalIn.z, w.m[2][2], nz);
                s3DPoint normal = {nx, ny, nz};
                // The record's three weights (floats kept in its words 1 to 3).
                float a;
                float b;
                float c;
                std::memcpy(&a, &morph->mF04[0], sizeof a);
                std::memcpy(&b, &morph->mF04[1], sizeof b);
                std::memcpy(&c, &morph->mF04[2], sizeof c);
                const float restA = 1.0f - a;
                float x = ppc_fmadds(corner[0].x, restA, corner[1].x * a);
                float y = ppc_fmadds(corner[0].y, restA, corner[1].y * a);
                float z = ppc_fmadds(corner[0].z, restA, corner[1].z * a);
                const float restB = 1.0f - b;
                x = ppc_fmadds(x, restB, corner[2].x * b);
                y = ppc_fmadds(y, restB, corner[2].y * b);
                z = ppc_fmadds(z, restB, corner[2].z * b);
                s3DPoint position = {x, y, z};
                matrix.m[3][0] = ppc_fmadds(c, normal.x, x);
                matrix.m[3][1] = ppc_fmadds(c, normal.y, y);
                matrix.m[3][2] = ppc_fmadds(c, normal.z, z);
                if ((entry.mFlags & 0x40000) != 0) {
                    CalculateOrientFromTrackMorph(corner, &position, &normal, morph, &matrix);
                }
            }
            std::memcpy(record.mBlock, &matrix, sizeof matrix);
        }
    }
    if ((node->mFlags & 0x10000000) != 0 && (node->mFlags & 0x10000) != 0) RecalculateInvalidNodeFaceNormals(node);
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get()) {
        if ((child->mFlags & 0x40000000) == 0) DoWorldTransformsForEmbeddedData(child, header, matrix, &matrix, records);
    }
}

// The embedded data of a mesh (the main app's TPEmbed.cpp range): TPSetupEmbeddedHeirarchy
// (0x101ff674) and TPInitialiseEmbeddedData (0x101ff7e4). A mesh's embedded records are the
// header's sEmbeddedData entries (file layout, include/m3d2.h); the set built for them and its
// records use native pointers (include/TPEngine.h).

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

// TPEmbed.cpp (main app 0x1020039c and 0x101ffa58, unit w114): turning a ride head off and giving a mesh's embedded data back.

// Gives the mesh's embedded set back: the blocks, the records and the set itself (MReleaseFreeF each, also for a NULL pointer, which it
// ignores), clearing the first two pointers after each call and the mesh's word at the end. A mesh without a set does nothing.
void TPDestroyEmbeddedData(tag_sMesh* mesh) {
    if (mesh->mF28 != nullptr) {
        sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
        MReleaseFreeF(set->mBlocks);
        set->mBlocks = nullptr;
        MReleaseFreeF(set->mRecords);
        set->mRecords = nullptr;
        MReleaseFreeF(mesh->mF28);
        mesh->mF28 = nullptr;
    }
}

// 0x101ffb0c, 264 bytes. For a mesh that is in the map (flag 1) and has embedded data with a
// block (the set's flag 1), works out the world matrices of the embedded nodes from the root,
// starting from the identity.
void TPProcessEmbeddedData(tag_sMesh* mesh) {
    if ((mesh->mF00 & 1) == 0) return;
    auto* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    if ((set->mFlags & 1) == 0) return;
    sMatrix identity;
    SetMatrixIdentity(&identity);
    DoWorldTransformsForEmbeddedData(mesh->mHeader->mF78.get(), mesh->mHeader, identity, &identity, set->mRecords);
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

// The small functions the advisor calls (main app of SimTheme Park 2.04b3), each from a range of its own:
//   GetDeferredAnim 0x100c7a74 and GetAnimLength 0x100c7bb4 (rsse.cpp, next to the animation tick),
//   SetMeshInUISpace 0x1006553c (TPRide.cpp, the mesh draw switches),
//   FindDataNodeIndexByID 0x101ffdc8 and FindDataNodeIndexByIDAdvisorTempHack 0x101fff90 (TPEmbed.cpp),
//   FindDataNodeIndexByDisableCode 0x1020017c (TPEmbed.cpp, unit v5L21).
// They are here and not in the files of their neighbours so that the advisor's unit library links without the rest of
// those files. (ScaleLMatrixNoTrans is in src/rsse_matrix.cpp and SND_DuckVolume in src/TPSound.cpp.)

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

// 0x101ffe74: the index of the n-th (counting from 1) embedded entry with a flag in `kind`, -1 when there are fewer.
int32_t FindNthDataNodeIndex(tag_sRideAnimSet* set, uint32_t kind, uint32_t n) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t found = 0;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        if ((kind & header->mEmbedded[i].mFlags) != 0) {
            found++;
            if (found == n) return static_cast<int32_t>(i);
        }
    }
    return -1;
}

// 0x101fff0c: how many embedded entries of the set's mesh have a flag in `kind`.
uint32_t CountDataNodesByType(tag_sRideAnimSet* set, uint32_t kind) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t count = 0;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        if ((kind & header->mEmbedded[i].mFlags) != 0) count++;
    }
    return count;
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

// 0x10200230. The original also compares index + the header's halfword 0x46 with the one at 0x44 (the result is dropped); the node count
// (0x44) reaches the assert's argument list because its register is still live.
void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) {
    sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    sEmbeddedRecord* record = set->mRecords + index;
    set->mFlags = set->mFlags | 4;
    set->mF08 = set->mF08 + 1;
    record->mFlags = record->mFlags | 2;
    tag_sM3D2Header* header = mesh->mHeader;
    uint32_t head;
    if (peep != 0) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n", peep,
                       static_cast<uint32_t>(header->mNodeCount));
        head = static_cast<CPerson*>(gThingArray[peep & 0xffff].mThing)->GetHeadSpriteID();
    } else {
        head = 0;
    }
    if ((head & 0x80000000u) != 0) {
        record->mF0C = static_cast<uint32_t>(ESP_SpawnScript(&gRideHeadSpawnBlock, static_cast<ESP_Sprites>(3),
                                                              static_cast<int32_t>(head & 0x7fffffffu), static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f));
    } else {
        record->mF0C = static_cast<uint32_t>(ESP_SpawnScript(&gRideHeadSpawnBlock, static_cast<ESP_Sprites>(1), static_cast<int32_t>(head),
                                                              static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f));
    }
}

// Turns the embedded entry `index` of the mesh's set off: the set's counter (word +8, the enabled heads as far as is known) goes down by
// one (wrapping) and, when it is then 0, the set's flag 4 (TickRideHeads runs) is cleared; the entry loses flag 2 (a head) whatever the
// counter is, its script is killed and its handle set to 0xffffffff. Nothing is bounds checked. The original also loads the mesh header's
// halfwords at 0x46 and 0x44 and compares index plus the first with the second, with no branch on the result (a compiled-out
// assertion); it is left out, which only matters for a mesh with no header.
void DisableRideHead(tag_sMesh* mesh, uint32_t index) {
    sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    set->mF08 = set->mF08 - 1;
    sEmbeddedRecord* record = set->mRecords + index;
    if (static_cast<sEmbeddedSet*>(mesh->mF28)->mF08 == 0) set->mFlags = set->mFlags & ~4u;
    record->mFlags = record->mFlags & ~2u;
    SCR_KillScript(static_cast<int32_t>(record->mF0C));
    record->mF0C = 0xffffffff;
}

// TPEmbed.cpp (main app of SimTheme Park 2.04b3): TickRideHeads 0x10200460, which turns the heads of a ride's
// embedded set towards the camera. A head is an embedded entry (flag 2) whose 0x40-byte block is a 4 x 4 matrix
// and whose word at +0xc is the handle of a script object; the function does not move the head, it gives the
// script object the head's position and three variables (13: which of 7 x 8 sampled directions looks closest to
// the camera, 9: the head's turn about its own axis as a fraction of a turn, 8: a constant, see below).
//
// The float work follows the listing instruction by instruction: the CodeWarrior compiler fused the products and
// sums that ppc_math.h's ppc_fmadds, ppc_fmsubs and ppc_fmsub stand for, and every angle becomes a sine table index
// as fctiwz does (ppc_fctiwz) and is masked with the table's mask.

float gRideHeadsVariable8;
int8_t gRideHeadsVariable8Set;

namespace {

constexpr float kMinusOne = -1.0f;            // 0x10265594
constexpr float kInitialVariable8 = 0.685f;   // 0x1026557c
constexpr float kTwoPi = 6.2831855f;          // 0x10265580
constexpr float kTwoThirdsPi = 2.0943952f;    // 0x10265584
constexpr float kSixthPi = 0.5235988f;        // 0x10265588
constexpr float kQuarterPi = 0.7853982f;      // 0x1026558c
constexpr float kMinusQuarterPi = -0.7853982f;   // 0x10265590
constexpr float kHalfPi = 1.5707964f;         // 0x102655a8

constexpr int32_t kRows = 7;      // directions are sampled in 7 rings of latitude (pi / 6 apart) ...
constexpr int32_t kColumns = 8;   // ... with 8 columns of longitude (pi / 4 apart) in each
constexpr uint32_t kHeadVariable = 13;
constexpr uint32_t kTurnVariable = 9;
constexpr uint32_t kConstantVariable = 8;

// The sine table entry of an angle already multiplied by the table's scale: the index is truncated as fctiwz does
// and masked. The cosine of an angle is the entry of angle + pi / 2.
float TableEntry(float scaledAngle) {
    const int32_t index = ppc_fctiwz(static_cast<double>(scaledAngle));
    return gTPGlobals.mSinTable[static_cast<uint32_t>(index) & gTPGlobals.mSinTableMask];
}

// The listing's frsp(fmsub(a, b, c * d)): the product of two floats is exact in double, the fused difference is
// rounded to double and then to single. One component of a cross product.
float CrossTerm(float a, float b, float c, float d) {
    const double product = static_cast<double>(c) * static_cast<double>(d);
    return static_cast<float>(ppc_fmsub(a, b, product));
}

// Finds the row and column of the sampled direction with the largest dot product with the unit direction
// (nx, ny, nz); the dot product has to beat 0, and a tie keeps the first. `best` is that sample's direction.
// The sample directions start at (0, 1, 0), turn about the y axis by pi / 4 for each column and about the x axis
// by pi / 6 for each row; the turns go through the sine table. When no sample beats 0 (the direction is all zeros or
// holds a NaN that the NaN test did not catch) the original leaves row, column and `best` as they were (registers
// and stack words of the previous head), so they are passed in by reference and the caller keeps them.
void FindClosestDirection(float nx, float ny, float nz, int32_t& row, int32_t& column, s3DPoint& best) {
    float vx = 0.0f;
    float vy = 1.0f;
    float vz = 0.0f;
    float bestDot = 0.0f;
    for (int32_t r = 0; r < kRows; ++r) {
        for (int32_t c = 0; c < kColumns; ++c) {
            float dot = vy * ny;
            dot = ppc_fmadds(vx, nx, dot);
            dot = ppc_fmadds(vz, nz, dot);
            if (bestDot < dot) {
                bestDot = dot;
                row = r;
                column = c;
                best.x = vx;
                best.y = vy;
                best.z = vz;
            }
            // Turn (vx, vz) about the y axis by pi / 4: sin(-pi / 4) is `minus`, sin(pi / 4) is `plus`.
            const float scale = gTPGlobals.mSinTableScale;
            const float minus = TableEntry(kMinusQuarterPi * scale);
            const float plus = TableEntry(kQuarterPi * scale);
            const float newX = ppc_fmadds(vx, plus, vz * minus);
            const float newZ = ppc_fmadds(-vx, minus, vz * plus);
            vx = newX;
            vz = newZ;
        }
        // Turn (vy, vz) about the x axis by pi / 6: sin(pi / 6) is `sine`, sin(2 pi / 3) is `cosine` (= cos(pi / 6)).
        const float scale = gTPGlobals.mSinTableScale;
        const float sine = TableEntry(kSixthPi * scale);
        const float cosine = TableEntry(kTwoThirdsPi * scale);
        const float newY = ppc_fmsubs(vy, cosine, vz * sine);
        const float newZ = ppc_fmadds(vy, sine, vz * cosine);
        vy = newY;
        vz = newZ;
    }
}

// The matrix that looks along the sample (row, column) with `best` as its third axis: row 1 is the "forward" vector
// worked out from the two angles (pi / 6 * row about x, -pi / 4 * column about y, from the start (0, 0, -1)), row 0
// forward x best and row 2 best.
void BuildLookMatrix(int32_t row, int32_t column, const s3DPoint& best, sMatrix* look) {
    const float scale = gTPGlobals.mSinTableScale;
    const float rowAngle = kSixthPi * static_cast<float>(row);
    const float rowSine = TableEntry(rowAngle * scale);
    const float rowCosine = TableEntry((kHalfPi + rowAngle) * scale);
    float forwardX = 0.0f;
    float forwardY = ppc_fmsubs(0.0f, rowCosine, kMinusOne * rowSine);
    float forwardZ = ppc_fmadds(0.0f, rowSine, kMinusOne * rowCosine);
    const float columnAngle = kMinusQuarterPi * static_cast<float>(column);
    const float columnSine = TableEntry(columnAngle * scale);
    const float columnCosine = TableEntry((kHalfPi + columnAngle) * scale);
    const float oldX = forwardX;
    const float oldZ = forwardZ;
    forwardX = ppc_fmadds(oldX, columnCosine, oldZ * columnSine);
    forwardZ = ppc_fmadds(-oldX, columnSine, oldZ * columnCosine);
    s3DPoint forward = {forwardX, forwardY, forwardZ};
    s3DPoint side = {CrossTerm(forwardY, best.z, forwardZ, best.y), CrossTerm(forwardZ, best.x, forwardX, best.z),
                     CrossTerm(forwardX, best.y, forwardY, best.x)};
    s3DPoint axis = best;
    SetMatrixIdentity(look);
    SetLMatrixOrientation(look, &side, &forward, &axis);
}

}  // namespace

void TickRideHeads(tag_sMesh*& meshRef) {
    // Where the original keeps the winning sample between heads (registers r30, r31 and stack words): see
    // FindClosestDirection. The caller's values at the first head are not known; 0 here.
    int32_t row = 0;
    int32_t column = 0;
    s3DPoint best = {0.0f, 0.0f, 0.0f};
    if ((static_cast<sEmbeddedSet*>(meshRef->mF28)->mFlags & 4) == 0) return;
    const uint32_t count = meshRef->mHeader->mEmbeddedCount;
    for (uint32_t i = 0; i < count; ++i) {
        tag_sMesh* mesh = meshRef;
        sEmbeddedRecord* record = &static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords[i];
        if ((record->mFlags & 2) == 0) continue;
        tag_sMiniNode* node = EmbeddedNode(mesh->mHeader, i);
        float* block = reinterpret_cast<float*>(record->mBlock);
        // A mirrored node flips the head's x axis and z axis rows (the block is changed every tick).
        if ((node->mFlags & 0x400) != 0) {
            block[0] = block[0] * kMinusOne;
            block[1] = block[1] * kMinusOne;
            block[2] = block[2] * kMinusOne;
            block[8] = block[8] * kMinusOne;
            block[9] = block[9] * kMinusOne;
            block[10] = block[10] * kMinusOne;
        }
        const int32_t handle = static_cast<int32_t>(record->mF0C);
        if ((node->mFlags & 0x8000) != 0) {
            SCR_SetVariable(handle, kHeadVariable, -1);
            continue;
        }
        SCR_SetScriptPosition(handle, block[12], block[13], block[14], 0);
        // L = the camera's matrix combined with the inverse of the head's: the camera as seen from the head.
        sMatrix toCamera;
        std::memcpy(&toCamera, block, sizeof toCamera);
        InvertMatrix(&toCamera, &toCamera);
        CombineMatricesNoWDest2nd(gCurrentCameraMatrix, &toCamera);
        // The unit direction to the camera. The length is worked out in double precision: x * x fused with the
        // single-rounded y * y, then the single-rounded z * z added.
        const float x = toCamera.m[3][0];
        const float y = toCamera.m[3][1];
        const float z = toCamera.m[3][2];
        const float ySquared = y * y;
        const float zSquared = z * z;
        double sum = ppc_fmadd(static_cast<double>(x), static_cast<double>(x), static_cast<double>(ySquared));
        sum = static_cast<double>(zSquared) + sum;
        const float length = static_cast<float>(std::sqrt(sum));
        const float nx = x / length;
        const float ny = y / length;
        const float nz = z / length;
        if (std::isnan(nx)) {
            // __fpclassifyf answered FP_NAN: the direction is 0 / 0.
            row = 0;
            column = 0;
        } else {
            FindClosestDirection(nx, ny, nz, row, column, best);
        }
        if (row == 0 || row == 6) column = 0;
        sMatrix look;
        BuildLookMatrix(row, column, best, &look);
        sMatrix lookInverse;
        InvertMatrix(&lookInverse, &look);
        CombineMatricesNoWDest2nd(&toCamera, &lookInverse);
        sMatrix turn = lookInverse;
        const float angle = GetLMatrixRotationAroundZAxisYOnly(&turn);
        SCR_SetVariable(handle, kHeadVariable, column + row * 8);
        SCR_SetFloatVariable(handle, kTurnVariable, angle / kTwoPi);
        if (gRideHeadsVariable8Set == 0) {
            gRideHeadsVariable8 = kInitialVariable8;
            gRideHeadsVariable8Set = 1;
        }
        SCR_SetFloatVariable(handle, kConstantVariable, gRideHeadsVariable8);
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

// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.

// ---- globals of the files here (bss, zero at start)
uint32_t gTPEmbedWordA;                 // 0x10396aa0
uint32_t gTPEmbedWordB;                 // 0x10396aa4
// ---- TPEmbed.cpp and the thread helpers
void __sinit_TPEmbed_cpp() {
    gTPEmbedWordB = 0;
    gTPEmbedWordA = 0x10000;
}
