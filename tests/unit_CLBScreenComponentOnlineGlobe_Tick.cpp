// Test library for the online globe's Tick and WindowMessagesFn (unit v5571; libtpw_unit_CLBScreenComponentOnlineGlobe_Tick.dylib,
// tests/test_CLBScreenComponentOnlineGlobe_Tick.py), main app:
//   src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Tick.cpp   Tick, WindowMessagesFn, gGlobeCentre, gGlobeViewTarget
//
// The callees outside the unit (TransformVertex, IsKeyDown, UpdateOnlineCityInfoOverlay, the Globe moves, WorldLobbySetZoomButton) are
// fakes that record what the real ones read and answer from the test's queues. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "FS_Online.h"
#include "TPSettings.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby;
CLBLobbyView* gLobbyView;
CMatrix* gei_ViewMatrix;
CDisplay gei_Display;
CPlayerSettings gPlayerSettings;

// The constructors and destructors of the two objects live in files this library does not compile.
CDisplay::CDisplay() {}
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FromBits(int64_t bits) {
    uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    const float* i = static_cast<const float*>(in);
    ev("TransformVertex", {CanonBits(i[0]), CanonBits(i[1]), CanonBits(i[2]), type});
    float* o = static_cast<float*>(out);
    o[0] = FromBits(ret("tvx"));
    o[1] = FromBits(ret("tvy"));
    o[2] = FromBits(ret("tvz"));
}

int32_t CLBLobby::IsKeyDown(int32_t flag) {
    ev("IsKeyDown", {flag});
    return static_cast<int32_t>(ret("IsKeyDown"));
}

void UpdateOnlineCityInfoOverlay(const CFSOnlineCity* city) {
    ev("Update", {city == nullptr ? -1 : city->mValue});
}

void WorldLobbySetZoomButton(int32_t zoomedIn) { ev("ZoomButton", {zoomedIn}); }

void CLBScreenComponentOnlineGlobe::GlobeLeft() { ev("GlobeLeft", {}); }
void CLBScreenComponentOnlineGlobe::GlobeRight() { ev("GlobeRight", {}); }
void CLBScreenComponentOnlineGlobe::GlobeUp() { ev("GlobeUp", {}); }
void CLBScreenComponentOnlineGlobe::GlobeDown() { ev("GlobeDown", {}); }
void CLBScreenComponentOnlineGlobe::GlobeZoomIn() { ev("GlobeZoomIn", {}); }
void CLBScreenComponentOnlineGlobe::GlobeZoomOut() { ev("GlobeZoomOut", {}); }

namespace {

alignas(16) char gProbe[0x2000];

template <class T, class M>
int64_t Off(T* base, M* member) {
    return reinterpret_cast<char*>(member) - reinterpret_cast<char*>(base);
}
#define OFFS(T, m) Off(reinterpret_cast<T*>(gProbe), &reinterpret_cast<T*>(gProbe)->m)

using FlagList = TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>;

}  // namespace

extern "C" {

uint64_t tpw_og_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"lobby", &gLobby},
        {"view", &gLobbyView},
        {"matrix", &gei_ViewMatrix},
        {"display", &gei_Display},
        {"settings", &gPlayerSettings},
        {"centre", &gGlobeCentre},
        {"target", &gGlobeViewTarget},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_og_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// mF70 holds an arena offset (lobby.h): the address of an offset and the offset of an address.
uint64_t tpw_og_arena_ptr(uint32_t offset) { return offset != 0 ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + offset)) : 0; }
uint32_t tpw_og_arena_offset(uint64_t address) { return address != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))) : 0; }

// The host offsets, in the order of the module's NAMES.
int32_t tpw_og_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFFS(CLBLobby, mTickUnits);
    out[n++] = OFFS(CLBLobby, mGlobals);
    out[n++] = OFFS(CLBLobby, mWidth);
    out[n++] = OFFS(CLBLobby, mHeight);
    out[n++] = sizeof(CLBLobby);
    out[n++] = OFFS(CLBLobbyGlobals, mGlobeRadiusIn);
    out[n++] = OFFS(CLBLobbyGlobals, mGlobeRadiusOut);
    out[n++] = sizeof(CLBLobbyGlobals);
    out[n++] = OFFS(CLBLobbyView, mPosition);
    out[n++] = OFFS(CLBLobbyView, mTarget);
    out[n++] = OFFS(CLBLobbyView, mLightUpOffset);
    out[n++] = OFFS(CLBLobbyView, mLightRightOffset);
    out[n++] = OFFS(CLBLobbyView, mLightDownward);
    out[n++] = sizeof(CLBLobbyView);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF0C) + 4;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF0C) + 12;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF34);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x00;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x04;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x08;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x0c;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x10;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x14;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x18;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x1c;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x20;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF44) + 0x24;
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF70);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mCityFlags);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF38);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF3C);
    out[n++] = OFFS(CLBScreenComponentOnlineGlobe, mF40);
    out[n++] = sizeof(CLBScreenComponentOnlineGlobe);
    out[n++] = OFFS(FlagList, mHead);
    out[n++] = sizeof(FlagList);
    out[n++] = OFFS(FlagList::CNode, mNext);
    out[n++] = sizeof(FlagList::CNode);
    out[n++] = OFFS(CLBGlobeCityFlag, mCity);
    out[n++] = sizeof(CLBGlobeCityFlag);
    out[n++] = OFFS(CFSOnlineCity, mValue);
    out[n++] = OFFS(CFSOnlineCity, mPosX);
    out[n++] = OFFS(CFSOnlineCity, mPosY);
    out[n++] = OFFS(CFSOnlineCity, mPosZ);
    out[n++] = sizeof(CFSOnlineCity);
    out[n++] = OFFS(CDisplay, mF04);
    out[n++] = OFFS(CPlayerSettings, mCurrent.mRMBScrollOn);
    return n;
}

// ---- the functions (the base-class call is direct: the test never builds the object, so there is no vtable)
void tpw_og_tick(uint64_t self) {
    reinterpret_cast<CLBScreenComponentOnlineGlobe*>(static_cast<uintptr_t>(self))->CLBScreenComponentOnlineGlobe::Tick();
}

void tpw_og_wmf(uint64_t self, int32_t message, int32_t a, int32_t b) {
    reinterpret_cast<CLBScreenComponentOnlineGlobe*>(static_cast<uintptr_t>(self))
        ->CLBScreenComponentOnlineGlobe::WindowMessagesFn(message, a, b);
}

}  // extern "C"
