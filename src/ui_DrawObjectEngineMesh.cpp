// The engine mesh draw objects' instancing and changes (main app of SimTheme Park 2.04b3, the ui_utils.cpp range 0x10187a28..0x1018a1b0, unit
// w42): InterfaceDrawEngineMesh::InstanceMe, CalculateFrame and Change; the two gradients' Change (InterfaceDrawEngineVGradient 0x101895e0,
// InterfaceDrawEngineHGradient 0x10189ea0) with the horizontal one's CalcVerts; and UpdateMeshLocalMatrix (0x100652f4, TPRide.cpp), which
// the gradients' Change calls. GetUIMatrix and CombineMatricesNoWDest1st are in src/ui_matrix.cpp. The constructors, destructors and Show are
// in src/ui_DrawObject.cpp. See include/ui_DrawObject.h.
//
// Change flags (the DOChange fields): 1 mFrame, 2 mDepth, 4 mF08 (the z rotation), 8 the first word (the gradients' value), 0x10 mRect, 0x40 mF0C
// and mF0E (the pivot), 0x80000 mFrameBase. 0x55 (1, 4, 0x10, 0x40) are the ones that place the object.
#include <algorithm>
#include <cstring>

#include "ppc_math.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"

namespace {

constexpr uint32_t kChangeFrame = 0x1;
constexpr uint32_t kChangeDepth = 0x2;
constexpr uint32_t kChangeRotation = 0x4;
constexpr uint32_t kChangeValue = 0x8;
constexpr uint32_t kChangeRect = 0x10;
constexpr uint32_t kChangePivot = 0x40;
constexpr uint32_t kChangePlace = 0x55;
constexpr uint32_t kChangeFrameBase = 0x80000;

// The node's flags: 0x8010 hides it from the drawing (CalculateFrame sets them on the node it leaves and clears them on the node it chooses).
constexpr uint32_t kNodeHidden = 0x8010;

constexpr float kDepthRange = 40.0f;    // 0x42200000 (data 0x102a7af8): a depth is this many units from the screen's plane to the far plane
constexpr float kOne = 1.0f;            // 0x3f800000
constexpr float kFull = 100.0f;         // 0x42c80000
constexpr float kHalf = 0.5f;           // 0x3f000000
constexpr float kPerValue = 1.0f / 1024.0f;   // 0x3a800000: the gradients' value 1024 is 100 per cent

// The rectangle the change gives, to both of the object's (the second is the one the first was last copied to, kept for the windows).
void TakeRect(InterfaceDrawObject& object, const DOChange& change) {
    const CoolRect* rect = change.mRect;
    object.mRect.mLeft = rect->mLeft;
    object.mRect.mTop = rect->mTop;
    object.mRect.mRight = rect->mRight;
    object.mRect.mBottom = rect->mBottom;
    object.mRect2.mLeft = object.mRect.mLeft;
    object.mRect2.mTop = object.mRect.mTop;
    object.mRect2.mRight = object.mRect.mRight;
    object.mRect2.mBottom = object.mRect.mBottom;
}

// The rectangle's size, each a short (the subtraction is made in an int and passed as a short).
int16_t RectWidth(const InterfaceDrawObject& object) { return static_cast<int16_t>(object.mRect.mRight - object.mRect.mLeft); }
int16_t RectHeight(const InterfaceDrawObject& object) { return static_cast<int16_t>(object.mRect.mBottom - object.mRect.mTop); }

// The value of a change's first word as a gradient takes it: 100 * (word / 1024), at most 100 and at least 0 (a NaN stays one, which the
// comparisons let through as the original's do).
float GradientFraction(const DOChange& change) {
    int32_t word;
    std::memcpy(&word, change.mF00, sizeof word);
    const float scaled = kFull * (static_cast<float>(word) * kPerValue);
    if (scaled > kFull) return kFull;
    if (scaled < 0.0f) return 0.0f;
    return scaled;
}

// Marks the scene as changed so that the engine redoes the view (gei_System flag 0x80000000).
void MarkViewChanged() { gei_System.mFlags |= CSystem::kViewChanged; }

// What both gradients do for a change that places them (flags 4, 0x10): the box, ordered, through GetUIMatrix to the matrix. The box is
// (x0, y0) to (x1, y1) in the gradient's units; the swap is made when the far corner is below the near one.
template <class Gradient>
void PlaceGradient(Gradient& gradient) {
    float minX = gradient.mBox[0];
    float minY = gradient.mBox[1];
    float maxX = gradient.mBox[3];
    float maxY = gradient.mBox[4];
    if (maxX < minX) std::swap(maxX, minX);
    if (maxY < minY) std::swap(maxY, minY);
    GetUIMatrix(&gradient.mMatrix, gradient.mRect.mLeft, gradient.mRect.mBottom, RectWidth(gradient), RectHeight(gradient), minX, minY,
                maxX - minX, maxY - minY, gradient.mF30, gradient.mF32, gradient.mF2C);
}

}  // namespace

// ---- InterfaceDrawEngineMesh

// An object that is in the library and has windows using it answers a new object of its own: an instance of its mesh from the UI's heap (flags 1:
// the nodes copied), in a new engine mesh draw object that has the old one's frame base, frame and frame count. Any other object (not in the
// library, or in it but not taken by a window, or without a mesh) answers itself.
InterfaceDrawObject* InterfaceDrawEngineMesh::InstanceMe() {
    if (mInLibrary == 0 || mResizable == 0 || mData == nullptr) return this;
    tag_sMesh* instance = nullptr;
    InstanceMesh(static_cast<tag_sMesh*>(mData), gUIHeap, 1, &instance, 0);
    InterfaceDrawEngineMesh* copy = new InterfaceDrawEngineMesh(instance);
    copy->mFrameBase = mFrameBase;
    copy->mFrame = mFrame;
    copy->mFrameCount = mFrameCount;
    return copy;
}

// The node of the frame: flags 0x80000 and 1 take the frame base and the frame from the change, the node is the one that many links down the
// mesh's chain from the header's first (the walk stops at the end of the chain, so a frame past it makes NULL; the count is taken as a short
// and counts down from there, so a negative sum walks the chain's whole length). The node left loses visibility (flags 0x8010), the new one
// gets it. Without either flag nothing is changed and the answer is the current node, whatever the other flags.
tag_sNode* InterfaceDrawEngineMesh::CalculateFrame(uint32_t flags, DOChange* change) {
    tag_sMesh* mesh = static_cast<tag_sMesh*>(mData);
    if ((flags & kChangeFrame) == 0 && (flags & kChangeFrameBase) == 0) return reinterpret_cast<tag_sNode*>(mNodeFlags);
    if ((flags & kChangeFrameBase) != 0) mFrameBase = change->mFrameBase;
    if ((flags & kChangeFrame) != 0) mFrame = change->mFrame;
    tag_sNode* node = mesh->mHeader->mF78;
    int32_t count = mFrame + mFrameBase;
    while (true) {
        const bool done = static_cast<int16_t>(count) == 0;
        --count;
        if (done || node == nullptr) break;
        node = node->mF0C;
    }
    if (mNodeFlags != nullptr) *mNodeFlags |= kNodeHidden;
    mNodeFlags = reinterpret_cast<uint32_t*>(node);
    if (mNodeFlags != nullptr) *mNodeFlags &= ~kNodeHidden;
    return node;
}

// The placement of the mesh. Nothing happens without a mesh, a change or flags, nor before a node has been chosen. The node's box (its two
// corners) goes through the node's matrix times its ancestors' (each parent's matrix is multiplied in, via mF04), the corners are put in order
// on x and y, and GetUIMatrix makes the object's matrix from them and the rectangle. A "localised" object (the language's folder) then gets
// the z scale (localised / 40) / box depth, the depth being its absolute value. The depth (mDepth, translation z) is kept through a placement
// and set from flag 2 as 1 - depth / 40; the engine mesh gets the matrix whenever anything above changed.
void InterfaceDrawEngineMesh::Change(uint32_t flags, DOChange* change) {
    if (mData == nullptr || change == nullptr || flags == 0) return;
    bool changed = false;
    if ((flags & kChangePivot) != 0) {
        mF30 = change->mF0C;
        mF32 = change->mF0E;
    }
    tag_sMesh* mesh = static_cast<tag_sMesh*>(mData);
    tag_sNode* node = CalculateFrame(flags, change);
    if (node == nullptr) return;
    if ((flags & kChangeRect) != 0) TakeRect(*this, *change);
    if ((flags & kChangeRotation) != 0) mF2C = change->mF08;
    float depth = mMatrix.m[3][2];
    if ((flags & kChangePlace) != 0) {
        s3DPoint minimum = {node->mBox.mMin[0], node->mBox.mMin[1], node->mBox.mMin[2]};
        s3DPoint maximum = {node->mBox.mMax[0], node->mBox.mMax[1], node->mBox.mMax[2]};
        sMatrix world = node->mMatrix;
        for (tag_sNode* parent = node->mF04; parent != nullptr; parent = parent->mF04) CombineMatricesNoWDest1st(&world, &parent->mMatrix);
        VectorThroughLMatrixNoW(&world, &minimum);
        VectorThroughLMatrixNoW(&world, &maximum);
        if (maximum.x < minimum.x) std::swap(maximum.x, minimum.x);
        if (maximum.y < minimum.y) std::swap(maximum.y, minimum.y);
        GetUIMatrix(&mMatrix, mRect.mLeft, mRect.mBottom, RectWidth(*this), RectHeight(*this), minimum.x, minimum.y, maximum.x - minimum.x,
                    maximum.y - minimum.y, mF30, mF32, mF2C);
        if (mLocalised != 0) {
            float extent = maximum.z - minimum.z;
            if (extent < 0.0f) extent = -extent;
            mMatrix.m[2][2] = (static_cast<float>(mLocalised) / kDepthRange) / extent;
        }
        changed = true;
    }
    if ((flags & kChangeDepth) != 0) {
        changed = true;
        mDepth = change->mDepth;
        depth = kOne - static_cast<float>(mDepth) / kDepthRange;
    }
    if (changed) {
        mMatrix.m[3][2] = depth;
        mesh->mEngineMesh->SetLocalMatrix(1, mMatrix);
        MarkViewChanged();
    }
}

// ---- the gradients

// The engine mesh of the mesh gets the matrix as its local matrix.
void UpdateMeshLocalMatrix(tag_sMesh* mesh, sMatrix* matrix) { mesh->mEngineMesh->SetLocalMatrix(1, *matrix); }

// The four vertices on the edge between the quad's two halves move to x = the fraction (the first set's lanes 1 and 2, the second's lanes 0
// and 1); the upper ones are at 100, the lower ones at 0. The fraction and the vertex pointer are read again for each group, as the original does.
void InterfaceDrawEngineHGradient::CalcVerts() {
    sVertexPosnSet* sets = mMeshSet.mVertexSets;
    sets[0].x[1] = mFraction;
    sets[0].y[1] = kFull;
    sets[0].z[1] = 0.0f;
    sets = mMeshSet.mVertexSets;
    sets[1].x[0] = mFraction;
    sets[1].y[0] = kFull;
    sets[1].z[0] = 0.0f;
    sets = mMeshSet.mVertexSets;
    sets[0].x[2] = mFraction;
    sets[0].y[2] = 0.0f;
    sets[0].z[2] = 0.0f;
    sets = mMeshSet.mVertexSets;
    sets[1].x[1] = mFraction;
    sets[1].y[1] = 0.0f;
    sets[1].z[1] = 0.0f;
}

// Like the mesh object's, for a gradient that has a quad of its own: the rectangle, the z rotation, the placement (the box, ordered, from
// 0x4c; flags 4 and 0x10), the depth, then flag 8: the first word sets the fraction (see GradientFraction) and, when that is a change, redoes
// the vertices and (inline here, the same as the quad's CalcUVs 0x10189d70) the texture coordinates: the fraction / 100 on the edge between the
// halves. A change of the fraction alone tells the engine the view changed; one that also placed the object does so after the matrix is sent.
// The depth is stored back whatever the flags are.
void InterfaceDrawEngineHGradient::Change(uint32_t flags, DOChange* change) {
    if (mData == nullptr || change == nullptr || flags == 0) return;
    bool changed = false;
    if ((flags & kChangeRect) != 0) TakeRect(*this, *change);
    if ((flags & kChangeRotation) != 0) mF2C = change->mF08;
    float depth = mMatrix.m[3][2];
    if ((flags & (kChangeRotation | kChangeRect)) != 0) {
        PlaceGradient(*this);
        changed = true;
    }
    if ((flags & kChangeDepth) != 0) {
        changed = true;
        mDepth = change->mDepth;
        depth = kOne - static_cast<float>(mDepth) / kDepthRange;
    }
    mMatrix.m[3][2] = depth;
    if ((flags & kChangeValue) != 0) {
        const float fraction = GradientFraction(*change);
        if (!(fraction == mFraction)) {
            mFraction = fraction;
            CalcVerts();
            const float edge = mFraction / kFull;
            sUVSet* uvs = mMeshSet.mUVSets;
            uvs[0].mF00[1] = edge;
            uvs[0].mF00[5] = kOne;
            uvs = mMeshSet.mUVSets;
            uvs[1].mF00[0] = edge;
            uvs[1].mF00[4] = kHalf;
            uvs = mMeshSet.mUVSets;
            uvs[0].mF00[2] = edge;
            uvs[0].mF00[6] = kHalf;
            uvs = mMeshSet.mUVSets;
            uvs[1].mF00[1] = edge;
            uvs[1].mF00[5] = 0.0f;
            if (!changed) MarkViewChanged();
        }
    }
    if (changed) {
        UpdateMeshLocalMatrix(mMeshSet.mMesh, &mMatrix);
        MarkViewChanged();
    }
}

// The vertical one is the same, but its quad's vertices and texture coordinates are redone by virtual members (CalcVerts and CalcUVs).
void InterfaceDrawEngineVGradient::Change(uint32_t flags, DOChange* change) {
    if (mData == nullptr || change == nullptr || flags == 0) return;
    bool changed = false;
    if ((flags & kChangeRect) != 0) TakeRect(*this, *change);
    if ((flags & kChangeRotation) != 0) mF2C = change->mF08;
    float depth = mMatrix.m[3][2];
    if ((flags & (kChangeRotation | kChangeRect)) != 0) {
        PlaceGradient(*this);
        changed = true;
    }
    if ((flags & kChangeDepth) != 0) {
        changed = true;
        mDepth = change->mDepth;
        depth = kOne - static_cast<float>(mDepth) / kDepthRange;
    }
    mMatrix.m[3][2] = depth;
    if ((flags & kChangeValue) != 0) {
        const float fraction = GradientFraction(*change);
        if (!(fraction == mFraction)) {
            mFraction = fraction;
            CalcVerts();
            CalcUVs();
            if (!changed) MarkViewChanged();
        }
    }
    if (changed) {
        UpdateMeshLocalMatrix(mMeshSet.mMesh, &mMatrix);
        MarkViewChanged();
    }
}
