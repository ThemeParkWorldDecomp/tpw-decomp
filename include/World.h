// CWorld (World.cpp, main app): the world's data and its construction (unit w113: CGame::InitWorld, CWorld::CWorld, Allocate,
// AllocateThingHeap, InitThingHeap); the rest of the class is still partial. The host layout equals the Mac layout (0x1da748 bytes):
// no member written by the constructor holds a pointer, so none is widened. Members the code never touches are opaque runs
// (named by their Mac offsets) that the units translating the later world functions split.
#pragma once
#include <cstddef>
#include <cstdint>

#include "Arrival.h"
#include "MapCell.h"
#include "ObjectControl.h"
#include "ParkTime.h"
#include "RegionEffectCell.h"
#include "StaffDB.h"
#include "TbDynamicStringTemplate.h"
#include "TrackCell.h"
#include "vector2d.h"

class CParkAnalyser;
class CAdvisor;
class CResearchLab;
class CBankAccount;    // unit w123b: GetBankAccount
class CObject;         // unit w126: CWorld::Register
class CHandyman;       // unit w139: CWorld::Register
class CMechanic;
class CEntertainer;
class CGuard;
class CResearcher;
struct IDXTYPEUniqueNameMapId;      // unit w126: CMap::AddRegionalEffector
struct IDXTYPEUniqueNameObjectId;   // unit w126: CControlManager::GetObjectControl
class CChallengeHQ;    // unit w123b: GetChallengeHQ
// For the thing list functions (unit w121); the definitions are in analyser.h.
class CThing;
struct IDXTYPEUniqueNameThingId;
template<class Tag, class T> struct tag_IDX_TYPE;

class CWeather;
enum NavMode : int32_t;   // include/TPCamera.h (unit w209: Vehicle::mNavMode)

// The map the world keeps at CWorld + 0x2d8 (CMap, Map.cpp; the size is not known, so it is only ever reached through CWorld::GetMap).
// The kind of region effect an object adds to the map (unit w126; the names are mine, the values are the ones CObject::CObject passes).
enum RegionEffector : int32_t {
    kRegionEffectorRelief = 1,
    kRegionEffectorLitter = 2,       // unit w145: CMapCell::RemoveLitter takes litter kind 7 out with it (name a guess)
    kRegionEffectorSecurity = 4,
    kRegionEffectorVomit = 5,        // unit w145: ... and litter kind 8 with this one (name a guess)
    kRegionEffectorFireworks = 7
};

// Unit w139: one region's effect record (0xc bytes each, eight at CWorld + 0x1d93dc; CMap::Init fills them from gMainBalance). The five
// effects are halfwords at +0 to +8, the radius a byte at +0xa; the byte at +0xb is never written by Init.
struct RegionEffectInfo {
    uint16_t mHappiness, mIllness, mHunger, mSecurity, mAttraction;
    uint8_t mRadius;
    uint8_t mPad;
};
static_assert(sizeof(RegionEffectInfo) == 0xc, "a region effect record is 0xc bytes");

class CMap {
public:
    // 0x101016cc (src/MapDelta_map.cpp, unit w145): adds the effect to the region the map cell (the 1 based cell id, by value: the HIGH halfword of the Mac's
    // register) lies in.
    void AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // 0x1010176c (src/MapDelta_map.cpp, unit w145): takes the effect out of the region the map cell lies in again (CObject::UpdateTurn's fireworks).
    void RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    void DeleteMapInEngine();   // 0x1010136c (196 bytes, src/MapDelta_map.cpp, unit w145)
    // 0x1013e5ac (216 bytes, untranslated): loads the level's attribute map file; nonzero when it loaded.
    int32_t LoadAttribMap(const char* path);
    void Init();   // 0x10100a8c (980 bytes, src/Map_init.cpp, unit w139): numbers the cells and tracks and resets the map (CWorld::SetupNewLevel, unit w117)
    // Unit w139 (untranslated callees of the unit's functions, stand-ins in the launcher).
    // 0x101012f4 (80 bytes, src/MapDelta_map.cpp, unit w145; a static function, no map pointer: r3 = x, r4 = y): 1 when the cell lies inside the level.
    static int32_t IsXYValid(int32_t x, int32_t y);
    // 0x101025c4 (src/MapDelta_map.cpp, unit w145): a new time stamp for the map. 0x10102604 (same file): stamps the map cell (the id is the HIGH
    // halfword of the Mac's register).
    uint32_t MakeNewTimeStamp();
    void SetTimeStamp(uint32_t stamp, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // 0x10101ae0 (100 bytes, src/MapDelta_map.cpp, unit w145): the map cell (the 1 based cell id x + y * 128 + 1) of park entrance A (which = 0) or B (which = 1), from
    // gMainBalance's EntranceAPosX/Y or EntranceBPosX/Y; the Mac answers it through a hidden result (r3), r4 = the map, r5 = which.
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetParkEntrancePos(int32_t which) const;
    // Unit w142: 0x101019bc (100 bytes, src/MapDelta_map.cpp, unit w145), the same for the ticket booth A (0) or B (1).
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetTicketBoothPos(int32_t which) const;
    // Unit w145 (src/MapDelta_map.cpp). The id of every map cell argument is the HIGH halfword of the Mac's register (tag_IDX_TYPE).
    // 0x10101640 (48 bytes): the five effect halfwords of the region effect cell of the map cell (at map + 0x1b1104, 10 bytes each).
    CRegionEffectCell* GetRegionalEffectCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // 0x1010180c (320 bytes): adds (add != 0) or takes out (add == 0) the effector's record of effects around the cell: each cell within
    // the record's radius gets the effect divided by (distance + 1), the distance being |dx| + |dy|.
    void ModifyRegionalEffector(int32_t add, RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    // 0x10101a50, 0x10101b74 and 0x10101c0c (100 bytes each): the cell id of the bus stop, the park side and the bus stop side of the
    // crossing, A (which = 0) or B, from gMainBalance. The crossing getters read A's Y for B too (the original's quirk).
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetBusStopPos(int32_t which) const;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetCrossingParkSidePos(int32_t which) const;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetCrossingBSSidePos(int32_t which) const;
    // 0x101026a0 (48 bytes) reads and 0x10102724 (24 bytes) reads by x and y the time stamp word of the 16 by 16 block the cell lies in
    // (a 33 word wide grid at map + 0x1b0000).
    uint32_t GetTimeStamp(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell);
    uint32_t GetTimeStampCell(uint32_t x, uint32_t y);
    void UpdateMapInEngine();   // 0x10101460 (196 bytes): refreshes the height field tile of every map cell and track cell
    void CleanUpPark();         // 0x10101554 (196 bytes): removes the litter that is not marked for collection from every map cell
    // Unit w146 (src/MapDelta_save.cpp). 0x10100e80 (1,096 bytes): loads (save == 0) or saves the 0x4000 cells of the three grids (map cells, track cells
    // and region effect cells) of the level: one status byte per cell says which of the three differ from a fresh cell and follow. Version 0 and 1 only;
    // 0 on success, 1 at the first failure. The save path also traces how many cells it left out as default (even for a version out of range).
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
};
// Unit w145: the counter MakeNewTimeStamp increments (Mac 0x1034c21c, BSS; the name is invented).
extern uint32_t gMapTimeStamp;

// The 150 object controls the world keeps at CWorld + 0x1d943c (0x12cc bytes on the Mac; reached only through CWorld::GetControlManager).
// Unit w176 (src/ControlManager.cpp) names the whole block: the 0x96 controls, then the count (+0x12c0), the id searched last (+0x12c4, a
// halfword and two pad bytes) and the control it found (+0x12c8, a pointer on the Mac). The host keeps a pointer in four bytes as an index:
// 0 is NULL, else the control's index + 1 (CWorld holds the word as part of mF1DA6FC).
class CControlManager {
public:
    void SetupInitialControls();   // 0x100fa2ac (68 bytes, src/ControlManager_setup.cpp, unit w129): clears the 0x12cc bytes and checks every control was set up
    // 0x100fa32c (304 bytes, src/ControlManager.cpp, unit w176; SetupInitialControls calls it last): every ride in the ride database that has no control yet
    // gets the next one (id, CostOfUpgrade, FatigueEffect, RipOffOK, researched when CostOfResearch is 0, three zero words).
    void CheckAllControlsInitialised();
    // 0x100fa0b8 (152 bytes, src/ControlManager.cpp, unit w176): the control of the object type (the id is the HIGH halfword of the Mac's register); the last
    // argument is 0 in the callers. The id searched last and its control are cached; a type with no control asserts (flag 0) and answers NULL.
    ObjectControl* GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag);
    const ObjectControl* GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) const;   // 0x100fa1b8 (140 bytes): the const overload the analyser's queries call; it does not cache
    // 0x100f9ed0 (432 bytes, src/ControlManager.cpp, unit w176): the 150 controls, the count and the cached id; 1 at the first that fails, else 0.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);

    ObjectControl mControls[0x96];   // 0x0000
    int32_t mNumObjectControls;      // 0x12c0  "mNumObjectControls"
    uint16_t mPreviousSearchKey;     // 0x12c4  "mPreviousSearchKey" (an ObjectId, two bytes)
    uint8_t mF12C6[2];               // 0x12c6  pad
    uint32_t mPreviousSearchResult;  // 0x12c8  the Mac's pointer to the control found last; here 0 or the control's index + 1
};
static_assert(sizeof(CControlManager) == 0x12cc, "the control manager is 0x12cc bytes");
static_assert(offsetof(CControlManager, mNumObjectControls) == 0x12c0 && offsetof(CControlManager, mPreviousSearchKey) == 0x12c4 &&
              offsetof(CControlManager, mPreviousSearchResult) == 0x12c8, "CControlManager layout");

// The world is also the CMacroAI (the staff database, the park time and the arrivals are its first 0x2d8 bytes).
class CMacroAI {
public:
    // Unit w205 (src/World_save.cpp). 0x1013a13c (96 bytes): destroys the staff database at +0 (the Mac's flag, which also deletes the
    // object, has no host form).
    ~CMacroAI();
    void Init();   // 0x1010096c (76 bytes, src/MacroAI.cpp, unit w129): CArrival::Init(+0x2c4), CStaffDB::Init(+0), CParkTime::Init(+0x2a0)
    // 0x101009dc (136 bytes, src/MacroAI.cpp, unit w129): the arrivals, the staff database and the park clock (CWorld::DoGameTurn calls it); then, when
    // the turn of *gGameWorldRef (not of `this`) is a multiple of 100, the golden ticket check (+0x2c0).
    void Update();
    // Unit w146 (src/MapDelta_save.cpp). 0x10100890 (172 bytes): the staff database (+0), the park time (+0x2a0) and the arrivals (+0x2c4) serialise in turn;
    // 1 at the first that fails, else 0.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
};

// Vehicle (the ride vehicles' steering state, vehicle.cpp). Unit w203 (src/Vehicle_steer.cpp) translated its geometry helpers, clear_behaviours and the
// destructor; unit w209 (src/Vehicle_path.cpp) translated the path code (recalc_path, next_step, is_stuck) and split the padding that code reads and writes
// into named fields (each tagged (w209)); w230 adds Serialise. The Mac object has no vtable and is at most 0xc8 bytes (CStaff holds
// one at +0xd4, include/ThingReceivers.h). The host keeps every Mac offset up to 0xc8; the spans no translated code touches are padding arrays named
// by their offsets. The Mac's word 0x30 is a POINTER to the target Vector2d: on the host it is the dead
// mF30 and the native pointer mTarget is appended after the Mac's last byte (the CThingArrayEntry convention below), so the host object is 8 bytes longer
// than the Mac's. Field names are invented.
class Vehicle {
public:
    static void clear_behaviours();   // 0x10133358 (60 bytes, src/Vehicle_steer.cpp, unit w203): empties the behaviour list; takes no argument
    // 0x101332cc (104 bytes, src/Vehicle_steer.cpp, unit w203): the Mac's DELETING form (the flag is in r4): when `this` is not null it destroys the five
    // waypoints, frees the block when (int16_t)flag > 0 and answers `this`. The host body is empty (Vector2d is trivial) and `delete` frees by itself.
    ~Vehicle();

    // Unit w203 (src/Vehicle_steer.cpp). (mForward, mSide) is the rotation from the world to the vehicle's frame and, transposed, back.
    Vector2d point_to_local(const Vector2d& p) const;   // 0x101333c4 (168 bytes): (p - mPos) in the vehicle's frame
    Vector2d vec_to_global(const Vector2d& v) const;    // 0x101334a4 (136 bytes): the transposed rotation, no translation
    Vector2d predict_pos(int32_t t) const;              // 0x10133b58 (96 bytes): mPos + mVelocity * t
    Fixed is_finished_path() const;                     // 0x101341a4 (216 bytes): a progress, gVehOne (1.0) when finished; not a bool
    // Unit w209 (src/Vehicle_path.cpp): the path code.
    enum RecalcPathMode : int32_t {};
    // 0x10133be4 (1392 bytes): plans the walk from mPos to `target` with the wall hugger (wallhug_tricky on gVehPath, with mNavMode), measures it and stamps it
    // with a new map time stamp; mode 1 puts the vehicle's own square in front of the path. Answers 1, or 0 when there is no path (then the vehicle is
    // finished and mUnreachable is 1). The mode is a word compared with 1 and nothing else.
    int32_t recalc_path(const Vector2d& target, RecalcPathMode mode);
    // 0x10133564 (640 bytes): one step. The weighted behaviours' forces are summed and capped, the velocity changes and is capped; the step is taken when the
    // nav rules (gVehNavRules) allow it between the two map squares, and mStuckHistory shifts in a 1 for a refused step and for a step without progress.
    void next_step();
    // 0x101342ac (304 bytes): 0 when the path is finished or the vehicle is exactly at the start of it (progress 1.0 is the finished answer), else the share
    // of the last 15 steps that were refused or made no progress, in 16.16 (bits 0 to 14 of mStuckHistory, divided by 15).
    Fixed is_stuck();

    // Unit w230 (src/Vehicle_serialise.cpp).
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused); // 0x10130eb4, 2212 B

    uint32_t mF00;                // +0x00 serialized mass, a value rather than a pointer
    Fixed mRadius;                // +0x04
    Vector2d mPos;                // +0x08
    Vector2d mVelocity;           // +0x10
    Fixed mMaxForce;              // +0x18 (Pilot::set_speed: 0.4 * speed)
    Fixed mMaxSpeed;              // +0x1c (Pilot::set_speed: 0.2 * speed; Arrival's cap)
    Vector2d mForward;            // +0x20 the vehicle's forward axis in world coordinates
    Vector2d mSide;               // +0x28 the vehicle's side axis in world coordinates
    uint32_t mF30;                // +0x30 the Mac's target pointer; loading clears this and native mTarget immediately before velocity
    uint32_t mF34;                // +0x34 cleared after loading formation_pos successfully
    Vector2d mF38;                // +0x38 serialized formation_pos
    Vector2d mF40;                // +0x40 serialized force
    uint32_t mPathStamp;          // +0x48 (w209) the map's time stamp recalc_path took when it planned the path (Follow_Path compares the ground's stamp with it)
    Vector2d mDestination;        // +0x4c (w209) the destination of the path (recalc_path's argument)
    int32_t mNumWaypoints;        // +0x54 the squares of the whole path (recalc_path: the wall hugger's count, 0xfc at most)
    int32_t mNumLoaded;           // +0x58 (w209) the waypoints held in mWaypoints: mNumWaypoints, 5 at most
    int32_t mWaypoint;            // +0x5c the current waypoint (an index, never range checked)
    uint8_t mPathFlag;            // +0x60 a byte: 1 means the path is finished (is_finished_path), non-zero counts as the last leg (Separation)
    uint8_t mPad61[3];            // +0x61 .. +0x63
    Vector2d mWaypoints[5];       // +0x64 .. +0x8b
    Fixed mLegLength[5];          // +0x8c .. +0x9f (w209) the length of the leg from waypoint i to i + 1 (the fifth is never written)
    Fixed mPathTotal;             // +0xa0 the path's length (is_finished_path divides by it)
    Fixed mPathLegA;              // +0xa4 a length added to the distance to the current waypoint
    Fixed mPathLegB;              // +0xa8 a length added after it
    Fixed mLastProgress;          // +0xac (w209) the progress of the last step (next_step compares the new one with it)
    uint32_t mStuckHistory;       // +0xb0 (w209) one bit per step, the newest in bit 0: 1 for a refused step or one without progress (is_stuck counts bits 0 to 14)
    NavMode mNavMode;             // +0xb4 (w209) the mode wallhug_tricky and the nav rules get (CStaff's mF188)
    int32_t mUnreachable;         // +0xb8 (w209) 1 when the last plan found no path, else 0
    uint8_t mPadBC[0xc];          // +0xbc .. +0xc7
    const Vector2d* mTarget;      // HOST ONLY, after the Mac's last byte (0xc8): the target the Mac keeps as a pointer at +0x30
};
static_assert(offsetof(Vehicle, mRadius) == 0x04 && offsetof(Vehicle, mPos) == 0x08 && offsetof(Vehicle, mVelocity) == 0x10, "Vehicle layout");
static_assert(offsetof(Vehicle, mMaxForce) == 0x18 && offsetof(Vehicle, mMaxSpeed) == 0x1c, "Vehicle layout");
static_assert(offsetof(Vehicle, mForward) == 0x20 && offsetof(Vehicle, mSide) == 0x28 && offsetof(Vehicle, mF30) == 0x30, "Vehicle layout");
static_assert(offsetof(Vehicle, mF34) == 0x34 && offsetof(Vehicle, mF38) == 0x38 && offsetof(Vehicle, mF40) == 0x40, "Vehicle layout");
static_assert(offsetof(Vehicle, mPathStamp) == 0x48 && offsetof(Vehicle, mDestination) == 0x4c && offsetof(Vehicle, mNumWaypoints) == 0x54, "Vehicle layout");
static_assert(offsetof(Vehicle, mNumLoaded) == 0x58, "Vehicle layout");
static_assert(offsetof(Vehicle, mWaypoint) == 0x5c && offsetof(Vehicle, mPathFlag) == 0x60 && offsetof(Vehicle, mPad61) == 0x61, "Vehicle layout");
static_assert(offsetof(Vehicle, mWaypoints) == 0x64 && offsetof(Vehicle, mLegLength) == 0x8c, "Vehicle layout");
static_assert(offsetof(Vehicle, mPathTotal) == 0xa0 && offsetof(Vehicle, mPathLegA) == 0xa4 && offsetof(Vehicle, mPathLegB) == 0xa8, "Vehicle layout");
static_assert(offsetof(Vehicle, mLastProgress) == 0xac && offsetof(Vehicle, mStuckHistory) == 0xb0 && offsetof(Vehicle, mNavMode) == 0xb4, "Vehicle layout");
static_assert(offsetof(Vehicle, mUnreachable) == 0xb8 && offsetof(Vehicle, mPadBC) == 0xbc, "Vehicle layout");
static_assert(offsetof(Vehicle, mTarget) == 0xc8 && sizeof(Vehicle) == 0xd0, "Vehicle layout");

// The ride vehicles' pilots: a Vehicle that follows a path (Pilot adds no data). CWorld::SetupNewLevel calls the static init_behaviours.
class Pilot : public Vehicle {
public:
    // Unit w203 (src/Vehicle_steer.cpp).
    void reset(int32_t x, int32_t y, int32_t vx, int32_t vy);   // 0x101347b0 (20 bytes): mPos = (x, y), mVelocity = (vx, vy)
    void new_target(int32_t x, int32_t y);                      // 0x101347ec (48 bytes): recalc_path((x, y), mode 0)
    void move();                                                // 0x10134844 (32 bytes): next_step()
    void get_pos(int32_t& x, int32_t& y);                       // 0x10134888 (20 bytes)
    void set_speed(float speed);                                // 0x101349cc (128 bytes): mMaxForce and mMaxSpeed from the speed, 2.0 at most
    Fixed is_finished_path() const;                             // 0x101348c4 (216 bytes): Vehicle::is_finished_path's code at its own address

    // Unit w209 (src/Vehicle_path.cpp).
    // 0x10134404 (480 bytes): empties the behaviour list and appends "avoid_walls" (weight 0.9), "follow_path" (0.5) and "separation" (0.1), each found by name in
    // gVehBehaviourTable; a name not found (or a full list) appends nothing. Takes no argument.
    static void init_behaviours();
    // 0x10134610 (328 bytes): a vehicle at rest at (0, 0), radius 0.2, speed 1.0 (set_speed), path finished (mPathFlag 1). It writes only the fields below, the
    // padding and mPathStamp, mLegLength and the bytes at +0xbc stay as they were.
    Pilot();
    // Unit v51034: 0x10113028 (96 bytes), the deleting form (flag in r4); ~Vehicle runs after the empty body.
    ~Pilot();
};
static_assert(sizeof(Pilot) == sizeof(Vehicle), "Pilot adds no data");

// 0x10025cb0 (untranslated): shuts the bitmap system down.
void BMP_CloseSystem();
// 0x10025a94 (500 bytes, untranslated): starts the bitmap system (no argument, no answer used).
void BMP_InitSystem();
// Unit v5659: 0x10025db8 (264 bytes, src/host_leaves_BMP_InitSystem.cpp): destroys every live ride (state 4 first) and rebuilds the free list of the
// 1024 link records at the world's 0x30.
void BMP_FreeAll();

// The two names SetupNewLevel looks for in the ride database: the "gates" and the "lights" feature (wide strings 0x1034cb18 and
// 0x1034caf8, built by __sinit_World_cpp 0x10141c14, which is not translated: src/World_setup.cpp defines both with their texts until it is).
extern TbDynamicStringTemplate<char16_t> gWorldGatesName;    // 0x1034cb18  "gates"
extern TbDynamicStringTemplate<char16_t> gWorldLightsName;   // 0x1034caf8  "lights"
// The three arrival vehicles' feature names (wide strings built by __sinit_World_cpp like the two above; src/World_gates.cpp defines them with
// their texts until it is translated). Names invented (unit w193).
extern TbDynamicStringTemplate<char16_t> gArrivalBusName;       // 0x1034cb78  "bus"
extern TbDynamicStringTemplate<char16_t> gArrivalSeaplaneName;  // 0x1034cb58  "seaplane"
extern TbDynamicStringTemplate<char16_t> gArrivalFerryName;     // 0x1034cb38  "Ferry"
// Unit w194 (src/World_sequences.cpp). The id of the thing that plays the winning animation (the "End" feature's object), 0 for none, and the
// feature's name in the ride database (a wide string built by __sinit_World_cpp like the names above; defined there with its text until it is
// translated). Names invented.
extern uint16_t gWinningAnimThing;                              // 0x1034cad4
extern TbDynamicStringTemplate<char16_t> gWorldEndName;         // 0x1034cad8  "End"

// The world's table of things (original name unknown; the symbols call the entry CWorld::_CThingArrayEntry): 0x2800 entries at
// 0x1034cbfc (BSS; the table ends where gGameWorld, 0x1037ebfc, begins), five words each on the Mac (units w103 and w113).
// Word 0 is the thing's pointer, 0 while the entry is free (host: native, mThing). Word 1 is the entry's own index
// (mF04[0]). Words 2 to 4 are pointers to entries: the next and the previous entry of the USED list and the next entry of the
// FREE list (CWorld::AddThingToThingList 0x1013b054, _CThingArrayEntry::FreeMe 0x1013af58; read from their decompiled text).
// CWorld::InitThingHeap (0x1013ab48) writes all five words of every entry. The host's pointers are native, so they sit in members
// appended after mF04, and mF04[1..3] are dead on the host.
struct CThingArrayEntry {
    void* mThing;            // word 0
    uint32_t mF04[4];        // words 1 to 4 (mF04[0] is the index; the rest are dead on the host)
    CThingArrayEntry* mNextUsed;   // word 2
    CThingArrayEntry* mPrevUsed;   // word 3
    CThingArrayEntry* mNextFree;   // word 4
    // 0x1013af58 (192 bytes, src/World_turn.cpp, unit w121; the Mac name is CWorld::_CThingArrayEntry::FreeMe, `this` is the entry):
    // unlinks the entry from the used list, pushes it on the free list and releases the thing with the plain operator delete.
    void FreeMe();
};
constexpr int32_t kThingArraySize = 0x2800;
extern CThingArrayEntry gThingArray[kThingArraySize];   // 0x1034cbfc
// The heads of the two lists (initialised data 0x1034cbf4 and 0x1034cbf8; original names unknown). CWorld::CWorld sets the free head
// to entry 0 and the used head to NULL; InitThingHeap sets them again with the free head at entry 1.
extern CThingArrayEntry* gUsedThingHead;   // 0x1034cbf4
extern CThingArrayEntry* gFreeThingHead;   // 0x1034cbf8

class CStaffHQ;
class CMechanicHQ;
class CWorld {
public:
    CWorld();                                   // 0x10139f70 (376 bytes, src/World_init.cpp)
    // 0x1013a1c0 (124 bytes): builds the thing heap, then registers the world as save block 0 ("World Struture", sic); the error
    // branch (a heap that did not build: CExitError type 8) is dead, AllocateThingHeap always answers 1.
    bool Allocate();
    bool AllocateThingHeap();                   // 0x1013aadc (60 bytes): gWorld = this, gWorldMap = the map, InitThingHeap, true
    void InitThingHeap();                       // 0x1013ab48 (148 bytes): the thing table and its lists (leaf, ignores this)
    void DoGameTurn();                          // 0x1013b278
    void DoOnlineGameTurn();                    // 0x1013b608
    void UpdateThingsInEngine(float step);      // 0x1013de70
    void UpdateBumperCarsInEngine(float step);  // 0x1013e09c
    // 0x1013ebc8 (96 bytes, src/World_getters.cpp, unit w123b): the analyser of the thing table row in mParkAnalyserThing; the const twin is 0x1013eb38.
    CParkAnalyser* GetParkAnalyser();
    // 0x10140fac (316 bytes, src/World_rides.cpp, unit w114): rebuilds gRidesOwned from the ride database (a ride counts when its DownloadedRideNumber
    // is not 0 and it is not a preview ride while `previews` is nonzero), then answers 0 when any of the 32 bytes of `required` has bit 0
    // set while the same byte of gRidesOwned has not, else 1. Only bit 0 of each byte is tested (the original's `(mask & r) == 1`).
    // It never reads the world.
    int32_t DoIHaveTheseRides(const char* required, int32_t previews);
    // 0x1013eab0 (96 bytes, src/World_GetAdvisor.cpp, unit w103): the advisor of the table behind the global at 0x10260728 whose row is the
    // world's short at +0x1da71c (five words a row), after a game assertion on the table's first word.
    CAdvisor* GetAdvisor();
    // 0x10140ee4 (104 bytes, src/World_sequences.cpp, unit w194): marks the ride (by its id) as selected in the park's downloaded ride bitmap.
    void FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);
    // 0x1013a9fc (src/World_exit.cpp): the end of a level: the mouse and the map are given up, every thing deleted, the vehicles'
    // behaviours and the bitmap system closed; mF1DA738 is 2 during and 1 after. CGame::ExitLevel calls it first.
    void ExitLevel();
    // 0x1013a264 (src/World_exit.cpp): CGame::ExitLevel calls this second; it does FreeThingHeap and nothing else.
    void Free();
    // 0x1013ac08 (src/World_exit.cpp): the Mac's loop of 0x2800 empty iterations (the per-slot free compiled to nothing).
    void FreeThingHeap();
    // 0x1013ac4c (src/World_turn.cpp, unit w121): kills and deletes every thing (and sets mF1DA738 to 2 itself).
    void DeleteAllThings();
    bool IsPlayingWinningAnim();   // 0x1013bdd0 (40 bytes, src/World_sequences.cpp, unit w194)
    void StartBankruptcySequence();   // 0x1013baa4 (396 bytes, src/World_sequences.cpp, unit w194)
    // Unit w205 (src/World_save.cpp).
    enum WorldState : int32_t {};   // the type SAVEVAR<CWorld::WorldState> (0x1014182c, 260 bytes) writes mF1DA738 as
    // 0x1013bf18 (2,740 bytes): writes the world to the save file: a version word (2), 25 fields (the thing table rows of the singleton things, the
    // turn, the start time, ...), the control manager, the macro layer and the map, then every used thing of the thing table by its kind byte.
    // 1 when all of it was written, 0 at the first part that fails (nothing is written after it).
    int32_t Save(void* file);
    // 0x1013e99c and 0x1013ea24 (96 bytes each): the staff and the mechanic headquarters, as GetAdvisor: the thing table row named by
    // mStaffHQThing (+0x1da718) and mMechanicHQThing (+0x1da71a).
    CStaffHQ* GetStaffHQ();
    CMechanicHQ* GetMechanicHQ();
    // Unit w207 (src/World_load.cpp). 0x1013c9f0 (4,756 bytes): reads what Save wrote (a version word, the world's 25 fields, the control manager,
    // the macro layer and the map), after resetting the thing table, then relinks the used list and makes every used thing of its kind in place
    // before its Serialise reads it. 1 when all of it was read, 0 at the first part that fails (nothing is read after it).
    int32_t Load(void* file);
    // 0x1013e89c, 0x1013e8dc, 0x1013e91c, 0x1013e95c (12 bytes each, src/LevelInit2.cpp, unit w110): the height field's place and
    // size. They ignore the world: each reads a word of gMainBalance (value slots 334 to 337, the rows "HeightfieldXStart",
    // "HeightfieldYStart", "HeightfieldWidth", "HeightfieldHeight").
    int32_t GetHeightfieldXStart() const;
    int32_t GetHeightfieldYStart() const;
    int32_t GetHeightfieldWidth() const;
    int32_t GetHeightfieldHeight() const;
    // 0x1013a2ac (1,828 bytes, src/World_setup.cpp, unit w117): the set-up of a new level: the level's balance files into gMainBalance,
    // the map and the macro AI reset, the ten singleton things made (their ids kept in the world), the "gates" and "lights" objects
    // found in the ride database, the research lab, the bitmap system and the pilots started, and the first event message posted.
    // The answer is 0 when the level's first or second balance file did not load (the easy mode file never counts), else 1.
    bool SetupNewLevel(char* levelFolder);
    // 0x1013ece8 (96 bytes, src/World_getters.cpp, unit w123b; the const twin is 0x1013ec58): the research lab of the thing table row in mResearchLabThing.
    CResearchLab* GetResearchLab();

    // The thing lists and the world's turn (unit w121, src/World_turn.cpp). The three that answer a thing id return the two byte struct
    // through a hidden pointer on the Mac and never read the world.
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetUsedThingHead() const;                       // 0x1013adb0 (40 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetNextUsedThing(const CThing* thing) const;    // 0x1013ae08 (112 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> AddThingToThingList(CThing* thing);             // 0x1013b054 (200 bytes)
    void DeletePendingThings();                                                                      // 0x1013dda8 (148 bytes)
    // 0x1013ff7c (12 bytes): the whole word at +0x1da710, nonzero while the park is closed (World.h keeps the name mF1DA710).
    int32_t IsParkClosed() const;
    int32_t ParkGates_GetStatus() const;                                                             // 0x1013ffb4 (148 bytes)
    // Unit w193 (src/World_gates.cpp): the park gates and the arrival vehicle (the bus, seaplane or ferry of CArrival::CreatePeople). DoNextLeg answers
    // the vehicle's thing id through a hidden result (CreatePeople ignores it); the status is 2 while the passengers get off, 6 an error, -1 or 0 idle
    // and 4 on its way back.
    void SetParkOpenOrClosed(int32_t open, int32_t gateFlag);                                 // 0x1013fc3c (780 bytes)
    void ParkGates_Open();                                                                    // 0x1014007c (148 bytes): sets the gates object's script variable 0 to 1
    void ParkGates_Close(int32_t flag);                                                       // 0x1014013c (200 bytes): sets it to 0 (flag 0) or 2
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ArrivalVehicle_DoNextLeg(int32_t leg);   // 0x10140234 (1120 bytes)
    int32_t ArrivalVehicle_GetStatus();                                                       // 0x101406cc (264 bytes)
    int32_t ArrivalVehicle_CrossingIsClear();                                                 // 0x1014080c (804 bytes)
    int32_t ArrivalVehicle_PeopleWaitingToLeave();                                            // 0x10140b6c (488 bytes)
    // Unit w194 (src/World_sequences.cpp): the sequences and the thing table helpers.
    void PopulatePark();                      // 0x1013b6ac (436 bytes): the first people of a park whose gates are open
    void StartingPopulationForOnline();       // 0x1013b88c (476 bytes): 25 people, then a mechanic and an entertainer for every ride
    void StartWinningSequence();              // 0x1013bc68 (308 bytes)
    void EndWinningSequence();                // 0x1013be2c (188 bytes)
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetPrevUsedThing(const CThing* thing) const;   // 0x1013aeb0 (112 bytes)
    void ReconstructFreeList();               // 0x1013dd14 (96 bytes)
    int32_t GetNewPersonIndex();              // 0x10140e64 (80 bytes)
    int32_t ConvertDLRidesUsedToIndices(uint16_t* out);   // 0x1014113c (304 bytes): out is 0x200 bytes
    void RestoreEngineObjects();              // 0x1013e210 (292 bytes)
    void DeleteEngineObjects();               // 0x1013e368 (528 bytes)
    int32_t ArrivalVehicle_PeopleLeftToDropOff();                                             // 0x10140d98 (8 bytes)
    int32_t ArrivalVehicle_IsSeagoing();                                                      // 0x10140de0 (76 bytes)

    // 0x1013ed74 (96 bytes, src/World_getters.cpp, unit w123b; unit w120 calls it): the weather thing of the table row in mWeatherThing (word 0 of that row).
    CWeather* GetWeather();
    // Unit w123b (src/World_getters.cpp): the other thing getters, all the same code as GetAdvisor: the world's halfword id is read, the assertion on
    // row 0 is made, then word 0 of the id's row is answered (NULL for a free row). The const twins answer pointers to const.
    const CParkAnalyser* GetParkAnalyser() const;   // 0x1013eb38, mParkAnalyserThing
    const CResearchLab* GetResearchLab() const;     // 0x1013ec58, mResearchLabThing
    CBankAccount* GetBankAccount();                 // 0x1013edfc, mBankAccountThing
    void Register(CObject* object);                 // 0x1013fa10 (src/World_register.cpp, unit w139): the world's list of rides (the CHandyman and CMechanic overloads are 0x1013ef14 and 0x1013f144)
    // Unit w139 (src/World_register.cpp, 116 bytes each): the five staff overloads put the thing at the head of its list (the head halfword is
    // the thing id; the old head goes into the thing at +0x20c). Heads: handyman 0x1da73c, mechanic 0x1da73e, entertainer 0x1da740,
    // researcher 0x1da742, guard 0x1da744; the object list head is 0x1da746 (link at +0xc of the object).
    void Register(CHandyman* staff);        // 0x1013ef14
    void Register(CMechanic* staff);        // 0x1013f144
    void Register(CEntertainer* staff);     // 0x1013f374
    void Register(CGuard* staff);           // 0x1013f5ac
    void Register(CResearcher* staff);      // 0x1013f7d8
    // Unit w178 (src/World_unregister.cpp, 344 bytes each): takes the thing out of its list again (the inverse of Register above): walks the list
    // from the head by the links until the thing's id, asserting that row 0 of the thing table is free at each step, then points the previous
    // thing's link (or the head) at the thing's own link.
    void Unregister(CHandyman* staff);      // 0x1013efb8
    void Unregister(CMechanic* staff);      // 0x1013f1e8
    void Unregister(CEntertainer* staff);   // 0x1013f41c
    void Unregister(CGuard* staff);         // 0x1013f650
    void Unregister(CResearcher* staff);    // 0x1013f880
    void Unregister(CObject* object);       // 0x1013fab4
    CChallengeHQ* GetChallengeHQ();                 // 0x1013ee88, mChallengeHQThing (not the neighbours at +0x1da728 and +0x1da738)
    // The map at +0x2d8 (CMap's size is not known: the world's unknown bytes are viewed through it).
    CMap* GetMap() { return reinterpret_cast<CMap*>(reinterpret_cast<uintptr_t>(this) + 0x2d8); }   // (plain arithmetic: the Mac does it on a NULL world too)
    // The object controls at +0x1d943c (SetupNewLevel reaches them through the global gGameWorld, not through its own `this`).
    CControlManager* GetControlManager() { return reinterpret_cast<CControlManager*>(reinterpret_cast<uintptr_t>(this) + 0x1d943c); }

    // Unit w123c (src/World_death.cpp). 0x1013b154 (48 bytes): queues the id on the global pending-delete list (gPendingDeleteList in
    // WorldThings.h, not a member of the world) with TbTList::AddHead 0x10141ad4; the world is not read. The argument's two bytes are
    // the high halfword of the Mac's register.
    void AddToPendingDeleteList(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);
    // 0x1013b1e4 (56 bytes): the world's random generator, state mStartTime (+0x1da708): state = state * 0x19660d + 0x3c6ef35f (32 bits),
    // stored, and the answer is the state's absolute value as a bit image (the state itself stays as it was, a negative one included).
    uint32_t GetRand();
    // 0x1013b244 (12 bytes, src/World_sequences.cpp, unit w194): seeds the random generator (CMapCell::PlaceLitter passes the cell id, a zero-extended halfword).
    void SeedRand(uint32_t seed);

    // Mac offsets in the comments; the constructor writes the members marked "cleared" and the cells, nothing else (a world built over
    // 0xa5 keeps 0xa5 everywhere else, so no member has an initialiser).
    CStaffDB mStaffDB;                              // 0x000  0x2a0
    CParkTime mParkTime;                            // 0x2a0  0x24
    CArrival mArrival;                              // 0x2c4  0x14
    CMapCell mMapCells[0x4000];                     // 0x2d8  0x110000 (GetMap() is the address of the first cell)
    CTrackCell mTrackCells[0x4000];                 // 0x1102d8  0xa0000
    uint8_t mF1B02D8[0x1104];                       // 0x1b02d8  never touched by the constructor
    CRegionEffectCell mRegionEffectCells[0x4000];   // 0x1b13dc  0x28000
    uint8_t mF1D93DC[0x60];                         // 0x1d93dc
    ObjectControl mObjectControls[0x96];            // 0x1d943c  0x12c0 (the Mac constructs them: ObjectControl's constructor is empty)
    uint8_t mF1DA6FC[0x1da708 - 0x1da6fc];          // 0x1da6fc
    uint32_t mStartTime;     // 0x1da708  time(NULL) when the level was set up (name proposed by unit w117)
    uint32_t mTurn;          // 0x1da70c
    uint32_t mF1DA710;       // 0x1da710  the constructor sets 1 (meaning unknown); 1 while the park is closed: SetParkOpenOrClosed (unit w193) sets and clears it
    uint32_t mPersonCount;       // 0x1da714  the people made so far: GetNewPersonIndex counts it up and answers it; SetupNewLevel clears it
    // The thing table rows (thing ids) of the singleton things SetupNewLevel makes, in the order it makes them (unit w117):
    uint16_t mStaffHQThing;       // 0x1da718  CStaffHQ
    uint16_t mMechanicHQThing;    // 0x1da71a  CMechanicHQ
    uint16_t mAdvisorThing;       // 0x1da71c  the advisor's row in gThingArray (unit w103, src/World_GetAdvisor.cpp)
    uint16_t mTagSystemThing;     // 0x1da71e  CTagSystem
    uint16_t mParkAnalyserThing;  // 0x1da720  CParkAnalyser (GetParkAnalyser 0x1013ebc8 reads it)
    uint16_t mResearchLabThing;   // 0x1da722  CResearchLab (GetResearchLab reads it)
    uint16_t mWeatherThing;       // 0x1da724  CWeather
    uint16_t mBankAccountThing;   // 0x1da726  CBankAccount
    uint16_t mUIMsgReceiverThing; // 0x1da728  CUIMsgReceiver
    uint16_t mArrivalVehicleThing;  // 0x1da72a  the vehicle now in use (one of the next three), 0 for none (unit w193)
    uint16_t mBusThing;             // 0x1da72c  the "bus" feature object, made on demand by ArrivalVehicle_DoNextLeg
    uint16_t mSeaplaneThing;        // 0x1da72e  the "seaplane" feature object
    uint16_t mFerryThing;           // 0x1da730  the "Ferry" feature object
    uint16_t mGatesThing;         // 0x1da732  the "gates" CObject (0 when the ride database has none)
    uint16_t mLightsThing;        // 0x1da734  the "lights" CObject (0 when the ride database has none)
    uint16_t mChallengeHQThing;   // 0x1da736  CChallengeHQ
    int32_t mF1DA738;        // 0x1da738  4 silences the park music and chatter
    uint16_t mF1DA73C;       // 0x1da73c  the constructor clears these six halfwords (in the order 3c, 3e, 40, 44, 42, 46)   // unit w139: head of the handyman list (CWorld::Register)
    uint16_t mF1DA73E;       // 0x1da73e   // unit w139: head of the mechanic list (CWorld::Register)
    uint16_t mF1DA740;       // 0x1da740   // unit w139: head of the entertainer list (CWorld::Register)
    uint16_t mF1DA742;       // 0x1da742   // unit w139: head of the researcher list (CWorld::Register)
    uint16_t mF1DA744;       // 0x1da744   // unit w139: head of the guard list (CWorld::Register)
    uint16_t mF1DA746;       // 0x1da746 (the object ends at 0x1da748, CGame::InitWorld's operator new size)   // unit w139: head of the object list (CWorld::Register)
};
static_assert(sizeof(CWorld) == 0x1da748, "the world is 0x1da748 bytes");
static_assert(offsetof(CWorld, mParkTime) == 0x2a0, "CParkTime at 0x2a0");
static_assert(offsetof(CWorld, mArrival) == 0x2c4, "CArrival at 0x2c4");
static_assert(offsetof(CWorld, mMapCells) == 0x2d8, "the map cells at 0x2d8 (GetMap)");
static_assert(offsetof(CWorld, mTrackCells) == 0x1102d8, "the track cells at 0x1102d8");
static_assert(offsetof(CWorld, mRegionEffectCells) == 0x1b13dc, "the region cells at 0x1b13dc");
static_assert(offsetof(CWorld, mObjectControls) == 0x1d943c, "the object controls at 0x1d943c");
static_assert(offsetof(CWorld, mStartTime) == 0x1da708, "the start time at 0x1da708");
static_assert(offsetof(CWorld, mTurn) == 0x1da70c, "mTurn at 0x1da70c");
static_assert(offsetof(CWorld, mPersonCount) == 0x1da714, "mPersonCount at 0x1da714");
static_assert(offsetof(CWorld, mStaffHQThing) == 0x1da718, "the thing ids from 0x1da718");
static_assert(offsetof(CWorld, mArrivalVehicleThing) == 0x1da72a, "mArrivalVehicleThing at 0x1da72a");
static_assert(offsetof(CWorld, mGatesThing) == 0x1da732, "mGatesThing at 0x1da732");
static_assert(offsetof(CWorld, mChallengeHQThing) == 0x1da736, "mChallengeHQThing at 0x1da736");
static_assert(offsetof(CWorld, mF1DA710) == 0x1da710, "mF1DA710 at 0x1da710");
static_assert(offsetof(CWorld, mAdvisorThing) == 0x1da71c, "mAdvisorThing at 0x1da71c");
static_assert(offsetof(CWorld, mF1DA738) == 0x1da738, "mF1DA738 at 0x1da738");
static_assert(offsetof(CWorld, mF1DA73C) == 0x1da73c, "the cleared halfwords at 0x1da73c");
// Two pointers to the world: CWorldSavvy::GetWorld and CTimeSig read gWorld (0x1037ec0c);
// CGame::InitWorld creates the world into gGameWorld (0x1037ebfc), which most of the game
// uses. gGameWorldRef (initialised data 0x102a460c) holds gGameWorld's address, so it
// behaves as a reference to it. Original names unknown.
extern CWorld* gWorld;
extern CWorld* gGameWorld;
extern CWorld*& gGameWorldRef;
// The world's map: CWorld::AllocateThingHeap stores the world + 0x2d8 here (global 0x1037ec08, original name unknown; the base of
// CCellBase::GetTrackCellEquiv's arithmetic, 0x100f48c4).
extern CMap* gWorldMap;   // 0x1037ec08
