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
#include <cmath>
#include <cstring>

#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

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

// The frames a time control has run, in 30ths of a second (as in src/RideAnimTick.cpp): the
// milliseconds between its start word (mF10) and its current one (mF14) as an unsigned count,
// at the control's rate.
float RunFrames(const sAnimTimeControl* control) {
    const uint32_t elapsed = static_cast<uint32_t>(control->mF14) - static_cast<uint32_t>(control->mF10);
    return control->mF0C * ((30.0f * static_cast<float>(elapsed)) / 1000.0f);
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

// 0x100c5d3c, 60 bytes.
int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b) {
    return EntryExists(set, a, b) ? 1 : 0;
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

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    SetCurRideAnimDuration(set, a, anim, loop, speed, b);
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
