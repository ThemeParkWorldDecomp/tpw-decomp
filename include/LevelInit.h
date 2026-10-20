// The start of a park (unit w106): the functions LevelInit1 runs, and the globals the first initialisation sets.
// SetAnimationDetail and RSSE_Init's words are in the rsse.cpp range (src/rsse_init.cpp), LoadLevelAdvisors,
// SetupLevelLights and LevelInit1 in the TPRide.cpp range (src/LevelInit.cpp).
#pragma once
#include <cstdint>

#include "Light.h"
#include "m3d2.h"

// 0x100c7c68 (src/rsse_init.cpp, 12 bytes): stores `detail` in the word at gTPGlobals + 0x400c (TPGlobals::mEngineFlags),
// replacing all its bits.
void SetAnimationDetail(uint32_t detail);
// RSSE_Init's two words besides the state (include/rsse.h's gRSSEState): the sum of the characters of the script op table,
// which RSSE_InitScript compares with the one in a script file, and a word that is only cleared.
extern uint32_t gRSSEScriptChecksum;   // 0x102f7be8
extern uint32_t gRSSEF7BE4;            // 0x102f7be4

// 0x1006375c (300 bytes, src/LevelInit.cpp): loads the global advisor model (slot 0, "<data>:Global:Advisor" with the theme
// texture folder under the theme path) and the theme's (slot 1, variant "_" and the first three letters of the theme's name),
// then starts the advisors.
void LoadLevelAdvisors();
// What CreateGlobalLight (0x10063d24, 1,156 bytes, src/LevelLights.cpp, unit w110) answers: a record of 0x14 bytes on the Mac
// (type at +0, +4 the directional light, +8 never written, +0xc the point light, the colour word at +0x10), allocated zeroed
// and leaked when the type is 0. The lights are the objects themselves (the Mac's record holds pointers to their 0x74 bytes, the
// first word of which points at the CLight inside). The host's record has native pointers.
struct sGlobalLight {
    int32_t mType;                  // 0x00  the light source type: 1 the engine's ambient light (no object), 2 directional, 3 point
    CLightParallel* mDirectional;   // 0x04  (type 2)
    void* mF08;                     // 0x08
    CLightPoint* mPoint;            // 0x0c  (type 3)
    uint32_t mColour;               // 0x10
};
// 0x10063d24 (src/LevelLights.cpp): makes a light of `type` with the colour word `colour` (RGBA bytes) and answers its record.
// Type 2: a directional light along `direction`; 3: a point light at `position` (a radius of 100); both are linked into the
// map's list 0x70. Type 1 sets the engine's GlobalDiffuse from the colour (alpha 1) and makes no object; type 0 reports "Invalid
// lightsource creation attempted" and answers NULL (the record leaks); any other type gives the bare record.
sGlobalLight* CreateGlobalLight(int32_t type, uint32_t colour, s3DPoint* position, s3DPoint* direction);
// The park's light (original names unknown).
extern sGlobalLight* gGlobalLight;       // 0x102c6c84  what CreateGlobalLight answered
extern s3DPoint gGlobalLightDirection;   // 0x102c6c88  the direction the level file gives the sun; SetupLevelLights normalises it
extern uint32_t gGlobalLightColour;      // 0x102c6c94  the light's colour word
// The level's ambient light colour, ARGB rotated left by a byte (CGame::InitLevel_Initialise, src/Game_level.cpp, unit w117); TickLevelLights
// (0x100645b0, untranslated) reads it. Global 0x102c6c98, original name unknown.
extern uint32_t gGlobalAmbientColour;
// The lowest values InitLevel_Initialise lets gHeightfieldRangeExtraX and gHeightfieldRangeExtraY take (it clamps the balance's
// ProceduralTexturing offsets up to them). Initialised data words 0x102a4d6c and 0x102a4d70, both 0 in the image and written by no code found;
// names invented.
extern int32_t gLevelRangeMinX;          // 0x102a4d6c
extern int32_t gLevelRangeMinY;          // 0x102a4d70
// 0x10064374 (208 bytes): makes the park's directional light from the sun's direction (normalised in place) and the colour,
// and sets bit 0x20 of the light's flags.
void SetupLevelLights();
// The word LevelInit1 fills with rand() (0x102f7bd0, the end of the particle system's run of globals; original name unknown).
extern int32_t gLevelRandom;
// The scale TickLevelLights gives the sun's direction (initialised data float 0x1029cf78, 1.0f; original name unknown).
extern float gGlobalLightScale;
