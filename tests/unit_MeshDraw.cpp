// Fakes for the mesh draw registration unit (src/MeshDraw.cpp): the engine's map lists and
// CMesh::SetLocalMatrix record their arguments; the engine mesh is one static object.
#include "TPEngine.h"
#include "tb_mesh.h"
#include "calllog.h"
#include <cstring>
using namespace calllog;
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }
CMapWho::CMapWho() {}
CMapWho::~CMapWho() {}
namespace {
CMesh engine;
CMapWho map;
int32_t IsEngine(const CMapLinkBase* p) { return p == static_cast<const CMapLinkBase*>(&engine); }
}
CMapWho* gei_MapWho = &map;
void CMesh::SetLocalMatrix(int32_t flags, sMatrix& matrix) {
    uint32_t w[16];
    std::memcpy(w, &matrix, sizeof w);
    ev("SetLocalMatrix", {this == &engine, flags});
    ev("Matrix", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("Matrix", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}
int32_t CMapWho::Link(CMapLinkBase* p, int32_t list) { ev("Link", {IsEngine(p), list}); return 0; }
int32_t CMapWho::Unlink(CMapLinkBase* p, int32_t list) { ev("Unlink", {IsEngine(p), list}); return 0; }
int32_t CMapWho::Link(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* p, int32_t list) {
    ev("LinkRange", {x0, z0, x1, z1, IsEngine(p), list});
    return 0;
}
int32_t CMapWho::Unlink(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* p) {
    ev("UnlinkRange", {x0, z0, x1, z1, IsEngine(p)});
    return 0;
}
// Runs one overload (op 0..4 in the order of the test's ENTRIES) on a tag_sMesh whose first
// word is `flags` and whose engine mesh is the static object or null; returns the first word.
extern "C" uint32_t tpw_md_run(int32_t op, uint32_t flags, int32_t present, uint32_t from, uint32_t to,
                               int32_t mode, const uint32_t* matrixWords) {
    tag_sMesh mesh{};
    mesh.mF00 = flags;
    mesh.mEngineMesh = present ? &engine : nullptr;
    sMatrix matrix;
    std::memcpy(&matrix, matrixWords, sizeof matrix);
    HFieldXZ fromXZ{static_cast<uint16_t>(from >> 16), static_cast<uint16_t>(from)};
    HFieldXZ toXZ{static_cast<uint16_t>(to >> 16), static_cast<uint16_t>(to)};
    switch (op) {
    case 0: EnableMeshDraw(&mesh, fromXZ, toXZ); break;
    case 1: EnableMeshDraw(&mesh); break;
    case 2: EnableMeshDraw(&mesh, &matrix, mode); break;
    case 3: DisableMeshDraw(&mesh); break;
    case 4: DisableMeshDraw(&mesh, fromXZ, toXZ); break;
    }
    return mesh.mF00;
}
