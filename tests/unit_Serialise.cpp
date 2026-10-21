// The fakes for the basic serialisation instances (src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp;
// libtpw_unit_Serialise.dylib; see calllog.h) and the test entry points. The file functions
// work on one fake file: the bytes to read come from the test, what is written is recorded
// as hex. A queue value "LbFile_Read:xfer" (or "LbFile_Write:xfer") replaces the transferred
// count and "LbFile_Read" (or "LbFile_Write") the result. GAME_ASSERTMSG and the serialise
// log record their arguments; the instances themselves are the real ones.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"
#include "calllog.h"

using calllog::ev;
using calllog::lab;
using calllog::ret;

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

namespace {

std::vector<uint8_t> gFileData;   // the bytes a read returns
char gFileHandle;                 // the fake file's address (a label for the log)

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    const auto* p = static_cast<const uint8_t*>(data);
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

}  // namespace

CSaveLoad gSaveLoad;

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// A read that fails or comes up short leaves the buffer alone (what it would hold is the
// file's unswapped bytes, a different number on each byte order).
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const uint32_t available = static_cast<uint32_t>(gFileData.size());
    uint32_t count = available < length ? available : length;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        // a successful read fills the buffer (zeros past the end of a short fake file)
        std::vector<uint8_t> bytes(length, 0);
        std::memcpy(bytes.data(), gFileData.data(), available < length ? available : length);
        std::memcpy(buffer, bytes.data(), length);
    }
    return TbError(result);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

extern "C" {

// The address of a global the log labels: "file" or "gSaveLoad".
void* tpw_sv_addr(const char* name) {
    if (std::strcmp(name, "file") == 0) return &gFileHandle;
    if (std::strcmp(name, "gSaveLoad") == 0) return &gSaveLoad;
    return nullptr;
}

void tpw_sv_set_file(const char* data, int32_t size) { gFileData.assign(data, data + size); }

// Runs one instance on `value`: which is 0..5 for the loaders and 6..11 for the savers in
// the order int, bool, unsigned int, unsigned short, char, wchar_t; 12 is the float loader and 13 the float saver; unit w146: 14 and 16 unsigned
// char, 15 and 17 short, 18 and 19 the thing id struct; unit w160: 20 LOADVAR<long>, 21 LOADVAR<unsigned long>, 22 SAVEVAR<long>, 23 SAVEVAR<unsigned long>,
// 24 and 25 LOADVARSTRUCT and SAVEVARSTRUCT of the object id, 26 and 27 of the map id, 28 and 29 of TbTimeStamp.
int32_t tpw_sv_call(int32_t which, void* value, const char* name, int32_t noLog) {
    void* file = &gFileHandle;
    switch (which) {
    case 0: return LOADVAR(file, static_cast<int32_t*>(value), name, noLog);
    case 1: return LOADVAR(file, static_cast<bool*>(value), name, noLog);
    case 2: return LOADVAR(file, static_cast<uint32_t*>(value), name, noLog);
    case 3: return LOADVAR(file, static_cast<uint16_t*>(value), name, noLog);
    case 4: return LOADVAR(file, static_cast<char*>(value), name, noLog);
    case 5: return LOADVAR(file, static_cast<char16_t*>(value), name, noLog);
    case 6: return SAVEVAR(file, static_cast<int32_t*>(value), name, noLog);
    case 7: return SAVEVAR(file, static_cast<bool*>(value), name, noLog);
    case 8: return SAVEVAR(file, static_cast<uint32_t*>(value), name, noLog);
    case 9: return SAVEVAR(file, static_cast<uint16_t*>(value), name, noLog);
    case 10: return SAVEVAR(file, static_cast<char*>(value), name, noLog);
    case 11: return SAVEVAR(file, static_cast<char16_t*>(value), name, noLog);
    case 12: return LOADVAR(file, static_cast<float*>(value), name, noLog);
    case 13: return SAVEVAR(file, static_cast<float*>(value), name, noLog);
    case 14: return LOADVAR(file, static_cast<uint8_t*>(value), name, noLog);
    case 15: return LOADVAR(file, static_cast<int16_t*>(value), name, noLog);
    case 16: return SAVEVAR(file, static_cast<uint8_t*>(value), name, noLog);
    case 17: return SAVEVAR(file, static_cast<int16_t*>(value), name, noLog);
    case 18: return LOADVARSTRUCT(file, static_cast<ThingId*>(value), name, noLog);
    case 19: return SAVEVARSTRUCT(file, static_cast<ThingId*>(value), name, noLog);
    case 20: return LOADVAR(file, static_cast<MacLong*>(value), name, noLog);
    case 21: return LOADVAR(file, static_cast<MacULong*>(value), name, noLog);
    case 22: return SAVEVAR(file, static_cast<MacLong*>(value), name, noLog);
    case 23: return SAVEVAR(file, static_cast<MacULong*>(value), name, noLog);
    case 24: return LOADVARSTRUCT(file, static_cast<ObjectId*>(value), name, noLog);
    case 25: return SAVEVARSTRUCT(file, static_cast<ObjectId*>(value), name, noLog);
    case 26: return LOADVARSTRUCT(file, static_cast<MapId*>(value), name, noLog);
    case 27: return SAVEVARSTRUCT(file, static_cast<MapId*>(value), name, noLog);
    case 28: return LOADVARSTRUCT(file, static_cast<TbTimeStamp*>(value), name, noLog);
    case 29: return SAVEVARSTRUCT(file, static_cast<TbTimeStamp*>(value), name, noLog);
    }
    return -12345;
}

// Unit w160: the Byteswap of the id types; which 0 is the object id (0x100fa688), 1 the thing id (0x10079a0c).
void tpw_sv_byteswap(int32_t which, void* value) {
    if (which == 0) static_cast<ObjectId*>(value)->Byteswap();
    else static_cast<ThingId*>(value)->Byteswap();
}

}  // extern "C"
