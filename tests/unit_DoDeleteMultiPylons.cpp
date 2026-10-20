// Test library for DoDeleteMultiPylons (unit v5647; libtpw_unit_DoDeleteMultiPylons.dylib, tests/test_DoDeleteMultiPylons.py), main app:
//   src/DisableCurrentPylonEditState_SetCoasterEditState_DoDeleteMultiPylons.cpp   DoDeleteMultiPylons
//
// The callees outside the unit are fakes that record what the real ones read; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ActionRecorder.h"
#include "UIInterface.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
tag_sCoasterInstance* gCoasterEditInstance;
int32_t gExtrudeTextureKind;
uint32_t gCoasterEditEnabled;
uint32_t gCoasterEditDown;
tag_sTrackBrace* gPylonMoveMark2;
tag_sTrackBrace* gPylonMoveMark1;
uint32_t gCoasterWord1788;
tag_sTrackBrace* gCoasterDragBrace;
uint32_t gCoasterInputFlags;
tag_sRideAnimSet* gRideAnimSets[8];
CActionRecorder gActionRecorder;
UIInterface gUIInterface;

namespace {
int32_t IsInst(const tag_sCoasterInstance* instance) { return instance == gCoasterEditInstance ? 1 : 0; }
}  // namespace

// ---- the fakes
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) { ev("FlagGenerateTrack", {IsInst(instance), flag}); }
void DeleteTrackBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) { ev("DeleteTrackBrace", {IsInst(instance), static_cast<int64_t>(brace->mNumber)}); }
void DestroyEndPylons(tag_sCoasterInstance* instance) { ev("DestroyEndPylons", {IsInst(instance)}); }
void DisableCurrentPylonEditState() { ev("DisableCurrentPylonEditState", {}); }
void SetCoasterButtonState(uint32_t state, uint32_t arg) { ev("SetCoasterButtonState", {static_cast<int64_t>(state), static_cast<int64_t>(arg)}); }
void GetCoasterEditAvailableButtons(uint32_t* out) { ev("GetCoasterEditAvailableButtons", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(out))}); }
void SetPotentialPylonArea() { ev("SetPotentialPylonArea", {}); }
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) { ev("SetCoasterState", {IsInst(instance), static_cast<int64_t>(state)}); }
void CoasterPlaceMouseMode() { ev("CoasterPlaceMouseMode", {}); }

void CActionRecorder::RecordAction(RecordedAction action, uint16_t a, uint16_t b) {
    ev("RecordAction", {this == &gActionRecorder ? 1 : 0, static_cast<int64_t>(action), static_cast<int64_t>(a), static_cast<int64_t>(b)});
}

void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { ev("ChangeCoasterButtonsEnabled", {this == &gUIInterface ? 1 : 0, static_cast<int64_t>(mask)}); }
void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { ev("ChangeCoasterButtonsDown", {this == &gUIInterface ? 1 : 0, static_cast<int64_t>(mask)}); }

extern "C" {

uint64_t tpw_ddmp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"inst", &gCoasterEditInstance}, {"ext", &gExtrudeTextureKind},     {"enabled", &gCoasterEditEnabled}, {"down", &gCoasterEditDown},
        {"mark2", &gPylonMoveMark2},     {"mark1", &gPylonMoveMark1},       {"w1788", &gCoasterWord1788},      {"drag", &gCoasterDragBrace},
        {"flags", &gCoasterInputFlags},  {"rideanim", &gRideAnimSets[0]},      {"startbrace", &gCoasterStartBrace},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ddmp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ddmp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mType));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBlock3C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF40));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBraces));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF118));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mPreviewBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mBrace64));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mWord70));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mData));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(sTrackBraceData, mF18Table));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterType, mFA4));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(sPylonHeadTable, mEntries));
    out[n++] = static_cast<int64_t>(offsetof(sPylonHeadEntry, mRec));
    out[n++] = static_cast<int64_t>(sizeof(sPylonHeadEntry));
    out[n++] = static_cast<int64_t>(offsetof(sPylonHeadRec, mY));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mNumber));
    return n;
}

void tpw_ddmp_run() { DoDeleteMultiPylons(); }

}  // extern "C"
