// GameVariables.cpp: the real definitions of game variables that no src file owns. They were zero-initialised stand-ins of the
// launcher (launcher/gen_stubs.py). Only libtpw compiles this file: the unit test libraries list their own sources and define
// these names themselves (fakes that record or fill them), so nothing here changes what a unit test compiles.
//
// Each comment gives the variable's address in the Mac program and its section: below 0x1025fcc0 a code constant, below 0x102b3a44
// initialised data (the value read from the program image is given), below 0x103bc8b0 BSS (zero), higher an import slot (a
// library's variable, defined here with the library's own value). Words the Mac holds as program addresses (vtables, TVectors) have
// no host referent: they are kept as the Mac has them, in host byte order, because only the identity of each table matters. The
// ride templates are game data and come from devtools/gen_game_tables.py at configure time (RideTemplates.inc).

#include <cstddef>
#include <cstdint>

#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "BlockSave.h"
#include "CMessageCentre.h"
#include "CellBase.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FinanceGraphPanelMsg.h"
#include "LLCellSystem.h"
#include "Light.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TbAudioSystem.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "WorldThings.h"
#include "email.h"
#include "intrface.h"
#include "online.h"
#include "rsse.h"
#include "tb_drawprimitive.h"
#include "tb_init.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

#include "RideTemplates.inc"   // TPW_RIDE_TEMPLATES, generated at configure time

// The four bytes of a 32-bit word in host (little-endian) order, for the tables the headers declare as bytes.
#define TPW_WORD_LE(T, w) \
    static_cast<T>((w) & 0xff), static_cast<T>(((w) >> 8) & 0xff), static_cast<T>(((w) >> 16) & 0xff), static_cast<T>(((w) >> 24) & 0xff)

// ---- Code constants (0x10000000 ... 0x1025fcc0)

// 0x1024eda3: the mesh the progress bar's gradient loads, "progress.wct".
char gProgBarMeshName[16] = "progress.wct";

// 0x1024e168: two wide format strings back to back, L" %ls" (5 units with its NUL) and, at +5 units, L" %d %%" (the number format).
char16_t gOptionsPanelNumberFormats[12] = u" %ls\0 %d %%";

// 0x1024edc2: the literals of ui_cityLobbyController.cpp. The Mac holds the wide strings big-endian; the host holds them as UTF-16
// little-endian at the same byte offsets: L"%d" at +0x00, L"upload" at +0x3d (an odd address) and L"gms.dat" at +0x4b. The narrow
// text at +0x06 is "!std::exception!!std::logic_error!!std::length_error!!".
alignas(2) const char gCityLobbyStringPool[0x60] = {
    0x25, 0x00, 0x64, 0x00, 0x00, 0x00, 0x21, 0x73, 0x74, 0x64, 0x3a, 0x3a, 0x65, 0x78, 0x63, 0x65,
    0x70, 0x74, 0x69, 0x6f, 0x6e, 0x21, 0x21, 0x73, 0x74, 0x64, 0x3a, 0x3a, 0x6c, 0x6f, 0x67, 0x69,
    0x63, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x21, 0x21, 0x73, 0x74, 0x64, 0x3a, 0x3a, 0x6c, 0x65,
    0x6e, 0x67, 0x74, 0x68, 0x5f, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x21, 0x21, 0x00, 0x75, 0x00, 0x70,
    0x00, 0x6c, 0x00, 0x6f, 0x00, 0x61, 0x00, 0x64, 0x00, 0x00, 0x00, 0x67, 0x00, 0x6d, 0x00, 0x73,
    0x00, 0x2e, 0x00, 0x64, 0x00, 0x61, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 0x1024c95e: the literals of FS_MasterSave.cpp. The wide text the code reads is L"gms.dat" at byte +0x5f (an odd address; the
// code adds 0x5f to the pool's address). As above, the wide strings are UTF-16 little-endian at the Mac's byte offsets: L"%ls" at
// +0x00, L"%ls%ls" at +0x08, L"%ls %ls" at +0x16, the narrow "!std::exception!!std::logic_error!!std::length_error!!" at +0x28.
// The units below are the image's bytes in pairs, so the odd offset shows as the text's characters in the high halves.
char16_t gMasterSaveWideText[0x38] = {
    0x0025, 0x006c, 0x0073, 0x0000, 0x0025, 0x006c, 0x0073, 0x0025,
    0x006c, 0x0073, 0x0000, 0x0025, 0x006c, 0x0073, 0x0020, 0x0025,
    0x006c, 0x0073, 0x0000, 0x0000, 0x7321, 0x6474, 0x3a3a, 0x7865,
    0x6563, 0x7470, 0x6f69, 0x216e, 0x7321, 0x6474, 0x3a3a, 0x6f6c,
    0x6967, 0x5f63, 0x7265, 0x6f72, 0x2172, 0x7321, 0x6474, 0x3a3a,
    0x656c, 0x676e, 0x6874, 0x655f, 0x7272, 0x726f, 0x2121, 0x6700,
    0x6d00, 0x7300, 0x2e00, 0x6400, 0x6100, 0x7400, 0x0000, 0x0000,
};

// ---- Initialised data (0x1025fcc0 ... 0x102b3a44)

// 0x1029cfb4: CLightPoint's vtable, 0x40 bytes (only its address is read, A(gCLightPointVt) in TPHField.cpp). The words are the Mac's.
unsigned char gCLightPointVt[0x40] = {
    TPW_WORD_LE(unsigned char, 0x1029cfac), TPW_WORD_LE(unsigned char, 0), TPW_WORD_LE(unsigned char, 0x102660e8),
    TPW_WORD_LE(unsigned char, 0x1029cfac), TPW_WORD_LE(unsigned char, 0xfffffff8), TPW_WORD_LE(unsigned char, 0x102660e0),
    TPW_WORD_LE(unsigned char, 0x10265790), TPW_WORD_LE(unsigned char, 0x10265798), TPW_WORD_LE(unsigned char, 0x102657a0),
    TPW_WORD_LE(unsigned char, 0x1029cfac), TPW_WORD_LE(unsigned char, 0xfffffff0), TPW_WORD_LE(unsigned char, 0x10265768),
    TPW_WORD_LE(unsigned char, 0x10265770), TPW_WORD_LE(unsigned char, 0x10265778), TPW_WORD_LE(unsigned char, 0x102660d8),
    TPW_WORD_LE(unsigned char, 0)};

// 0x102b1778: the vtable of std::basic_ifstream<char>'s ios part (0x18 bytes in the image, the code adds 0xc for the virtual base's
// table). The words are the Mac's.
char gIfstreamVtbl[0x20] = {TPW_WORD_LE(char, 0x102b1770), TPW_WORD_LE(char, 0),          TPW_WORD_LE(char, 0x102686f8),
                            TPW_WORD_LE(char, 0x102b1770), TPW_WORD_LE(char, 0xffffffb4), TPW_WORD_LE(char, 0x102686e0)};
// 0x102b173c: the same for the second ifstream class (0x18 bytes in the image).
char gIfstreamVtblC[0x20] = {TPW_WORD_LE(char, 0x102b1734), TPW_WORD_LE(char, 0),          TPW_WORD_LE(char, 0x102686f0),
                             TPW_WORD_LE(char, 0x102b1734), TPW_WORD_LE(char, 0xfffffff4), TPW_WORD_LE(char, 0x102686e8)};
// 0x102b1f50: the vtable of the ofstream part of the wide stream (the first four of its words).
uint32_t gOfsWVtbl[4] = {0x102b1f48, 0, 0x10268930, 0x102b1f48};

// 0x102b1960: COnlineParser's 36 token handlers, 12 bytes each: {0 (this delta), 0xffffffff (virtual index), the member's TVector}.
// The TVectors are the Mac's (0x102687d8 ParseSay down to 0x10268758 ParseFilter, two per command and four for ParseBuddy); a host
// 32-bit word cannot hold a member pointer, so the table cannot yet reach the translated Parse* members (the token dispatch,
// CTokenCtor and CParser::Parse, is a callee shim of another unit).
ParserHandler gOnlineTokenHandlers[36] = {
    {{0, 0xffffffff, 0x102687d8}}, {{0, 0xffffffff, 0x102687d8}},   // ParseSay 0x1020ef70
    {{0, 0xffffffff, 0x102687d0}}, {{0, 0xffffffff, 0x102687d0}},   // ParseTell 0x1020f0cc
    {{0, 0xffffffff, 0x102687c8}}, {{0, 0xffffffff, 0x102687c8}},   // ParseReply 0x1020f2f0
    {{0, 0xffffffff, 0x102687c0}}, {{0, 0xffffffff, 0x102687c0}},   // ParseLast 0x1020f460
    {{0, 0xffffffff, 0x102687b8}}, {{0, 0xffffffff, 0x102687b8}},   // ParseShout 0x1020f5d0
    {{0, 0xffffffff, 0x102687b0}}, {{0, 0xffffffff, 0x102687b0}},   // ParseEmote 0x1020f72c
    {{0, 0xffffffff, 0x102687a8}}, {{0, 0xffffffff, 0x102687a8}},   // ParseEmoteTell 0x1020f888
    {{0, 0xffffffff, 0x102687a0}}, {{0, 0xffffffff, 0x102687a0}},   // ParseEarmuffs 0x1020faf8
    {{0, 0xffffffff, 0x10268798}}, {{0, 0xffffffff, 0x10268798}},   // ParseIgnore 0x1020fbd4
    {{0, 0xffffffff, 0x10268790}}, {{0, 0xffffffff, 0x10268790}},   // ParseBlackMark 0x1020fd70
    {{0, 0xffffffff, 0x10268788}}, {{0, 0xffffffff, 0x10268788}},   // ParseBuddy 0x1020ff0c
    {{0, 0xffffffff, 0x10268788}}, {{0, 0xffffffff, 0x10268788}},
    {{0, 0xffffffff, 0x10268780}}, {{0, 0xffffffff, 0x10268780}},   // ParsePage 0x102100a4
    {{0, 0xffffffff, 0x10268778}}, {{0, 0xffffffff, 0x10268778}},   // ParseAfk 0x1021023c
    {{0, 0xffffffff, 0x10268770}}, {{0, 0xffffffff, 0x10268770}},   // ParseGoto 0x10210314
    {{0, 0xffffffff, 0x10268768}}, {{0, 0xffffffff, 0x10268768}},   // ParseLocate 0x102104ac
    {{0, 0xffffffff, 0x10268760}}, {{0, 0xffffffff, 0x10268760}},   // ParseVote 0x10210648
    {{0, 0xffffffff, 0x10268758}}, {{0, 0xffffffff, 0x10268758}},   // ParseFilter 0x10210720
};

// 0x1029be00: the 14 ride templates of BMP_CreateRide, 52 words each (generated at configure time from the player's copy).
uint32_t gRideTemplates[14 * 0x34] = {TPW_RIDE_TEMPLATES};

// Halfword values that stand for "no thing / no cell / no object" in the ids the code compares with. Every one of them is 0 in the
// image (initialised data), so these are zero-valued; they are read-only on the Mac, listed here with their addresses.
uint16_t gLLCCThingListEnd = 0;     // 0x1029e20c
uint16_t gLLCCE380 = 0;             // 0x1029e380
const uint16_t gLLCRNoCell = 0;     // 0x1029d430
uint16_t gStrandedQueueEnd = 0;     // 0x102a0e96
uint16_t gNoThoughtThing = 0;       // 0x102a0e98
uint16_t gThingLoopEmptyCellId = 0; // 0x102a0b24 (MapCell.h: the id of an empty cell, initial 0)
uint16_t gUI2DMapThingListEnd = 0;  // 0x102b2948
uint16_t gUI2DMapObjectListEnd = 0; // 0x102b294a
uint16_t gThingIdNone = 0;          // 0x102b294c
uint16_t gObjectIdNone = 0;         // 0x102b294e
uint16_t gRideListPending = 0;      // 0x102acee8
uint16_t gRideListHelpShown = 0;    // 0x102aceea
uint16_t gUIMainId7858 = 0;         // 0x102a7858
uint16_t gUIMainId785A = 0;         // 0x102a785a
uint16_t gUIMainId785C = 0;         // 0x102a785c
uint16_t gUIMainId785E = 0;         // 0x102a785e
uint16_t gUIMainId7860 = 0;         // 0x102a7860
uint16_t gUIMainId7862 = 0;         // 0x102a7862
uint16_t gUIMainId7864 = 0;         // 0x102a7864

// ---- BSS (0x102b3a44 ... 0x103bc8b0): zero on the Mac, so zero here.

// The camera and the first-person ride.
float gCameraTarget[3] = {};                      // 0x102b5bd8
s3DPoint gCameraViewPoint = {};                   // 0x102b5bc0
tag_sRideAnimSet* gFirstPersonRideSet = nullptr;  // 0x102b5a28
uint32_t gFirstPersonRideInstance = 0;            // 0x102b5a3c

// The ride the pointer is over.
tag_sRideAnimSet* gHighlightedRide = nullptr;           // 0x102c6ce8
tag_sRideAnimSet* gPointedRide = nullptr;               // 0x102c6ce4
s3DPoint gPointedRidePosition = {};                     // 0x102c6ccc
s3DPoint gPickupWorldPosn = {};                         // 0x102c6cec
s3DPoint gPointerWorldPosn = {};                        // 0x102c6cf8
tag_sRideAnimSet* gPotentialHighlightedRide = nullptr;  // 0x102d1f08
uint32_t gMaxMeshFaces4 = 0;                            // 0x102d1f14
uint32_t gMaxMeshF3E = 0;                               // 0x102d1f18
uint32_t gMaxMeshNodes = 0;                             // 0x102d1f1c

// The coaster editor.
uint32_t gCoasterWord16BC = 0;                // 0x102c16bc
uint32_t gCoasterEditEnabled = 0;             // 0x102c16d4
uint32_t gCoasterEditDown = 0;                // 0x102c16d8
int32_t gPylonMoveZ = 0;                      // 0x102c16c4
int32_t gPylonMoveX = 0;                      // 0x102c16c8
float gPylonSecondCost = 0;                   // 0x102c176c
tag_sPylon* gPylonMoveOrigData = nullptr;     // 0x102c1774
float gPylonMoveHeight = 0;                   // 0x102c1778
tag_sTrackBrace* gPylonMoveBrace = nullptr;   // 0x102c177c
tag_sTrackBrace* gPylonMoveMark2 = nullptr;   // 0x102c1780
tag_sTrackBrace* gPylonMoveMark1 = nullptr;   // 0x102c1784
void* gObjectTable[0x100] = {};               // 0x102b6f04
uint32_t gObjectTableLowest = 0;              // 0x102b7304
uint32_t gObjectTableFirstFree = 0;           // 0x102b7308
uint32_t gObjectTableCount = 0;               // 0x102b730c

// The cell grids.
const CMapCell* gMapGridBase = nullptr;       // 0x1034c1d0
const CMapCell* gMapGridEnd = nullptr;        // 0x1034c1cc
int32_t gMapGridCellSize = 0;                 // 0x1034c1c0
const CTrackCell* gTrackGridBase = nullptr;   // 0x1034c1c8
const CTrackCell* gTrackGridEnd = nullptr;    // 0x1034c1c4
int32_t gTrackGridCellSize = 0;               // 0x1034c1bc
CCellBase::CELL_IndexType gLLCC4824 = {};     // 0x102e4824
int32_t gLLCC4858 = 0;                        // 0x102e4858
char gTrackCostFree = 0;                      // 0x102ddb35
uint32_t gSkirtCellFlags[20][20] = {};        // 0x102da2ec
uint32_t gSkirtRows = 0;                      // 0x102da92c
uint32_t gSkirtColumns = 0;                   // 0x102da930
CNeighbourStep gNeighbourSteps[9] = {};       // 0x1034c3ac, filled by Peep.cpp's static initialiser

// The things and the world.
CThingArrayEntry* gUsedThingHead = nullptr;   // 0x1034cbf4
CThingArrayEntry* gFreeThingHead = nullptr;   // 0x1034cbf8
CThingArrayEntry gThingArray[kThingArraySize] = {};   // 0x1034cbfc
CWorld* gGameWorld = nullptr;                 // 0x1037ebfc
CMap* gWorldMap = nullptr;                    // 0x1037ec08
CWorld* gWorld = nullptr;                     // 0x1037ec0c
CBlockSave gBlockSave = {};                   // 0x1034c1ac
// 0x1034cbd4 (the registration node gSinitNodeList sits 0x14 above): the list __sinit_World_cpp builds in place.
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> gPendingDeleteList = {};
int32_t gRSSEF7BE0 = 0;                       // 0x102f7be0, rsse.cpp counts it up and down and answers it

// The program's objects.
CActionRecorder gActionRecorder = {};         // 0x1037ec44
CIntrface gIntrface = {};                     // 0x102dc1ac
CMessageCentre* gMessageCentre = nullptr;     // 0x1037f6c4
CRideDatabase gRideDatabase = {};             // 0x1037f6c8
UIInterface gUIInterface = {};                // 0x10396440
CUI2DMap gUI2DMap = {};                       // 0x1039742c
CFSLoadSave gLoadSave = {};                   // 0x10396a7c (see the findings: gLoadSaveWords is the Mac object's words, kept separate)
CFSGlobalMasterSave* gMasterSave = nullptr;   // 0x10380a8c
COnlineControl* gOnlineControl = nullptr;     // 0x10396d28
CFSOnlineSystem* gOnlineSystem = nullptr;     // 0x10380ab0

// The status panels.
BogStatusPanel gBogStatusPanel = {};                 // 0x10394800
FeatureStatusPanel gFeatureStatusPanel = {};         // 0x10394870
RideStatusPanel gRideStatusPanel = {};               // 0x103959a4
ShopStatusPanel gShopStatusPanel = {};               // 0x103959e0
ShowStatusPanel gShowStatusPanel = {};               // 0x10395a58
StaffRoomStatusPanel gStaffRoomStatusPanel = {};     // 0x10395a9c
StaffStatusPanel gStaffStatusPanel = {};             // 0x10395b1c
UpgradeStatusPanel gUpgradeStatusPanel = {};         // 0x10395b4c
VisitorStatusPanel gVisitorStatusPanel = {};         // 0x1037ec14
uint8_t gFinancePanelColours[8][5] = {};             // 0x103948d8, ui_FinancePanel.cpp fills it
CThing* gStaffRoomPanelThing = nullptr;              // 0x103947a4
StaffType gStaffRoomStaffType = {};                  // 0x10395a98
CThing* gStaffStatusThing = nullptr;                 // 0x10395b18
char16_t gStaffEditName[0x20] = {};                  // 0x10395ad4, the name edit box's text (the panel gives it 0x20 units)
// gMainPanel2Text (0x103949e8, the wide string object) is defined at host size by ui_mainpanel2.cpp, which builds it in place.
uint8_t gMainPanel2TextNode[12] = {};                // 0x103949fc, its registration record

// The master save and the online strings: each string object is built in place by its file's static initialiser (the host's string
// is larger than the Mac's 0x14 bytes), each node is the __register_global_object record (12 bytes on the Mac).
alignas(16) uint8_t gMasterSaveString[64] = {};      // 0x10380a90
alignas(16) uint8_t gMasterSaveNode[16] = {};        // 0x10380aa4
OnlineWideStringGlobal gOnlineSinitString2;          // 0x10380ab4
alignas(16) uint8_t gOnlineSinitNodeB[16] = {};      // 0x10380ac8
OnlineWideStringGlobal gUploadNameTail;              // 0x10380ad4
alignas(16) uint8_t gOnlineSinitNodeA[16] = {};      // 0x10380ae8

// ---- Engine data (the engine library's variables)

CDrawPrimitive* gei_DrawPrimitive = nullptr;         // engine data 0x10052b3c, assigned by tb_init.cpp
sAlignedBox gs_WorldBoundingArea = {};               // engine export, filled by TPEngine.cpp

// ---- Import slots (0x103bc8b0 ...): the platform layer's equivalents of the library's variables

SamsUtilities::UKeyboard* SamsUtilities::gKeyboard = nullptr;   // 0x103bcef4: NULL until the first use creates the keyboard

// 0x103bd05c (c/c++_shared __wctype_map, 256 halfwords): the character class bits of the characters below 0x100 in MSL's C
// library; the IP filter treats bits 2 and 4 as a separator. The values are the library's table (0x1003fab8 in its image).
uint16_t gSwearWctypeMap[256] = {
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030,
    0x0030, 0x0030, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x0080,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080,
    0x0080, 0x0080, 0x0080, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0008, 0x0008, 0x0008, 0x0008, 0x0001,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0080, 0x0080,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0080, 0x0080, 0x0080, 0x0080, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0040, 0x0080, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0080, 0x0080, 0x0080,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080,
    0x0008, 0x0080, 0x0080, 0x0080, 0x0080, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0000,
};

// 0x103bd0a0 (c/c++_shared __wupper_map, 256 halfwords): the upper-case map of the characters below 0x100, MacRoman (a-z to A-Z, the
// accented lower case letters to their capitals). The values are the library's table (0x1003feb8 in its image).
uint16_t gWUpperMap[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f,
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x00e7,
    0x00cb, 0x00e5, 0x0080, 0x00cc, 0x0081, 0x0082, 0x0083, 0x00e9,
    0x00e6, 0x00e8, 0x00ea, 0x00ed, 0x00eb, 0x00ec, 0x0084, 0x00ee,
    0x00f1, 0x00ef, 0x0085, 0x00cd, 0x00f2, 0x00f4, 0x00f3, 0x0086,
    0x00a0, 0x00a1, 0x00a2, 0x00a3, 0x00a4, 0x00a5, 0x00a6, 0x00a7,
    0x00a8, 0x00a9, 0x00aa, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x00af,
    0x00b0, 0x00b1, 0x00b2, 0x00b3, 0x00b4, 0x00b5, 0x00b6, 0x00b7,
    0x00b8, 0x00b9, 0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00ae, 0x00af,
    0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5, 0x00c6, 0x00c7,
    0x00c8, 0x00c9, 0x00ca, 0x00cb, 0x00cc, 0x00cd, 0x00ce, 0x00ce,
    0x00d0, 0x00d1, 0x00d2, 0x00d3, 0x00d4, 0x00d5, 0x00d6, 0x00d7,
    0x00d9, 0x00d9, 0x00da, 0x00db, 0x00dc, 0x00dd, 0x00de, 0x00df,
    0x00e0, 0x00e1, 0x00e2, 0x00e3, 0x00e4, 0x00e5, 0x00e6, 0x00e7,
    0x00e8, 0x00e9, 0x00ea, 0x00eb, 0x00ec, 0x00ed, 0x00ee, 0x00ef,
    0x00f0, 0x00f1, 0x00f2, 0x00f3, 0x00f4, 0x00f5, 0x00f6, 0x00f7,
    0x00f8, 0x00f9, 0x00fa, 0x00fb, 0x00fc, 0x00fd, 0x00fe, 0x00ff,
};
