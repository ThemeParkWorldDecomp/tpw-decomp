// The script system's start and the dynamic cell meshes' set-up and teardown (main application, analyser.cpp range; unit w111).
#include <cstring>
#include <new>

#include "DynamicMesh.h"
#include "LbFile.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "analyser.h"
#include "platform/msl.h"

uint32_t gScriptInitialised;
uint32_t gScriptF7C2C;
int32_t gScriptTableSize;
void* gScriptTable;
int32_t gDynamicCellF9BD4;
uint32_t gDynamicCellsReady;
uint32_t gDynamicCellRandom[110];
int32_t gDynamicCellF9D94;
float gDynamicCellF9D98;
float gDynamicCellF9D9C;
float gDynamicCellF9DA0;
int32_t gDynamicCellFBDBC;
uint8_t gDynamicCellFlag = 1;     // initialised data
CDynamicMesh* gBlueprintMesh;
CDynamicMesh* gWaterMesh;
CDynamicMesh* gFlagsMesh;

// 0x100d8d94 (140 bytes): only the first call does anything.
void SCR_InitScriptSystem() {
    if (gScriptInitialised == 0) {
        gScriptInitialised = 1;
        gScriptF7C2C = 0x32;
        gScriptTableSize = 100;
        gScriptTable = LbMem_Alloc(400);
        std::memset(gScriptTable, 0, static_cast<uint32_t>(gScriptTableSize) << 2);
        SCR_InitCheckTable();
    }
}

namespace {

// The box every dynamic mesh is made in: 0 to 1024 across, from -10 to -200 in depth (the constants at 0x10265088 to 0x1026507c).
CDynamicMesh* MakeMesh(uint16_t sizeA, const char* name, DYN_Type type, uint32_t* ok) {
    return new (std::nothrow) CDynamicMesh(sizeA, 0x14, ok, name, type, 0.0f, -10.0f, -200.0f, 1024.0f, 1024.0f, 1024.0f);
}

}  // namespace

// 0x100df8fc (572 bytes). The constructor's report (the first word of a four word buffer on the stack) decides whether the
// textures are given back: the first mesh's failure frees them, a later mesh's only if no mesh failed before. The buffer is not
// cleared between the meshes, so a mesh whose memory could not be had (the constructor does not run) is judged by the report
// of the one before it; for the first mesh the Mac reads an unset stack word (the host reads 0).
void DC_SetupDynamicCellMeshes() {
    gDynamicCellF9BD4 = 0;
    if (DMLoadTextures(&gDynamicCellTextures) != 0) {
        gDynamicCellFBDBC = 0;
        gDynamicCellF9DA0 = 0.0f;
        gDynamicCellF9D9C = 0.0f;
        gDynamicCellF9D98 = 0.0f;
        gDynamicCellFlag = 1;
        gDynamicCellF9D94 = 0;
        bool noFailure = true;
        for (int32_t i = 0; i < 0x6e; ++i) gDynamicCellRandom[i] = static_cast<uint32_t>(msl_rand() % 3 + 0xf);
        uint32_t ok[4] = {};
        gBlueprintMesh = MakeMesh(0x600, "BlueprintMesh", kDynBlueprint, ok);
        if (ok[0] == 0) {
            noFailure = false;
            DMFreeTextures(&gDynamicCellTextures);
        }
        gWaterMesh = MakeMesh(0x86, "WaterMesh", kDynWater, ok);
        if (noFailure && ok[0] == 0) {
            noFailure = false;
            DMFreeTextures(&gDynamicCellTextures);
        }
        gFlagsMesh = MakeMesh(0x180, "FlagsMesh", kDynFlags, ok);
        if (noFailure && ok[0] == 0) DMFreeTextures(&gDynamicCellTextures);
        gDynamicCellsReady = 1;
    }
}

// 0x100e01ac (132 bytes): the three destructors are called with the deleting flag (a NULL mesh does nothing).
void DC_FreeDynamicCellMeshes() {
    if (gDynamicCellsReady != 0) {
        delete gBlueprintMesh;
        delete gWaterMesh;
        delete gFlagsMesh;
        DMFreeTextures(&gDynamicCellTextures);
    }
    gDynamicCellsReady = 0;
}
