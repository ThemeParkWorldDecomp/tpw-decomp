// The resizable frame and slider draw objects (main app of SimTheme Park 2.04b3, the ui_utils.cpp range 0x10187a28..0x10188fb0, unit w44):
// InterfaceDrawEngineResizableFrame::Change 0x101887f0 and InstanceMe 0x10188a68, InterfaceDrawEngineResizableSlider::Change 0x10188cf0 and
// InstanceMe 0x10188ed0. Their constructors and destructors are in src/ui_DrawObject.cpp; the base class's CalculateFrame, Change and
// InstanceMe are in src/ui_DrawObjectEngineMesh.cpp. See include/ui_DrawObject.h.
//
// A frame is a mesh of one node whose 16 vertices (four sets of four, x, y, z each) form a 3 x 3 patch: when the window is larger than the
// mesh's box, the vertices on the right (x above 0) move right by the difference in width and those at the bottom (y below 0) move down
// by the difference in height, so the borders keep their size and the middle stretches. A slider does the same for its first eight
// vertices, vertically only.
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"

namespace {

constexpr uint32_t kChangeRect = 0x10;

// The rectangle of a change goes into the object's rectangle (+0x14..+0x1a) and into its second one (+0x20..+0x26). Only the four shorts
// are written (the words at +0x10 and +0x1c are the rectangles' vtables).
void TakeRect(InterfaceDrawObject& object, const CoolRect& rect) {
    object.mRect.mLeft = rect.mLeft;
    object.mRect.mTop = rect.mTop;
    object.mRect.mRight = rect.mRight;
    object.mRect.mBottom = rect.mBottom;
    object.mRect2.mLeft = object.mRect.mLeft;
    object.mRect2.mTop = object.mRect.mTop;
    object.mRect2.mRight = object.mRect.mRight;
    object.mRect2.mBottom = object.mRect.mBottom;
}

// The vertex i (0 to 15) of a node: block i / 4, lane i % 4 of the structure-of-arrays sets.
sVertexPosnSet& VertexSet(tag_sNode* node, int index) { return node->mVertices[index >> 2]; }

}  // namespace

// ---- the frame

// 0x10188a68. An object that is in the library and has windows using it (mResizable) and a mesh answers a new frame object over an instance
// of its mesh (InstanceMesh in the UI's heap, flags 4 where the base class passes 1); any other answers itself. The original makes a plain
// engine mesh object (0x84 bytes) and gives it the frame's vtable; the new object's frame base, frame and frame count are the old one's.
InterfaceDrawObject* InterfaceDrawEngineResizableFrame::InstanceMe() {
    if (mInLibrary == 0 || mResizable == 0 || mData == nullptr) return this;
    tag_sMesh* instance = nullptr;
    InstanceMesh(static_cast<tag_sMesh*>(mData), gUIHeap, 4, &instance, 0);
    InterfaceDrawEngineResizableFrame* copy = new InterfaceDrawEngineResizableFrame(instance);
    copy->mFrameBase = mFrameBase;
    copy->mFrame = mFrame;
    copy->mFrameCount = mFrameCount;
    return copy;
}

// 0x101887f0. Nothing at all without a record (the base class's Change is not reached either). Otherwise the frame is chosen (CalculateFrame)
// and, when that gives a node: flag 0x10 takes the rectangle; the node's vertices are stretched by the difference between the rectangle and
// the node's box (the original's test of the flags before that is always true); the box grows the same way; then the base class's Change.
// The deltas are floats: dx = (float)(right - left) - (maxX - minX), dy = -((float)(bottom - top) - (maxY - minY)). A vertex moves right by
// dx when its x is above 0 and by dy when its y is below 0 (a NaN moves neither). The box's minimum y becomes minY + dy and its maximum x
// maxX + dx.
void InterfaceDrawEngineResizableFrame::Change(uint32_t flags, DOChange* change) {
    if (change == nullptr) return;
    tag_sNode* node = CalculateFrame(flags, change);
    if (node != nullptr) {
        if ((flags & kChangeRect) != 0) TakeRect(*this, *change->mRect);
        const float minX = node->mBox.mMin[0];
        const float minY = node->mBox.mMin[1];
        const float minZ = node->mBox.mMin[2];
        const float maxX = node->mBox.mMax[0];
        const float maxY = node->mBox.mMax[1];
        const float maxZ = node->mBox.mMax[2];
        const float boxWidth = maxX - minX;
        const float boxHeight = maxY - minY;
        const int32_t width = static_cast<int32_t>(mRect.mRight) - static_cast<int32_t>(mRect.mLeft);
        const int32_t height = static_cast<int32_t>(mRect.mBottom) - static_cast<int32_t>(mRect.mTop);
        const float dx = static_cast<float>(width) - boxWidth;
        const float dy = -(static_cast<float>(height) - boxHeight);
        for (int i = 0; i < 16; ++i) {
            sVertexPosnSet& set = VertexSet(node, i);
            const int lane = i & 3;
            float x = set.x[lane];
            float y = set.y[lane];
            const float z = set.z[lane];
            if (x > 0.0f) x = x + dx;
            if (y < 0.0f) y = y + dy;
            set.x[lane] = x;
            set.y[lane] = y;
            set.z[lane] = z;
        }
        node->mBox.mMin[0] = minX;
        node->mBox.mMin[1] = minY + dy;
        node->mBox.mMin[2] = minZ;
        node->mBox.mMax[0] = maxX + dx;
        node->mBox.mMax[1] = maxY;
        node->mBox.mMax[2] = maxZ;
    }
    InterfaceDrawEngineMesh::Change(flags, change);
}

// ---- the slider

// 0x10188ed0: the frame's InstanceMe, for the slider (the same code with the slider's vtable).
InterfaceDrawObject* InterfaceDrawEngineResizableSlider::InstanceMe() {
    if (mInLibrary == 0 || mResizable == 0 || mData == nullptr) return this;
    tag_sMesh* instance = nullptr;
    InstanceMesh(static_cast<tag_sMesh*>(mData), gUIHeap, 4, &instance, 0);
    InterfaceDrawEngineResizableSlider* copy = new InterfaceDrawEngineResizableSlider(instance);
    copy->mFrameBase = mFrameBase;
    copy->mFrame = mFrame;
    copy->mFrameCount = mFrameCount;
    return copy;
}

// 0x10188cf0. The node is the mesh header's FIRST node (not the chosen one), read before anything is tested (so a NULL mesh or header is
// read through NULL). With a record that has flag 0x10 and a node: the rectangle is taken, then the node's first eight vertices (the top
// two sets) move down only: y below 0 gets dy = -((float)(bottom - top) - (maxY - minY)); x and z are rewritten as they were; the box's
// minimum y becomes minY + dy. The base class's Change always follows (even for a NULL record).
void InterfaceDrawEngineResizableSlider::Change(uint32_t flags, DOChange* change) {
    tag_sNode* node = static_cast<tag_sMesh*>(mData)->mHeader->mF78;
    if (change != nullptr && (flags & kChangeRect) != 0 && node != nullptr) {
        TakeRect(*this, *change->mRect);
        const float minX = node->mBox.mMin[0];
        const float minY = node->mBox.mMin[1];
        const float minZ = node->mBox.mMin[2];
        const float maxX = node->mBox.mMax[0];
        const float maxY = node->mBox.mMax[1];
        const float maxZ = node->mBox.mMax[2];
        const int32_t height = static_cast<int32_t>(mRect.mBottom) - static_cast<int32_t>(mRect.mTop);
        const float dy = -(static_cast<float>(height) - (maxY - minY));
        for (int i = 0; i < 8; ++i) {
            sVertexPosnSet& set = VertexSet(node, i);
            const int lane = i & 3;
            const float x = set.x[lane];
            float y = set.y[lane];
            const float z = set.z[lane];
            if (y < 0.0f) y = y + dy;
            set.x[lane] = x;
            set.y[lane] = y;
            set.z[lane] = z;
        }
        node->mBox.mMin[0] = minX;
        node->mBox.mMin[1] = minY + dy;
        node->mBox.mMin[2] = minZ;
        node->mBox.mMax[0] = maxX;
        node->mBox.mMax[1] = maxY;
        node->mBox.mMax[2] = maxZ;
    }
    InterfaceDrawEngineMesh::Change(flags, change);
}
