// The cell system's start and end (main application, LLCellSystem.cpp; unit w111). The stores keep the listing's order: the
// callees (the map reset, the mesh loaders, the hoarding) see the words set so far.
#include "LLCellSystem.h"

uint8_t gLLCC4818;
uint8_t gLLCC47C4;
int32_t gLLCCWidth;
int32_t gLLCCHeight;
int32_t gLLCC47CC;
int32_t gLLCC4814;
int32_t gLLCC4810;
int32_t gLLCC480C;
int32_t gLLCC4808;
int32_t gLLCC4804;
int32_t gLLCC4800;
uint16_t gLLCCDE0E2;
int32_t gLLCCC214;
int32_t gLLCCC210;
int32_t gLLCCC208;
int32_t gLLCCC204;
int32_t gLLCCC200;
int32_t gLLCC47EC;
int32_t gLLCC47E8;
int32_t gLLCC47E4;
int32_t gLLCC47E0;
int32_t gLLCCDB6C;
uint16_t gLLCCDB68;
int32_t gLLCC47C8;

// 0x10090b68 (332 bytes).
void LLCC_SystemInit(int32_t width, int32_t height) {
    gLLCC4818 = 0;
    gLLCC47C4 = 0;
    gLLCCWidth = width;
    gLLCCHeight = height;
    gLLCC47CC = 0;
    gLLCC4814 = 0;
    gLLCC4810 = 0;
    gLLCC480C = 0;
    gLLCC4808 = 0;
    gLLCC4804 = -1;
    gLLCC4800 = -1;
    gLLCCDE0E2 = 0;
    LLCC_ResetMap(width, height);
    LLCM_SetIllegalPlacement(0);
    gLLCCC214 = 0;
    gLLCCC210 = 0;
    gLLCCC208 = 0;
    gLLCCC204 = 0;
    gLLCCC200 = 0;
    gLLCC47EC = 1;
    gLLCC47E8 = 0;
    gLLCC47E4 = 0;
    gLLCC47E0 = 0;
    LLCM_LoadQMeshes();
    LLCM_LoadKTrackMeshes();
    LLCM_LoadWTrackMeshes();
    LLCM_LoadHoardingMeshes();
    LLCC_SystemResetClickStack();
    LLCC_ResetBuyLandList();
    gLLCCDB6C = 0;
    gLLCCDB68 = 0;
    LLCC_DrawInitialHoarding();
    gLLCC47C8 = 0;
}

// 0x10090cdc (60 bytes).
void LLCC_SystemDeInit() {
    LLCM_DestroyQMeshes();
    LLCM_DestroyKTrackMeshes();
    LLCM_DestroyWTrackMeshes();
    LLCM_DestroyHoardingMeshes();
}
