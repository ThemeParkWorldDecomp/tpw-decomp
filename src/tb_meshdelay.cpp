// tb_meshdelay.cpp: the delayed meshes (include/tb_meshdelay.h).
#include "tb_meshdelay.h"

#include "tb_mesh.h"

CMeshDelay gMeshDelay;

void CMeshDelay::Reset() { mCount = 0; }

int32_t CMeshDelay::Prepare() {
    for (int32_t i = 0; i < mCount; i++) {
        mMeshes[i]->mFlags |= 0x10;
        mMeshes[i]->Prepare();   // virtual (CMesh's vtable slot 0x14)
    }
    return 1;
}
