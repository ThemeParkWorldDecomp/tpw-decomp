// TPEmbed.cpp (main app). PARTIAL: TickRideHeads (src/game/TPEmbed.cpp) is translated; the rest of the file is not.
#pragma once
#include <cstdint>

struct tag_sMesh;

// The float TickRideHeads gives its script variable 8 (0.685 the first time, a static of TPEmbed.cpp whose Mac
// name is unknown) and the flag that says it has been set (a signed byte: any nonzero value counts as set).
extern float gRideHeadsVariable8;       // 0x10396a9c
extern int8_t gRideHeadsVariable8Set;   // 0x10396a98

// 0x10200460 (1688 bytes, src/game/TPEmbed.cpp): for each embedded entry of the mesh's embedded set that has
// flag 2 (a head), turns the head to look at the camera: it works out which of 7 x 8 sampled directions comes
// closest to the direction from the head to the camera and gives the head's script object that choice
// (variable 13), the head's turn about its own axis (variable 9) and variable 8, and its position.
void TickRideHeads(tag_sMesh*& mesh);

// 0x1020039c (144 bytes, src/game/TPEmbed.cpp, unit w114): turns embedded entry `index` of the mesh's embedded set off: the set's
// word at +8 (the number of enabled heads, as far as is known) goes down by one and, at zero, the set's flag 4 is cleared; the entry's
// flag 2 is cleared, its script (word +0xc) killed with SCR_KillScript and the handle set to 0xffffffff. No bounds are checked. (The
// original also compares index + the header's halfword at 0x46 with the one at 0x44 and drops the result: a compiled-out assertion.)
void DisableRideHead(tag_sMesh* mesh, uint32_t index);
// Unit w149 (untranslated) 0x10200230 (312 bytes, TPEmbed.cpp): turns embedded entry `index` on for the peep with thing id `peep` (the counterpart of DisableRideHead).
void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep);
// Unit v5962: the script start block EnableRideHead hands to ESP_SpawnScript (0x1029fda8, initial word 0x10266a50; name invented, only the address is used).
extern uint32_t gRideHeadSpawnBlock;
