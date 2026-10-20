#include "TPEngine.h"
#include "tb_mesh.h"

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
void DisableMeshDraw(tag_sMesh* mesh) {
    const int32_t list = (mesh->mF00 & 0x10) != 0 ? 0x10 : (mesh->mF00 & 8) != 0 ? 0x20 : 0x70;
    gei_MapWho->Unlink(LinkBase(mesh), list);
    mesh->mF00 &= 0xffffffe6;
}
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    gei_MapWho->Unlink(from.mF02, from.mF00, to.mF02, to.mF00, LinkBase(mesh));
    mesh->mF00 &= ~1u;
}
