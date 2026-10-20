// vehicle.cpp, unit w230: save/load and __sinit_vehicle_cpp. Native runtime boundaries below are support glue.
#include "VehicleSteer.h"
#include "Serialise.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

namespace {
void SwapForFile(Vector2d& v) {
    v.x = serialise::SwapWord(v.x);
    v.y = serialise::SwapWord(v.y);
}
void UnswapForFile(Vector2d& v) {
    v.x = serialise::UnswapWord(v.x);
    v.y = serialise::UnswapWord(v.y);
}

// Mac long and bool retain their distinct helper identities without host-long aliasing or bool normalization.
template<class Value, class Field> int32_t Scalar(void* file, Field& field, const char* name, int32_t save) {
    static_assert(sizeof(Value) == sizeof(Field), "serialized scalar width");
    if constexpr (std::is_same<Value, Field>::value) {
        return save ? SAVEVAR<Value>(file, &field, name, 0) : LOADVAR<Value>(file, &field, name, 0);
    }
    Value value;
    std::memcpy(&value, &field, sizeof(value));
    int32_t result = save ? SAVEVAR<Value>(file, &value, name, 0) : LOADVAR<Value>(file, &value, name, 0);
    if (!save) std::memcpy(&field, &value, sizeof(value));
    return result;
}
int32_t Vector(void* file, Vector2d& field, const char* name, int32_t save) {
    return save ? SAVEVARSTRUCT(file, &field, name, 0) : LOADVARSTRUCT(file, &field, name, 0);
}

Seek gSeek;
Arrival gArrival;
Separation gSeparation;
Avoid_Obstacle gAvoidObstacle;
Repel_Obstacle gRepelObstacle;
Avoid_Walls gAvoidWalls;
Follow_Path gFollowPath;
VehicleRegistrationNode gListNode, gSumNode, gObjectNodes[7];

// Placement construction keeps each real native class identity; it leaves the un-copied name tail alone.
template<class Type> void InitializeBehaviour(Type& object, const char* name, VehicleRuntimeCallback destructor, unsigned index) {
    new (&object) Type;
    VehicleRuntimeCopyName(object.mName, name);
    VehicleRuntimeRegister(&object, destructor, &gObjectNodes[index]);
}
} // namespace

// 0x10134a74, 268 B and 0x10134bc0, 252 B. Each Fixed is converted separately.
template<> int32_t SAVEVARSTRUCT<Vector2d>(void* file, Vector2d* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(8, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    Vector2d copy = *value;
    GAME_ASSERTMSG(1, "should be a struct");
    SwapForFile(copy);
    if (!LbFile_Write(file, &copy, 8, &written).Succeeded() || written != 8) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}
template<> int32_t LOADVARSTRUCT<Vector2d>(void* file, Vector2d* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(8, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, 8, &read).Succeeded() || read != 8) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should be a struct");
    UnswapForFile(*value);
    return 0;
}
// 0x10134cfc, 236 B and 0x10134e20, 260 B. NavMode is an unvalidated signed word.
template<> int32_t LOADVAR<NavMode>(void* file, NavMode* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<NavMode>(void* file, NavMode* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

// 0x10130eb4, 2212 B. Every failure is canonicalized, including negative helper failures.
int32_t Vehicle::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version != 0 && version != 1) return 0;
    if (Vector(file, mF40, "force", save)) return 1;
    if (Vector(file, mF38, "formation_pos", save)) return 1;
    if (!save) mF34 = 0;
    if (Vector(file, mForward, "local_xaxis", save)) return 1;
    if (Vector(file, mSide, "local_yaxis", save)) return 1;
    if (Scalar<MacLong>(file, mF00, "mass", save)) return 1;
    if (Scalar<MacLong>(file, mMaxForce, "max_force", save)) return 1;
    if (Scalar<MacLong>(file, mMaxSpeed, "max_speed", save)) return 1;
    if (Scalar<int32_t>(file, mUnreachable, "mCantReachDest", save)) return 1;
    if (Scalar<NavMode>(file, mNavMode, "nav_mode", save)) return 1;
    if (Scalar<int32_t>(file, mNumLoaded, "path_buffer_count", save)) return 1;
    if (Scalar<int32_t>(file, mWaypoint, "path_count", save)) return 1;
    if (Scalar<bool>(file, mPathFlag, "path_finished", save)) return 1;
    if (Scalar<int32_t>(file, mLastProgress, "path_last_progress", save)) return 1;
    if (Scalar<MacULong>(file, mStuckHistory, "path_stuck_buffer", save)) return 1;
    if (Scalar<int32_t>(file, mPathLegA, "path_subpath_dist", save)) return 1;
    if (Scalar<int32_t>(file, mPathLegB, "path_tail_dist", save)) return 1;
    if (Vector(file, mDestination, "path_target_pos", save)) return 1;
    if (Scalar<MacULong>(file, mPathStamp, "path_timestamp", save)) return 1;
    if (Scalar<MacLong>(file, mNumWaypoints, "path_total_count", save)) return 1;
    if (Scalar<int32_t>(file, mPathTotal, "path_total_dist", save)) return 1;
    if (Vector(file, mPos, "pos", save)) return 1;
    if (Scalar<MacLong>(file, mRadius, "radius", save)) return 1;
    for (int32_t p = 0; p < 5; ++p) {
        if (Vector(file, mWaypoints[p], "subpath_buffer[p]", save)) return 1;
        if (Scalar<int32_t>(file, mLegLength[p], "subpath_dist[p]", save)) return 1;
    }
    if (!save) {
        mF30 = 0;
        mTarget = nullptr;
    }
    return Vector(file, mVelocity, "vel", save) ? 1 : 0;
}

Behaviour* gVehBehaviourTable[8] = {&gSeek, &gArrival, &gSeparation, &gAvoidObstacle, &gRepelObstacle, &gAvoidWalls, &gFollowPath, nullptr};

// 0x10134f5c, 616 B. Ten allocated entries, but an end token eighty entries from data, as in the original.
void InitVehicleGlobals() {
    using Callback = VehicleRuntimeCallback;
    gVehZero = 0;
    gVehOne = 0x10000;
    void* raw = VehicleRuntimeAllocate(0x58);
    BehaviourEntry* data = VehicleRuntimeConstruct(raw, Callback::EntryConstructor, nullptr, 8, 10);
    gVehBehaviours.mData = data;
    gVehBehaviours.mTop = data;
    gVehBehaviours.mEnd = reinterpret_cast<BehaviourEntry*>(reinterpret_cast<uintptr_t>(data) + 80 * sizeof(BehaviourEntry));
    VehicleRuntimeRegister(&gVehBehaviours, Callback::ListDestructor, &gListNode);
    gVehSteerSum.x = 0;
    gVehSteerSum.y = 0;
    VehicleRuntimeRegister(&gVehSteerSum, Callback::VectorDestructor, &gSumNode);
    InitializeBehaviour(gSeek, "seek", Callback::SeekDestructor, 0);
    InitializeBehaviour(gArrival, "arrival", Callback::ArrivalDestructor, 1);
    InitializeBehaviour(gSeparation, "separation", Callback::SeparationDestructor, 2);
    InitializeBehaviour(gAvoidObstacle, "avoid_obstacle", Callback::AvoidObstacleDestructor, 3);
    InitializeBehaviour(gRepelObstacle, "repel_obstacle", Callback::RepelObstacleDestructor, 4);
    InitializeBehaviour(gAvoidWalls, "avoid_walls", Callback::AvoidWallsDestructor, 5);
    InitializeBehaviour(gFollowPath, "follow_path", Callback::FollowPathDestructor, 6);
}

#ifndef TPW_VEHICLE_UNIT_RUNTIME
namespace {
void* gEntryAllocation;
void CleanupVehicleEntries() { std::free(gEntryAllocation); }
}
// HOST SUPPORT: native array storage, default initialization, and cleanup. The original callbacks are not executed.
void* VehicleRuntimeAllocate(uint32_t originalBytes) {
    (void)originalBytes;
    gEntryAllocation = std::malloc(8 + 10 * sizeof(BehaviourEntry));
    return gEntryAllocation;
}
BehaviourEntry* VehicleRuntimeConstruct(void* raw, VehicleRuntimeCallback, const VehicleRuntimeCallback*, uint32_t, uint32_t count) {
    auto* entries = reinterpret_cast<BehaviourEntry*>(static_cast<unsigned char*>(raw) + 8);
    for (uint32_t i = 0; i < count; ++i) new (&entries[i]) BehaviourEntry{};
    return entries;
}
void VehicleRuntimeRegister(void*, VehicleRuntimeCallback destructor, VehicleRegistrationNode*) {
    if (destructor == VehicleRuntimeCallback::ListDestructor) std::atexit(CleanupVehicleEntries);
}
char* VehicleRuntimeCopyName(char* destination, const char* source) { return std::strcpy(destination, source); }
namespace {
struct VehicleBootstrap { VehicleBootstrap() { InitVehicleGlobals(); } } gVehicleBootstrap;
}
#endif
