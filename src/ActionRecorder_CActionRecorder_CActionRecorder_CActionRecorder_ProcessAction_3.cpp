// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.
#include <cstdint>
#include <new>
#include "ActionRecorder.h"
#include "Debug.h"
#include "LbFile.h"
#include "Serialise.h"
#include "bfl_FileSystem.h"
#include "Ptr32.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include "TbTList.h"
#include "Game.h"
#include "ExitErr.h"
#include "InGame.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "keytable_actions.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);

uint8_t gInGameFontNode[16];   // 0x1037f1b4
uint8_t gInGameFontDtor[8];    // stands for the destructor word 0x102673f0

namespace v51072_callee {
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }
__attribute__((weak)) void ThingIdListRemoveAll(void* list) {
    using List = TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>;
    static_cast<List*>(list)->RemoveAll();
}
}  // namespace v51072_callee

// 0x10146524: the replay of an action that is no longer used; only the trace (the text is not read).
void CActionRecorder::ProcessAction(int32_t action, uint8_t count, char16_t* text) {
    CTRACE(2, "AR replay - %d (%d, %d) - NO LONGER USED\n", action, count);
}

// 0x10145360, 0x10145318 and 0x10143cac: the Mac's bodies only set r3 (1, 1 and 0); the host declares them void.
void CActionRecorder::GetNextCameraFileEntry() {}
void CActionRecorder::GetNextTimingFileEntry() {}
void CActionRecorder::SaveRecordingToDisk() {}

namespace {
struct WideRep {
    uint32_t mF00;       // 0x00
    uint32_t mF04;       // 0x04
    int32_t mRefs;       // 0x08 the share count (0 is taken as 1)
    Ptr32<void> mBuffer; // 0x0c
};
struct WideString {
    Ptr32<WideRep> mRep; // 0x00
};
}  // namespace

// 0x101468f8: gives up the share; the last owner frees the buffer and the representation.
void WideStringDestroy(void* string, int16_t flag) {
    WideString* self = static_cast<WideString*>(string);
    if (self == nullptr) return;
    WideRep* rep = self->mRep.get();
    if (rep->mRefs == 0) rep->mRefs = 1;
    int32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) self->mRep.mOffset = 0;
    if (self->mRep.get() != nullptr) {
        v51072_callee::OperatorDelete(self->mRep->mBuffer.get());
        v51072_callee::OperatorDelete(self->mRep.get());
    }
    if (flag > 0) v51072_callee::OperatorDelete(self);
}

// 0x101437e0
void CMemFileDestroy(void* file, int16_t flag) {
    bfl::FileSystem::CMemFile* self = static_cast<bfl::FileSystem::CMemFile*>(file);
    if (self == nullptr) return;
    self->bfl::FileSystem::CFile::~CFile();
    if (flag > 0) v51072_callee::OperatorDelete(self);
}

// 0x10141e34
void ThingIdListDestroy(void* list, int16_t flag) {
    if (list == nullptr) return;
    v51072_callee::ThingIdListRemoveAll(list);
    if (flag > 0) v51072_callee::OperatorDelete(list);
}

// 0x10142008
void ThingArrayEntryInit(uint32_t* entry) {
    entry[0] = 0;
    entry[1] = 0;
    entry[2] = 0;
    entry[3] = 0;
    entry[4] = 0;
}

// 0x1014356c
void VisitorStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

// 0x1014350c
void VisitorStatusPanel::Locate() { ShowStaffPanel(gStaffRoomPanelThing); }

// 0x101434a0: the panel's window word is at the Mac's +0xc.
void VisitorStatusPanel::KidGone(CThing* thing) {
    if (reinterpret_cast<UIStatusPanelFields*>(this)->mWindow) reinterpret_cast<UIStatusPanel*>(this)->ObjectGone(thing);
}

void VisitorStatusPanel::DeInitWindow() {}         // 0x10143084
void VisitorStatusPanel::DeInitStatusWindow() {}   // 0x10142aa0
void VisitorStatusPanel::DeInitViewWindow() { UIMain_DestroySpritePreview(); }   // 0x10142110
void VisitorStatusPanel::InitViewWindow() {}       // 0x101420d0

// 0x10142098
CMap* CWorldSavvy::GetMap() const { return gWorldMap; }

// 0x1014b5cc
void __sinit_InGame_cpp() {
    new (&gInGameFont) InterfaceFont();
    __register_global_object(&gInGameFont, gInGameFontDtor, gInGameFontNode);
}

// 0x1014bf38
void __sinit_ExitErr_cpp() {
    gExitError.mError = 0;
    gExitError.mText[0] = 0;
}

// 0x1014c250: the map is embedded in the world at +0x2d8.
int32_t cheat_cleanup_park() {
    reinterpret_cast<CMap*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8)->CleanUpPark();
    return 1;
}

int32_t show_script() { return 1; }     // 0x1014c4e0
int32_t LoadPtclLib() { return 1; }     // 0x1014c50c
int32_t save_recording() { return 1; }  // 0x1014c660

// 0x1014c538, 0x1014c594, 0x1014c5f8
int32_t recreate_recording() {
    gActionRecorder.ReplayRecording(static_cast<ReplayType>(0));
    return 1;
}
int32_t recreate_recording_real_time() {
    gActionRecorder.ReplayRecording(static_cast<ReplayType>(2));
    return 1;
}
int32_t recreate_recording_medium_speed() {
    gActionRecorder.ReplayRecording(static_cast<ReplayType>(1));
    return 1;
}

// 0x1014c690
int32_t pause() {
    gGame.TogglePause(true);
    return 1;
}
