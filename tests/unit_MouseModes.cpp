// Test library for the rest of Mouse.cpp (unit w144; libtpw_unit_MouseModes.dylib, tests/test_MouseModes.py), main app:
//   src/game/Mouse.cpp  CMMPathDraw, CMMOnline, CMMCoaster, CMMFirstPersonTarget, __sinit_Mouse_cpp (and the CMouseMode bodies of include/Mouse.h)
//   src/game/Mouse.cpp        (landed) AssignMouseMode, CMMDefault: they run for real, so the mode handover (old mode released and deleted) is tested
// Every other callee is a fake that records the event the PowerPC stub records. The globals of other files are defined here, zero at start.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CellBase.h"
#include "Debug.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "intrface.h"
#include "online.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes (gMouse, gMouseMode, the two mouse words, gMouseWord1F8/1FC are src/Mouse*.cpp's)
CThingArrayEntry gThingArray[kThingArraySize];
CIntrface gIntrface;
CWorld* gGameWorld;
UIInterface gUIInterface;
CRideDatabase gRideDatabase;
COnlineControl* gOnlineControl;

namespace {
CWorld* gWorldStore;               // the world block (gGameWorld points to it, or is NULL in a case)
RideInfo* gRideStore;              // the one ride record: GetRideInfo and GetDBInfo answer it
alignas(16) unsigned char gMouseStore[16];

// A mode the base class bodies are called on (GetMode is pure in the base class).
class TestMode : public CMouseMode {
public:
    int32_t GetMode() override { return 0; }
};

// A fake mode (label 9): the destructor, GetMode, OnAssign and OnRelease record.
class FakeMode : public CMouseMode {
public:
    ~FakeMode() override { ev("Mode.dtor", {lab(this)}); }
    int32_t GetMode() override {
        ev("Mode.GetMode", {lab(this)});
        return static_cast<int32_t>(ret("ModeGetMode", 0));
    }
    void OnAssign() override { ev("Mode.OnAssign", {lab(this), lab(gMouseMode)}); }
    void OnRelease() override { ev("Mode.OnRelease", {lab(this), lab(gMouseMode)}); }
};

CTrackCell* TrackCell(int32_t k) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + (static_cast<int64_t>(k) - 1) * 0x28);
}
}  // namespace

CGameType::CGameType() { mType = kGameTypeNormal; }

// ---- the nothrow new: the answer is the queue "New" (0 a NULL). The size is recorded as the Mac's.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault) || size == sizeof(CMMOnline)) mac = 8;
    else if (size == sizeof(COnlineControl)) mac = 0xe8c;
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the cells
int32_t CMapCell::IsFlag(uint16_t flag) const { ev("IsFlag", {lab(this), flag}); return static_cast<int32_t>(ret("IsFlag", 0)); }
int32_t CMapCell::IsWalkable() const { ev("IsWalkable", {lab(this)}); return static_cast<int32_t>(ret("IsWalkable", 0)); }
CTrackCell* CCellBase::GetTrackCellEquiv() {
    ev("GetTrackCellEquiv", {lab(this)});
    return TrackCell(static_cast<int32_t>(ret("Equiv", 1)));
}
int32_t CTrackCell::IsTrackX() const { ev("IsTrackX", {lab(this)}); return static_cast<int32_t>(ret("IsTrackX", 0)); }
int32_t CTrackCell::IsHoarding() const { ev("IsHoarding", {lab(this)}); return static_cast<int32_t>(ret("IsHoarding", 0)); }

// ---- the cell system, the cursor, the shift key, the rides, the objects, the coaster, the online control
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag) { ev("LLCM_MouseMoved", {x, y, flag}); }
void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LLCM_LeftMouseButtonDown", {x, y, shift, flag}); }
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag}); }
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag}); }
void LLCC_SystemSetRide(const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id, int32_t track) { ev("LLCC_SystemSetRide", {id.mIndex, track}); }
void LLCC_SystemSetRideQueue(int32_t queue) { ev("LLCC_SystemSetRideQueue", {queue}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }
void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("LLCR_OverRide", {x, y});
    *out = static_cast<uint16_t>(ret("Override", 0));
}
void UIInterface::SetGameWindowCursor(int16_t cursor) { ev("SetGameWindowCursor", {cursor}); }
void UIInterface::CheckMouseContext(int context) { ev("CheckMouseContext", {context}); }
int32_t UIInterface::GetShiftKeyState() { ev("GetShiftKeyState"); return static_cast<int32_t>(ret("Shift", 0)); }
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return gRideStore;
}
RideInfo* CObject::GetDBInfo() const { ev("GetDBInfo", {lab(this)}); return gRideStore; }
void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {lab(this), on}); }
void ToggleFirstPersonWalkAround() { ev("ToggleFirstPersonWalkAround"); }
void CoasterAcceptLeftMouseClick(int32_t down) { ev("CoasterAcceptLeftMouseClick", {down}); }
COnlineControl::COnlineControl() { ev("Online.ctor", {lab(this)}); }
void COnlineControl::SetAvatarDestination(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("SetAvatarDestination", {lab(this), cell.mIndex}); }
void COnlineControl::TeleportAvatar(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("TeleportAvatar", {lab(this), cell.mIndex}); }

// ---- trace and assertion
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        const int value = va_arg(args, int);
        va_end(args);
        ev("CTRACE", {level, value}, {format});
    } else {
        ev("CTRACE", {level}, {format});
    }
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    char shown[160];
    std::snprintf(shown, sizeof shown, "%.159s", format);
    ev("GAME_ASSERTMSG", {condition}, {shown});
}

// Object kinds of the wrappers: 0 CMMPathDraw, 1 CMMOnline, 2 CMMCoaster, 3 CMMFirstPersonTarget, 4 a plain CMouseMode (TestMode).
// Operations: 0 OnLeftClick, 1 OnLeftRelease, 2 OnRightClick, 3 OnRightRelease, 4 OnDoubleClick, 5 OnMouseMove, 6 WhileLeftDown,
// 7 WhileRightDown, 8 GetMode, 9 OnAssign, 10 OnRelease.
namespace {
#define MM_CALL(Type, p, op, a, b, c, d)                                                \
    do {                                                                                \
        auto* m = reinterpret_cast<Type*>(p);                                           \
        switch (op) {                                                                   \
            case 0: m->Type::OnLeftClick(a, b); break;                                  \
            case 1: m->Type::OnLeftRelease(a, b); break;                                \
            case 2: m->Type::OnRightClick(a, b); break;                                 \
            case 3: m->Type::OnRightRelease(a, b); break;                               \
            case 4: m->Type::OnDoubleClick(a, b); break;                                \
            case 5: m->Type::OnMouseMove(a, b, c, d); break;                            \
            case 6: m->Type::WhileLeftDown(a, b); break;                                \
            case 7: m->Type::WhileRightDown(a, b); break;                               \
            case 8: return m->Type::GetMode();                                          \
            case 9: m->Type::OnAssign(); break;                                         \
            case 10: m->Type::OnRelease(); break;                                       \
            default: break;                                                             \
        }                                                                               \
    } while (0)
}  // namespace

extern "C" {

extern int32_t tpw_cl_on;

uint64_t tpw_mm_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return reinterpret_cast<uintptr_t>(gWorldStore);
    if (std::strcmp(name, "map") == 0) return gWorldStore == nullptr ? 0 : reinterpret_cast<uintptr_t>(gWorldStore->GetMap());
    return 0;
}

// Once per process: the world (zero, never constructed) and the ride record.
void tpw_mm_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<CWorld*>(std::calloc(1, sizeof(CWorld)));
    gGameWorld = gWorldStore;
    if (gRideStore == nullptr) gRideStore = static_cast<RideInfo*>(std::calloc(1, sizeof(RideInfo)));
}

// A block for a test object (the pool is reused: every call zeroes the block again). Label 8 is the mouse; label 60 the ride record (the four
// numbers are set by tpw_mm_ride); others plain zeroed blocks (the online control is label 70).
uint64_t tpw_mm_obj(int32_t label, uint32_t size) {
    static void* pool[128];
    if (label == 8) { std::memset(gMouseStore, 0xa5, sizeof gMouseStore); return reinterpret_cast<uintptr_t>(gMouseStore); }
    if (label == 60) { std::memset(gRideStore, 0, sizeof(RideInfo)); return reinterpret_cast<uintptr_t>(gRideStore); }
    const uint32_t want = size < 0x2000 ? 0x2000 : size;
    if (pool[label] == nullptr) pool[label] = std::calloc(1, want);
    std::memset(pool[label], 0, want);
    return reinterpret_cast<uintptr_t>(pool[label]);
}
void tpw_mm_ride(int32_t upgrade, int32_t track, int32_t queue, int32_t uitype) {
    gRideStore->Value(kRideSlot_UpgradeType).mInteger = upgrade;
    gRideStore->Value(kRideSlot_WhichTrackType).mInteger = track;
    gRideStore->Value(kRideSlot_HasQueue).mInteger = queue;
    gRideStore->mUIType = uitype;
}

uint64_t tpw_mm_fake_mode() { return reinterpret_cast<uintptr_t>(new FakeMode); }

// An object of the kind made with `new` (the pointer is deletable), the constructor's fields set by tpw_mm_set_field afterwards.
uint64_t tpw_mm_new(int32_t kind) {
    // The constructor's own events (CMMOnline's cursor call) are not part of the case: the PowerPC side builds the object from a block.
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    CMouseMode* made;
    switch (kind) {
        case 0: made = new CMMPathDraw(static_cast<CELL_MODES>(0)); break;
        case 1: made = new CMMOnline; break;
        case 2: made = new CMMCoaster; break;
        case 3: made = new CMMFirstPersonTarget; break;
        default: made = new TestMode; break;
    }
    tpw_cl_on = was;
    return reinterpret_cast<uintptr_t>(made);
}

// A constructor on a block filled with 0xa5. Kind 0 variant 0/1 is the PathDraw's first and second constructor.
uint64_t tpw_mm_ctor(int32_t kind, int32_t variant, int32_t mode, int32_t id) {
    void* block = std::malloc(256);
    std::memset(block, 0xa5, 256);
    switch (kind) {
        case 0:
            if (variant == 0) new (block) CMMPathDraw(static_cast<CELL_MODES>(mode));
            else new (block) CMMPathDraw(static_cast<CELL_MODES>(mode), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)});
            break;
        case 1: new (block) CMMOnline; break;
        case 2: new (block) CMMCoaster; break;
        default: new (block) CMMFirstPersonTarget; break;
    }
    return reinterpret_cast<uintptr_t>(block);
}

// Fields by number: 0 word at +4, 1 CMMPathDraw's cell mode, 2 its flag word, 3 its object id.
void tpw_mm_set_field(uint64_t object, int32_t kind, int32_t field, int64_t value) {
    auto* mode = reinterpret_cast<CMouseMode*>(object);
    auto* draw = reinterpret_cast<CMMPathDraw*>(object);
    switch (field) {
        case 0: mode->mWord4 = static_cast<int32_t>(value); break;
        case 1: if (kind == 0) draw->mCellMode = static_cast<CELL_MODES>(value); break;
        case 2: if (kind == 0) draw->mF0C = static_cast<int32_t>(value); break;
        case 3: if (kind == 0) draw->mObjectId = static_cast<uint16_t>(value); break;
        default: break;
    }
}
int64_t tpw_mm_get_field(uint64_t object, int32_t kind, int32_t field) {
    auto* mode = reinterpret_cast<CMouseMode*>(object);
    auto* draw = reinterpret_cast<CMMPathDraw*>(object);
    switch (field) {
        case 0: return mode->mWord4;
        case 1: return kind == 0 ? static_cast<int32_t>(draw->mCellMode) : 0;
        case 2: return kind == 0 ? draw->mF0C : 0;
        case 3: return kind == 0 ? draw->mObjectId : 0;
        default: return 0;
    }
}

// The fixed state of a case: key numbers are the Python module's.
void tpw_mm_set(int32_t key, int64_t value, int64_t other) {
    switch (key) {
        case 1: gMouse = reinterpret_cast<CMouse*>(value); break;
        case 2: gMouseMode = reinterpret_cast<CMouseMode*>(value); break;
        case 3: gMouseWord1E0 = static_cast<int32_t>(value); break;
        case 4: gMouseWord1E4 = static_cast<int32_t>(value); break;
        case 6: gIntrface.mF24 = static_cast<uint16_t>(value); break;
        case 7: gThingArray[value].mThing = reinterpret_cast<void*>(other); break;
        case 8: std::memset(gThingArray, 0, sizeof(CThingArrayEntry) * 16); break;
        case 11: TheGameType().mType = static_cast<GameType>(value); break;
        case 16: gGameWorld = value != 0 ? gWorldStore : nullptr; break;
        case 17: gOnlineControl = reinterpret_cast<COnlineControl*>(value); break;
        default: break;
    }
}

// The state read back (the order the Python module reads them).
void tpw_mm_dump(int64_t* out) {
    int n = 0;
    out[n++] = lab(gMouse);
    out[n++] = lab(gMouseMode);
    out[n++] = gMouseWord1E0;
    out[n++] = gMouseWord1E4;
    out[n++] = lab(gOnlineControl);
}

// The two words the Mac's __sinit_Mouse_cpp stores: here the load-time values.
void tpw_mm_sinit(int64_t* out) {
    out[0] = gMouseWord1F8;
    out[1] = gMouseWord1FC;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_mm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    return n;
}

// ---- the functions, one wrapper each with C types only (objects as uint64_t)
int32_t tpw_mm_call(uint64_t object, int32_t kind, int32_t op, int32_t a, int32_t b, int32_t c, int32_t d) {
    switch (kind) {
        case 0: MM_CALL(CMMPathDraw, object, op, a, b, c, d); break;
        case 1: MM_CALL(CMMOnline, object, op, a, b, c, d); break;
        case 2: MM_CALL(CMMCoaster, object, op, a, b, c, d); break;
        case 3: MM_CALL(CMMFirstPersonTarget, object, op, a, b, c, d); break;
        default: {
            auto* m = reinterpret_cast<TestMode*>(object);
            switch (op) {
                case 0: m->CMouseMode::OnLeftClick(a, b); break;
                case 1: m->CMouseMode::OnLeftRelease(a, b); break;
                case 2: m->CMouseMode::OnRightClick(a, b); break;
                case 3: m->CMouseMode::OnRightRelease(a, b); break;
                case 4: m->CMouseMode::OnDoubleClick(a, b); break;
                case 5: m->CMouseMode::OnMouseMove(a, b, c, d); break;
                case 6: m->CMouseMode::WhileLeftDown(a, b); break;
                case 7: m->CMouseMode::WhileRightDown(a, b); break;
                case 9: m->CMouseMode::OnAssign(); break;
                case 10: m->CMouseMode::OnRelease(); break;
                default: break;
            }
        }
    }
    return 0;
}

// The destructors: the flag is tested as a positive halfword (extsh) on the Mac; the caller passes the halfword's sign.
void tpw_mm_dtor(uint64_t object, int32_t kind, int32_t flag) {
    switch (kind) {
        case 0: { auto* p = reinterpret_cast<CMMPathDraw*>(object); if (flag > 0) delete p; else p->~CMMPathDraw(); break; }
        case 1: { auto* p = reinterpret_cast<CMMOnline*>(object); if (flag > 0) delete p; else p->~CMMOnline(); break; }
        case 2: { auto* p = reinterpret_cast<CMMCoaster*>(object); if (flag > 0) delete p; else p->~CMMCoaster(); break; }
        case 3: { auto* p = reinterpret_cast<CMMFirstPersonTarget*>(object); if (flag > 0) delete p; else p->~CMMFirstPersonTarget(); break; }
        default: {
            auto* p = reinterpret_cast<TestMode*>(object);
            if (flag > 0) delete p; else p->~TestMode();   // the base destructor is inline in include/Mouse.h: the derived one runs it
        }
    }
}

}  // extern "C"
