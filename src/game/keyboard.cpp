// keyboard.cpp: name proven by __sinit_keyboard_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "BMP_InsideArea.h"
#include "Cheat.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "CVariableTime.h"
#include "DateMessages.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "InGame.h"
#include "InterfaceFontWrapper.h"
#include "intrface.h"
#include "keyboard.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LLCellSystem.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "m3d2.h"
#include "Main.h"
#include "MainBalance.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "Mouse.h"
#include "MslSubscriptionTree.h"
#include "online.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "Postcard.h"
#include "Ptr32.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SkyObjects.h"
#include "tb_ddraw.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbTList.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "v51029_decls.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ios>
#include <new>
#include <string>

// TestAdvisor2/3/4 (step the advisor index and call the advisor) and delete_ride (keyboard actions).

// 0x1014bf74
int32_t delete_ride() {
    LLCC_SystemSetMode(static_cast<CELL_MODES>(0x33), true);
    int32_t v = gIntrface.mF24;
    LLCM_LeftMouseButtonUp((v - 1) & 0x7f, static_cast<int32_t>(static_cast<uint32_t>(v - 1) >> 7), 0, 0);
    return 1;
}

// 0x1014c010: the "backtrack" key. Makes the mouse if there is none; when the mouse mode answers 0 or 1 and the
// cell system is in mode 0, plays a sound at the selected cell, clicks it in mode 0x32, otherwise (mode 3) backtracks.
int32_t backtrack() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;   // 1 byte on the Mac; the original tests the result
        if (mouse != nullptr) {
            CMMDefault* mode = new CMMDefault;
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    uint32_t mouseMode = (gMouseMode == nullptr) ? 0 : static_cast<uint32_t>(gMouseMode->GetMode());
    if (LLCC_SystemIsMode(0) != 0 && mouseMode <= 1) {
        const uint16_t id = gIntrface.mF24;
        const uint32_t x = static_cast<uint32_t>(id - 1) & 0x7f;
        const uint32_t y = static_cast<uint32_t>(id - 1) >> 7;
        LLCC_SystemGetMode();
        const CMapCell* cell = reinterpret_cast<const CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 +
                                                                  static_cast<int32_t>(id - 1) * 0x44);
        if (cell->IsPath()) {
            const float fx = static_cast<float>(static_cast<int32_t>(x));
            const float fy = static_cast<float>(static_cast<int32_t>(y));
            SOUND_SendEvent(0, gSoundBanks[3], 0x5f, static_cast<int32_t>(10.0f * fx), 0, static_cast<int32_t>(10.0f * fy));
            LLCC_SystemSetMode(static_cast<CELL_MODES>(0x32), true);
            LLCM_LeftMouseButtonUp(static_cast<int32_t>(x), static_cast<int32_t>(y), 0, 0);
            LLCC_SystemSetMode(static_cast<CELL_MODES>(0), true);
        }
    } else if (mouseMode == 3) {
        LLCC_SystemBacktrack(nullptr, true);
    }
    return 1;
}

// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.

// 0x1014c250: the map is embedded in the world at +0x2d8.
int32_t cheat_cleanup_park() {
    reinterpret_cast<CMap*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8)->CleanUpPark();
    return 1;
}

// 0x1014c2b0
int32_t TestAdvisor2() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex += 10;
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// 0x1014c33c
int32_t TestAdvisor3() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex -= 10;
        if (gTestAdvisorIndex < 1) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// 0x1014c3c8
int32_t TestAdvisor4() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex += 0x32;
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// keyboard.cpp (main app): eight key actions (unit v5034): the ride turns, the quick saves and loads, the vomit and the advisor test.
//   turn_object_counterclockwise 0x1014cafc, turn_object_clockwise 0x1014ca74, quickload_layout 0x1014c97c, quicksave_layout 0x1014c89c,
//   place_vomit 0x1014ce2c, quickload 0x1014c7bc, quicksave 0x1014c6dc, TestAdvisor 0x1014c454. Each answers 1.

// The header declares CFSLoadSave::Save as void (another unit defines it so), but the original's caller tests r3; call the same symbol
// through a member pointer retyped to return int32_t (a member's mangled name has no return type).
static int32_t CFSLoadSave_SaveResult(CFSLoadSave* self, const TbDynamicStringTemplate<char16_t>& name) {
    void (CFSLoadSave::*declared)(const TbDynamicStringTemplate<char16_t>&) = &CFSLoadSave::Save;
    int32_t (CFSLoadSave::*asInt)(const TbDynamicStringTemplate<char16_t>&);
    static_assert(sizeof(declared) == sizeof(asInt), "member pointer size");
    std::memcpy(&asInt, &declared, sizeof(asInt));
    return (self->*asInt)(name);
}

int32_t gTestAdvisorIndex = 0x103;      // 0x102a4e54, original name unknown: the next advisor response TestAdvisor calls
TbStringBase<char> gQuickSaveSuffix;     // 0x1037f248, original name unknown: appended to the theme name for the layout quick save

int32_t TestAdvisor() {
    if (ADV_Stat() == 0) {
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    } else {
        ADV_Slap();
    }
    return 1;
}

int32_t show_script() { return 1; }     // 0x1014c4e0
int32_t LoadPtclLib() { return 1; }     // 0x1014c50c
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

int32_t save_recording() { return 1; }  // 0x1014c660

// 0x1014c690
int32_t pause() {
    gGame.TogglePause(true);
    return 1;
}

int32_t quicksave() {
    TbDynamicStringTemplate<char> name(gGame.GetCurrentThemeName());
    TbDynamicStringTemplate<char16_t> wide;
    StringToUniString(wide, name);
    if (CFSLoadSave_SaveResult(&gLoadSave, wide) != 0)
        DEBUG_MESSAGE("QuickSave OK", 0);
    else
        DEBUG_MESSAGE("Quicksave failed", 0);
    return 1;
}

int32_t quickload() {
    TbDynamicStringTemplate<char> name(gGame.GetCurrentThemeName());
    TbDynamicStringTemplate<char16_t> wide;
    StringToUniString(wide, name);
    if (gLoadSave.Load(wide) != 0)
        DEBUG_MESSAGE("QuickSave Loaded", 0);
    else
        DEBUG_MESSAGE("QuickSave Load Failed", 0);
    return 1;
}

int32_t quicksave_layout() {
    TbDynamicStringTemplate<char> name;
    name = gGame.GetCurrentThemeName();
    name += gQuickSaveSuffix;
    CSaveLoadHeader header;
    if (gSaveLoad.Save(name.mData, &header, static_cast<CSaveLoad::LoadType>(1)) != 0)
        DEBUG_MESSAGE("QuickSave of Action Recording succeeded", 0);
    else
        DEBUG_MESSAGE("Quicksave of Action Recording failed", 0);
    return 1;
}

int32_t quickload_layout() {
    TbDynamicStringTemplate<char> name;
    name = gGame.GetCurrentThemeName();
    name += gQuickSaveSuffix;
    CSaveLoadHeader header;
    if (gSaveLoad.Load(name.mData, &header, static_cast<CSaveLoad::LoadType>(1)) != 0) {
        CTRACE(0x499602d2, "QuickLoad of Action Recording succeeded");
        gActionRecorder.ReplayRecording(static_cast<ReplayType>(0));
    } else {
        CTRACE(0x499602d2, "QuickLoad of Action Recording Failed !!!");
    }
    return 1;
}

int32_t turn_object_clockwise() {
    int32_t rotation = LLCC_GetRideRotation() + 0x5a;
    LLCC_SetRideRotation(rotation % 0x168, true);
    return 1;
}

// The ride's rotation in degrees, turned by 90 (clockwise) or 270 (counterclockwise), the remainder of 360 signed.
int32_t turn_object_counterclockwise() {
    int32_t rotation = LLCC_GetRideRotation() + 0x10e;
    LLCC_SetRideRotation(rotation % 0x168, true);
    return 1;
}

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

int32_t abort_coaster_edit() {
    gUIInterface.EndCoasterEdit();
    return 1;
}

// The litter goes on the cell under the pointer (the id is the 16-bit mF24; id 0 reaches the cell before the first, as the original does).
int32_t place_vomit() {
    uint16_t id = gIntrface.mF24;
    CMapCell* cell = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 +
                                                 static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
    cell->PlaceLitter(static_cast<CMapCell::LitterType>(7));
    return 1;
}

// keyboard.cpp key actions (unit v5341): start_strike, clear_patrol_area, wheres_my_script, abort_all_requesters.

// All five staff types strike, then the end of month message goes out (the message centre is made on first use: operator new(0x1d0)
// and __construct_array of 0x1d headers on the Mac).
int32_t start_strike() {
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(0));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(1));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(2));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(3));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(4));
    CMsgEndOfMonth message;
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre();
    gMessageCentre->PostMessage(message);
    return 1;
}

// The person the UI has selected is a staff member of kind 4 to 8 (mechanic, handyman, entertainer, guard, researcher).
int32_t clear_patrol_area() {
    if (gUIInterface.GetSelectedPeep() != nullptr) {
        uint8_t kind = reinterpret_cast<uint8_t*>(gUIInterface.GetSelectedPeep())[2];
        if (kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7) {
            reinterpret_cast<CStaff*>(gUIInterface.GetSelectedPeep())->CancelPatrolArea();
        }
    }
    return 1;
}

// Prints where the pointed thing's script is (kinds 1, 4 to 8 and 0x12).
int32_t wheres_my_script() {
    uint8_t* thing = reinterpret_cast<uint8_t*>(gIntrface.mPointedThing);
    if (thing != nullptr) {
        uint8_t kind = thing[2];
        if (kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12) {
            float x, y, z;
            int32_t mode;
            SCR_GetScriptPosition(*reinterpret_cast<int32_t*>(thing + 0xc), &x, &y, &z, &mode);
            CTRACE(0x1c, "SP: (%g, %g, %g, %d)\n", static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), mode);
        }
    }
    return 1;
}

// The mouse is made on first use (the Mac inlines this at three places; the second and third are dead after the first).
static void MakeMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

// 1 when locators were removed; 0 when the mode is 0 or 1 already; otherwise the mouse goes back to the default mode and the cells too.
int32_t abort_all_requesters() {
    if (gUIInterface.RemoveLocators() != 0) return 1;
    MakeMouse();
    int32_t mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (mode == 0) return 0;
    MakeMouse();
    mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (mode == 1) return 0;
    CMMDefault* fresh = new CMMDefault;
    MakeMouse();
    gMouse->AssignMouseMode(fresh);
    LLCC_SystemSetMode(0, true);
    return 1;
}

int32_t slow_down_time() {
    gClock.mTime.mPausableTime.mVariableTime.DecreaseTimeRate();
    return 1;
}

int32_t speed_up_time() {
    gClock.mTime.mPausableTime.mVariableTime.IncreaseTimeRate();
    return 1;
}

// keyboard.cpp key actions, main app 0x1014d364 .. 0x1014e73c (unit v51108): each answers 1.

// The hud and movie toggles (names invented).
int32_t gKeyHudToggle = 0;      // 0x1037f238
int32_t gKeyMovieToggle = 0;    // 0x1037f23c

namespace keyaction_callee {
// The runtime's operator new (0x102384b8), the game heap: the arena on the host (the staff it makes are things, whose addresses the
// lists keep in 32 bits); weak so the test library can fake it.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return tpw::ArenaAlloc(size); }
}  // namespace keyaction_callee

namespace {
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> StaffCell;

template <class T>
void MakeKeyStaff(uint32_t size) {
    StaffCell cell;
    cell.mIndex = gIntrface.mF24;
    void* memory = keyaction_callee::OperatorNew(size);
    if (memory) new (memory) T(cell);
}
}  // namespace

// 0x1014d364 (48 bytes)
int32_t normal_speed_time() {
    reinterpret_cast<CVariableTime*>(&gClock)->SetTimeRate(1.0);
    return 1;
}

// 0x1014d3bc (56 bytes)
int32_t open_park() {
    gGameWorld->SetParkOpenOrClosed(1, 0);
    return 1;
}

// 0x1014d414 (56 bytes)
int32_t close_park() {
    gGameWorld->SetParkOpenOrClosed(0, 0);
    return 1;
}

// Keyboard cheat actions: the weather keys, chatter, real time lighting and start_cheating.

// 0x1037f240 / 0x1037f244 (BSS): the guard byte and the value of ChatterToggle's function-static
int8_t gChatterToggleGuard;
int32_t gChatterToggleOn;

// 0x1014d470 (132 bytes)
int32_t start_cheating() {
    CCheat* cheat = reinterpret_cast<CCheat*>(gGameWinCheat);
    if (gGameWinCheatGuard == 0) {
        new (cheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    cheat->SetCheatStatus(true);
    gGameWorld->GetBankAccount()->MakeDeposit(1);
    return 1;
}
// 0x1014d51c (96 bytes)
int32_t ChatterToggle() {
    if (gChatterToggleGuard == 0) {
        gChatterToggleOn = 1;
        gChatterToggleGuard = 1;
    }
    SND_SetChatterOnOff(gChatterToggleOn);
    gChatterToggleOn = 1 - gChatterToggleOn;
    return 1;
}

// 0x1014d5a0 (108 bytes)
int32_t WeatherUp() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality + 1);
    return 1;
}

// 0x1014d62c (108 bytes)
int32_t WeatherDown() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality - 1);
    return 1;
}

// 0x1014d6bc (108 bytes)
int32_t WeatherUpLots() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality + 10);
    return 1;
}

// 0x1014d74c (108 bytes)
int32_t WeatherDownLots() {
    gGameWorld->GetWeather()->mF64 = 1;
    int32_t quality = static_cast<int32_t>(gGameWorld->GetWeather()->mF0C);
    gGameWorld->GetWeather()->SetQuality(quality - 10);
    return 1;
}

// 0x1014d7e0 (56 bytes)
int32_t WeatherReset() {
    gGameWorld->GetWeather()->mF64 = 0;
    return 1;
}

// 0x1014d83c (88 bytes)
int32_t DeactivateWeather() {
    gGameWorld->GetWeather()->mF64 = 1;
    gGameWorld->GetWeather()->SetQuality(100);
    return 1;
}

// 0x1014d8bc (96 bytes)
int32_t RealTimeLightingMasterToggle() {
    gei_Display.mFlags ^= 0x800;
    const char* state = (gei_Display.mFlags & 0x800) != 0 ? "dis" : "en";
    CTRACE(0x499602d2, "Real time lighting %sabled", state);
    return 1;
}

// 0x1014d950 (28 bytes)
int32_t FPSDisplayToggle() {
    gIntrface.mShowFPS = (gIntrface.mShowFPS == 0);
    return 1;
}

// 0x1014d994 (56 bytes)
int32_t complete_all_research() {
    gGameWorld->GetResearchLab()->CheatCompleteAll();
    return 1;
}

// 0x1014d9f8 (56 bytes)
int32_t uncomplete_all_research() {
    gGameWorld->GetResearchLab()->CheatCompleteOnlyPaidFor();
    return 1;
}

// 0x1014da60 (56 bytes)
int32_t create_arrival_vehicle() {
    gGameWorld->ArrivalVehicle_DoNextLeg(1);
    return 1;
}

// keyboard.cpp cheats: place_litter 0x1014dac8 (136 bytes) and place_bomb 0x1014db74 (92 bytes).
// Both put litter on the cell whose number is the interface's mF24 (the cells start at +0x2d8 of the world, 0x44 bytes each).

static CMapCell* CellOf(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

// 0x1014dac8 (136 bytes): a random kind 1 to count - 1 (an unsigned modulo; the count is never 1)
int32_t place_litter() {
    uint16_t id = gIntrface.mF24;   // read before GetRand
    uint32_t rand = gGameWorld->GetRand();
    uint32_t divisor = gLitterKindCount - 1;
    uint32_t kind = rand - (rand / divisor) * divisor + 1;
    CellOf(id)->PlaceLitter(static_cast<CMapCell::LitterType>(kind));
    return 1;
}

// 0x1014db74 (92 bytes): litter kind 8 (the bomb)
int32_t place_bomb() {
    CellOf(gIntrface.mF24)->PlaceLitter(static_cast<CMapCell::LitterType>(8));
    return 1;
}

// keyboard.cpp: the spawn and loan actions of the key tables (unit v5028).

namespace {
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellTag;

CellTag CurrentCell() {
    CellTag cell;
    cell.mIndex = gIntrface.mF24;
    return cell;
}

template <class T>
void MakeStaff(uint32_t size) {
    CellTag cell = CurrentCell();
    void* memory = online_callee::OperatorNew(size);
    if (memory) new (memory) T(cell);
}
}  // namespace

// 0x1014dbf4 (136 bytes)
int32_t spawn_person() {
    if (gGameWorld->ParkGates_GetStatus() == 1) {
        void* memory = online_callee::OperatorNew(0x22c);
        if (memory) {
            CellTag cell = CurrentCell();
            new (memory) CPerson(cell);
        }
    } else {
        gGameWorld->SetParkOpenOrClosed(1, 0);
    }
    return 1;
}

// 0x1014dca0 (176 bytes)
int32_t spawn_five_persons() {
    if (gGameWorld->ParkGates_GetStatus() == 1) {
        for (int32_t i = 0; i < 5; i++) {
            void* memory = online_callee::OperatorNew(0x22c);
            if (memory) {
                CellTag cell = CurrentCell();
                new (memory) CPerson(cell);
            }
        }
    } else {
        gGameWorld->SetParkOpenOrClosed(1, 0);
    }
    return 1;
}

// 0x1014dd7c (48 bytes)
int32_t populate_park() {
    gGameWorld->PopulatePark();
    return 1;
}

// 0x1014ddd0 (92 bytes)
int32_t start_winning_sequence() {
    if (gGameWorld->IsPlayingWinningAnim())
        gGameWorld->EndWinningSequence();
    else
        gGameWorld->StartWinningSequence();
    return 1;
}

// 0x1014de5c (48 bytes)
int32_t start_bankruptcy_sequence() {
    gGameWorld->StartBankruptcySequence();
    return 1;
}

// 0x1014debc (88 bytes)
int32_t spawn_entertainer() {
    MakeStaff<CEntertainer>(0x214);
    return 1;
}

// 0x1014df3c (88 bytes)
int32_t spawn_handyman() {
    MakeStaff<CHandyman>(0x218);
    return 1;
}

// 0x1014dfbc (88 bytes)
int32_t spawn_mechanic() {
    MakeKeyStaff<CMechanic>(0x218);
    return 1;
}

// 0x1014e03c (88 bytes)
int32_t spawn_researcher() {
    MakeKeyStaff<CResearcher>(0x214);
    return 1;
}

// 0x1014e0bc (88 bytes)
int32_t spawn_guard() {
    MakeKeyStaff<CGuard>(0x214);
    return 1;
}

// 0x1014e138 (232 bytes): the cell is read once, before the first allocation
int32_t spawn_bag_o_staff() {
    CellTag cell = CurrentCell();
    void* memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CGuard(cell);
    memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CResearcher(cell);
    memory = online_callee::OperatorNew(0x218);
    if (memory) new (memory) CMechanic(cell);
    memory = online_callee::OperatorNew(0x218);
    if (memory) new (memory) CHandyman(cell);
    memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CEntertainer(cell);
    return 1;
}

// 0x1014e248 (116 bytes)
int32_t apply_for_loan_1() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(0))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e2e4 (116 bytes)
int32_t apply_for_loan_2() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(1))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e380 (116 bytes)
int32_t apply_for_loan_3() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(2))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e41c (116 bytes)
int32_t apply_for_loan_4() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(3))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e4b8 (60 bytes)
int32_t get_doshed_up() {
    gGameWorld->GetBankAccount()->MakeDeposit(10000);
    return 1;
}

// 0x1014e518 (40 bytes)
int32_t snapshot() {
    GAMESYSTEM_Screenshot();
    return 1;
}

// 0x1014e560 (60 bytes)
int32_t continuous_snapshot() {
    gKeyMovieToggle = 1 - gKeyMovieToggle;
    GAMESYSTEM_ScreenshotMovie(gKeyMovieToggle);
    return 1;
}

// 0x1014e5c8 (44 bytes)
int32_t togglegamewindow() {
    gUIInterface.SwitchGameWindow();
    return 1;
}

// 0x1014e61c (60 bytes)
int32_t togglehud() {
    gKeyHudToggle = 1 - gKeyHudToggle;
    GAMESYSTEM_SetHudState(gKeyHudToggle);
    return 1;
}

// The postcard panel (main app, ui_Postcard.cpp, 0x101b0ae4..0x101b2418), the keyboard key and UIInterface forwarding call.

int32_t TakePostCard() {
    gUIInterface.DoPostcard();
    return 1;
}

// 0x1014e6c8 (44 bytes)
int32_t mainmenu() {
    gUIInterface.ShowGameMainMenu();
    return 1;
}

// 0x1014e714 (44 bytes)
int32_t Screen_BuyObject() {
    gUIInterface.Screen_BuyObject();
    return 1;
}

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x1014e768
int32_t Screen_BuyStaff() { gUIInterface.Screen_BuyStaff(); return 1; }
// 0x1014e7bc
int32_t Screen_ParkStatus() { gUIInterface.Screen_ParkStatus(); return 1; }
// 0x1014e810
int32_t Screen_AllItems() { gUIInterface.Screen_AllItems(); return 1; }
// 0x1014e864
int32_t Screen_AllStaff() { gUIInterface.Screen_AllStaff(); return 1; }
// 0x1014e8b8
int32_t Screen_AllPeeps() { gUIInterface.Screen_AllPeeps(); return 1; }
// 0x1014e90c
int32_t Screen_Finance() { gUIInterface.Screen_Finance(); return 1; }
// 0x1014e960
int32_t Screen_Loans() { gUIInterface.Screen_Loans(); return 1; }
// 0x1014e9b0
int32_t Screen_StaffCosts() { gUIInterface.Screen_StaffCosts(); return 1; }
// 0x1014ea04
int32_t Screen_EntryPrice() { gUIInterface.Screen_EntryPrice(); return 1; }
// 0x1014ea58
int32_t Screen_Map() { gUIInterface.ShowMap(); return 1; }
// 0x1014eaa8
int32_t Screen_StaffLoc() { gUIInterface.Screen_StaffLoc(); return 1; }
// 0x1014eafc
int32_t Screen_PeepLoc() { gUIInterface.Screen_PeepLoc(); return 1; }
// 0x1014eb50
int32_t Screen_Research() { gUIInterface.Screen_Research(); return 1; }
// 0x1014eba4
int32_t CamcorderMode() { gUIInterface.CamcorderMode(); return 1; }
// 0x1014ebf4
int32_t ToggleHelpText() { gUIInterface.ToggleHelpText(); return 1; }

// Keyboard actions: golden ticket cheats and area delete mode (main 0x1014ec48 .. 0x1014f148).

namespace {

CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    return gMasterSave;
}

int32_t WinLocal(int32_t type) {
    MasterSave()->EarnedTicket(static_cast<GoldenTicketTypeLocal>(type));
    return 1;
}

}  // namespace

int32_t enter_area_delete_mode() {   // 0x1014ec48
    CMMPathDraw* mode = new CMMPathDraw(static_cast<CELL_MODES>(0x3a));
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse();
        CMMDefault* def = new CMMDefault();
        mouse->AssignMouseMode(def);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(mode);
    return 1;
}
int32_t win_ticket_0() { return WinLocal(0); }   // 0x1014ed40
int32_t win_ticket_1() { return WinLocal(1); }   // 0x1014edd8
int32_t win_ticket_2() { return WinLocal(2); }   // 0x1014ee70
int32_t win_ticket_3() { return WinLocal(3); }   // 0x1014ef08
int32_t win_ticket_4() { return WinLocal(4); }   // 0x1014efa0
int32_t win_ticket_5() { return WinLocal(5); }   // 0x1014f038

int32_t win_ticket_6() {   // 0x1014f0d0
    MasterSave()->EarnedTicket(static_cast<GoldenTicketTypeGlobal>(0), 100);
    return 1;
}

// keytable actions at 0x1014f16c to 0x1014fb2c: win_ticket_7/8/9, enter_hoarding_mode, force_ride_breakdown, park_attraction_value.

namespace {

// The shared body of win_ticket_7 to 9: create the master save on first use, then earn the ticket.
int32_t WinTicket(int32_t type) {
    if (gMasterSave == nullptr) {
        void* mem = ::operator new(0x4c);
        CFSGlobalMasterSave* save = nullptr;
        if (mem != nullptr) save = new (mem) CFSGlobalMasterSave();
        gMasterSave = save;
    }
    gMasterSave->EarnedTicket(static_cast<GoldenTicketTypeGlobal>(type), 100);
    return 1;
}

}  // namespace

int32_t win_ticket_7() { return WinTicket(1); }   // 0x1014f16c
int32_t win_ticket_8() { return WinTicket(2); }   // 0x1014f208
int32_t win_ticket_9() { return WinTicket(3); }   // 0x1014f2a4

// 0x1014f340
int32_t context_help() {
    gGameWorld->GetAdvisor()->PlayLastTutorialMessage();
    return 1;
}

// 0x1014f39c
int32_t show_version() { gUIInterface.ShowVersionTag(); return 1; }

// Unit v51117: FS_MasterSave.cpp's static initialiser, the online callbacks that make the online system on first use, and keyboard actions.

// 0x1014f3ec
int32_t save_hoardings() {
    LLCC_WriteHoardingsToBalanceFile();
    return 1;
}
// 0x1014f43c
int32_t save_complete_hoardings() {
    LLCC_WriteCompleteHoardingsToBalanceFile();
    return 1;
}

int32_t enter_hoarding_mode() {   // 0x1014f494
    CMMPathDraw* path = nullptr;
    void* pathMem = ::operator new(0x14);
    if (pathMem != nullptr) path = new (pathMem) CMMPathDraw(static_cast<CELL_MODES>(0x39));
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(::operator new(1));
        if (mouse != nullptr) {
            CMMDefault* def = nullptr;
            void* defMem = ::operator new(8);
            if (defMem != nullptr) def = new (defMem) CMMDefault();
            mouse->AssignMouseMode(def);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(path);
    return 1;
}

int32_t select_debug_person() { return 1; }

int32_t set_allow_loops_no_research() { return 1; }
int32_t next_debug_text_mode() { return 1; }
int32_t toggle_cell_flash() { return 1; }

// show_person_history_info 0x1014f65c: when the pointed thing is a staff member, guest or such (kind byte at +2 is
// 1, 4 to 8 or 0x12), report all its events. Always answers 1.

int32_t show_person_history_info() {   // 0x1014f65c
    CThing* thing = gIntrface.mPointedThing;
    if (thing != nullptr) {
        uint8_t kind = reinterpret_cast<uint8_t*>(thing)[2];
        bool show = kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12;
        if (show) reinterpret_cast<CPeep*>(thing)->ReportAllEvents();
    }
    return 1;
}

int32_t force_ride_worn() { return 1; }

int32_t force_ride_breakdown() {   // 0x1014f7e0
    CThing* under = gIntrface.mThingUnderPointer;
    if (under == nullptr) return 0;
    // The listing asks GetThingId of the pointed thing's slot (r4 is the thing, r3 the result slot).
    auto id = under->GetThingId();
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    CObject* object = static_cast<CObject*>(gThingArray[id.mIndex].mThing);
    if (object != nullptr) {
        if (!object->IsBrokenDown()) object->RequestBreakdown();
    }
    return 1;
}

int32_t force_ride_condemned() { return 1; }
int32_t force_ride_fix() { return 1; }
int32_t debug_cell_neighbs() { return 1; }
int32_t debug_cell_dirs() { return 1; }
int32_t debug_track_cell_neighbs() { return 1; }
int32_t debug_hoarding_cell_neighbs() { return 1; }
int32_t debug_track_cell_dirs() { return 1; }

int32_t park_attraction_value() {   // 0x1014fa2c
    gGameWorldRef->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    gGameWorldRef->mParkTime.GetFunnyTimeSinceGameTurn(0);
    int32_t points = gGameWorld->GetParkAnalyser()->GetTotalParkAttractionPoints();
    CTRACE(3, "Park attraction points: %d\n", points);
    uint8_t first, second, third;
    gGameWorldRef->GetParkAnalyser()->GetThreeMostDominantThoughts(first, second, third);
    CTRACE(3, "Most dominant thoughts: %2d, %2d, %2d\n", first, second, third);
    return 1;
}

int32_t dbgchan0() { return 1; }
int32_t dbgchan1() { return 1; }
int32_t dbgchan2() { return 1; }
int32_t dbgchan3() { return 1; }
int32_t dbgchan4() { return 1; }
int32_t dbgchan5() { return 1; }
int32_t dbgchan6() { return 1; }
int32_t dbgchan7() { return 1; }
int32_t dbgchan8() { return 1; }
int32_t dbgchan9() { return 1; }

// 0x1014fce4: cycles the anaglyph bits (0xc) of the display flags: 0xc -> 0x4, 0x4 -> 0, anything else -> 0xc.
int32_t AnaglyphToggle() {
    uint32_t flags = gei_Display.mFlags;
    switch (flags & 0xc) {
    case 0xc:
        gei_Display.mFlags = flags & ~0xcu;
        gei_Display.mFlags |= 4;
        break;
    case 4:
        gei_Display.mFlags = flags & ~0xcu;
        break;
    default:
        gei_Display.mFlags |= 0xc;
        break;
    }
    return 1;
}

// keyboard.cpp (main app), unit w43: Enable 0x101500b8, Disable 0x101500ec and EnableAllKeytables 0x10151010.

// unit w53: Search 0x1014fd68. The first match wins; count is signed.
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    for (int32_t index = 0; index < mCount; ++index) {
        if (mEntries[index].mKey == key && mEntries[index].mModifiers == modifiers)
            return static_cast<int16_t>(index);
    }
    return -1;
}

// keyboard.cpp (main app), unit w50: CKeyTable::KeyDown 0x1014fdec and KeyUp 0x1014fef0 (216 bytes each).

// The index of the first entry whose key and modifiers match, -1 when none does. The Mac keeps it in a 16-bit register
// (extsh) and stops the loop at the first hit, so a later entry of the same key never wins.
static int32_t FindEntry(const CKeyTable* table, int16_t key, int16_t modifiers) {
    int16_t found = -1;
    for (int32_t index = 0; index < table->mCount && found < 0; ++index) {
        const CKeyTableEntry& entry = table->mEntries[index];
        if (entry.mKey == key && entry.mModifiers == modifiers) found = static_cast<int16_t>(index);
    }
    return found;
}

// A disabled table answers 0 and touches nothing. For the first entry that matches: its down action (when it has one) is
// called with 0, then the entry's state becomes 1 (re-read through the table, so an action that moves the table is
// followed). The answer is the action's, 0 when there is no action or no entry.
int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    if (mEnabled == 0) return 0;
    const int32_t found = FindEntry(this, key, modifiers);
    if (found <= -1) return 0;
    int32_t answer = 0;
    if (mEntries[found].mAction != nullptr) answer = reinterpret_cast<CKeyAction>(mEntries[found].mAction)(0);
    mEntries[found].mState = 1;
    return answer;
}

// The same for a key released: the second action (the word at entry +0x0c) and state 0.
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    if (mEnabled == 0) return 0;
    const int32_t found = FindEntry(this, key, modifiers);
    if (found <= -1) return 0;
    int32_t answer = 0;
    if (mEntries[found].mUpAction != nullptr) answer = reinterpret_cast<CKeyAction>(mEntries[found].mUpAction)(0);
    mEntries[found].mState = 0;
    return answer;
}

// keyboard.cpp (main app): CKeyTable::GetState 0x1014fff0 (156 bytes). Translated by the maintainer 2026-10-14 from the listing
//; module test_CameraMessages.
//
// The state of the entry with this id (KeyDown sets 1, KeyUp 0); the table's enabled word is not looked at. The entry at the id's
// own index is tried first, then the first entry with that id; an id out of the table or with no entry answers 0.

int16_t CKeyTable::GetState(int16_t id) {
    if (id < 0 || id >= mCount) return 0;
    if (mEntries[id].mId == id) return mEntries[id].mState;
    int16_t found = -1;
    for (int32_t index = 0; index < mCount && found < 0; ++index) {
        if (mEntries[index].mId == id) found = static_cast<int16_t>(index);
    }
    if (found > -1) return mEntries[found].mState;
    return 0;
}

void CKeyTable::Enable() { mEnabled = 1; }

void CKeyTable::Disable() {
    mEnabled = 0;
    for (int32_t index = 0; index < mCount; ++index) mEntries[index].mState = 0;
}

// keyboard.cpp: the seven static entry arrays, in Mac order (unit w53).
// Generated by a one-off script

// 0x102a4e58: 12 entries.
CKeyTableEntry gKeyEntriesSystem[12] = {
    {0, 80, 0, 0, {}, reinterpret_cast<void*>(pause), nullptr, "pause"},
    {1, 72, 12, 0, {}, reinterpret_cast<void*>(ToggleHelpText), nullptr, "toghelp"},
    {2, 119, 0, 0, {}, reinterpret_cast<void*>(snapshot), nullptr, nullptr},
    {3, 119, 63, 0, {}, reinterpret_cast<void*>(continuous_snapshot), nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
};

// 0x102a4f48: 15 entries.
CKeyTableEntry gKeyEntriesGame[15] = {
    {0, 27, 0, 0, {}, reinterpret_cast<void*>(abort_all_requesters), nullptr, nullptr},
    {1, 44, 0, 0, {}, reinterpret_cast<void*>(turn_object_clockwise), nullptr, nullptr},
    {2, 46, 0, 0, {}, reinterpret_cast<void*>(turn_object_counterclockwise), nullptr, nullptr},
    {3, 118, 0, 0, {}, reinterpret_cast<void*>(enter_area_delete_mode), nullptr, nullptr},
    {4, 114, 0, 0, {}, reinterpret_cast<void*>(togglegamewindow), nullptr, nullptr},
    {5, 8, 0, 0, {}, reinterpret_cast<void*>(backtrack), nullptr, nullptr},
    {6, 27904, 0, 0, {}, reinterpret_cast<void*>(slow_down_time), nullptr, nullptr},
    {7, 27392, 0, 0, {}, reinterpret_cast<void*>(speed_up_time), nullptr, nullptr},
    {8, 27136, 0, 0, {}, reinterpret_cast<void*>(normal_speed_time), nullptr, nullptr},
    {9, 67, 12, 0, {}, reinterpret_cast<void*>(close_park), nullptr, nullptr},
    {10, 79, 12, 0, {}, reinterpret_cast<void*>(open_park), nullptr, nullptr},
    {11, 76, 48, 0, {}, reinterpret_cast<void*>(quickload), nullptr, nullptr},
    {12, 83, 48, 0, {}, reinterpret_cast<void*>(quicksave), nullptr, nullptr},
    {13, 112, 0, 0, {}, reinterpret_cast<void*>(context_help), nullptr, nullptr},
    {14, 86, 48, 0, {}, reinterpret_cast<void*>(show_version), nullptr, nullptr},
};

// 0x102a5074: 25 entries.
CKeyTableEntry gKeyEntriesCamera[25] = {
    {0, 9728, 0, 0, {}, nullptr, nullptr, nullptr},
    {1, 10240, 0, 0, {}, nullptr, nullptr, nullptr},
    {2, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {3, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {4, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {5, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {6, 9472, 0, 0, {}, nullptr, nullptr, nullptr},
    {7, 9984, 0, 0, {}, nullptr, nullptr, nullptr},
    {8, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {9, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {10, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {11, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {12, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {13, 104, 0, 0, {}, nullptr, nullptr, nullptr},
    {14, 98, 0, 0, {}, nullptr, nullptr, nullptr},
    {15, 100, 0, 0, {}, nullptr, nullptr, nullptr},
    {17, 32, 63, 0, {}, reinterpret_cast<void*>(AllowFullCameraZoomout), nullptr, nullptr},
    {18, 105, 0, 0, {}, nullptr, nullptr, nullptr},
    {19, 103, 0, 0, {}, nullptr, nullptr, nullptr},
    {20, 101, 0, 0, {}, reinterpret_cast<void*>(ResetCameraPosition), nullptr, nullptr},
    {21, 97, 0, 0, {}, nullptr, nullptr, nullptr},
    {22, 99, 0, 0, {}, nullptr, nullptr, nullptr},
    {16, 102, 0, 0, {}, nullptr, nullptr, nullptr},
    {23, 116, 12, 0, {}, reinterpret_cast<void*>(AnaglyphToggle), nullptr, nullptr},
    {24, 9216, 0, 0, {}, reinterpret_cast<void*>(ResetCameraPosition), nullptr, nullptr},
};

// 0x102a5998: 42 entries.
CKeyTableEntry gKeyEntriesCheat[42] = {
    {0, 122, 48, 0, {}, reinterpret_cast<void*>(WeatherDown), nullptr, nullptr},
    {1, 123, 48, 0, {}, reinterpret_cast<void*>(WeatherUp), nullptr, nullptr},
    {2, 122, 3, 0, {}, reinterpret_cast<void*>(WeatherDownLots), nullptr, nullptr},
    {3, 123, 3, 0, {}, reinterpret_cast<void*>(WeatherUpLots), nullptr, nullptr},
    {4, 123, 12, 0, {}, reinterpret_cast<void*>(WeatherReset), nullptr, nullptr},
    {5, 123, 15, 0, {}, reinterpret_cast<void*>(DeactivateWeather), nullptr, nullptr},
    {6, 123, 63, 0, {}, reinterpret_cast<void*>(RealTimeLightingMasterToggle), nullptr, nullptr},
    {7, 122, 63, 0, {}, reinterpret_cast<void*>(FPSDisplayToggle), nullptr, nullptr},
    {8, 76, 3, 0, {}, reinterpret_cast<void*>(place_litter), nullptr, nullptr},
    {9, 86, 3, 0, {}, reinterpret_cast<void*>(place_vomit), nullptr, nullptr},
    {10, 89, 3, 0, {}, reinterpret_cast<void*>(cheat_cleanup_park), nullptr, nullptr},
    {11, 82, 48, 0, {}, reinterpret_cast<void*>(complete_all_research), nullptr, nullptr},
    {12, 82, 51, 0, {}, reinterpret_cast<void*>(uncomplete_all_research), nullptr, nullptr},
    {13, 49, 3, 0, {}, reinterpret_cast<void*>(spawn_person), nullptr, nullptr},
    {14, 49, 15, 0, {}, reinterpret_cast<void*>(spawn_five_persons), nullptr, nullptr},
    {15, 49, 63, 0, {}, reinterpret_cast<void*>(populate_park), nullptr, nullptr},
    {16, 49, 51, 0, {}, reinterpret_cast<void*>(start_winning_sequence), nullptr, nullptr},
    {17, 48, 51, 0, {}, reinterpret_cast<void*>(start_bankruptcy_sequence), nullptr, nullptr},
    {18, 53, 3, 0, {}, reinterpret_cast<void*>(spawn_guard), nullptr, nullptr},
    {19, 54, 3, 0, {}, reinterpret_cast<void*>(spawn_researcher), nullptr, nullptr},
    {20, 55, 3, 0, {}, reinterpret_cast<void*>(spawn_mechanic), nullptr, nullptr},
    {21, 56, 3, 0, {}, reinterpret_cast<void*>(spawn_handyman), nullptr, nullptr},
    {22, 57, 3, 0, {}, reinterpret_cast<void*>(spawn_entertainer), nullptr, nullptr},
    {23, 48, 3, 0, {}, reinterpret_cast<void*>(spawn_bag_o_staff), nullptr, nullptr},
    {24, 97, 12, 0, {}, reinterpret_cast<void*>(apply_for_loan_1), nullptr, nullptr},
    {25, 98, 12, 0, {}, reinterpret_cast<void*>(apply_for_loan_2), nullptr, nullptr},
    {26, 99, 12, 0, {}, reinterpret_cast<void*>(apply_for_loan_3), nullptr, nullptr},
    {27, 100, 12, 0, {}, reinterpret_cast<void*>(apply_for_loan_4), nullptr, nullptr},
    {28, 49, 48, 0, {}, reinterpret_cast<void*>(get_doshed_up), nullptr, nullptr},
    {29, 71, 48, 0, {}, reinterpret_cast<void*>(win_ticket_0), nullptr, nullptr},
    {30, 97, 48, 0, {}, reinterpret_cast<void*>(win_ticket_1), nullptr, nullptr},
    {31, 98, 48, 0, {}, reinterpret_cast<void*>(win_ticket_2), nullptr, nullptr},
    {32, 99, 48, 0, {}, reinterpret_cast<void*>(win_ticket_3), nullptr, nullptr},
    {33, 100, 48, 0, {}, reinterpret_cast<void*>(win_ticket_4), nullptr, nullptr},
    {34, 101, 48, 0, {}, reinterpret_cast<void*>(win_ticket_5), nullptr, nullptr},
    {35, 102, 48, 0, {}, reinterpret_cast<void*>(win_ticket_6), nullptr, nullptr},
    {36, 103, 48, 0, {}, reinterpret_cast<void*>(win_ticket_7), nullptr, nullptr},
    {37, 104, 48, 0, {}, reinterpret_cast<void*>(win_ticket_8), nullptr, nullptr},
    {38, 105, 48, 0, {}, reinterpret_cast<void*>(win_ticket_9), nullptr, nullptr},
    {39, 66, 3, 0, {}, reinterpret_cast<void*>(force_ride_breakdown), nullptr, nullptr},
    {40, 66, 51, 0, {}, reinterpret_cast<void*>(force_ride_worn), nullptr, nullptr},
    {41, 83, 60, 0, {}, reinterpret_cast<void*>(start_strike), nullptr, nullptr},
};

// 0x102a5268: 4 entries.
CKeyTableEntry gKeyEntriesCoaster[4] = {
    {0, 67, 48, 0, {}, reinterpret_cast<void*>(OutputCoasterValues), nullptr, nullptr},
    {1, 85, 48, 0, {}, reinterpret_cast<void*>(ToggleDrawUpSpline), nullptr, nullptr},
    {2, 27, 0, 0, {}, reinterpret_cast<void*>(abort_coaster_edit), nullptr, "abortcoaster"},
    {3, 8, 0, 0, {}, reinterpret_cast<void*>(backtrack), nullptr, "backtrack"},
};

// 0x102a52b8: 17 entries.
CKeyTableEntry gKeyEntriesShortcuts[17] = {
    {0, 27, 0, 0, {}, reinterpret_cast<void*>(mainmenu), nullptr, "menu"},
    {1, 66, 0, 0, {}, reinterpret_cast<void*>(Screen_BuyObject), nullptr, "buy"},
    {2, 72, 0, 0, {}, reinterpret_cast<void*>(Screen_BuyStaff), nullptr, "hire"},
    {3, 73, 0, 0, {}, reinterpret_cast<void*>(Screen_ParkStatus), nullptr, "parkstatus"},
    {4, 65, 0, 0, {}, reinterpret_cast<void*>(Screen_AllItems), nullptr, "allitems"},
    {5, 83, 0, 0, {}, reinterpret_cast<void*>(Screen_AllStaff), nullptr, "allstaff"},
    {6, 86, 0, 0, {}, reinterpret_cast<void*>(Screen_AllPeeps), nullptr, "allpeeps"},
    {7, 70, 0, 0, {}, reinterpret_cast<void*>(Screen_Finance), nullptr, "financeinfo"},
    {8, 76, 0, 0, {}, reinterpret_cast<void*>(Screen_Loans), nullptr, "loans"},
    {9, 84, 0, 0, {}, reinterpret_cast<void*>(Screen_StaffCosts), nullptr, "staffcosts"},
    {10, 71, 0, 0, {}, reinterpret_cast<void*>(Screen_EntryPrice), nullptr, "entryprice"},
    {11, 82, 0, 0, {}, reinterpret_cast<void*>(Screen_Research), nullptr, "research"},
    {12, 32, 0, 0, {}, reinterpret_cast<void*>(Screen_Map), nullptr, "map"},
    {13, 83, 12, 0, {}, reinterpret_cast<void*>(Screen_StaffLoc), nullptr, "staffloc"},
    {14, 86, 12, 0, {}, reinterpret_cast<void*>(Screen_PeepLoc), nullptr, "peeploc"},
    {15, 80, 12, 0, {}, reinterpret_cast<void*>(TakePostCard), nullptr, "postcard"},
    {16, 67, 0, 0, {}, reinterpret_cast<void*>(CamcorderMode), nullptr, "camcorder"},
};

// 0x102a540c: 71 entries.
CKeyTableEntry gKeyEntriesDebug[71] = {
    {0, 8, 48, 0, {}, nullptr, nullptr, nullptr},
    {1, 11520, 0, 0, {}, reinterpret_cast<void*>(show_script), nullptr, nullptr},
    {2, 11520, 48, 0, {}, reinterpret_cast<void*>(LoadPtclLib), nullptr, nullptr},
    {3, 13, 3, 0, {}, nullptr, nullptr, nullptr},
    {4, 66, 15, 0, {}, reinterpret_cast<void*>(force_ride_condemned), nullptr, nullptr},
    {6, 70, 48, 0, {}, reinterpret_cast<void*>(force_ride_fix), nullptr, nullptr},
    {7, 66, 48, 0, {}, reinterpret_cast<void*>(place_bomb), nullptr, nullptr},
    {8, 66, 12, 0, {}, reinterpret_cast<void*>(park_attraction_value), nullptr, nullptr},
    {9, 68, 0, 0, {}, reinterpret_cast<void*>(debug_cell_dirs), nullptr, nullptr},
    {10, 68, 12, 0, {}, reinterpret_cast<void*>(debug_track_cell_dirs), nullptr, nullptr},
    {11, 0, 0, 0, {}, reinterpret_cast<void*>(toggle_cell_flash), nullptr, nullptr},
    {12, 78, 0, 0, {}, reinterpret_cast<void*>(debug_cell_neighbs), nullptr, nullptr},
    {13, 78, 12, 0, {}, reinterpret_cast<void*>(debug_track_cell_neighbs), nullptr, nullptr},
    {14, 78, 3, 0, {}, reinterpret_cast<void*>(debug_hoarding_cell_neighbs), nullptr, nullptr},
    {15, 85, 0, 0, {}, reinterpret_cast<void*>(select_debug_person), nullptr, nullptr},
    {16, 85, 3, 0, {}, reinterpret_cast<void*>(set_allow_loops_no_research), nullptr, nullptr},
    {17, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {20, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {18, 72, 3, 0, {}, reinterpret_cast<void*>(save_hoardings), nullptr, nullptr},
    {19, 72, 15, 0, {}, reinterpret_cast<void*>(save_complete_hoardings), nullptr, nullptr},
    {21, 71, 3, 0, {}, reinterpret_cast<void*>(enter_hoarding_mode), nullptr, nullptr},
    {23, 73, 12, 0, {}, reinterpret_cast<void*>(show_person_history_info), nullptr, nullptr},
    {22, 74, 0, 0, {}, reinterpret_cast<void*>(enter_area_delete_mode), nullptr, nullptr},
    {24, 0, 0, 0, {}, reinterpret_cast<void*>(wheres_my_script), nullptr, nullptr},
    {25, 48, 12, 0, {}, reinterpret_cast<void*>(dbgchan0), nullptr, nullptr},
    {26, 49, 12, 0, {}, reinterpret_cast<void*>(dbgchan1), nullptr, nullptr},
    {27, 50, 12, 0, {}, reinterpret_cast<void*>(dbgchan2), nullptr, nullptr},
    {28, 51, 12, 0, {}, reinterpret_cast<void*>(dbgchan3), nullptr, nullptr},
    {29, 52, 12, 0, {}, reinterpret_cast<void*>(dbgchan4), nullptr, nullptr},
    {30, 53, 12, 0, {}, reinterpret_cast<void*>(dbgchan5), nullptr, nullptr},
    {31, 54, 12, 0, {}, reinterpret_cast<void*>(dbgchan6), nullptr, nullptr},
    {32, 55, 12, 0, {}, reinterpret_cast<void*>(dbgchan7), nullptr, nullptr},
    {33, 56, 12, 0, {}, reinterpret_cast<void*>(dbgchan8), nullptr, nullptr},
    {34, 57, 12, 0, {}, reinterpret_cast<void*>(dbgchan9), nullptr, nullptr},
    {35, 116, 3, 0, {}, reinterpret_cast<void*>(ChatterToggle), nullptr, nullptr},
    {36, 91, 0, 0, {}, reinterpret_cast<void*>(DecCoasterConst1), nullptr, nullptr},
    {37, 91, 3, 0, {}, reinterpret_cast<void*>(DecCoasterConst2), nullptr, nullptr},
    {38, 93, 0, 0, {}, reinterpret_cast<void*>(IncCoasterConst1), nullptr, nullptr},
    {39, 93, 3, 0, {}, reinterpret_cast<void*>(IncCoasterConst2), nullptr, nullptr},
    {41, 118, 12, 0, {}, reinterpret_cast<void*>(delete_ride), nullptr, nullptr},
    {42, 11520, 12, 0, {}, reinterpret_cast<void*>(togglehud), nullptr, nullptr},
    {43, 8960, 0, 0, {}, reinterpret_cast<void*>(TestAdvisor), nullptr, nullptr},
    {44, 8960, 12, 0, {}, reinterpret_cast<void*>(TestAdvisor2), nullptr, nullptr},
    {45, 8960, 3, 0, {}, reinterpret_cast<void*>(TestAdvisor3), nullptr, nullptr},
    {46, 8960, 15, 0, {}, reinterpret_cast<void*>(TestAdvisor4), nullptr, nullptr},
    {47, 67, 12, 0, {}, reinterpret_cast<void*>(close_park), nullptr, nullptr},
    {48, 79, 12, 0, {}, reinterpret_cast<void*>(open_park), nullptr, nullptr},
    {49, 67, 63, 0, {}, reinterpret_cast<void*>(start_cheating), nullptr, nullptr},
    {50, 76, 15, 0, {}, reinterpret_cast<void*>(recreate_recording), nullptr, nullptr},
    {51, 76, 63, 0, {}, reinterpret_cast<void*>(recreate_recording_real_time), nullptr, nullptr},
    {52, 76, 51, 0, {}, reinterpret_cast<void*>(recreate_recording_medium_speed), nullptr, nullptr},
    {53, 83, 15, 0, {}, reinterpret_cast<void*>(save_recording), nullptr, nullptr},
    {54, 80, 3, 0, {}, reinterpret_cast<void*>(ToggleProceduralTexturing), nullptr, nullptr},
    {55, 87, 0, 0, {}, reinterpret_cast<void*>(create_arrival_vehicle), nullptr, nullptr},
    {56, 87, 48, 0, {}, reinterpret_cast<void*>(clear_patrol_area), nullptr, nullptr},
    {57, 13, 48, 0, {}, reinterpret_cast<void*>(ToggleFullScreen), nullptr, nullptr},
    {58, 86, 48, 0, {}, reinterpret_cast<void*>(next_debug_text_mode), nullptr, nullptr},
    {59, 71, 48, 0, {}, reinterpret_cast<void*>(win_ticket_0), nullptr, nullptr},
    {60, 97, 48, 0, {}, reinterpret_cast<void*>(win_ticket_1), nullptr, nullptr},
    {61, 98, 48, 0, {}, reinterpret_cast<void*>(win_ticket_2), nullptr, nullptr},
    {62, 99, 48, 0, {}, reinterpret_cast<void*>(win_ticket_3), nullptr, nullptr},
    {63, 100, 48, 0, {}, reinterpret_cast<void*>(win_ticket_4), nullptr, nullptr},
    {64, 101, 48, 0, {}, reinterpret_cast<void*>(win_ticket_5), nullptr, nullptr},
    {65, 102, 48, 0, {}, reinterpret_cast<void*>(win_ticket_6), nullptr, nullptr},
    {66, 103, 48, 0, {}, reinterpret_cast<void*>(win_ticket_7), nullptr, nullptr},
    {67, 104, 48, 0, {}, reinterpret_cast<void*>(win_ticket_8), nullptr, nullptr},
    {68, 105, 48, 0, {}, reinterpret_cast<void*>(win_ticket_9), nullptr, nullptr},
    {69, 120, 12, 0, {}, reinterpret_cast<void*>(quickload_layout), nullptr, nullptr},
    {70, 117, 12, 0, {}, reinterpret_cast<void*>(quicksave_layout), nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
    {0, 0, 0, 0, {}, nullptr, nullptr, nullptr},
};

// keyboard.cpp (main app), unit w53: InitialiseKeytables 0x1015014c,
// DeInitialiseKeyTables 0x1015120c. Table objects have native entry pointers.

namespace {
CKeyTable* MakeTable(CKeyTableEntry* entries, int16_t count, const char* name) {
    auto* table = static_cast<CKeyTable*>(::operator new(sizeof(CKeyTable), std::nothrow));
    if (table != nullptr) {
        table->mEntries = entries;
        table->mCount = count;
        table->mEnabled = 1;
        std::strncpy(table->mName, name, 15);
    }
    return table;
}

// The indexed entry is preferred even when an earlier entry has the same id.
// FormatString may change the table's fields, so read the entries after the call.
void NameKey(CKeyTable* table, int16_t id, uint32_t textId) {
    if (table->mCount <= id) return;
    char16_t buffer[2];
    static_cast<TbITable*>(gText.mResourcePointers[2])->FormatString(buffer, 2, textId);
    CKeyTableEntry* entries = table->mEntries;
    if (entries[id].mId == id) {
        entries[id].mKey = static_cast<int16_t>(buffer[0]);
        return;
    }
    for (int32_t index = 0; index < table->mCount; ++index) {
        if (entries[index].mId == id) {
            entries[static_cast<int16_t>(index)].mKey = static_cast<int16_t>(buffer[0]);
            return;
        }
    }
}
}

void InitialiseKeytables() {
    gKeyTable = MakeTable(gKeyEntriesSystem, 12, "system");
    gKeyTableGame = MakeTable(gKeyEntriesGame, 15, "game");
    gKeyTableCamera = MakeTable(gKeyEntriesCamera, 25, "camera");
    gKeyTableCheat = MakeTable(gKeyEntriesCheat, 42, "cheat");
    gKeyTableCoaster = MakeTable(gKeyEntriesCoaster, 4, "coaster");
    gKeyTableShortcuts = MakeTable(gKeyEntriesShortcuts, 17, "shortcuts");
    for (int16_t id = 1; id <= 11; ++id) NameKey(gKeyTableShortcuts, id, id - 1);
    NameKey(gKeyTableShortcuts, 16, 11);
    NameKey(gKeyTableShortcuts, 13, 12);
    NameKey(gKeyTableShortcuts, 14, 13);
    NameKey(gKeyTableShortcuts, 15, 14);
    NameKey(gKeyTableGame, 1, 18);
    NameKey(gKeyTableGame, 2, 19);
    NameKey(gKeyTableGame, 9, 16);
    NameKey(gKeyTableGame, 10, 17);
    NameKey(gKeyTable, 0, 20);
    NameKey(gKeyTable, 1, 15);
    gKeyTableDebug = MakeTable(gKeyEntriesDebug, 71, "debug");
}

// The coaster table keeps its enabled word and every entry keeps its state.
void EnableAllKeytables() {
    gKeyTable->mEnabled = 1;
    gKeyTableGame->mEnabled = 1;
    gKeyTableCamera->mEnabled = 1;
    gKeyTableCheat->mEnabled = 1;
    gKeyTableShortcuts->mEnabled = 1;
    gKeyTableDebug->mEnabled = 1;
}

// DisableAllKeytables (main app, keyboard.cpp, 0x1015108c, 340 bytes). Unit w39.
//
// Disables six of the seven key tables: the enabled word of each is cleared and so is the state word of each of its entries (the
// original inlines CKeyTable::Disable, 0x101500ec, six times). The coaster table (0x1037f224) is not in the list: it stays as it is.
// The seven table pointers are made by InitialiseKeytables (0x1015014c, untranslated), which defines them too when it is translated.

CKeyTable* gKeyTable;            // 0x1037f234
CKeyTable* gKeyTableGame;        // 0x1037f230
CKeyTable* gKeyTableCamera;      // 0x1037f22c
CKeyTable* gKeyTableCheat;       // 0x1037f228
CKeyTable* gKeyTableCoaster;     // 0x1037f224
CKeyTable* gKeyTableShortcuts;   // 0x1037f220
CKeyTable* gKeyTableDebug;       // 0x1037f21c

namespace {

// Clears the table's enabled word and then every entry's state word; the count is a signed short, so a negative one has no entries.
// (A table that does not exist is not tested for: the original writes through it.)
void DisableTable(CKeyTable* table) {
    table->mEnabled = 0;
    for (int32_t index = 0; index < table->mCount; ++index) table->mEntries[index].mState = 0;
}

}  // namespace

void DisableAllKeytables() {
    DisableTable(gKeyTable);
    DisableTable(gKeyTableGame);
    DisableTable(gKeyTableCamera);
    DisableTable(gKeyTableCheat);
    DisableTable(gKeyTableShortcuts);
    DisableTable(gKeyTableDebug);
}

// The globals deliberately retain their freed pointers.
void DeInitialiseKeyTables() {
    if (gKeyTable != nullptr) ::operator delete(gKeyTable);
    if (gKeyTableGame != nullptr) ::operator delete(gKeyTableGame);
    if (gKeyTableCamera != nullptr) ::operator delete(gKeyTableCamera);
    if (gKeyTableCheat != nullptr) ::operator delete(gKeyTableCheat);
    if (gKeyTableCoaster != nullptr) ::operator delete(gKeyTableCoaster);
    if (gKeyTableShortcuts != nullptr) ::operator delete(gKeyTableShortcuts);
    if (gKeyTableDebug != nullptr) ::operator delete(gKeyTableDebug);
}

// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.

// ---------------------------------------------------------------------------------------------------------------- keyboard.cpp initialiser

int32_t gKeyboardF28C;
int32_t gKeyboardF288;
alignas(16) uint8_t gKeyboardWideString[48];
uint8_t gKeyboardWideNode[32];
uint8_t gKeyboardQuickNode[32];
static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof(gKeyboardWideString), "wide string storage");
static_assert(sizeof(TbDynamicStringTemplate<char>) <= sizeof(TbStringBase<char>) + 8, "narrow string storage");

// 0x101512fc (120 bytes). Two words are set (0 and 0x10000); a wide string (the text at 0x10248c7e) and the narrow quick-save
// suffix ("_online", gQuickSaveSuffix) are built and each registered for destruction.
void __sinit_keyboard_cpp() {
    gKeyboardF28C = 0;
    gKeyboardF288 = 0x10000;
    new (gKeyboardWideString) TbDynamicStringTemplate<char16_t>(u"gms.dat");
    __register_global_object(gKeyboardWideString, reinterpret_cast<void*>(&UIMain_DestroyWString), gKeyboardWideNode);
    new (&gQuickSaveSuffix) TbDynamicStringTemplate<char>("_online");
    __register_global_object(&gQuickSaveSuffix, reinterpret_cast<void*>(&UIMain_DestroyString), gKeyboardQuickNode);
}
