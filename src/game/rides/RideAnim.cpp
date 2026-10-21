// RideAnim.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#define TPW_GLOBE_INIT_REAL 1
#include "AdvisorScoring.h"
#include "Bitmap.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "email.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "LevelInit.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSound.h"
#include "ui_main.h"
#include "UIInterface.h"
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

void SetRideAnimTime(uint32_t time) { gTPGlobals.mF4010 = time; }
uint32_t GetRideAnimTime(int32_t which) {
    if (which != 0) return gTPGlobals.mF4018;
    return gTPGlobals.mF4010;
}

// The ride animation set's set-up, the main app's rsse.cpp range: RAInitType (0x100c3430),
// CorrectNodeFlags1 (0x10067cf4), ResetAnimTimeStats (0x100c5a90), RAInitNodeAnimNodes
// (0x100c32ac) and InitRideAnimSet (0x100c7768). LoadRideAnimSet (src/RideAnimLoad.cpp)
// calls them. The mesh files' records keep their 32-bit layout (include/m3d2.h); the set
// and its time controls use native pointers (include/TPEngine.h).

// 0x100c32ac, 316 bytes. Points each node animation at its node and sets flag 0x800 on the
// node. The node is the header's full node with the animation's index (mF14), or the mini node
// past them. With mesh flag 4 it is the first full node whose mF50 is that index, and a node
// animation with no match keeps the node it had. Flag 0x4000 on the node animation swaps the
// words of its morph points (the record's first pointer, mVertexCount words of the node).
// Unless firstKind is nonzero, flag 0x100 is cleared on the node. InitRideAnimSet passes 1 for
// the first of the twelve animation kinds and 0 for the rest.
void RAInitNodeAnimNodes(tag_sMesh* mesh, tag_sAnimHeader* animation, int32_t firstKind) {
    tag_sM3D2Header* header = mesh->mHeader;
    tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
    for (uint32_t i = 0; i < animation->mNodeAnimCount; ++i, ++nodeAnim) {
        if ((mesh->mF00 & 4) == 0) {
            const uint32_t index = nodeAnim->mF14;
            if (index < header->mNodeCount) {
                nodeAnim->mNode = header->mNodes.get() + index;
            } else {
                nodeAnim->mNode = reinterpret_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
            }
        } else {
            for (uint32_t k = 0; k < header->mNodeCount; ++k) {
                tag_sNode* node = header->mNodes.get() + k;
                if (node->mF50 == nodeAnim->mF14) {
                    nodeAnim->mNode = node;
                    break;
                }
            }
        }
        if ((nodeAnim->mFlags & 0x4000) != 0) {
            auto* points = reinterpret_cast<sMorphPoints*>(nodeAnim->mMorph.get());
            SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(points->mF00.get()), nodeAnim->mNode->mVertexCount);
        }
        nodeAnim->mNode->mFlags |= 0x800;
        if (firstKind == 0) nodeAnim->mNode->mFlags &= 0xfffffeffu;
    }
}

// 0x100c3430, 76 bytes. Marks the mesh's header as animated (flag 0x20) when the animation
// has node animations. The original walks the node animations with a loop whose body does
// nothing in this build, so the loop is left out. Returns 1 on both exits, as the original
// does in r3; LoadRideAnimSet ignores the result.
int32_t RAInitType(tag_sMesh* mesh, tag_sAnimHeader* animation) {
    tag_sM3D2Header* header = mesh->mHeader;
    if (animation == nullptr || animation->mNodeAnimCount == 0) return 1;
    header->mFlags |= 0x20;
    return 1;
}

// The ride animation tick of the main app (SimTheme Park 2.04b3): what moves a ride set's time
// controls on and applies the animations they select to the nodes of the set's mesh. Eight
// functions of three ranges of the inventory (rsse.cpp, TPRide.cpp, TPEmbed.cpp):
//   TickRideAnimSet 0x100c6a20, DoAnimateRideSet 0x100c6c2c, RAAnimateTextureFrames 0x100c3678,
//   AnimateNode 0x100c45d4, GetAnimFrameData 0x100c34bc, IsMeshDrawEnabled 0x10065364,
//   TPProcessEmbeddedData 0x101ffb0c and DoWorldTransformsForEmbeddedData 0x101fef24.
// AnimateRideSet (src/RideTick.cpp) calls TickRideAnimSet, DoAnimateRideSet and
// TPProcessEmbeddedData; InstanceRide (src/RideTypes.cpp) calls the last as well.

namespace {

// 0x410ffffb: 9 less five units in the last place. AnimateNode spreads a rotation key's blend
// over eight weights with it, so that a fraction of 1.0 stays inside the last one.
constexpr float kKeyScale = 0x1.1ffff6p+3f;

// The frames a time control has run, in 30ths of a second: the milliseconds between its start
// word (mF10) and its current one (mF14), taken as an unsigned count, at the control's rate.
float RunFrames(const sAnimTimeControl* control) {
    const uint32_t elapsed = static_cast<uint32_t>(control->mF14) - static_cast<uint32_t>(control->mF10);
    return control->mF0C * ((30.0f * static_cast<float>(elapsed)) / 1000.0f);
}

// The word of TPGlobals' clock a control follows: 0x4018 with the control's flag 0x40, else 0x4010.
uint32_t ClockWord(const sAnimTimeControl* control) {
    return (control->mFlags & 0x40) == 0 ? gTPGlobals.mF4010 : gTPGlobals.mF4018;
}

// Brings a control's frame (mF20) up to `time`, the clock word read at the start of the step.
// Without flags 2 and 4 it runs on from its start word; with flag 4 it is set to the animation's
// length, the current word standing that many frames after the start; with flag 2 it is set to
// frame 0. (Twice in DoAnimateRideSet: before the test for the end and after a new animation.)
void SetControlFrame(sAnimTimeControl* control, uint32_t time) {
    if ((control->mFlags & 6) == 0) {
        control->mF14 = static_cast<int32_t>(time);
        control->mF18 = static_cast<int32_t>(time);
        control->mF20 = RunFrames(control);
        return;
    }
    if ((control->mFlags & 2) == 0) {
        const uint32_t frames =
            cw_cvt_fp2unsigned(static_cast<double>(((1000.0f * control->mF1C) / 30.0f) / control->mF0C));
        control->mF14 = static_cast<int32_t>(static_cast<uint32_t>(control->mF10) + frames);
        control->mF20 = control->mF1C;
    } else {
        control->mF14 = control->mF10;
        control->mF20 = 0.0f;
    }
    control->mF18 = static_cast<int32_t>(time);
}

// Applies an animation's node animations to the nodes they name and its animated textures to
// the mesh's textures (twice in DoAnimateRideSet).
void ApplyAnimation(sAnimTimeControl* control, tag_sMesh* mesh, tag_sAnimHeader* animation) {
    if (animation->mNodeAnimCount != 0) {
        tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
        for (uint32_t i = 0; i < animation->mNodeAnimCount; ++i, ++nodeAnim) {
            AnimateNode(control, mesh->mHeader, animation, nodeAnim->mNode.get(), nodeAnim);
        }
    }
    if ((animation->mF00 & 2) != 0 && (gTPGlobals.mEngineFlags & 8) == 0) {
        RAAnimateTextureFrames(mesh->mHeader, animation, control);
    }
}

}  // namespace

int32_t IsMeshDrawEnabled(tag_sMesh* mesh);

// 0x100c34bc, 372 bytes. See rsse.h. The key size follows from `type` (compared as a signed
// number: only 1, 2 and 4 are keys). The keys are scanned from the front while their frame is
// at or before the control's frame (mF20 as an unsigned count, saturating).
int32_t GetAnimFrameData(sAnimTimeControl* control, void* keys, uint32_t count, uint32_t type, int32_t wrap,
                         uint32_t* first, uint32_t* second, float* fraction) {
    uint32_t stride;
    switch (static_cast<int32_t>(type)) {
    case 1: stride = 4; break;
    case 2: stride = 0x14; break;
    case 4: stride = 0x10; break;
    default: return 0;
    }
    const float frameNow = control->mF20;
    const uint32_t frame = cw_cvt_fp2unsigned(static_cast<double>(frameNow));
    const uint8_t* bytes = static_cast<const uint8_t*>(keys);
    auto frameOf = [&](uint32_t i) {
        uint16_t value;
        std::memcpy(&value, bytes + stride * i, sizeof value);
        return static_cast<uint32_t>(value);
    };
    bool found = false;
    uint32_t last = 0;
    uint32_t lastFrame = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t key = frameOf(i);
        if (key > frame) break;
        found = true;
        last = i;
        lastFrame = key;
    }
    if (!found) return 0;
    uint32_t next;
    uint32_t nextFrame;
    if (last == count - 1) {
        if (wrap == 0) {
            next = last;
            nextFrame = frameOf(last) + 1;
        } else {
            next = 0;
            nextFrame = frameOf(0);
        }
    } else {
        next = last + 1;
        nextFrame = frameOf(next);
    }
    const float from = static_cast<float>(lastFrame);
    const float to = static_cast<float>(nextFrame);
    *fraction = (frameNow - from) / (to - from);
    *first = last;
    *second = next;
    return 1;
}

// 0x100c3678, 200 bytes. For each animated texture, the last of its (frame, value) pairs that
// the control's frame has reached becomes the texture's second 16-bit word, and a texture that
// changed gets flag 0x400 in its first.
void RAAnimateTextureFrames(tag_sM3D2Header* header, tag_sAnimHeader* animation, sAnimTimeControl* control) {
    const uint32_t frame = cw_cvt_fp2unsigned(static_cast<double>(control->mF20));
    const sAnimatedTexture* animated = animation->mAnimatedTextures.get();
    for (uint32_t t = animation->mAnimatedTextureCount; t != 0; --t, ++animated) {
        sTexture* texture = header->mTextures.get() + animated->mF00;
        const uint16_t* pairs = animated->mPairs.get();
        for (uint32_t p = animated->mPairCount; p != 0; --p) {
            const uint16_t* pair = pairs + 2 * (p - 1);
            if (frame < pair[0]) continue;
            if (texture->mF02 != pair[1]) {
                texture->mF02 = pair[1];
                texture->mF00 |= 0x400;
            }
            break;
        }
    }
}

// The node animation's interpolators of the main app (SimTheme Park 2.04b3): what AnimateNode
// (src/RideAnimTick.cpp) calls to turn an animation's keys, shapes and texture tracks into a
// node's matrix and texture coordinates. Fifteen leaf functions of the rsse.cpp and RideAnim*.cpp
// ranges, all float work:
//   shapes   RABezierInterFromShape 0x100c84ec, RABezierTangentFromShape 0x100c86e8,
//            RALinearInterFromShape 0x100c88e0, RALinearTangentFromShape 0x100c89c8,
//            RACatmulInterFromShape 0x100c8a8c, RAInterpPosnAtFrame 0x100c8ca8
//   matrices SetLMatrixOrientation 0x100c1eac, SetLMatrixScaleNoTrans 0x100c1ab8,
//            SetLMatrixXZOrientation 0x100c2ee8, CombineMatricesNoWDest2nd (s3DAxis) 0x100c37a4
//   rotation RACalcOrientationBetweenControlPoints 0x100c8054, RASphericalLinearInterpolate
//            0x100c7db0, RAQuaternionToLAxis 0x100c7ca0
//   textures RAAnimateTextureCoords 0x100c82d0, RAGetUVFrameData 0x100c81a0
// The CodeWarrior compiler fused many of the products and sums; each helper below says which
// instructions it stands for, and the fused ones go through include/ppc_math.h.

namespace {

// divwu, mullw and subf of the listing: a % n. The PowerPC leaves the quotient of a division by
// zero undefined; Unicorn answers 0 and so does this (the remainder is then a).
uint32_t UnsignedRemainder(uint32_t a, uint32_t n) {
    const uint32_t quotient = n != 0 ? a / n : 0;
    return a - quotient * n;
}

// The whole part of a shape's fraction and what is left of it: fctiwz (saturating), the whole
// number as a single, and the single-precision difference (the unfused fsubs of the listing).
struct SplitFraction {
    int32_t whole;
    float rest;
};

SplitFraction Split(float fraction) {
    const int32_t whole = ppc_fctiwz(static_cast<double>(fraction));
    return {whole, fraction - static_cast<float>(whole)};
}

// The points a shape function works on: the array it is given, else the shape's own.
const s3DPoint* PointsOf(const tag_sShape* shape, const s3DPoint* given) {
    return given != nullptr ? given : shape->mPoints.get();
}

// A point of an array by an index that the Mac works out in 32 bits (a wrapped negative index
// reaches back before the array).
const s3DPoint& PointAt(const s3DPoint* points, uint32_t index) { return points[static_cast<int32_t>(index)]; }

// The four points around `index` of a ring of `count` points (RABezier*, RACatmul*): the point
// before it, itself, and the two after it, each as (index + count + offset) % count. `base` is
// index + count, so that the point before cannot go below 0.
struct Ring {
    const s3DPoint* before;
    const s3DPoint* at;
    const s3DPoint* next;
    const s3DPoint* after;
};

Ring RingAround(const s3DPoint* points, uint32_t base, uint32_t count) {
    return {&PointAt(points, UnsignedRemainder(base - 1, count)), &PointAt(points, UnsignedRemainder(base, count)),
            &PointAt(points, UnsignedRemainder(base + 1, count)), &PointAt(points, UnsignedRemainder(base + 2, count))};
}

// One coordinate of RABezierInterFromShape: the cubic bezier segment that starts at b, with the
// control points a and c and the end d. They are the points at index, index - 1, index + 1 and
// index + 2 of the ring (AnimateNode passes the index 3 * segment + 1, so b is the segment's own
// first point). The listing's fused sequence, which builds the result on b.
float BezierCoordinate(float a, float b, float c, float d, float t) {
    const float threeA = 3.0f * a;
    float cubic = -6.0f * a;
    float quadratic = ppc_fmadds(-1.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, b, cubic);
    const float linear = ppc_fmadds(-3.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, c, cubic);
    quadratic = ppc_fmadds(-3.0f, c, quadratic);
    cubic = t * cubic;
    quadratic = d + quadratic;
    cubic = t * cubic;
    quadratic = t * quadratic;
    quadratic = t * quadratic;
    cubic = ppc_fmadds(t, quadratic, cubic);
    cubic = ppc_fmadds(t, linear, cubic);
    return b + cubic;
}

// One coordinate of RABezierTangentFromShape (same names).
float BezierSlope(float a, float b, float c, float d, float t) {
    const float threeA = 3.0f * a;
    float cubic = -6.0f * a;
    float quadratic = ppc_fmadds(-1.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, b, cubic);
    const float linear = ppc_fmadds(-3.0f, b, threeA);
    cubic = ppc_fmadds(3.0f, c, cubic);
    quadratic = ppc_fmadds(-3.0f, c, quadratic);
    cubic = 2.0f * cubic;
    quadratic = d + quadratic;
    cubic = t * cubic;
    quadratic = 3.0f * quadratic;
    quadratic = t * quadratic;
    cubic = ppc_fmadds(t, quadratic, cubic);
    return linear + cubic;
}

// One coordinate of RACatmulInterFromShape: p0 to p3 are the points before, at, after and two
// after the index.
float CatmullCoordinate(float p0, float p1, float p2, float p3, float t) {
    const float negP0 = -1.0f * p0;
    float even = -5.0f * p1;
    float odd = ppc_fmadds(3.0f, p1, negP0);
    even = ppc_fmadds(2.0f, p0, even);
    odd = ppc_fmadds(-3.0f, p2, odd);
    even = ppc_fmadds(4.0f, p2, even);
    const float diff = negP0 + p2;
    odd = p3 + odd;
    even = even - p3;
    odd = t * odd;
    even = t * even;
    odd = t * odd;
    even = t * even;
    even = ppc_fmadds(t, odd, even);
    even = ppc_fmadds(t, diff, even);
    even = ppc_fmadds(2.0f, p1, even);
    return 0.5f * even;
}

// The sine table of TPGlobals at the angle whose table index (angle * scale, truncated) is
// `scaled`; the index is and-ed with the mask, so a negative angle wraps.
float SineAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

constexpr float kHalfPi = 1.5707964f;   // 0x3fc90fdb

}  // namespace

// 0x100c37a4, 292 bytes. For each of the first three columns and rows 0 to 2 (no translation row
// and no fourth column): source row r (the r-th axis) times the destination's old column, as
// y * old[1] fused with x * old[0] and then z * old[2]. The old column is read before it is written.
void CombineMatricesNoWDest2nd(s3DAxis* source, sMatrix* destination) {
    const s3DPoint* const rows[3] = {&source->mX, &source->mY, &source->mZ};
    for (int32_t column = 0; column < 3; ++column) {
        const float old0 = destination->m[0][column];
        const float old1 = destination->m[1][column];
        const float old2 = destination->m[2][column];
        for (int32_t row = 0; row < 3; ++row) {
            float sum = rows[row]->y * old1;
            sum = ppc_fmadds(rows[row]->x, old0, sum);
            sum = ppc_fmadds(rows[row]->z, old2, sum);
            destination->m[row][column] = sum;
        }
    }
}

// The ride animation set's animation changes and the compressed morph decoder of the main app
// (SimTheme Park 2.04b3): what starts an animation on a time control and puts the set's mesh in
// the state that animation needs, and what turns a node's compressed morph into vertices. Nine
// functions of the rsse.cpp range of the inventory:
//   IsValidAnimation 0x100c5d3c, SetupAnimationFlags 0x100c5e18, ResetRideAnimationSingleChannel
//   0x100c5054, SetNewAnim 0x100c60fc, SetCurRideAnim 0x100c6670, InterpolateCMorphData
//   0x100c390c, InterpolateCMorphBoundData 0x100c3cac, RADecompressMorph 0x100c40dc and
//   ResetRideAnimationAllChannels 0x100c54ec.
// InstanceRide (src/RideTypes.cpp) and CLBMeshInstance::PlayAnim call SetCurRideAnim;
// AnimateRideSet (src/RideTick.cpp) and DoAnimateRideSet (src/RideAnimTick.cpp) call
// SetCurRideAnim, SetNewAnim, ResetRideAnimationSingleChannel and IsValidAnimation; AnimateNode
// (src/RideAnimTick.cpp) calls RADecompressMorph.
// The mesh file's records (nodes, animations, morphs) keep the file's layout with Ptr32 fields
// (include/m3d2.h); the set and its time controls are native objects (include/TPEngine.h).

namespace {

constexpr uint32_t kNoKind = 0xc;   // a control with no animation; also "no group" to the checks below
constexpr float kMillisecondsPerFrame = 0x1.0aaaaap+5f;   // 0x42055555 (33.333332): SetCurRideAnim's time left

// True when kind has entries (group flags 1 or 2) and the index is below their count. The original
// inlines this test in SetCurRideAnim (four times) and SetNewAnim (twice); IsValidAnimation is its
// out-of-line copy.
bool EntryExists(const tag_sRideAnimSet* set, uint32_t kind, uint32_t index) {
    if (kind == kNoKind) return false;
    const sRideAnimGroup& group = set->mGroups[kind];
    return (group.mFlags & 3) != 0 && group.mCount > index;
}

// The node of a header with the given place: a full node (0xa0 bytes) below the header's node
// count, a mini node (0x58 bytes) after them. The mini node's record is read as a full node by
// the callers that touch words past 0x58 (the original does the same).
tag_sNode* NodeAt(tag_sM3D2Header* header, uint32_t index) {
    if (index < header->mNodeCount) return header->mNodes.get() + index;
    return reinterpret_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
}

// The length of an animation in milliseconds: its frames (mF08 less mF04, an unsigned count) at 30 a
// second.
uint32_t AnimationMilliseconds(const tag_sAnimHeader* animation) {
    const float frames = static_cast<float>(static_cast<uint32_t>(animation->mF08 - animation->mF04));
    return cw_cvt_fp2unsigned(static_cast<double>((1000.0f * frames) / 30.0f));
}

// The three 10-bit signed fields of a compressed vertex word (the loader leaves them at the top of the word,
// src/m3d2_load.cpp): x in bits 31..22, y in 21..12, z in 11..2.
float FieldX(uint32_t word) { return static_cast<float>(static_cast<int32_t>(word) >> 22); }
float FieldY(uint32_t word) { return static_cast<float>(static_cast<int32_t>(word << 10) >> 22); }
float FieldZ(uint32_t word) { return static_cast<float>(static_cast<int32_t>(word << 20) >> 22); }

// 0x100c40dc's three searches: moves the key data's current row (mF10) on while the frame of the row after it
// is below `frame`, then answers how far `frame` lies from the row's frame to the next one. Nothing bounds the
// search (the original runs on into whatever follows the frame table).
float KeyFraction(sCMKeyData* key, float frame) {
    const uint16_t* frames = key->mF08.get();
    while (static_cast<float>(frames[key->mF10 + 1]) < frame) key->mF10 += 1;
    const float first = static_cast<float>(frames[key->mF10]);
    const float second = static_cast<float>(frames[key->mF10 + 1]);
    return (frame - first) / (second - first);
}

// The key data's vertex words: a row of mKeyCount of them per frame (the header names the two counts the other
// way round: word 2 of the record is the number of vertices in a row, and the size of the table of vertex
// numbers at mF04), the current row at mF10.
const uint32_t* CurrentRow(const sCMKeyData* key) {
    const uint32_t count = key->mKeyCount;
    return reinterpret_cast<const uint32_t*>(key->mVertices.get()) + static_cast<uint32_t>(key->mF10 * count);
}

}  // namespace

// 0x100c45d4, 2,008 bytes. See rsse.h. Each block is switched on by a flag of the node animation
// (nodeAnim->mFlags): 0x20000 the enable list, 0x1000 (without 0x4000) the compressed morph,
// 8 the rotation keys, 0x80 the scale keys, 1 or 0x200 the position along a shape or a path (with
// 0x400 the heading along it), and 0x10000 the texture coordinates. TPGlobals' words 0x401c and
// 0x4020 take the rotation track's key count and the shape's point count as they are used.
int32_t AnimateNode(sAnimTimeControl* control, tag_sM3D2Header* header, tag_sAnimHeader* animation, tag_sNode* node,
                    tag_sNodeAnim* nodeAnim) {
    if (control->mF20 > control->mF1C) return 0;
    const uint32_t frame = cw_cvt_fp2unsigned(static_cast<double>(control->mF20));
    // The node is hidden (0x10) from the last enable entry at or before the frame, when it is
    // negative or 0, and shown when positive. An entry's frame is its magnitude as a 16-bit
    // number, sign-extended and compared as unsigned (so -32768 is a frame of 0xffff8000).
    if ((nodeAnim->mFlags & 0x20000) != 0) {
        const int16_t* enables = nodeAnim->mEnables.get();
        const int16_t* found = nullptr;
        for (uint32_t left = nodeAnim->mEnableCount; left != 0; --left) {
            const int16_t* entry = enables + (left - 1);
            const int16_t magnitude = *entry < 0 ? static_cast<int16_t>(-static_cast<int32_t>(*entry)) : *entry;
            if (frame >= static_cast<uint32_t>(static_cast<int32_t>(magnitude))) {
                found = entry;
                break;
            }
        }
        if (found != nullptr) {
            if (*found > 0) node->mFlags &= ~0x10u;
            else node->mFlags |= 0x10;
        }
    }
    if ((nodeAnim->mFlags & 0x1000) != 0 && (nodeAnim->mFlags & 0x4000) == 0) {
        RADecompressMorph(node, nodeAnim->mMorph.get(), control->mF20, animation->mF08, header->mFlags & 4);
        header->mFlags |= 0x40000;
    }
    uint32_t first = 0;
    uint32_t second = 0;
    float fraction = 0.0f;
    // Rotation: the key at or before the frame blended with the next, the blend between them
    // shaped by the key's table of eight weights (mF02 the table's record, 0xffff for none).
    if ((nodeAnim->mFlags & 8) != 0 &&
        GetAnimFrameData(control, nodeAnim->mQuatKeys.get(), nodeAnim->mF10, 2, 1, &first, &second, &fraction) != 0) {
        gTPGlobals.mF401C = nodeAnim->mF10;
        const sQuatKey& key = nodeAnim->mQuatKeys.get()[first];
        if (key.mF02 != 0xffff) {
            const float scaled = kKeyScale * fraction;
            const uint8_t* weights = nodeAnim->mF34.get() + static_cast<uint32_t>(key.mF02) * 8;
            const uint32_t step = cw_cvt_fp2unsigned(static_cast<double>(scaled));
            const float within = scaled - static_cast<float>(step);
            float low;
            float high;
            if (step == 0) {
                low = 0.0f;
                high = static_cast<float>(weights[0]) / 255.0f;
            } else if (step < 8) {
                low = static_cast<float>(weights[step - 1]) / 255.0f;
                high = static_cast<float>(weights[step]) / 255.0f;
            } else {
                low = static_cast<float>(weights[7]) / 255.0f;
                high = 1.0f;
            }
            fraction = ppc_fmadds(1.0f - within, low, within * high);
        }
        s3DAxis axis;
        RACalcOrientationBetweenControlPoints(nodeAnim->mQuatKeys.get(), first, fraction, &axis);
        if ((header->mFlags & 4) != 0) CombineMatricesNoWDest2nd(&axis, &node->mMatrix);
        else SetLMatrixOrientation(&node->mMatrix, &axis.mX, &axis.mY, &axis.mZ);
    }
    // Scale: the two keys around the frame, blended.
    if ((nodeAnim->mFlags & 0x80) != 0 &&
        GetAnimFrameData(control, nodeAnim->mScaleKeys.get(), nodeAnim->mF12, 4, 0, &first, &second, &fraction) != 0) {
        const sScaleKey& keyA = nodeAnim->mScaleKeys.get()[first];
        const sScaleKey& keyB = nodeAnim->mScaleKeys.get()[second];
        const float rest = 1.0f - fraction;
        float x = ppc_fmadds(rest, keyA.mF04[0], fraction * keyB.mF04[0]);
        float y = ppc_fmadds(rest, keyA.mF04[1], fraction * keyB.mF04[1]);
        float z = ppc_fmadds(rest, keyA.mF04[2], fraction * keyB.mF04[2]);
        SetLMatrixScaleNoTrans(&node->mMatrix, x, y, z);
    }
    // Position: along the node animation's shape (flag 1, the frame found among the shape's
    // frame pairs) or, for a node under a parent that is moving along one (flag 0x200 and the
    // parent's flag 8), along the header's shape that the parent names, at the place the node
    // animation's path controller gives for the frame (its mF00 the first frame and mWordCount
    // the length, both also the path's size).
    tag_sNode* parent = node->mF04.get();
    const uint32_t flags = nodeAnim->mFlags;
    if (((flags & 0x200) != 0 && parent != nullptr && (parent->mFlags & 8) != 0) || (flags & 1) != 0) {
        tag_sShape* shape = nullptr;
        bool onShape = false;
        if ((flags & 1) != 0) {
            shape = nodeAnim->mShapes.get();
            onShape = GetAnimFrameData(control, shape->mWords.get(), shape->mWordPairs, 1, 0, &first, &second, &fraction) != 0;
            if (onShape && (shape->mF00 & 2) != 0) first = first * 3 + 1;
        } else {
            sPathController* path = nodeAnim->mPath.get();
            const float now = control->mF20;
            const float start = static_cast<float>(path->mF00);
            if (now >= start && now <= start + static_cast<float>(static_cast<uint32_t>(path->mWordCount))) {
                onShape = true;
                shape = header->mShapes.get() + parent->mF52;
                float place;
                RAInterpPosnAtFrame(reinterpret_cast<float*>(path->mWords.get()), static_cast<uint32_t>(path->mWordCount),
                                    now - start, &place, shape->mF00 & 1);
                place = static_cast<float>(std::fmod(1000.0 + static_cast<double>(place), 100.0));
                const uint32_t points = (shape->mF00 & 2) != 0 ? shape->mPointCount / 3u : shape->mPointCount;
                float scaled;
                if ((shape->mF00 & 1) != 0) scaled = (static_cast<float>(points) * place) / 100.0f;
                else scaled = ((static_cast<float>(points) - 1.0f) * place) / 100.0f;
                const uint32_t whole = cw_cvt_fp2unsigned(static_cast<double>(scaled));
                fraction = scaled - static_cast<float>(whole);
                first = (shape->mF00 & 2) != 0 ? whole * 3 + 1 : whole;
            }
        }
        if (onShape) {
            gTPGlobals.mF4020 = shape->mPointCount;
            s3DPoint at;
            if ((shape->mF00 & 2) != 0) RABezierInterFromShape(shape, nullptr, first, fraction, &at);
            else if ((shape->mF00 & 8) != 0) RALinearInterFromShape(shape, nullptr, first, fraction, &at);
            else RACatmulInterFromShape(shape, nullptr, first, fraction, &at);
            if ((header->mFlags & 4) != 0) {
                node->mMatrix.m[3][0] = node->mMatrix.m[3][0] + at.x;
                node->mMatrix.m[3][1] = node->mMatrix.m[3][1] + at.y;
                node->mMatrix.m[3][2] = node->mMatrix.m[3][2] + at.z;
            } else {
                node->mMatrix.m[3][0] = at.x;
                node->mMatrix.m[3][1] = at.y;
                node->mMatrix.m[3][2] = at.z;
            }
            // Heading: the shape's tangent at the place, made a unit vector (the length is
            // worked out in double precision, the squares of y and z rounded to single first).
            if ((flags & 0x400) != 0) {
                s3DPoint tangent;
                if ((shape->mF00 & 2) != 0) {
                    RABezierTangentFromShape(shape, nullptr, first, fraction, &tangent);
                } else if ((shape->mF00 & 8) != 0) {
                    RALinearTangentFromShape(shape, nullptr, first, fraction, &tangent);
                } else {
                    RACatmulInterFromShape(shape, nullptr, first, 0.1f + fraction, &tangent);
                }
                const double xx = static_cast<double>(tangent.x) * static_cast<double>(tangent.x);
                const double yy = static_cast<double>(tangent.y * tangent.y);
                const double zz = static_cast<double>(tangent.z * tangent.z);
                const float length = static_cast<float>(std::sqrt(zz + (xx + yy)));
                tangent.x = tangent.x / length;
                tangent.y = tangent.y / length;
                tangent.z = tangent.z / length;
                if ((flags & 8) == 0) {
                    node->mMatrix.m[1][0] = 0.0f;
                    node->mMatrix.m[1][1] = 1.0f;
                    node->mMatrix.m[1][2] = 0.0f;
                }
                SetLMatrixXZOrientation(&node->mMatrix, &tangent);
            }
        }
    }
    if ((((gTPGlobals.mEngineFlags & 0x10) == 0 && (node->mFlags & 0x4000) == 0) || (header->mFlags & 4) != 0) &&
        (nodeAnim->mFlags & 0x10000) != 0) {
        RAAnimateTextureCoords(control, nodeAnim, header->mFlags & 4);
    }
    return 1;
}

// RATick 0x100c4e20 and QuickCopyUnmaskedMeshVertices 0x100c4f18 (unit v5308).

// 0x100c4e20, 172 bytes: runs every node animation of `animation` on the mesh's header at the control's frame, then
// (animation flag 2, engine flag 8 clear) steps its animated textures. Always answers 1. A NULL animation does nothing.
int32_t RATick(sAnimTimeControl* control, tag_sMesh* mesh, tag_sAnimHeader* animation) {
    if (animation != nullptr) {
        if (animation->mNodeAnimCount != 0) {
            tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
            for (int32_t i = 0; i < static_cast<int32_t>(animation->mNodeAnimCount); ++i, ++nodeAnim) {
                AnimateNode(control, mesh->mHeader, animation, nodeAnim->mNode.get(), nodeAnim);
            }
        }
        if ((animation->mF00 & 2) != 0 && (gTPGlobals.mEngineFlags & 8) == 0) {
            RAAnimateTextureFrames(mesh->mHeader, animation, control);
        }
    }
    return 1;
}

// 0x100c4f18, 240 bytes: for each of `to`'s full nodes (the count of `to`'s header) that has vertices (mVertexCount)
// and either flag 0x200000 or `force` nonzero: copies `from`'s node's vertices (when both have a vertex block and
// the SOURCE node's (mVertexCount + 3) >> 2 sets are not zero) and always the bounding box. Both meshes' node arrays
// are walked in step.
void QuickCopyUnmaskedMeshVertices(tag_sMesh* from, tag_sMesh* to, int32_t force) {
    tag_sNode* dest = to->mHeader->mNodes.get();
    tag_sNode* source = from->mHeader->mNodes.get();
    for (uint32_t left = to->mHeader->mNodeCount; left != 0; --left, ++dest, ++source) {
        if (dest->mVertexCount == 0) continue;
        if ((dest->mFlags & 0x200000) == 0 && force == 0) continue;
        if (dest->mVertices != nullptr && source->mVertices != nullptr) {
            int32_t sets = (static_cast<int32_t>(source->mVertexCount) + 3) >> 2;
            if (sets != 0) std::memcpy(dest->mVertices.get(), source->mVertices.get(), static_cast<size_t>(sets) * 0x30);
        }
        std::memcpy(&dest->mBox, &source->mBox, sizeof(sAlignedBox));
    }
}

// 0x100c5054, 1,068 bytes. `animation` is the one that has been playing, `other` the one that takes over (NULL
// for none). In the set's mesh (header H; the type's mesh, which the set's mSelf holds, is header T):
//  (1) With set flag 8 every node loses 0x10; else, with set flag 0x100000, the engine flags are taken as having
//      bit 1 for the rest of the call (a local copy).
//  (2) The nodes `animation` disables lose 0x10 (unless the top bit is set), and so does every node it animates
//      (after the node lost the animation's flags: 0x800000 with mesh header flag 4, else 0xf40000).
//  (3) The scratch geometry buffer (a word per node) is cleared and keeps, for each node `animation` animates, that
//      node animation's flags; `other`'s node animations then clear their flags from it.
//  (4) Unless (local) engine flag 1 is set and the header has no flag 4: each node `animation` animates whose
//      remaining flags include 0x289 gets its rotation, scale and position rows back from T's node (the 3 x 3 of
//      the matrix and its translation, the fourth column stays), with 0x1000 its vertices and bounding box, with
//      0x10000 its texture coordinates.
//  (5) If `animation` has flag 2 and the engine flags lack 8, every texture of H with 0x40 gets 0x400 and its
//      second word cleared.
void ResetRideAnimationSingleChannel(tag_sRideAnimSet* set, tag_sAnimHeader* animation, tag_sAnimHeader* other) {
    const uint32_t* engineFlagsWord = &gTPGlobals.mEngineFlags;
    uint32_t engineFlags = gTPGlobals.mEngineFlags;
    uint32_t* scratch = static_cast<uint32_t*>(gScratchGeomBuffer);
    tag_sM3D2Header* typeHeader = set->mSelf->mHeads->mHeader;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    const bool setFlag8 = (set->mFlags & 8) != 0;
    if (setFlag8 || (set->mFlags & 0x100000) != 0) {
        if (!setFlag8) {
            engineFlags |= 1;
        } else {
            for (uint32_t i = 0; i < header->mAllNodeCount; ++i) NodeAt(header, i)->mFlags &= ~0x10u;
        }
    }
    const uint32_t cleared = (header->mFlags & 4) != 0 ? 0x800000 : 0xf40000;
    const uint16_t* disabled = animation->mNodesToDisable.get();
    for (uint32_t left = animation->mDisableCount; left != 0; --left, ++disabled) {
        tag_sNode* node = NodeAt(header, *disabled);
        if ((node->mFlags & 0x80000000) == 0) node->mFlags &= ~0x10u;
    }
    std::memset(scratch, 0, static_cast<uint32_t>(header->mAllNodeCount) << 2);
    tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
    for (uint32_t left = animation->mNodeAnimCount; left != 0; --left, ++nodeAnim) {
        scratch[nodeAnim->mF14] = nodeAnim->mFlags;
        tag_sNode* node = NodeAt(header, nodeAnim->mF14);
        node->mFlags &= ~cleared;
        if ((node->mFlags & 0x80000000) == 0) node->mFlags &= ~0x10u;
    }
    if ((engineFlags & 1) == 0 || (header->mFlags & 4) != 0) {
        if (other != nullptr) {
            tag_sNodeAnim* otherAnim = other->mNodeAnims.get();
            for (uint32_t left = other->mNodeAnimCount; left != 0; --left, ++otherAnim) {
                scratch[otherAnim->mF14] &= ~otherAnim->mFlags;
            }
        }
        nodeAnim = animation->mNodeAnims.get();
        for (uint32_t left = animation->mNodeAnimCount; left != 0; --left, ++nodeAnim) {
            const uint32_t index = nodeAnim->mF14;
            tag_sNode* node = NodeAt(header, index);
            tag_sNode* source = NodeAt(typeHeader, index);
            const uint32_t flags = scratch[index];
            if ((flags & 0x289) != 0) {
                // The matrix without its fourth column (twelve words, copied as bits).
                for (int row = 0; row < 4; ++row) {
                    std::memcpy(node->mMatrix.m[row], source->mMatrix.m[row], 3 * sizeof(float));
                }
            }
            if ((flags & 0x1000) != 0) {
                if (node->mVertices != nullptr && source->mVertices != nullptr &&
                    ((node->mVertexCount + 3) >> 2) != 0) {
                    std::memcpy(node->mVertices.get(), source->mVertices.get(),
                                static_cast<size_t>((node->mVertexCount + 3) >> 2) * sizeof(sVertexPosnSet));
                }
                std::memcpy(&node->mBox, &source->mBox, sizeof(sAlignedBox));
            }
            if ((flags & 0x10000) != 0 && node->mUVs != nullptr && source->mUVs != nullptr &&
                ((node->mF5E + 3) >> 2) != 0) {
                std::memcpy(node->mUVs.get(), source->mUVs.get(),
                            static_cast<size_t>((node->mF5E + 3) >> 2) * sizeof(sUVSet));
            }
        }
    }
    if ((animation->mF00 & 2) != 0 && (*engineFlagsWord & 8) == 0) {
        sTexture* texture = header->mTextures.get();
        for (uint32_t left = header->mTextureCount; left != 0; --left, ++texture) {
            if ((texture->mF00 & 0x40) != 0) {
                texture->mF00 |= 0x400;
                texture->mF02 = 0;
            }
        }
    }
}

// 0x100c5a90, 236 bytes. Sets a ride animation's time control. With no animation, the length
// (mF1C), mF20 and mF10 are cleared. Otherwise the length is the animation's frame range
// (mF08 - mF04, as unsigned). Mode 0 converts the limit, clamped to the length, into a count
// of frames: ((1000 * limit) / 30) / the control's rate, rounded by cw_cvt_fp2unsigned. mF10
// then becomes mF14 less that count, and mF18 follows mF10. Any other mode sets mF10, mF14 and
// mF18 to TPGlobals' word 0x4010, or 0x4018 when flag 0x40 is set.
void ResetAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation, int32_t mode, float limit) {
    if (animation == nullptr) {
        control->mF1C = 0.0f;
        control->mF20 = 0.0f;
        control->mF10 = 0;
        return;
    }
    control->mF1C = static_cast<float>(static_cast<double>(static_cast<uint32_t>(animation->mF08 - animation->mF04)));
    if (mode == 0) {
        if (control->mF1C < limit) limit = control->mF1C;
        const uint32_t frames = cw_cvt_fp2unsigned(static_cast<double>(((1000.0f * limit) / 30.0f) / control->mF0C));
        control->mF10 = static_cast<int32_t>(static_cast<uint32_t>(control->mF14) - frames);
        control->mF18 = control->mF10;
    } else {
        const uint32_t word = (control->mFlags & 0x40) == 0 ? gTPGlobals.mF4010 : gTPGlobals.mF4018;
        control->mF10 = static_cast<int32_t>(word);
        control->mF18 = control->mF10;
        control->mF14 = control->mF10;
    }
}

// 0x100c5bcc, 92 bytes. With a time, both mF14 and mF18 take it; then mF20 = rate * ((30 * frames) / 1000)
// where frames is the unsigned mF14 - mF10.
void GetAnimTimeStats(sAnimTimeControl* control, uint32_t* time) {
    if (time != nullptr) {
        control->mF14 = static_cast<int32_t>(*time);
        control->mF18 = static_cast<int32_t>(*time);
    }
    const float frames = static_cast<float>(static_cast<uint32_t>(control->mF14 - control->mF10));
    const float scaled = 30.0f * frames;
    control->mF20 = control->mF0C * (scaled / 1000.0f);
}

// 0x100c5c68, 132 bytes. With an animation, mF1C is its frame range (unsigned) and mF20 is
// ((30 * elapsed frames) / 1000) / rate. Without, mF1C and mF20 are cleared, and mF10.
void RestoreAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation) {
    if (animation == nullptr) {
        control->mF1C = 0.0f;
        control->mF20 = 0.0f;
        control->mF10 = 0;
        return;
    }
    control->mF1C = static_cast<float>(static_cast<uint32_t>(animation->mF08 - animation->mF04));
    const float frames = static_cast<float>(static_cast<uint32_t>(control->mF14 - control->mF10));
    const float scaled = 30.0f * frames;
    const float seconds = scaled / 1000.0f;
    control->mF20 = seconds / control->mF0C;
}

// 0x100c5d3c, 60 bytes.
int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b) {
    return EntryExists(set, a, b) ? 1 : 0;
}

float GetAnimRemainingMilliseconds(sAnimTimeControl* control) {
    const float kMillisecondsPerFrame = 33.333332f;   // 0x10264e50: 0x42055555
    float left = control->mF1C - control->mF20;
    return kMillisecondsPerFrame * left;
}

// 0x100c5e18, 372 bytes. Every node animation marks its node 0x40000 (it is animated), 0x200000 for the node
// animation's flag 0x1000, 0x400000 for 0x10000 and 0x100000 for any of 0x289. Then each node the animation
// disables is flagged 0x10 (unless its top bit is set): with the mesh's flag 4 the numbers in the list name the
// nodes by their own number (word 0x50, the first full or mini node that has it), else by their place.
void SetupAnimationFlags(tag_sAnimHeader* animation, tag_sMesh* mesh) {
    tag_sM3D2Header* header = mesh->mHeader;
    tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
    for (uint32_t left = animation->mNodeAnimCount; left != 0; --left, ++nodeAnim) {
        uint32_t* flags = &nodeAnim->mNode.get()->mFlags;
        *flags |= 0x40000;
        if ((nodeAnim->mFlags & 0x1000) != 0) *flags |= 0x200000;
        if ((nodeAnim->mFlags & 0x10000) != 0) *flags |= 0x400000;
        if ((nodeAnim->mFlags & 0x289) != 0) *flags |= 0x100000;
    }
    if (animation->mNodesToDisable == nullptr) return;
    const uint16_t* disabled = animation->mNodesToDisable.get();
    for (int32_t i = 0; i < static_cast<int32_t>(animation->mDisableCount); ++i) {
        if ((mesh->mF00 & 4) != 0) {
            for (int32_t n = 0; n < static_cast<int32_t>(header->mAllNodeCount); ++n) {
                tag_sNode* node = NodeAt(header, static_cast<uint32_t>(n));
                if (node->mF50 == disabled[i]) {
                    if ((node->mFlags & 0x80000000) == 0) node->mFlags |= 0x10;
                    break;
                }
            }
        } else {
            tag_sNode* node = NodeAt(header, disabled[i]);
            if ((node->mFlags & 0x80000000) == 0) node->mFlags |= 0x10;
        }
    }
}

// 0x100c5fd4, 136 bytes. Counts the time controls whose kind (mF04) is below 0xc into mAnimatedCount,
// and sets flag 0x80000 of the mesh header's flags when there is any (clears it first).
void GetNumActiveChannels(tag_sRideAnimSet* set) {
    set->mHeads->mHeader->mFlags &= ~0x80000u;
    set->mAnimatedCount = 0;
    for (uint32_t i = 0; i < set->mTimeCount; i++) {
        const sAnimTimeControl* control = reinterpret_cast<const sAnimTimeControl*>(static_cast<char*>(set->mTimeControls) + i * 0x38);
        set->mAnimatedCount += (control->mF04 < 0xc) ? 1 : 0;
    }
    if (set->mAnimatedCount == 0) return;
    set->mHeads->mHeader->mFlags |= 0x80000u;
}

// A ride's working copy of its animation set (the main app's TPRide.cpp, TPEmbed.cpp, rsse.cpp and
// ui_ErrorDisplay.cpp ranges, as the inventory files them): InstanceRideAnimSet and what it calls
// (InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its sibling), and the
// small functions LoadRideType calls on a loaded set (CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads,
// GetFontPath, OverrideActiveChannels) with ERRORBox.
//
// The copies are made the way the original makes them: one block from the allocator (MReleaseAllocF), the
// pieces laid after each other in it, then every pointer that led into the original's tables made to lead
// into the copies. The mesh and animation records keep the file's layout (Ptr32 fields, arena offsets), so
// the original's arithmetic on addresses is arithmetic on offsets here. The game's own records (tag_sMesh,
// the embedded set and its records, tag_sRideAnimSet and its entries, tag_sDMeshSet) have native pointers
// and are larger than on the Mac, so the block's fixed parts are larger too (see InstanceMesh).

// 0x100c609c, 32 bytes. Marks a set as animating through its channels: the word at 0x14 becomes 1 (a pointer
// field in the game's other uses, hence the cast) and the mesh header gets flag 0x80000.
void OverrideActiveChannels(tag_sRideAnimSet* set) {
    set->mF14 = reinterpret_cast<void*>(1);
    set->mHeads->mHeader->mFlags |= 0x80000;
}

// 0x100c60fc, 1,208 bytes. Answers the length in milliseconds of the animation the control ends up on (0 for
// none). Kinds 0xd and 0xe are not animations: 0xd holds the control at frame 0 and 0xe at the end of the current
// animation; they leave the control's kind and index alone. Any other kind selects the animation; kind 0xc (or a
// kind whose group has no entries) switches the control off. Quirks kept: the entry is taken from the group
// without comparing the index with the group's count (only the "has entries" test is made, the count test
// decides the clock mode); the stale limit of a mode 1 call is not passed on (ResetAnimTimeStats ignores it).
uint32_t SetNewAnimDuration(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control,
                            uint32_t flags, float speed) {
    control->mFlags = (control->mFlags & ~1u) | (flags & 1);
    if ((flags & 4) != 0) control->mFlags |= 0x20;
    else control->mFlags &= ~0x20u;
    const bool special = kind - 0xd <= 1;
    int32_t mode = 1;    // 0: the clock follows the old animation's position, 1: from now
    float limit = 0.0f;  // the position (frames past the old length) mode 0 starts from
    if (control->mF04 != kNoKind) {
        if (special) {
            control->mFlags |= kind == 0xd ? 2 : 0x14;
        } else if (EntryExists(set, kind, index)) {
            const float frame = control->mF20;
            const float length = control->mF1C;
            if (frame > length) {
                limit = frame - length;
                mode = 0;
            }
            control->mFlags &= ~6u;
        } else {
            mode = 0;
        }
    }
    bool applied = false;
    if (!special) {
        const bool hasGroup = kind != kNoKind && (set->mGroups[kind].mFlags & 3) != 0;
        tag_sAnimHeader* animation = hasGroup ? set->mGroups[kind].mEntries[index].mAnimation : nullptr;
        if (control->mF04 != kNoKind && (flags & 0xc) == 0) {
            tag_sAnimHeader* old = set->mGroups[control->mF04].mEntries[control->mF08].mAnimation;
            ResetRideAnimationSingleChannel(set, old, animation);
        }
        if (hasGroup) {
            control->mF04 = kind;
            control->mF08 = index;
            control->mF0C = speed;
            ResetAnimTimeStats(control, animation, mode, limit);
            if ((set->mFlags & 0x8000) == 0) control->mF20 = RunFrames(control);
            applied = true;
            if ((flags & 8) == 0) SetupAnimationFlags(animation, set->mHeads);
        } else {
            control->mF04 = kNoKind;
        }
    }
    if (!applied) {
        if (!special) {
            control->mF10 = 0;
            control->mF14 = 0;
            control->mF18 = 0;
            control->mF0C = 1.0f;
            control->mF20 = RunFrames(control);
        } else {
            const uint32_t clock = (control->mFlags & 0x40) == 0 ? gTPGlobals.mF4010 : gTPGlobals.mF4018;
            control->mF10 = static_cast<int32_t>(clock);
            if ((control->mFlags & 2) == 0) {
                const uint32_t frames = cw_cvt_fp2unsigned(
                    static_cast<double>(((1000.0f * control->mF1C) / 30.0f) / control->mF0C));
                control->mF14 = static_cast<int32_t>(static_cast<uint32_t>(control->mF10) + frames);
                control->mF20 = control->mF1C;
            } else {
                control->mF14 = control->mF10;
                control->mF20 = 0.0f;
            }
            control->mF18 = control->mF10;
        }
    }
    if ((set->mFlags & 0x1000000) == 0) {
        tag_sM3D2Header* header = set->mHeads->mHeader;
        header->mFlags &= 0xfff7ffffu;
        set->mAnimatedCount = 0;
        const sAnimTimeControl* controls = static_cast<const sAnimTimeControl*>(set->mTimeControls);
        for (uint32_t i = 0; i < set->mTimeCount; ++i) {
            if (controls[i].mF04 < kNoKind) set->mAnimatedCount += 1;
        }
        if (set->mAnimatedCount != 0) header->mFlags |= 0x80000;
    }
    if (EntryExists(set, control->mF04, control->mF08)) {
        return AnimationMilliseconds(set->mGroups[control->mF04].mEntries[control->mF08].mAnimation);
    }
    return 0;
}

void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags,
                float speed) {
    SetNewAnimDuration(set, kind, index, control, flags, speed);
}

// 0x100c6670, 692 bytes. Answers the time in milliseconds the control's animations will take: when the new one is
// queued behind the one playing, what the playing one has left (33.333332 per frame, a signed 32-bit count) and
// the new one's length (1000 when the new one does not exist); else SetNewAnim's answer (1000 for none).
// Quirk kept (found in the listing): the test for the playing animation's existence is made again after the
// queueing; it cannot fail then, so its other branch is not translated.
uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed,
                                uint32_t b) {
    sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls) + b;
    if ((set->mFlags & 8) != 0 && (set->mHeads->mF00 & 1) == 0 && a != kNoKind) {
        EnableMeshDraw(set->mHeads, set->mMatrix, 1);
    }
    const bool playing = EntryExists(set, control->mF04, control->mF08);
    const bool notPastTheEnd = control->mF20 <= control->mF1C;   // false for a NaN
    if (playing && notPastTheEnd && (control->mFlags & 6) == 0 && a - 0xd > 1 && (loop & 2) == 0) {
        control->mF24 = a;
        control->mF28 = anim;
        control->mF2C = loop;
        control->mF30 = speed;
        const float left = kMillisecondsPerFrame * (control->mF1C - control->mF20);
        uint32_t result = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(left)));
        if (EntryExists(set, a, anim)) result += AnimationMilliseconds(set->mGroups[a].mEntries[anim].mAnimation);
        else result += 1000;
        return result;
    }
    if ((loop & 2) != 0) {
        control->mF24 = kNoKind;
        control->mF28 = 0;
        control->mF30 = 0.0f;
    }
    const uint32_t result = SetNewAnimDuration(set, a, anim, control, loop, speed);
    return result == 0 ? 1000 : result;
}

// 0x100c390c, 832 bytes. For each vertex of the key data (its number in the table at mF04: the set of four is
// the number shifted right by 2, the lane its low two bits) the point between the current row and the next at
// the fraction t: each row's field times the morph's scale (mF20) plus its offset (mF14), blended
// row * (1 - t) + next * t with the second product rounded first. `add` nonzero adds it to the point.
void InterpolateCMorphData(sMorphSet* morph, sVertexPosnSet* positions, sCMKeyData* key, float t, int32_t add) {
    const uint32_t count = key->mKeyCount;
    const uint16_t* numbers = key->mF04.get();
    const uint32_t* row = CurrentRow(key);
    const uint32_t* next = row + count;
    const float rest = 1.0f - t;
    const s3DPoint& offset = morph->mF14;
    const s3DPoint& scale = morph->mF20;
    for (uint32_t left = count; left != 0; --left) {
        const uint32_t number = *numbers++;
        const uint32_t from = *row++;
        const uint32_t to = *next++;
        sVertexPosnSet& set = positions[number >> 2];
        const uint32_t lane = number & 3;
        const float x = ppc_fmadds(ppc_fmadds(FieldX(from), scale.x, offset.x), rest,
                                   ppc_fmadds(FieldX(to), scale.x, offset.x) * t);
        const float y = ppc_fmadds(ppc_fmadds(FieldY(from), scale.y, offset.y), rest,
                                   ppc_fmadds(FieldY(to), scale.y, offset.y) * t);
        const float z = ppc_fmadds(ppc_fmadds(FieldZ(from), scale.z, offset.z), rest,
                                   ppc_fmadds(FieldZ(to), scale.z, offset.z) * t);
        if (add == 0) {
            set.x[lane] = x;
            set.y[lane] = y;
            set.z[lane] = z;
        } else {
            set.x[lane] = set.x[lane] + x;
            set.y[lane] = set.y[lane] + y;
            set.z[lane] = set.z[lane] + z;
        }
    }
}

// 0x100c3cac, 976 bytes. The key data's current row and the next hold two words each: the corners of the node's
// box. Each is blended like a vertex (see InterpolateCMorphData); the minimum is then widened by the scale and
// by a quarter, the maximum likewise the other way. `add` nonzero adds the corners to what the box holds.
void InterpolateCMorphBoundData(sMorphSet* morph, sAlignedBox* box, sCMKeyData* key, float t, int32_t add) {
    const uint32_t count = key->mKeyCount;
    const uint32_t* row = CurrentRow(key);
    const uint32_t* next = row + count;
    const float rest = 1.0f - t;
    const s3DPoint& offset = morph->mF14;
    const s3DPoint& scale = morph->mF20;
    constexpr float kQuarter = 0.25f;
    const float lowX = ppc_fmadds(ppc_fmadds(FieldX(row[0]), scale.x, offset.x), rest,
                                  ppc_fmadds(FieldX(next[0]), scale.x, offset.x) * t);
    const float lowY = ppc_fmadds(ppc_fmadds(FieldY(row[0]), scale.y, offset.y), rest,
                                  ppc_fmadds(FieldY(next[0]), scale.y, offset.y) * t);
    const float lowZ = ppc_fmadds(ppc_fmadds(FieldZ(row[0]), scale.z, offset.z), rest,
                                  ppc_fmadds(FieldZ(next[0]), scale.z, offset.z) * t);
    if (add == 0) {
        box->mMin[0] = (lowX - scale.x) - kQuarter;
        box->mMin[1] = (lowY - scale.y) - kQuarter;
        box->mMin[2] = (lowZ - scale.z) - kQuarter;
    } else {
        box->mMin[0] = box->mMin[0] + ((lowX - scale.x) - kQuarter);
        box->mMin[1] = box->mMin[1] + ((lowY - scale.y) - kQuarter);
        box->mMin[2] = box->mMin[2] + ((lowZ - scale.z) - kQuarter);
    }
    const float highX = ppc_fmadds(ppc_fmadds(FieldX(row[1]), scale.x, offset.x), rest,
                                   ppc_fmadds(FieldX(next[1]), scale.x, offset.x) * t);
    const float highY = ppc_fmadds(ppc_fmadds(FieldY(row[1]), scale.y, offset.y), rest,
                                   ppc_fmadds(FieldY(next[1]), scale.y, offset.y) * t);
    const float highZ = ppc_fmadds(ppc_fmadds(FieldZ(row[1]), scale.z, offset.z), rest,
                                   ppc_fmadds(FieldZ(next[1]), scale.z, offset.z) * t);
    if (add == 0) {
        box->mMax[0] = (scale.x + highX) + kQuarter;
        box->mMax[1] = (scale.y + highY) + kQuarter;
        box->mMax[2] = (scale.z + highZ) + kQuarter;
    } else {
        box->mMax[0] = box->mMax[0] + ((scale.x + highX) + kQuarter);
        box->mMax[1] = box->mMax[1] + ((scale.y + highY) + kQuarter);
        box->mMax[2] = box->mMax[2] + ((scale.z + highZ) + kQuarter);
    }
}

// 0x100c40dc, 1,196 bytes. The morph's key data are an array of mKeyDataCount: the first one holds the node's
// bounding box, the second (when the morph's flag 2 is set) the vertices every key shares, the rest the vertices
// that move. Each has a current row (mF10) that only moves forward from call to call. The first call of a node
// (the node's flag 0x800000 clear) starts every key data at its first row, interpolates the box, and (flag 2)
// puts the shared vertices on the node (replacing, or with `add` nonzero adding), and in replacing mode marks the
// node 0x800000 so that later calls skip the shared vertices and continue from the rows they reached. Then each
// moving key data is interpolated into the node's vertex sets. The node ends with 0x10000.
// The original also takes the frame's integer part with modf and compares it with the fourth argument, and
// does nothing with the result: neither is translated.
void RADecompressMorph(tag_sNode* node, sMorphSet* morph, float frame, uint32_t a, int32_t b) {
    uint32_t remaining = morph->mKeyDataCount;
    sCMKeyData* key = morph->mKeyData.get();
    if ((node->mFlags & 0x800000) == 0) {
        for (uint32_t i = 0; i < remaining; ++i) key[i].mF10 = 0;
        InterpolateCMorphBoundData(morph, &node->mBox, key, KeyFraction(key, frame), b);
        ++key;
        --remaining;
        if ((morph->mF00 & 2) != 0) {
            const uint32_t* words = reinterpret_cast<const uint32_t*>(key->mVertices.get());
            const uint16_t* numbers = key->mF04.get();
            for (uint32_t i = 0; i < key->mKeyCount; ++i, ++words) {
                const uint32_t number = numbers[i];
                float* lane = &node->mVertices.get()[number >> 2].x[number & 3];
                const float x = ppc_fmadds(FieldX(*words), morph->mF20.x, morph->mF14.x);
                const float y = ppc_fmadds(FieldY(*words), morph->mF20.y, morph->mF14.y);
                const float z = ppc_fmadds(FieldZ(*words), morph->mF20.z, morph->mF14.z);
                if (b == 0) {
                    lane[0] = x;
                    lane[4] = y;
                    lane[8] = z;
                } else {
                    lane[0] = lane[0] + x;
                    lane[4] = lane[4] + y;
                    lane[8] = lane[8] + z;
                }
            }
            ++key;
            --remaining;
        }
        if (b == 0) node->mFlags |= 0x800000;
    } else {
        InterpolateCMorphBoundData(morph, &node->mBox, key, KeyFraction(key, frame), b);
        ++key;
        --remaining;
        if ((morph->mF00 & 2) != 0) {
            ++key;
            --remaining;
        }
    }
    for (; remaining != 0; --remaining, ++key) {
        InterpolateCMorphData(morph, node->mVertices.get(), key, KeyFraction(key, frame), b);
    }
    node->mFlags |= 0x10000;
}

// 0x100c54ec, 1,372 bytes. The first pass assigns and subtracts scratch masks in control order;
// the second restores the old animation's remaining geometry and reapplies each group entry.
void ResetRideAnimationAllChannels(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    tag_sM3D2Header* sourceHeader = set->mSelf->mHeads->mHeader;
    uint32_t engineFlags = gTPGlobals.mEngineFlags;
    auto* scratch = static_cast<uint32_t*>(gScratchGeomBuffer);
    const uint32_t cleared = (header->mFlags & 4) != 0 ? 0x800000 : 0xf40000;
    if ((set->mFlags & 8) != 0) {
        for (uint32_t i = 0; i < header->mAllNodeCount; ++i) NodeAt(header, i)->mFlags &= ~0x10u;
    } else if ((set->mFlags & 0x100000) != 0) {
        engineFlags |= 1;
    }
    std::memset(scratch, 0, static_cast<uint32_t>(header->mAllNodeCount) * sizeof(uint32_t));
    auto* controls = static_cast<sAnimTimeControl*>(set->mTimeControls);
    for (uint32_t c = 0; c < set->mTimeCount; ++c) {
        sAnimTimeControl& control = controls[c];
        if ((control.mFlags & 0x20) != 0) continue;
        tag_sAnimHeader* animation = control.mF34.get();
        if (animation != nullptr) {
            const uint16_t* disabled = animation->mNodesToDisable.get();
            for (uint32_t i = 0; i < animation->mDisableCount; ++i) {
                tag_sNode* node = NodeAt(header, disabled[i]);
                if ((node->mFlags & 0x80000000) == 0) node->mFlags &= ~0x10u;
            }
            const tag_sNodeAnim* records = animation->mNodeAnims.get();
            for (uint32_t i = 0; i < animation->mNodeAnimCount; ++i) {
                scratch[records[i].mF14] = records[i].mFlags;
                tag_sNode* node = NodeAt(header, records[i].mF14);
                node->mFlags &= ~cleared;
                if ((node->mFlags & 0x80000000) == 0) node->mFlags &= ~0x10u;
            }
        }
        if (control.mF04 == kNoKind) continue;
        const sRideAnimGroup& group = set->mGroups[control.mF04];
        if ((group.mFlags & 3) == 0 || ((engineFlags & 1) != 0 && (header->mFlags & 4) == 0)) continue;
        tag_sAnimHeader* other = group.mEntries[control.mF08].mAnimation;
        if (other != nullptr) {
            const tag_sNodeAnim* records = other->mNodeAnims.get();
            for (uint32_t i = 0; i < other->mNodeAnimCount; ++i) scratch[records[i].mF14] &= ~records[i].mFlags;
        }
    }
    bool texturesReset = false;
    for (uint32_t c = 0; c < set->mTimeCount; ++c) {
        sAnimTimeControl& control = controls[c];
        if (control.mF04 == kNoKind) continue;
        const sRideAnimGroup& group = set->mGroups[control.mF04];
        if ((group.mFlags & 3) == 0) continue;
        tag_sAnimHeader* animation = control.mF34.get();
        tag_sAnimHeader* other = group.mEntries[control.mF08].mAnimation;
        if ((control.mFlags & 0x20) == 0 && animation != nullptr) {
            if ((engineFlags & 1) == 0 || (header->mFlags & 4) != 0) {
                const tag_sNodeAnim* records = animation->mNodeAnims.get();
                for (uint32_t i = 0; i < animation->mNodeAnimCount; ++i) {
                    const uint32_t index = records[i].mF14;
                    tag_sNode* node = NodeAt(header, index);
                    tag_sNode* source = NodeAt(sourceHeader, index);
                    const uint32_t flags = scratch[index];
                    if ((flags & 0x289) != 0) {
                        for (int row = 0; row < 4; ++row) {
                            std::memcpy(node->mMatrix.m[row], source->mMatrix.m[row], 3 * sizeof(float));
                        }
                    }
                    if ((flags & 0x1000) != 0) {
                        if (node->mVertices != nullptr && source->mVertices != nullptr &&
                            ((node->mVertexCount + 3) >> 2) != 0) {
                            std::memcpy(node->mVertices.get(), source->mVertices.get(),
                                        static_cast<size_t>((node->mVertexCount + 3) >> 2) * sizeof(sVertexPosnSet));
                        }
                        std::memcpy(&node->mBox, &source->mBox, sizeof(sAlignedBox));
                    }
                    if ((flags & 0x10000) != 0) {
                        if (node->mUVs != nullptr && source->mUVs != nullptr &&
                            ((node->mF5E + 3) >> 2) != 0) {
                            std::memcpy(node->mUVs.get(), source->mUVs.get(),
                                        static_cast<size_t>((node->mF5E + 3) >> 2) * sizeof(sUVSet));
                        }
                    }
                }
            }
            if ((animation->mF00 & 2) != 0 && (gTPGlobals.mEngineFlags & 8) == 0 && !texturesReset) {
                sTexture* textures = header->mTextures.get();
                for (uint32_t i = 0; i < header->mTextureCount; ++i) {
                    if ((textures[i].mF00 & 0x40) != 0 && textures[i].mF02 != 0) {
                        textures[i].mF00 |= 0x400;
                        textures[i].mF02 = 0;
                    }
                }
                texturesReset = true;
            }
        }
        if ((control.mFlags & 4) != 0) control.mFlags |= 0x10;
        SetupAnimationFlags(other, set->mHeads);
    }
}

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) {
    sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[flag];
    if (time.mF24 != 0xc) time.mF24 = 0xc;
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    SetCurRideAnimDuration(set, a, anim, loop, speed, b);
}

// The ride animation clock and the step that moves a ride set's animations on (main app of
// SimTheme Park 2.04b3; the inventory's rsse.cpp range): TickRideAnims 0x100c6964 (152 bytes)
// and AnimateRideSet 0x100c74d4 (600 bytes). The animation of one time control
// (DoAnimateRideSet, 0x100c6c2c) and everything else they call is not part of this file.

// The frame's time for the animations, read once per frame: TPGlobals' word 0x4010 is the clock
// in milliseconds, 0x4014 the time since the last tick in seconds (the difference is taken as
// an unsigned count), and 0x4018 the fixed clock the time controls with flag 0x40 follow.
void TickRideAnims() {
    const uint32_t previous = gTPGlobals.mF4010;
    gTPGlobals.mF4010 = static_cast<uint32_t>(gClock.Read());
    const uint32_t elapsed = gTPGlobals.mF4010 - previous;
    gTPGlobals.mF4014 = static_cast<float>(static_cast<double>(elapsed)) / 1000.0f;
    gTPGlobals.mF4018 = static_cast<uint32_t>(gClock.ReadFixed());
}

// 0x100c6a20, 252 bytes. Takes the clock word into the control that is running, works out how
// far it has got and counts it as done when it is past its length or switched off (kind 0xc).
// Answers 1 for no set or when every pass counted it done, else 0 (also 0 for a set without
// animations or one that is held, flag 0x8000).
// Quirk, kept: the loop runs once for each of the set's mTimeCount controls but never moves on
// from the first one, so only the first control is ever ticked (mTimeCount times) and the answer
// says whether the first control is done (always 1 for a set with no controls).
int32_t TickRideAnimSet(tag_sRideAnimSet* set) {
    if (set == nullptr) return 1;
    if (set->mAnimationCount == 0 || (set->mFlags & 0x8000) != 0) return 0;
    uint32_t done = 0;
    auto* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
    for (uint32_t left = set->mTimeCount; left != 0; --left) {
        if (control->mF04 == 0xc) {
            ++done;
            continue;
        }
        const uint32_t time = ClockWord(control);
        control->mF14 = static_cast<int32_t>(time);
        control->mF18 = static_cast<int32_t>(time);
        control->mF20 = RunFrames(control);
        if (control->mF20 > control->mF1C) ++done;
    }
    return done == set->mTimeCount ? 1 : 0;
}

// 0x100c6b54, 144 bytes. Every node (or mini node) of the set's mesh with flag 0x100 loses flag 0x800;
// if there was one, the view is marked changed. The set loses flag 0x40000000.
void StaticifyCreateOnlyAnimNodes(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    bool changed = false;
    for (uint32_t i = 0; i < header->mAllNodeCount; i++) {
        uint32_t* flags;
        if (i < header->mNodeCount) flags = &header->mNodes.get()[i].mFlags;
        else flags = &header->mMiniNodes.get()[i - header->mNodeCount].mFlags;
        if ((*flags & 0x100) != 0) {
            changed = true;
            *flags &= ~0x800u;
        }
    }
    if (changed) gei_System.mFlags |= CSystem::kViewChanged;
    set->mFlags &= ~0x40000000u;
}

// 0x100c6c2c, 1,448 bytes. Moves one time control of a set on and applies its animation. `kind`
// (the control's mF04, 0xc for none) and mF08 name the current animation in the set's groups.
// A held set (flag 0x8000) only applies the animation at the frame the control has. Otherwise the
// control's frame is brought up to the clock (SetControlFrame), and a control past its animation's
// length (mF20 above mF1C)
//   - marks itself done first (flag 8, for kind 0);
//   - starts the animation queued in mF24 (kind), mF28 (index), mF2C (flags) and mF30 (speed),
//     which then empty (kind 0xc);
//   - or, with nothing queued and the set's flags 0x18 clear, starts the control's own animation
//     again (control flag 1: its own kind, index and speed, the speed mF0C when mF30 is 0) or
//     the idle one (kind 0xe);
//   - or, with set flag 0x10 and not 8, holds the last frame and marks the set (0x40400000);
//   - or, with set flag 8, switches the control off (SetNewAnim kind 0xc) and takes the mesh out
//     of the map (the range form with the set's flag 0x4000).
// A control marked done (flag 8, kind not 0) marks the set (0x40000000). A control that started a
// new animation gets its frame worked out again. The nodes are animated when the set says so (flag
// 0x400000) or `a` is nonzero and the control either animates (flag 4 clear) or has been asked to
// (flag 0x10, cleared afterwards); the control remembers the animation it applied. A held end
// switches the control off after that (SetNewAnim kind 0xc, flags 4).
// Quirk, not reproduced: for a held set whose current kind has no animations the original takes the
// animation from r30, which is AnimateRideSet's `flags` argument (0 in every call the lobby makes);
// the port takes none.
void DoAnimateRideSet(tag_sRideAnimSet* set, sAnimTimeControl* control, int32_t a) {
    if (set == nullptr) return;
    const uint32_t kind = control->mF04;
    if (kind == 0xc) return;
    if ((set->mFlags & 0x8000) != 0) {
        tag_sAnimHeader* animation = nullptr;
        if (set->mGroups[kind].mCount != 0) animation = set->mGroups[kind].mEntries[control->mF08].mAnimation;
        tag_sMesh* mesh = set->mHeads;
        if (animation != nullptr) ApplyAnimation(control, mesh, animation);
        return;
    }
    bool restarted = false;
    bool holding = false;
    const uint32_t time = ClockWord(control);
    SetControlFrame(control, time);
    if (control->mF20 > control->mF1C) {
        if (control->mF04 == 0) control->mFlags |= 8;
        if (control->mF24 == 0xc) {
            if ((set->mFlags & 0x18) == 0) {
                restarted = true;
                if ((control->mFlags & 1) == 0) {
                    SetNewAnim(set, 0xe, 0, control, a != 0 ? 0 : 8, 1.0f);
                } else {
                    const float speed = control->mF30 == 0.0f ? control->mF0C : control->mF30;
                    SetNewAnim(set, control->mF04, control->mF08, control, (a != 0 ? 0u : 8u) | 1, speed);
                }
            } else if ((set->mFlags & 8) == 0) {
                holding = true;
                control->mF20 = control->mF1C;
                set->mFlags |= 0x40400000;
                control->mFlags &= ~8u;
            } else {
                SetNewAnim(set, 0xc, 0, control, 0, 1.0f);
                if (IsMeshDrawEnabled(set->mHeads) != 0) {
                    if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
                    else DisableMeshDraw(set->mHeads);
                }
            }
        } else {
            restarted = true;
            SetNewAnim(set, control->mF24, control->mF28, control, control->mF2C | (a != 0 ? 0u : 8u), control->mF30);
            control->mF24 = 0xc;
            control->mF28 = 0;
            control->mF30 = 0.0f;
        }
    }
    if (control->mF04 != 0 && (control->mFlags & 8) != 0) {
        set->mFlags |= 0x40000000;
        control->mFlags &= ~8u;
    }
    tag_sAnimHeader* animation = nullptr;
    if (control->mF04 != 0xc) {
        const sRideAnimGroup& group = set->mGroups[control->mF04];
        if (group.mCount != 0) animation = group.mEntries[control->mF08].mAnimation;
        else control->mF08 = 0;
    }
    if (restarted && control->mF04 != 0xc) SetControlFrame(control, time);
    if ((set->mFlags & 0x400000) != 0 || (a != 0 && ((control->mFlags & 4) == 0 || (control->mFlags & 0x10) != 0))) {
        tag_sMesh* mesh = set->mHeads;
        if (animation != nullptr) ApplyAnimation(control, mesh, animation);
        control->mF34 = animation;
        control->mFlags &= ~0x10u;
    }
    if (holding) SetNewAnim(set, 0xc, 0, control, 4, 1.0f);
}

// rsse.cpp (main app of SimTheme Park 2.04b3): the face normals of a mesh after an animation has moved its vertices
// (RecalculateInvalidNodeFaceNormals 0x100c7224, RecalculateInvalidFaceNormals 0x100c7418).

namespace {

// The position of vertex `index` of a node: the vertices are kept four to a set (x, y and z of four vertices), the set
// being the index >> 2 and the lane the two low bits.
void VertexPosition(const sVertexPosnSet* sets, uint32_t index, float* position) {
    const sVertexPosnSet& set = sets[index >> 2];
    const uint32_t lane = index & 3;
    position[0] = set.x[lane];
    position[1] = set.y[lane];
    position[2] = set.z[lane];
}

}  // namespace

void RecalculateInvalidNodeFaceNormals(tag_sNode* node) {
    const uint16_t* remap = node->mF94.get();
    const sFace* face = node->mFaces.get();
    for (uint32_t count = node->mF5C; count != 0; --count, ++face) {
        // The face's second slot is the corner both edges start from.
        float origin[3];
        float first[3];
        float last[3];
        const sVertexPosnSet* sets = node->mVertices.get();
        VertexPosition(sets, remap[face->mF02[1]], origin);
        VertexPosition(sets, remap[face->mF02[0]], first);
        VertexPosition(sets, remap[face->mF02[2]], last);
        const float ux = first[0] - origin[0];
        const float uy = first[1] - origin[1];
        const float uz = first[2] - origin[2];
        const float vx = last[0] - origin[0];
        const float vy = last[1] - origin[1];
        const float vz = last[2] - origin[2];
        // The cross product v x u: each component is one exact product less a second one in a fused subtract, rounded to
        // double and then to single.
        s3DPoint* normal = node->mPoints.get() + (face->mIndexAndFlag >> 1);
        normal->x = static_cast<float>(ppc_fmsub(vy, uz, static_cast<double>(vz) * static_cast<double>(uy)));
        normal->y = static_cast<float>(ppc_fmsub(vz, ux, static_cast<double>(vx) * static_cast<double>(uz)));
        normal->z = static_cast<float>(ppc_fmsub(vx, uy, static_cast<double>(vy) * static_cast<double>(ux)));
        // The length: x * x is the exact product in double, the other two squares are rounded to single first; the sum
        // takes z * z first.
        const double xx = static_cast<double>(normal->x) * static_cast<double>(normal->x);
        const double yy = static_cast<double>(normal->y * normal->y);
        const double zz = static_cast<double>(normal->z * normal->z);
        const float length = static_cast<float>(std::sqrt(zz + (xx + yy)));
        normal->x = normal->x / length;
        normal->y = normal->y / length;
        normal->z = normal->z / length;
        // A face with no area has length 0 and a NaN for x (the class code 1 of the library's __fpclassifyf): it points down.
        if (std::isnan(normal->x)) {
            normal->x = 0.0f;
            normal->y = -1.0f;
            normal->z = 0.0f;
        }
    }
    node->mFlags &= ~0x10000u;
}

void RecalculateInvalidFaceNormals(tag_sM3D2Header* header) {
    tag_sNode* node = header->mNodes.get();
    for (uint32_t count = header->mNodeCount; count != 0; --count, ++node) {
        if ((node->mFlags & 0x10000) != 0) RecalculateInvalidNodeFaceNormals(node);
    }
    header->mFlags &= ~0x40000u;
}

// Moves one set on. Unless the set is held (0x4 or 0x8000) its current animation is ticked up
// to three times, each tick that finishes one (TickRideAnimSet returns 1) starting the next
// through time control 5. A set with animations then animates every time control; `flags`
// (the loop word of SetCurRideAnim, and bit 3 for the embedded data) comes from the caller.
// The original returns 1, or 0 for no set; nothing reads it.
void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) {
    if (set == nullptr) return;
    if ((set->mFlags & 0x8004) == 0) {
        uint32_t rounds = 0;
        do {
            if (TickRideAnimSet(set) != 1) break;
            SetCurRideAnim(set, 5, 0, flags, 1.0f, 0);
            ++rounds;
        } while (rounds < 3);
    }
    if (set->mAnimationCount != 0) {
        // A header with flag 4 has its node animations reset from the start every frame.
        if ((set->mHeads->mHeader->mFlags & 4) != 0) {
            sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
            for (uint32_t left = set->mTimeCount; left != 0; --left, ++control) {
                if (control->mF04 != 0xc) {
                    ResetRideAnimationSingleChannel(set, set->mGroups[control->mF04].mEntries[control->mF08].mAnimation,
                                                    nullptr);
                }
            }
        }
        sAnimTimeControl* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
        for (uint32_t left = set->mTimeCount; left != 0; --left, ++control) DoAnimateRideSet(set, control, a);
        // Flag 0x40000000: the nodes and mini nodes with flag 0x100 lose 0x800, and the view is
        // flagged as changed if there was one.
        if ((set->mFlags & 0x40000000) != 0) {
            bool changed = false;
            tag_sM3D2Header* header = set->mHeads->mHeader;
            for (uint32_t i = 0; i < header->mAllNodeCount; ++i) {
                uint32_t* nodeFlags;
                if (i < header->mNodeCount) nodeFlags = &header->mNodes.get()[i].mFlags;
                else nodeFlags = &header->mMiniNodes.get()[i - header->mNodeCount].mFlags;
                if ((*nodeFlags & 0x100) != 0) {
                    changed = true;
                    *nodeFlags &= ~0x800u;
                }
            }
            if (changed) gei_System.mFlags |= CSystem::kViewChanged;
            set->mFlags &= ~0x40000000u;
        }
        set->mFlags &= ~0x400000u;
        // A header with flag 4 has the face normals of its marked nodes calculated again.
        tag_sM3D2Header* header = set->mHeads->mHeader;
        if ((header->mFlags & 4) != 0) {
            tag_sNode* node = header->mNodes.get();
            for (uint32_t left = header->mNodeCount; left != 0; --left, ++node) {
                if ((node->mFlags & 0x10000) != 0) RecalculateInvalidNodeFaceNormals(node);
            }
            header->mFlags &= ~0x40000u;
        }
    }
    tag_sMesh* mesh = set->mHeads;
    if (mesh->mF28 != nullptr) {
        if (((flags & 8) == 0 || (mesh->mHeader->mFlags & 4) != 0) && (set->mFlags & 0x20000000) == 0) {
            TPProcessEmbeddedData(mesh);
        }
    }
}

// 0x100c7768, 396 bytes. Sets up a ride animation set. Each of its time controls gets flag 0x40
// when mode is nonzero, then ResetAnimTimeStats with no animation, and words 0x04 and 0x24 set to
// 0xc when init is nonzero. When keepNodes is zero, flag 0x100 goes on every node, mini nodes
// included, and the set counts as empty unless a later kind has an entry. Every entry of the
// twelve kinds with an animation is then set up with RAInitNodeAnimNodes, passing 1 for the first
// kind and 0 for the rest. A set with keepNodes zero and no entries in the later kinds gets flag
// 0x10. The header calls the fourth argument keepNodes, but the code sets flag 0x100 when it is
// zero. The original returns 1.
int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t init, int32_t mode, int32_t keepNodes) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    auto* control = static_cast<sAnimTimeControl*>(set->mTimeControls);
    for (uint32_t t = set->mTimeCount; t != 0; --t, ++control) {
        if (mode != 0) control->mFlags |= 0x40;
        ResetAnimTimeStats(control, nullptr, 0, 0.0f);
        if (init != 0) {
            control->mF04 = 0xc;
            control->mF24 = 0xc;
        }
    }
    bool allEmpty;
    if (keepNodes == 0) {
        for (uint32_t n = 0; n < header->mAllNodeCount; ++n) {
            tag_sMiniNode* node;
            if (n < header->mNodeCount) {
                node = header->mNodes.get() + n;
            } else {
                node = header->mMiniNodes.get() + (n - header->mNodeCount);
            }
            node->mFlags |= 0x100;
        }
        allEmpty = true;
    } else {
        allEmpty = false;
    }
    for (uint32_t kind = 0; kind < 12; ++kind) {
        sRideAnimGroup& group = set->mGroups[kind];
        for (uint32_t k = 0; k < group.mCount; ++k) {
            tag_sAnimHeader* animation = group.mEntries[k].mAnimation;
            if (animation != nullptr) RAInitNodeAnimNodes(set->mHeads, animation, kind == 0 ? 1 : 0);
        }
        if (kind != 0 && group.mCount != 0) allEmpty = false;
    }
    if (allEmpty) set->mFlags |= 0x10;
    return 1;
}

// The ride mesh's frame callback and animation queries (SimTheme Park 2.04b3).
// TPRide.cpp: AnimateRideCallback 0x10069a98, ProcessRideAnim 0x1005b24c,
// HandleActiveHoardings 0x100619d4. rsse.cpp: GetCurrentAnimStats 0x100c7930.

// 0x100c7930, 136 bytes. Runtime helper 0x10237cd8 is __cvt_fp2unsigned.
uint32_t GetCurrentAnimStats(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index,
                             uint32_t* playing, uint32_t control) {
    auto* record = static_cast<sAnimTimeControl*>(set->mTimeControls) + control;
    if (kind != nullptr) *kind = record->mF04;
    if (index != nullptr) *index = record->mF08;
    if (playing != nullptr) *playing = record->mFlags & 1;
    const float frames = record->mF1C - record->mF20;
    const float milliseconds = 0x1.0aaaaap+5f * frames;
    return cw_cvt_fp2unsigned(milliseconds);
}

// The answer is the time control's flags word (r3 is loaded from it last).
uint32_t GetCurrentAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    const sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[control];
    if (kind != nullptr) *kind = time.mF04;
    if (index != nullptr) *index = time.mF08;
    return time.mFlags;
}

// The small functions the advisor calls (main app of SimTheme Park 2.04b3), each from a range of its own:
//   GetDeferredAnim 0x100c7a74 and GetAnimLength 0x100c7bb4 (rsse.cpp, next to the animation tick),
//   SetMeshInUISpace 0x1006553c (TPRide.cpp, the mesh draw switches),
//   FindDataNodeIndexByID 0x101ffdc8 and FindDataNodeIndexByIDAdvisorTempHack 0x101fff90 (TPEmbed.cpp),
//   FindDataNodeIndexByDisableCode 0x1020017c (TPEmbed.cpp, unit v5L21).
// They are here and not in the files of their neighbours so that the advisor's unit library links without the rest of
// those files. (ScaleLMatrixNoTrans is in src/rsse_matrix.cpp and SND_DuckVolume in src/TPSound.cpp.)

// A time control is 0x38 bytes (sAnimTimeControl); words 0x24 and 0x28 hold the animation queued behind the playing one.
void GetDeferredAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    const sAnimTimeControl& time = static_cast<sAnimTimeControl*>(set->mTimeControls)[control];
    if (kind != nullptr) *kind = time.mF24;
    if (index != nullptr) *index = time.mF28;
}

// ---- the animation time controls (0x38 bytes each)

void UpdateRideAnimSpeedAllChannels(tag_sRideAnimSet* set, float speed) {
    for (uint32_t i = 0; i < set->mTimeCount; i++) static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF30 = speed;
}

void UpdateRideAnimSpeedImmediate(tag_sRideAnimSet* set, float speed, int32_t flag) {
    static_cast<sAnimTimeControl*>(set->mTimeControls)[static_cast<uint32_t>(flag)].mF0C = speed;
}

// The length of an animation header is in frames (word 8) at 30 a second. The product is a 32-bit unsigned number
// (the original's `mulli` then a multiply-high by 0x88888889 and a shift, which is the division by 30).
uint32_t GetAnimLength(tag_sRideAnimSet* set, uint32_t kind, uint32_t index) {
    if (kind == 0xc) return 0;
    const sRideAnimGroup& group = set->mGroups[kind];
    if ((group.mFlags & 3) == 0 || group.mCount <= index) return 0;
    const uint32_t frames = group.mEntries[index].mAnimation->mF08;
    return (frames * 1000u) / 30u;
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3): RSSE_Init (0x100d32e8, 212 bytes) and SetAnimationDetail (0x100c7c68,
// 12 bytes). The script system starts here: its state words are set, and a checksum of the script language's op table is
// made (RSSE_InitScript compares a script file's header with it).

// The animation detail level (LevelInit1 passes 0) goes into the word at gTPGlobals + 0x400c, which the headers call the engine
// flags (RideAnimInterp, RideAnimTick and RideAnimChange test its bits): the original overwrites them whole.
void SetAnimationDetail(uint32_t detail) {
    gTPGlobals.mEngineFlags = detail;
}

// 0x100c7ca0, 208 bytes. The rotation matrix of the quaternion (x, y, z, w), scaled by 2 / |q|^2, whose
// squared length is summed as w*w + (z*z + (x*x + y*y)). Nothing is fused; row 0 is
// (1 - (yy + zz), xy + wz, xz - wy), row 1 (xy - wz, 1 - (xx + zz), yz + wx), row 2
// (xz + wy, yz - wx, 1 - (xx + yy)), each product of two coordinates made of one scaled by 2 / |q|^2.
void RAQuaternionToLAxis(sQuaternion* quaternion, s3DAxis* out) {
    const float x = quaternion->x;
    const float y = quaternion->y;
    const float z = quaternion->z;
    const float w = quaternion->w;
    const float norm = (w * w) + ((z * z) + ((x * x) + (y * y)));
    const float scale = 2.0f / norm;
    const float scaledZ = z * scale;
    const float scaledY = y * scale;
    const float scaledX = x * scale;
    const float zz = z * scaledZ;
    const float yy = y * scaledY;
    const float wz = w * scaledZ;
    const float xy = x * scaledY;
    const float wy = w * scaledY;
    const float xz = x * scaledZ;
    const float xx = x * scaledX;
    const float wx = w * scaledX;
    const float yz = y * scaledZ;
    out->mX.x = 1.0f - (yy + zz);
    out->mX.y = xy + wz;
    out->mX.z = xz - wy;
    out->mY.x = xy - wz;
    out->mY.y = 1.0f - (xx + zz);
    out->mY.z = yz + wx;
    out->mZ.x = xz + wy;
    out->mZ.y = yz - wx;
    out->mZ.z = 1.0f - (xx + yy);
}

// 0x100c7db0, 580 bytes. See rsse.h. dot = a . b (fused). With 1 + dot not above 0.001 (or NaN) the
// quaternions are nearly opposite: out = (-a.y, a.x, -a.w, a.z) (a quarter turn), blended with a by the
// sines of t and 1 - t times a quarter turn, in x, y and z only. With 1 - dot above 0.001 they are
// apart: the weights are sin((1 - t) * theta) / sin(theta) for a and sin(t * theta) / sin(theta) for
// b, theta = acos(dot) as a single, the sines from the table. Else they are close: weights 1 - t and t.
void RASphericalLinearInterpolate(sQuaternion* a, sQuaternion* b, float t, sQuaternion* out) {
    float dot = a->y * b->y;
    dot = ppc_fmadds(a->x, b->x, dot);
    dot = ppc_fmadds(a->z, b->z, dot);
    dot = ppc_fmadds(a->w, b->w, dot);
    constexpr float kEpsilon = 0.001f;   // 0x3a83126f
    if (!(1.0f + dot > kEpsilon)) {
        out->x = ppc_fneg(a->y);
        out->y = a->x;
        out->z = ppc_fneg(a->w);
        out->w = a->z;
        const float rest = 1.0f - t;
        const float scale = gTPGlobals.mSinTableScale;
        const float weightB = SineAt((kHalfPi * t) * scale);
        const float weightA = SineAt((kHalfPi * rest) * scale);
        // (out->w is not blended.)
        out->x = ppc_fmadds(weightA, a->x, weightB * out->x);
        out->y = ppc_fmadds(weightA, a->y, weightB * out->y);
        out->z = ppc_fmadds(weightA, a->z, weightB * out->z);
        return;
    }
    float weightA;
    float weightB;
    if (1.0f - dot > kEpsilon) {
        const float theta = static_cast<float>(std::acos(static_cast<double>(dot)));
        const float scale = gTPGlobals.mSinTableScale;
        const float rest = 1.0f - t;
        const float scaledT = scale * (t * theta);
        const float scaledRest = scale * (rest * theta);
        const float scaledTheta = theta * scale;
        const float sinTheta = SineAt(scaledTheta);
        weightA = SineAt(scaledRest) / sinTheta;
        weightB = SineAt(scaledT) / sinTheta;
    } else {
        weightA = 1.0f - t;
        weightB = t;
    }
    out->x = ppc_fmadds(weightA, a->x, weightB * b->x);
    out->y = ppc_fmadds(weightA, a->y, weightB * b->y);
    out->z = ppc_fmadds(weightA, a->z, weightB * b->z);
    out->w = ppc_fmadds(weightA, a->w, weightB * b->w);
}

// 0x100c8054, 248 bytes. See rsse.h. The next key is the one after `index`, or `index` again when it
// is not below TPGlobals' word 0x401c less one (compared as unsigned: with 0 keys every index has a
// next). The plain blend is (1 - t) * from fused with t * to, in all four numbers.
void RACalcOrientationBetweenControlPoints(sQuatKey* keys, uint32_t index, float fraction, s3DAxis* out) {
    const uint32_t next = index < gTPGlobals.mF401C - 1 ? index + 1 : index;
    sQuaternion* from = reinterpret_cast<sQuaternion*>(keys[static_cast<int32_t>(index)].mF04);
    sQuaternion* to = reinterpret_cast<sQuaternion*>(keys[static_cast<int32_t>(next)].mF04);
    sQuaternion blended;
    if ((gTPGlobals.mEngineFlags & 2) != 0) {
        const float rest = 1.0f - fraction;
        blended.x = ppc_fmadds(from->x, rest, to->x * fraction);
        blended.y = ppc_fmadds(from->y, rest, to->y * fraction);
        blended.z = ppc_fmadds(from->z, rest, to->z * fraction);
        blended.w = ppc_fmadds(from->w, rest, to->w * fraction);
    } else {
        RASphericalLinearInterpolate(from, to, fraction, &blended);
    }
    RAQuaternionToLAxis(&blended, out);
}

// 0x100c81a0, 232 bytes. The frame is the control's mF20 as an unsigned count. The first key whose
// number is above it ends the scan: *index is the key before (for the first key that is -1, and the
// number before the array is read), the fraction is (frame - before) / (after - before) with the
// frame as the float mF20, not the count. With no such key *index is count - 2 and *fraction 1.0.
void RAGetUVFrameData(sAnimTimeControl*& control, uint16_t*& frames, uint32_t count, uint32_t& index, float& fraction) {
    const uint32_t frame = cw_cvt_fp2unsigned(static_cast<double>(control->mF20));
    const uint16_t* key = frames;
    for (uint32_t i = 0; i < count; ++i, ++key) {
        if (frame >= *key) continue;
        index = i - 1;
        const float before = static_cast<float>(frames[static_cast<int32_t>(index)]);
        const float after = static_cast<float>(*key);
        fraction = (control->mF20 - before) / (after - before);
        return;
    }
    index = count - 2;
    fraction = 1.0f;
}

// 0x100c82d0, 460 bytes. See rsse.h. Track k of the node animation's texture coordinates has its
// frame numbers from mF0C and its (u, v) pairs from mF10, both from the track's first place; the
// pair is the blend of the one at the key RAGetUVFrameData finds and the next, (1 - t) * from fused
// with t * to. The node's sets hold four u and then four v each.
void RAAnimateTextureCoords(sAnimTimeControl* control, tag_sNodeAnim* nodeAnim, int32_t additive) {
    tag_sNode* node = nodeAnim->mNode.get();
    sUVAnim* animation = nodeAnim->mUVAnim.get();
    for (uint32_t k = 0; k < animation->mTrackCount; ++k) {
        const sUVTrackControl& track = animation->mTrack.get()[k];
        uint16_t* frames = animation->mF0C.get() + track.mF00;
        const float* pairs = animation->mF10.get() + 2 * static_cast<uint32_t>(track.mF00);
        uint32_t index;
        float fraction;
        RAGetUVFrameData(control, frames, track.mF02, index, fraction);
        // (The index is -1 or -2 for a frame before the track's first key; the Mac works the address
        // out in 32 bits, which a signed index reproduces.)
        const float* from = pairs + 2 * static_cast<int32_t>(index);
        const float* to = from + 2;
        const float rest = 1.0f - fraction;
        const float u = ppc_fmadds(rest, from[0], fraction * to[0]);
        const float v = ppc_fmadds(rest, from[1], fraction * to[1]);
        float* set = node->mUVs.get()[k >> 2].mF00;
        float& slotU = set[k & 3];
        float& slotV = set[4 + (k & 3)];
        if (additive != 0) {
            slotU = slotU + u;
            slotV = slotV + v;
        } else {
            slotU = u;
            slotV = v;
        }
    }
}
// 0x100c84ec, 428 bytes. The cubic bezier point of the shape (three points a segment, the points at
// 3 * whole + index - 1 to + 2 of a ring of TPGlobals' word 0x4020 points) at the fractional part of
// `fraction`.
void RABezierInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) * 3 + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = BezierCoordinate(ring.at->x, ring.before->x, ring.next->x, ring.after->x, t);
    out->y = BezierCoordinate(ring.at->y, ring.before->y, ring.next->y, ring.after->y, t);
    out->z = BezierCoordinate(ring.at->z, ring.before->z, ring.next->z, ring.after->z, t);
}

// 0x100c86e8, 420 bytes. The bezier's direction at the same place (the derivative of the above).
void RABezierTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) * 3 + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = BezierSlope(ring.at->x, ring.before->x, ring.next->x, ring.after->x, t);
    out->y = BezierSlope(ring.at->y, ring.before->y, ring.next->y, ring.after->y, t);
    out->z = BezierSlope(ring.at->z, ring.before->z, ring.next->z, ring.after->z, t);
}

// 0x100c88e0, 152 bytes. The point between points (index + whole) and the next, at the fractional
// part; each coordinate is (1 - t) * from fused with t * to. No wrapping: an index past the shape's
// last point reads whatever follows.
void RALinearInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const s3DPoint* points = PointsOf(shape, a);
    const SplitFraction split = Split(fraction);
    const uint32_t place = index + static_cast<uint32_t>(split.whole);
    const s3DPoint& from = PointAt(points, place);
    const s3DPoint& to = PointAt(points, place + 1);
    const float t = split.rest;
    const float rest = 1.0f - t;
    out->x = ppc_fmadds(rest, from.x, t * to.x);
    out->y = ppc_fmadds(rest, from.y, t * to.y);
    out->z = ppc_fmadds(rest, from.z, t * to.z);
}

// 0x100c89c8, 112 bytes. The step from point (index + whole) to the next: to - from. The fraction
// only picks the segment. Quirk: the original compares `a` with null but no branch follows, so it
// always takes the shape's own points and `a` is not used (AnimateNode always passes null).
void RALinearTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const s3DPoint* points = shape->mPoints.get();
    const uint32_t place = index + static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fraction)));
    const s3DPoint& from = PointAt(points, place);
    const s3DPoint& to = PointAt(points, place + 1);
    out->x = to.x - from.x;
    out->y = to.y - from.y;
    out->z = to.z - from.z;
}

// 0x100c8a8c, 460 bytes. The Catmull-Rom point between points (index + whole) and the next on a ring
// of TPGlobals' word 0x4020 points; the four points are the one before, the two and the one after.
void RACatmulInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out) {
    const SplitFraction split = Split(fraction);
    const uint32_t base = index + static_cast<uint32_t>(split.whole) + gTPGlobals.mF4020;
    const Ring ring = RingAround(PointsOf(shape, a), base, gTPGlobals.mF4020);
    const float t = split.rest;
    out->x = CatmullCoordinate(ring.before->x, ring.at->x, ring.next->x, ring.after->x, t);
    out->y = CatmullCoordinate(ring.before->y, ring.at->y, ring.next->y, ring.after->y, t);
    out->z = CatmullCoordinate(ring.before->z, ring.at->z, ring.next->z, ring.after->z, t);
}

// 0x100c8ca8, 356 bytes. See rsse.h. The track's values are percentages that wrap round at 100: when
// the two neighbours are more than 50 apart the lower gets 100 added, and the blend is taken modulo
// 100. A frame equal to the count (its whole part) stands on the last value with a fraction one more.
// Without `wrap` the last value is its own neighbour; with it the neighbour of the last is the first.
// (A zero count with `wrap` divides by zero: undefined on the PowerPC.)
void RAInterpPosnAtFrame(float* values, uint32_t count, float frame, float* out, int32_t wrap) {
    double wholeFrame;
    float fraction = static_cast<float>(std::modf(static_cast<double>(frame), &wholeFrame));
    uint32_t index = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(static_cast<float>(wholeFrame))));
    if (index == count) {
        index = index - 1;
        fraction = fraction + 1.0f;
    }
    float low;
    float high;
    if (wrap != 0) {
        low = values[static_cast<int32_t>(index)];
        high = values[UnsignedRemainder(index + 1, count)];
    } else {
        low = values[static_cast<int32_t>(index)];
        high = index < count - 1 ? values[static_cast<int32_t>(index) + 1] : low;
    }
    const float apart = static_cast<float>(std::fabs(static_cast<double>(low) - static_cast<double>(high)));
    if (apart > 50.0f) {
        if (low < high) low = low + 100.0f;
        else high = high + 100.0f;
        *out = ppc_fmadds(1.0f - fraction, low, fraction * high);
        *out = static_cast<float>(std::fmod(static_cast<double>(*out), 100.0));
    } else {
        *out = ppc_fmadds(1.0f - fraction, low, fraction * high);
    }
}

// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.

// ---- destructors with the hidden delete flag (a signed 16-bit value)
__attribute__((weak)) void Bitmap_DeleteArray(void* block) { ::operator delete[](block); }

// 0x100ca4bc
Bitmap* Bitmap_DtorMac(Bitmap* self, int16_t flag) {
    if (self != nullptr) {
        if (self->mData != nullptr) Bitmap_DeleteArray(self->mData);
        self->mWidth = 0;
        self->mHeight = 0;
        self->mBytesPerPixel = 0;
        self->mData = nullptr;
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}
