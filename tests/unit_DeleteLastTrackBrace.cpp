// Test library for DeleteTrackBrace and DeleteLastTrackBrace (unit v5337; libtpw_unit_DeleteLastTrackBrace.dylib,
// tests/test_DeleteLastTrackBrace.py), main app:
//   src/game/rides/CoasterEdit.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read. The globals of other files the unit reads are
// defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

void RemoveTrackBraceFromPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);
void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance);
void DeletePylon(tag_sCoasterInstance* instance, tag_sPylon* pylon, int32_t flag);

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterWord1790;
uint32_t gCoasterInputFlags;
tag_sRideAnimSet* gRideAnimSets[64];
CActionRecorder gActionRecorder;
UIInterface gUIInterface;

// ---- the fakes
void RemoveTrackBraceFromPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    ev("RemoveTrackBraceFromPylon", {lab(instance), lab(brace)});
}
void StopFlashingObject(tag_sRideAnimSet* set) { ev("StopFlashingObject", {lab(set)}); }
void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance) {
    ev("RemoveBraceFromCollisionSystem", {lab(brace), lab(instance)});
}
void DestroyTrackBrace(tag_sTrackBrace* brace, sPylonControl* control) { ev("DestroyTrackBrace", {lab(brace), lab(control)}); }
void DeletePylon(tag_sCoasterInstance* instance, tag_sPylon* pylon, int32_t flag) {
    ev("DeletePylon", {lab(instance), lab(pylon), flag});
}
void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    ev("Assign3DObjectToAIWorld", {lab(instance), a, b, c, d});
}
void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon) {
    ev("UpdatePylonStackStructures", {lab(control), lab(pylon)});
}
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b) {
    ev("UpdatePylonStackMeshes", {lab(control), lab(pylon), a, b});
}
void SetPotentialPylonArea() { ev("SetPotentialPylonArea", {}); }
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {lab(instance), flag}); }
void GetCoasterEditAvailableButtons(uint32_t* out) { ev("GetCoasterEditAvailableButtons", {static_cast<int64_t>(out != nullptr)}); }
void SetCoasterButtonState(uint32_t state, uint32_t arg) { ev("SetCoasterButtonState", {state, arg}); }
void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { ev("ChangeCoasterButtonsEnabled", {lab(this), mask}); }
void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { ev("ChangeCoasterButtonsDown", {lab(this), mask}); }
void CActionRecorder::RecordAction(RecordedAction action) { ev("RecordAction", {lab(this), static_cast<int32_t>(action)}); }

extern "C" {

uint64_t tpw_dltb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"edit", &gCoasterEditInstance},  {"enabled", &gCoasterEditEnabled}, {"down", &gCoasterEditDown},
        {"drag", &gCoasterDragBrace},     {"w1790", &gCoasterWord1790},      {"input", &gCoasterInputFlags},
        {"sets", &gRideAnimSets[0]},      {"recorder", &gActionRecorder},    {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dltb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host's offsets, in the order of the module's MAC list.
int32_t tpw_dltb_layout(int64_t* out) {
    int32_t n = 0;
    auto o = [&](int64_t v) { out[n++] = v; };
    o(offsetof(tag_sCoasterInstance, mFlags));
    o(offsetof(tag_sCoasterInstance, mType));
    o(offsetof(tag_sCoasterInstance, mBlock3C));
    o(offsetof(tag_sCoasterInstance, mBraceCount));
    o(offsetof(tag_sCoasterInstance, mLastBrace));
    o(offsetof(tag_sCoasterInstance, mBraces));
    o(offsetof(tag_sCoasterInstance, mBraceB5C));
    o(offsetof(tag_sCoasterInstance, mPreviewBrace));
    o(offsetof(tag_sCoasterInstance, mBrace64));
    o(offsetof(tag_sCoasterInstance, mF118));
    o(offsetof(tag_sCoasterInstance, mFlashBrace));
    o(sizeof(tag_sCoasterInstance));
    o(offsetof(tag_sTrackBrace, mNumber));
    o(offsetof(tag_sTrackBrace, mFlags));
    o(offsetof(tag_sTrackBrace, mPrev));
    o(offsetof(tag_sTrackBrace, mNext));
    o(offsetof(tag_sTrackBrace, mF6C));
    o(offsetof(tag_sTrackBrace, mWord70));
    o(offsetof(tag_sTrackBrace, mData));
    o(sizeof(tag_sTrackBrace));
    o(offsetof(sTrackBraceData, mFlags));
    o(offsetof(sTrackBraceData, mCellX));
    o(offsetof(sTrackBraceData, mCellZ));
    o(offsetof(sTrackBraceData, mF18Table));
    o(offsetof(sTrackBraceData, mF30));
    o(offsetof(sTrackBraceData, mOwner));
    o(sizeof(sTrackBraceData));
    o(offsetof(sPylonControl, mF2C));
    o(sizeof(sPylonControl));
    o(offsetof(tag_sRideAnimSet, mFlags));
    o(offsetof(tag_sRideAnimSet, mHeads));
    o(sizeof(tag_sRideAnimSet));
    o(offsetof(tag_sMesh, mF28));
    o(sizeof(tag_sMesh));
    o(reinterpret_cast<const char*>(&gActionRecorder.mRecording) - reinterpret_cast<const char*>(&gActionRecorder));
    o(0xa4);   // the type's word, read by its offset
    o(0xc0);
    o(0x70);   // the owner's word, read by its offset
    o(0x90);
    o(4);      // the set's word 4, the records
    o(0x14);   // a record's size
    o(4);      // a record's word 4, the block
    return n;
}

void tpw_dltb_delete(uint64_t instance, uint64_t brace) {
    DeleteTrackBrace(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<tag_sTrackBrace*>(brace));
}
void tpw_dltb_deletelast() { DeleteLastTrackBrace(); }

}  // extern "C"
