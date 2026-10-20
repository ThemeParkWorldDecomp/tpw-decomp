// keyboard.cpp (main app): eight key actions (unit v5034): the ride turns, the quick saves and loads, the vomit and the advisor test.
//   turn_object_counterclockwise 0x1014cafc, turn_object_clockwise 0x1014ca74, quickload_layout 0x1014c97c, quicksave_layout 0x1014c89c,
//   place_vomit 0x1014ce2c, quickload 0x1014c7bc, quicksave 0x1014c6dc, TestAdvisor 0x1014c454. Each answers 1.
#include "keytable_actions.h"

#include <cstring>

#include "ActionRecorder.h"
#include "Advisor.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "SaveLoad.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "World.h"
#include "intrface.h"

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

// The ride's rotation in degrees, turned by 90 (clockwise) or 270 (counterclockwise), the remainder of 360 signed.
int32_t turn_object_counterclockwise() {
    int32_t rotation = LLCC_GetRideRotation() + 0x10e;
    LLCC_SetRideRotation(rotation % 0x168, true);
    return 1;
}

int32_t turn_object_clockwise() {
    int32_t rotation = LLCC_GetRideRotation() + 0x5a;
    LLCC_SetRideRotation(rotation % 0x168, true);
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

// The litter goes on the cell under the pointer (the id is the 16-bit mF24; id 0 reaches the cell before the first, as the original does).
int32_t place_vomit() {
    uint16_t id = gIntrface.mF24;
    CMapCell* cell = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 +
                                                 static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x44);
    cell->PlaceLitter(static_cast<CMapCell::LitterType>(7));
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

int32_t TestAdvisor() {
    if (ADV_Stat() == 0) {
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    } else {
        ADV_Slap();
    }
    return 1;
}
