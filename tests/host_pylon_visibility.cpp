// The real ground gamut can omit a tall support that intersects the ride camera.
// Check global mesh registration, removal, hidden supports and restored braces.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "m3d2.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "platform/arena.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "tb_clipping.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"


namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, double value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%g)\n", what, value);
}

alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
sHField gRecord;
CMatrix gView;
CMatrix gFinal;
std::vector<float> gFarPlanes;   // what each AdjustProjection was given

constexpr float kNear = 1.0f;

// The stand-in math unit: the camera's axes are the view matrix's rows 0 (right), 1 (up) and 2 (forward), its position
// row 3; a point d ahead comes out with w = d and z = far (d - near) / (far - near), so the clipper finds it inside
// from the near plane out to the far one, and x and y inside a quarter turn either way.
class StandInMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count, int32_t) override {
        const float far = gFarPlanes.empty() ? 1000.0f : gFarPlanes.back();
        for (int32_t i = 0; i < count; i++) {
            const float* p = reinterpret_cast<const float*>(static_cast<const uint8_t*>(in) + i * inStride);
            float* o = reinterpret_cast<float*>(static_cast<uint8_t*>(out) + i * outStride);
            const float rx = p[0] - gView.m[3][0];
            const float ry = p[1] - gView.m[3][1];
            const float rz = p[2] - gView.m[3][2];
            const float d = rx * gView.m[2][0] + ry * gView.m[2][1] + rz * gView.m[2][2];
            o[0] = rx * gView.m[0][0] + ry * gView.m[0][1] + rz * gView.m[0][2];
            o[1] = rx * gView.m[1][0] + ry * gView.m[1][1] + rz * gView.m[1][2];
            o[2] = far * (d - kNear) / (far - kNear);
            o[3] = d;
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
StandInMath gMath;

}  // namespace

// ---- the engine objects the unit reads
CDisplay gei_Display;
CMatrix gei_ProjectionMatrix;
CMatrix* gei_ViewMatrix = &gView;
CMatrix* gei_FinalMatrix = &gFinal;
CTransformBase* gei_MathUnit = &gMath;
CClipping* gei_Clipping = nullptr;
CDisplay::CDisplay() {}
CTransformBase::~CTransformBase() {}

float gProjectionFOV = 60.0f;

// ---- the stand-ins
void CMatrix::AdjustProjection(float farPlane) { gFarPlanes.push_back(farPlane); }
// The view matrix's turn (type 3) of a corner ray: by rows 0, 1 and 2, as the stand-in math unit's camera.
void CMatrix::TransformVertex(void* out, void* in, int32_t) {
    const float* v = static_cast<const float*>(in);
    float* o = static_cast<float*>(out);
    for (int32_t i = 0; i < 3; i++) o[i] = v[0] * m[0][i] + v[1] * m[1][i] + v[2] * m[2][i];
}
int32_t PreprocessMatrices() { return 1; }
void msl_assertion_failed(const char*, const char*, int32_t) {}

namespace {

constexpr float kPi = 3.14159265f;

// A map of `columns` by `rows` cells of `cell`, heights low..high, origin at 0, 0.
void MakeMap(uint32_t columns, uint32_t rows, float cell, float low, float high) {
    std::memset(&gRecord, 0, sizeof gRecord);
    gRecord.mF08[0] = 0.0f;
    gRecord.mF08[1] = 0.0f;
    gRecord.mF08[2] = cell;
    gRecord.mF08[3] = cell;
    gRecord.mF18[0] = columns;
    gRecord.mF18[1] = rows;
    gRecord.mF20[0] = low;
    gRecord.mF20[1] = high;
}

// The camera at x, y, z, turned `heading` (radians from +z toward +x) and tilted `pitch` (radians, below 0 down).
void SetCamera(float x, float y, float z, float heading, float pitch) {
    std::memset(&gView, 0, sizeof gView);
    const float fx = std::sin(heading) * std::cos(pitch);
    const float fy = std::sin(pitch);
    const float fz = std::cos(heading) * std::cos(pitch);
    const float rx = std::cos(heading);
    const float rz = -std::sin(heading);
    gView.m[0][0] = rx;
    gView.m[0][2] = rz;
    gView.m[1][0] = fy * rz;   // up = forward x right, with right horizontal
    gView.m[1][1] = fz * rx - fx * rz;
    gView.m[1][2] = -fy * rx;
    gView.m[2][0] = fx;
    gView.m[2][1] = fy;
    gView.m[2][2] = fz;
    gView.m[3][0] = x;
    gView.m[3][1] = y;
    gView.m[3][2] = z;
    gView.m[3][3] = 1.0f;
}

struct Result {
    int32_t first, end;          // the rows of cells in view
    int32_t farthestRow;         // the farthest row that has a cell in view
    bool cornerInView;           // the map's far corner cell (last column, last row)
    float testFar;               // the far plane the boxes were tested against
    float drawFar;               // the far plane the projection got at the end
};

Result Run(uint32_t displayFlags, float viewDistance) {
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0, sizeof field);
    field.mF00 = &gRecord;
    field.mInvCellX = 1.0f / gRecord.mF08[2];
    field.mInvCellZ = 1.0f / gRecord.mF08[3];
    std::memset(static_cast<void*>(&gei_Display), 0, sizeof gei_Display);
    gei_Display.mFlags = displayFlags;
    gei_Display.mFirstPersonViewDistance = viewDistance;
    gFarPlanes.clear();
    Check(field.DivideAndConquer() == 1, "DivideAndConquer's answer");
    const SGamutQuads& cells = *field.QuadRows();
    Result r{cells.mFirstRow, cells.mEndRow, -1, false, 0.0f, 0.0f};
    const int32_t lastColumn = static_cast<int32_t>(gRecord.mF18[0]) - 1;
    const int32_t lastRow = static_cast<int32_t>(gRecord.mF18[1]) - 1;
    for (int32_t row = cells.mFirstRow; row < cells.mEndRow && row < 128; row++) {
        if (row < 0 || cells.mLow[row] >= cells.mHigh[row]) continue;
        r.farthestRow = row;
        if (row == lastRow && cells.mLow[row] <= lastColumn && cells.mHigh[row] > lastColumn) r.cornerInView = true;
    }
    Check(gFarPlanes.size() == 2, "AdjustProjection calls", static_cast<double>(gFarPlanes.size()));
    if (gFarPlanes.size() == 2) {
        r.testFar = gFarPlanes[0];
        r.drawFar = gFarPlanes[1];
    }
    return r;
}


} // namespace

// Mesh drawing is counted, while the real map lists, footprint culling and clip codes run.
int gPrepared=0;
SMapLinkType* gPreparedContext=nullptr;
uint32_t gPreparedFlags=0;
int gLocalLights=0, gGlobalLights=0, gPreparedLocal=0, gPreparedGlobal=0;
int gPrepareResult=1;
CMapWho* gei_MapWho=nullptr;
CHField* gei_HField=&Field();
uint32_t gRenderEntryFlags=0;
CSystem gei_System;
CSystem::CSystem() {}
CMapWho::CMapWho() {}
CMapWho::~CMapWho() {}
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() {
    ++gPrepared;
    gPreparedContext=gei_MapWho->mF30;
    gPreparedFlags=gRenderEntryFlags;
    gLocalLights=gGlobalLights=0;
    // The light-list requests of CMesh::StaticCalculateLights.
    if (gei_MapWho->mF30) gei_MapWho->mF30->PrepareLights();
    if (!(gRenderEntryFlags&0x01000000) && gei_MapWho->mF14[6]) gei_MapWho->mF14[6]->PrepareLights();
    gPreparedLocal=gLocalLights; gPreparedGlobal=gGlobalLights;
    return gPrepareResult;
}
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return mHeader; }
int32_t CHField::Prepare() { return 1; }

class TestLight : public CMapLinkBase {
public:
    explicit TestLight(bool global) : mGlobal(global) {}
    int32_t Prepare() override { if(mGlobal) ++gGlobalLights; else ++gLocalLights; return 1; }
    int32_t GetLinkType() override { return 1; }
    void* GetData() override { return nullptr; }
    bool mGlobal;
};
int main() {
    MakeMap(96, 85, 10.0f, -10.0f, 10.0f);
    // From a high coaster, looking 60 degrees above the horizon at a hill ahead.
    SetCamera(400.0f, 80.0f, 400.0f, 0.0f, 60.0f * kPi / 180.0f);
    Run(CDisplay::kEnableFog, 700.0f);
    auto& cells = *Field().QuadRows();
    const int col = 40, row = 51; // 110..120 ahead; beyond the always-added nearby block.
    const bool ground = row >= cells.mFirstRow && row < cells.mEndRow &&
        cells.mLow[row] <= col && cells.mHigh[row] > col;
    std::printf("ground cell (%d,%d) prepared=%d, ground Y=[-10,10], camera Y=80 pitch=60deg\n", col, row, ground);
    Check(!ground, "footprint omitted although support top is ahead of camera");
    // A support from ground level to Y=150 intersects the view.
    float support[8][3]={{400,0,510},{410,0,510},{400,0,520},{410,0,520},
                         {400,150,510},{410,150,510},{400,150,520},{410,150,520}};
    SVertex transformed[8];
    gMath.Transform(&gFinal,transformed,support,16,12,8,0);
    unsigned char answer[4]={255,0,0,0}, codes[8];
    CClipping::CPP_TestData(answer,nullptr,codes,transformed,8,16,255);
    std::printf("support XYZ bounds clip AND=%u OR=%u (AND=0 means intersects view)\n",answer[0],answer[1]);
    Check(answer[0]==0,"support box intersects frustum while its map cell is omitted");
    // Moving camera toward the hill's crest changes footprint admission, producing a blink.
    SetCamera(400.0f,80.0f,400.0f,0.0f,-5.0f*kPi/180.0f);
    Run(CDisplay::kEnableFog,700.0f);
    auto& lower = *Field().QuadRows();
    const bool ground2 = row >= lower.mFirstRow && row < lower.mEndRow &&
        lower.mLow[row] <= col && lower.mHigh[row] > col;
    std::printf("same footprint at pitch=-5deg prepared=%d\n",ground2);
    Check(ground2,"same support footprint admitted at lower camera pitch");

    CMapWho map;
    std::memset(&map,0,sizeof map);
    gei_MapWho=&map;
    std::vector<SMapLinkType*> heads(96*85,nullptr);
    SMapLinkType links[16]{};
    for(int i=0;i<15;i++) links[i].mNext=&links[i+1];
    map.mMap=heads.data(); map.mWidth=96; map.mHeight=85; map.mFree=links;
    CMesh engine;
    tag_sM3D2Header header{};
    engine.mHeader=&header;
    tag_sMesh mesh{}; mesh.mEngineMesh=&engine;
    auto& set=*static_cast<tag_sRideAnimSet*>(tpw::ArenaAlloc(sizeof(tag_sRideAnimSet)));
    std::memset(&set,0,sizeof set);
    set.mHeads=&mesh; set.mFlags=0x84000; set.mFE4=0x3fa;
    tag_sTrackBrace brace{};
    set.mBraceBlock=&brace; header.mRideSet=&set;
    set.mFC0.mF02=40; set.mFC0.mF00=51; set.mFC4=set.mFC0;
    EnableMeshDraw(&mesh,set.mFC0,set.mFC4);
    SetCamera(400,80,400,0,60.0f*kPi/180.0f);
    Run(CDisplay::kEnableFog,700);
    map.mGamut=Field().QuadRows();
    gPrepared=0;
    Check(map.PrepareWorldspace()==1 && gPrepared==0,"original registration misses the visible support");
#ifdef TPW_TEST_PORT_FIX
    PortDrawCoasterPylon(&set);
    Check((set.mFlags&0x4000)==0 && (set.mFlags&0x80000)!=0,"support uses mesh visibility");
    Check(set.mFE4==0x3fa && set.mFC0.mF02==40 && set.mFC0.mF00==51,"save flags and footprint preserved");
    Check(heads[40+51*96]==nullptr,"old footprint link removed");
    Check(map.mF14[6] && map.mF14[6]->mLink==static_cast<CMapLinkBase*>(&engine),"support globally registered");
    gPrepared=0;
    Check(map.PrepareWorldspace()==1 && gPrepared==1,"visible support prepared with ground outside view");
    const int used=map.mNodesUsed;
    PortDrawCoasterPylon(&set);
    Check(map.mNodesUsed==used,"repeated repair adds no links");
    DisableMeshDraw(&mesh);
    Check(map.mF14[6]==nullptr,"support removal leaves no global link");
    EnableMeshDraw(&mesh);
    gPrepared=0;
    Check(map.PrepareWorldspace()==1 && gPrepared==1,"support can be enabled again");
    DisableMeshDraw(&mesh);
    // A saved park recreates the original footprint link and instance flags, then restores its brace.
    set.mFlags|=0x4000;
    EnableMeshDraw(&mesh,set.mFC0,set.mFC4);
    PortDrawCoasterPylon(&set);
    gPrepared=0;
    Check(map.PrepareWorldspace()==1 && gPrepared==1 && !heads[40+51*96],"restored support takes the same repaired path");
    DisableMeshDraw(&mesh);
    set.mFlags|=0x4000;
    PortDrawCoasterPylon(&set);
    Check(!(mesh.mF00&1) && !map.mF14[6],"a hidden support stays hidden");

    TestLight localLight(false), globalLight(true), secondLight(false);
    SMapLinkType localNode{&localLight,nullptr}, globalNode{&globalLight,nullptr}, secondNode{&secondLight,nullptr};
    heads[40+51*96]=&localNode;
    map.mF14[6]=&globalNode;
    EnableMeshDraw(&mesh);
    Check(map.PrepareWorldspace()==1 && gPreparedContext==&localNode && gPreparedFlags==0,
          "outside-ground support retains footprint light context and render flags");
    Check(gPreparedLocal==1 && gPreparedGlobal==1,"local and global lights prepared once each");
    Check(gRenderEntryFlags==0,"global render flags restored after preparation");
    // With part of a larger footprint visible, use its first admitted cell, as the original did.
    set.mFC0.mF02=39; set.mFC4.mF02=41; set.mFC4.mF00=52;
    SGamutQuads admitted{};
    admitted.mFirstRow=52; admitted.mEndRow=53; admitted.mLow[52]=40; admitted.mHigh[52]=41;
    heads[40+52*96]=&secondNode;
    map.mGamut=&admitted;
    Check(map.PrepareWorldspace()==1 && gPreparedContext==&secondNode,
          "first visible footprint cell supplies lights when one is admitted");
    Check(gPreparedLocal==1 && gPreparedGlobal==1,"visible footprint's lights prepared once each");
    // Other global meshes retain the original global-list context.
    engine.mPortPylonSet=nullptr;
    Check(map.PrepareWorldspace()==1 && gPreparedContext==&globalNode && gPreparedFlags==0x01000000,
          "ordinary global mesh context unchanged");
    Check(gPreparedLocal==0 && gPreparedGlobal==1,"ordinary global lights are not duplicated");
    engine.mPortPylonSet=&set;
    gPrepareResult=0;
    Check(map.PrepareWorldspace()==0 && map.mF30==&globalNode && gRenderEntryFlags==0x01000000,
          "failure also restores global context");
    gPrepareResult=1;
    DisableMeshDraw(&mesh);
#else
    DisableMeshDraw(&mesh,set.mFC0,set.mFC4);
    Check(heads[40+51*96]==nullptr,"original footprint removal");
#endif
    std::printf("%d checks, %d failed\n",gChecks,gFailures);
    tpw::ArenaFree(&set);
    return gFailures!=0;
}
