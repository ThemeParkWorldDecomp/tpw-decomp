// The ride animation tick of the main app (SimTheme Park 2.04b3): what moves a ride set's time
// controls on and applies the animations they select to the nodes of the set's mesh. Eight
// functions of three ranges of the inventory (rsse.cpp, TPRide.cpp, TPEmbed.cpp):
//   TickRideAnimSet 0x100c6a20, DoAnimateRideSet 0x100c6c2c, RAAnimateTextureFrames 0x100c3678,
//   AnimateNode 0x100c45d4, GetAnimFrameData 0x100c34bc, IsMeshDrawEnabled 0x10065364,
//   TPProcessEmbeddedData 0x101ffb0c and DoWorldTransformsForEmbeddedData 0x101fef24.
// AnimateRideSet (src/RideTick.cpp) calls TickRideAnimSet, DoAnimateRideSet and
// TPProcessEmbeddedData; InstanceRide (src/RideTypes.cpp) calls the last as well.
#include <cmath>
#include <cstring>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"

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

// 0x10065364, 12 bytes. Bit 0 of the mesh's first word: the mesh is linked into the map.
int32_t IsMeshDrawEnabled(tag_sMesh* mesh) { return mesh->mF00 & 1; }

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

// 0x101ffb0c, 264 bytes. For a mesh that is in the map (flag 1) and has embedded data with a
// block (the set's flag 1), works out the world matrices of the embedded nodes from the root,
// starting from the identity.
void TPProcessEmbeddedData(tag_sMesh* mesh) {
    if ((mesh->mF00 & 1) == 0) return;
    auto* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    if ((set->mFlags & 1) == 0) return;
    sMatrix identity;
    SetMatrixIdentity(&identity);
    DoWorldTransformsForEmbeddedData(mesh->mHeader->mF78.get(), mesh->mHeader, identity, &identity, set->mRecords);
}

// 0x101fef24, 1,744 bytes. See TPEngine.h. A node without flag 0x20000000 only multiplies its own
// matrix into the running one. A node with it has an embedded record (the node's mF50 less the
// header's mF46): if the record is complete (flag 0x20 with none of 0xe set it is skipped) the
// node's matrix is multiplied in, and when the entry has flag 0x40 or 0x40000 and the node's
// parent has flag 0x200000 the matrix's translation becomes the point the entry's morph record
// gives: the triangle (a face of the parent) carried through `world`, its corners blended by the
// record's weights a and b, and moved c along the face's normal (also carried). With entry flag
// 0x40000 the rotation comes from CalculateOrientFromTrackMorph. The matrix then goes in the
// record's block. After it, a node with flags 0x10000000 and 0x10000 has its normals calculated
// again, and the nodes below it are walked with the matrix as it now is (children with flag
// 0x40000000 are skipped), each handed that matrix as the world matrix of its parent.
void DoWorldTransformsForEmbeddedData(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sMatrix* world,
                                      sEmbeddedRecord* records) {
    if ((node->mFlags & 0x20000000) == 0) {
        CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    } else {
        const int32_t index = static_cast<int32_t>(node->mF50) - static_cast<int32_t>(header->mF46);
        sEmbeddedRecord& record = records[index];
        const sEmbeddedData& entry = header->mEmbedded.get()[index];
        if ((record.mFlags & 0x20) == 0 || (record.mFlags & 0xe) != 0) {
            CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
            // (The parent is not checked for null: the original faults on a missing one.)
            if ((entry.mFlags & 0x40040) != 0 && (node->mF04->mFlags & 0x200000) != 0) {
                tag_sNode* parent = node->mF04.get();
                sTrackMorphingParent* morph = entry.mMorphParent.get();
                const sFace* face = parent->mFaces.get() + morph->mF00;
                // The face's three corners, carried through the parent's world matrix.
                s3DPoint corner[3];
                for (int k = 0; k < 3; ++k) {
                    const uint16_t word = parent->mF94.get()[face->mF02[k]];
                    const sVertexPosnSet& set = parent->mVertices.get()[word >> 2];
                    const uint32_t lane = word & 3;
                    corner[k] = {set.x[lane], set.y[lane], set.z[lane]};
                    VectorThroughLMatrixNoW(world, &corner[k]);
                }
                // The face's normal, carried through the rotation only.
                const s3DPoint normalIn = parent->mPoints.get()[(face->mIndexAndFlag >> 1) & 0x7fff];
                const sMatrix& w = *world;
                float nx = normalIn.y * w.m[1][0];
                float ny = normalIn.y * w.m[1][1];
                float nz = normalIn.y * w.m[1][2];
                nx = ppc_fmadds(normalIn.x, w.m[0][0], nx);
                ny = ppc_fmadds(normalIn.x, w.m[0][1], ny);
                nz = ppc_fmadds(normalIn.x, w.m[0][2], nz);
                nx = ppc_fmadds(normalIn.z, w.m[2][0], nx);
                ny = ppc_fmadds(normalIn.z, w.m[2][1], ny);
                nz = ppc_fmadds(normalIn.z, w.m[2][2], nz);
                s3DPoint normal = {nx, ny, nz};
                // The record's three weights (floats kept in its words 1 to 3).
                float a;
                float b;
                float c;
                std::memcpy(&a, &morph->mF04[0], sizeof a);
                std::memcpy(&b, &morph->mF04[1], sizeof b);
                std::memcpy(&c, &morph->mF04[2], sizeof c);
                const float restA = 1.0f - a;
                float x = ppc_fmadds(corner[0].x, restA, corner[1].x * a);
                float y = ppc_fmadds(corner[0].y, restA, corner[1].y * a);
                float z = ppc_fmadds(corner[0].z, restA, corner[1].z * a);
                const float restB = 1.0f - b;
                x = ppc_fmadds(x, restB, corner[2].x * b);
                y = ppc_fmadds(y, restB, corner[2].y * b);
                z = ppc_fmadds(z, restB, corner[2].z * b);
                s3DPoint position = {x, y, z};
                matrix.m[3][0] = ppc_fmadds(c, normal.x, x);
                matrix.m[3][1] = ppc_fmadds(c, normal.y, y);
                matrix.m[3][2] = ppc_fmadds(c, normal.z, z);
                if ((entry.mFlags & 0x40000) != 0) {
                    CalculateOrientFromTrackMorph(corner, &position, &normal, morph, &matrix);
                }
            }
            std::memcpy(record.mBlock, &matrix, sizeof matrix);
        }
    }
    if ((node->mFlags & 0x10000000) != 0 && (node->mFlags & 0x10000) != 0) RecalculateInvalidNodeFaceNormals(node);
    for (tag_sNode* child = node->mF0C.get(); child != nullptr; child = child->mF08.get()) {
        if ((child->mFlags & 0x40000000) == 0) DoWorldTransformsForEmbeddedData(child, header, matrix, &matrix, records);
    }
}
