// Unit v5050: the bump (collision) objects of the advisor world, main app.
// Structures keep the Mac's 32-bit layout (Ptr32 fields as uint32_t holding a 32-bit addressable address).
#pragma once
#include <cstdint>

#include "Ptr32.h"

// One collision object, 0x20 bytes.
struct tabCOLOBJECT {
    uint32_t flags;     // 0x00: bit 0 live, bit 5 (0x20) box (else circle), bit 6 (0x40) outside-area object
    int32_t x;          // 0x04
    int32_t y;          // 0x08
    int32_t radiusX;    // 0x0c radius of the circle, half width of the box
    int32_t radiusY;    // 0x10 half height of the box
    uint32_t pad14[3];  // 0x14
};
static_assert(sizeof(tabCOLOBJECT) == 0x20, "tabCOLOBJECT");

// What tagBUMP::owner points at: the kind (0 field) and the bounce scale (0x2c field, 1024 = 1.0).
struct tagBUMPOwner {
    int32_t kind;       // 0x00: -5, -8, -12 and -13 get the push on mC
    uint32_t pad04[10]; // 0x04
    int32_t scale;      // 0x2c
};
static_assert(sizeof(tagBUMPOwner) == 0x30, "tagBUMPOwner");

struct tagBUMP {
    uint32_t flags;     // 0x00
    uint32_t pad04[12]; // 0x04
    int32_t x;          // 0x34
    int32_t y;          // 0x38
    int32_t vx;         // 0x3c
    int32_t vy;         // 0x40
    int32_t dirX;       // 0x44
    int32_t dirY;       // 0x48
    uint32_t pad4c[3];  // 0x4c
    int32_t push;       // 0x58
    uint32_t pad5c[15]; // 0x5c
    Ptr32<tabCOLOBJECT> col;     // 0x98 current collision object
    Ptr32<tagBUMPOwner> owner;   // 0x9c
    uint32_t pada0;     // 0xa0
    int32_t counter;    // 0xa4
};
static_assert(sizeof(tagBUMP) == 0xa8, "tagBUMP");

// The list of collision objects, main app data 0x102b56c0 (not exported; name invented).
struct tabCOLLIST {
    uint32_t pad00[6];  // 0x00
    Ptr32<tabCOLOBJECT> first;   // 0x18
    int32_t count;      // 0x1c live objects
};
extern tabCOLLIST gColList;   // main app data 0x102b56c0

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object);   // 0x1002bc34 (outside the unit)

void BMP_InsideArea(tagBUMP* bump);    // 0x1002c260
void BMP_OutsideArea(tagBUMP* bump);   // 0x1002be04

// Unit v5062: the bump launch and collision (src/SetupHeadIDs.cpp). The ride record (tagBUMPRIDE, 0xd0 bytes) and the bump
// (0xac bytes apart in the list) are read by Mac offset there.
struct tagBUMPRIDE;
void SetupHeadIDs(tagBUMP* bump, int32_t flag);                        // 0x1002cddc
void BMP_Collision(tagBUMP* bump);                                     // 0x1002c7ec
// Unit v5682: BMP_Tick's callees outside the unit (src/BMP_Tick.cpp).
struct tabBUOY;
void BMP_AI(tagBUMP* bump);                                            // 0x10029128
void BMP_ControlSound(tagBUMP* bump);                                  // 0x10028e30
// Unit v5966 (src/BMP_Tick_BMP_ControlSound.cpp): sets the ride record's words 0x14 and 0x18 when the id matches.
void BMP_SetRideModelSelection(int32_t ride, int32_t a, int32_t b);    // 0x10026be4
int32_t BMP_GetCarsOnRide(int32_t ride);                               // 0x10026e74
tabBUOY* GetFinishLine(tagBUMPRIDE* ride);                             // 0x1002d340
void SetNextBuoyTarget(tabBUOY* buoy, tagBUMP* bump);                  // 0x100296f4
tagBUMP* BMP_StartBump(int32_t x, int32_t y, int32_t type);            // 0x1002eaac (outside the unit)
void SetControlType(tagBUMPRIDE* ride, tagBUMP* bump);                 // 0x1002d404 (outside the unit)

// Unit v5087: the bump removal (src/BMP_REMOVEBUMP.cpp). The list's words by Mac offset: +0 the magic 0x4a454647 when live,
// +8 the ride records (0xd0 bytes each), +0x10 the bumps (0xac bytes each), +0x14 their count (Ptr32 offsets from the arena base).
extern uint16_t gAdvisorThingId;   // 0x1029c960 (name invented): the advisor's own row in gThingArray (halfword)
int32_t DoAreasIntersect(tabCOLOBJECT* a, tabCOLOBJECT* b);   // 0x1002b6c0 (outside the unit)
// 0x10030020 (outside the unit): answers the thing id of a ride id in the halfword at out (a hidden result pointer on the Mac).
void TrackRideIdToThingId(uint16_t* out, int32_t rideId);
extern uint16_t gUsedListEnd;   // 0x1029cc10 (name invented, src/LevelLeaves_TrackRideIdToThingId.cpp): the id that ends the used-list walk
tagBUMP* FindTargetBump(tagBUMP* bump, tagBUMPRIDE* ride, int32_t* index);   // 0x1002df10
int32_t RemovePeepsFromBump(tagBUMP* bump, int32_t emit);                   // 0x1002e464
int32_t BMP_DOCK(tagBUMP* bump);                                            // 0x1002e740

// Unit v5112: the point tests over the collision list (src/BMP_InsideArea_PointOutAllInverse.cpp).
namespace NS_MacDoze { struct tagPOINT; }
int32_t PointOutAllInverse(int32_t x, int32_t y);                 // 0x1002bb64: 0 when a live outside-area object holds the point, else 1
tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y);          // 0x1002ba94: the first live inside-area object holding the point
// 0x1002b408: out = (object x, object y) moved by 0x87c (2172) per side the object's flag bits 2,3,4,5 leave open; answers out.
NS_MacDoze::tagPOINT* NavPointInArea(tabCOLOBJECT* object, NS_MacDoze::tagPOINT* out);

// Unit v5139: the peep lists of a ride and BMP_LeaveBump (src/BMP_REMOVEBUMP_BMP_PollForPeeps.cpp).
struct tag_sRideAnimSet;
void BMP_AddPeepToRideLeavingList(int32_t ride, uint32_t peep);   // 0x1002e268
void BMP_LeaveBump(tagBUMP* bump);                                // 0x1002e370
void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag);     // 0x100c6604 (outside the unit)

// Unit v5220: the ride commands (src/BMP_FixRide.cpp). The ride record is 0xd0 bytes, the bump slot 0xac bytes (read by Mac offset).
struct tagBUMPRIDE {
    int32_t kind;         // 0x00
    int32_t f04;          // 0x04 copied to the bump's 0x88 by BMP_BeginRide
    uint32_t pad08[18];   // 0x08
    int32_t state;        // 0x50 BMP_BeginRide: 1 to start
    int32_t broken;       // 0x54 2 when broken
    uint32_t pad58[30];   // 0x58
};
static_assert(sizeof(tagBUMPRIDE) == 0xd0, "tagBUMPRIDE");
struct tagBUMPSLOT {
    uint32_t flags;       // 0x00 bit 0 live, 0x4000 animated
    uint32_t pad04[1];    // 0x04
    uint32_t animSet;     // 0x08 index into gRideAnimSets
    uint32_t pad0c[1];    // 0x0c
    uint32_t animState;   // 0x10
    uint32_t pad14[4];    // 0x14
    int32_t animIndex;    // 0x24
    uint32_t pad28[1];    // 0x28
    int32_t emitter;      // 0x2c particle emitter handle, -1 none
    uint32_t pad30[22];   // 0x30
    int32_t f88;          // 0x88
    uint32_t pad8c[4];    // 0x8c
    Ptr32<tagBUMPRIDE> owner;   // 0x9c
    uint32_t pada0[3];    // 0xa0
};
static_assert(sizeof(tagBUMPSLOT) == 0xac, "tagBUMPSLOT");
struct tagBUMPWORLD {     // main app data 0x102b56c0 (the same global as gColList; this is the view the ride commands use)
    uint32_t magic;       // 0x00 0x4a454647 when live
    uint32_t pad04;       // 0x04
    Ptr32<tagBUMPRIDE> rides;   // 0x08
    uint32_t pad0c;       // 0x0c
    Ptr32<tagBUMPSLOT> bumps;   // 0x10
    int32_t bumpCount;    // 0x14
};
// The chain BMP_BreakRide follows from a ride anim set: mHeads->mF28 (table) -> entries[animIndex].node -> position floats.
struct tagBumpHeadNode { uint8_t pad00[0x30]; float x; float y; float z; };   // floats at 0x30, 0x34, 0x38
struct tagBumpHeadEntry { uint32_t f00; tagBumpHeadNode* node; uint32_t pad[3]; };   // 0x14 bytes on the Mac, node at 0x04
struct tagBumpHeadTable { uint32_t f00; tagBumpHeadEntry* entries; };   // entries at 0x04 on the Mac

// Unit v5247: the chunk writer of the advisor save (src/host_leaves_Write_BumpToChunk.cpp).
struct tabBMPCHUNK {
    int32_t start;             // 0x00 file position of the chunk header
    Ptr32<uint8_t> file;       // 0x04 the file handle
    uint32_t pad08;            // 0x08
    uint32_t tag;              // 0x0c
    int32_t dataSize;          // 0x10
    int32_t physicalSize;      // 0x14
};
static_assert(sizeof(tabBMPCHUNK) == 0x18, "tabBMPCHUNK");
// 0x10021960: writes the bump's 0xac bytes (43 words, the last one past tagBUMP) big-endian; answers the bytes written, 0 on error.
uint32_t Write_BumpToChunk(tabBMPCHUNK* chunk, tagBUMP& bump);
// 0x10021e44: writes the chunk header (tag, data size, physical size), frees the chunk, answers the physical size.
int32_t Write_CloseChunk(tabBMPCHUNK* chunk);

// Unit v5344: the ride teardown and performance (src/BMP_FixRide_BMP_CountTrackSections.cpp). The world is the list at 0x102b56c0
// (+0xc counts the live ride records). BMP_CountTrackSections and BMP_SetRidePerformance are declared in TPAdvisor.h and rsse.h.
extern int32_t gRidePerfMax;   // 0x1029ca24 (name invented): initial 100, the upper clamp of BMP_SetRidePerformance
extern int32_t gRidePerfMin;   // 0x1029ca28 (name invented): initial 0, the lower clamp
void BMP_FreeTrack(int32_t ride);                      // 0x100270e0
void BMP_DestroyRide(int32_t ride);                    // 0x10027274
void FreeBuoysForRide(tagBUMPRIDE* ride, int32_t n);   // 0x10027494 (outside the unit)
void FreeColObjectsForRide(tagBUMPRIDE* ride);         // 0x10027424 (outside the unit)
void FreeTrackForRide(tagBUMPRIDE* ride);              // 0x1002ef00 (outside the unit)

// Unit v5367: the collision object tests (src/BMP_InsideArea_PointOutAllInverse_PointInColObject.cpp).
// 0x1002b4cc: the live inside-area object that intersects area, whose open-side point is free, nearest to (px, py); NULL if none.
tabCOLOBJECT* ClosestIntersectingColObjectToPoint(tabCOLOBJECT* area, int32_t px, int32_t py, int32_t includeSelf);

// Unit v5627: the bump's move (src/BMP_Move.cpp). The bump's words by Mac offset: 0x08 its gRideAnimSets index, 0x10 the car anim
// (5 or 0xc), 0x14 the emitter state, 0x20 the sound handle, 0x24 and 0x28 head entries (-1 none), 0x2c the emitter (-1 none), 0x4c the
// speed, 0x50 the steer target, 0x54 the heading (9 bits), 0x58 the target heading, 0x5c the turn, 0x60 the turn's change. The owner (a
// tagBUMPRIDE) by Mac offset: 0x24 the speed scale (1024 = 1.0). The list at 0x102b56c0 by Mac offset: +4 the tick (its low 5 bits
// gate the effects), +0x38 the random seed (seed * 0x343fd + 0x269ec3).
struct tagBUMPTRIGGER {   // what a collision object's word 0x18 points at when its flag 0x100 is set (names invented)
    uint32_t flags;       // 0x00  2: a car anim change waits for the deferred anim
    int32_t axis;         // 0x04  0xc: compare x, else y
    int32_t open;         // 0x08  0: closed
    uint32_t animSet;     // 0x0c  gRideAnimSets index, 0 none
    int32_t magic;        // 0x10  0xdac
};
extern int32_t gBumpTurnMax;   // 0x1029cb30 (name invented): initial 16, the turn's upper clamp (a std::min reference)
extern int32_t gBumpTurnMin;   // 0x1029cb34 (name invented): initial -16, the lower clamp
void BMP_Move(tagBUMP* bump);                       // 0x10029b9c
// Unit v5656 (src/BMP_Move_SetNavRoute.cpp).
extern int32_t gBumpThrustLimitA;   // 0x1029cb28 (name invented): initial 0x30
extern int32_t gBumpThrustLimitB;   // 0x1029cb2c (name invented): initial 0x16
int32_t BMP_Distance(int32_t dx, int32_t dy);      // 0x1002b318 (outside the unit)
void BMP_Thrust(tagBUMP* bump, int32_t amount);    // 0x10029ac8 (outside the unit)
int32_t BumpToPoint(tagBUMP* bump, int32_t x, int32_t y);   // 0x1002992c
int32_t SetNavRoute(tagBUMP* bump, int32_t x, int32_t y);   // 0x100297cc
void BMP_MaskCar(tagBUMP* bump, int32_t mask);      // 0x1002ae74 (outside the unit)
void PossitionSound1(tagBUMP* bump);                // 0x1002b248 (outside the unit)
// 0x100c7a00 (outside the unit): the playing animation's kind and place; answers the time control's flags word (4: it ended).
uint32_t GetCurrentAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control);

// Unit v5630: the advisor save (src/host_leaves_Write_BumpToChunk_BMP_Save.cpp). The views BMP_Save reads by Mac offset (names
// invented). The ride record's words it writes: 0x04, 0x10, 0x14, 0x18, 0x1c, 0x50, 0xb0, 0xb4, 0xb8; its lists at 0xbc
// (tagBUMPSAVETRACK), 0xc4 and 0xc8 (tagBUMPSAVELINK). The bump's: 0x30 a tagBUMPSAVELINK list, 0x7c its table index (-1 none).
struct tagBUMPSAVELINK {
    int32_t value;                   // 0x00
    uint32_t pad04[2];               // 0x04
    int32_t value0c;                 // 0x0c (written for the bump's list only)
    Ptr32<tagBUMPSAVELINK> next;     // 0x10
};
static_assert(sizeof(tagBUMPSAVELINK) == 0x14, "tagBUMPSAVELINK");
struct tagBUMPSAVETRACK {
    uint32_t pad00;                  // 0x00
    int32_t f04;                     // 0x04
    uint32_t pad08[3];               // 0x08
    int32_t f14;                     // 0x14
    int32_t f18;                     // 0x18
    uint32_t pad1c[2];               // 0x1c
    Ptr32<tagBUMPSAVETRACK> next;    // 0x24
};
static_assert(sizeof(tagBUMPSAVETRACK) == 0x28, "tagBUMPSAVETRACK");
struct tagBUMPSAVEENTRY {            // the world's table at 0x20, 0x1c bytes each
    uint32_t pad00[2];               // 0x00
    int32_t f08;                     // 0x08
    int32_t f0c;                     // 0x0c
    Ptr32<tagBUMPRIDE> ride;         // 0x10
    uint32_t pad14[2];               // 0x14
};
static_assert(sizeof(tagBUMPSAVEENTRY) == 0x1c, "tagBUMPSAVEENTRY");
struct tagBUMPSAVEWORLD {            // main app data 0x102b56c0 (the same global as gColList; the view BMP_Save uses)
    uint32_t magic;                  // 0x00 0x4a454647 when live
    uint32_t pad04;                  // 0x04
    Ptr32<tagBUMPRIDE> rides;        // 0x08 0xd0 bytes each
    int32_t rideCount;               // 0x0c the live ride records
    Ptr32<uint8_t> bumps;            // 0x10 0xac bytes each
    int32_t bumpCount;               // 0x14 the live bumps
    uint32_t pad18[2];               // 0x18
    Ptr32<tagBUMPSAVEENTRY> table;   // 0x20
};
static_assert(sizeof(tagBUMPSAVEWORLD) == 0x24, "tagBUMPSAVEWORLD");
extern const char gBMPSaveTime[];    // 0x1023cbfa "17:56:39" (name invented)
extern const char gBMPSaveDate[];    // 0x1023cc03 "Jan  8 2001" (name invented)
// Unit v5706 (src/BMP_InsideArea_PointOutAllInverse_BMP_Distance.cpp).
void SingleSoundAtBump(tagBUMP* bump, int32_t event);                       // 0x1002b180
void MineToSystemCords(int32_t x, int32_t y, NS_MacDoze::tagPOINT* out);    // 0x1002b0dc
int32_t BMP_Save(void* file);        // 0x10022464: answers Write_CloseChunk's answer for the save chunk, 0 when nothing is written
// Unit v5L09 (src/BMP_Load.cpp): reads what BMP_Save writes; answers the save chunk's physical size, 0 when it failed.
int32_t BMP_Load(void* file);        // 0x10023850
// Unit v5558: the ride creation (src/BMP_FixRide_BMP_CountTrackSections_BMP_CreateRide.cpp).
// 0x1029be00 (name invented): the ride templates, 0xd0 bytes each, template (-kind - 1) for the kinds -1 to -14.
extern uint32_t gRideTemplates[14 * 0x34];
// 0x1002756c: copies the kind's template into a free ride (or the one id names), sets the kind up, answers kind << 8 | index.
int32_t BMP_CreateRide(int32_t kind, int32_t x, int32_t y, int32_t rotation, int32_t id, int32_t f10);
tabCOLOBJECT* BMP_StartColObject(int32_t x, int32_t y, int32_t radius);                 // 0x1002edfc (outside the unit)
tabCOLOBJECT* BMP_AddBuoyToRide(tagBUMPRIDE* ride, int32_t x, int32_t y);               // 0x1002ebd0 (outside the unit)
void BMP_StartColObjectForRide(tagBUMPRIDE* ride, int32_t x, int32_t y, int32_t shape); // 0x1002ed74 (outside the unit)
// Unit v5659 (src/host_leaves_BMP_InitSystem.cpp).
void SetBuoySequence(tagBUMPRIDE* ride);   // 0x1002d390 (outside the unit)
// Unit v5926 (src/BMP_InsideArea_PointOutAllInverse_BMP_Distance_BMP_MaskCar_2.cpp).
extern int32_t gBumpAiMinDist;   // 0x1029cab0 (name invented): initial 1, the lower clamp of the chase distance
extern int32_t gBumpAiMaxStep;   // 0x1029cab4 (name invented): initial 0x18, the upper clamp of the chase step

// Unit v51017 (src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp): the chunk reader.
#include "TbError.h"
// 0x1002ffc4 (outside the unit; the answer comes through a hidden buffer): reads size bytes of the file into buffer.
TbError BMPFile_Read(void* file, void* buffer, uint32_t size, uint32_t* transferred);
uint32_t Read_OpenChunk(void* file, Ptr32<tabBMPCHUNK>* out);                    // 0x10021fac
uint32_t Read_DataFromChunk(tabBMPCHUNK* chunk, void* buffer, uint32_t size);    // 0x100220bc (pad08 is the read position)
uint32_t Read_BumpFromChunk(tabBMPCHUNK* chunk, tagBUMP& bump);                  // 0x100221d4

// Unit v51036 (src/BMP_FixRide_BMP_CountTrackSections_BMP_SetRideDuration.cpp). The list at 0x102b56c0 by Mac offset: +0x20 the buoy
// table (Ptr32, 0x400 entries of 0x1c bytes), +0x24 the live buoys. Buoy: +0 flags (bit 0 live), +4 index, +8 x, +0xc y, +0x10 its ride,
// +0x14 the next buoy of the ride. The ride's +0xa8 first buoy, +0xac their count; +0x58 its camera mesh.
tabBUOY* BMP_StartBuoy(int32_t x, int32_t y);                // 0x1002ec84
void __sinit_HoardingsBalance_cpp();                         // 0x10021074
extern uint8_t gHoardingsDtorVector[16];                     // stand-in for 0x10265fa0
extern uint8_t gHoardingsNode434C[16];                       // stand-in for 0x102b455c
extern uint8_t gHoardingsNode4130[16];                       // stand-in for 0x102b4340

// Unit v51069 (src/AdvisorScoring_sinit_Advisor_cpp.cpp). A buoy of a ride's list, 0x1c bytes (Ptr32 fields, offsets from the Mac).
struct tabBUOY {
    uint32_t flags;               // 0x00  bit 0 live, bit 1 (2) skipped by SetBuoySequence, bit 2 (4) the finish line
    int32_t index;                // 0x04
    int32_t x;                    // 0x08
    int32_t y;                    // 0x0c
    Ptr32<tagBUMPRIDE> ride;      // 0x10
    Ptr32<tabBUOY> next;          // 0x14
    uint32_t pad18;               // 0x18
};
static_assert(sizeof(tabBUOY) == 0x1c, "tabBUOY");
int32_t FindBuoy(tagBUMPRIDE* ride, int32_t x, int32_t y);   // 0x10025960: the index of the ride's buoy at (x, y), 0 when none

// Unit v5566: a track section placed on a ride record (src/BMP_TrackSectionPlaceSection.cpp). The list at 0x102b56c0 by Mac offset:
// +8 ride records, +0x18 buoy pool (0x20 bytes each, 0x800), +0x1c live buoys, +0x20 collision object pool (0x1c each, 0x400),
// +0x24 live objects, +0x28 section pool (0x28 each, 0x400), +0x2c live sections. Ride record: +0xa8 object list, +0xac count, +0xbc sections.
// The type's low halfword picks the buoy layout (jump table 0x1029cbd8), bit 0x10000 skips the collision object. Answers 1 placed, 0 refused.
int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9);   // 0x1002ef98
