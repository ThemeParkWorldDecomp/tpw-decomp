// Mouse.cpp (main app), unit w143: CSpritePlaceMode (the base of the two staff modes), CMMPlaceStaff and CMMPickUpStaff. The modes are made
// by the staff panel (not translated yet). Every member name and every global added here is invented; the layouts are the Mac's (Mouse.h).
//   CSpritePlaceMode: OnMouseMove 0x1007e528 (388 bytes), the destructor 0x1007ec14 (88), OnRelease 0x10081550 (68)
//   CMMPlaceStaff:    constructor 0x1007eb94 (60), destructor 0x10081320 (100), GetMode 0x100813b0 (8), OnAssign 0x1007ec9c (492),
//                     OnRelease 0x1007eeb8 (152), OnLeftClick 0x1007ef80 (1,208)
//   CMMPickUpStaff:   constructor 0x1007f46c (48), destructor 0x10081258 (140), GetMode 0x100812e8 (8), OnAssign 0x1007f4d8 (132),
//                     OnLeftClick 0x1007f58c (904), OnRightClick 0x1007fa74 (4), OnRelease 0x1007f948 (252)
// The destructors only rewrite the vtable words on the Mac (and free `this` for a positive flag, which is the delete expression's job here).
#include <cstdint>
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
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"

// The null cell id of CStaffHQ::mF60 (a const halfword of the image at 0x1029d434; its value there is 0). The name is invented.
extern const uint16_t gStaffHQNullId = 0;

namespace {
// The data block ESP_SpawnScript takes as its first argument (0x1029f768, Mouse.cpp's data; its first words are 0x102669d8, 0x10, 0x1200,
// 0x10266a50, 6, 0x10266a60, 0, 0x10266a60, 1 ...: pointers into the TOC area and small numbers, meaning unknown). The host gives it an opaque
// block of the same first words under an invented name; ESP_SpawnScript is untranslated.
uint32_t gStaffSpawnBlock[12] = {0x102669d8, 0x10, 0x1200, 0x10266a50, 6, 0x10266a60, 0, 0x10266a60, 1, 0x10266a60, 2, 0x10266a60};

constexpr const char* kUnknownStaffPlace = "CMMPlaceStaff: placing unknown staff type";                 // 0x1023df1b
constexpr const char* kUnknownStaffClick = "CMMPlaceStaff::OnLeftClick() - unknown staff type";       // 0x1023df45
constexpr const char* kCannotPlace = "Cannot place staff member here - the cell is of type %d\n";     // 0x1023df77
constexpr const char* kCannotPut = "Can't put staff here";                                             // 0x1023dfb0
constexpr const char* kDropping = "Dropping staff where he was before he was picked up";               // 0x1023dfc5
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023dff9

constexpr int32_t kCursorPlace = 9;   // SetGameWindowCursor's argument

// The map cell of the pointer (gIntrface.mF24, the 1 based id): plain arithmetic on the world, as the Mac does it.
CMapCell* PointerCell() {
    const uint16_t id = gIntrface.mF24;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<uint32_t>((id - 1) * 0x44u));
}

// The Mac makes the mouse and its default mode in this order inline: the mouse (an empty class on the Mac, operator new(1)), then the default
// mode (operator new(8)), which the mouse takes; then the two words of the mouse's data are set to -1. A mouse that could not be made leaves
// the words alone. The answer is the mouse, NULL when it could not be made.
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

// Puts the default mode back: it is made first, then the mouse if there is none, then the mouse takes the mode (and deletes the current one,
// which is `this` of the caller: nothing may touch a member afterwards). The Mac calls the assignment even for a missing mouse.
void AssignDefaultMode() {
    CMMDefault* mode = new (std::nothrow) CMMDefault;
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

// The script a mode spawns: the old handle is killed AFTER the new script exists.
void ReplaceScript(int32_t& handle, int32_t fresh) {
    if (handle != 0) SCR_KillScript(handle);
    handle = fresh;
}
}  // namespace

// ---- CSpritePlaceMode

CSpritePlaceMode::~CSpritePlaceMode() {}

// 0x1007e528. The script follows the pointer (the point lifted 5.0f); over a track, a flag, a hoarding, or a cell that is neither grass, a path
// nor a queue a square face is drawn under the point (5.0f to the left and to the front, one unit up).
void CSpritePlaceMode::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {
    if (mScript == 0) return;
    const s3DPoint* const picked = GetPickupWorldPosn();
    const float x = picked->x;
    float y = picked->y;
    const float z = picked->z;
    y = y + 5.0f;
    CMapCell* const cell = PointerCell();
    SCR_SetScriptPosition(mScript, x, y, z, 4);
    bool draw = false;
    if (cell->IsTrack() != 0) {
        draw = true;
    } else if (cell->IsFlag(0x40) != 0) {
        draw = true;
    } else if (cell->IsHoarding() != 0) {
        draw = true;
    } else {
        if (cell->IsGrass() != 0) return;
        if (cell->IsPath() != 0) return;
        if (cell->IsQueue() != 0) return;
        draw = true;
    }
    if (draw) {
        DC_AddSquareFaceToList(x - 5.0f, 1.0f, z - 5.0f, 0, 1, 0, 1.0f, 0, 0, 0, static_cast<DYN_Type>(0));
    }
}

// 0x10081550: kills the script, if there is one.
void CSpritePlaceMode::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
}

// ---- CMMPlaceStaff

CMMPlaceStaff::CMMPlaceStaff(int32_t model, uint8_t payGrade, uint8_t flag, int32_t value)
    : mModel(model), mPayGrade(payGrade), mF11(flag), mF14(value), mPlaced(0) {}

CMMPlaceStaff::~CMMPlaceStaff() {}

int32_t CMMPlaceStaff::GetMode() {
    return 5;
}

// 0x1007ec9c: the cursor, then the script of the staff kind's sprite (the sprite number is not the model: 4 -> 6, 5 -> 5, 6 -> 4, 7 -> 7,
// 8 -> 8). Any other model asserts and spawns nothing.
void CMMPlaceStaff::OnAssign() {
    gUIInterface.SetGameWindowCursor(kCursorPlace);
    int32_t sprite = 0;
    switch (mModel) {
        case 6: sprite = 4; break;
        case 5: sprite = 5; break;
        case 4: sprite = 6; break;
        case 7: sprite = 7; break;
        case 8: sprite = 8; break;
        default:
            GAME_ASSERTMSG(0, kUnknownStaffPlace);
            return;
    }
    const int32_t handle = ESP_SpawnScript(gStaffSpawnBlock, static_cast<ESP_Sprites>(sprite), mF11, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
    ReplaceScript(mScript, handle);
}

// 0x1007eeb8: kills the script; the staff database is told what became of the staff member (0 once placed, else 1); the advisor drops the
// message 0x135.
void CMMPlaceStaff::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
    CStaffDB* const database = reinterpret_cast<CStaffDB*>(gGameWorld);   // the database is the first member of the world
    if (mPlaced != 0) {
        database->FateOfPickedUpStaff(static_cast<CStaffDB::StaffFate>(0));
    } else {
        database->FateOfPickedUpStaff(static_cast<CStaffDB::StaffFate>(1));
    }
    gGameWorld->GetAdvisor()->CancelMessage(static_cast<AdvisorMessage>(0x135));
}

// 0x1007ef80: the cell under the pointer takes the staff member when it is not a track, a flag or a hoarding and is grass, a path or a queue;
// the staff object is made by its model, the mode goes back to the default (which deletes this mode).
void CMMPlaceStaff::OnLeftClick(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    CMapCell* const cell = PointerCell();
    bool refused = false;
    if (cell->IsTrack() != 0) {
        refused = true;
    } else if (cell->IsFlag(0x40) != 0) {
        refused = true;
    } else if (cell->IsHoarding() != 0) {
        refused = true;
    } else if (cell->IsGrass() == 0 && cell->IsPath() == 0 && cell->IsQueue() == 0) {
        refused = true;
    }
    if (refused) {
        CTRACE(2, kCannotPlace, static_cast<int32_t>(cell->mType));
        return;
    }
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellId{id};
    switch (mModel) {
        case 5: new (std::nothrow) CHandyman(cellId, mPayGrade, mF11, mF14); break;
        case 6: new (std::nothrow) CEntertainer(cellId, mPayGrade, mF11, mF14); break;
        case 7: new (std::nothrow) CGuard(cellId, mPayGrade, mF11, mF14); break;
        case 4: new (std::nothrow) CMechanic(cellId, mPayGrade, mF11, mF14); break;
        case 8: new (std::nothrow) CResearcher(cellId, mPayGrade, mF11, mF14); break;
        default: GAME_ASSERTMSG(0, kUnknownStaffClick); break;
    }
    mPlaced = 1;
    AssignDefaultMode();
}

// ---- CMMPickUpStaff

CMMPickUpStaff::CMMPickUpStaff(ESP_Sprites sprite, uint8_t flag) : mSprite(sprite), mF10(flag) {}

CMMPickUpStaff::~CMMPickUpStaff() {}

int32_t CMMPickUpStaff::GetMode() {
    return 6;
}

void CMMPickUpStaff::OnRightClick(int32_t, int32_t) {}

// 0x1007f4d8
void CMMPickUpStaff::OnAssign() {
    gUIInterface.SetGameWindowCursor(kCursorPlace);
    const int32_t handle = ESP_SpawnScript(gStaffSpawnBlock, mSprite, mF10, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
    ReplaceScript(mScript, handle);
}

// 0x1007f58c: the cell must not be a flag, a hoarding or a track (this order) and must be grass, a path or a queue: the staff headquarters
// drops the staff member there and the mode goes back to the default (which deletes this mode).
void CMMPickUpStaff::OnLeftClick(int32_t, int32_t) {
    const uint16_t id = gIntrface.mF24;
    CMapCell* const cell = PointerCell();
    bool refused = false;
    if (cell->IsFlag(0x40) != 0) {
        refused = true;
    } else if (cell->IsHoarding() != 0) {
        refused = true;
    } else if (cell->IsTrack() != 0) {
        refused = true;
    } else if (cell->IsGrass() == 0 && cell->IsPath() == 0 && cell->IsQueue() == 0) {
        refused = true;
    }
    if (refused) {
        CTRACE(2, kCannotPut);
        return;
    }
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellId{id};
    gGameWorld->GetStaffHQ()->DropStaff(cellId);
    AssignDefaultMode();
}

// 0x1007f948: kills the script; a staff member still held by the headquarters (mF60 is not the null id) is put back in the cell he was picked
// up from (the thing's cell x and y, 1 based).
void CMMPickUpStaff::OnRelease() {
    if (mScript != 0) SCR_KillScript(mScript);
    mScript = 0;
    const uint16_t held = gGameWorld->GetStaffHQ()->mF60;
    if (held != gStaffHQNullId) {
        CTRACE(2, kDropping);
        const uint16_t thingId = gGameWorld->GetStaffHQ()->mF60;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        const CThing* const thing = static_cast<const CThing*>(gThingArray[thingId].mThing);
        const uint16_t cell = static_cast<uint16_t>(thing->mCellX + (static_cast<uint32_t>(thing->mCellY) << 7) + 1);
        gGameWorld->GetStaffHQ()->DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});
    }
}
