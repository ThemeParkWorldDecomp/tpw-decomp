// Unit w230: recording scalar, file, assertion and runtime fakes. The six owned bodies are real.
#include <cstdint>
#include <cstring>
#include <string>
#include <typeinfo>
#include <new>
#include "calllog.h"
#include "platform/arena.h"
#include "VehicleSteer.h"
#include "Serialise.h"
using namespace calllog;
Fixed gVehZero, gVehOne;
Fixed gVehAvoidRange, gVehAvoidNearest; // Unreferenced startup neighbors must remain untouched.
Vector2d gVehSteerSum;
BehaviourList gVehBehaviours;
CSaveLoad gSaveLoad;
namespace {
Vehicle* gVehicle;
unsigned char* gRaw;
BehaviourEntry* gConstructed;
VehicleRegistrationNode* gNodes[10];
uint8_t gFile[512];
uint32_t gFileSize, gCursor;
bool gPriming;
std::string Hex(const void* p, uint32_t n) {
    static const char digits[] = "0123456789abcdef";
    const auto* bytes = static_cast<const uint8_t*>(p);
    std::string out;
    for (uint32_t i = 0; i < n; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}
int Type(const Behaviour* p) {
    if (typeid(*p) == typeid(Seek)) return 0;
    if (typeid(*p) == typeid(Arrival)) return 1;
    if (typeid(*p) == typeid(Separation)) return 2;
    if (typeid(*p) == typeid(Avoid_Obstacle)) return 3;
    if (typeid(*p) == typeid(Repel_Obstacle)) return 4;
    if (typeid(*p) == typeid(Avoid_Walls)) return 5;
    if (typeid(*p) == typeid(Follow_Path)) return 6;
    return -1;
}
// Offsets of native temporary scalar copies are mapped by their pre-call value among the distinct case fields, independently of names.
int Offset(const void* p, int kind) {
    if (!gVehicle) return 0;
    uintptr_t address = reinterpret_cast<uintptr_t>(p), base = reinterpret_cast<uintptr_t>(gVehicle);
    if (address >= base && address < base + 0xc8) return static_cast<int>(address - base);
    if (kind == 3) return 0x60;
    const int longOffsets[] = {0, 4, 0x18, 0x1c, 0x54};
    const int ulongOffsets[] = {0x48, 0xb0};
    const int* offsets = kind == 1 ? longOffsets : ulongOffsets;
    int count = kind == 1 ? 5 : 2;
    for (int i = 0; i < count; ++i)
        if (std::memcmp(p, reinterpret_cast<const uint8_t*>(gVehicle) + offsets[i], 4) == 0) return offsets[i];
    return -1;
}
template<class V> int32_t Var(void* file, V* value, const char* name, int32_t noLog, int kind, bool save) {
    uint32_t word = 0;
    std::memcpy(&word, value, sizeof(V));
    const int offset = Offset(value, kind);
    const int32_t answer = static_cast<int32_t>(ret("scalar_result"));
    const uint32_t loaded = static_cast<uint32_t>(ret("loaded"));
    if (!save) std::memcpy(value, &loaded, sizeof(V));
    ev("scalar", {kind, save, lab(file), offset, noLog, word, save ? word : (sizeof(V) == 1 ? loaded & 255 : loaded), answer}, {name});
    return answer;
}
}
#define VAR(TypeName, Kind) \
template<> int32_t LOADVAR<TypeName>(void* f, TypeName* v, const char* n, int32_t l) { return Var(f,v,n,l,Kind,false); } \
template<> int32_t SAVEVAR<TypeName>(void* f, TypeName* v, const char* n, int32_t l) { return Var(f,v,n,l,Kind,true); }
VAR(MacLong, 1)
VAR(int32_t, 2)
VAR(bool, 3)
VAR(MacULong, 4)
#undef VAR
void GAME_ASSERTMSG(int32_t c, const char* format, ...) { ev("assert", {c}, {format}); }
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) { ev("log", {lab(this), size, state}, {name}); }
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    const int32_t status = static_cast<int32_t>(ret("status"));
    *transferred = static_cast<uint32_t>(ret("transfer", length));
    const uint32_t prefix = static_cast<uint32_t>(ret("prefix", length));
    const uint32_t count = prefix < length ? prefix : length;
    ev("read", {lab(file), gVehicle ? Offset(buffer, 2) : 0, length, status, *transferred, count});
    std::memcpy(buffer, gFile + gCursor, count);
    gCursor += length;
    return TbError(status);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const int32_t status = static_cast<int32_t>(ret("status"));
    *transferred = static_cast<uint32_t>(ret("transfer", length));
    ev("write", {lab(file), length, status, *transferred}, {Hex(buffer, length).c_str()});
    return TbError(status);
}
// Calculate remains outside the unit. Its distinct native implementations provide actual class vtables.
#define CALCULATE(Class) Vector2d Class::calculate(Vehicle& v) { ev("calculate", {lab(this), lab(&v)}); return {}; }
CALCULATE(Seek)
CALCULATE(Arrival)
CALCULATE(Separation)
CALCULATE(Avoid_Obstacle)
CALCULATE(Repel_Obstacle)
CALCULATE(Avoid_Walls)
CALCULATE(Follow_Path)
#undef CALCULATE
void* VehicleRuntimeAllocate(uint32_t size) {
    if (!gPriming) ev("allocate", {size, static_cast<uint32_t>(gVehZero), static_cast<uint32_t>(gVehOne)});
    return gRaw;
}
BehaviourEntry* VehicleRuntimeConstruct(void* raw, VehicleRuntimeCallback ctor, const VehicleRuntimeCallback* dtor, uint32_t stride, uint32_t count) {
    if (!gPriming) ev("construct", {lab(raw), static_cast<int>(ctor), lab(dtor), stride, count});
    return gConstructed;
}
void VehicleRuntimeRegister(void* object, VehicleRuntimeCallback destructor, VehicleRegistrationNode* node) {
    const int callback = static_cast<int>(destructor);
    gNodes[callback] = node;
    if (gPriming) return;
    ev("register", {lab(object), callback, lab(node)});
    if (callback == 1) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(gConstructed);
        ev("list_snapshot", {lab(gVehBehaviours.mData), static_cast<int64_t>((reinterpret_cast<uintptr_t>(gVehBehaviours.mTop)-base)/sizeof(BehaviourEntry)),
            static_cast<int64_t>((reinterpret_cast<uintptr_t>(gVehBehaviours.mEnd)-base)/sizeof(BehaviourEntry))});
    } else if (callback == 2) {
        ev("sum_snapshot", {static_cast<uint32_t>(gVehSteerSum.x), static_cast<uint32_t>(gVehSteerSum.y)});
    } else {
        auto* behaviour = static_cast<Behaviour*>(object);
        ev("object_snapshot", {lab(object), Type(behaviour)}, {Hex(behaviour->mName,32).c_str()});
    }
    ev("node_before", {lab(node)}, {Hex(node,12).c_str()});
    std::memset(node, 0x80 + callback, 12);
}
char* VehicleRuntimeCopyName(char* destination, const char* source) {
    Behaviour* object = nullptr;
    for (int i = 0; i < 7; ++i) if (gVehBehaviourTable[i]->mName == destination) object = gVehBehaviourTable[i];
    if (!gPriming) ev("strcpy", {lab(object), 4, Type(object)}, {source});
    return std::strcpy(destination, source);
}
extern "C" {
uint64_t tpw_vsr_alloc(uint32_t n) { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(n)); }
uint64_t tpw_vsr_addr(int index) {
    if (index == 0) return reinterpret_cast<uintptr_t>(&gVehZero);
    if (index == 1) return reinterpret_cast<uintptr_t>(&gVehOne);
    if (index == 2) return reinterpret_cast<uintptr_t>(&gVehSteerSum);
    if (index == 3) return reinterpret_cast<uintptr_t>(&gVehBehaviours);
    if (index == 4) return reinterpret_cast<uintptr_t>(&gSaveLoad);
    if (index == 5) return reinterpret_cast<uintptr_t>(gVehBehaviourTable);
    if (index == 6) return reinterpret_cast<uintptr_t>(&gVehAvoidRange);
    if (index == 7) return reinterpret_cast<uintptr_t>(&gVehAvoidNearest);
    return 0;
}
uint64_t tpw_vsr_object(int i) { return reinterpret_cast<uintptr_t>(gVehBehaviourTable[i]); }
uint64_t tpw_vsr_node(int i) { return reinterpret_cast<uintptr_t>(gNodes[i]); }
int tpw_vsr_type(int i) { return Type(gVehBehaviourTable[i]); }
void tpw_vsr_prepare(uint64_t raw, uint64_t data) {
    gRaw = reinterpret_cast<unsigned char*>(raw);
    gConstructed = reinterpret_cast<BehaviourEntry*>(data);
    gPriming = true;
    InitVehicleGlobals();
    gPriming = false;
}
void tpw_vsr_file(uint64_t data, uint32_t n, uint64_t vehicle) {
    std::memcpy(gFile, reinterpret_cast<const void*>(data), n);
    gFileSize = n;
    gCursor = 0;
    gVehicle = reinterpret_cast<Vehicle*>(vehicle);
}
uint32_t tpw_vsr_serialise(uint64_t vehicle, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<Vehicle*>(vehicle)->Serialise(reinterpret_cast<void*>(file), save, version, unused);
}
uint32_t tpw_vsr_template(int which, uint64_t file, uint64_t value, const char* name, int32_t noLog) {
    if (which == 0) return SAVEVARSTRUCT(reinterpret_cast<void*>(file), reinterpret_cast<Vector2d*>(value), name, noLog);
    if (which == 1) return LOADVARSTRUCT(reinterpret_cast<void*>(file), reinterpret_cast<Vector2d*>(value), name, noLog);
    if (which == 2) return LOADVAR(reinterpret_cast<void*>(file), reinterpret_cast<NavMode*>(value), name, noLog);
    return SAVEVAR(reinterpret_cast<void*>(file), reinterpret_cast<NavMode*>(value), name, noLog);
}
void tpw_vsr_init() { InitVehicleGlobals(); }
int tpw_vsr_layout(int64_t* out) {
    const int64_t values[] = {sizeof(Vehicle), offsetof(Vehicle,mTarget), sizeof(BehaviourEntry), sizeof(BehaviourList), sizeof(Seek),
        static_cast<int64_t>(reinterpret_cast<uintptr_t>(gVehBehaviourTable[0]->mName)-reinterpret_cast<uintptr_t>(gVehBehaviourTable[0]))};
    std::memcpy(out,values,sizeof(values));
    return 6;
}
}
