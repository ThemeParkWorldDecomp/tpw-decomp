// Debug.cpp (main app): trace output and the debug overlays. PARTIAL; untranslated.
// GAME_ASSERTMSG is in the Advisor.cpp range of the inventory.
#pragma once
#include <cstdint>

// 0x1006fc5c (src/Debug.cpp): a number of its class, the class's base plus `index`.
enum eIDClass : int32_t { kIDClassLobbyMeshType = 18000 };   // the other classes are unknown
uint32_t GetUniqueID(eIDClass idClass, uint32_t index);
void CTRACE(uint32_t level, const char* format, ...);   // 0x1006fcbc
void TRACE(const char* format, ...);                    // 0x1006fcfc
// 0x1006fc90 (4 bytes): an empty function in the 2.04b3 build (a stand-in until translated); the save file reader calls it with
// a message and 0.
void DEBUG_MESSAGE(const char* message, uint32_t flags);
// 0x1006fd3c (src/RideInstance.cpp): the original is an empty function (it drops its message); the port formats the
// text and writes it to the log through SamsUtilities::UDebug. No dialog, no exit.
void ERRORBox(const char* format, ...);
// Reports `format` when `condition` is 0 (0x1000dde8).
void GAME_ASSERTMSG(int32_t condition, const char* format, ...);
void TickRideHighlights();   // 0x1006cff8
void TickFlashObjects();     // 0x1006d5fc
// The flashing objects (Debug.cpp range, unit w133's TickFlashObjects): the phase, 0 up to 2 (initialised data float 0x102d1efc), and the
// head of the list of ride animation sets whose points are flashed, linked through tag_sRideAnimSet::mFD0 (0x102d1f00). Names invented.
struct tag_sRideAnimSet;
extern float gFlashPhase;                    // 0x102d1efc
extern tag_sRideAnimSet* gFlashObjects;      // 0x102d1f00

// Unit v5221: DoOrSetupSkirtDeform (src/Debug_overlays_DoOrSetupSkirtDeform.cpp). The by-value sMatrix is 64 bytes (r6.. then the stack).
struct tag_sNode;
struct sHField;
struct sMatrix;
void DoOrSetupSkirtDeform(tag_sNode* a, tag_sNode* b, uint16_t** list, sMatrix matrix, sHField* hf, int32_t setup, int32_t* count,
                          float lift, tag_sNode** bestNode, int32_t* bestCount, uint32_t flags, tag_sNode* only, int32_t mode);   // 0x1006daa4
extern uint32_t gSkirtCellFlags[20][20];   // 0x102da2ec  rows of 0x50 bytes; names invented
extern uint32_t gSkirtRows;                // 0x102da92c
extern uint32_t gSkirtColumns;             // 0x102da930

// Unit v5915 (src/Debug_overlays_DecideRideShapeDeformEdges.cpp).
struct RideShape;
void DecideRideShapeDeformEdges(RideShape* shape, int32_t width, int32_t height);   // 0x1006d76c
