// The cell costs' setters (LLCellSystem.cpp range, main app, SimTheme Park 2.04b3; unit w117): four 12 byte functions, each storing its
// argument in a word of the cell system. No clamp, no answer. CGame::InitLevel_Initialise calls them with the level's balance.
#include "LLCellSystem.h"

int32_t gLLCC47DC = 0;   // 0x102e47dc  the path cost
int32_t gLLCC47D8 = 0;   // 0x102e47d8  the queue cost
int32_t gLLCC47D4 = 0;   // 0x102e47d4  the kart track cost
int32_t gLLCC47D0 = 0;   // 0x102e47d0  the water track cost

void LLCC_SystemSetPathCost(int32_t cost) { gLLCC47DC = cost; }          // 0x10091b24
void LLCC_SystemSetQueueCost(int32_t cost) { gLLCC47D8 = cost; }         // 0x10091b9c
void LLCC_SystemSetKartTrackCost(int32_t cost) { gLLCC47D4 = cost; }     // 0x10091c14
void LLCC_SystemSetWaterTrackCost(int32_t cost) { gLLCC47D0 = cost; }    // 0x10091c94
