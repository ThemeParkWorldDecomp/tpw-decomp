// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)
#include "TPEngine.h"
#include "m3d2.h"
#include "tb_mesh.h"

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

// 0x1005e9f4.
void DestroyRideHeightMap(tag_sRideAnimSet* set) {
    MReleaseFreeF(set->mHeightMap);
    set->mHeightMap = nullptr;
}

// 0x1006fbd8.
void DestroySkirtVertices(tag_sRideAnimSet* set) {
    MReleaseFreeF(set->mHeads->mSkirtVertices);
    set->mHeads->mSkirtVertices = nullptr;
}

// 0x10064d7c.
void DestroyMeshHeader(tag_sM3D2Header* header) { MReleaseFreeF(header->mBlock.get()); }

// 0x10064de0. The type's engine mesh is closed and deleted (its virtual destructor with the delete flag) only when the mesh has no engine
// meshes counted on it.
void DestroyMesh(tag_sMesh* mesh) {
    if ((mesh->mF00 & 1) != 0) DisableMeshDraw(mesh);
    if (mesh->mEngineMesh != nullptr) DestroyEngineMesh(mesh);
    if (mesh->mTypeEngineMesh != nullptr && mesh->mEngineMeshes == 0) {
        mesh->mTypeEngineMesh->Close();
        if (mesh->mTypeEngineMesh != nullptr) delete mesh->mTypeEngineMesh;
    }
    tag_sNode* node = mesh->mHeader->mNodes.get();
    for (uint32_t left = mesh->mHeader->mNodeCount; left != 0; --left, ++node) {
        if (node != mesh->mRootNode && (node->mFlags & 0x1000) != 0) MReleaseFreeF(node->mF98.get());
    }
    MReleaseFreeF(mesh->mBlock14);
    mesh->mBlock14 = nullptr;
    TPDestroyEmbeddedData(mesh);
    MReleaseFreeF(mesh->mHeader->mBlock.get());
    mesh->mHeader = nullptr;
    if (mesh != nullptr) MReleaseFreeF(mesh);
}

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

// 0x10063338. An instance's teardown: the dynamic mesh set's mesh (DestroyMeshInstance's body inline; no texture names are freed here,
// unlike DMFreeMesh), the animations' blocks (+0x44), the set's mesh and the block the instance lives in. QUIRK: the mesh is taken out
// of the draw lists twice (over the map range when the set has flag 0x4000, then once more plainly). Answers 1 (the callers ignore it).
void DestroyRideAnimSetInstance(tag_sRideAnimSet* set, sHeapInfo*, uint32_t) {
    tag_sDMeshSet* dynamicSet = set->mDynamicMeshSet;
    if (dynamicSet != nullptr) {
        if ((dynamicSet->mF00 & 0x4000) != 0) DMDisableMeshRender(dynamicSet, set->mFC0, set->mFC4);
        tag_sDMeshSet* current = set->mDynamicMeshSet;
        tag_sMesh* dynamicMesh = current->mMesh;
        if ((dynamicMesh->mF00 & 1) != 0) DisableMeshDraw(dynamicMesh);
        DestroyEngineMesh(dynamicMesh);
        MReleaseFreeF(dynamicMesh->mAllocation);
        --current->mShared->mInstances;
        MReleaseFreeF(current->mAllocation);
        set->mDynamicMeshSet = nullptr;
    }
    for (uint32_t i = 0; i < set->mAnimationCount; ++i) {
        tag_sAnimHeader* animation = set->mAnimations[i];
        if (animation != nullptr) MReleaseFreeF(animation->mF44.get());
    }
    tag_sMesh* mesh = set->mHeads;
    if ((mesh->mF00 & 1) != 0) {
        if ((set->mFlags & 0x4000) == 0) {
            DisableMeshDraw(mesh);
        } else {
            DisableMeshDraw(mesh, set->mFC0, set->mFC4);
        }
    }
    mesh = set->mHeads;
    if ((mesh->mF00 & 1) != 0) DisableMeshDraw(mesh);
    DestroyEngineMesh(mesh);
    MReleaseFreeF(mesh->mAllocation);
    MReleaseFreeF(set->mInstanceSelf);
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
