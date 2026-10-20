// The ride vehicles' steering behaviours and vector helpers (vehicle.cpp, main app, SimTheme Park 2.04b3; unit w203, src/Vehicle_steer.cpp).
// The Vehicle and Pilot classes are in World.h (the world's setup and exit code call them); everything else of vehicle.cpp that unit translated is
// here, so that a class named Arrival (CArrival's header is a World.h include) and a global truncate (POSIX has one) stay away from the
// files that include World.h. Names are invented except those the original's symbols give.
#pragma once
#include <cstdint>

#include "World.h"
#include "vector2d.h"

// A steering behaviour: the Mac object is a vtable pointer and a 32-byte name (the constructor strcpy's it; Pilot::init_behaviours finds a behaviour by
// strcmp of the name at +4). calculate answers the force the behaviour asks for. The Mac table has RTTI, a zero word, then calculate at +8;
// it has no virtual destructor slot. The host retains normal native vptrs.
class Behaviour {
public:
    virtual Vector2d calculate(Vehicle& vehicle) = 0;
    char mName[0x20];   // +0x04 on the Mac (never read by the translated calculate functions)
};

// Each concrete behaviour derives without data of its own and overrides calculate.
class Seek : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override;   // 0x10131788 (100 bytes): (target - mPos) - mVelocity
};

class Arrival : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override;   // 0x1013181c (404 bytes): seek slowing down within 2.0 * mMaxSpeed of the target
};

class Separation : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override;   // 0x10131c64 (308 bytes): away from the vehicles around (do_separation)
};

class Repel_Obstacle : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override;   // 0x10131f08 (136 bytes): away from the obstacles around (do_repel_obstacle)
};

class Avoid_Obstacle : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override;   // 0x101320e0 (384 bytes): sidestep the nearest obstacle ahead (do_avoid_obstacle)
};

class Avoid_Walls : public Behaviour {
public:
    // 0x10132298 (2648 bytes, unit w218, src/Vehicle_walls.cpp): keeps the vehicle off the map's walls. Looks at the walls of the square it stands on in the
    // directions the predicted position (predict_pos by gVehOne) lies in: a wall the prediction has come within the radius of gives a push straight back,
    // twice the overlap; else a wall corner of the diagonal square that the move's line passes within the radius of gives the push away from the corner;
    // else (0, 0).
    Vector2d calculate(Vehicle& vehicle) override;
};

// An obstacle as the callbacks hand it over: only the position and the radius are read (the size is unknown).
struct Obstacle {
    Vector2d mPos;     // +0x00
    Fixed mRadius;     // +0x08
};

// The behaviour list: gVehBehaviours holds {mData, mTop, mEnd}, entries of two words on the Mac (the weight first, then the behaviour; the pointers are
// native on the host). Pilot::init_behaviours (unit w209) appends the weighted behaviours it finds by name: weights 0xe666 (0.9), 0x8000 (0.5)
// and 0x1999 (0.1) in the three entries its text shows.
struct BehaviourEntry {
    Fixed mWeight;             // +0x00
    Behaviour* mBehaviour;     // +0x04 on the Mac
};

struct BehaviourList {
    BehaviourEntry* mData;     // the first entry
    BehaviourEntry* mTop;      // one past the last entry in use, the next free one (the Mac's sinit sets it to mData)
    BehaviourEntry* mEnd;      // the Mac's sinit sets it to mData + 0x280 bytes (sic)
};

// The globals of vehicle.cpp, initialized by InitVehicleGlobals (0x10134f5c).
extern Fixed gVehZero;               // 0x1034ca90: 0
extern Fixed gVehOne;                // 0x1034ca8c: 0x10000 (1.0)
extern Vector2d gVehSteerSum;        // 0x1034ca60: the callbacks' accumulator (the force being summed)
extern Fixed gVehAvoidRange;         // 0x1034ca58: Avoid_Obstacle's look-ahead range
extern Fixed gVehAvoidNearest;       // 0x1034ca5c: the nearest hit so far (0x7fffffff for none)
extern BehaviourList gVehBehaviours; // 0x1034ca74: the behaviour list

// Vector helpers of vehicle.cpp (the Mac passes the Vector2d answer through a hidden pointer in r3).
Vector2d perp(const Vector2d& v);                    // 0x1013380c (24 bytes): (-v.y, v.x)
Vector2d normalise(const Vector2d& v);               // 0x1013384c (380 bytes): v scaled to a length of 1.0 (about)
Vector2d truncate(const Vector2d& v, int32_t max);   // 0x101339f4 (312 bytes): v scaled down to a length of max when it is longer

// The callbacks the candidates walks call with each candidate (the Mac passes them as TVectors 0x10266d70, 0x10266d60 and 0x10266d50).
void do_separation(const Vehicle& me, const Vehicle* other);                 // 0x10131a7c (276 bytes)
void do_repel_obstacle(const Vehicle& vehicle, const Obstacle* obstacle);    // 0x10131dcc (256 bytes)
void do_avoid_obstacle(const Vehicle& vehicle, const Obstacle* obstacle);    // 0x10131fc8 (220 bytes)

// ---- Unit w209 (src/Vehicle_path.cpp, src/VehicleCallbacks_map.cpp): the path following. Names invented except those the original's symbols give.
struct wallhug_path;       // include/TPCamera.h
enum NavMode : int32_t;    // include/TPCamera.h (declared in World.h too)

// The behaviour that steers a vehicle along its planned path.
class Follow_Path : public Behaviour {
public:
    // 0x10132d7c (1184 bytes): replans when the ground's time stamp is newer than the path's or the vehicle is stuck (is_stuck >= 0x6666), takes the
    // waypoint steps, and answers the force that seeks the next waypoint (Arrival at the last one) minus the velocity; (0, 0) minus the velocity when
    // there is no path or it is finished.
    Vector2d calculate(Vehicle& vehicle) override;
};

// The two functions next_step hands the nav rules (the original's VehicleCallbacks.cpp; src/VehicleCallbacks_map.cpp).
Vector2d map_global_to_local(uint32_t x, uint32_t y);   // 0x101359e8 (20 bytes): the map square of a position, (x >> 16, y >> 16), logical shifts. The answer type is a guess
int32_t map_fn(int32_t x, int32_t y, void* data);       // 0x10135fd4 (108 bytes): the square's number, (x + (y << 7) + 1) & 0xffff, or -1 when CMap::IsXYValid refuses it; data is not used

using VehNavRulesFn = int32_t (*)(int32_t, int32_t, NavMode);

// The globals of vehicle.cpp that unit w209 meets (src/Vehicle_path.cpp defines them).
extern wallhug_path gVehPath;                     // 0x1034c708 (BSS, 0x208 bytes): the buffer recalc_path hands the wall hugger and reads the path from
extern void* gVehMapData;                         // 0x1034ca94 (BSS): map_fn's third argument
extern VehNavRulesFn gVehNavRules;                // 0x102a45a0: the nav rules, called through this pointer (host: &implement_nav_rulesA2B)
extern Behaviour* gVehBehaviourTable[8];          // 0x102a4478: the behaviours Pilot::init_behaviours searches by name (Mac: seven objects, 0x30 bytes apart, from
                                                  // 0x1034c908 up to 0x1034ca28 and a null eighth word; native typed objects in Vehicle_serialise.cpp)

// ---- Unit w218 (src/Vehicle_walls.cpp, src/VehicleCallbacks_nav.cpp): the wall avoidance and the callbacks of the vehicles' walks and nav rules. Names
// invented except those the original's symbols give.
// The candidate walks: each calls the function with the vehicle and each candidate within the radius.
// 0x101356b8 (636 bytes): the things of the map squares within the radius, (radius + 0xffff) >> 16 squares around the vehicle's, that are vehicles (kinds 1, 4
// to 8 and 0x12) other than the one given and nearer than the radius and not at its very spot; the function is called for the first five of them.
void callback_collision_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Vehicle*));
// 0x1013598c (4 bytes): an empty function on the Mac, so the original never repels or avoids an obstacle.
void callback_obstacle_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Obstacle*));

// 0x10135a2c (20 bytes): the position of a map square's corner, (x << 16, y << 16), plain 32-bit shifts. The answer type is a guess (as map_global_to_local's)
Vector2d map_local_to_global(uint32_t x, uint32_t y);

// 0x10135a70 (1324 bytes): the nav rules, answering whether a step from the square numbered `from` to the square numbered `to` is allowed in the mode.
// gVehNavRules points at it (the Mac keeps its TVector, 0x10266d88, in the data). Only the low 16 bits of a number are used (the square is the number - 1:
// x in the low 7 bits, y above); a step of more than one square in either direction is refused and asserts once (GAME_ASSERTMSG, the two statics below
// say it did); 1 for a step to the same square or to a neighbour (a diagonal one only when one of the two ways round it is free of walls), 0 when a wall
// stands in the way or `to` is -1.
int32_t implement_nav_rulesA2B(int32_t from, int32_t to, NavMode mode);
extern uint8_t gVehNavRulesInit;       // 0x1034ca98 (BSS): the guard byte of the function's static below; 0 until the first call, which clears the static and sets it
extern int32_t gVehNavRulesAsserted;   // 0x1034ca9c (BSS): 1 once a step the rules refuse has asserted, so that each assert is shown once

// Unit w230 (src/Vehicle_serialise.cpp).
void InitVehicleGlobals(); // 0x10134f5c, 616 B (__sinit_vehicle_cpp), invented native spelling

// HOST SUPPORT: runtime boundaries, logical callback identities, and independent registration nodes. No original functions.
enum class VehicleRuntimeCallback { EntryConstructor, ListDestructor, VectorDestructor, SeekDestructor, ArrivalDestructor,
    SeparationDestructor, AvoidObstacleDestructor, RepelObstacleDestructor, AvoidWallsDestructor, FollowPathDestructor };
struct VehicleRegistrationNode { unsigned char bytes[12]; };
void* VehicleRuntimeAllocate(uint32_t originalBytes);
BehaviourEntry* VehicleRuntimeConstruct(void* raw, VehicleRuntimeCallback constructor, const VehicleRuntimeCallback* destructor,
    uint32_t originalStride, uint32_t count);
void VehicleRuntimeRegister(void* object, VehicleRuntimeCallback destructor, VehicleRegistrationNode* node);
char* VehicleRuntimeCopyName(char* destination, const char* source);

// ---- Unit v5733 (src/VehicleCallbacks_nav_FixedSizeArray_Dtor_FixedSizeArray.cpp)
// The array of behaviour entries the vehicle code keeps (the rest of the layout is unknown).
template <class T> struct FixedSizeArray {
    T* mData;   // +0x00: the new[] block the destructor destroys
};
// 0x101355dc (96 bytes): FixedSizeArray<BehaviourEntry>::~FixedSizeArray(short deleteFlag). Does nothing for NULL; else destroys the block at mData
// (__destroy_new_array(mData, 0)) and, when the hidden flag (low 16 bits, signed) is above 0, deletes the object. Answers this.
FixedSizeArray<BehaviourEntry>* FixedSizeArray_BehaviourEntry_Dtor(FixedSizeArray<BehaviourEntry>* self, int16_t deleteFlag);
// HOST SUPPORT: the runtime imports the destructor calls (glue 0x10238ad0 and 0x10238218); weak defaults live in the src file.
void FixedSizeArrayDestroyNewArray(void* array, int32_t destructor);
void FixedSizeArrayOperatorDelete(void* object);

// ---- Unit v51064 (src/CPerson_..._LOADVAR_CMsgPeepArrived_GetType.cpp). The Mac destructors store the vtable (not modelled) and delete when the hidden
// halfword flag is above 0; the behaviours have no virtual destructor, so these are free functions that answer self.
Arrival* Arrival_Dtor(Arrival* self, int16_t deleteFlag);          // 0x10135474
Seek* Seek_Dtor(Seek* self, int16_t deleteFlag);                   // 0x101354f0
Behaviour* Behaviour_Dtor(Behaviour* self, int16_t deleteFlag);    // 0x10135568
void BehaviourEntry_Ctor(BehaviourEntry* self);                    // 0x1013567c: both words cleared
