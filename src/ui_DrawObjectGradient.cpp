// The vertical gradient draw object's constructor 0x10188ff8 (792 bytes), destructor 0x1018934c (140), CalcVerts 0x10189414 (92), CalcUVs
// 0x101894b0 (76) and Show 0x10189538 (108) (Mac file ui_utils.cpp; unit w118). Its Change is in src/ui_DrawObjectEngineMesh.cpp (unit w42).
//
// The object is a quad of the interface in a dynamic mesh set of its own: two halves of four vertices (the top (0,100) (100,100) (100,f)
// (0,f) and the bottom (0,f) (100,f) (100,0) (0,0), f being the fraction), eight points, two UV sets and four faces. The Mac builds the
// set, its one entry and its texture set inside the object and lets DMLoadTextures and DMCreateMesh fill in the arrays; the constructor then
// writes the quad. The offsets of the Mac's object are in the header; the stores below keep the listing's order.
#include <cstring>

#include "ui_DrawObject.h"

namespace {
constexpr float kZero = 0.0f;      // 0x10265468
constexpr float kFull = 100.0f;    // 0x1026544c
constexpr float kHalf = 0.5f;      // 0x10265444
constexpr float kOne = 1.0f;       // 0x10265488
constexpr float kBack = -1.0f;     // 0x10265448
const char kTextureFolder[] = "data:ui:textures";   // 0x1024daf5
}  // namespace

InterfaceDrawEngineVGradient::InterfaceDrawEngineVGradient(void* mesh) : InterfaceDrawObject(mesh) {
    mTextureEntry.mFlags = 2;
    mTextureEntry.mName = static_cast<char*>(mesh);
    std::memset(&mTextures, 0, sizeof(mTextures));
    mTextures.mFlags = 0;
    mTextures.mCount = 1;
    mTextures.mFolder = kTextureFolder;
    mTextures.mEntries = &mTextureEntry;
    mMeshSet.mF00 = 0x1003;
    mMeshSet.mCount = 1;
    mMeshSet.mEntries = &mItem;
    mMeshSet.mMatrix = &mMatrix;
    mItem.mFlags = 3;
    mItem.mFaceCount = 4;
    mItem.mVertexCount = 8;
    mItem.mUVCount = 8;
    mItem.mFaceSetCount = 1;
    mItem.mPointCount = 8;
    mData = nullptr;
    // the box, in the entry and in mBox (the Mac has the one)
    mBox[0] = mItem.mBox[0] = kZero;
    mBox[1] = mItem.mBox[1] = kZero;
    mBox[2] = mItem.mBox[2] = kZero;
    mBox[3] = mItem.mBox[3] = kFull;
    mBox[4] = mItem.mBox[4] = kFull;
    mBox[5] = mItem.mBox[5] = kZero;
    if (DMLoadTextures(&mTextures) != 0) {
        if (DMCreateMesh(&mMeshSet, &mTextures, static_cast<const char*>(mesh), 1) == 0) {
            DMFreeTextures(&mTextures);
        } else {
            SetMatrixIdentity(mItem.mMatrix);
            sVertexPosnSet* vertices = mMeshSet.mVertexSets;
            vertices[0].x[0] = kZero;
            vertices[0].y[0] = kFull;
            vertices[0].z[0] = kZero;
            vertices = mMeshSet.mVertexSets;
            vertices[0].x[1] = kFull;
            vertices[0].y[1] = kFull;
            vertices[0].z[1] = kZero;
            mFraction = kZero;
            CalcVerts();
            vertices = mMeshSet.mVertexSets;
            vertices[1].x[2] = kFull;
            vertices[1].y[2] = kZero;
            vertices[1].z[2] = kZero;
            vertices = mMeshSet.mVertexSets;
            vertices[1].x[3] = kZero;
            vertices[1].y[3] = kZero;
            vertices[1].z[3] = kZero;
            // the eight points, all (0, 0, -1)
            for (int i = 0; i < 8; ++i) {
                s3DPoint* points = mMeshSet.mPoints;
                points[i].x = kZero;
                points[i].y = kZero;
                points[i].z = kBack;
            }
            // the faces: (0 1 2), (2 3 0), (4 5 6), (6 7 4); the first word of each is left as it was
            sFace* faces = mMeshSet.mFaces;
            faces[0].mF02[0] = 0;
            faces = mMeshSet.mFaces;
            faces[0].mF02[1] = 1;
            faces = mMeshSet.mFaces;
            faces[0].mF02[2] = 2;
            faces = mMeshSet.mFaces;
            faces[1].mF02[0] = 2;
            faces = mMeshSet.mFaces;
            faces[1].mF02[1] = 3;
            faces = mMeshSet.mFaces;
            faces[1].mF02[2] = 0;
            faces = mMeshSet.mFaces;
            faces[2].mF02[0] = 4;
            faces = mMeshSet.mFaces;
            faces[2].mF02[1] = 5;
            faces = mMeshSet.mFaces;
            faces[2].mF02[2] = 6;
            faces = mMeshSet.mFaces;
            faces[3].mF02[0] = 6;
            faces = mMeshSet.mFaces;
            faces[3].mF02[1] = 7;
            faces = mMeshSet.mFaces;
            faces[3].mF02[2] = 4;
            sFaceSet* faceSet = mItem.mFaceSets;
            faceSet->mTexture = mTextures.mTextures;
            faceSet = mItem.mFaceSets;
            faceSet->mFirstFace = 0;
            faceSet = mItem.mFaceSets;
            faceSet->mFaceCount = 4;
            faceSet = mItem.mFaceSets;
            faceSet->mF08 = 0;
            faceSet = mItem.mFaceSets;
            faceSet->mF0A = 7;
            sUVSet* uvs = mMeshSet.mUVSets;
            uvs[0].mF00[0] = kHalf;
            uvs[0].mF00[4] = kOne;
            uvs = mMeshSet.mUVSets;
            uvs[0].mF00[1] = kOne;
            uvs[0].mF00[5] = kOne;
            uvs = mMeshSet.mUVSets;
            uvs[1].mF00[3] = kZero;
            uvs[1].mF00[7] = kZero;
            uvs = mMeshSet.mUVSets;
            uvs[1].mF00[2] = kHalf;
            uvs[1].mF00[6] = kZero;
            CalcUVs();
            mData = reinterpret_cast<void*>(1);
        }
    }
}

// The textures and the mesh are freed (in that order) only when the constructor finished; the base destructor and the delete are the
// language's.
InterfaceDrawEngineVGradient::~InterfaceDrawEngineVGradient() {
    if (mData != nullptr) {
        DMFreeTextures(&mTextures);
        DMFreeMesh(&mMeshSet);
    }
}

// The four vertices on the edge between the halves go to the fraction (y); x and z are fixed. The fraction and the set's pointer are read
// again before each group.
void InterfaceDrawEngineVGradient::CalcVerts() {
    float fraction = mFraction;
    sVertexPosnSet* vertices = mMeshSet.mVertexSets;
    vertices[0].x[2] = kFull;
    vertices[0].y[2] = fraction;
    vertices[0].z[2] = kZero;
    fraction = mFraction;
    vertices = mMeshSet.mVertexSets;
    vertices[1].x[1] = kFull;
    vertices[1].y[1] = fraction;
    vertices[1].z[1] = kZero;
    fraction = mFraction;
    vertices = mMeshSet.mVertexSets;
    vertices[0].x[3] = kZero;
    vertices[0].y[3] = fraction;
    vertices[0].z[3] = kZero;
    fraction = mFraction;
    vertices = mMeshSet.mVertexSets;
    vertices[1].x[0] = kZero;
    vertices[1].y[0] = fraction;
    vertices[1].z[0] = kZero;
}

// The texture coordinates on the edge between the halves: the fraction / 100 (single precision), computed first.
void InterfaceDrawEngineVGradient::CalcUVs() {
    const float edge = mFraction / kFull;
    sUVSet* uvs = mMeshSet.mUVSets;
    uvs[0].mF00[3] = kHalf;
    uvs[0].mF00[7] = edge;
    uvs = mMeshSet.mUVSets;
    uvs[0].mF00[2] = kOne;
    uvs[0].mF00[6] = edge;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[0] = kZero;
    uvs[1].mF00[4] = edge;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[1] = kHalf;
    uvs[1].mF00[5] = edge;
}

// A change of state enables or disables the mesh's drawing; the same state does nothing. The 16-bit argument is compared as a short.
void InterfaceDrawEngineVGradient::Show(int16_t shown) {
    if (shown != mShown) {
        if (shown != 0) {
            DMEnableMeshRender(&mMeshSet);
        } else {
            DMDisableMeshRender(&mMeshSet);
        }
        mShown = shown;
    }
}
