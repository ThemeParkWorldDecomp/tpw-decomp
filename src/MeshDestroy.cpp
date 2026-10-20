// The end of a mesh's life (main app, TPRide.cpp range; unit w75): DestroyEngineMesh 0x10065004 (140 bytes) and DestroyMeshInstance
// 0x100631c4 (92 bytes).
#include "TPEngine.h"
#include "tb_mesh.h"

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

// 0x100631c4. The flag test and DisableMeshDraw run here too, and again inside DestroyEngineMesh (the first call clears the flag in
// the real DisableMeshDraw), then the block the instance lives in is released. The Mac answers 1 (r3; the callers ignore it); the heap
// and the flags are not used.
void DestroyMeshInstance(tag_sMesh* mesh, sHeapInfo*, uint32_t) {
    if ((mesh->mF00 & 1) != 0) DisableMeshDraw(mesh);
    DestroyEngineMesh(mesh);
    MReleaseFreeF(mesh->mAllocation);
}
