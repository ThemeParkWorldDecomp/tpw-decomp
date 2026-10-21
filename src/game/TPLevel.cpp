// TPLevel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "Debug.h"
#include "FS_System.h"
#include "Game.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "LevelInit.h"
#include "LevelInit2.h"
#include "LevelSystems.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "m3d2.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapWho.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "tb_hfield.h"
#include "tb_resource_system.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

// TPRide.cpp range of the main app (SimTheme Park 2.04b3): the first initialisation of a park, LevelInit1 (0x100638b0, 220
// bytes) and the two parts of it that load the advisors and make the sun (LoadLevelAdvisors 0x1006375c, SetupLevelLights
// 0x10064374). LevelInit2 (0x100639b0) is the second stage.

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

// The park's second initialisation and its teardown (main application, TPRide.cpp range; unit w110): LevelInit2 and LevelDeInit,
// with the four height field accessors of CWorld they read the level's place and size through.

// The height field's place and size (names invented).
int32_t gHeightfieldXStart;        // 0x102c6c70
int32_t gHeightfieldYStart;        // 0x102c6c6c
int32_t gHeightfieldWidth;         // 0x102c6c68
int32_t gHeightfieldHeight;        // 0x102c6c64
float gHeightfieldXOrigin;         // 0x102c6c60
float gHeightfieldYOrigin;         // 0x102c6c5c
int32_t gHeightfieldRangeExtraX;   // 0x102c6c58
int32_t gHeightfieldRangeExtraY;   // 0x102c6c54

// 0x100639b0 (428 bytes). The level's attribute map is loaded, then the four numbers of the height field are asked of the world
// (height first, then width, y start, x start) and stored in the opposite order. With a height field header that has a height
// field, the two origins are the field's scale words (mF08[2] and mF08[3], at +0x10 and +0x14) times the starts, each start
// taken as an unsigned number (the 0x43300000 idiom: exact in double, rounded to single, then the multiply in single). With
// options bit 0x2000 the procedural textures' range is set to the level's rectangle, widened by two words nothing writes.
// Then the dynamic cell meshes, the poly search, the highlight and scratch buffers (the scratch buffer is made small), the
// heightmap system, the coaster types, the low-level cell system (128 by 128) and the script system. Answers 1. The world the
// accessors are called on is *gGameWorldRef (the Mac loads it twice); they never use it, so it may be NULL.
int32_t LevelInit2() {
    LoadLevelAttributeMap();
    const int32_t height = gGameWorldRef->GetHeightfieldHeight();
    const int32_t width = gGameWorldRef->GetHeightfieldWidth();
    const int32_t yStart = gGameWorldRef->GetHeightfieldYStart();
    const int32_t xStart = gGameWorldRef->GetHeightfieldXStart();
    gHeightfieldXStart = xStart;
    gHeightfieldYStart = yStart;
    gHeightfieldWidth = width;
    gHeightfieldHeight = height;
    tag_sM3D2Header* header = gHeightfieldHeader;
    if (header != nullptr && header->mHField != nullptr) {
        const float xOrigin = static_cast<float>(static_cast<double>(static_cast<uint32_t>(xStart)));
        const float yOrigin = static_cast<float>(static_cast<double>(static_cast<uint32_t>(yStart)));
        gHeightfieldXOrigin = header->mHField->mF08[2] * xOrigin;
        gHeightfieldYOrigin = header->mHField->mF08[3] * yOrigin;
    }
    if ((gGameOptions & 0x2000) != 0) {
        const int32_t farX = gHeightfieldXStart + (gHeightfieldWidth + gHeightfieldRangeExtraX);
        const int32_t farY = gHeightfieldYStart + (gHeightfieldHeight + gHeightfieldRangeExtraY);
        PT_SetRange(gHeightfieldXStart, gHeightfieldYStart, farX, farY);
    }
    DC_SetupDynamicCellMeshes();
    SetupForPolySearch();
    DestroyHighlightColourBuffers();
    DestroyScratchGeomBuffer();
    CreateScratchGeomBuffer(0);
    CreateHighlightColourBuffers();
    SetupHeightmapSystem();
    InitialiseCoasterTypes();
    LLCC_SystemInit(0x80, 0x80);
    SCR_InitScriptSystem();
    return 1;
}

// 0x10063b80 (284 bytes). `keep` (tested as its low byte) is set when the same level is started again: the poly search, the
// sprites, the height field, the ride types and the highlight buffers are then kept. The ride instances are walked by number
// (1 up, the sets table is indexed from 1) while the number is below the count of instances plus free slots, which
// DestroyRideInstance changes, so the sum is read again each time.
void LevelDeInit(bool keep) {
    DC_FreeDynamicCellMeshes();
    DestroyScratchGeomBuffer();
    ADV_Close();
    DestroyAdvisor(1);
    DestroyAdvisor(0);
    ClosedownRideHoardings();
    DestroyCoasterInstances();
    DestroyCoasterTypes();
    DestroyLevelLights();
    if (!keep) SetdownFromPolySearch();
    LLCC_SystemDeInit();
    if (!keep) ESP_RemoveSprites();
    for (uint32_t i = 0; i < gRideCountB + gRideCountA; ++i) {
        if (gRideAnimSets[i + 1] != nullptr) DestroyRideInstance(i + 1);
    }
    if (!keep) {
        DestroyLevelHeightfield();
        DestroyAllRideTypes();
        DestroyHighlightColourBuffers();
    }
    DestroyPathAllocs();
}

// 0x10063cc0
void RestoreLevelHeightfield() { RestoreHFieldDataFromCopy(gHeightfieldHeader, gHeightfieldCopy); }

// The park's lights (main application, TPRide.cpp and Advisor.cpp ranges; unit w110): CreateGlobalLight with the three CLight
// setters it calls. The CMapLinkBase constructor it also calls (0x10007b7c) is inline in include/MapWho.h.

namespace {

// The text DataErrorHandler is given for a light of type 0 (0x1023d3de).
char gInvalidLightText[] = "Invalid lightsource creation attempted";

// What both object branches of CreateGlobalLight do once the light exists. The colour word is stored first; the four colour
// floats follow, each a byte over 255 times 1.0f (the original's multiply by one stays in the listing), in the order alpha
// (low byte), red (top byte), green (bits 16..23), blue (bits 8..15); the light is told it changed before the word and again
// before the caller's vector goes in.
void StoreLightColour(CLight* light, uint32_t colour) {
    gei_System.UpdateLights();
    light->mColorWord = colour;
    const float one = 1.0f;
    const float alpha = static_cast<float>(static_cast<double>(colour & 0xff)) / 255.0f;
    const float red = static_cast<float>(static_cast<double>(colour >> 24)) / 255.0f;
    const float green = static_cast<float>(static_cast<double>((colour >> 16) & 0xff)) / 255.0f;
    const float blue = static_cast<float>(static_cast<double>((colour >> 8) & 0xff)) / 255.0f;
    light->mColor[0] = alpha * one;
    light->mColor[1] = red * one;
    light->mColor[2] = green * one;
    light->mColor[3] = blue * one;
}

}  // namespace

// 0x10063d24 (1,156 bytes). The record is 0x14 bytes of zeroed memory on the Mac (MReleaseAllocF with flag 1; the host's record
// is larger, with native pointers). Type 2 makes a CLightParallel (flags 2, normal (0, 0, 1) and colour white until the colour
// word overwrites the floats), type 3 a CLightPoint (flags 4, radius 100); both copy the caller's vector (direction for 2,
// position for 3, read after the colour and without a NULL test: the original faults on NULL), link themselves into the map's
// list 0x70 and go in the record. Type 1 writes the engine's GlobalDiffuse and makes nothing. Type 0 reports and answers NULL
// (the record is leaked); a negative type or 4 and above answers the bare record. The original tests the new object for NULL
// before its set-up but dereferences it afterwards anyway; the host's new never answers NULL.
sGlobalLight* CreateGlobalLight(int32_t type, uint32_t colour, s3DPoint* position, s3DPoint* direction) {
    sGlobalLight* record = static_cast<sGlobalLight*>(MReleaseAllocF(sizeof(sGlobalLight), 1));
    if (type == 2) {
        CLightParallel* object = new CLightParallel;
        CLight* light = object;
        light->mFlags = 2;
        light->SetNormal(0.0f, 0.0f, 1.0f);
        light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        StoreLightColour(light, colour);
        const float z = direction->z;
        const float y = direction->y;
        const float x = direction->x;
        gei_System.UpdateLights();
        light->mNormal.x = x;
        light->mNormal.y = y;
        light->mNormal.z = z;
        gei_MapWho->Link(light, 0x70);
        record->mDirectional = object;
    } else if (type < 2) {
        if (type == 0) {
            DataErrorHandler(gInvalidLightText);
            return nullptr;
        }
        if (type >= 0) {
            GlobalDiffuse[0] = 1.0f;
            const float red = static_cast<float>(static_cast<double>(colour >> 24));
            const float green = static_cast<float>(static_cast<double>((colour >> 16) & 0xff));
            const float blue = static_cast<float>(static_cast<double>((colour >> 8) & 0xff));
            GlobalDiffuse[1] = red / 255.0f;
            GlobalDiffuse[2] = green / 255.0f;
            GlobalDiffuse[3] = blue / 255.0f;
        }
    } else if (type < 4) {
        CLightPoint* object = new CLightPoint;
        CLight* light = object;
        light->mFlags = 4;
        light->SetRadius(100.0f);
        light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        StoreLightColour(light, colour);
        const float z = position->z;
        const float y = position->y;
        const float x = position->x;
        gei_System.UpdateLights();
        light->mPosition.x = x;
        light->mPosition.y = y;
        light->mPosition.z = z;
        gei_MapWho->Link(light, 0x70);
        record->mPoint = object;
    }
    record->mColour = colour;
    record->mType = type;
    return record;
}

// 0x100641f8 (92 bytes): the square is stored in single precision and the inverse is made from the stored square.
void CLight::SetRadius(float radius) {
    gei_System.UpdateLights();
    mRadius = radius;
    mRadiusSquared = radius * radius;
    mInverseRadiusSquared = 1.0f / mRadiusSquared;
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

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

namespace {

// What the Mac's DataErrorHandler titles its box (0x1023ebf6).
const char kDataErrorCaption[] = "M3D2 Exporter - Data Error";

}  // namespace

// 0x1006446c (280 bytes). Type 2 takes out the directional light and then runs the point light's part too (the record's
// pointer is NULL, so it unlinks NULL and deletes nothing); 3 only the point light; 1 sets GlobalDiffuse to (1, 0, 0, 0) first and
// then does what 2 does (both pointers are NULL: two unlinks of NULL); 0 reports and leaves; a negative type or 4 and above
// does nothing. A light is unlinked (its link base, NULL for none) from the map's list 0x70, then deleted through its virtual
// deleting destructor (flag 1).
void DestroyLevelLights() {
    sGlobalLight* record = gGlobalLight;
    const int32_t type = record->mType;
    bool directional = false;
    if (type == 2) {
        directional = true;
    } else if (type > 1) {
        if (type > 3) return;
    } else {
        if (type == 0) {
            DataErrorHandler(gInvalidLightText);
            return;
        }
        if (type < 0) return;
        GlobalDiffuse[0] = 1.0f;
        const float zero = 0.0f;
        GlobalDiffuse[1] = zero;
        GlobalDiffuse[2] = zero;
        GlobalDiffuse[3] = zero;
        directional = true;
    }
    if (directional) {
        CMapLinkBase* link = nullptr;
        if (record->mDirectional != nullptr) link = record->mDirectional;
        gei_MapWho->Unlink(link, 0x70);
        if (record->mDirectional != nullptr) delete record->mDirectional;
    }
    CMapLinkBase* link = nullptr;
    if (record->mPoint != nullptr) link = record->mPoint;
    gei_MapWho->Unlink(link, 0x70);
    if (record->mPoint != nullptr) delete record->mPoint;
}

bool DataErrorHandler(char* text);

// TPRide.cpp range of the main app (SimTheme Park 2.04b3), unit w133: TickLevelLights (0x100645b0, 532 bytes), the park's per-frame light
// update (the sun's direction and colour, and the engine's ambient light).

float gGlobalLightScale = 1.0f;   // 0x1029cf78

// With `force` set: the ambient light (GlobalDiffuse: alpha 1, then red, green and blue from the ambient colour word, each byte over 255)
// and the sun. The sun's direction is normalised in place as in SetupLevelLights, but with the scale in the numerator (gGlobalLightScale
// over the length, then each component times that), and handed to the directional light's normal; then the light's colour word and its
// four colour floats (alpha in the low byte, red in the top byte, green, blue) are made from gGlobalLightColour. Every light write is
// preceded by gei_System.UpdateLights(). Without `force` nothing happens.
void TickLevelLights(int32_t force) {
    if (force == 0) return;
    GlobalDiffuse[0] = 1.0f;
    const uint32_t ambient = gGlobalAmbientColour;
    GlobalDiffuse[1] = static_cast<float>(ambient >> 24) / 255.0f;
    GlobalDiffuse[2] = static_cast<float>((ambient >> 16) & 0xff) / 255.0f;
    GlobalDiffuse[3] = static_cast<float>((ambient >> 8) & 0xff) / 255.0f;

    float* direction = &gGlobalLightDirection.x;
    const double x = direction[0];
    const float yy = direction[1] * direction[1];
    const float zz = direction[2] * direction[2];
    const double sum = static_cast<double>(zz) + (x * x + static_cast<double>(yy));
    const float length = static_cast<float>(std::sqrt(sum));
    const float scale = gGlobalLightScale / length;
    direction[0] = direction[0] * scale;
    direction[1] = direction[1] * scale;
    direction[2] = direction[2] * scale;
    const float nx = direction[0];
    const float ny = direction[1];
    const float nz = direction[2];
    CLight* light = gGlobalLight->mDirectional;
    gei_System.UpdateLights();
    light->mNormal.x = nx;
    light->mNormal.y = ny;
    light->mNormal.z = nz;

    const uint32_t colour = gGlobalLightColour;
    light = gGlobalLight->mDirectional;
    gei_System.UpdateLights();
    light->mColorWord = colour;
    light->mColor[0] = (static_cast<float>(colour & 0xff) / 255.0f) * 1.0f;
    light->mColor[1] = (static_cast<float>(colour >> 24) / 255.0f) * 1.0f;
    light->mColor[2] = (static_cast<float>((colour >> 16) & 0xff) / 255.0f) * 1.0f;
    light->mColor[3] = (static_cast<float>((colour >> 8) & 0xff) / 255.0f) * 1.0f;
}

// 0x100647ec: the light handle of the index-th embedded record of the ride's mesh
sNodeLightParams* GetLightsourceHandle(uint32_t ride, uint32_t index) {
    sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(gRideAnimSets[ride]->mHeads->mF28);
    uintptr_t record = reinterpret_cast<uintptr_t>(embedded->mRecords) + index * sizeof(sEmbeddedRecord);
    if (record == 0) return nullptr;
    return reinterpret_cast<sNodeLightParams*>(static_cast<uintptr_t>(reinterpret_cast<sEmbeddedRecord*>(record)->mF08));
}

// Light-node helpers (unit v5272).

namespace {

// The shared tail: alpha 1.0, the three colours, then the colour word from the stored floats.
void StoreColour(CLight* light, float red, float green, float blue) {
    gei_System.UpdateLights();
    light->mColor[0] = 1.0f;
    light->mColor[1] = red;
    light->mColor[2] = green;
    light->mColor[3] = blue;
    const uint32_t r = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[1])));
    const uint32_t g = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[2])));
    const uint32_t b = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[3])));
    const uint32_t a = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[0])));
    light->mColorWord = (r << 24) | ((g & 0xffu) << 16) | ((b & 0xffu) << 8) | (a & 0xffu);
}

}  // namespace

// 0x10064854 (248 bytes): the colour is read before UpdateLights.
void SetLightColourRelative(sNodeLightParams* params, float scale) {
    const float* colour = params->mColour;
    CLight* light = *params->mLight;
    const float b = colour[2] * scale;
    const float g = colour[1] * scale;
    const float r = colour[0] * scale;
    StoreColour(light, r, g, b);
}

// 0x1006498c (232 bytes)
void SetLightColourExplicit(sNodeLightParams* params, float red, float green, float blue) {
    StoreColour(*params->mLight, red, green, blue);
}

// 0x10064ab8 (68 bytes)
void EnableLight(sNodeLightParams* params) {
    CLight* light = *params->mLight;
    gei_System.UpdateLights();
    light->mFlags &= 0xefffffffu;
}

// 0x10064b30 (68 bytes)
void DisableLight(sNodeLightParams* params) {
    CLight* light = *params->mLight;
    gei_System.UpdateLights();
    light->mFlags |= 0x10000000u;
}
