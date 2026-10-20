// Test library for the park file reader and the most-recent-save loader (libtpw_unit_SaveLoad.dylib; see
// tests/test_SaveLoad.py): src/SaveLoad.cpp (CSaveLoad::Load) and src/FS_LoadSave.cpp (CFSLoadSave::LoadMostRecent) of unit w108.
// Both functions are real, and LoadMostRecent calls the real Load.
//
// Everything else is a fake on both sides that records through calllog: the assertion, CTRACE and DEBUG_MESSAGE, the file
// classes (a disc file, a memory file, their constructors, destructors, Close, Seek and GetSize, and the access objects behind
// them, whose Read and Write record and answer from queues), the temporary allocator, the zlib size and unpack calls, the
// stand-ins CSaveLoad::LoadHeader, LoadModules, PreLoadOperations and PostLoadOperations, the user info (its constructor and the
// folder it answers), the string conversion, the sub folder test, the time stamp compare and the disc storage with its listing.
// The bullfrog strings are functional and unrecorded, as on the PowerPC side.
//
// The Python module sets the world up with the tpw_sl_ functions below (with recording off), then calls one function with
// recording on and reads the state back.
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using calllog::ev;
using calllog::forget;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// ------------------------------------------------------------------ bullfrog strings (functional)

#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
#include "fake_strings.h"
#include "fake_file_slots.h"

// ------------------------------------------------------------------ globals of other files

TPGlobals::TPGlobals() {}   // the unit starts from a blank object, the test sets the strings it reads
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CFSUserInfo* gUserInfo;
CFSLoadSave gLoadSave;
CSaveLoad gSaveLoad;
SamsUtilities::UAllocatorTemporary* SamsUtilities::gMemoryTemporary;

namespace {

constexpr uint32_t kCap = 4096;   // the most bytes a fake buffer holds (and a fake read fills)
CSaveLoadHeader gHeader;          // the header Load is given
TbDynamicStringTemplate<char16_t> gUsersPath;   // what ReturnUsersPathWithTheme answers
uint8_t gVersionBytes[4];         // the file's first word, as the file has it
int32_t gReads;                   // reads answered so far in the case

// The first four bytes of a block as a little endian number (the PowerPC side reads the bytes the same way).
int64_t FirstWord(const void* block) {
    if (block == nullptr) return -1;
    const uint8_t* b = static_cast<const uint8_t*>(block);
    return static_cast<int64_t>(b[0]) | (static_cast<int64_t>(b[1]) << 8) | (static_cast<int64_t>(b[2]) << 16) |
           (static_cast<int64_t>(b[3]) << 24);
}

// ---- the access objects behind the files: Read and Write record, answer from the queues "Access::Read" and "Access::Write"
// (all the bytes asked for when the queue is empty), and fill what they are given
class FakeAccess : public bfl::FileSystem::CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("Access::Read", {lab(this), lab(dest), size});
        if (dest != nullptr) {
            uint8_t* bytes = static_cast<uint8_t*>(dest);
            if (gReads == 0) {
                std::memcpy(bytes, gVersionBytes, std::min<uint32_t>(size, 4));
            } else {
                for (uint32_t i = 0; i < std::min(size, kCap); ++i) bytes[i] = static_cast<uint8_t>(i * 7 + 3);
            }
        }
        ++gReads;
        return static_cast<int32_t>(ret("Access::Read", static_cast<int32_t>(size)));
    }
    int32_t Write(const void* source, uint32_t size) override {
        ev("Access::Write", {lab(this), lab(source), size, FirstWord(source)});
        return static_cast<int32_t>(ret("Access::Write", static_cast<int32_t>(size)));
    }
};
FakeAccess gPool[4];
int32_t gPoolUsed;

// An access object for a file that opened: the queue "disc impl" or "mem impl" (default 1) says whether it gets one.
void Attach(bfl::FileSystem::CFile& file, const char* key) {
    if (ret(key, 1) == 0) return;
    file.mImplementation = &gPool[gPoolUsed++];
    newlab(file.mImplementation);
}

// ---- the disc storage's listing: the entries the case gives, in order
struct Entry {
    uint32_t attributes;
    uint32_t size;
    uint32_t words[6];   // the header bytes 8 to 0x1f: the stamp (two words), then four more
    char prefix[64];     // the text before the name in the entry's buffer (the name pointer follows it)
    char name[300];
};
Entry gEntries[64];
int32_t gEntryCount;
int32_t gNextEntry;

void FillInfo(bfl::FileSystem::CFileInfo& info, const Entry& entry) {
    info.mF00 = entry.size;
    info.mAttributes = entry.attributes;
    std::memcpy(info.mF08, entry.words, sizeof entry.words);
    std::snprintf(info.mNameBuffer, sizeof info.mNameBuffer, "%s%s", entry.prefix, entry.name);
    info.mName = info.mNameBuffer + std::strlen(entry.prefix);
}

}  // namespace

// ------------------------------------------------------------------ debug output

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}
void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {flags}, {message}); }

// ------------------------------------------------------------------ the file classes

bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) { ev("CFile::CFile", {newlab(this)}); }
bfl::FileSystem::CFile::~CFile() { ev("CFile::~CFile", {lab(this)}); }
void bfl::FileSystem::CFile::Close() { ev("CFile::Close", {lab(this)}); }
int32_t bfl::FileSystem::CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("CFile::Seek", {lab(this), offset, whence});
    return static_cast<int32_t>(ret("CFile::Seek"));
}
int32_t bfl::FileSystem::CFile::GetSize() {
    ev("CFile::GetSize", {lab(this)});
    return static_cast<int32_t>(ret("CFile::GetSize"));
}
bfl::FileSystem::CDiscFile::CDiscFile() {}    // (the bullfrog constructor runs the CFile's, which records)
bfl::FileSystem::CDiscFile::~CDiscFile() {}   // (and so does its destructor)
TbError bfl::FileSystem::CDiscFile::Open(const char* name, uint32_t mode) {
    ev("CDiscFile::Open", {lab(this), mode}, {name});
    const int32_t result = static_cast<int32_t>(ret("CDiscFile::Open"));
    if (result >= 0) Attach(*this, "disc impl");
    return TbError(result);
}
TbError bfl::FileSystem::CMemFile::Open(uint32_t size) {
    ev("CMemFile::Open", {lab(this), size});
    const int32_t result = static_cast<int32_t>(ret("CMemFile::Open"));
    if (result >= 0) Attach(*this, "mem impl");
    return TbError(result);
}

// ------------------------------------------------------------------ the temporary allocator

SamsUtilities::UAllocatorTemporary::UAllocatorTemporary() { ev("UAllocatorTemporary", {newlab(this)}); }
SamsUtilities::UAllocatorTemporary::~UAllocatorTemporary() {}
uint8_t* SamsUtilities::UAllocatorTemporary::Allocate(uint32_t size) {
    ev("Allocate", {lab(this), size});
    if (ret("Allocate", 1) == 0) return nullptr;
    uint8_t* block = static_cast<uint8_t*>(std::calloc(1, std::min(size, kCap) + 16));
    newlab(block);
    return block;
}
void SamsUtilities::UAllocatorTemporary::Free(uint8_t*& block) {
    ev("Free", {lab(this), lab(block)});
    forget(block);
    std::free(block);
    block = nullptr;
}

// ------------------------------------------------------------------ the zlib calls

uint32_t LbZLib_MemDecompressedSize(void* source) {
    ev("LbZLib_MemDecompressedSize", {lab(source), FirstWord(source)});
    return static_cast<uint32_t>(ret("LbZLib_MemDecompressedSize", 64));
}
uint32_t LbZLib_MemDecompress(void* source, void* dest, void* workspace) {
    ev("LbZLib_MemDecompress", {lab(source), lab(dest), lab(workspace)});
    if (dest != nullptr)
        for (int32_t i = 0; i < 16; ++i) static_cast<uint8_t*>(dest)[i] = static_cast<uint8_t>(i * 13 + 0x21);
    return 0;
}

// ------------------------------------------------------------------ the rest of CSaveLoad (stand-ins in the game)

int32_t CSaveLoad::LoadHeader(bfl::FileSystem::CFile& file, LoadType type, CSaveLoadHeader* header) {
    ev("LoadHeader", {lab(this), lab(&file), type, lab(header)});
    return static_cast<int32_t>(ret("LoadHeader", 1));
}
int32_t CSaveLoad::LoadModules(bfl::FileSystem::CFile& file, LoadType type) {
    ev("LoadModules", {lab(this), lab(&file), type});
    return static_cast<int32_t>(ret("LoadModules", 1));
}
void CSaveLoad::PreLoadOperations() { ev("PreLoadOperations", {lab(this)}); }
void CSaveLoad::PostLoadOperations() { ev("PostLoadOperations", {lab(this)}); }

// ------------------------------------------------------------------ what LoadMostRecent calls

// The user info: only its constructor is called (the members' constructors do nothing here).
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() { ev("CFSUserInfo", {newlab(this)}); }
TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    ev("ReturnUsersPathWithTheme", {lab(this)});
    return gUsersPath;
}
// The conversion keeps Latin-1 (characters above 0xff become '?'); the recorded text is the wide one narrowed the same way.
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    char text[512];
    int32_t n = 0;
    for (; in.mData[n] != 0 && n < 511; ++n) text[n] = in.mData[n] > 0xff ? '?' : static_cast<char>(in.mData[n]);
    text[n] = 0;
    ev("UniStringToString", {}, {text});
    out = text;
    return n + 1;
}
// A sub folder: the real rule (a directory attribute and a name that does not start with a dot), recorded.
bool FS_IsSubDirectory(const bfl::FileSystem::CFileInfo& info) {
    ev("FS_IsSubDirectory", {info.mAttributes}, {info.mName});
    return (info.mAttributes & 0x10) != 0 && info.mName[0] != '.';
}
int32_t LbFile_CompareFileStamps(const TbFileStamp* a, const TbFileStamp* b) {
    ev("LbFile_CompareFileStamps", {a->mLow, a->mHigh, b->mLow, b->mHigh});
    return static_cast<int32_t>(ret("CompareFileStamps"));
}

bfl::FileSystem::CFileStorage::CFileStorage() : mAccess(nullptr) { ev("CFileStorage", {newlab(this)}); }
bfl::FileSystem::CFileStorage::~CFileStorage() { ev("~CFileStorage", {lab(this)}); }
TbError bfl::FileSystem::CDiscFileStorage::Open(const char* path) {
    ev("CDiscFileStorage::Open", {lab(this)}, {path});
    gNextEntry = 0;
    return TbError(static_cast<int32_t>(ret("CDiscFileStorage::Open")));
}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    ev("CFileStorage::FindFirst", {lab(this)}, {pattern});
    const int32_t result = static_cast<int32_t>(ret("FindFirst"));
    if (result < 0) return TbError(result);
    if (gEntryCount == 0) return TbError(-1);
    FillInfo(info, gEntries[0]);
    gNextEntry = 1;
    return TbError(result);
}
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo& info) {
    ev("CFileStorage::FindNext", {lab(this)});
    // the queue "FindNext" answers each call: negative ends the listing, anything else is a success (default 0)
    const int32_t result = static_cast<int32_t>(ret("FindNext"));
    if (result < 0) return TbError(result);
    if (gNextEntry >= gEntryCount) return TbError(-1);
    FillInfo(info, gEntries[gNextEntry++]);
    return TbError(result);
}
// The storage members this unit does not use (unrecorded).
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) {
    return TbError(0);
}
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }

// ------------------------------------------------------------------ test entry points

extern "C" {

// The objects and globals the module places labels on or reads.
void* tpw_sl_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return &gSaveLoad;
    if (std::strcmp(name, "gHeader") == 0) return &gHeader;
    if (std::strcmp(name, "gUserInfo") == 0) return &gUserInfo;
    if (std::strcmp(name, "gMemoryTemporary") == 0) return &SamsUtilities::gMemoryTemporary;
    if (std::strcmp(name, "gOpenTestWord") == 0) return &gSaveLoadOpenTestWord;
    if (std::strcmp(name, "mAllFiles") == 0) return &gTPGlobals.mAllFiles;
    if (std::strcmp(name, "mSaveFilePattern") == 0) return &gTPGlobals.mSaveFilePattern;
    if (std::strcmp(name, "mSeparator") == 0) return &gTPGlobals.mSeparator;
    return nullptr;
}
void tpw_sl_set_text(const char* name, const char* text) {
    *static_cast<TbDynamicStringTemplate<char>*>(tpw_sl_addr(name)) = text;
}

// Clears the world: the listing, the reads, the access objects and the file's first word.
void tpw_sl_reset(const uint8_t* versionBytes) {
    gEntryCount = 0;
    gNextEntry = 0;
    gReads = 0;
    gPoolUsed = 0;
    std::memcpy(gVersionBytes, versionBytes, 4);
    gSaveLoad.mF00 = 0;
    gSaveLoad.mError = 0;
    gUserInfo = nullptr;
    SamsUtilities::gMemoryTemporary = nullptr;
    // Load holds its memory file in the arena; the arena's first use allocates its hole list with operator new, which the heap
    // count would see in the first case that gets there, so the arena is reserved here.
    tpw::ArenaFree(tpw::ArenaAlloc(1));
}
void tpw_sl_entry(uint32_t attributes, uint32_t size, const uint32_t* words, const char* prefix, const char* name) {
    if (gEntryCount >= 64) return;
    Entry& entry = gEntries[gEntryCount++];
    entry.attributes = attributes;
    entry.size = size;
    std::memcpy(entry.words, words, sizeof entry.words);
    std::snprintf(entry.prefix, sizeof entry.prefix, "%s", prefix);
    std::snprintf(entry.name, sizeof entry.name, "%s", name);
}
void tpw_sl_set_users_path(const char16_t* text) { gUsersPath = text; }
// A user info and an allocator that exist already.
void* tpw_sl_make_user_info() {
    gUserInfo = new CFSUserInfo;
    return gUserInfo;
}
void* tpw_sl_make_allocator() {
    SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return SamsUtilities::gMemoryTemporary;
}
void tpw_sl_set_error(int32_t value) { gSaveLoad.mError = value; }
int32_t tpw_sl_error() { return gSaveLoad.mError; }

int32_t tpw_sl_load(const char* path, void* header, int32_t type) {
    return gSaveLoad.Load(path, static_cast<CSaveLoadHeader*>(header), static_cast<CSaveLoad::LoadType>(type));
}
void tpw_sl_load_most_recent() { gLoadSave.LoadMostRecent(); }

}  // extern "C"
