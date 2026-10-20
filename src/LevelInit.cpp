// TPRide.cpp range of the main app (SimTheme Park 2.04b3): the first initialisation of a park, LevelInit1 (0x100638b0, 220
// bytes) and the two parts of it that load the advisors and make the sun (LoadLevelAdvisors 0x1006375c, SetupLevelLights
// 0x10064374). LevelInit2 (0x100639b0) is the second stage.
#include "LevelInit.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Advisor.h"
#include "FS_System.h"
#include "Game.h"
#include "Main.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "platform/msl.h"
#include "ui_ErrorDisplay.h"

sGlobalLight* gGlobalLight;       // 0x102c6c84
s3DPoint gGlobalLightDirection;   // 0x102c6c88
uint32_t gGlobalLightColour;      // 0x102c6c94
int32_t gLevelRandom;             // 0x102f7bd0

// Two advisors: slot 0 is the global one ("<data path>:Global:Advisor", textures from "<theme path>:Advisor:<texture
// folder>"), slot 1 the theme's (models in "<theme path>:Advisor", textures in "<data path>:Global:Advisor:<texture folder>",
// variant "_" and the first three letters of the current theme's name). The buffers are the frame's: 1,036, 1,024 and 1,024
// bytes (the first is the one the first sprintf fills).
void LoadLevelAdvisors() {
    char globalPath[1036];
    char folder[1024];
    char variant[1024];
    std::sprintf(globalPath, "%s:Global:Advisor", gTPFileInfo.GetDataPath().mData);
    std::sprintf(folder, "%s:Advisor:%s", get_theme_path(), gThemeTextureFolder);
    LoadAdvisor(globalPath, folder, nullptr, 0);
    std::strcpy(variant, "_");
    std::strncat(variant, gGame.GetCurrentThemeName().mData, 3);
    std::sprintf(globalPath, "%s:Advisor", get_theme_path());
    std::sprintf(folder, "%s:Global:Advisor:%s", gTPFileInfo.GetDataPath().mData, gThemeTextureFolder);
    LoadAdvisor(globalPath, folder, variant, 1);
    ADV_Init();
}

// The sun: its direction is normalised in place (the length is the square root, taken in double, of z squared plus x squared
// plus y squared, where x squared is exact in double and the other two squares are rounded to single first; the length is
// rounded to single and each component divided by it, so a zero direction gives NaN), a directional light (type 2) is made
// with the colour word, and its flags get bit 0x20.
void SetupLevelLights() {
    float* direction = &gGlobalLightDirection.x;
    const double x = direction[0];
    const float yy = direction[1] * direction[1];
    const float zz = direction[2] * direction[2];
    const double sum = static_cast<double>(zz) + (x * x + static_cast<double>(yy));
    const float length = static_cast<float>(std::sqrt(sum));
    direction[0] = direction[0] / length;
    direction[1] = direction[1] / length;
    direction[2] = direction[2] / length;
    gGlobalLight = CreateGlobalLight(2, gGlobalLightColour, nullptr, &gGlobalLightDirection);
    CLight* light = gGlobalLight->mDirectional;
    light->mFlags |= 0x20;
}

// The first stage of a park: the level's random word, the animation detail 0, the camera, the height field (with a detail
// from the dialog's options: bit 4 gives 0x10, else bit 2 0x20, else bit 1 0x40, else 0x20), the advisors and the sun.
// The original also looks for the last ':' in the theme path and drops the answer (kept). Answers 1.
int32_t LevelInit1() {
    gLevelRandom = msl_rand();
    SetAnimationDetail(0);
    InitCamera();
    const char* themePath = get_theme_path();
    if (themePath != nullptr) static_cast<void>(std::strrchr(themePath, ':'));
    int32_t detail;
    const uint32_t options = gTPDialogOptions;
    if ((options & 4) != 0) detail = 0x10;
    else if ((options & 2) != 0) detail = 0x20;
    else if ((options & 1) != 0) detail = 0x40;
    else detail = 0x20;
    CreateScratchGeomBuffer(1);
    LoadLevelHeightfield(0, detail);
    LoadLevelAdvisors();
    Load1stPersonMesh();
    SetupLevelLights();
    return 1;
}
