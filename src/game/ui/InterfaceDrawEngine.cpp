// InterfaceDrawEngine.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceDrawEngine code, named after it.

#define TPW_UIMENU_CTOR_EXTERNAL
#include "Advisor.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "InGame.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "LbTime.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainWindowCallback.h"
#include "platform/msl.h"
#include "Postcard.h"
#include "ppc_math.h"
#include "Quickload.h"
#include "rsse.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "std_basic_streambuf_wchar.h"
#include "tb_ddraw.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "TbColour.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPResource.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "ui_textmanager.h"
#include "UIInterface.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>

// ui_main.cpp (main app of SimTheme Park 2.04b3), the part that starts the interface:
// the meshes of the interface (0x10184abc..0x10185640), InitGeneralUI (0x10185b68), the
// clock tick objects (0x10185b08, 0x10187994) and the file's global strings.
//
// Every .md2 file of the "UI" resource becomes a draw object in the library: the frames
// ("!frame", "!f_plain", "!cbut") and the slider ("!slider") under their file name, any
// other mesh under the name of its root node. The meshes of the language's folder follow.
// The first mesh loaded is what puts the "welcome" picture on the screen (CMesh::Init
// steps the loading picture).

TbDynamicStringTemplate<char> gUIButtonName("!cbut");
uint32_t clockTickFunction::tickyticky() { return 0; }

// Unit v5L16 (the tail): two leaves of the main app (SimTheme Park 2.04b3). UIMain_GetObjectName (0x101810fc, 60 bytes, Mac file
// ui_main.cpp) and CalcZTrans (0x101879d0, 52 bytes, the draw objects' depth, Mac file ui_DrawObject.cpp).

// The depth of interface layer z: 1 - z / 40 in single precision (the Mac's int to double conversion is rounded to single by its
// fsubs, then fdivs and fsubs; the 40.0f is the data word at 0x102a7af8 that the engine mesh also reads).
float CalcZTrans(int32_t z) {
    constexpr float kDepthRange = 40.0f;
    constexpr float kOne = 1.0f;
    return kOne - static_cast<float>(z) / kDepthRange;
}

// The UI's draw objects and their library (main app of SimTheme Park 2.04b3): the cursor
// base class and the cursor table (0x101cc6a0..0x101ccc8c), the draw object base class and
// DrawObjectLibrary (0x101cccc0..0x101cd42c), the engine mesh objects'
// constructors, destructors and Show (0x10187a28..0x10188c04), and the base's three empty
// members that the compiler put with them (0x1018a1b0..0x1018a234).
// PARTIAL: see ui_DrawObject.h.

sHeapInfo* gUIHeap;

// ---- InterfaceDrawEngineMesh

// Marks the mesh as the interface's (header flag 0x200), takes its node count as the number
// of frames, flags every node of the chain from the header's root (0xc8010, and 0x8000000
// too on a 640-wide screen) and makes the mesh's engine mesh. The matrix is not set.
InterfaceDrawEngineMesh::InterfaceDrawEngineMesh(void* mesh) : InterfaceDrawObject(mesh) {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data != nullptr) {
        data->mHeader->mFlags |= 0x200;
        mFrameCount = static_cast<int16_t>(data->mHeader->mNodeCount);
        for (tag_sNode* node = data->mHeader->mF78; node != nullptr; node = node->mF0C) {
            node->mFlags |= 0xc8010;
            if (gScreenWidth == 0x280) node->mFlags |= 0x8000000;
        }
        CreateEngineMesh(data);
    }
    mNodeFlags = nullptr;
    mLocalised = 0;
}

// An object the library has owns its mesh's engine mesh; any other is an instance.
InterfaceDrawEngineMesh::~InterfaceDrawEngineMesh() {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data != nullptr) {
        if (mInLibrary == 0) DestroyMeshInstance(data, gUIHeap, 0);
        else DestroyEngineMesh(data);
    }
}

void InterfaceDrawEngineMesh::Show(int16_t shown) {
    tag_sMesh* data = static_cast<tag_sMesh*>(mData);
    if (data == nullptr || shown == mShown) return;
    if (shown == 0) {
        if (mNodeFlags != nullptr) *mNodeFlags |= 0x8010;
        DisableMeshDraw(data);
    } else {
        if (mNodeFlags != nullptr) *mNodeFlags &= 0xffff7fef;
        EnableMeshDraw(data, &mMatrix, 0);
    }
    mShown = shown;
}

// The UI's matrix helpers (main app of SimTheme Park 2.04b3): GetUIMatrix (0x10187ce4, 848 bytes, the ui_utils.cpp range that holds the engine
// mesh draw objects) and the matrix combiner CombineMatricesNoWDest1st (0x100c2694, 548 bytes, the rsse matrix code), which GetUIMatrix and the
// engine mesh object's Change use. See include/ui_DrawObject.h and include/rsse.h.

namespace {

// The constants the listing loads (0x10265460..0x10265488 and the data words 0x102a7af0, 0x102a7af4).
constexpr float kPi = 3.14159274f;           // 0x40490fdb
constexpr float kHalfPi = 1.57079637f;       // 0x3fc90fdb
constexpr float kUnitsPerTurn = 2048.0f;     // 0x45000000: the angle is in 1/4096ths of a turn, so 2048 of them are pi
constexpr float kHalfWidth = 1024.0f;        // 0x44800000: half the UI's 2048 units
constexpr float kHeight = 1536.0f;           // 0x44c00000
constexpr float kHalfHeight = 768.0f;        // 0x44400000
constexpr float kInverseWidth = 1.0f / 2048.0f;     // 0x3a000000 (data 0x102a7af0)
constexpr float kInverseHeight = 1.0f / 1536.0f;    // 0x3a2aaaab (data 0x102a7af4)
constexpr float kFour = 4.0f;                // 0x40800000
constexpr float kThree = 3.0f;               // 0x40400000
constexpr float kQuarter = 0.25f;            // 0x3e800000

}  // namespace

void CombineMatricesNoWDest1st(sMatrix* destination, sMatrix* source);

// The matrix that takes an object whose box (lower left corner x, y and size w, h in the mesh's units) is placed on the UI rectangle with the
// given left and BOTTOM edge and size (units of the 2048 x 1536 space) to the engine's screen. Five identity matrices are made, in this order:
// the rotation, the translation back from the pivot, the placement, the scale and the translation to the pivot.
//   placement: x = (left - 1024) * sx - x0 and y = (1536 - bottom - 768) * sy - y0, with sx, sy the mesh units per UI unit (w / width, h /
//              height); the products fuse with the subtractions as the listing has them;
//   scale:     (2 / sx) / 2048 and (2 / sy) / 1536 (the data words are 1/2048 and 1/1536), z 0;
//   rotation (only for a nonzero angle): the angle is pi * angle / 2048 radians; the sine and cosine come from TPGlobals' sine table at the
//              index (angle * scale, truncated) & mask, the cosine's from (pi/2 + angle) * scale. The 2 x 2 part is [cos, -4 sin / 3; 3 sin / 4,
//              cos] (the screen is 4:3). The pivot, in the UI's units from its centre, is moved to the origin by the pivot matrix (translation
//              -(pivotX - 1024) * sx, -(1536 - pivotY - 768) * sy) and back by the other (the same without the minus), both in the object's units.
// The result is placement * (pivot * rotation * back) * scale, copied whole (the fourth column is the identity's).
void GetUIMatrix(sMatrix* matrix, int16_t left, int16_t bottom, int16_t width, int16_t height, float x, float y, float w, float h,
                 int16_t pivotX, int16_t pivotY, int16_t angle) {
    sMatrix rotation;
    sMatrix back;
    sMatrix placement;
    sMatrix scale;
    sMatrix pivot;
    SetMatrixIdentity(&rotation);
    SetMatrixIdentity(&back);
    SetMatrixIdentity(&placement);
    SetMatrixIdentity(&scale);
    SetMatrixIdentity(&pivot);
    const float scaleX = w / static_cast<float>(width);
    const float scaleY = h / static_cast<float>(height);
    const float inverseX = 2.0f / scaleX;
    const float inverseY = 2.0f / scaleY;
    const float up = kHeight - static_cast<float>(bottom);
    placement.m[3][0] = ppc_fnmsubs(kHalfWidth, scaleX, ppc_fmsubs(static_cast<float>(left), scaleX, x));
    placement.m[3][1] = ppc_fnmsubs(kHalfHeight, scaleY, ppc_fmsubs(up, scaleY, y));
    scale.m[0][0] = inverseX * kInverseWidth;
    scale.m[1][1] = inverseY * kInverseHeight;
    scale.m[2][2] = 0.0f;
    if (angle != 0) {
        const float radians = (kPi * static_cast<float>(angle)) / kUnitsPerTurn;
        const uint32_t mask = gTPGlobals.mSinTableMask;
        const float turn = gTPGlobals.mSinTableScale;
        const float sine = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(ppc_fctiwz(radians * turn))];
        const float cosine = gTPGlobals.mSinTable[mask & static_cast<uint32_t>(ppc_fctiwz((kHalfPi + radians) * turn))];
        const float offsetX = static_cast<float>(pivotX) - kHalfWidth;
        const float offsetY = (kHeight - static_cast<float>(pivotY)) - kHalfHeight;
        const float negativeX = ppc_fneg(offsetX);
        const float negativeY = ppc_fneg(offsetY);
        const float negativeSine = ppc_fneg(sine);
        const float threeSine = kThree * sine;
        pivot.m[3][0] = negativeX * scaleX;
        back.m[3][0] = offsetX * scaleX;
        pivot.m[3][1] = negativeY * scaleY;
        back.m[3][1] = offsetY * scaleY;
        rotation.m[0][0] = cosine;
        rotation.m[1][1] = cosine;
        rotation.m[0][1] = (kFour * negativeSine) / kThree;
        rotation.m[1][0] = threeSine * kQuarter;
        CombineMatricesNoWDest1st(&pivot, &rotation);
        CombineMatricesNoWDest1st(&pivot, &back);
    }
    CombineMatricesNoWDest1st(&placement, &pivot);
    CombineMatricesNoWDest1st(&placement, &scale);
    *matrix = placement;
}

// The engine mesh draw objects' instancing and changes (main app of SimTheme Park 2.04b3, the ui_utils.cpp range 0x10187a28..0x1018a1b0, unit
// w42): InterfaceDrawEngineMesh::InstanceMe, CalculateFrame and Change; the two gradients' Change (InterfaceDrawEngineVGradient 0x101895e0,
// InterfaceDrawEngineHGradient 0x10189ea0) with the horizontal one's CalcVerts; and UpdateMeshLocalMatrix (0x100652f4, TPRide.cpp), which
// the gradients' Change calls. GetUIMatrix and CombineMatricesNoWDest1st are in src/ui_matrix.cpp. The constructors, destructors and Show are
// in src/ui_DrawObject.cpp. See include/ui_DrawObject.h.
//
// Change flags (the DOChange fields): 1 mFrame, 2 mDepth, 4 mF08 (the z rotation), 8 the first word (the gradients' value), 0x10 mRect, 0x40 mF0C
// and mF0E (the pivot), 0x80000 mFrameBase. 0x55 (1, 4, 0x10, 0x40) are the ones that place the object.

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

void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawEngineMesh::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}

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

InterfaceDrawEngineResizableFrame::InterfaceDrawEngineResizableFrame(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableFrame::~InterfaceDrawEngineResizableFrame() {}

// The resizable frame and slider draw objects (main app of SimTheme Park 2.04b3, the ui_utils.cpp range 0x10187a28..0x10188fb0, unit w44):
// InterfaceDrawEngineResizableFrame::Change 0x101887f0 and InstanceMe 0x10188a68, InterfaceDrawEngineResizableSlider::Change 0x10188cf0 and
// InstanceMe 0x10188ed0. Their constructors and destructors are in src/ui_DrawObject.cpp; the base class's CalculateFrame, Change and
// InstanceMe are in src/ui_DrawObjectEngineMesh.cpp. See include/ui_DrawObject.h.
//
// A frame is a mesh of one node whose 16 vertices (four sets of four, x, y, z each) form a 3 x 3 patch: when the window is larger than the
// mesh's box, the vertices on the right (x above 0) move right by the difference in width and those at the bottom (y below 0) move down
// by the difference in height, so the borders keep their size and the middle stretches. A slider does the same for its first eight
// vertices, vertically only.

namespace {

// The rectangle of a change goes into the object's rectangle (+0x14..+0x1a) and into its second one (+0x20..+0x26). Only the four shorts
// are written (the words at +0x10 and +0x1c are the rectangles' vtables).
void TakeRect_2(InterfaceDrawObject& object, const CoolRect& rect) {
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
        if ((flags & kChangeRect) != 0) TakeRect_2(*this, *change->mRect);
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

InterfaceDrawEngineResizableSlider::InterfaceDrawEngineResizableSlider(void* mesh) : InterfaceDrawEngineMesh(mesh) {}
InterfaceDrawEngineResizableSlider::~InterfaceDrawEngineResizableSlider() {}

// 0x10188cf0. The node is the mesh header's FIRST node (not the chosen one), read before anything is tested (so a NULL mesh or header is
// read through NULL). With a record that has flag 0x10 and a node: the rectangle is taken, then the node's first eight vertices (the top
// two sets) move down only: y below 0 gets dy = -((float)(bottom - top) - (maxY - minY)); x and z are rewritten as they were; the box's
// minimum y becomes minY + dy. The base class's Change always follows (even for a NULL record).
void InterfaceDrawEngineResizableSlider::Change(uint32_t flags, DOChange* change) {
    tag_sNode* node = static_cast<tag_sMesh*>(mData)->mHeader->mF78;
    if (change != nullptr && (flags & kChangeRect) != 0 && node != nullptr) {
        TakeRect_2(*this, *change->mRect);
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

// The vertical gradient draw object's constructor 0x10188ff8 (792 bytes), destructor 0x1018934c (140), CalcVerts 0x10189414 (92), CalcUVs
// 0x101894b0 (76) and Show 0x10189538 (108) (Mac file ui_utils.cpp; unit w118). Its Change is in src/ui_DrawObjectEngineMesh.cpp (unit w42).
//
// The object is a quad of the interface in a dynamic mesh set of its own: two halves of four vertices (the top (0,100) (100,100) (100,f)
// (0,f) and the bottom (0,f) (100,f) (100,0) (0,0), f being the fraction), eight points, two UV sets and four faces. The Mac builds the
// set, its one entry and its texture set inside the object and lets DMLoadTextures and DMCreateMesh fill in the arrays; the constructor then
// writes the quad. The offsets of the Mac's object are in the header; the stores below keep the listing's order.

namespace {
constexpr float kZero = 0.0f;      // 0x10265468
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

void UpdateMeshLocalMatrix(tag_sMesh* mesh, sMatrix* matrix);

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

// The horizontal gradient's constructor (0x101898c8, main app): a quad of two halves made as a dynamic mesh set of its own. Its neighbour is
// src/ui_DrawObjectEngineMesh.cpp (Change, CalcVerts).

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

// Unit v51129: std::basic_string<wchar_t>::assign (0x101b2510), the horizontal gradient's destructor (0x10189c0c), the ride list's GetList
// (0x1017b938), the server list's base destructor (0x1017a6e4), the theme list's LoggedInGetList (0x10179f50) and destructor (0x10179d28).

// 0x10189c0c (140): the vtable is stored, the textures and the mesh are freed when the mesh was made (mData set), then the base's
// destructor runs. The delete flag (r4) is the C++ delete.
InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient() {
    if (mData != nullptr) {
        DMFreeTextures(&mTextures);
        DMFreeMesh(&mMeshSet);
    }
}

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

// Unit v51012: the rest of the interface's ui_utils.cpp helpers (main app of SimTheme Park 2.04b3):
//   UI_DrawPostcardText 0x1018bbc4, __sinit_ui_utils_cpp 0x1018bda8, MakeRenderAreaFromRAW 0x1018b434,
//   InterfaceDrawEngineHGradient::CalcUVs 0x10189d70, InterfaceVisitorTimeTextObject::Change 0x1018efcc.

// 0x10189d70: the texture coordinates of the quad: the fraction / 100 on the edge between the halves.
void InterfaceDrawEngineHGradient::CalcUVs() {
    const float edge = mFraction / 100.0f;
    sUVSet* uvs = mMeshSet.mUVSets;
    uvs[0].mF00[1] = edge;
    uvs[0].mF00[5] = 1.0f;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[0] = edge;
    uvs[1].mF00[4] = 0.5f;
    uvs = mMeshSet.mUVSets;
    uvs[0].mF00[2] = edge;
    uvs[0].mF00[6] = 0.5f;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[1] = edge;
    uvs[1].mF00[5] = 0.0f;
}

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

// 0x10189df8: the vertical gradient's Show (the mesh render follows the state).
void InterfaceDrawEngineHGradient::Show(int16_t shown) {
    if (shown != mShown) {
        if (shown != 0) {
            DMEnableMeshRender(&mMeshSet);
        } else {
            DMDisableMeshRender(&mMeshSet);
        }
        mShown = shown;
    }
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

void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// The base window's small virtual members (main app, UI toolkit; unit w54): DrawFunction, Enable, operator-, GetValue, GetText and
// GetHelpText. Every window exposes them to the frame through its vtable (0x102a7d94); the button (src/InterfaceButton.cpp) and the
// other controls override some and call these.

// 0x1018a270 (4 bytes): nothing (the Mac's is a bare blr).
void InterfaceWindow::DrawFunction() {}

// InterfaceWindow (main app, UI toolkit 0x101da564..0x101dfe84, ClientDraw 0x1018a2a8 and InterfaceTimer::Stop
// 0x101d8428): the drawing walk, the positions, the z rotation, the frame animation, and UI_DefaultWindowCallback, the
// handler every window starts with. The draw objects a window owns are reached through their virtual members
// (include/ui_DrawObject.h): Draw for the client rectangle, Change for everything else, with the flags below.
//
// Change flags this file uses (DOChange fields): 1 mFrame, 4 mF08 (the z rotation), 0x10 mRect, 0x40 mF0C and mF0E (the
// pivot), 0x80 the first word (a value). The Mac builds the record on the stack with only the fields it sets; the host's
// is zeroed first, which nothing can see.

// ---- drawing

// The window's client draw object draws the client rectangle, one level above the window.
void InterfaceWindow::ClientDraw() {
    if (mClientDrawObject != nullptr) mClientDrawObject->Draw(0, &mClientRect, static_cast<int16_t>(mZ + 1));
}

// InterfaceScrollBar (main app, 0x101d6280..0x101d72fc and the two empty draw members at 0x1018a328/0x1018a364): the scroll bar control
// and its message function. See include/InterfaceScrollBar.h for the class and for the coordinates; the thumb is the child with id 3,
// the buttons those with ids 1 and 2.

void InterfaceScrollBar::DrawFunction() {}
void InterfaceScrollBar::ClientDraw() {}

// List box drawing, selection and entry-tree teardown (main app, unit w37b).

// 0x1018a3a0: the original is empty.
void InterfaceListBox::DrawFunction() {}

// 0x1018a3dc (1088 bytes). Indentation is cleared entirely when walking up to a sibling.
void InterfaceListBox::ClientDraw() {
    int16_t left, top, right, bottom;
    mClientRect.Get(&left, &top, &right, &bottom);
    mListRect.Get(&left, &top, &right, &bottom);
    auto xPixel = [](int32_t x) { return (x * gUIRealScreenWidth) / 2048; };
    auto yPixel = [](int16_t y) { return (int32_t(y) * gUIRealScreenHeight) / 1536; };
    TbRect clip{xPixel(left), yPixel(top), xPixel(right), yPixel(bottom)};
    LbPalette_FindColour(&_lbGlobalPalette, 0, 0, 0);
    LbPalette_FindColour(&_lbGlobalPalette, 255, 255, 255);
    TbColour selected;
    selected.mAlpha = 255;
    selected.mRed = selected.mGreen = selected.mBlue = 128;
    selected.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 128, 128, 128);
    TbColour textColour;
    textColour.mAlpha = textColour.mRed = textColour.mGreen = textColour.mBlue = 255;
    textColour.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 255, 255, 255);
    tagLBEntry* entry = GetIndexedEntry(mF150);
    int32_t y = mListRect.mTop;
    selected.mRed = 200;
    selected.mGreen = selected.mBlue = 0;
    selected.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 200, 0, 0);
    if (CFont::Set(CFont::kFont1, CFont::kOneColour) == 0) return;
    char indent[32];
    std::sprintf(indent, "%s", "");
    LbDraw_SetClipRect(&clip);
    if (entry != nullptr) {
        for (tagLBEntry* parent = entry; parent->mParent != nullptr; parent = parent->mParent) {
            std::strcat(indent, "    ");
        }
        for (int32_t row = 0; row < mF156 && entry != nullptr; ++row) {
            if (mF148 == row + mF150) {
                clip = {xPixel(static_cast<int16_t>(mListRect.mLeft + 2)), yPixel(static_cast<int16_t>(y)),
                        xPixel(static_cast<int16_t>(mListRect.mRight - 2)), yPixel(static_cast<int16_t>(y + mF154))};
                LbDraw_RectangleFilled(&clip, selected);
            }
            wchar_t wide[32];
            for (int i = 0; i < 32; ++i) wide[i] = entry->mText[i];
            char text[128];
            std::sprintf(text, "%s%ls", indent, wide);
            LbDraw_Text(xPixel(mListRect.mLeft + 2), yPixel(static_cast<int16_t>(y)), text, textColour);
            if (entry->mChild != nullptr) {
                entry = entry->mChild;
                std::strcat(indent, "    ");
            } else if (entry->mNext != nullptr) {
                entry = entry->mNext;
            } else {
                while (entry != nullptr && entry->mNext == nullptr) entry = entry->mParent;
                indent[0] = '\0';
                if (entry != nullptr) entry = entry->mNext;
            }
            y += mF154;
        }
    }
    LbDraw_SetClipRect(nullptr);
}

// Main UI edit box, 0x101cd468..0x101cf1ac; Mac layout in ui_controls.h (unit w37c).

void InterfaceEditBox::DrawFunction() {} // 0x1018a850
void InterfaceEditBox::ClientDraw() {} // 0x1018a88c

void InterfaceBar::DrawFunction() {}

// 0x1018a8fc, 0x1018a8c4: nothing.
void InterfaceTextBox::ClientDraw() {}

// List control drawing and teardown (main app, unit w37b).

// 0x1018a934 (104 bytes). The count and array are read again for each item.
void InterfaceListControl2::ClientDraw() {
    if (mItemWindows != nullptr) {
        for (int32_t i = 0; i < mVisibleRows; ++i) mItemWindows[i]->DrawMe();
    }
}

// 0x1018a9d4 (8 bytes): no text.
char16_t* InterfaceWindow::GetText() { return nullptr; }

// 0x1018aa0c (8 bytes): the help text's number (mHelpText, -1 from the constructor).
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
