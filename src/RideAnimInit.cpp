// The ride animation set's set-up, the main app's rsse.cpp range: RAInitType (0x100c3430),
// CorrectNodeFlags1 (0x10067cf4), ResetAnimTimeStats (0x100c5a90), RAInitNodeAnimNodes
// (0x100c32ac) and InitRideAnimSet (0x100c7768). LoadRideAnimSet (src/RideAnimLoad.cpp)
// calls them. The mesh files' records keep their 32-bit layout (include/m3d2.h); the set
// and its time controls use native pointers (include/TPEngine.h).
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "platform/samsutils.h"

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

// 0x10067cf4, 236 bytes. Corrects the node flags of every animation in the set. A node
// animation with flag 0x1000 marks the header (0x2000). One with 0x10000 flags each node it
// names (0x80), and the texture of each of the node's face sets (0x8030 in the texture's
// first word). A node index past the full nodes names a mini node. For such a node the
// original reads the face count and the face set pointer from the record after it, which
// lie past the mini node's first 0x58 bytes. This reads the same bytes in host order, so the
// two agree only where those bytes are zero.
void CorrectNodeFlags1(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    for (uint32_t a = 0; a < set->mAnimationCount; ++a) {
        tag_sAnimHeader* animation = set->mAnimations[a];
        tag_sNodeAnim* nodeAnim = animation->mNodeAnims.get();
        for (uint32_t n = animation->mNodeAnimCount; n != 0; --n, ++nodeAnim) {
            const uint32_t index = nodeAnim->mF14;
            tag_sNode* node;
            if (index < header->mNodeCount) {
                node = header->mNodes.get() + index;
            } else {
                node = reinterpret_cast<tag_sNode*>(header->mMiniNodes.get() + (index - header->mNodeCount));
            }
            if ((nodeAnim->mFlags & 0x1000) != 0) header->mFlags |= 0x2000;
            if ((nodeAnim->mFlags & 0x10000) != 0) {
                sFaceSet* faceSets = node->mFaceSets.get();
                for (uint32_t f = 0; f < node->mFaceSetCount; ++f) {
                    node->mFlags |= 0x80;
                    sTexture* texture = faceSets[f].mTexture.get();
                    if (texture != nullptr) texture->mF00 |= 0x8030;
                }
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
