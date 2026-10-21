// TPAdvisor.cpp: name proven by __sinit_TPAdvisor_cpp in the main application.

#include "Debug.h"
#include "MapWho.h"
#include "Quickload.h"
#include "rsse.h"
#include "tb_resource_system.h"
#include "TPAdvisor.h"
#include "TPEngine.h"
#include <cstdio>

// TPAdvisor.cpp (main app of SimTheme Park 2.04b3): the advisor's model, 0x10031bc0..0x1003228c, and the slots'
// constructor 0x10032454. (The file's text reader is in src/TPAdvisor.cpp, which two other unit libraries link, so the
// model is kept apart.)
//
// The game keeps four slots (gAdvisorStore), each a ride animation set loaded from "Advisor<variant>.md2" in the global
// advisor folder, an instance of it that is drawn and animated (group 5 holds his animations), a matrix to stand on and a
// light. The instance is drawn in the interface's space, scaled down, lit by the slot's directional light, which is linked
// into map list 0x10. LoadAdvisor makes all that; ADV_Tick and DressAdvisor (src/Advisor.cpp) drive it.

sAdvisorStore gAdvisorStore[4];   // 0x102b56fc

// Loads and instances the advisor mesh "Advisor<variant>.md2" into slot `which`: the ride set (flags 0x50120, one time
// control), its instance (0x2117, drawn in the interface's space with the slot's matrix and linked into map list 0x10),
// at the matrix's translation (0.6, -0.6, 0.2) and white light along (26.67, -26.67, 800), the root node scaled to
// (0.01125, 0.001, 0.015). Nothing is checked: a mesh that does not load leaves the previous instance in place, and
// none at all faults (the flags are set on a NULL instance, as the original does).
void LoadAdvisor(char* path, char* folder, char* variant, uint32_t which) {
    sAdvisorStore& slot = gAdvisorStore[which];
    SetMatrixIdentity(&slot.mMatrix);
    slot.mMatrix.m[3][0] = 0.6f;
    slot.mMatrix.m[3][1] = -0.6f;
    slot.mMatrix.m[3][2] = 0.2f;
    gei_System.UpdateLights();
    slot.mLight.mColorWord = 0xffffffffu;
    slot.mLight.mColor[0] = 1.0f;
    slot.mLight.mColor[1] = 1.0f;
    slot.mLight.mColor[2] = 1.0f;
    slot.mLight.mColor[3] = 1.0f;
    gei_System.UpdateLights();
    slot.mLight.mNormal = {26.666668f, -26.666668f, 800.0f};
    char name[1024];
    if (variant != nullptr) {
        std::snprintf(name, sizeof name, "Advisor%s.md2", variant);
    } else {
        std::snprintf(name, sizeof name, "Advisor.md2");
    }
    SetupDirectoryForMeshLoad(name, path, folder);
    LoadRideAnimSet(name, 0x50120, &slot.mSet, nullptr, 1);
    InstanceRideAnimSet(slot.mSet, nullptr, 0x2117, &slot.mInstance, 0);
    InitRideAnimSet(slot.mInstance, 1, 1, 1);
    slot.mInstance->mFlags |= 4;
    if (slot.mInstance != nullptr) {
        SetMeshInUISpace(slot.mInstance->mHeads);
        slot.mInstance->mMatrix = &slot.mMatrix;
        gei_MapWho->Link(&slot.mLight, 0x10);
        s3DPoint scale = {0.015f, 0.001f, 0.015f};
        scale.x = scale.x * 0.75f;
        ScaleLMatrixNoTrans(&slot.mInstance->mHeads->mHeader->mF78->mMatrix, &scale);
    }
}

// Takes the advisor off the screen (and starts nothing): when the mesh is drawn, the animation set to "none" (kind 12,
// the special flag 2) and the mesh's drawing switched off (over the map range the set was drawn for, when it has one).
void DisableAdvisorDisplay(uint32_t which) {
    tag_sRideAnimSet* set = gAdvisorStore[which].mInstance;
    if (set == nullptr) return;
    if (IsMeshDrawEnabled(set->mHeads) == 0) return;
    CTRACE(5, "Kill advisor\n");
    SetCurRideAnim(set, 0xc, 0, 2, 1.0f, 0);
    if ((set->mFlags & 0x4000) != 0) {
        DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    } else {
        DisableMeshDraw(set->mHeads);
    }
}

void TickAdvisor(uint32_t which) {
    tag_sRideAnimSet* set = gAdvisorStore[which].mInstance;
    if (set != nullptr) AnimateRideSet(set, 1, 0);
}

// Unlinks the light, destroys the instance and the set. Tests the instance only: a slot with a set and no instance is
// left as it is.
void DestroyAdvisor(uint32_t which) {
    sAdvisorStore& slot = gAdvisorStore[which];
    if (slot.mInstance == nullptr) return;
    gei_MapWho->Unlink(&slot.mLight, 0x10);
    DestroyRideAnimSetInstance(slot.mInstance, nullptr, 0);
    DestroyRideAnimSet(slot.mSet, 0);
    slot.mInstance = nullptr;
    slot.mSet = nullptr;
}

// Starts animation `animation` (1 to 15, group 5) with the special flag 0x10; the answer is SetCurRideAnim's time.
uint32_t StartAdvisorAnimation(uint32_t animation, uint32_t which) {
    CTRACE(5, "Initial advisor call\n");
    tag_sRideAnimSet* set = gAdvisorStore[which].mInstance;
    if (set == nullptr) return 1000;
    return SetCurRideAnimDuration(set, 5, animation - 1, 0x10, 1.0f, 0);
}

namespace {
// The node of the embedded entry `accessory` (flag 0x400) of slot `which`'s mesh: the entry's place plus the header's
// first embedded node (mF46) picks a full node, or past those a mini node. NULL when there is no such entry.
tag_sMiniNode* AccessoryNode(uint32_t accessory, uint32_t which) {
    tag_sRideAnimSet* set = gAdvisorStore[which].mInstance;
    const uint32_t entry = FindDataNodeIndexByID(set, 0x400, accessory);
    if (entry == 0xffffffffu) return nullptr;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    const uint32_t place = entry + header->mF46;
    if (place < header->mNodeCount) return &header->mNodes[place];
    return &header->mMiniNodes[place - header->mNodeCount];
}
}  // namespace

// Flag 0x10 on a node is "not drawn": Enable clears it, Disable sets it.
void EnableAdvisorAccessory(uint32_t accessory, uint32_t which) {
    tag_sMiniNode* node = AccessoryNode(accessory, which);
    if (node != nullptr) node->mFlags &= ~0x10u;
}

void DisableAdvisorAccessory(uint32_t accessory, uint32_t which) {
    tag_sMiniNode* node = AccessoryNode(accessory, which);
    if (node != nullptr) node->mFlags |= 0x10u;
}

tag_sRideAnimSet* GetAdvisorRideAnim(uint32_t which) { return gAdvisorStore[which].mInstance; }
