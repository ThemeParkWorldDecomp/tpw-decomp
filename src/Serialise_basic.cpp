// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"

// Loaders: 0x1000ef18 (int), 0x1000f144 (bool), 0x100f44a4 (unsigned int), 0x100f5074
// (unsigned short), 0x1016d2c8 (char), 0x10111844 (wchar_t).
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000ef18, 236 bytes
}
template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000f144, 228 bytes
}
template<> int32_t LOADVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f44a4, 236 bytes
}
template<> int32_t LOADVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f5074, 236 bytes
}
template<> int32_t LOADVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1016d2c8, 228 bytes
}
template<> int32_t LOADVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x10111844, 236 bytes
}

// Savers: 0x1000f4b0 (int), 0x1000f70c (bool), 0x100f45bc (unsigned int), 0x100f5530
// (unsigned short), 0x1016d3d4 (char), 0x10111a98 (wchar_t).
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f4b0, 260 bytes
}
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f70c, 236 bytes
}
template<> int32_t SAVEVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f45bc, 260 bytes
}
template<> int32_t SAVEVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f5530, 252 bytes
}
template<> int32_t SAVEVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1016d3d4, 236 bytes
}
template<> int32_t SAVEVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x10111a98, 252 bytes
}

// Unit w146. The one-byte and two-byte instances MapDelta.cpp's serialisers use: LOADVAR and SAVEVAR of unsigned char (0x100f17c8, 228 bytes;
// 0x100f1a04, 236 bytes) and short (0x100f518c, 236 bytes; 0x100f5658, 252 bytes). The shared bodies are the same as the others': the listings
// differ only in the byte size, the swap (a byte is not swapped) and the order of the two asserts.
template<> int32_t LOADVAR<uint8_t>(void* file, uint8_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f17c8, 228 bytes
}
template<> int32_t SAVEVAR<uint8_t>(void* file, uint8_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f1a04, 236 bytes
}
template<> int32_t LOADVAR<int16_t>(void* file, int16_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f518c, 236 bytes
}
template<> int32_t SAVEVAR<int16_t>(void* file, int16_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f5658, 252 bytes
}

// The thing id (a halfword in a struct): the file's order is little-endian, so the two bytes are exchanged on the way in and out (the listings'
// sthbrx). The Mac's swap functions are inlined into the instances.
namespace {
using ThingIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}
static void SwapForFile(ThingIdStruct& id) { id.mIndex = serialise::SwapWord(id.mIndex); }
static void UnswapForFile(ThingIdStruct& id) { id.mIndex = serialise::UnswapWord(id.mIndex); }

template<> int32_t LOADVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x1000eb4c, 224 bytes
}
template<> int32_t SAVEVARSTRUCT<ThingIdStruct>(void* file, ThingIdStruct* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x1000e9d4, 240 bytes
}

// Unit w160. LOADVAR and SAVEVAR of the Mac's long (0x1000ee04, 0x1000f384) and unsigned long (0x1000f02c, 0x1000f5dc), four bytes each (MacLong and
// MacULong in Serialise.h); the listings are the int instances' (the log call, the file call, the failure assert, the two success asserts in
// the order "should not be a struct" then "dummy assert", the reversal of the four bytes).
template<> int32_t LOADVAR<MacLong>(void* file, MacLong* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000ee04, 236 bytes
}
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x1000f02c, 236 bytes
}
template<> int32_t SAVEVAR<MacLong>(void* file, MacLong* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f384, 260 bytes
}
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x1000f5dc, 260 bytes
}

// The object id and the map id (halfwords in structs, swapped like the thing id: lhz; sthbrx / the rlwinm pair) and TbTimeStamp. The Mac keeps a
// TbTimeStamp big-endian (high word at +0) and the file holds the 8 bytes little-endian: the listings reverse the whole value through the shift
// helpers 0x10237f50 and 0x10237f78 (masks 0xff0000, 0xff000000, 0xff00, 0xff on each half), stored as stw low word at +4, high word at +0; here
// serialise::SwapWord reverses the 8 bytes (WordOf<8> in Serialise.h).
namespace {
using ObjectIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using MapIdStruct = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
}
static void SwapForFile(ObjectIdStruct& id) { id.mIndex = serialise::SwapWord(id.mIndex); }
static void UnswapForFile(ObjectIdStruct& id) { id.mIndex = serialise::UnswapWord(id.mIndex); }
static void SwapForFile(MapIdStruct& id) { id.mIndex = serialise::SwapWord(id.mIndex); }
static void UnswapForFile(MapIdStruct& id) { id.mIndex = serialise::UnswapWord(id.mIndex); }
static void SwapForFile(TbTimeStamp& stamp) { stamp.mTicks = serialise::SwapWord(stamp.mTicks); }
static void UnswapForFile(TbTimeStamp& stamp) { stamp.mTicks = serialise::UnswapWord(stamp.mTicks); }

template<> int32_t LOADVARSTRUCT<ObjectIdStruct>(void* file, ObjectIdStruct* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f1300, 224 bytes
}
template<> int32_t SAVEVARSTRUCT<ObjectIdStruct>(void* file, ObjectIdStruct* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f1188, 240 bytes
}
template<> int32_t LOADVARSTRUCT<MapIdStruct>(void* file, MapIdStruct* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f53cc, 224 bytes
}
template<> int32_t SAVEVARSTRUCT<MapIdStruct>(void* file, MapIdStruct* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f58c0, 240 bytes
}
template<> int32_t LOADVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);   // 0x100f1468, 516 bytes
}
template<> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* file, TbTimeStamp* value, const char* name, int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);   // 0x100f1b1c, 536 bytes
}

// The id types' Byteswap: the halfword's two bytes exchanged in place (lhz; sthbrx; blr).
template<> void tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>::Byteswap() {
    mIndex = __builtin_bswap16(mIndex);   // 0x100fa688, 12 bytes
}
template<> void tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>::Byteswap() {
    mIndex = __builtin_bswap16(mIndex);   // 0x10079a0c, 12 bytes
}
