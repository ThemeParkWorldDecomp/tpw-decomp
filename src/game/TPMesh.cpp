// TPMesh.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "CBalanceTable.h"
#include "Clock.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "keytable_actions.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "OptionsBalance.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Quickload.h"
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
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "Wavelet.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

// The engine start-up glue of the main app (0x10059c70..0x10068d90 in part; the inventory
// files the range under TPRide.cpp, and the original file name is unknown): the options
// table and the options dialog turned into engine flags, the window, the OpenGL context and
// the engine's start, the engine's Mac* hooks, the per-frame engine calls, the memory hooks,
// the procedural texturing switches and the scratch geometry buffer.
//
// The rest of the range (heightfield, ride height maps, hoardings, instancing, meshes,
// quickload) is not translated yet.

int32_t gMemorySystemInitialised;
// 0x10064bd4
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    if (size == 0) return nullptr;
    if ((flags & 1) != 0) return NS_MacDoze::GlobalAlloc(0x40, size);   // GMEM_ZEROINIT
    return NS_MacDoze::GlobalAlloc(0, size);
}

// 0x10064c54
void* MReleaseFreeF(void* block) {
    if (block == nullptr) return nullptr;
    return NS_MacDoze::GlobalFree(static_cast<NS_MacDoze::InternalHandle*>(block));
}

// 0x10064cb0
void InitialiseMemorySystem() { gMemorySystemInitialised = 1; }

// Loading meshes (main program of SimTheme Park 2.04b3): LoadMesh, CreateMeshTypeEngineMesh
// and the mesh paths (0x100b7d04..0x100b9140, which the inventory files under rsse.cpp),
// CreateMeshFromM3D2Header and CreateEngineMesh (0x10064cf0, 0x10064f40, TPRide.cpp there).

tag_sMesh* CreateMeshFromM3D2Header(tag_sM3D2Header* header) {
    if (header == nullptr) return nullptr;
    auto* mesh = static_cast<tag_sMesh*>(MReleaseAllocF(sizeof(tag_sMesh), 1));
    mesh->mF00 = 0;
    mesh->mHeader = header;
    return mesh;
}

// Ride and mesh teardown (main app, unit w105): the destroyers the lobby's leaving reaches. DestroyRideType 0x1006a768 (608 bytes),
// DestroyRideAnimSet 0x1006a5e8 (324), DestroyRideAnimSetInstance 0x10063338 (368), DestroyMesh 0x10064de0 (308), DestroyMeshHeader
// 0x10064d7c (40), DestroyRideHeightMap 0x1005e9f4 (60), DestroySkirtVertices 0x1006fbd8 (68, Debug.cpp on the Mac), DMFreeMesh
// 0x10030ae8 (240) and the two DMDisableMeshRender 0x100309c8 (68) and 0x10030a48 (84) (TPAdvisor.cpp on the Mac).
// (DestroyMeshInstance 0x100631c4 is in MeshDestroy.cpp.)

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

void CreateEngineMesh(tag_sMesh* mesh) {
    CMesh* engine = new CMesh;
    engine->Init(mesh->mHeader);
    if (mesh->mF08 == nullptr) ++mesh->mEngineMeshes;
    else ++mesh->mF08->mEngineMeshes;
    mesh->mEngineMesh = engine;
}

// The end of a mesh's life (main app, TPRide.cpp range; unit w75): DestroyEngineMesh 0x10065004 (140 bytes) and DestroyMeshInstance
// 0x100631c4 (92 bytes).

// 0x10065004. A drawn mesh (flag 1) is first taken out of the map lists. The count of engine meshes goes down on mF08 when it is set,
// else on the mesh itself (it is not cleared). The engine mesh is deleted through its virtual destructor with the delete flag, and the
// pointer is cleared (also when there was none).
void DestroyEngineMesh(tag_sMesh* mesh) {
    if ((mesh->mF00 & 1) != 0) DisableMeshDraw(mesh);
    tag_sMesh* counted = mesh;
    if (mesh->mF08 != nullptr) counted = mesh->mF08;
    --counted->mEngineMeshes;
    if (mesh->mEngineMesh != nullptr) delete mesh->mEngineMesh;
    mesh->mEngineMesh = nullptr;
}

namespace {
CMapLinkBase* LinkBase(tag_sMesh* mesh) {
    return mesh->mEngineMesh ? static_cast<CMapLinkBase*>(static_cast<CMesh*>(mesh->mEngineMesh)) : nullptr;
}
}

void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    gei_MapWho->Link(from.mF02, from.mF00, to.mF02, to.mF00, LinkBase(mesh), 0x70);
    mesh->mF00 |= 1;
}
void EnableMeshDraw(tag_sMesh* mesh) {
    gei_MapWho->Link(LinkBase(mesh), 0x70);
    mesh->mF00 |= 1;
}
void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t mode) {
    static_cast<CMesh*>(mesh->mEngineMesh)->SetLocalMatrix(1, *matrix);
    gei_MapWho->Link(LinkBase(mesh), mode == 0 ? 0x20 : 0x10);
    mesh->mF00 |= mode == 0 ? 9 : 0x11;
}

// The engine mesh draw objects' instancing and changes (main app of SimTheme Park 2.04b3, the ui_utils.cpp range 0x10187a28..0x1018a1b0, unit
// w42): InterfaceDrawEngineMesh::InstanceMe, CalculateFrame and Change; the two gradients' Change (InterfaceDrawEngineVGradient 0x101895e0,
// InterfaceDrawEngineHGradient 0x10189ea0) with the horizontal one's CalcVerts; and UpdateMeshLocalMatrix (0x100652f4, TPRide.cpp), which
// the gradients' Change calls. GetUIMatrix and CombineMatricesNoWDest1st are in src/ui_matrix.cpp. The constructors, destructors and Show are
// in src/ui_DrawObject.cpp. See include/ui_DrawObject.h.
//
// Change flags (the DOChange fields): 1 mFrame, 2 mDepth, 4 mF08 (the z rotation), 8 the first word (the gradients' value), 0x10 mRect, 0x40 mF0C
// and mF0E (the pivot), 0x80000 mFrameBase. 0x55 (1, 4, 0x10, 0x40) are the ones that place the object.

// ---- the gradients

// The engine mesh of the mesh gets the matrix as its local matrix.
void UpdateMeshLocalMatrix(tag_sMesh* mesh, sMatrix* matrix) { mesh->mEngineMesh->SetLocalMatrix(1, *matrix); }

// The ride animation tick of the main app (SimTheme Park 2.04b3): what moves a ride set's time
// controls on and applies the animations they select to the nodes of the set's mesh. Eight
// functions of three ranges of the inventory (rsse.cpp, TPRide.cpp, TPEmbed.cpp):
//   TickRideAnimSet 0x100c6a20, DoAnimateRideSet 0x100c6c2c, RAAnimateTextureFrames 0x100c3678,
//   AnimateNode 0x100c45d4, GetAnimFrameData 0x100c34bc, IsMeshDrawEnabled 0x10065364,
//   TPProcessEmbeddedData 0x101ffb0c and DoWorldTransformsForEmbeddedData 0x101fef24.
// AnimateRideSet (src/RideTick.cpp) calls TickRideAnimSet, DoAnimateRideSet and
// TPProcessEmbeddedData; InstanceRide (src/RideTypes.cpp) calls the last as well.

// 0x10065364, 12 bytes. Bit 0 of the mesh's first word: the mesh is linked into the map.
int32_t IsMeshDrawEnabled(tag_sMesh* mesh) { return mesh->mF00 & 1; }

void DisableMeshDraw(tag_sMesh* mesh) {
    const int32_t list = (mesh->mF00 & 0x10) != 0 ? 0x10 : (mesh->mF00 & 8) != 0 ? 0x20 : 0x70;
    gei_MapWho->Unlink(LinkBase(mesh), list);
    mesh->mF00 &= 0xffffffe6;
}
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    gei_MapWho->Unlink(from.mF02, from.mF00, to.mF02, to.mF00, LinkBase(mesh));
    mesh->mF00 &= ~1u;
}

// The small functions the advisor calls (main app of SimTheme Park 2.04b3), each from a range of its own:
//   GetDeferredAnim 0x100c7a74 and GetAnimLength 0x100c7bb4 (rsse.cpp, next to the animation tick),
//   SetMeshInUISpace 0x1006553c (TPRide.cpp, the mesh draw switches),
//   FindDataNodeIndexByID 0x101ffdc8 and FindDataNodeIndexByIDAdvisorTempHack 0x101fff90 (TPEmbed.cpp),
//   FindDataNodeIndexByDisableCode 0x1020017c (TPEmbed.cpp, unit v5L21).
// They are here and not in the files of their neighbours so that the advisor's unit library links without the rest of
// those files. (ScaleLMatrixNoTrans is in src/rsse_matrix.cpp and SND_DuckVolume in src/TPSound.cpp.)

void SetMeshInUISpace(tag_sMesh*& mesh) { mesh->mHeader->mFlags |= 0x200; }

// 0x10065588
void SetMeshPosition(tag_sMesh* mesh, float x, float y, float z) {
    sMatrix* matrix = &mesh->mHeader->mF78.get()->mMatrix;
    matrix->m[3][0] = x;
    matrix->m[3][1] = y;
    matrix->m[3][2] = z;
}

// 0x100655d8
void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale) {
    ScaleLMatrixNoTrans(&mesh->mHeader->mF78.get()->mMatrix, scale->x, scale->y, scale->z);
}
