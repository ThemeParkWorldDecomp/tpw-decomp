// The save-file serialisation templates (LOADVAR, SAVEVAR, LOADVARSTRUCT, SAVEVARSTRUCT).
// The instances for the basic types are in the Advisor.cpp range of the inventory and
// elsewhere. Translated in src/Serialise_basic.cpp: LOADVAR and SAVEVAR of int (0x1000ef18,
// 0x1000f4b0), bool (0x1000f144, 0x1000f70c), unsigned int (0x100f44a4, 0x100f45bc),
// unsigned short (0x100f5074, 0x100f5530), char (0x1016d2c8, 0x1016d3d4) and wchar_t, which
// is char16_t here (0x10111844, 0x10111a98); unit w146 adds unsigned char (0x100f17c8, 0x100f1a04),
// short (0x100f518c, 0x100f5658) and LOADVARSTRUCT / SAVEVARSTRUCT of the thing id
// (tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>: 0x1000eb4c, 0x1000e9d4); CMapCell::LitterType
// (0x10103af0, 0x10103c30) is in src/MapDelta_save.cpp. Unit w160 adds LOADVAR / SAVEVAR of the Mac's long
// (MacLong: 0x1000ee04, 0x1000f384) and unsigned long (MacULong: 0x1000f02c, 0x1000f5dc), LOADVARSTRUCT /
// SAVEVARSTRUCT of the object id (0x100f1300, 0x100f1188), the map id (0x100f53cc, 0x100f58c0) and TbTimeStamp
// (0x100f1468, 0x100f1b1c); unit w179 adds LOADVAR / SAVEVAR of CELL_MODES (0x100f52a0, 0x100f577c) and LOADVARSTRUCT / SAVEVARSTRUCT of
// CCellBase::CELL_IndexType (0x100f4f0c, 0x100f4d70) in src/CellBase.cpp; files that use their
// own types instantiate them (TPSettings.cpp). The shared bodies are the serialise::
// templates below; an instance is an explicit specialization calling them, so the
// instances stay separate symbols.
//
// Values are stored little-endian (the PC's order): the Mac version swaps them (4 bytes
// reversed, 2 bytes swapped, 1 byte left alone). A struct (the ...STRUCT forms) converts its
// own fields with SwapForFile(Struct&). Results: 0 on success, 1 on failure.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"

// The Mac's long and unsigned long (four bytes; the host's are eight), for the instances LOADVAR / SAVEVAR<long> 0x1000ee04, 0x1000f384 and
// <unsigned long> 0x1000f02c, 0x1000f5dc (unit w160). No translated caller uses them yet: the existing callers keep the int32 instances
// (the same file bytes).
enum MacLong : int32_t {};
enum MacULong : uint32_t {};

template<class Value> int32_t LOADVAR(void* file, Value* value, const char* name, int32_t noLog);
template<class Value> int32_t SAVEVAR(void* file, Value* value, const char* name, int32_t noLog);
template<class Value> int32_t LOADVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);
template<class Value> int32_t SAVEVARSTRUCT(void* file, Value* value, const char* name, int32_t noLog);

namespace serialise {

// A 1-, 2- or 4-byte value between the host's order and the file's (little-endian). The
// Mac swaps 4 bytes by reversing, 2 bytes by exchanging the halves and leaves 1 byte alone.
template<size_t Size> struct WordOf;
template<> struct WordOf<1> { using Type = uint8_t; };
template<> struct WordOf<2> { using Type = uint16_t; };
template<> struct WordOf<4> { using Type = uint32_t; };
template<> struct WordOf<8> { using Type = uint64_t; };   // unit w160: TbTimeStamp's tick count (whole value byte-reversed)

template<class Value> Value SwapWord(Value value) {
    static_assert(sizeof(Value) == 1 || sizeof(Value) == 2 || sizeof(Value) == 4 || sizeof(Value) == 8, "a 1, 2, 4 or 8-byte value");
    typename WordOf<sizeof(Value)>::Type word;
    uint8_t bytes[sizeof(Value)];
    std::memcpy(&word, &value, sizeof(Value));
    for (size_t i = 0; i < sizeof(Value); ++i) bytes[i] = static_cast<uint8_t>(word >> (8 * i));
    std::memcpy(&value, bytes, sizeof(Value));
    return value;
}
template<class Value> Value UnswapWord(Value value) {
    static_assert(sizeof(Value) == 1 || sizeof(Value) == 2 || sizeof(Value) == 4 || sizeof(Value) == 8, "a 1, 2, 4 or 8-byte value");
    typename WordOf<sizeof(Value)>::Type word = 0;
    uint8_t bytes[sizeof(Value)];
    std::memcpy(bytes, &value, sizeof(Value));
    for (size_t i = 0; i < sizeof(Value); ++i) word = static_cast<decltype(word)>(word | static_cast<decltype(word)>(bytes[i]) << (8 * i));
    std::memcpy(&value, &word, sizeof(Value));
    return value;
}

template<class Value> int32_t LoadVar(void* file, Value* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(sizeof(Value), name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, sizeof(Value), &read).Succeeded() || read != sizeof(Value)) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where LOADVARSTRUCT should be used instead");
    // a byte is not swapped (and a bool is never normalised: a file byte of 2 stays 2)
    if constexpr (sizeof(Value) > 1) *value = UnswapWord(*value);
    return 0;
}

template<class Value> int32_t SaveVar(void* file, Value* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(sizeof(Value), name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    Value copy;
    std::memcpy(&copy, value, sizeof(Value));   // a copy of the bytes: the saved value is not changed
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where SAVEVARSTRUCT should be used instead");
    if constexpr (sizeof(Value) > 1) copy = SwapWord(copy);
    if (!LbFile_Write(file, &copy, sizeof(Value), &written).Succeeded() || written != sizeof(Value)) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}

template<class Struct> int32_t LoadStruct(void* file, Struct* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(sizeof(Struct), name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, sizeof(Struct), &read).Succeeded() || read != sizeof(Struct)) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should be a struct");
    UnswapForFile(*value);
    return 0;
}

template<class Struct> int32_t SaveStruct(void* file, Struct* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(sizeof(Struct), name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    Struct copy = *value;
    GAME_ASSERTMSG(1, "should be a struct");
    SwapForFile(copy);
    if (!LbFile_Write(file, &copy, sizeof(Struct), &written).Succeeded() || written != sizeof(Struct)) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}

}  // namespace serialise

// Unit w230 (src/Vehicle_serialise.cpp).
struct Vector2d;
enum NavMode : int32_t;
template<> int32_t SAVEVARSTRUCT<Vector2d>(void*, Vector2d*, const char*, int32_t); // 0x10134a74, 268 B
template<> int32_t LOADVARSTRUCT<Vector2d>(void*, Vector2d*, const char*, int32_t); // 0x10134bc0, 252 B
template<> int32_t LOADVAR<NavMode>(void*, NavMode*, const char*, int32_t); // 0x10134cfc, 236 B
template<> int32_t SAVEVAR<NavMode>(void*, NavMode*, const char*, int32_t); // 0x10134e20, 260 B

// Unit v5045 (src/ParkMessages_SAVEVAR.cpp).
enum ResearchCat : int32_t;
template<> int32_t SAVEVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t); // 0x10124680, 260 B
template<> int32_t LOADVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t); // 0x10124554, 236 B (unit v51002 tests it)
