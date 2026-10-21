// Entry points for the ride animation set unit (src/game/rides/RideAnim.cpp, src/game/rides/TPRide.cpp). The file records are
// built in the arena, since their Ptr32 fields need it; the set and the records the code
// builds use native pointers, as in the game. Each entry point frees the blocks of the
// previous call first.
#include "TPEngine.h"
#include "m3d2.h"
#include "TPGlobals.h"
#include "fake_strings.h"
#include "calllog.h"
#include "platform/arena.h"
#include <cstdint>
#include <cstring>
#include <vector>

// TPGlobals is defined in src/game/TPGlobals.cpp, which this unit does not link: the constructors are
// empty and the object is zeroed, and the test writes the two words the time set-up reads.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;

// The byte swap of the morph points: the fake records which morph block it was given (its
// index in gSwapBlocks, -1 for none) and the count.
std::vector<const void*> gSwapBlocks;
namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) {
    int64_t index = -1;
    for (size_t i = 0; i < gSwapBlocks.size(); ++i) {
        if (gSwapBlocks[i] == words) index = static_cast<int64_t>(i);
    }
    calllog::ev("Swap", {index, count});
}
}  // namespace SamsUtilities

namespace {
std::vector<void*> gBlocks;

// A zeroed arena block that lives until the next entry point.
void* Block(size_t bytes) {
    void* p = tpw::ArenaAllocZeroed(bytes);
    gBlocks.push_back(p);
    return p;
}

void FreeBlocks() {
    for (void* p : gBlocks) tpw::ArenaFree(p);
    gBlocks.clear();
}
}  // namespace

// MReleaseAllocF (the engine's zeroed allocation): records the call (its ordinal, the flags, and the
// size for the third call, the blocks) and hands out an arena block. gAllocs keeps the blocks in order.
std::vector<void*> gAllocs;
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    const int64_t ordinal = static_cast<int64_t>(gAllocs.size());
    calllog::ev("Alloc", {ordinal, flags, ordinal == 2 ? static_cast<int64_t>(size) : 0});
    void* block = Block(size);
    gAllocs.push_back(block);
    return block;
}

// RAInitType on a mesh whose header has `flags`, and an animation with `count` node
// animations (or none). out receives the result and the header's flags after the call.
extern "C" void tpw_rai_type(uint32_t flags, uint32_t count, int32_t hasAnimation, uint32_t* out) {
    FreeBlocks();
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mFlags = flags;
    tag_sMesh mesh{};
    mesh.mHeader = header;
    tag_sAnimHeader animation{};
    animation.mNodeAnimCount = static_cast<uint16_t>(count);
    out[0] = static_cast<uint32_t>(RAInitType(&mesh, hasAnimation != 0 ? &animation : nullptr));
    out[1] = header->mFlags;
}

// CorrectNodeFlags1 on a set built from the words the test writes (cnf_words in
// tests/test_RideAnimInit.py): header flags, node, mini node, texture and animation counts;
// then per node its flags, face set count and the face sets' texture indices (-1 for none);
// per mini node its flags; per texture its first word; per animation its node animation count
// and the flags and index of each. out receives the header flags, the node flags, the mini
// node flags and the texture words.
extern "C" void tpw_cnf(const uint32_t* w, uint32_t* out) {
    FreeBlocks();
    size_t p = 0;
    const uint32_t headerFlags = w[p++];
    const uint32_t nodeCount = w[p++];
    const uint32_t miniCount = w[p++];
    const uint32_t textureCount = w[p++];
    const uint32_t animationCount = w[p++];
    // The node, mini node and texture tables are padded, so the reads past a mini node stay
    // inside the block.
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mFlags = headerFlags;
    header->mNodeCount = static_cast<uint16_t>(nodeCount);
    auto* nodes = static_cast<tag_sNode*>(Block(nodeCount * sizeof(tag_sNode) + 0x40));
    auto* minis = static_cast<tag_sMiniNode*>(Block(miniCount * sizeof(tag_sMiniNode) + 0x40));
    auto* textures = static_cast<sTexture*>(Block(textureCount * sizeof(sTexture) + 0x40));
    header->mNodes = nodes;
    header->mMiniNodes = minis;
    for (uint32_t i = 0; i < nodeCount; ++i) {
        tag_sNode& node = nodes[i];
        node.mFlags = w[p++];
        const uint32_t faces = w[p++];
        node.mFaceSetCount = static_cast<uint16_t>(faces);
        if (faces != 0) {
            auto* faceSets = static_cast<sFaceSet*>(Block(faces * sizeof(sFaceSet)));
            node.mFaceSets = faceSets;
            for (uint32_t f = 0; f < faces; ++f) {
                const uint32_t index = w[p++];
                if (index != 0xffffffffu) faceSets[f].mTexture = &textures[index];
            }
        }
    }
    for (uint32_t j = 0; j < miniCount; ++j) minis[j].mFlags = w[p++];
    for (uint32_t t = 0; t < textureCount; ++t) textures[t].mF00 = static_cast<uint16_t>(w[p++]);
    std::vector<tag_sAnimHeader*> animations;
    for (uint32_t a = 0; a < animationCount; ++a) {
        auto* animation = static_cast<tag_sAnimHeader*>(Block(sizeof(tag_sAnimHeader)));
        const uint32_t count = w[p++];
        animation->mNodeAnimCount = static_cast<uint16_t>(count);
        auto* nodeAnims = static_cast<tag_sNodeAnim*>(Block(count * sizeof(tag_sNodeAnim) + 0x40));
        for (uint32_t b = 0; b < count; ++b) {
            nodeAnims[b].mFlags = w[p++];
            nodeAnims[b].mF14 = static_cast<uint16_t>(w[p++]);
        }
        animation->mNodeAnims = nodeAnims;
        animations.push_back(animation);
    }
    tag_sMesh mesh{};
    mesh.mHeader = header;
    tag_sRideAnimSet set{};
    set.mHeads = &mesh;
    set.mAnimationCount = animationCount;
    set.mAnimations = animations.data();
    CorrectNodeFlags1(&set);
    size_t o = 0;
    out[o++] = header->mFlags;
    for (uint32_t i = 0; i < nodeCount; ++i) out[o++] = nodes[i].mFlags;
    for (uint32_t j = 0; j < miniCount; ++j) out[o++] = minis[j].mFlags;
    for (uint32_t t = 0; t < textureCount; ++t) out[o++] = textures[t].mF00;
}

// ResetAnimTimeStats on a control of 14 words (the first 0x38 bytes), an animation with the
// frame range (f04, f08) or none, the mode and limit, and TPGlobals' words 0x4010 and 0x4018.
// out receives the control's 14 words after the call.
extern "C" void tpw_rats(const uint32_t* controlWords, uint32_t f04, uint32_t f08, int32_t hasAnimation,
                         int32_t mode, float limit, uint32_t g4010, uint32_t g4018, uint32_t* out) {
    sAnimTimeControl control{};
    std::memcpy(&control, controlWords, sizeof control);
    tag_sAnimHeader animation{};
    animation.mF04 = f04;
    animation.mF08 = f08;
    gTPGlobals.mF4010 = g4010;
    gTPGlobals.mF4018 = g4018;
    ResetAnimTimeStats(&control, hasAnimation != 0 ? &animation : nullptr, mode, limit);
    std::memcpy(out, &control, sizeof control);
}

// TPSetupEmbeddedHeirarchy on a header with `base` (mF46) and `count` embedded records, whose
// sEmbeddedData flags are emb[], and a mesh whose set has records with flags rec[]. The nodes
// come from the words the test writes (set_words in tests/test_RideAnimInit.py): per node its
// mF50 index, flags, child and sibling (node numbers, 0xffffffff for none). The root is node
// number `root`. out receives the result, the node flags and the set's record flags.
extern "C" void tpw_set(const uint32_t* w, uint32_t* out) {
    FreeBlocks();
    size_t p = 0;
    const uint32_t first = w[p++];
    const uint32_t count = w[p++];
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mF46 = static_cast<uint16_t>(first);
    header->mEmbeddedCount = static_cast<uint16_t>(count);
    auto* embedded = static_cast<sEmbeddedData*>(Block(count * sizeof(sEmbeddedData) + 0x20));
    header->mEmbedded = embedded;
    for (uint32_t k = 0; k < count; ++k) embedded[k].mFlags = w[p++];
    std::vector<sEmbeddedRecord> records(count);
    for (uint32_t k = 0; k < count; ++k) records[k].mFlags = w[p++];
    const uint32_t nodeCount = w[p++];
    auto* nodes = static_cast<tag_sNode*>(Block(nodeCount * sizeof(tag_sNode) + 0x20));
    std::vector<uint32_t> child(nodeCount), sibling(nodeCount);
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodes[i].mF50 = static_cast<uint16_t>(w[p++]);
        nodes[i].mFlags = w[p++];
        child[i] = w[p++];
        sibling[i] = w[p++];
    }
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodes[i].mF0C = child[i] == 0xffffffffu ? nullptr : &nodes[child[i]];
        nodes[i].mF08 = sibling[i] == 0xffffffffu ? nullptr : &nodes[sibling[i]];
    }
    const uint32_t root = w[p++];
    sEmbeddedSet set{};
    set.mRecords = records.data();
    tag_sMesh mesh{};
    mesh.mF28 = &set;
    out[0] = static_cast<uint32_t>(TPSetupEmbeddedHeirarchy(&mesh, &nodes[root], header));
    size_t o = 1;
    for (uint32_t i = 0; i < nodeCount; ++i) out[o++] = nodes[i].mFlags;
    for (uint32_t k = 0; k < count; ++k) out[o++] = records[k].mFlags;
}

// RAInitNodeAnimNodes on a mesh of flags `meshFlags`, nodes (mF50, flags, vertex count), `minis`
// mini nodes and an animation of node animations (flags, index, has morph points), from the words
// the test writes (rnai_words in tests/test_RideAnimInit.py). Every node animation starts at the
// first node (the first mini node when there are no full nodes). out receives the node flags,
// the mini node flags and each node animation's node number (full nodes first, then minis, -1 for
// none). The swap calls are in the call log.
extern "C" void tpw_rnai(const uint32_t* w, uint32_t* out) {
    FreeBlocks();
    gSwapBlocks.clear();
    size_t p = 0;
    const uint32_t meshFlags = w[p++];
    const uint32_t nodeCount = w[p++];
    const uint32_t miniCount = w[p++];
    const uint32_t firstKind = w[p++];
    const uint32_t animCount = w[p++];
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mNodeCount = static_cast<uint16_t>(nodeCount);
    auto* nodes = static_cast<tag_sNode*>(Block(nodeCount * sizeof(tag_sNode) + 0x40));
    auto* minis = static_cast<tag_sMiniNode*>(Block(miniCount * sizeof(tag_sMiniNode) + 0x40));
    header->mNodes = nodes;
    header->mMiniNodes = minis;
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodes[i].mF50 = static_cast<uint16_t>(w[p++]);
        nodes[i].mFlags = w[p++];
        nodes[i].mVertexCount = static_cast<uint16_t>(w[p++]);
    }
    for (uint32_t j = 0; j < miniCount; ++j) minis[j].mFlags = 0;
    tag_sMesh mesh{};
    mesh.mF00 = meshFlags;
    mesh.mHeader = header;
    auto* anims = static_cast<tag_sNodeAnim*>(Block(animCount * sizeof(tag_sNodeAnim) + 0x40));
    for (uint32_t b = 0; b < animCount; ++b) {
        anims[b].mFlags = w[p++];
        anims[b].mF14 = static_cast<uint16_t>(w[p++]);
        const bool hasMorph = w[p++] != 0;
        anims[b].mNode = nodeCount != 0 ? &nodes[0] : reinterpret_cast<tag_sNode*>(&minis[0]);
        if (hasMorph) {
            auto* points = static_cast<sMorphPoints*>(Block(sizeof(sMorphPoints)));
            void* block = Block(16);
            points->mF00 = block;
            anims[b].mMorph = reinterpret_cast<sMorphSet*>(points);
            gSwapBlocks.push_back(block);
        }
    }
    tag_sAnimHeader animation{};
    animation.mNodeAnimCount = static_cast<uint16_t>(animCount);
    animation.mNodeAnims = anims;
    RAInitNodeAnimNodes(&mesh, &animation, static_cast<int32_t>(firstKind));
    size_t o = 0;
    for (uint32_t i = 0; i < nodeCount; ++i) out[o++] = nodes[i].mFlags;
    for (uint32_t j = 0; j < miniCount; ++j) out[o++] = minis[j].mFlags;
    for (uint32_t b = 0; b < animCount; ++b) {
        const tag_sNode* target = anims[b].mNode.get();
        int64_t id = -1;
        for (uint32_t i = 0; i < nodeCount; ++i) {
            if (target == &nodes[i]) id = i;
        }
        for (uint32_t j = 0; j < miniCount; ++j) {
            if (target == reinterpret_cast<const tag_sNode*>(&minis[j])) id = nodeCount + j;
        }
        out[o++] = static_cast<uint32_t>(id);
    }
}

// InitRideAnimSet on a set built from the words the test writes (iras_words in
// tests/test_RideAnimInit.py): init, mode, keepNodes, the set flags, the mesh flags, the node,
// mini node, time control and animation counts; the time controls (14 words each); the nodes
// (mF50, flags, vertex count); the mini node flags; the animations (node animation count, then
// flags, index and has-morph for each); and the twelve groups (entry count, then the animation
// number of each entry, 0xffffffff for a null entry). out receives the result, the set flags,
// the time control words, the node flags, the mini node flags and the node number of every node
// animation of every animation, in order. The swap calls are in the call log.
extern "C" void tpw_iras(const uint32_t* w, uint32_t* out) {
    FreeBlocks();
    gSwapBlocks.clear();
    size_t p = 0;
    const uint32_t init = w[p++];
    const uint32_t mode = w[p++];
    const uint32_t keepNodes = w[p++];
    const uint32_t setFlags = w[p++];
    const uint32_t meshFlags = w[p++];
    const uint32_t nodeCount = w[p++];
    const uint32_t miniCount = w[p++];
    const uint32_t timeCount = w[p++];
    const uint32_t animCount = w[p++];
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mNodeCount = static_cast<uint16_t>(nodeCount);
    header->mAllNodeCount = static_cast<uint16_t>(nodeCount + miniCount);
    auto* nodes = static_cast<tag_sNode*>(Block(nodeCount * sizeof(tag_sNode) + 0x40));
    auto* minis = static_cast<tag_sMiniNode*>(Block(miniCount * sizeof(tag_sMiniNode) + 0x40));
    header->mNodes = nodes;
    header->mMiniNodes = minis;
    auto* controls = static_cast<sAnimTimeControl*>(Block(timeCount * sizeof(sAnimTimeControl) + 0x40));
    for (uint32_t t = 0; t < timeCount; ++t) {
        std::memcpy(&controls[t], w + p, sizeof(sAnimTimeControl));
        p += sizeof(sAnimTimeControl) / sizeof(uint32_t);
    }
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodes[i].mF50 = static_cast<uint16_t>(w[p++]);
        nodes[i].mFlags = w[p++];
        nodes[i].mVertexCount = static_cast<uint16_t>(w[p++]);
    }
    for (uint32_t j = 0; j < miniCount; ++j) minis[j].mFlags = w[p++];
    tag_sMesh mesh{};
    mesh.mF00 = meshFlags;
    mesh.mHeader = header;
    std::vector<tag_sAnimHeader*> animations;
    for (uint32_t a = 0; a < animCount; ++a) {
        auto* animation = static_cast<tag_sAnimHeader*>(Block(sizeof(tag_sAnimHeader)));
        const uint32_t count = w[p++];
        animation->mNodeAnimCount = static_cast<uint16_t>(count);
        auto* nodeAnims = static_cast<tag_sNodeAnim*>(Block(count * sizeof(tag_sNodeAnim) + 0x40));
        for (uint32_t b = 0; b < count; ++b) {
            nodeAnims[b].mFlags = w[p++];
            nodeAnims[b].mF14 = static_cast<uint16_t>(w[p++]);
            const bool hasMorph = w[p++] != 0;
            nodeAnims[b].mNode = nodeCount != 0 ? &nodes[0] : reinterpret_cast<tag_sNode*>(&minis[0]);
            if (hasMorph) {
                auto* points = static_cast<sMorphPoints*>(Block(sizeof(sMorphPoints)));
                void* block = Block(16);
                points->mF00 = block;
                nodeAnims[b].mMorph = reinterpret_cast<sMorphSet*>(points);
                gSwapBlocks.push_back(block);
            }
        }
        animation->mNodeAnims = nodeAnims;
        animations.push_back(animation);
    }
    tag_sRideAnimSet set{};
    set.mFlags = setFlags;
    set.mHeads = &mesh;
    set.mTimeCount = static_cast<uint16_t>(timeCount);
    set.mTimeControls = controls;
    std::vector<std::vector<sRideAnimEntry>> entries(12);
    for (uint32_t kind = 0; kind < 12; ++kind) {
        const uint32_t count = w[p++];
        for (uint32_t k = 0; k < count; ++k) {
            const uint32_t index = w[p++];
            entries[kind].push_back({k, index == 0xffffffffu ? nullptr : animations[index]});
        }
        set.mGroups[kind].mCount = count;
        set.mGroups[kind].mEntries = entries[kind].data();
    }
    const int32_t result = InitRideAnimSet(&set, static_cast<int32_t>(init), static_cast<int32_t>(mode),
                                           static_cast<int32_t>(keepNodes));
    size_t o = 0;
    out[o++] = static_cast<uint32_t>(result);
    out[o++] = set.mFlags;
    std::memcpy(out + o, controls, timeCount * sizeof(sAnimTimeControl));
    o += timeCount * sizeof(sAnimTimeControl) / sizeof(uint32_t);
    for (uint32_t i = 0; i < nodeCount; ++i) out[o++] = nodes[i].mFlags;
    for (uint32_t j = 0; j < miniCount; ++j) out[o++] = minis[j].mFlags;
    for (tag_sAnimHeader* animation : animations) {
        for (uint32_t b = 0; b < animation->mNodeAnimCount; ++b) {
            const tag_sNode* target = animation->mNodeAnims.get()[b].mNode.get();
            int64_t id = -1;
            for (uint32_t i = 0; i < nodeCount; ++i) {
                if (target == &nodes[i]) id = i;
            }
            for (uint32_t j = 0; j < miniCount; ++j) {
                if (target == reinterpret_cast<const tag_sNode*>(&minis[j])) id = nodeCount + j;
            }
            out[o++] = static_cast<uint32_t>(id);
        }
    }
}

// TPInitialiseEmbeddedData on a mesh whose header has `first` (mF46) and `count` embedded entries,
// and `total` nodes (the first `nodes` full, the rest mini nodes). The words are written by
// ied_words in tests/test_RideAnimInit.py: first, count, full node count, mini node count, the
// entry flags, then per node its flags, mF50, mF04 link, child and sibling (node numbers; 0xffffffff
// for none). Node 0 is the root (mF78). out receives the set (has, flags, block count, and whether
// the mesh's pointer, the records and the blocks are the first, second and third allocations), the
// records (flags, word 0xc, block number: 0xffffffff for none, 0xfffffffe for a stray pointer), and
// the node flags. The allocations are in the call log.
extern "C" void tpw_ied(const uint32_t* w, uint32_t* out) {
    FreeBlocks();
    gAllocs.clear();
    size_t p = 0;
    const uint32_t first = w[p++];
    const uint32_t count = w[p++];
    const uint32_t fullCount = w[p++];
    const uint32_t miniCount = w[p++];
    const uint32_t total = fullCount + miniCount;
    auto* header = static_cast<tag_sM3D2Header*>(Block(sizeof(tag_sM3D2Header)));
    header->mF46 = static_cast<uint16_t>(first);
    header->mEmbeddedCount = static_cast<uint16_t>(count);
    header->mNodeCount = static_cast<uint16_t>(fullCount);
    auto* embedded = static_cast<sEmbeddedData*>(Block(count * sizeof(sEmbeddedData) + 0x20));
    header->mEmbedded = embedded;
    for (uint32_t k = 0; k < count; ++k) embedded[k].mFlags = w[p++];
    auto* nodes = static_cast<tag_sNode*>(Block(fullCount * sizeof(tag_sNode) + 0x20));
    auto* minis = static_cast<tag_sMiniNode*>(Block(miniCount * sizeof(tag_sMiniNode) + 0x20));
    header->mNodes = nodes;
    header->mMiniNodes = minis;
    // Node i is a full node below fullCount, else a mini node. Links to a mini node are typed as
    // tag_sNode, as the original does (the code reads only the first 0x58 bytes through them).
    auto nodeAt = [&](uint32_t i) -> tag_sMiniNode* {
        return i < fullCount ? reinterpret_cast<tag_sMiniNode*>(&nodes[i]) : &minis[i - fullCount];
    };
    std::vector<uint32_t> link(total), child(total), sibling(total);
    for (uint32_t i = 0; i < total; ++i) {
        tag_sMiniNode* node = nodeAt(i);
        node->mFlags = w[p++];
        node->mF50 = static_cast<uint16_t>(w[p++]);
        link[i] = w[p++];
        child[i] = w[p++];
        sibling[i] = w[p++];
    }
    for (uint32_t i = 0; i < total; ++i) {
        tag_sMiniNode* node = nodeAt(i);
        node->mF04 = reinterpret_cast<tag_sNode*>(nodeAt(link[i]));
        node->mF0C = child[i] == 0xffffffffu ? nullptr : reinterpret_cast<tag_sNode*>(nodeAt(child[i]));
        node->mF08 = sibling[i] == 0xffffffffu ? nullptr : reinterpret_cast<tag_sNode*>(nodeAt(sibling[i]));
    }
    header->mF78 = reinterpret_cast<tag_sNode*>(nodeAt(0));
    tag_sMesh mesh{};
    mesh.mHeader = header;
    TPInitialiseEmbeddedData(&mesh);
    auto* set = static_cast<sEmbeddedSet*>(mesh.mF28);
    size_t o = 0;
    if (set == nullptr) {
        for (int k = 0; k < 6; ++k) out[o++] = 0;
    } else {
        const uint8_t* blocks = set->mBlocks;
        const uint8_t* first0 = gAllocs.size() > 2 ? static_cast<const uint8_t*>(gAllocs[2]) : nullptr;
        out[o++] = 1;
        out[o++] = set->mFlags;
        out[o++] = set->mBlockCount;
        out[o++] = static_cast<const void*>(set) == (gAllocs.size() > 0 ? gAllocs[0] : nullptr);
        out[o++] = set->mRecords == (gAllocs.size() > 1 ? gAllocs[1] : nullptr);
        out[o++] = blocks == first0;
        for (uint32_t i = 0; i < count; ++i) {
            const sEmbeddedRecord& record = set->mRecords[i];
            out[o++] = record.mFlags;
            out[o++] = record.mF0C;
            uint32_t where = 0xfffffffeu;
            if (record.mBlock == nullptr) {
                where = 0xffffffffu;
            } else if (first0 != nullptr && record.mBlock >= first0 && record.mBlock < first0 + 0x40 * set->mBlockCount &&
                       (record.mBlock - first0) % 0x40 == 0) {
                where = static_cast<uint32_t>((record.mBlock - first0) / 0x40);
            }
            out[o++] = where;
        }
    }
    for (uint32_t i = 0; i < total; ++i) out[o++] = nodeAt(i)->mFlags;
}
