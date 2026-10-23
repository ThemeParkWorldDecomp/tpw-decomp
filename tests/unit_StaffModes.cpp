// Test library for the park click and the staff mouse modes (unit w143; libtpw_unit_StaffModes.dylib, tests/test_StaffModes.py), main app:
//   src/game/Mouse.cpp     CSpritePlaceMode (OnMouseMove 0x1007e528, the destructor 0x1007ec14, OnRelease 0x10081550), CMMPlaceStaff, CMMPickUpStaff
//   src/game/ui/ui_main.cpp   ContextMouseClick 0x10183320
//   src/game/TPPointer.cpp   GetPickupWorldPosn 0x10067220
//   src/game/Mouse.cpp           (landed) AssignMouseMode, CMMDefault: they run for real, so the mode handover (the old mode released and deleted) is tested
// Every other callee is a fake that records the event the PowerPC stub records: the cell predicates, the five staff constructors, DropStaff,
// the staff HQ, the advisor, the script and drawing calls, the cursor, the trace, the assertion, the panels, the keyboard, the nothrow new,
// the delete (calllog.cpp), CMMOnline and the CMMPathDraw (constructors and virtuals; unit w144 translates it). The globals of other files are
// defined here, zero at start like the Mac's bss. Answers come from the case's queues.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "StaffDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "TbAudioSystem.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "intrface.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (gMouse, gMouseMode and the two words are src/game/Mouse.cpp's)
int32_t gGameWinWordC38;
CThingArrayEntry gThingArray[kThingArraySize];
CIntrface gIntrface;
CWorld* gGameWorld;
UIInterface gUIInterface;
s3DPoint gPickupWorldPosn;
int32_t gSoundBanks[8];
StaffStatusPanel gStaffStatusPanel;
VisitorStatusPanel gVisitorStatusPanel;
namespace SamsUtilities {
UKeyboard* gKeyboard;
}

namespace {
void* gSelected;      // what UI_GetSelectedObject answers
void* gPeep;          // what UI_GetSelectedPeep answers
void* gHQ;            // the staff HQ, the advisor, the bank account and the object control the world's getters answer
void* gAdvisor;
void* gBank;
ObjectControl* gControl;
CWorld* gWorldStore;   // the world block (gGameWorld points to it, or is NULL in a case)

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

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

// The keyboard: the preset one and those the nothrow new makes answer IsCommandDown from the queue "Command".
class FakeKeyboard : public SamsUtilities::UKeyboard {
public:
    uint8_t IsCommandDown() override {
        ev("Keyboard.IsCommandDown", {lab(this)});
        return static_cast<uint8_t>(ret("Command", 0));
    }
};
alignas(16) unsigned char gProtoStore[sizeof(FakeKeyboard)];
FakeKeyboard* gProto;
alignas(16) unsigned char gKeyboardStore[sizeof(FakeKeyboard)];
alignas(16) unsigned char gMouseStore[16];
}  // namespace

// ---- the CMMPathDraw (unit w144 translates it): the constructors record, the virtuals record like the Mac's fake vtable
CMMPathDraw::CMMPathDraw(CELL_MODES mode) {
    ev("PathDraw.ctor", {lab(this), static_cast<int32_t>(mode)});
}
CMMPathDraw::CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("PathDraw.ctor2", {lab(this), static_cast<int32_t>(mode), id.mIndex});
}
CMMPathDraw::~CMMPathDraw() { ev("PathDraw.dtor", {lab(this)}); }
void CMMPathDraw::OnLeftClick(int32_t, int32_t) {}
void CMMPathDraw::OnLeftRelease(int32_t, int32_t) {}
void CMMPathDraw::OnRightClick(int32_t, int32_t) {}
void CMMPathDraw::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMPathDraw::WhileLeftDown(int32_t, int32_t) {}
void CMMPathDraw::WhileRightDown(int32_t, int32_t) {}
int32_t CMMPathDraw::GetMode() { return 3; }
void CMMPathDraw::OnAssign() { ev("PathDraw.OnAssign", {lab(this), lab(gMouseMode)}); }

// ---- CMMOnline (a fake: the online replacement of the default mode)
CMMOnline::CMMOnline() { ev("Online.ctor", {lab(this)}); }
CMMOnline::~CMMOnline() { ev("Online.dtor", {lab(this)}); }
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { ev("Online.GetMode", {lab(this)}); return 7; }
void CMMOnline::OnAssign() { ev("Online.OnAssign", {lab(this), lab(gMouseMode)}); }
CGameType::CGameType() { mType = kGameTypeNormal; }

// ---- the nothrow new: the answer is the queue "New" (0 a NULL). The size is recorded as the Mac's; the five staff classes (0x214 or 0x218 on
// the Mac, the Python module checks which) are one token.
void* RecordedNew(std::size_t size) {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault) || size == sizeof(CMMOnline)) mac = 8;
    else if (size == sizeof(CMMPathDraw)) mac = 0x14;
    else if (size == sizeof(SamsUtilities::UKeyboard)) mac = 0xc;
    else if (size == sizeof(CMechanic) || size == sizeof(CHandyman) || size == sizeof(CEntertainer) || size == sizeof(CGuard) ||
             size == sizeof(CResearcher)) mac = 0x500;
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = std::malloc(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return RecordedNew(size); }

// The staff are made on the game heap (the Mac's operator new), which is the arena on the host since unit v5L31 (src/game/Mouse.cpp
// NewStaff); the unit links no arena, and the arena's allocation is the same recording new.
namespace tpw {
void* ArenaAlloc(std::size_t size) { return RecordedNew(size); }
}  // namespace tpw

// ---- the keyboard
namespace SamsUtilities {
UKeyboard::UKeyboard() {
    // The object made by the unit's new is given the fake's vtable, so that IsCommandDown records and answers like the Mac's fake.
    if (gProto != nullptr) std::memcpy(static_cast<void*>(this), static_cast<void*>(gProto), sizeof(void*));
    ev("UKeyboard.ctor", {lab(this)});
}
uint8_t UKeyboard::IsOptionDown() { return 0; }
uint8_t UKeyboard::IsCommandDown() { return 0; }
uint32_t UKeyboard::GetModifiers() { return 0; }   // the import 0x10001b00, which the tests never reach on this class
}  // namespace SamsUtilities

// ---- the cells
int32_t CMapCell::IsFlag(uint16_t flag) const { ev("IsFlag", {lab(this), flag}); return static_cast<int32_t>(ret("IsFlag", 0)); }
int32_t CMapCell::IsPath() const { ev("IsPath", {lab(this)}); return static_cast<int32_t>(ret("IsPath", 0)); }
int32_t CMapCell::IsQueue() const { ev("IsQueue", {lab(this)}); return static_cast<int32_t>(ret("IsQueue", 0)); }
int32_t CMapCell::IsTrack() const { ev("IsTrack", {lab(this)}); return static_cast<int32_t>(ret("IsTrack", 0)); }
int32_t CMapCell::IsHoarding() const { ev("IsHoarding", {lab(this)}); return static_cast<int32_t>(ret("IsHoarding", 0)); }
int32_t CMapCell::IsGrass() const { ev("IsGrass", {lab(this)}); return static_cast<int32_t>(ret("IsGrass", 0)); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("ThingId", 0))};
}
void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("LLCR_OverRide", {x, y});
    *out = static_cast<uint16_t>(ret("Override", 0));
}
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag}); }
void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("LLCC_SetRideRotation", {rotation, flag}); }

// ---- the world's getters and the objects they answer
CStaffHQ* CWorld::GetStaffHQ() { ev("GetStaffHQ", {lab(this)}); return static_cast<CStaffHQ*>(gHQ); }
CAdvisor* CWorld::GetAdvisor() { ev("GetAdvisor", {lab(this)}); return static_cast<CAdvisor*>(gAdvisor); }
CBankAccount* CWorld::GetBankAccount() { ev("GetBankAccount", {lab(this)}); return static_cast<CBankAccount*>(gBank); }
ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {type.mIndex, flag});
    return gControl;
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("Balance", 0));
}
void CStaffHQ::DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("DropStaff", {lab(this), cell.mIndex}); }
void CStaffDB::FateOfPickedUpStaff(StaffFate fate) { ev("FateOfPickedUpStaff", {lab(this), static_cast<int32_t>(fate)}); }
int32_t CAdvisor::CancelMessage(AdvisorMessage message) { ev("CancelMessage", {lab(this), static_cast<int32_t>(message)}); return 0; }

// ---- the five staff constructors (the object is the nothrow new's, recorded under one token)
CMechanic::CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t pay, uint8_t flag, int32_t value) {
    ev("CMechanic.ctor", {lab(this), cell.mIndex, pay, flag, value});
}
CHandyman::CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t pay, uint8_t flag, int32_t value) {
    ev("CHandyman.ctor", {lab(this), cell.mIndex, pay, flag, value});
}
CEntertainer::CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t pay, uint8_t flag, int32_t value) {
    ev("CEntertainer.ctor", {lab(this), cell.mIndex, pay, flag, value});
}
CGuard::CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t pay, uint8_t flag, int32_t value) {
    ev("CGuard.ctor", {lab(this), cell.mIndex, pay, flag, value});
}
CResearcher::CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, int32_t pay, uint8_t flag, int32_t value) {
    ev("CResearcher.ctor", {lab(this), cell.mIndex, pay, flag, value});
}

// ---- scripts, drawing, sound, trace, assertion
int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    const uint32_t* words = static_cast<const uint32_t*>(block);
    ev("ESP_SpawnScript", {words[0], words[1], words[2], words[3], words[4], words[5], static_cast<int32_t>(sprite), a,
                           static_cast<int32_t>(acts), CanonBits(x), CanonBits(y), CanonBits(z)});
    return static_cast<int32_t>(ret("Spawn", 0));
}
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    ev("SCR_SetScriptPosition", {handle, CanonBits(x), CanonBits(y), CanonBits(z), mode});
}
void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t unused2, int32_t keepPosition, DYN_Type type) {
    ev("DC_AddSquareFaceToList", {CanonBits(x), CanonBits(y), CanonBits(z), side, texture, rotation, CanonBits(unused1), large, unused2,
                                  keepPosition, static_cast<int32_t>(type)});
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}
void UIInterface::SetGameWindowCursor(int16_t cursor) { ev("SetGameWindowCursor", {cursor}); }
void UIInterface::CheckMouseContext(int context) { ev("CheckMouseContext", {context}); }
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

// ---- the user interface
int32_t MainMenuUp() { ev("MainMenuUp"); return static_cast<int32_t>(ret("MainMenuUp", 0)); }
void UI_CheckSelected() { ev("UI_CheckSelected"); }
CThing* UI_GetSelectedPeep() { ev("UI_GetSelectedPeep"); return static_cast<CThing*>(gPeep); }
void* UI_GetSelectedObject() { ev("UI_GetSelectedObject"); return gSelected; }
void UI_SetSelectedObject(void* object) { ev("UI_SetSelectedObject", {lab(object)}); }
void UIMain_GotoStatusPanelForThing(void* thing) { ev("UIMain_GotoStatusPanelForThing", {lab(thing)}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel"); }
void CoasterEditMode(void* object) { ev("CoasterEditMode", {lab(object)}); }
void StaffStatusPanel::Show(CThing* thing) { ev("StaffStatusPanel.Show", {lab(thing)}); }
void VisitorStatusPanel::Show(CThing* thing) { ev("VisitorStatusPanel.Show", {lab(thing)}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_sm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldStore},
        {"map", gWorldStore == nullptr ? nullptr : static_cast<const void*>(gWorldStore->GetMap())},
        {"pickup", &gPickupWorldPosn},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// Once per process: the world (zero, never constructed) and the fakes' prototype keyboard.
void tpw_sm_init() {
    if (gWorldStore == nullptr) gWorldStore = static_cast<CWorld*>(std::calloc(1, sizeof(CWorld)));
    gGameWorld = gWorldStore;
    if (gProto == nullptr) gProto = new (gProtoStore) FakeKeyboard;
    if (gControl == nullptr) gControl = static_cast<ObjectControl*>(std::calloc(1, sizeof(ObjectControl) + 64));
}

// A block for a test object: the labelled ones that are not modes (label 8 the mouse, 12 the preset keyboard, 50 the staff HQ, 51 the advisor,
// 52 the bank account, 53 the object control, others plain zeroed blocks of the size). The pool is reused: every call zeroes the block again.
uint64_t tpw_sm_obj(int32_t label, uint32_t size) {
    static void* pool[96];
    if (label == 8) { std::memset(gMouseStore, 0xa5, sizeof gMouseStore); return reinterpret_cast<uintptr_t>(gMouseStore); }
    if (label == 12) { std::memcpy(gKeyboardStore, gProtoStore, sizeof gKeyboardStore); return reinterpret_cast<uintptr_t>(gKeyboardStore); }
    if (label == 53) { std::memset(gControl, 0, sizeof(ObjectControl) + 64); return reinterpret_cast<uintptr_t>(gControl); }
    if (pool[label] == nullptr) pool[label] = std::calloc(1, size < 0x100 ? 0x100 : size);
    std::memset(pool[label], 0, size);
    if (label == 50) gHQ = pool[label];
    if (label == 51) gAdvisor = pool[label];
    if (label == 52) gBank = pool[label];
    return reinterpret_cast<uintptr_t>(pool[label]);
}

// A fake mode (label 9) made on the heap, so that the mouse can delete it.
uint64_t tpw_sm_fake_mode() { return reinterpret_cast<uintptr_t>(new FakeMode); }

// A staff mode on the heap (kind 0 CMMPlaceStaff, 1 CMMPickUpStaff), the way the staff panel makes it: `new` (the pointer is deletable).
uint64_t tpw_sm_new(int32_t kind) {
    if (kind == 0) return reinterpret_cast<uintptr_t>(new CMMPlaceStaff(0, 0, 0, 0));
    return reinterpret_cast<uintptr_t>(new CMMPickUpStaff(static_cast<ESP_Sprites>(0), 0));
}
// The constructors on a block (so that the Python module reads the fields the constructor stored).
uint64_t tpw_sm_ctor_place(int32_t model, int32_t pay, int32_t flag, int32_t value) {
    void* block = std::malloc(sizeof(CMMPlaceStaff) + 64);
    std::memset(block, 0xa5, sizeof(CMMPlaceStaff) + 64);
    new (block) CMMPlaceStaff(model, static_cast<uint8_t>(pay), static_cast<uint8_t>(flag), value);
    return reinterpret_cast<uintptr_t>(block);
}
uint64_t tpw_sm_ctor_pick(int32_t sprite, int32_t flag) {
    void* block = std::malloc(sizeof(CMMPickUpStaff) + 64);
    std::memset(block, 0xa5, sizeof(CMMPickUpStaff) + 64);
    new (block) CMMPickUpStaff(static_cast<ESP_Sprites>(sprite), static_cast<uint8_t>(flag));
    return reinterpret_cast<uintptr_t>(block);
}

// The fields of a staff mode by number: 0 script handle, 1 model, 2 pay grade, 3 flag byte +0x11, 4 value +0x14, 5 placed, 6 sprite, 7 byte +0x10 of the pick up.
void tpw_sm_set_field(uint64_t object, int32_t kind, int32_t field, int64_t value) {
    auto* place = reinterpret_cast<CMMPlaceStaff*>(object);
    auto* pick = reinterpret_cast<CMMPickUpStaff*>(object);
    switch (field) {
        case 0: static_cast<CSpritePlaceMode*>(kind == 0 ? static_cast<CSpritePlaceMode*>(place) : pick)->mScript = static_cast<int32_t>(value); break;
        case 1: place->mModel = static_cast<int32_t>(value); break;
        case 2: place->mPayGrade = static_cast<uint8_t>(value); break;
        case 3: place->mF11 = static_cast<uint8_t>(value); break;
        case 4: place->mF14 = static_cast<int32_t>(value); break;
        case 5: place->mPlaced = static_cast<uint8_t>(value); break;
        case 6: pick->mSprite = static_cast<ESP_Sprites>(value); break;
        case 7: pick->mF10 = static_cast<uint8_t>(value); break;
        default: break;
    }
}
int64_t tpw_sm_get_field(uint64_t object, int32_t kind, int32_t field) {
    auto* place = reinterpret_cast<CMMPlaceStaff*>(object);
    auto* pick = reinterpret_cast<CMMPickUpStaff*>(object);
    switch (field) {
        case 0: return kind == 0 ? static_cast<CSpritePlaceMode*>(place)->mScript : static_cast<CSpritePlaceMode*>(pick)->mScript;
        case 1: return place->mModel;
        case 2: return place->mPayGrade;
        case 3: return place->mF11;
        case 4: return place->mF14;
        case 5: return place->mPlaced;
        case 6: return static_cast<int32_t>(pick->mSprite);
        case 7: return pick->mF10;
        default: return 0;
    }
}

// The fixed state of a case: key numbers are the Python module's.
void tpw_sm_set(int32_t key, int64_t value, int64_t other) {
    switch (key) {
        case 0: SamsUtilities::gKeyboard = reinterpret_cast<SamsUtilities::UKeyboard*>(value); break;
        case 1: gMouse = reinterpret_cast<CMouse*>(value); break;
        case 2: gMouseMode = reinterpret_cast<CMouseMode*>(value); break;
        case 3: gMouseWord1E0 = static_cast<int32_t>(value); break;
        case 4: gMouseWord1E4 = static_cast<int32_t>(value); break;
        case 5: gGameWinWordC38 = static_cast<int32_t>(value); break;
        case 6: gIntrface.mF24 = static_cast<uint16_t>(value); break;
        case 7: gThingArray[value].mThing = reinterpret_cast<void*>(other); break;
        case 8: std::memset(gThingArray, 0, sizeof(CThingArrayEntry) * 16); break;
        case 9: gSelected = reinterpret_cast<void*>(value); break;
        case 10: gPeep = reinterpret_cast<void*>(value); break;
        case 11: TheGameType().mType = static_cast<GameType>(value); break;
        case 12: gSoundBanks[3] = static_cast<int32_t>(value); break;
        case 13: {   // the pickup point's three words
            const uint32_t bits[3] = {static_cast<uint32_t>(value), static_cast<uint32_t>(other), 0};
            std::memcpy(&gPickupWorldPosn.x, &bits[0], 4);
            std::memcpy(&gPickupWorldPosn.y, &bits[1], 4);
            break;
        }
        case 14: {
            const uint32_t bits = static_cast<uint32_t>(value);
            std::memcpy(&gPickupWorldPosn.z, &bits, 4);
            break;
        }
        case 15: gSoundBanks[3] = 0; break;
        case 16: gGameWorld = value != 0 ? gWorldStore : nullptr; break;
        default: break;
    }
}

// The state read back: the words as labels or numbers (the order the Python module reads them).
void tpw_sm_dump(int64_t* out) {
    int n = 0;
    out[n++] = lab(SamsUtilities::gKeyboard);
    out[n++] = lab(gMouse);
    out[n++] = lab(gMouseMode);
    out[n++] = gMouseWord1E0;
    out[n++] = gMouseWord1E4;
    out[n++] = lab(gSelected);
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF60));
    out[n++] = static_cast<int64_t>(offsetof(s3DPoint, z));
    return n;
}

// ---- the functions, one wrapper each with C types only (objects as uint64_t)
int32_t tpw_sm_context() { return ContextMouseClick(); }
uint64_t tpw_sm_pickup() { return reinterpret_cast<uintptr_t>(GetPickupWorldPosn()); }

void tpw_sm_place_leftclick(uint64_t o, int32_t x, int32_t y) { reinterpret_cast<CMMPlaceStaff*>(o)->CMMPlaceStaff::OnLeftClick(x, y); }
void tpw_sm_place_assign(uint64_t o) { reinterpret_cast<CMMPlaceStaff*>(o)->CMMPlaceStaff::OnAssign(); }
void tpw_sm_place_release(uint64_t o) { reinterpret_cast<CMMPlaceStaff*>(o)->CMMPlaceStaff::OnRelease(); }
int32_t tpw_sm_place_getmode(uint64_t o) { return reinterpret_cast<CMMPlaceStaff*>(o)->CMMPlaceStaff::GetMode(); }
void tpw_sm_place_dtor(uint64_t o, int32_t flag) {
    auto* p = reinterpret_cast<CMMPlaceStaff*>(o);
    if (flag > 0) delete p;
    else p->~CMMPlaceStaff();
}

void tpw_sm_pick_leftclick(uint64_t o, int32_t x, int32_t y) { reinterpret_cast<CMMPickUpStaff*>(o)->CMMPickUpStaff::OnLeftClick(x, y); }
void tpw_sm_pick_rightclick(uint64_t o, int32_t x, int32_t y) { reinterpret_cast<CMMPickUpStaff*>(o)->CMMPickUpStaff::OnRightClick(x, y); }
void tpw_sm_pick_assign(uint64_t o) { reinterpret_cast<CMMPickUpStaff*>(o)->CMMPickUpStaff::OnAssign(); }
void tpw_sm_pick_release(uint64_t o) { reinterpret_cast<CMMPickUpStaff*>(o)->CMMPickUpStaff::OnRelease(); }
int32_t tpw_sm_pick_getmode(uint64_t o) { return reinterpret_cast<CMMPickUpStaff*>(o)->CMMPickUpStaff::GetMode(); }
void tpw_sm_pick_dtor(uint64_t o, int32_t flag) {
    auto* p = reinterpret_cast<CMMPickUpStaff*>(o);
    if (flag > 0) delete p;
    else p->~CMMPickUpStaff();
}

// The base class's members, called on a CMMPlaceStaff (kind 0) or a CMMPickUpStaff (kind 1) with the qualified name.
void tpw_sm_base_move(uint64_t o, int32_t kind, int32_t a, int32_t b, int32_t c, int32_t d) {
    CSpritePlaceMode* p = kind == 0 ? static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPlaceStaff*>(o))
                                    : static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPickUpStaff*>(o));
    p->CSpritePlaceMode::OnMouseMove(a, b, c, d);
}
void tpw_sm_base_release(uint64_t o, int32_t kind) {
    CSpritePlaceMode* p = kind == 0 ? static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPlaceStaff*>(o))
                                    : static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPickUpStaff*>(o));
    p->CSpritePlaceMode::OnRelease();
}
// CSpritePlaceMode is abstract here, so clang makes its complete-object destructor (D1) a trap; the base-object one (D2) is the body.
extern "C++" void SmBaseDestructor(CSpritePlaceMode*) asm("__ZN16CSpritePlaceModeD2Ev");
void tpw_sm_base_dtor(uint64_t o, int32_t kind, int32_t flag) {
    CSpritePlaceMode* p = kind == 0 ? static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPlaceStaff*>(o))
                                    : static_cast<CSpritePlaceMode*>(reinterpret_cast<CMMPickUpStaff*>(o));
    SmBaseDestructor(p);
    if (flag > 0) ::operator delete(p);
}

}  // extern "C"
