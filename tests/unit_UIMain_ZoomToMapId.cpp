// Test library for seven small ui_main functions (unit v5723; libtpw_unit_UIMain_ZoomToMapId.dylib, tests/test_UIMain_ZoomToMapId.py), main app:
//   src/game/ui/ui_main.cpp
//
// The callees outside the unit (SetCameraPOI, CPeep::GetInterpolatedPeepPosition, SCR_KillScript, ESP_Update2DScript, PAR_InitEmitter,
// DestroyRidePreview, InterfaceWindow::GetVisibility / GetPosition, CoolRect and CoolRegion, UKeyboard) are recording fakes. The globals of other
// files are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "TPCamera.h"
#include "GameSys.h"
#include "Game.h"
#include "TbAudioSystem.h"
#include "UI_Base.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
CPeep* gSpritePreviewPeep = nullptr;
int32_t gSpritePreviewHandle = 0;
sRidePreviewInfo* gRidePreview = nullptr;
int32_t gTickNow;
int32_t gLastTurnTime;
SamsUtilities::UKeyboard* SamsUtilities::gKeyboard = nullptr;
// gGame without CGame's constructor: raw storage under the program's symbol.
alignas(16) char gGameBytes[0x10000] asm("_gGame");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

char* gWinBuf;
char* gPeepBuf;
char* gRideBuf;
char* gIdBuf;
char* gKbBuf;
bool gQuiet = false;

class MockKeyboard : public SamsUtilities::UKeyboard {
public:
    uint32_t GetModifiers() override {
        ev("GetModifiers", {});
        return static_cast<uint32_t>(ret("mods"));
    }
};

float BitsToFloat(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

// ---- the fakes
void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value) {
    ev("SetCameraPOI", {x ? CanonBits(*x) : -1, z ? CanonBits(*z) : -1, zoom != nullptr, yaw != nullptr, value != nullptr});
}

void CPeep::GetInterpolatedPeepPosition(float t, int32_t ranged, float& x, float& y, float& z, uint32_t& stamp) {
    ev("Interp", {lab(this), CanonBits(t), ranged});
    x = BitsToFloat(ret("px"));
    y = BitsToFloat(ret("py"));
    z = BitsToFloat(ret("pz"));
    stamp = static_cast<uint32_t>(ret("pst"));
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

void ESP_Update2DScript(int32_t handle, int32_t source) { ev("ESP_Update2DScript", {handle, source}); }

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return 0;
}

void DestroyRidePreview(sRidePreviewInfo* info) { ev("DestroyRidePreview", {lab(info)}); }

int16_t InterfaceWindow::GetVisibility() {
    ev("GetVisibility", {lab(this)});
    return static_cast<int16_t>(ret("vis"));
}

int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), lab(rect), subtract});
    rect->mLeft = static_cast<int16_t>(ret("posL"));
    rect->mTop = static_cast<int16_t>(ret("posT"));
    rect->mRight = static_cast<int16_t>(ret("posR"));
    rect->mBottom = static_cast<int16_t>(ret("posB"));
    return 0;
}

// CoolRegion and CoolRect: the vtables need every virtual.
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }

CoolRect::CoolRect() { ev("CoolRect", {lab(this)}); }
CoolRect::CoolRect(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::~CoolRect() { ev("~CoolRect", {lab(this)}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

namespace SamsUtilities {
UKeyboard::UKeyboard() {
    if (!gQuiet) ev("UKeyboard", {});
}
uint8_t UKeyboard::IsOptionDown() { return 0; }
uint8_t UKeyboard::IsCommandDown() { return 0; }
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_uim_addr(const char* name) {
    if (gWinBuf == nullptr) {
        gWinBuf = static_cast<char*>(tpw::ArenaAllocZeroed(0x100));
        gPeepBuf = static_cast<char*>(tpw::ArenaAllocZeroed(0x100));
        gRideBuf = static_cast<char*>(tpw::ArenaAllocZeroed(0x100));
        gIdBuf = static_cast<char*>(tpw::ArenaAllocZeroed(0x10));
        gKbBuf = static_cast<char*>(tpw::ArenaAllocZeroed(0x40));
    }
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"peepg", &gSpritePreviewPeep}, {"handle", &gSpritePreviewHandle}, {"rideg", &gRidePreview},
        {"tick", &gTickNow},            {"last", &gLastTurnTime},          {"kbg", &SamsUtilities::gKeyboard},
        {"gamepause", &reinterpret_cast<CGame*>(gGameBytes)->mF1C}, {"win", gWinBuf},                {"peep", gPeepBuf},
        {"ride", gRideBuf},             {"id", gIdBuf},                    {"kbd", gKbBuf},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_uim_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uim_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CGame) <= sizeof(gGameBytes));
    return n;
}

// Puts a keyboard whose GetModifiers answers the "mods" queue into gKeyboard.
void tpw_uim_setkb() {
    gQuiet = true;
    SamsUtilities::gKeyboard = new (gKbBuf) MockKeyboard;
    gQuiet = false;
}

void tpw_uim_zoommap(uint64_t id) {
    UIMain_ZoomToMapId(*reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>*>(static_cast<uintptr_t>(id)));
}
void tpw_uim_zoompeep(uint64_t peep) { UIMain_ZoomToPeep(reinterpret_cast<CPeep*>(static_cast<uintptr_t>(peep))); }
uint32_t tpw_uim_shift() { return UIMain_GetShiftKeyState(); }
void tpw_uim_destroysprite() { UIMain_DestroySpritePreview(); }
void tpw_uim_updatesprite() { UIMain_UpdateSpritePreview(); }
void tpw_uim_mid(int32_t id, uint64_t window) {
    UIMain_MidWindowParticles(id, reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)));
}
void tpw_uim_destroyride() { UIMain_DestroyRidePreview(); }

}  // extern "C"
