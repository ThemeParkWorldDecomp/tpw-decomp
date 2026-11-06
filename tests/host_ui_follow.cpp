// host_ui_follow.cpp: a live shape change must reach every mesh and gradient draw object (the port's fix resslider,
// PortFollowUnits in src/game/ui/InterfaceDrawEngine.cpp). These objects bake a matrix from the unit space when they are
// placed, so a window that does not move (the remote's are anchored left, so their offset is 0) keeps the old shape's
// stretch until something next places it. The mesh, frame, slider and both gradients are the real ones, with GetUIMatrix and
// the real Change; the engine's meshes, textures and the base class are stand-ins. After a change of units each object must
// hold what a new object placed in the new units holds, down to the depth, must have told the engine, and must come back
// to the same bits on the way back. An object without a rectangle or a mesh must be left alone, a destroyed one must not be
// visited, and without the live change nothing is listed. Run by ctest ("ui_follow"); made only with the port's fixes, the
// original's unit space is one size for the game's life. Exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "platform/arena.h"
#include "fake_strings.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "m3d2.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"

int16_t gUIUnitWidth = 2048;
int16_t gUIUnitHeight = 1536;
int32_t gScreenWidth = 1024;
int32_t gScreenHeight = 768;
sHeapInfo* gUIHeap;
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CSystem::CSystem() {}
CSystem gei_System;
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }

namespace {
int gBaseChanges = 0;      // the base class's Change: a destroyed object's calls end here
int gLocalMatrixes = 0;
sMatrix gLastLocal;
CMesh* gLastEngine = nullptr;
}  // namespace

void CMesh::SetLocalMatrix(int32_t, sMatrix& matrix) {
    ++gLocalMatrixes;
    gLastLocal = matrix;
    gLastEngine = this;
}

InterfaceDrawObject::InterfaceDrawObject(void* data) : mData(data) {
    mShown = mFrameBase = mFrame = mFrameCount = mInLibrary = mResizable = 0;
    mF28 = mF2A = mF2C = mF2E = mF30 = mF32 = mDepth = 0;
    mRect.mLeft = mRect.mTop = mRect.mRight = mRect.mBottom = 0;
    mRect2 = mRect;
}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) { ++gBaseChanges; }
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

void EnableMeshDraw(tag_sMesh*, sMatrix*, int32_t) {}
void DisableMeshDraw(tag_sMesh*) {}
void CreateEngineMesh(tag_sMesh*) {}
void DestroyEngineMesh(tag_sMesh*) {}
void DestroyMeshInstance(tag_sMesh*, sHeapInfo*, uint32_t) {}
int32_t InstanceMesh(tag_sMesh*, sHeapInfo*, uint32_t, tag_sMesh**, uint32_t) { return 0; }

namespace {
CMesh gGradientEngine;
tag_sMesh gGradientMesh;
sVertexPosnSet gGradientVertices[16][4];
sUVSet gGradientUVs[16][4];
int gGradientSlot = 0;
}  // namespace

int32_t DMLoadTextures(sDTextureSet*) { return 1; }
void DMFreeTextures(sDTextureSet*) {}
int32_t DMFreeMesh(tag_sDMeshSet*) { return 0; }
int32_t DMEnableMeshRender(tag_sDMeshSet*) { return 0; }
int32_t DMDisableMeshRender(tag_sDMeshSet*) { return 0; }
int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet*, const char*, uint32_t) {
    static sFaceSet faceSets[16][4];
    static sFace faces[16][8];
    static s3DPoint points[16][16];
    static sMatrix matrixes[16];
    const int slot = gGradientSlot++ % 16;
    gGradientMesh.mEngineMesh = &gGradientEngine;
    set->mMesh = &gGradientMesh;
    set->mVertexSets = gGradientVertices[slot];
    set->mUVSets = gGradientUVs[slot];
    set->mFaces = faces[slot];
    set->mEntries->mFaceSets = faceSets[slot];
    set->mPoints = points[slot];
    set->mEntries->mMatrix = &matrixes[slot];
    return 1;
}

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAILED: %s\n", what);
}

bool Same(const sMatrix& a, const sMatrix& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

// A mesh with one node of 16 vertices and a box, as a loaded frame or slider has.
struct MeshData {
    tag_sMesh mesh{};
    CMesh engine;
    tag_sM3D2Header* header;
    tag_sNode* node;

    MeshData() {
        header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
        node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        auto* vertices = static_cast<sVertexPosnSet*>(tpw::ArenaAllocZeroed(4 * sizeof(sVertexPosnSet)));
        node->mVertices = vertices;
        for (int i = 0; i < 16; ++i) {
            vertices[i >> 2].x[i & 3] = (i & 1) != 0 ? 100.0f : 0.0f;
            vertices[i >> 2].y[i & 3] = (i & 2) != 0 ? -100.0f : 0.0f;
        }
        node->mMatrix.m[0][0] = node->mMatrix.m[1][1] = node->mMatrix.m[2][2] = node->mMatrix.m[3][3] = 1.0f;
        node->mBox.mMin[1] = -100.0f;
        node->mBox.mMax[0] = 100.0f;
        node->mBox.mMax[2] = 1.0f;
        header->mNodeCount = 1;
        header->mF78 = node;
        mesh.mHeader = header;
        mesh.mEngineMesh = &engine;
    }
};

constexpr uint32_t kFrame = 0x1, kDepth = 0x2, kRect = 0x10;

// Where a window would put it: the same rectangle whatever the units (the remote's panels are anchored to the left).
void Place(InterfaceDrawObject& object, int16_t frame = 0) {
    CoolRect rect;
    rect.mLeft = 40;
    rect.mTop = 1100;
    rect.mRight = 640;
    rect.mBottom = 1500;
    DOChange change{};
    change.mFrame = frame;
    change.mDepth = 8;
    change.mRect = &rect;
    object.Change(kFrame | kDepth | kRect, &change);
}

void SetUnits(int16_t wide, int16_t high = 1536) {
    gUIUnitWidth = wide;
    gUIUnitHeight = high;
}

// One kind of object: the matrix goes stale, follows to what a new one holds, tells the engine, keeps its depth, and comes back.
template <class Object, class Make, class Engine>
void Follows(const char* name, Make make, Engine engineOf) {
    char what[128];
    const auto say = [&](const char* text) {
        std::snprintf(what, sizeof what, "%s: %s", name, text);
        return what;
    };
    const int16_t pairs[][2] = {{2048, 2457}, {2048, 2730}, {2457, 2730}, {2457, 2048}, {2730, 2048}, {2730, 2457}};
    for (const auto& pair : pairs) {
        SetUnits(pair[0]);
        Object* object = make();
        Place(*object);
        const sMatrix before = object->mMatrix;
        Check(before.m[3][2] == 1.0f - 8.0f / 40.0f, say("the depth is the one it was placed at"));

        SetUnits(pair[1]);
        Object* fresh = make();
        Place(*fresh);
        Check(!Same(fresh->mMatrix, before), say("a new one differs at the new units (else the test proves nothing)"));
        Check(Same(object->mMatrix, before), say("nothing moves before the follow"));

        gLocalMatrixes = 0;
        gLastEngine = nullptr;
        const sMatrix wanted = fresh->mMatrix;
        delete fresh;
        PortFollowUnits();
        Check(Same(object->mMatrix, wanted), say("after the follow it holds what a new one holds"));
        Check(object->mMatrix.m[3][2] == before.m[3][2], say("the depth stays"));
        Check(gLocalMatrixes >= 1, say("the engine is told"));
        Check(gLastEngine == engineOf(*object), say("it is this object's engine mesh that is told"));
        Check(Same(gLastLocal, object->mMatrix), say("the engine has the new matrix"));
        Check(object->mRect.mLeft == 40 && object->mRect.mTop == 1100 && object->mRect.mRight == 640 && object->mRect.mBottom == 1500,
              say("the rectangle stays"));
        Check(object->mRect2.mLeft == 40 && object->mRect2.mRight == 640, say("the second rectangle stays"));

        SetUnits(pair[0]);
        PortFollowUnits();
        Check(Same(object->mMatrix, before), say("the way back gives the same bits"));
        delete object;
    }
}

template <class Object>
void Destroyed(const char* what, void* data) {
    SetUnits(2048);
    alignas(Object) static unsigned char storage[sizeof(Object)];
    auto* object = new (storage) Object(data);
    Place(*object);
    object->~Object();
    gBaseChanges = 0;
    gLocalMatrixes = 0;
    SetUnits(2730);
    PortFollowUnits();
    Check(gBaseChanges == 0 && gLocalMatrixes == 0, what);
}

CMesh* EngineOfMesh(InterfaceDrawEngineMesh& object) { return static_cast<tag_sMesh*>(object.mData)->mEngineMesh; }

template <class Object>
Object* MakeMesh() {
    auto* data = new MeshData;   // (kept: the objects' nodes are its; the test ends soon)
    return new Object(&data->mesh);
}

}  // namespace

int main() {
    gPortLiveResolution = true;

    Follows<InterfaceDrawEngineMesh>("mesh", [] { return MakeMesh<InterfaceDrawEngineMesh>(); }, EngineOfMesh);
    Follows<InterfaceDrawEngineResizableFrame>("frame", [] { return MakeMesh<InterfaceDrawEngineResizableFrame>(); }, EngineOfMesh);
    Follows<InterfaceDrawEngineResizableSlider>("slider", [] { return MakeMesh<InterfaceDrawEngineResizableSlider>(); }, EngineOfMesh);
    static char kName[] = "meter.wct";
    const auto gradientEngine = [](auto&) { return &gGradientEngine; };
    Follows<InterfaceDrawEngineVGradient>("vertical gradient", [] { return new InterfaceDrawEngineVGradient(kName); }, gradientEngine);
    Follows<InterfaceDrawEngineHGradient>("horizontal gradient", [] { return new InterfaceDrawEngineHGradient(kName); }, gradientEngine);

    // A frame or a slider the windows have not placed (the library's own, whose instances the windows use). The slider's Change
    // would stretch its vertices to nothing, since it needs no frame chosen.
    {
        SetUnits(2048);
        auto* data = new MeshData;
        InterfaceDrawEngineResizableSlider slider(&data->mesh);
        const float vertexY = data->node->mVertices[0].y[2];
        const float boxY = data->node->mBox.mMin[1];
        auto* frameData = new MeshData;
        InterfaceDrawEngineResizableFrame frame(&frameData->mesh);
        DOChange choose{};
        frame.Change(kFrame, &choose);
        gLocalMatrixes = 0;
        SetUnits(2730);
        PortFollowUnits();
        Check(data->node->mVertices[0].y[2] == vertexY, "a slider with no rectangle keeps its vertices");
        Check(data->node->mBox.mMin[1] == boxY, "a slider with no rectangle keeps its box");
        Check(gLocalMatrixes == 0, "an object with no rectangle tells the engine nothing");
    }

    // A window's frame is a hover or a page: the follow must keep the one chosen, and its box is the one placed.
    {
        SetUnits(2048);
        auto* data = new MeshData;
        auto* second = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        second->mMatrix = data->node->mMatrix;
        second->mBox.mMin[1] = -50.0f;
        second->mBox.mMax[0] = 300.0f;
        second->mBox.mMax[2] = 1.0f;
        data->node->mF0C = second;
        data->header->mNodeCount = 2;
        InterfaceDrawEngineMesh mesh(&data->mesh);
        Place(mesh, 1);
        SetUnits(2730);
        PortFollowUnits();
        InterfaceDrawEngineMesh fresh(&data->mesh);
        Place(fresh, 1);
        Check(Same(mesh.mMatrix, fresh.mMatrix), "the second frame is placed from its own box");
        Check(reinterpret_cast<tag_sNode*>(mesh.mNodeFlags) == second, "the follow keeps the frame chosen");
    }

    // A slider without a mesh would read through nothing, and a mesh without one must not be told anything.
    {
        SetUnits(2048);
        InterfaceDrawEngineResizableSlider slider(nullptr);
        slider.mRect.mLeft = 10;
        slider.mRect.mRight = 20;
        slider.mRect.mTop = 10;
        slider.mRect.mBottom = 20;
        gLocalMatrixes = 0;
        SetUnits(2730);
        PortFollowUnits();
        Check(gLocalMatrixes == 0, "an object without a mesh is passed over");
    }

    // A destroyed object is not visited: in storage of the test's own, it keeps the base class's table and a rectangle.
    Destroyed<InterfaceDrawEngineMesh>("a destroyed mesh is not visited", &(new MeshData)->mesh);
    Destroyed<InterfaceDrawEngineVGradient>("a destroyed vertical gradient is not visited", kName);
    Destroyed<InterfaceDrawEngineHGradient>("a destroyed horizontal gradient is not visited", kName);

    // Without the live change (the original's way) nothing is listed, so no allocation shows in the differential tests.
    {
        gPortLiveResolution = false;
        SetUnits(2048);
        auto* mesh = MakeMesh<InterfaceDrawEngineMesh>();
        Place(*mesh);
        const sMatrix before = mesh->mMatrix;
        SetUnits(2730);
        PortFollowUnits();
        Check(Same(mesh->mMatrix, before), "without the live change an object is not followed");
        delete mesh;
    }

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures != 0;
}
