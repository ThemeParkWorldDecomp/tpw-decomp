// Test library for the staff panel's status and location leaves (unit v5076; libtpw_unit_hackfollowmode.dylib, tests/test_hackfollowmode.py), main app:
//   src/ui_staffPanel_msg_hackfollowmode.cpp   UpdateStatus, UpdateLocator, StaffLocationSwitch, hackfollowmode
//
// The callees outside the unit are recording fakes (GetNextStaffMember, the window members, the CStaff queries, TbITable::FormatString, the
// drawing and zoom calls). The windows are real InterfaceWindow objects of this library whose constructor and virtuals are defined here: SetValue
// and SetText record events, the rest do nothing. The globals of other files are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_main.h"

using namespace calllog;

// gText's member constructor lives in a file this library does not compile; gText is never used except for its resource pointer.
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*) {}
InterfaceFont::InterfaceFont() {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
InterfaceFont::~InterfaceFont() {}
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// ---- globals of other files, defined here
uint32_t gFirstPersonFlags;
CText gText;
char16_t gUIMainConvertBuffer[0x4000];
InterfaceWindow* gMainPanel;
CThing* gStaffPanelThing;
InterfaceWindow* gStaffPanelC;

// ---- the test's own state
static CThing* gNextThing;            // what GetNextStaffMember makes the panel's thing when its answer is 1
static InterfaceWindow* gKids[3];     // the children 0x147, 0x148 and 0x14a of the panel

// ---- InterfaceWindow: constructor and virtuals (the members the unit calls are fakes below)
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this), lab(text)});
    return 0;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0x147) return gKids[0];
    if (id == 0x148) return gKids[1];
    if (id == 0x14a) return gKids[2];
    return nullptr;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
void InterfaceWindow::unlink() { ev("unlink", {lab(this)}); }
void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) {
    ev("link", {lab(this), lab(child), static_cast<uint8_t>(a), static_cast<uint8_t>(b)});
}

// ---- the other callees
void UIMain_ZoomToPeep(CPeep* peep) { ev("ZoomToPeep", {lab(peep)}); }
void GetNextStaffMember() {
    ev("GetNextStaffMember", {gStaffPanelLocation, lab(gStaffPanelThing)});
    gStaffPanelThing = ret("GetNext") ? gNextThing : nullptr;
}
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    ev("DrawLine", {static_cast<int32_t>(mode), x, y, *endX, *endY});
}
int32_t UIMain_GetStaffStateFrame(int32_t state) {
    ev("GetStaffStateFrame", {state});
    return static_cast<int32_t>(ret("frame"));
}
int32_t CStaff::GetGeneralStaffState() const {
    ev("GetGeneralStaffState", {lab(this)});
    return static_cast<int32_t>(ret("state"));
}
const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return gUIMainConvertBuffer + 0x3800;
}
int32_t CStaff::HasPatrolArea() const {
    ev("HasPatrolArea", {lab(this)});
    return static_cast<int32_t>(ret("has"));
}
void CStaff::GetPatrolArea(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* start,
                           tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* end) const {
    ev("GetPatrolArea", {lab(this)});
    start->mIndex = static_cast<uint16_t>(ret("pstart"));
    end->mIndex = static_cast<uint16_t>(ret("pend"));
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    if (id == 0x1c8) {
        va_list args;
        va_start(args, id);
        TbIntegerVTag* tag = va_arg(args, TbIntegerVTag*);
        va_end(args);
        ev("FormatString", {lab(this), lab(out), static_cast<int64_t>(size), static_cast<int64_t>(id), tag->mMagic, 1, tag->mId, tag->mValue});
        out[0] = static_cast<char16_t>(0x41 + (id & 0xf));
        out[1] = static_cast<char16_t>(tag->mValue);
    } else {
        ev("FormatString", {lab(this), lab(out), static_cast<int64_t>(size), static_cast<int64_t>(id)});
        out[0] = static_cast<char16_t>(0x41 + (id & 0xf));
        out[1] = 0x2d;
    }
    out[2] = 0;
    return 0;
}

extern "C" {

uint64_t tpw_sp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffPanelThing},
        {"panel", &gStaffPanelC},
        {"main", &gMainPanel},
        {"flags", &gFirstPersonFlags},
        {"loc", &gStaffPanelLocation},
        {"guard", &gStaffPanelFrameGuard},
        {"frame", &gStaffPanelFrame},
        {"buf", gUIMainConvertBuffer},
        {"table", &gText.mResourcePointers[6]},
        {"next", &gNextThing},
        {"kids", gKids},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sp_mkwin() {
    void* memory = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0)));
}

int32_t tpw_sp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    return n;
}

void tpw_sp_status() { UpdateStatus(); }
void tpw_sp_locator() { UpdateLocator(); }
void tpw_sp_switch(int32_t which) { StaffLocationSwitch(which); }
void tpw_sp_hack() { hackfollowmode(); }

}  // extern "C"
