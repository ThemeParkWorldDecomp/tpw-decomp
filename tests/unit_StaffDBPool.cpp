// Unit w226: recording dependencies for the staff database differential tests.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
namespace { bool KeepFreed(void*, bool) { return true; } }
#define TPW_CALLLOG_EVENTS 12000
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 4096
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"
#include "StaffDB.h"
#include "MainBalance.h"
#include "World.h"
#include "Mouse.h"
#include "UIInterface.h"
#include "HLSprite.h"
#include "Text.h"
#include "TbITable.h"
#include "ThingReceivers.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "LbFile.h"
#include "Debug.h"
using namespace calllog;

// Byte storage avoids constructing unrelated global subsystems.
alignas(16) unsigned char gMainBalanceBytes[sizeof(CMainBalance)] __asm__("_gMainBalance");
alignas(16) unsigned char gTextBytes[sizeof(CText)] __asm__("_gText");
CSaveLoad gSaveLoad;
UIInterface gUIInterface;
CThingArrayEntry gThingArray[kThingArraySize];
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
namespace {
alignas(16) unsigned char worldBytes[sizeof(CWorld)];
alignas(16) unsigned char oldMouse[sizeof(CMouse)];
alignas(16) unsigned char tables[5][8];
alignas(16) unsigned char things[32][0x80];
char16_t thingNames[32][128];
char16_t output[128];
unsigned char fileBytes[4096];
char fileHandle;
uint32_t fileCursor;
CStaffDB* DB() { return reinterpret_cast<CStaffDB*>(worldBytes); }
CStaffInfo* Info(int32_t slot) { return reinterpret_cast<CStaffInfo*>(DB()->mRecords + 20 * slot); }
std::string Hex(const void* p, uint32_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    const auto* bytes = static_cast<const unsigned char*>(p);
    for (uint32_t i = 0; i < n; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}
std::u16string Name(int64_t table, uint32_t id) {
    const std::string text = "T" + std::to_string(table - 20) + ":" + std::to_string(id);
    return std::u16string(text.begin(), text.end());
}
}
CWorld* gWorld = reinterpret_cast<CWorld*>(worldBytes);

// Challenge.cpp requires eleven unrelated symbols. Fake only its StaffType pair,
// recording the field offset, name, value and noLog flag on both machines.
template<> int32_t LOADVAR<StaffType>(void* file, StaffType* value, const char* name, int32_t noLog) {
    ev("stafftype_load", {lab(file), reinterpret_cast<unsigned char*>(value) - worldBytes, noLog, static_cast<uint32_t>(*value)}, {name});
    *value = static_cast<StaffType>(ret("stafftype"));
    return static_cast<int32_t>(ret("stafftype_result"));
}
template<> int32_t SAVEVAR<StaffType>(void* file, StaffType* value, const char* name, int32_t noLog) {
    ev("stafftype_save", {lab(file), reinterpret_cast<unsigned char*>(value) - worldBytes, noLog, static_cast<uint32_t>(*value)}, {name});
    return static_cast<int32_t>(ret("stafftype_result"));
}

// Random draws, sprites, notifications and world iteration record all arguments.
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("rand"));
}
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("sprites", {set});
    return static_cast<int32_t>(ret("sprites"));
}
void UIInterface::StaffDBChange(int32_t slot) { ev("StaffDBChange", {lab(this), slot}); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("head", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head"));
    return result;
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("next", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next"));
    return result;
}
const char16_t* CStaff::GetName() const {
    ev("staff_name", {lab(this)});
    return thingNames[lab(this) - 40];
}
uint32_t CStaff::GetSalary(StaffType type, uint8_t grade) {
    ev("salary", {type, grade});
    return static_cast<uint32_t>(ret("salary"));
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("format", {lab(this), size, id});
    const std::u16string text = Name(lab(this), id).substr(0, size != 0 ? size - 1 : 0);
    for (uint32_t i = 0; i < size; ++i) {
        out[i] = i < text.size() ? text[i] : 0;
        if (out[i] == 0) break;
    }
    return 0;
}
int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count) {
    ev("compare", {}, {narrow(a).c_str(), narrow(b).c_str()});
    for (uint32_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        if (a[i] == 0) return 0;
    }
    return 0;
}
void TRACE(const char* text, ...) { ev("TRACE", {}, {text}); }
void CTRACE(uint32_t level, const char* text, ...) { ev("CTRACE", {level}, {text}); }
void GAME_ASSERTMSG(int32_t value, const char* text, ...) { ev("assert", {value}, {text}); }
void DEBUG_MESSAGE(const char* text, uint32_t flags) { ev("debug", {flags}, {text}); }

// File bytes are the real templates' little endian stream, including noncanonical flag bytes.
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("serialise", {lab(this), size, state}, {name});
}
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("read", {lab(file), length}, {Hex(fileBytes + fileCursor, length).c_str()});
    const int64_t forced = ret("xfer", -1);
    *transferred = forced < 0 ? length : static_cast<uint32_t>(forced);
    const int32_t result = static_cast<int32_t>(ret("io"));
    if (result == 0 && *transferred == length) std::memcpy(buffer, fileBytes + fileCursor, length);
    fileCursor += length;
    return TbError(result);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("xfer", -1);
    *transferred = forced < 0 ? length : static_cast<uint32_t>(forced);
    return TbError(static_cast<int32_t>(ret("io")));
}
int32_t LbFile_GetPosition(void* file) {
    ev("position", {lab(file)});
    return static_cast<int32_t>(ret("position"));
}

// Allocation events distinguish the three classes, whose host and Mac sizes differ.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ev("new", {size == sizeof(CMouse) ? 1 : size == sizeof(CMMDefault) ? 2 : 3});
    if (ret("new", 1) == 0) return nullptr;
    return std::calloc(1, size);
}
CMMDefault::CMMDefault() { ev("default", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
CSpritePlaceMode::~CSpritePlaceMode() {}
void CSpritePlaceMode::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CSpritePlaceMode::OnRelease() {}
CMMPlaceStaff::CMMPlaceStaff(int32_t model, uint8_t grade, uint8_t subtype, int32_t name) {
    ev("place", {newlab(this), model, grade, subtype, name});
}
CMMPlaceStaff::~CMMPlaceStaff() {}
void CMMPlaceStaff::OnLeftClick(int32_t, int32_t) {}
int32_t CMMPlaceStaff::GetMode() { return 5; }
void CMMPlaceStaff::OnAssign() {}
void CMMPlaceStaff::OnRelease() {}
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("assign", {lab(this), lab(mode)}); }

extern "C" {
uint64_t tpw_sdp_addr(int32_t which) {
    const void* entries[] = {worldBytes, gStaffNameScratch, gStaffGeneratedName, output, &gSaveLoad, &fileHandle, &gUIInterface,
                            oldMouse, fileBytes, &gMouseWord1E0, &gMouseWord1E4, &gStaffDBWord6E0, &gStaffDBWord6E4};
    return reinterpret_cast<uintptr_t>(entries[which]);
}
uint64_t tpw_sdp_table(int32_t i) { return reinterpret_cast<uintptr_t>(tables[i]); }
uint64_t tpw_sdp_thing(int32_t i) { return reinterpret_cast<uintptr_t>(things[i]); }
uint64_t tpw_sdp_thing_name(int32_t i) { return reinterpret_cast<uintptr_t>(thingNames[i]); }
void tpw_sdp_row(int32_t id, uint64_t thing) { gThingArray[id].mThing = reinterpret_cast<void*>(thing); }
void tpw_sdp_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
void tpw_sdp_reset(int32_t mouse) {
    fileCursor = 0;
    gWorld = reinterpret_cast<CWorld*>(worldBytes);
    gMouse = mouse != 0 ? reinterpret_cast<CMouse*>(oldMouse) : nullptr;
    for (int32_t i = 0; i < 5; ++i) gText.mResourcePointers[11 + i] = tables[i];
}
int32_t tpw_sdp_mouse() { return gMouse == nullptr ? 0 : gMouse == reinterpret_cast<CMouse*>(oldMouse) ? 1 : 2; }
int32_t tpw_sdp_layout() { return sizeof(CStaffDB) == 0x2a0 && sizeof(CStaffInfo) == 20 && offsetof(CWorld, mTurn) == 0x1da70c; }
void tpw_sdp_dtor(int32_t slot, int32_t flag) {
    if (flag > 0) delete Info(slot);
    else Info(slot)->~CStaffInfo();
}
int32_t tpw_sdp_info_save(int32_t slot, int32_t save, int32_t version, uint32_t unused) { return Info(slot)->Serialise(&fileHandle, save, version, unused); }
void tpw_sdp_name(int32_t slot, int32_t size) { Info(slot)->GetName(output, size); }
void tpw_sdp_genname(int32_t slot) { Info(slot)->GenerateName(); }
void tpw_sdp_generate(int32_t slot, int32_t type) { Info(slot)->Generate(type, slot); }
uint32_t tpw_sdp_salary(int32_t slot) { return Info(slot)->GetSalary(); }
void tpw_sdp_valid(int32_t slot, int32_t value, int32_t id) { Info(slot)->SetValid(value != 0, id); }
int32_t tpw_sdp_db_save(int32_t save, int32_t version, uint32_t unused) { return DB()->Serialise(&fileHandle, save, version, unused); }
int32_t tpw_sdp_get(uint32_t slot) { return DB()->GetStaff(slot) == nullptr ? -1 : slot; }
int32_t tpw_sdp_hire(uint32_t slot) { return DB()->HireStaff(slot); }
void tpw_sdp_cap() { DB()->EnforceCap(); }
void tpw_sdp_count() { DB()->CountStaffTypes(); }
void tpw_sdp_minimum() { DB()->EnsureMinimumAmountInPool(); }
int32_t tpw_sdp_genstaff(int32_t type) { return DB()->GenerateStaff(type); }
int32_t tpw_sdp_numtype(int32_t type) { return DB()->GetNumStaffOfType(type); }
int32_t tpw_sdp_num() { return DB()->GetNumStaff(); }
void tpw_sdp_fate(int32_t fate) { DB()->FateOfPickedUpStaff(static_cast<CStaffDB::StaffFate>(fate)); }
int32_t tpw_sdp_inuse() { return DB()->IsNameInUse(output); }
uint32_t tpw_sdp_free() { return DB()->GetFirstFreeSlot(); }
void tpw_sdp_opening() { DB()->GenerateOpeningStaffPool(); }
void tpw_sdp_sinit(int32_t* out) { out[0] = gStaffDBWord6E0; out[1] = gStaffDBWord6E4; }
}
