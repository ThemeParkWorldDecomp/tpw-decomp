// Test library for the master save file (libtpw_unit_MasterSave.dylib; see tests/test_MasterSave.py): src/FS_MasterSave_file.cpp (Save, Load,
// Serialise) and src/FS_MasterSave_park.cpp (the park record's loader constructor and Serialise) of unit w89b, over the master save and
// park record of units w32 and w89 (all real, with the std containers), the bullfrog strings (real) and the 16-bit C library.
//
// Fakes that record through calllog: the serialisation instances of the basic types (bool, int, unsigned int, unsigned short, char,
// char16_t) which read and write one fake file, CPlayerSettings::SerialiseLocal (tested by test_TPSettings), LbFile_Open and LbFile_Close,
// CTRACE, SwearFilter::Init, CSYS_TPFileInfo::GetDataPath and CText::GetLanguageName. The balance table and the string table that the
// ticket code of unit w89 links with are fakes that only record.
//
// The Python module builds a world (a master save with theme records, a set of bought things, the global gMasterSave) with tpw_mv_build,
// calls one function with tpw_mv_call and reads the state with tpw_mv_dump.
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "LbFile.h"
#include "RideDB.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "calllog.h"
#include "platform/msl.h"

using namespace calllog;

// ---- globals of other files
CText gText;
CSYS_TPFileInfo gTPFileInfo;
CFSGlobalMasterSave* gMasterSave;
CRideDatabase gRideDatabase;
CPlayerSettings gPlayerSettings;   // the fake Init below leaves it as it is

// (the text object CText holds by value: its members are never used here, but its constructor and destructor must link)
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// (the ride records are never made here)
RideInfo::~RideInfo() {}

// The player settings are only a label here: the constructor, destructor and Init do nothing.
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

namespace {

// ---- the fake file: the bytes the reads return (set by the test), the position, and the one handle LbFile_Open hands out
std::string gFileData;
std::size_t gFilePosition;
char gFileHandle[16];
char gLanguageName[64];   // CText::GetLanguageName's text
constexpr int kLabelMaster = 1;
constexpr int kLabelOther = 2;
constexpr int kLabelSettings = 3;
constexpr int kLabelText = 4;
constexpr int kLabelFileInfo = 5;
constexpr int kLabelFile = 8;

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    const auto* p = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

// Reads up to `size` bytes; the count read.
uint32_t ReadFile(void* dest, uint32_t size) {
    const std::size_t left = gFileData.size() - gFilePosition;
    const uint32_t n = static_cast<uint32_t>(size < left ? size : left);
    if (n != 0) std::memcpy(dest, gFileData.data() + gFilePosition, n);
    gFilePosition += n;
    return n;
}

// A serialisation instance: it records the call, reads or writes `size` bytes of the fake file (little-endian, as the host's order is),
// and answers 0, except at the call number gFailAt (counting the calls that got as far as answering, of every instance together), which
// answers gFailValue. A load that comes up short answers 1 and leaves the value alone (and is not counted).
int64_t gVarCalls;
int64_t gFailAt = -1;
int64_t gFailValue = 1;
int32_t VarResult() {
    const int64_t n = gVarCalls++;
    return n == gFailAt ? static_cast<int32_t>(gFailValue) : 0;
}
template<class Value> int32_t FakeLoad(const char* kind, void* file, Value* value, const char* name, int32_t noLog) {
    ev(kind, {lab(file), noLog}, {name});
    uint8_t bytes[4] = {0, 0, 0, 0};
    if (ReadFile(bytes, sizeof(Value)) != sizeof(Value)) return 1;
    std::memcpy(value, bytes, sizeof(Value));
    return VarResult();
}
template<class Value> int32_t FakeSave(const char* kind, void* file, const Value* value, const char* name, int32_t noLog) {
    ev(kind, {lab(file), noLog}, {name, Hex(value, sizeof(Value)).c_str()});
    return VarResult();
}

}  // namespace

template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<bool>", file, reinterpret_cast<uint8_t*>(value), name, noLog);
}
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<bool>", file, reinterpret_cast<uint8_t*>(value), name, noLog);
}
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<int>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<int>", file, value, name, noLog);
}
template<> int32_t LOADVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<uint>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<uint>", file, value, name, noLog);
}
template<> int32_t LOADVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<ushort>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<uint16_t>(void* file, uint16_t* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<ushort>", file, value, name, noLog);
}
template<> int32_t LOADVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<char>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<char>(void* file, char* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<char>", file, value, name, noLog);
}
template<> int32_t LOADVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<wchar>", file, value, name, noLog);
}
template<> int32_t SAVEVAR<char16_t>(void* file, char16_t* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<wchar>", file, value, name, noLog);
}

// ---- the file functions: Open answers the queue "LbFile_Open" (a negative answer is a failure)
TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    ev("LbFile_Open", {mode}, {name});
    const int32_t result = static_cast<int32_t>(ret("LbFile_Open"));
    file = result >= 0 ? gFileHandle : nullptr;
    return TbError(result);
}
TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {lab(file)});
    return TbError(0);
}

// ---- the player settings' serialiser (src/TPSettings.cpp; its own test is test_TPSettings): it records its arguments and answers the queue
int32_t CPlayerSettings::SerialiseLocal(void* file, int32_t save, uint32_t version) {
    ev("CPlayerSettings::SerialiseLocal", {lab(this), lab(file), save, version});
    return static_cast<int32_t>(ret("SerialiseLocal"));
}

// ---- the trace output: the level and the formatted text
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}

// ---- the swear filter (OnlinePerson.cpp): the call is recorded, the queue "SwearFilter::Init" gives the answer; the queue
// "SwearFilter::ClearGlobal" (a 1) makes the call empty gMasterSave, as an Init that tore the global down would (it reaches Load's second
// gMasterSave == NULL test)
bool SwearFilter::Init(const char* wordFile, const char* allowedFile, bool flag) {
    ev("SwearFilter::Init", {lab(this), flag}, {wordFile, allowedFile});
    const bool answer = ret("SwearFilter::Init") != 0;
    if (ret("SwearFilter::ClearGlobal") != 0) gMasterSave = nullptr;
    return answer;
}

// ---- the paths: the data path object and the language name are the world's
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    ev("CSYS_TPFileInfo::GetDataPath", {lab(this)});
    return mDataPath;
}
char* CText::GetLanguageName() {
    ev("CText::GetLanguageName", {lab(this)});
    return gLanguageName;
}

// ---- the balance table and the string table that the ticket code links with: they only record
void CBalanceTable::Init() { ev("Balance.Init", {newlab(this)}); }
void CBalanceTable::UnInit() { ev("Balance.UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("Balance.Load", {lab(this), mode}, {path});
    return 0;
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    if (size != 0) out[0] = 0;
    return 0;
}

namespace {

// ---- the world
CFSGlobalMasterSave* gThis;     // the object under test
CFSGlobalMasterSave* gOther;    // a second master save, the global one in some cases
CFSGlobalParkData* gPark;       // a record on its own (the record tests)
alignas(16) unsigned char gParkBlock[sizeof(CFSGlobalParkData)];   // the block the constructor test builds a record in (the host's pointers are wider than the Mac's)
CFSGlobalParkData* gParkInBlock;

// A reader over the flat integer list the Python module sends.
struct Reader {
    const int64_t* p;
    int64_t next() { return *p++; }
};

std::string ReadName(Reader& r) {
    std::string name;
    const int64_t n = r.next();
    for (int64_t i = 0; i < n; ++i) name += static_cast<char>(r.next());
    return name;
}

// A park record: name, 6 tickets, 4 awards, 4 scores, the two flags, both sign names (33 units each). Every member is set (the record
// is made by the translated constructor, then overwritten).
CFSGlobalParkData* ReadPark(Reader& r) {
    const std::string name = ReadName(r);
    auto* park = new CFSGlobalParkData(TbDynamicStringTemplate<char>(name.c_str()));
    for (int i = 0; i < 6; ++i) park->mTickets[i] = static_cast<uint8_t>(r.next());
    for (int i = 0; i < 4; ++i) park->mAwards[i] = static_cast<uint8_t>(r.next());
    for (int i = 0; i < 4; ++i) park->mAwardScores[i] = static_cast<int32_t>(r.next());
    park->mHasSignNames = static_cast<uint8_t>(r.next());
    park->mAllResearchCompleted = static_cast<uint8_t>(r.next());
    for (int i = 0; i < 0x21; ++i) park->mSignNameA[i] = static_cast<char16_t>(r.next());
    for (int i = 0; i < 0x21; ++i) park->mSignNameB[i] = static_cast<char16_t>(r.next());
    return park;
}

// A master save: the flat fields, the bought things and the theme records.
CFSGlobalMasterSave* ReadMaster(Reader& r) {
    auto* master = new CFSGlobalMasterSave;
    for (int i = 0; i < 4; ++i) master->mGlobalTickets[i] = static_cast<uint8_t>(r.next());
    for (int i = 0; i < 2; ++i) master->mSecretTickets[i] = static_cast<uint8_t>(r.next());
    master->mTicketsSpent = static_cast<int32_t>(r.next());
    master->mExtraKeys = static_cast<int32_t>(r.next());
    master->mEasyMode = static_cast<uint8_t>(r.next());
    master->mSwearFilter = static_cast<uint8_t>(r.next());
    master->mFirstTimePlayer = static_cast<uint8_t>(r.next());
    const int64_t bought = r.next();
    for (int64_t i = 0; i < bought; ++i) master->mBought.insert(static_cast<uint16_t>(r.next()));
    const int64_t parks = r.next();
    for (int64_t i = 0; i < parks; ++i) {
        CFSGlobalParkData* park = ReadPark(r);
        master->mThemes.insert(std::make_pair(park->mName, park));
    }
    return master;
}

// ---- the dump: integers, in the order the Python module dumps the PowerPC side
std::vector<int64_t> gDump;
void Put(int64_t value) { gDump.push_back(value); }

void PutName(const char* text, std::size_t length) {
    Put(static_cast<int64_t>(length));
    bool ended = false;
    for (int i = 0; i < 24; ++i) {
        if (!ended && text[i] == 0) ended = true;
        Put(ended ? 0 : static_cast<unsigned char>(text[i]));
    }
}

void PutPark(const CFSGlobalParkData* park) {
    for (int i = 0; i < 6; ++i) Put(park->mTickets[i]);
    for (int i = 0; i < 4; ++i) Put(park->mAwards[i]);
    for (int i = 0; i < 4; ++i) Put(park->mAwardScores[i]);
    Put(park->mBalance != nullptr);
    Put(park->mHasSignNames);
    Put(park->mAllResearchCompleted);
    PutName(park->mName.mData, static_cast<std::size_t>(park->mName.mLength));
    for (int i = 0; i < 0x21; ++i) Put(park->mSignNameA[i]);
    for (int i = 0; i < 0x21; ++i) Put(park->mSignNameB[i]);
}

void PutMaster(const CFSGlobalMasterSave* master) {
    for (int i = 0; i < 4; ++i) Put(master->mGlobalTickets[i]);
    for (int i = 0; i < 2; ++i) Put(master->mSecretTickets[i]);
    Put(master->mTicketsSpent);
    Put(master->mExtraKeys);
    Put(master->mEasyMode);
    Put(master->mSwearFilter);
    Put(master->mFirstTimePlayer);
    Put(static_cast<int64_t>(master->mBought.size()));
    int shown = 0;
    for (uint16_t thing : master->mBought) {
        if (shown++ < 40) Put(thing);
    }
    Put(static_cast<int64_t>(master->mThemes.size()));
    for (const auto& theme : master->mThemes) {
        PutName(theme.first.mData, static_cast<std::size_t>(theme.first.mLength));
        PutPark(theme.second);
    }
}

}  // namespace

extern "C" {

// Clears the world and sets the labels (the same fixed ones the Python module gives the PowerPC side).
void tpw_mv_reset(const char* dataPath, const char* language, const char* fileBytes, int32_t fileSize, int64_t failAt,
                  int64_t failValue) {
    gVarCalls = 0;
    gFailAt = failAt;
    gFailValue = failValue;
    gThis = nullptr;
    gOther = nullptr;
    gMasterSave = nullptr;
    gPark = nullptr;
    gParkInBlock = nullptr;
    gFileData.assign(fileBytes, static_cast<std::size_t>(fileSize));
    gFilePosition = 0;
    std::strncpy(gLanguageName, language, sizeof(gLanguageName) - 1);
    gLanguageName[sizeof(gLanguageName) - 1] = 0;
    gTPFileInfo.mDataPath = dataPath;
    tpw_cl_label(kLabelFile, gFileHandle);
    tpw_cl_label(kLabelSettings, &gPlayerSettings);
    tpw_cl_label(kLabelText, &gText);
    tpw_cl_label(kLabelFileInfo, &gTPFileInfo);
}

// Builds the master saves: the one under test (labelled 1) and, when `other` is set, a second one (labelled 2). `global` says what
// gMasterSave is: 0 NULL, 1 the object under test, 2 the other one.
void tpw_mv_build(const int64_t* data, int32_t global) {
    Reader r{data};
    gThis = ReadMaster(r);
    tpw_cl_label(kLabelMaster, gThis);
    if (global == 2) {
        gOther = ReadMaster(r);
        tpw_cl_label(kLabelOther, gOther);
    }
    gMasterSave = global == 0 ? nullptr : global == 1 ? gThis : gOther;
}

// Builds the stand-alone record of the record tests.
void tpw_mv_build_park(const int64_t* data) {
    Reader r{data};
    gPark = ReadPark(r);
}

// Makes the block the constructor test builds in: filled with 0xa5, as the PowerPC side's memory is.
void tpw_mv_build_block() { std::memset(gParkBlock, 0xa5, sizeof gParkBlock); }

// The calls. a[0] is the function: 0 Save(path), 1 Load(path), 2 master Serialise(file, save, version), 3 park Serialise,
// 4 the park's loader constructor (in the block, with the name `text`). The result is the answer (0 for the constructor).
int64_t tpw_mv_call(const int64_t* a, const char* text) {
    void* file = gFileHandle;
    switch (a[0]) {
        case 0: return gThis->Save(text);
        case 1: return gThis->Load(text);
        case 2: return gThis->Serialise(file, static_cast<int32_t>(a[1]), static_cast<uint32_t>(a[2]));
        case 3: return gPark->Serialise(file, static_cast<int32_t>(a[1]), static_cast<uint32_t>(a[2]));
        case 4: {
            CSerialise serialise;
            gParkInBlock = new (gParkBlock) CFSGlobalParkData(serialise, TbDynamicStringTemplate<char>(text));
            return 0;
        }
    }
    return -1;
}

// The state: where gMasterSave points (0 NULL, 1 the object, 2 the other one, 3 a new one), the master saves, the stand-alone record,
// the record built in the block and the file position. The out array holds the count first.
void tpw_mv_dump(int64_t* out) {
    gDump.clear();
    const int kind = gMasterSave == nullptr ? 0 : gMasterSave == gThis ? 1 : gMasterSave == gOther ? 2 : 3;
    Put(kind);
    if (gThis != nullptr) PutMaster(gThis);
    if (gOther != nullptr) PutMaster(gOther);
    if (kind == 3) PutMaster(gMasterSave);
    if (gPark != nullptr) PutPark(gPark);
    if (gParkInBlock != nullptr) PutPark(gParkInBlock);
    Put(static_cast<int64_t>(gFilePosition));
    out[0] = static_cast<int64_t>(gDump.size());
    for (std::size_t i = 0; i < gDump.size() && i < 8000; ++i) out[1 + i] = gDump[i];
}

void tpw_mv_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}

}  // extern "C"
