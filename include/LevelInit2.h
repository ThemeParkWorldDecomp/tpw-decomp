// The park's second initialisation and teardown (unit w110, src/game/TPLevel.cpp): the callees LevelInit2 and LevelDeInit reach
// that are not translated yet (main application, TPRide.cpp unless noted). Each is a stand-in in the launcher.
#pragma once
#include <cstdint>

bool LoadLevelAttributeMap();                       // 0x1005be44 (124 bytes)
void DC_SetupDynamicCellMeshes();                   // 0x100df8fc (572 bytes, analyser.cpp)
void SetupForPolySearch();                          // 0x1005c7e8 (60 bytes)
void DestroyHighlightColourBuffers();               // 0x1006d26c (88 bytes, Debug.cpp)
void CreateHighlightColourBuffers();                // 0x1006d1d0 (104 bytes, Debug.cpp)
void SetupHeightmapSystem();                        // 0x1005f6f4 (24 bytes)
void InitialiseCoasterTypes();                      // 0x10042478 (72 bytes, TPCoaster.cpp)
void LLCC_SystemInit(int32_t a, int32_t b);         // 0x10090b68 (332 bytes, LLCellSystem.cpp): LevelInit2 passes 0x80, 0x80
void SCR_InitScriptSystem();                        // 0x100d8d94 (140 bytes, analyser.cpp)
void DC_FreeDynamicCellMeshes();                    // 0x100e01ac (132 bytes, analyser.cpp)
void ClosedownRideHoardings();                      // 0x10061448 (40 bytes)
void DestroyCoasterTypes();                         // 0x1003acf0 (472 bytes, TPCoaster.cpp)
void DestroyLevelLights();                          // 0x1006446c (280 bytes)
void SetdownFromPolySearch();                       // 0x1005c850 (60 bytes)
void LLCC_SystemDeInit();                           // 0x10090cdc (60 bytes, LLCellSystem.cpp)
// 0x100b9834 (72 bytes, rsse.cpp): a message box with the text; the answer is not used by CreateGlobalLight.
bool DataErrorHandler(char* text);
