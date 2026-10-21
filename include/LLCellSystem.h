// The cell system (LLCellSystem.cpp of the main app, unit w111, src/LLCellSystem.cpp): its start and end. The globals are named
// after their addresses (the real names are unknown); the first column of each comment is the Mac's width.
#pragma once
#include <cstdint>

#include "CellBase.h"
#include "ObjectParts.h"

// 0x10090b68 (332 bytes): clears the system's words (listing order), stores the map's size, resets the map and the illegal
// placement, loads the four mesh sets, resets the click stack and the buy land list and draws the initial hoarding.
void LLCC_SystemInit(int32_t width, int32_t height);
// 0x10090cdc (60 bytes): destroys the four mesh sets.
void LLCC_SystemDeInit();
// The callees, all untranslated stand-ins:
void LLCC_ResetMap(int32_t width, int32_t height);   // 0x100916a8 (204 bytes)
void LLCM_SetIllegalPlacement(int32_t placement);    // 0x10088934 (12 bytes)
void LLCM_LoadQMeshes();                             // 0x10081a98 (264 bytes)
void LLCM_LoadKTrackMeshes();                        // 0x10081dd0 (284 bytes)
void LLCM_LoadWTrackMeshes();                        // 0x10082174 (284 bytes)
void LLCM_LoadHoardingMeshes();                      // 0x1008255c (252 bytes)
void LLCC_SystemResetClickStack();                   // 0x10091dd4 (16 bytes)
void LLCC_ResetBuyLandList();                        // 0x10096190 (4 bytes)
void LLCC_DrawInitialHoarding();                     // 0x10095420 (600 bytes)
void LLCM_DestroyQMeshes();                          // 0x10081bc8 (100 bytes)
void LLCM_DestroyKTrackMeshes();                     // 0x10081f18 (112 bytes)
void LLCM_DestroyWTrackMeshes();                     // 0x100822bc (100 bytes)
void LLCM_DestroyHoardingMeshes();                   // 0x10082688 (100 bytes)

extern uint8_t gLLCC4818;     // 0x102e4818  byte, cleared
extern uint8_t gLLCC47C4;     // 0x102e47c4  byte, cleared
extern int32_t gLLCCWidth;    // 0x102e47fc  LLCC_SystemInit's first argument
extern int32_t gLLCCHeight;   // 0x102e47f8  its second
extern int32_t gLLCC47CC;     // 0x102e47cc  cleared
extern int32_t gLLCC4814;     // 0x102e4814  cleared
extern int32_t gLLCC4810;     // 0x102e4810  cleared
extern int32_t gLLCC480C;     // 0x102e480c  cleared
extern int32_t gLLCC4808;     // 0x102e4808  cleared
extern int32_t gLLCC4804;     // 0x102e4804  -1
extern int32_t gLLCC4800;     // 0x102e4800  -1
extern uint16_t gLLCCDE0E2;   // 0x102de0e2  half word, cleared
extern int32_t gLLCCC214;     // 0x102dc214  cleared (after LLCC_ResetMap and LLCM_SetIllegalPlacement)
extern int32_t gLLCCC210;     // 0x102dc210  cleared
extern int32_t gLLCCC208;     // 0x102dc208  cleared
extern int32_t gLLCCC204;     // 0x102dc204  cleared
extern int32_t gLLCCC200;     // 0x102dc200  cleared
extern int32_t gLLCC47EC;     // 0x102e47ec  1
extern int32_t gLLCC47E8;     // 0x102e47e8  cleared
extern int32_t gLLCC47E4;     // 0x102e47e4  cleared
extern int32_t gLLCC47E0;     // 0x102e47e0  cleared
extern int32_t gLLCCDB6C;     // 0x102ddb6c  cleared (after the mesh sets are loaded)
extern uint16_t gLLCCDB68;    // 0x102ddb68  half word, cleared
extern int32_t gLLCC47C8;     // 0x102e47c8  cleared last

// ---- The cell costs (unit w117, src/LLCellSystem_costs.cpp): each setter stores its argument in the word and answers nothing; the
// game sets them from the level's balance (CGame::InitLevel_Initialise). The words are named after their addresses.
void LLCC_SystemSetPathCost(int32_t cost);        // 0x10091b24 (12 bytes)
void LLCC_SystemSetQueueCost(int32_t cost);       // 0x10091b9c
void LLCC_SystemSetSessionCost(int32_t cost);     // 0x10091d14 (12 bytes, src/LLCellSystem_modes.cpp, unit w185; CashWindowCallback calls it with 0 every frame)
void LLCC_SystemAddToSessionCost(int32_t cost);   // 0x10091d50 (20 bytes, src/LLCellSystem_modes.cpp, unit w185)
void LLCC_SystemSetKartTrackCost(int32_t cost);   // 0x10091c14
void LLCC_SystemSetWaterTrackCost(int32_t cost);  // 0x10091c94
extern int32_t gLLCC47DC;   // 0x102e47dc  the path cost
extern int32_t gLLCC47D8;   // 0x102e47d8  the queue cost
extern int32_t gLLCC47D4;   // 0x102e47d4  the kart track cost
extern int32_t gLLCC47D0;   // 0x102e47d0  the water track cost

// ---- LLCellMouse.cpp and the click stack (unit w112a). All names below are invented. ----
// A mesh table entry (12 bytes on the Mac, initialised data of the image; 16 here with the native pointer). The four tables are
// contiguous on the Mac (Q, go-kart, water, hoarding), which is why a NULL name at index 0 of a table copies the previous table's
// last type: the loaders reproduce that.
struct sLLCMMesh {
    uint32_t mRideType;         // 0x00  what LoadRideType answered (0 in the image)
    const char* mName;          // 0x04  NULL, "-" or a mesh file name
    uint8_t mTimeControls;      // 0x08  InstanceRide's `a` (read by LLCCM_Create*Mesh)
    uint8_t mQueueFlag;         // 0x09  the Q loader's 0x100000 flag
    uint8_t mF0A;               // 0x0a  unread
    uint8_t mF0B;               // 0x0b  unread
};
extern sLLCMMesh gLLCMQMeshes[8];          // 0x1029db48  queue meshes
extern sLLCMMesh gLLCMKTrackMeshes[20];    // 0x1029dba8  go-kart track meshes
extern sLLCMMesh gLLCMWTrackMeshes[25];    // 0x1029dc98  water ride track meshes
extern sLLCMMesh gLLCMHoardingMeshes[28];  // 0x1029ddc4  hoarding meshes
extern int32_t gLLCMQSize[2];              // 0x102dcea8  {1, 1} after LLCM_LoadQMeshes: LoadRideType's width and depth in cells
extern int32_t gLLCMTrackSize[2];          // 0x102dc220  {2, 2} after the other three loaders
extern int32_t gLLCMIllegalPlacement;      // 0x102dc218  LLCM_SetIllegalPlacement's word (LLCM_GetIllegalPlacement 0x10088970 reads it)
extern int32_t gLLCCClickStackCount;       // 0x102e47c0  the click stack's entry count (LLCC_SystemAddToClickStack's)
// 0x100914f4 (12 bytes, src/DynamicMesh.cpp; LLCellSystem.cpp on the Mac): the cell system's mode (gLLCC480C).
int32_t LLCC_SystemGetMode();

// The four instancers (src/LLCellMeshes.cpp, unit w120; LLCellMouse.cpp on the Mac). The decompiled text prints the wrong parameters and a
// void answer for three of them: the tile is a CELL_IndexType passed by value (r5 to r7), `restore` is r8 (0 from SetHFieldTile, 1 from
// RefreshHFieldTile), and all four answer the instance number (InstanceRide's or RestoreRideInstance's answer), which the callers store in
// the cell's mMeshInstance. The ride is placed from an sFixedRideParams whose mF0C is the address of the size pair (gLLCMQSize or
// gLLCMTrackSize). Q has no cell shift; the others shift (x, y) by the rotation (180: both, 90: y, 270: x, after rot = 360 - degrees).
uint32_t LLCCM_CreateQMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore);          // 0x10081c58 (224 bytes)
uint32_t LLCCM_CreateKTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore);    // 0x10081fb8 (284 bytes)
// W also starts the ride's animation 12 (SetCurRideAnim) when the instance number is not 0.
uint32_t LLCCM_CreateWTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore);    // 0x10082350 (364 bytes)
uint32_t LLCCM_CreateHoardingMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore);  // 0x10082720 (284 bytes)

// Unit w133 (LLCC_DrawInitialHoarding, src/LLCellSystem_hoarding.cpp). The park's hoarding extents (initialised data, all -1 in the image;
// names invented): left, top, right, bottom in cells.
extern int32_t gHoardingLeft;     // 0x1029e044
extern int32_t gHoardingTop;      // 0x1029e048
extern int32_t gHoardingRight;    // 0x1029e03c
extern int32_t gHoardingBottom;   // 0x1029e040
extern uint8_t gLLCCE0CC;         // 0x102de0cc  LLCC_DrawInitialHoarding sets it to 4 (initialised data, name invented)
extern const uint16_t gLLCRNoCell;   // 0x1029d430  the halfword LLCR_OverRide answers when a column has no cell
// Untranslated callees of LLCC_DrawInitialHoarding (stand-ins in the launcher).
class CHoardingsBalance;
void LLCC_SystemSetMode(int32_t mode, bool flag);                                        // 0x10090e18
void LLCM_SetCurrentObject(bool flag);                                                   // 0x10088590
class CObject;
CObject* LLCM_GetCurrentObject();   // 0x100888fc (12 bytes, untranslated; unit w153)
int32_t LLCC_BuildHoardingDB(int32_t right, int32_t bottom, int32_t left, int32_t top, char mode, bool flag);   // 0x1008e6e0 (untranslated; unit w201 keeps its answer)
void RecalculateParkDarkening();                                                         // 0x1005c59c
void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y);                                 // 0x10089210 (unit w142; the Mac answers through a hidden halfword result, y in r5)
void LLCR_OverRide(uint16_t* out, int32_t* x, int32_t* y);                               // 0x1008905c (stand-in; unit v5033 calls it with the addresses of its x and y)
void LLCC_SetRideRotation(int32_t rotation, bool flag);                                 // 0x10090d40 (unit w143; src/LLCellSystem_modes.cpp, unit w185)
void LLCR_GetHoardingExtents(int32_t* a, int32_t* b, int32_t* c, int32_t* d);            // 0x1008f9c8

// Unit w201 (src/LLCellSystem_hoardimage.cpp): the park's hoarding walked, locked, redrawn and checked. The image words are initialised data of the
// Mac (all names invented): a 0x43 by 0x43 byte grid of half cells, indexed x * 0x43 + y, the NEW image directly followed by the CURRENT one (one array here, as the Mac keeps them back to back).
void LLCC_SystemConstructHoardingClickStack();                                           // 0x100936f4 (512 bytes): the click stack along the hoarding from (right, bottom)
void LLCC_LockHoarding(bool lock);                                                       // 0x10093934 (432 bytes): sets (lock) or clears the lock flag 0x20 along the hoarding
void LLCC_DrawDefaultHoardings();                                                        // 0x100956a8 (204 bytes)
void LLCC_DrawHoardingsFromBalanceFile(CHoardingsBalance* balance);                      // 0x100957a4 (156 bytes; the Mac leaves 1 in r3, unread)
void LLCC_ReviseHoarding(int32_t x1, int32_t y1, int32_t x2, int32_t y2);                // 0x100961c0 (424 bytes)
void LLCC_FloodFill(int32_t x, int32_t y, uint8_t oldValue, uint8_t fill);               // 0x10096398 (712 bytes; oldValue is never read)
void LLCC_CreateCurrentHoardingImage();                                                  // 0x1009668c (372 bytes)
void LLCC_CopyCurrentToNewImage();                                                       // 0x10096838 (108 bytes)
void LLCC_AddNewAreaToImage(int32_t x1, int32_t y1, int32_t x2, int32_t y2);             // 0x100968d8 (80 bytes)
int32_t LLCC_SoIsItLegal();                                                              // 0x10096958 (140 bytes)
int32_t LLCC_IsClearLandLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2);           // 0x10096a0c (8 bytes): always 1
bool LLCC_IsPatrolAreaLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2);             // 0x10096a44 (340 bytes): whether some cell of the area is neither hoarding nor flagged 0x40
extern int32_t gLLCCReviseAnswer;                  // 0x1029df30  LLCC_ReviseHoarding stores LLCC_BuildHoardingDB's answer (initialised data: 1; name invented)
extern uint8_t gLLCCHoardingImages[2 * 0x43 * 0x43];   // 0x102de198  the new image (0x1189 bytes), directly followed by the current image (0x102df321)

// Unit w206 (src/LLCellSystem_hoardedit.cpp): the park's inside filled from the locked hoarding, and a hoarding edit checked for legality and cost.
void LLCC_FillInsideOfPark();                                                            // 0x10093b0c (2,836 bytes)
int32_t LLCC_IsHoardingEditLegal(int32_t x1, int32_t y1, int32_t x2, int32_t y2, bool charge);   // 0x1009464c (3,488 bytes; answers 0 or 1 as a word)
// The edit check's edge tally (names invented). Index 0 is the column at the larger x, 1 the row at the larger y, 2 the column at the smaller x,
// 3 the row at the smaller y. The counts are words at 0x102e05a0, 0x102e05a4, 0x102e05a8, 0x102e05ac; the marks are 0x80 bytes each, at
// 0x102e05b0, 0x102e0630, 0x102e06b0, 0x102e0730 (the click stack copy starts at 0x102e07b8).
extern int32_t gLLCCEdgeCount[4];       // 0x102e05a0
extern uint8_t gLLCCEdgeMarks[4][0x80]; // 0x102e05b0

// Unit w208 (src/LLCellSystem_hoardwrite.cpp): the park's hoarding written to the theme's hoardings balance file, and the static objects of
// LLCellSystem.cpp (__sinit_LLCellSystem_cpp, 0x10096f04, 764 bytes: the source defines them in the order the Mac constructs them). All names
// are invented. Each writer opens the theme's file for the name gTPGlobals holds, writes two rows per click of the copied click stack (the
// click minus the hoarding's right or bottom edge), then the closing pair of rows (0x200 each), and closes the file.
template<class Char> class TbDynamicStringTemplate;
namespace bfl { namespace FileSystem { class CDiscFile; } }
// One row: "<prefix><index><key>\t<value>\n", or only the line end when the index is -1. Answers 1 as a word on every path (the callers test for 0).
int32_t LLCC_WriteHoardingsLine(const TbDynamicStringTemplate<char>& key, int32_t index, int32_t value);   // 0x1009588c (580 bytes)
int32_t LLCC_WriteHoardingsToBalanceFile();           // 0x10095b20 (764 bytes; 0 or 1 as a word; the name is gTPGlobals.mF40EC)
int32_t LLCC_WriteCompleteHoardingsToBalanceFile();   // 0x10095e54 (764 bytes; 0 or 1 as a word; the name is gTPGlobals.mF4100)
extern bfl::FileSystem::CDiscFile gLLCCHoardingsFile;           // 0x102e058c  the file both writers open
extern TbDynamicStringTemplate<char> gLLCCHoardingFileHeader;   // 0x102e056c  "#ThemeParkWorld - Hoardings.sam" (neither writer writes it)
extern TbDynamicStringTemplate<char> gLLCCHoardingFileNote;     // 0x102e054c  "#DO NOT EDIT BY HAND" (nor this one)
extern TbDynamicStringTemplate<char> gLLCCHoardingKeyPrefix;    // 0x102e052c  "HoardingClicks["
extern TbDynamicStringTemplate<char> gLLCCHoardingKeyX;         // 0x102e050c  "].X"
extern TbDynamicStringTemplate<char> gLLCCHoardingKeyY;         // 0x102e04ec  "].Y"
extern TbDynamicStringTemplate<char> gLLCCHoardingEOL;          // 0x102e04cc  "\n"
extern TbDynamicStringTemplate<char> gLLCCHoardingSep;          // 0x102e04ac  "\t"
// The nine compass offsets (the cell itself, then the eight neighbours; x then y) the Mac stores 8 bytes apart from 0x102de0e4, each registered with
// the destructor of the MapDelta statics (0x102663d0). Only LLCC_SystemGetPointedPeepThing (0x10096bc8, untranslated) reads them. The element type
// is a guess: MapDelta is 12 bytes here.
extern const MapDelta gLLCCPointedOffsets[9];   // 0x102de0e4
extern int32_t gLLCC481C;   // 0x102e481c  0x10000 after __sinit_LLCellSystem_cpp (no reader found; 1.0 in 16.16?)
extern int32_t gLLCC4820;   // 0x102e4820  0 after it (no reader found)
// Unit v5689 (src/LLCellSystem_hoardimage_RemoveBridge.cpp).
class CTrackCell;
extern uint16_t gLLCCThingListEnd;   // 0x1029e20c (name invented): the id that ends a cell's thing list in LLCC_SystemGetPointedPeepThing (0 at start)
void RemoveBridge(CTrackCell* cell, MapDelta a, MapDelta b);   // 0x1009722c (712 bytes): the two MapDeltas are passed by reference (8 bytes copied)
// 0x1023b6e0 (bullfrog glue): the decimal digits of `value` into `buffer`, answers the buffer.
namespace NS_MacDoze { char* _itoa(int32_t value, char* buffer, int32_t radix); }

// Unit w144 (untranslated; CMMPathDraw and the first person target): the cell system's mouse and ride calls. LLCC_SystemSetRide gets the address of
// the mode's id halfword on the Mac (r3 = this + 0x10), so the id is passed by reference.
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag);                          // 0x10083180
void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t flag);   // 0x10084a00
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag);     // 0x10084ca4
void LLCC_SystemSetRide(const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id, int32_t track);   // 0x100915bc
void LLCC_SystemSetRideQueue(int32_t queue);                                        // 0x10091994
void LLCC_SystemSetRideTrack(int32_t track);                                        // 0x10091a5c

// Unit w185 (src/LLCellSystem_modes.cpp; LLCellSystem.cpp on the Mac): the mode switches, the ride and cost words, the cursor, the click stack
// and the first block fix up. SetMode, SetRideRotation, SetRide, SetRideQueue, SetRideTrack, SetSessionCost and AddToSessionCost are declared above.
int32_t LLCC_GetRideRotation();                          // 0x10090de0 (12 bytes)
void LLCC_SystemSetModeNR(int32_t mode, bool flag);      // 0x100911f0 (716 bytes): SetMode's twin that does not reset the mode's words
int32_t LLCC_SystemGetLastMode();                        // 0x1009152c (12 bytes)
int32_t LLCC_SystemIsMode(int32_t mode);                 // 0x10091568 (32 bytes)
void LLCC_SystemSetCursor();                             // 0x1009179c (460 bytes)
int32_t LLCC_SystemGetRideQueue();                       // 0x10091a08 (36 bytes)
int32_t LLCC_SystemGetRideTrack();                       // 0x10091ad0 (36 bytes)
int32_t LLCC_SystemGetPathCost();                        // 0x10091b60 (12 bytes)
// Unit v5536 (src/LLCC_DrawKartBlock.cpp): 0x10089cfc (1308 bytes); the third argument is unused (r5).
int32_t LLCC_DrawKartBlock(int32_t x, int32_t y, int32_t unused, int32_t flagA, int32_t flagB, uint8_t direction);
int32_t LLCC_SystemGetQueueCost();                       // 0x10091bd8 (12 bytes)
// (LLCC_SystemGetKartTrackCost 0x10091c54 and LLCC_SystemGetWaterTrackCost 0x10091cd4 are declared in TrackCell.h, as uint32_t.)
int32_t LLCC_SystemGetSessionCost();                     // 0x10091d98 (12 bytes)
int32_t LLCC_SystemGetClickStackEntries();               // 0x10091e18 (12 bytes)
int32_t LLCC_SystemAddToClickStack(int32_t x, int32_t y);   // 0x10091e5c (64 bytes): 0 when the stack is over full, else 1
void LLCC_SystemMakeClickStackCopy();                    // 0x10091ed0 (88 bytes)
void LLCC_FixFirstBlock(bool flag);                      // 0x10091f5c (760 bytes)

// The click stack: 0x400 entries of two words (all names invented). The Mac keeps the copy, its two words and the stack in one run of memory:
// the copy ends at 0x102e27b8 (its index word), the stack at 0x102e47c0 (gLLCCClickStackCount, then gLLCC47C4's word).
struct LLCCClick {
    int32_t mX;
    int32_t mY;
};
static_assert(sizeof(LLCCClick) == 8, "a click is two words");
extern LLCCClick gLLCCClickStack[0x400];       // 0x102e27c0
extern LLCCClick gLLCCClickStackCopy[0x400];   // 0x102e07b8
extern int32_t gLLCCClickCopyIndex;            // 0x102e27b8  cleared by the copy
extern int32_t gLLCCClickCopyCount;            // 0x102e27bc  the copy's entry count

// New words of the unit (initialised data of the image; the real names are unknown, the names follow the addresses except where said).
extern int32_t gLLCC9F14;      // 0x1029df14  -1; mode 0 sets it to -1 again
extern uint16_t gLLCC9F24;     // 0x1029df24  0xffff
extern uint16_t gLLCC9F26;     // 0x1029df26  0xffff
extern int32_t gLLCCE288;      // 0x1029e288  0x80
extern int32_t gLLCCE28C;      // 0x1029e28c  0x80
extern uint16_t gLLCCDB58;     // 0x102ddb58  0
extern uint16_t gLLCCDB5A;     // 0x102ddb5a  0
extern int32_t gLLCCE0DC;      // 0x102de0dc  0; the selected ride's track
extern uint16_t gLLCCE0E0;     // 0x102de0e0  0; the selected ride's id
extern int32_t gLLCCFirstBlockX;   // 0x1029e02c  -1 (name invented): the first block's cell
extern int32_t gLLCCFirstBlockY;   // 0x1029e030  -1 (name invented)

// Untranslated callees of the unit (stand-ins in the launcher).
void AbortMovePylon();                                   // 0x10057df4 (356 bytes, untranslated)
void SetCoasterEdit(void* coaster, int32_t flag);        // 0x10058758 (124 bytes, untranslated)
int32_t LLCM_GetIllegalPlacement();                      // 0x10088970 (12 bytes, untranslated)
extern uint8_t gLLCCDB34;      // 0x102ddb34  set to 1 by every left button down (defined in src/LLCM_LeftMouseButtonDown.cpp)
extern uint8_t gLLCCE090;      // 0x102de090  nonzero while a brace drag is pending (defined in src/LLCM_LeftMouseButtonDown.cpp)

// Unit w204 (src/LLCellSystem_clickstacks.cpp, src/LLCellRide_offsets.cpp): backtracking a click, the queue walk and the two track walks that
// fill the click stack, and the track end offsets.
int32_t LLCC_SystemBacktrack(CObject* obj, bool fixBridges);   // 0x10092280 (912 bytes): 0 or 1
int32_t LLCC_SystemConstructQueueClickStack(CObject* obj);   // 0x10092644 (1980 bytes): 0, or the direction byte when the walk met a cell that is not a queue
void LLCC_SystemConstructTrackClickStack(CObject* obj);   // 0x10092e44 (1016 bytes)
void LLCC_SystemConstructReverseTrackClickStack(CObject* obj);   // 0x10093280 (1064 bytes)
uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation);   // 0x1008c620 (132 bytes): the direction bit, moves the cell
uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation);   // 0x1008c6d8 (168 bytes): the opposite of the direction bit
extern LLCCClick gLLCCBacktrackClick;   // 0x102e07b0  the click Backtrack pops (name invented; the eight bytes before the copy of the stack)
// Untranslated callees of the unit (stand-ins in the launcher).
void DeleteLastTrackBrace();   // 0x10056a80 (720 bytes)
uint8_t LLCR_FixBridges(CObject* ride, bool a, bool b, bool c);   // 0x1008fa34 (2020 bytes)
uint8_t LLCR_FixBridgesReverse(CObject* ride, bool a, bool b, bool c);   // 0x1009024c (2216 bytes; the answer is not read)

// Unit v5059 (src/LLCellMeshes_LLCM_RebuildUpgradeBridges.cpp).
void LLCM_CoverUpgrade(int32_t* x, int32_t* y);          // 0x100828e4 (888 bytes)
void LLCM_RebuildUpgradeBridges(int32_t x, int32_t y);   // 0x10082cf0 (1116 bytes)
void LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);   // 0x100a30f8 (912 bytes, stand-in)

// Unit v5082 (src/LLCC_Kill_Them_KILL_THEM_ALL.cpp): LLCC_BuildHoardingDB (declared above) and the rectangle clearer. The four words are defined by
// the unit (names invented): the extents of the hoarding walk, min x / min y / max x / max y.
int32_t LLCC_Kill_Them_KILL_THEM_ALL(int32_t x1, int32_t y1, int32_t x2, int32_t y2);   // 0x1008ebc0 (1,212 bytes): 1
void LLCR_DeleteRide(int32_t x, int32_t y, int32_t flag);   // 0x100892c4 (2,576 bytes, stand-in)
extern int32_t gLLCCHoardMinX;   // 0x102de0c8  0x400 at the start of the walk
extern int32_t gLLCCHoardMinY;   // 0x102de0c4  0x400
extern int32_t gLLCCHoardMaxX;   // 0x102de0c0  0
extern int32_t gLLCCHoardMaxY;   // 0x102de0bc  0

// Unit v5182 (src/LLCellMouse_LLCM_SetCurrentObject.cpp): the current object's track end and the track edit mode's start. Names are invented.
class CTrackCell;
void LLCM_StartTrackEditMode(int32_t x, int32_t y, uint16_t unused);   // 0x10088a64 (1,372 bytes; the third argument is never read)
void LLCC_TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag);   // 0x1008ddfc: calls LLCC_TraceTrackCount
int32_t LLCC_TraceTrackCount(CTrackCell* from, CTrackCell* to, int32_t flag);   // 0x1008ddfc body: the number of cells walked, -1 when a cell is no track (2,212 bytes, src/LLCC_TraceTrack.cpp): the number of cells walked, -1 when a cell is no track
extern int32_t gLLCC9F18;        // 0x1029df18  -1 (initialised data): the current object's cell x
extern int32_t gLLCC9F1C;        // 0x1029df1c  -1: its cell y
extern int32_t gLLCC9F20;        // 0x1029df20  -1: its rotation
extern uint16_t gLLCCE028;       // 0x1029e028  0: the upgrade parent LLCM_SetCurrentObject compares with
extern uint16_t gLLCCE02A;       // 0x1029e02a  0xffff: the cell id LLCM_StartTrackEditMode compares gLLCCDB68 with
extern int32_t gLLCCE034;        // 0x1029e034  -1: the track end x
extern int32_t gLLCCE038;        // 0x1029e038  -1: the track end y
extern CTrackCell* gLLCCDB48;    // 0x102ddb48  0 (BSS, a pointer): the current object's track cell
extern uint16_t gLLCCDB5C;       // 0x102ddb5c  0: gLLCCDB68's previous value
// The host's pointers are 64 bits but gLLCCDB6C (0x102ddb6c, the current object on the Mac) is a 32-bit word here: this unit keeps the whole
// pointer in this native word and the pointer's low 32 bits in gLLCCDB6C; the pointer counts only while the two agree (0 and a word cleared by
// another unit leave no current object). Host only, no Mac address.
extern CObject* gLLCMCurrentObject;

// Unit v5186 (src/LLCC_Kill_Them_KILL_THEM_ALL_LLCC_AbortPylonMove.cpp): LLCC_AbortPylonMove (declared in TPCoaster.h), LLCC_IsRideMovable and
// LLCC_GetRideBlendInfo (declared in TPEngine.h). The globals are defined by the unit (names invented).
int32_t LLCC_IsRideMovable(CObject* ride);   // 0x1008f330 (528 bytes): 1 unless the ride's track type is 1 or 2 and the cell before its start or after its end has other than one neighbour
extern int32_t gLLCCE0B4;    // 0x1029e0b4  the pylon's x, -1 for none (initially -1)
extern int32_t gLLCCE0B8;    // 0x1029e0b8  the pylon's y, -1 for none (initially -1)
extern uint32_t gLLCCE08C;   // 0x102de08c  the mesh instance word the track cell gets back
struct LLCCRideBlend {
    uint8_t mCells[0x100];   // 0x000  x * 16 + z
    int32_t mWidth;          // 0x100
    int32_t mHeight;         // 0x104
};
extern LLCCRideBlend gLLCCDF84;   // 0x102ddf84
// Unit v5691: LLCC_GetRideProfileInfo's table (0x102ddb7c): 16 x 16 words (x * 16 + z), then the shape's size at +0x400 and +0x404.
extern int32_t gLLCCDB7C[0x102];

// Unit v5633 (src/LLCellMeshes_LLCM_RebuildUpgradeBridges_LLCM_MouseMoved.cpp): LLCM_MouseMoved's untranslated callees and the words it
// defines (names invented from the Mac addresses).
void LLCM_TrackCellBoundary(int32_t x, int32_t y, int32_t* cellX, int32_t* cellY, int32_t* outX, int32_t* outY);   // 0x100889ac (128 bytes, stand-in)
// (Unit v5635 translated it, src/LLCC_DrawKartBlock_LLCR_PlaceRide.cpp: the answer is the placement block or NULL, see below.)
int32_t LLCR_PlaceRide(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c);    // 0x1008a248 (6,572 bytes; answers the block's address, low 32 bits, or 0)
struct LLCRPlacement* LLCR_PlaceRideBlock(int32_t x, int32_t y, int32_t a, int32_t rotation, int32_t b, int32_t c);   // the body of LLCR_PlaceRide, answers the block or NULL
extern uint16_t gLLCC9F34;    // 0x1029df34  0: the ride id the ride mode does not preview
extern int32_t gLLCC9F38;     // 0x1029df38  1: the hoarding's least x
extern int32_t gLLCC9F3C;     // 0x1029df3c  1: the hoarding's least y
extern uint16_t gLLCC9F40;    // 0x1029df40  0: the cell id mode 0x1a compares gLLCCDB68 with
extern int32_t gLLCCC20C;     // 0x102dc20c  cleared with gLLCCC200 to gLLCCC214
extern int32_t gLLCCDB38;     // 0x102ddb38  the ride preview's y
extern int32_t gLLCCDB3C;     // 0x102ddb3c  the ride preview's x
extern int32_t gLLCCDB60;     // 0x102ddb60  1 while the mouse cell is the drag start (gLLCC4804, gLLCC4800)
extern int32_t gLLCCDE0B4;    // 0x102de0b4  the ride under the mouse's info word (record +0x54)
extern int32_t gLLCCDE0B8;    // 0x102de0b8  the mode's info word (2, 3, or the ride record's +0x54)

// Unit v5635 (src/LLCC_DrawKartBlock_LLCR_PlaceRide.cpp): LLCR_PlaceRide (declared above) and its words (BSS; names invented).
// The block LLCR_PlaceRide answers (0x102de0a0, 0x14 bytes): the entrance cell, the footprint's top left cell, the entrance's direction.
struct LLCRPlacement {
    int32_t mEntranceX;            // 0x00  -1 until a build places the entrance
    int32_t mEntranceY;            // 0x04
    int32_t mTopLeftX;             // 0x08
    int32_t mTopLeftY;             // 0x0c
    uint8_t mEntranceDirection;    // 0x10
    uint8_t mF11[3];               // 0x11
};
static_assert(sizeof(LLCRPlacement) == 0x14, "the placement block is 0x14 bytes");
extern LLCRPlacement gLLCRPlacement;   // 0x102de0a0
extern int32_t gLLCRPlaceRotation;     // 0x102de0d0  the rotation of the last placement
extern int32_t gLLCRPlaceY;            // 0x102de0d4  its 5 by 5 square's y
extern int32_t gLLCRPlaceX;            // 0x102de0d8  its 5 by 5 square's x
// Untranslated callees (stand-ins in the launcher). The ride's cells: the kind (0 none; 1 path, 9 entrance, 10 exit, 11 kart and
// 16 water track, 23 the cell beside) and the direction byte of the footprint's (col, row).
class CObject;
void LLCR_SetRide(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);   // 0x1008bc20 (92 bytes)
void LLCR_GetWidthHeight(int32_t* width, int32_t* height);                 // 0x1008bdcc (32 bytes)
int32_t LLCR_GetRideCell(int32_t col, int32_t row);                         // 0x1008bccc (32 bytes)
uint8_t LLCR_GetRideCellDirection(int32_t col, int32_t row);                // 0x1008bd14 (32 bytes; the caller keeps the low byte)
void LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);  // 0x1008bd68 (20 bytes)
CObject* LLCR_AddRideToDB(int32_t rotation);                                // 0x1008be1c (740 bytes): the ride (+4 and +6 its cell bytes)

// Unit v5687 (src/TrackCell_flags_CTrackCell_SetHFieldTile_LLCC_BreakTrack.cpp).
// 0x100a510c (1,156 bytes): takes the direction bits (dir, else the cell's own direction; at the track end only the neighbour bits the direction
// lacks) off the cell, with the cell beyond; answers the track cell of the first block's id. 0x100a55bc (224 bytes): the mesh instance word
// of the track cell at (x, y), or of its parent for a track X cell (0 with a trace when the parent is gLLCCE380).
class CTrackCell;
CTrackCell* LLCC_BreakTrack(int32_t x, int32_t y, uint8_t dir);
uint32_t LLCC_GetTrackMeshAt(int32_t x, int32_t y);
extern uint16_t gLLCCE380;    // 0x1029e380  0 (initialised data, name invented): the cell id that stands for no parent

// Unit v5883 (src/LLCM_LeftMouseButtonDown_LLCM_LeftMouseButtonUp.cpp, which defines the words; names are the addresses).
// LLCM_LeftMouseButtonUp's own words, the undeclared callees it calls and PlacePylon's four-argument overload.
class CCellBase;
class CThing;
struct LLCR_PRR;   // LLCR_PlaceRide's answer (a block address, low 32 bits)
extern int32_t gLLCC9F28;      // 0x1029df28  -1 (initialised data): the queue start's x (LLCR_StartQueue's out parameter)
extern int32_t gLLCC9F2C;      // 0x1029df2c  -1: its y
extern int32_t gLLCC9F44;      // 0x1029df44  1 (initialised data): the smallest x of a hoarding edit
extern int32_t gLLCC9F48;      // 0x1029df48  1: the smallest y
extern int32_t gLLCCDB30;      // 0x102ddb30  0: the clicks of the path and queue modes since the first (the advisor speaks at 3)
extern int32_t gLLCCDB44;      // 0x102ddb44  0: cleared by the ride placement modes
extern CCellBase* gLLCCDB4C;   // 0x102ddb4c  0 (a pointer): the queue start cell LLCR_StartQueue answered
extern CThing* gLLCCDB64;      // 0x102ddb64  0 (a pointer): the selected peep (UIInterface::GetSelectedPeep)
int32_t LLCC_BuildTrackDB(bool flag);                                                    // 0x1008ce04 (4,048 bytes, stand-in): nonzero when the track is whole
CCellBase* LLCR_StartQueue(LLCR_PRR* placement, int32_t* x, int32_t* y);                 // 0x1008c128 (452 bytes, stand-in)
CTrackCell* LLCR_StartTrack(LLCR_PRR* placement, int32_t* x, int32_t* y, int32_t track); // 0x1008c320 (716 bytes, stand-in)
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, uint32_t* meshSlot);            // 0x10052a30 (108 bytes; the answer is not read)

// Unit v5932 (src/LLCR_OverRide_LLCR_OverRide_3.cpp, which defines the words; names are the addresses).
struct RideInfo;
extern uint16_t gLLCCE04C;     // 0x1029e04c  0 (halfword): the thing id LLCR_OverRide's cell answer is compared with
extern uint16_t gLLCCE092;     // 0x102de092  0 (halfword): the cell id of the ride being placed
extern int32_t gLLCCE094;      // 0x102de094  0: the selected ride record's word 0x18 (its shape; on the host the low half of the pointer)
// Host only: the selected ride's shape whole (balance slot Shape, a native pointer to a block outside the arena), set beside
// gLLCCE094 by LLCR_SetRide and read by LLCR_GetRideCell, LLCR_GetRideCellDirection and LLCR_GetWidthHeight.
extern const uint8_t* gLLCRRideShape;
extern RideInfo* gLLCCE098;    // 0x102de098  0 (a pointer): the selected ride record
extern uint16_t gLLCCE09C;     // 0x102de09c  0 (halfword; the Mac reads it as the high half of a word): the selected ride's id
// Unit v51063 (src/LLCR_OverRide_LLCR_OverRide_3_LLCR_GetRideCell.cpp): the static initialiser of LLCellMouse.cpp.
extern int32_t gLLCCDB74;      // 0x102ddb74  0x10000 after __sinit_LLCellMouse_cpp
extern int32_t gLLCCDB78;      // 0x102ddb78  0 after __sinit_LLCellMouse_cpp
void __sinit_LLCellMouse_cpp();   // 0x10088ff4 (60 bytes): also gLLCCDB68, gLLCCDB5C, gLLCCDB5A, gLLCCDB58 = 0
// 0x1008905c (396 bytes) is declared above; 0x100889ac and 0x1008be1c are in src/LLCR_OverRide_LLCR_OverRide_3.cpp.

// Unit v5982 (src/LLCellRide_offsets_LLCC_SetTrackAnimReverse_LLCC_BuildTrackDB_2.cpp defines the two words; names are the addresses).
struct tag_sRideAnimSet;
extern uint16_t gLLCCE0B0;     // 0x1029e0b0  0 (halfword): the cell answer LLCC_BuildTrackDB compares a straight section's ride with
extern uint16_t gLLCCE0B2;     // 0x1029e0b2  0xffff (halfword): the cell answer it compares a start-flag cell's ride with
// Callees of LLCC_BuildTrackDB (stand-ins in the launcher).
// 0x1002ef98 (4,080 bytes, unit v5566, declared in BMP_InsideArea.h too): answers 1 placed, 0 refused; LLCC_BuildTrackDB ignores it.
int32_t BMP_TrackSectionPlaceSection(int32_t ride, int32_t x, int32_t z, int32_t kind, int32_t count, uint32_t thing, int32_t info);

// Unit v5999 (src/LLCR_OverRide_LLCR_DeleteRide_2.cpp defines the word; name invented).
extern uint16_t gLLCRNoRide;   // 0x1029e04e  0 (halfword): the thing id LLCR_OverRide answers when no ride stands on the cell (LLCR_DeleteRide compares with it)
int32_t FindDataNodeIndexByDisableCode(tag_sRideAnimSet* set, uint32_t code, uint32_t id);   // 0x1020017c (104 bytes, src/advisor_leaves.cpp): -1 for none

// Unit v51060 (src/MapDelta_map_CMapCell_IsQueue.cpp): 0x10090b2c (16 bytes), the static initialiser of LLCellRide.cpp: gLLCCE092 = 0.
void __sinit_LLCellRide_cpp();
