// Test library for UpdateSatellite and UI2DMReturnScaleColour (unit v5193; libtpw_unit_UpdateSatellite.dylib, tests/test_UpdateSatellite.py), main app:
//   src/game/ui/ui_2dmap.cpp, src/game/ui/ui_worldLobbyController.cpp   both functions and gUISatelliteParent
//
// InterfaceWindow::GetWindow and the virtual SetValue are recording fakes (the other virtuals and the constructor are empty definitions so the
// class has a vtable here). The windows GetWindow answers are six static objects, one per id 0x16007..0x1600c.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "DrawRenderClasses.h"
#include "InterfaceWindow.h"

using namespace calllog;

CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

namespace {

alignas(InterfaceWindow) unsigned char gStore[6][sizeof(InterfaceWindow)];
InterfaceWindow* gWin[6];

InterfaceWindow* Win(int k) {
    if (!gWin[k]) gWin[k] = new (gStore[k]) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    return gWin[k];
}

}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int k = id - 0x16007;
    return (k >= 0 && k < 6) ? Win(k) : nullptr;
}

int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}

extern "C" {

uint64_t tpw_us_addr(const char* name) {
    if (std::strcmp(name, "gSat") == 0) return reinterpret_cast<uintptr_t>(&gUISatelliteParent);
    if (name[0] == 'w' && name[1] >= '0' && name[1] <= '5') return reinterpret_cast<uintptr_t>(Win(name[1] - '0'));
    return 0;
}

uint64_t tpw_us_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_us_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(bfl::draw::CColour));
    out[n++] = static_cast<int64_t>(offsetof(bfl::draw::CColour, mFormatCache));
    return n;
}

void tpw_us_update(int32_t which, int32_t value) { UpdateSatellite(which, value); }

void tpw_us_colour(uint64_t out, int32_t percent, uint64_t a, uint64_t b, uint64_t c) {
    using bfl::draw::CColour;
    *reinterpret_cast<CColour*>(out) = UI2DMReturnScaleColour(percent, *reinterpret_cast<const CColour*>(a),
                                                              *reinterpret_cast<const CColour*>(b), *reinterpret_cast<const CColour*>(c));
}

}  // extern "C"
