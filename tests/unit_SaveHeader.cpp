// Test library for the save file header (libtpw_unit_SaveHeader.dylib; see tests/test_SaveHeader.py, unit w114): CSaveLoad::LoadHeader,
// CSaveLoadHeader::Serialise and the static initialisation of SaveLoad.cpp (gSaveLegalText, gSaveLoad's words), all in
// src/game/park/SkyObjects.cpp, src/game/save/SaveLoad.cpp.
//
// Real: those three, the Bullfrog strings (the legal text table is built with them) and the real gSaveLoad. Fakes that record through
// calllog: LbFile_Read (one fake file made of the case's bytes, read in order), GAME_ASSERTMSG, SamsUtilities::_USwapBlock16 (a no-op on
// this little endian host, as the platform's), TbStringBase<char16_t>::operator!= (logs which table entry and the text it was asked to
// compare, answers from a queue), CWorld::DoIHaveTheseRides (logs the ride record, answers from a queue) and the six LOADVAR/SAVEVAR
// instances Serialise calls (they log the value's place as an offset from the first value of the case and answer from a queue).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "World.h"
#include "calllog.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

constexpr uint32_t kFileLength = 0x1000;
uint8_t gFileData[kFileLength];   // the bytes a read returns
uint32_t gFileSize;
uint32_t gFilePosition;           // how many bytes the successful reads have handed out
char gFileHandle;                 // the fake file's address (a label for the log)
char gWorldBlock[16];             // the world LoadHeader calls DoIHaveTheseRides on (only its address is used)
const void* gVarBase;             // the first value pointer a LOADVAR/SAVEVAR fake saw in the case
int32_t gVarCalls;                // the LOADVAR/SAVEVAR calls so far in the case
int32_t gFailAt;                  // the call (counting from 0) that answers 1; -1 for none

uint64_t Fnv(const void* data, uint32_t size) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ static_cast<const uint8_t*>(data)[i]) * 0x100000001b3ull;
    return hash & 0xffffffffull;
}

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[static_cast<const uint8_t*>(data)[i] >> 4];
        out += kDigits[static_cast<const uint8_t*>(data)[i] & 15];
    }
    return out;
}

// One LOADVAR or SAVEVAR call of Serialise: the file, the value's offset from the first value of the case, noLog, which object the first
// value was in (the test's header has a label; a header on the stack has none) and the name.
int32_t VarEvent(const char* name, void* file, const void* value, const char* text, int32_t noLog) {
    if (gVarBase == nullptr) gVarBase = value;
    const int64_t offset = static_cast<const char*>(value) - static_cast<const char*>(gVarBase);
    ev(name, {lab(file), offset, noLog, lab(gVarBase)}, {text});
    return gVarCalls++ == gFailAt ? 1 : 0;
}

}  // namespace

// String members the unit's sources reference and nothing here reaches.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files
CWorld* gGameWorld;
uint8_t gParkRequiredRides[0x20];

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// (the read position moves only on a read that succeeded; a failed read ends LoadHeader anyway)
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length, gFilePosition});
    const uint32_t available = gFileSize > gFilePosition ? gFileSize - gFilePosition : 0;
    uint32_t count = available < length ? available : length;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::memset(buffer, 0, length);
        std::memcpy(buffer, gFileData + gFilePosition, available < length ? available : length);
        gFilePosition += length;
    }
    return TbError(result);
}

void SamsUtilities::_USwapBlock16(uint16_t*, int32_t count) { ev("_USwapBlock16", {count}); }

// The legal text compare: which table entry (the distance from the table, in entries) and the text the buffer holds (its first 640 units
// as a hash and the first characters).
template<> bool TbStringBase<char16_t>::operator!=(const char16_t* text) const {
    const int64_t index = (reinterpret_cast<intptr_t>(this) - reinterpret_cast<intptr_t>(gSaveLegalText)) /
                          static_cast<int64_t>(sizeof(gSaveLegalText[0]));
    ev("operator!=", {index, static_cast<int64_t>(Fnv(text, 0x500))}, {narrow(text).c_str()});
    return ret("operator!=") != 0;
}

int32_t CWorld::DoIHaveTheseRides(const char* required, int32_t previews) {
    ev("DoIHaveTheseRides", {lab(this), previews, static_cast<int64_t>(Fnv(required, 0x100))}, {Hex(required, 0x20).c_str()});
    return static_cast<int32_t>(ret("DoIHaveTheseRides", 1));
}

template<> int32_t LOADVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return VarEvent("LOADVAR<wchar_t>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return VarEvent("SAVEVAR<wchar_t>", file, value, name, noLog);
}
template<> int32_t LOADVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return VarEvent("LOADVAR<unsigned short>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return VarEvent("SAVEVAR<unsigned short>", file, value, name, noLog);
}
template<> int32_t LOADVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return VarEvent("LOADVAR<float>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return VarEvent("SAVEVAR<float>", file, value, name, noLog);
}

extern "C" {

// The address of a thing the log labels: "file", "world".
void* tpw_sh_addr(const char* name) {
    if (std::strcmp(name, "file") == 0) return &gFileHandle;
    if (std::strcmp(name, "world") == 0) return gWorldBlock;
    return nullptr;
}

// Starts a case: the file's bytes (at most kFileLength), the position back at 0, the world pointer set, the first-value mark cleared, and
// the LOADVAR/SAVEVAR call that fails (counting from 0; -1 for none).
void tpw_sh_begin(const char* data, int32_t size, int32_t failAt) {
    gFailAt = failAt;
    gVarCalls = 0;
    gFileSize = static_cast<uint32_t>(size < static_cast<int32_t>(kFileLength) ? size : kFileLength);
    std::memcpy(gFileData, data, gFileSize);
    gFilePosition = 0;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
    gVarBase = nullptr;
}

// How many bytes the successful reads have handed out.
int32_t tpw_sh_position() { return static_cast<int32_t>(gFilePosition); }

// LoadHeader of the fake file: the error word and the park's required rides start as given and are read back (error, rides).
int32_t tpw_sh_load_header(int32_t type, void* header, int32_t error, uint8_t* rides, int32_t* errorOut) {
    gSaveLoad.mError = error;
    std::memcpy(gParkRequiredRides, rides, 0x20);
    const int32_t result = gSaveLoad.LoadHeader(*reinterpret_cast<bfl::FileSystem::CFile*>(&gFileHandle),
                                                static_cast<CSaveLoad::LoadType>(type), static_cast<CSaveLoadHeader*>(header));
    *errorOut = gSaveLoad.mError;
    std::memcpy(rides, gParkRequiredRides, 0x20);
    return result;
}

// The header's Serialise on the fake file.
int32_t tpw_sh_serialise(void* header, int32_t save, int32_t version, uint32_t unused, int32_t failAt) {
    gVarBase = nullptr;
    gFailAt = failAt;
    gVarCalls = 0;
    return static_cast<CSaveLoadHeader*>(header)->Serialise(&gFileHandle, save, version, unused);
}

// Entry `index` of the legal text table: its length, its units (up to `max`), its capacity.
int32_t tpw_sh_legal(int32_t index, uint16_t* out, int32_t max) {
    const TbDynamicStringTemplate<char16_t>& text = gSaveLegalText[index];
    for (int32_t i = 0; i < text.mLength && i < max; ++i) out[i] = text.mData[i];
    return text.mLength;
}

// The words the static initialisation sets in gSaveLoad (0x1037fefc +0, +4, +0x14, +0x18).
void tpw_sh_saveload(uint32_t* out) {
    out[0] = gSaveLoad.mF00;
    out[1] = static_cast<uint32_t>(gSaveLoad.mError);
    out[2] = gSaveLoad.mF14;
    out[3] = gSaveLoad.mF18;
}

}  // extern "C"
