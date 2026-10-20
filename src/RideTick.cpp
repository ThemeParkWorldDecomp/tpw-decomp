// The ride animation clock and the step that moves a ride set's animations on (main app of
// SimTheme Park 2.04b3; the inventory's rsse.cpp range): TickRideAnims 0x100c6964 (152 bytes)
// and AnimateRideSet 0x100c74d4 (600 bytes). The animation of one time control
// (DoAnimateRideSet, 0x100c6c2c) and everything else they call is not part of this file.
#include "Clock.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "rsse.h"
#include "tb_resource_system.h"

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
