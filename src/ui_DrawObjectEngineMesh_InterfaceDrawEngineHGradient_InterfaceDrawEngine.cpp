// The horizontal gradient's constructor (0x101898c8, main app): a quad of two halves made as a dynamic mesh set of its own. Its neighbour is
// src/ui_DrawObjectEngineMesh.cpp (Change, CalcVerts).
#include <cstdint>
#include <cstring>

#include "TPEngine.h"
#include "rsse.h"
#include "ui_DrawObject.h"

namespace {
const char kHGradientFolder[] = "data:ui:textures";   // 0x1024daf5
}

// `mesh` is the texture's name. The Mac builds the entry, the texture set and the mesh set in place, loads the textures, makes the mesh and
// fills its quad: 8 vertices (two sets of four), 8 normals (0, 0, -1), 4 faces in 2 triangle pairs, one face set of 4 faces and the UVs.
// mData is 0 until the mesh is made, then 1.
InterfaceDrawEngineHGradient::InterfaceDrawEngineHGradient(void* mesh) : InterfaceDrawObject(mesh) {
    mTexEntry.mFlags = 2;
    mTexEntry.mName = static_cast<char*>(mesh);
    std::memset(static_cast<void*>(&mTextures), 0, sizeof(mTextures));
    mTextures.mFlags = 0;
    mTextures.mCount = 1;
    mTextures.mFolder = kHGradientFolder;
    mTextures.mEntries = &mTexEntry;
    mMeshSet.mF00 = 0x1003;
    mMeshSet.mCount = 1;
    mMeshSet.mEntries = &mEntry;
    mMeshSet.mMatrix = &mMatrix;
    mEntry.mFlags = 3;
    mEntry.mFaceCount = 4;
    mEntry.mVertexCount = 8;
    mEntry.mUVCount = 8;
    mEntry.mFaceSetCount = 1;
    mEntry.mPointCount = 8;
    mData = nullptr;
    mBox[0] = 0.0f;
    mBox[1] = 0.0f;
    mBox[2] = 0.0f;
    mBox[3] = 100.0f;
    mBox[4] = 100.0f;
    mBox[5] = 0.0f;
    for (int i = 0; i < 6; ++i) mEntry.mBox[i] = mBox[i];
    if (DMLoadTextures(&mTextures) == 0) return;
    if (DMCreateMesh(&mMeshSet, &mTextures, static_cast<const char*>(mesh), 1) == 0) {
        DMFreeTextures(&mTextures);
        return;
    }
    SetMatrixIdentity(mEntry.mMatrix);
    sVertexPosnSet* v = mMeshSet.mVertexSets;
    v[0].x[0] = 0.0f;
    v[0].y[0] = 100.0f;
    v[0].z[0] = 0.0f;
    v = mMeshSet.mVertexSets;
    v[1].x[2] = 100.0f;
    v[1].y[2] = 100.0f;
    v[1].z[2] = 0.0f;
    mFraction = 0.0f;
    CalcVerts();
    v = mMeshSet.mVertexSets;
    v[1].x[3] = 100.0f;
    v[1].y[3] = 0.0f;
    v[1].z[3] = 0.0f;
    v = mMeshSet.mVertexSets;
    v[0].x[3] = 0.0f;
    v[0].y[3] = 0.0f;
    v[0].z[3] = 0.0f;
    float* p = reinterpret_cast<float*>(mMeshSet.mPoints);
    for (int i = 0; i < 8; ++i) {
        p = reinterpret_cast<float*>(mMeshSet.mPoints);
        p[i * 3] = 0.0f;
        p[i * 3 + 1] = 0.0f;
        p[i * 3 + 2] = -1.0f;
    }
    sFace* f = mMeshSet.mFaces;
    f[0].mF02[0] = 0;
    f[0].mF02[1] = 1;
    f[0].mF02[2] = 2;
    f[1].mF02[0] = 2;
    f[1].mF02[1] = 3;
    f[1].mF02[2] = 0;
    f[2].mF02[0] = 4;
    f[2].mF02[1] = 6;
    f[2].mF02[2] = 7;
    f[3].mF02[0] = 7;
    f[3].mF02[1] = 5;
    f[3].mF02[2] = 4;
    sFaceSet* fs = mEntry.mFaceSets;
    fs->mTexture = mTextures.mTextures;
    fs->mFirstFace = 0;
    fs->mFaceCount = 4;
    fs->mF08 = 0;
    fs->mF0A = 7;
    sUVSet* uv = mMeshSet.mUVSets;
    uv[0].mF00[0] = 0.0f;
    uv[0].mF00[4] = 1.0f;
    uv[1].mF00[2] = 1.0f;
    uv[1].mF00[6] = 0.5f;
    uv[0].mF00[3] = 0.0f;
    uv[0].mF00[7] = 0.5f;
    uv[1].mF00[3] = 1.0f;
    uv[1].mF00[7] = 0.0f;
    CalcUVs();
    mData = reinterpret_cast<void*>(1);
}

// Lead 2026-10-20 (v51129): the destructor (0x10189c0c, the class's key function, so the vtable comes with it) is translated in
// src/ui_Postcard_panel_std_basic_string_assign_2.cpp.
// Lead 2026-10-20 (v51115): Show (0x10189df8) is translated in src/ui_TextObjects_InterfacePasswordTextObject_InterfacePasswordTex.cpp.
