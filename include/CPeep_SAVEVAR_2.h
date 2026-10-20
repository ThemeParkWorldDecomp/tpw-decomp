#pragma once
// Unit SAVEVAR_2 (Peep.cpp, 0x101162d4 and 0x10116418): the ESP_Sprites save and the static initialiser of Peep.cpp.
#include <cstdint>

// 0x102a0f04: the zero word the compiler keeps for TbError comparisons (read once by SAVEVAR<ESP_Sprites>; nothing writes it).
extern int32_t gPeepTbErrorZero;
// 0x1034c498 (0x10000 after __sinit_Peep_cpp) and 0x1034c49c (0 after it): two words no reader is known for.
extern int32_t gPeepC498;
extern int32_t gPeepC49C;
// The registration records (12 bytes each) at 0x1034c3f4 .. 0x1034c454 (nine, the first object's at the highest address).
extern uint32_t gPeepSinitNodes[9][3];
// Stand-in for the destructor word 0x102663d0 handed to __register_global_object.
extern char gPeepMapDeltaDtorVector[8];

void __sinit_Peep_cpp();   // 0x10116418 (504 bytes)
