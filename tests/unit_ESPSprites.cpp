// Test library for the ESP sprite loader (libtpw_unit_ESPSprites.dylib; see tests/test_ESPSprites.py):
// src/ESP_sprites.cpp (ESP_CSprite, ESP_SortFileList, ESP_FindESPFiles, ESP_LoadSprites, ESP_GetParticleSprite,
// ESP_InitialiseAndLoadSprites), src/HLSprite.cpp (the lists' array) and the game's default names (src/TPGlobals.cpp).
//
// Real: those, with the bullfrog strings functional and unrecorded (fake_strings.h). Everything else the unit calls is a
// fake that records through tests/calllog.h: the directory search (CTPBaseResource::FindFirst, FindNext and
// pGetCurrentSearchStorage over the case's listings), the resource manager, the file storages and files (a storage opens
// the case's files by name, a file serves their bytes), the engine's sprite file (CSpriteFileIO::Load and
// FlagAsSemitransparent), CSpriteList::Init, CMapWho::Link, the options, the theme name, the string helpers and CTRACE.
// Objects the unit makes with operator new get a label as they are allocated (kinds by size: a sprite, a directory entry,
// a file or storage, a sprite file) and their memory is filled with 0xa5 first, so a field no constructor sets shows as
// that. The fakes allocate nothing with operator new while recording (the observer would take it for a game object).
//
// The world is built by the tpw_esp_* functions (listings, files, theme, options, globals, a few objects), operations run
// through tpw_esp_op, and tpw_esp_dump writes the observable state as (label, number) records that the test compares with
// the numbers the PowerPC side reads from the emulator's memory in the same order.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_CALLLOG_LABELS 4096
#define TPW_CALLLOG_QUEUE_VALUES 1024
#include "calllog.cpp"

#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_System.h"
#include "Game.h"
#include "HLSprite.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPOptions.h"
#include "TPResource.h"
#include "TPString.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_resource_display.h"
#include "tb_starfield.h"

// The allocation observer tells the game's objects apart by size, and a std::basic_string temporary of the string join asks
// operator new for 32 bytes for a text of 23 to 30 characters, which is the size of a host CSpriteFileIO: the join must not use it.
#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW 1
#include "fake_strings.h"

using namespace calllog;
using bfl::FileSystem::CAccessItem;
using bfl::FileSystem::CFile;
using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

namespace {

// Labels the PowerPC side gives the same objects (test_ESPSprites.py).
enum {
    kLabelGame = 1, kLabelMapWho = 2, kLabelManager = 3, kLabelResource = 4, kLabelStorage = 5,   // 5..7
    kLabelInfo = 10,                                                                              // 10..13
    kLabelSlot = 20,                                                                              // 20..23
    kLabelLinkBase = 30,                                                                          // 30..46: the lists
};
constexpr int kStorages = 3, kInfos = 4, kSlots = 4, kFilesMax = 1400, kListingsMax = 40, kEntriesMax = 150;

// ---- the case's world
struct Entry {
    char buffer[0x104];
    int32_t offset;       // where mName points in the buffer
    int32_t storage;
    uint32_t words[8];    // the entry's first eight words (mF00, mAttributes, then six more)
};
struct Listing {
    char pattern[96];
    int32_t count;
    Entry entries[kEntriesMax];
};
struct FileData {
    int32_t storage;
    char name[96];
    int32_t result;       // what OpenFile answers
    int32_t size;
    uint8_t bytes[360];
};
Listing gListings[kListingsMax];
int32_t gListingCount;
FileData gFiles[kFilesMax];
int32_t gFileCount;
Listing* gCurrentListing;      // the search in progress
int32_t gCursor;
int32_t gCurrentStorage;       // the storage of the entry FindFirst or FindNext last gave
uint8_t gScratchBytes[0x200];

// ---- the files: one stream per file object
class FakeAccess : public CAccessItem {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("CFile.Read", {lab(mOwner), size});
        int64_t remaining = static_cast<int64_t>(mSize) - mPosition;
        if (remaining < 0) remaining = 0;
        const int64_t count = size < remaining ? size : remaining;
        if (dest != nullptr && count > 0) std::memcpy(dest, mBytes + mPosition, static_cast<std::size_t>(count));
        mPosition += static_cast<int32_t>(count);
        const int64_t forced = ret("Read", -2);
        return static_cast<int32_t>(forced == -2 ? count : forced);
    }
    const CFile* mOwner = nullptr;
    const uint8_t* mBytes = nullptr;
    int32_t mSize = 0;
    int32_t mPosition = 0;
};
constexpr int kAccessPool = 4096;
FakeAccess gAccess[kAccessPool];
int32_t gAccessUsed;

// ---- the objects the world stands on
alignas(16) unsigned char gGameManager[64];       // a CTPResourceManager that is only ever pointed at
alignas(16) unsigned char gMapWhoBytes[256];      // a CMapWho that is only ever pointed at
alignas(16) unsigned char gResourceBytes[sizeof(CTPSpriteResource)];
CTPSpriteResource* gResource;
CFileStorage gStorages[kStorages];
CFileInfo gInfos[kInfos];
alignas(16) unsigned char gSlotBytes[kSlots][sizeof(ESP_CSprite)];
ESP_CSprite* gSlots[kSlots];
int64_t gOut[4];                                   // GetCustomData's outputs

// ---- the allocation observer
void ObserveAllocation(std::size_t size, bool array, void* block) {
    if (!on() || array) return;
    int64_t mac;
    if (size == sizeof(ESP_CSprite)) mac = 0x260;
    else if (size == sizeof(CFileInfo)) mac = 0x128;
    else if (size == sizeof(CFile)) mac = 8;   // a CFile and a CFileStorage are both 16 bytes here, 8 on the Mac
    else if (size == sizeof(CSpriteFileIO)) mac = 0x10;
    else return;
    std::memset(block, 0xa5, size);
    newlab(block);
    ev("new", {mac});
}
static_assert(sizeof(CFile) == sizeof(CFileStorage), "one size for both");
static_assert(sizeof(ESP_CSprite) != sizeof(CFileInfo) && sizeof(CSpriteFileIO) != sizeof(CFile), "kinds told apart by size");

uintptr_t StorageId(const CFileStorage& storage) { return reinterpret_cast<uintptr_t>(storage.mAccess); }

}  // namespace

// ---- globals of other files
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
COptions::~COptions() {}
COptions gOptions;
TPGlobals gTPGlobals;
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapWhoBytes);
#ifdef __APPLE__
extern unsigned char gDisplayStorage[sizeof(CDisplay)] __asm__("_gei_Display");
#else
extern unsigned char gDisplayStorage[sizeof(CDisplay)] __asm__("gei_Display");
#endif
alignas(16) unsigned char gDisplayStorage[sizeof(CDisplay)] = {};
tag_sM3D2Header* gHeightfieldHeader;

// ---- the file layer
namespace bfl {
namespace FileSystem {

CAccessItem::CAccessItem() : mNext(nullptr), mReferences(0) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(-1); }
int32_t CAccessItem::GetMacFileReference() { return -1; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

// A file object gets the next stream of the pool; it has no bytes until a storage opens a file into it.
CFile::CFile() : mImplementation(nullptr) {
    ev("CFile.ctor", {lab(this)});
    if (gAccessUsed < kAccessPool) {
        gAccess[gAccessUsed].mOwner = this;
        gAccess[gAccessUsed].mBytes = nullptr;
        gAccess[gAccessUsed].mSize = 0;
        gAccess[gAccessUsed].mPosition = 0;
        mImplementation = &gAccess[gAccessUsed++];
    }
}
CFile::~CFile() { ev("CFile.dtor", {lab(this)}); }
void CFile::Close() { ev("CFile.Close", {lab(this)}); }
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(-1); }
int32_t CFile::GetMacFileReference() { return -1; }

CFileStorage::CFileStorage() : mAccess(nullptr) { ev("CFileStorage.ctor", {lab(this)}); }
CFileStorage::CFileStorage(const CFileStorage& other) : mAccess(other.mAccess) {}
// A copy keeps the original's identity (which of the case's storages it is).
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    ev("CFileStorage.assign", {lab(this), lab(&other), static_cast<int64_t>(StorageId(other))});
    mAccess = other.mAccess;
    return *this;
}
CFileStorage::~CFileStorage() { ev("CFileStorage.dtor", {lab(this)}); }
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
// Opens the case's file of that name in that storage: the answer is the file's, or -1 for a name the storage does not
// have; a queued answer replaces it. A file that opens starts at its first byte.
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    const int64_t id = static_cast<int64_t>(StorageId(*this));
    ev("OpenFile", {lab(this), id, lab(&file), mode, data != nullptr}, {name});
    const FileData* found = nullptr;
    for (int32_t i = 0; i < gFileCount; ++i) {
        if (gFiles[i].storage == id && std::strcmp(gFiles[i].name, name) == 0) {
            found = &gFiles[i];
            break;
        }
    }
    int32_t result = found != nullptr ? found->result : -1;
    const int64_t forced = ret("OpenFile", -9);
    if (forced != -9) result = static_cast<int32_t>(forced);
    if (result >= 0 && file.mImplementation != nullptr) {
        auto* access = static_cast<FakeAccess*>(file.mImplementation);
        access->mBytes = found != nullptr ? found->bytes : gScratchBytes;
        access->mSize = found != nullptr ? found->size : 0;
        access->mPosition = 0;
    }
    return TbError(result);
}
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(-1); }
const char* CFileStorage::GetRootPath() { return ""; }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::CreateDir(const char*) { return TbError(-1); }
TbError CFileStorage::Delete(const char*) { return TbError(-1); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(-1); }

}  // namespace FileSystem
}  // namespace bfl

// ---- the resources
CTPBaseResource::~CTPBaseResource() {}
int32_t CTPBaseResource::OpenFile(CFile&, const TbDynamicStringTemplate<char>&, uint32_t) { return 0; }
int32_t CTPBaseResource::OpenFile(CFile&, const TbDynamicStringTemplate<char>&, CFileStorage&, uint32_t) { return 0; }
CTPSpriteResource::CTPSpriteResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPSpriteResource::~CTPSpriteResource() {}

namespace {
// Gives `info` the listing's entry at the cursor; the storage it was found in is remembered.
void GiveEntry(CFileInfo& info) {
    const Entry& entry = gCurrentListing->entries[gCursor];
    std::strcpy(info.mNameBuffer, entry.buffer);
    info.mName = info.mNameBuffer + entry.offset;
    info.mF00 = entry.words[0];
    info.mAttributes = entry.words[1];
    std::memcpy(info.mF08, &entry.words[2], sizeof info.mF08);
    gCurrentStorage = entry.storage;
}
}  // namespace

// The first entry of the listing for the pattern (none: nothing found); a queued answer replaces the natural one.
int32_t CTPBaseResource::FindFirst(const TbDynamicStringTemplate<char>& pattern, CFileInfo& info) {
    ev("FindFirst", {lab(this)}, {pattern.mData});
    gCurrentListing = nullptr;
    gCursor = 0;
    for (int32_t i = 0; i < gListingCount; ++i) {
        if (std::strcmp(gListings[i].pattern, pattern.mData) == 0) gCurrentListing = &gListings[i];
    }
    int32_t found = 0;
    if (gCurrentListing != nullptr && gCurrentListing->count > 0) {
        GiveEntry(info);
        found = 1;
    }
    const int64_t forced = ret("FindFirst", -9);
    return forced == -9 ? found : static_cast<int32_t>(forced);
}
int32_t CTPBaseResource::FindNext(CFileInfo& info) {
    ev("FindNext", {lab(this)});
    int32_t found = 0;
    if (gCurrentListing != nullptr && gCursor + 1 < gCurrentListing->count) {
        ++gCursor;
        GiveEntry(info);
        found = 1;
    }
    const int64_t forced = ret("FindNext", -9);
    return forced == -9 || found == 0 ? found : static_cast<int32_t>(forced);
}
CFileStorage* CTPBaseResource::pGetCurrentSearchStorage() const {
    ev("pGetCurrentSearchStorage", {lab(this)});
    return &gStorages[gCurrentStorage];
}
// The case's resource unless the answer is 0 (a queued answer other than 1 makes ESP_LoadSprites fail).
int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource) {
    ev("FindResource", {lab(this)}, {name.mData});
    const int32_t result = static_cast<int32_t>(ret("FindResource", 1));
    if (result != 0) *resource = gResource;
    return result;
}

// ---- the game, the options and the string helpers
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    ev("GetCurrentThemeName", {lab(this)});
    return mCurrentThemeName;
}
int32_t COptions::GetNumKidSprites() const {
    ev("GetNumKidSprites");
    return static_cast<int32_t>(ret("GetNumKidSprites", 3));
}
int32_t COptions::GetNumStaffSprites() const {
    ev("GetNumStaffSprites");
    return static_cast<int32_t>(ret("GetNumStaffSprites", 2));
}
namespace {
int Lower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : static_cast<unsigned char>(c); }
// What the library's case-insensitive comparison gives: <0, 0 or >0.
int CompareIgnoringCase(const char* a, const char* b) {
    while (Lower(*a) == Lower(*b)) {
        if (*a == 0) return 0;
        ++a;
        ++b;
    }
    return Lower(*a) - Lower(*b);
}
}  // namespace
bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    ev("TPStrEq", {}, {a.mData, b.mData});
    return CompareIgnoringCase(a.mData, b.mData) == 0;
}
bool TPStrGtr(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    ev("TPStrGtr", {}, {a.mData, b.mData});
    return CompareIgnoringCase(a.mData, b.mData) > 0;
}
// The real one (src/FS_System.cpp) appends ":" unless the path's last character is one; recorded here with the path as it was.
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("FS_AddDirectorySeparator", {}, {path.mData});
    const int32_t length = path.Length();
    if (length == 0 || path.mData[length - 1] != ':') path += ":";
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* text = va_arg(args, const char*);
    va_end(args);
    ev("CTRACE", {level}, {format, text});
}
void TRACE(const char*, ...) {}

// ---- the engine
SSpriteGameData** CSpriteList::Init(int32_t count) {
    ev("CSpriteList.Init", {lab(this), count});
    return reinterpret_cast<SSpriteGameData**>(ret("Init", 0x70000000 + lab(this) * 0x100));
}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
int32_t CMapWho::Link(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link, int32_t list) {
    ev("CMapWho.Link", {lab(this), a, b, c, d, lab(link), list});
    return 1;
}
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("CMapWho.LinkList", {lab(this), lab(link), list});
    return 1;
}
// Load reads the first four bytes of the data file, as the real one reads its header, so that the log shows which file it
// was handed and where the stream stood.
int32_t CSpriteFileIO::Load(CSpriteFileIO* io, void* file) {
    ev("CSpriteFileIO.Load", {lab(io), lab(file), 0});   // the third word is r5, which the caller clears and Load ignores
    uint8_t head[4] = {0xee, 0xee, 0xee, 0xee};
    const int32_t got = static_cast<CFile*>(file)->Read(head, 4);
    ev("CSpriteFileIO.Head", {got, head[0], head[1], head[2], head[3]});
    io->mCount = static_cast<int32_t>(ret("LoadCount", 3));
    io->mRecords = reinterpret_cast<SSpriteEngineData*>(static_cast<uintptr_t>(ret("LoadRecords", 0x40000000)));
    return static_cast<int32_t>(ret("Load", 1));
}
int32_t CSpriteFileIO::Free() { ev("CSpriteFileIO.Free", {lab(this)}); return 1; }
void CSpriteFileIO::FlagAsSemitransparent() { ev("CSpriteFileIO.FlagAsSemitransparent", {lab(this)}); }

// ---- the test's entry points
namespace {

struct Record {
    char label[40];
    int64_t value;
};
Record* gDump;
int32_t gDumpCount, gDumpCapacity;

void Put(const char* label, int64_t value) {
    if (gDumpCount == gDumpCapacity) {
        gDumpCapacity = gDumpCapacity == 0 ? 4096 : gDumpCapacity * 2;
        gDump = static_cast<Record*>(std::realloc(gDump, sizeof(Record) * static_cast<std::size_t>(gDumpCapacity)));
    }
    Record& record = gDump[gDumpCount++];
    std::snprintf(record.label, sizeof record.label, "%s", label);
    record.value = value;
}
void Put(const char* prefix, int index, const char* label, int64_t value) {
    char text[40];
    std::snprintf(text, sizeof text, "%s[%d].%s", prefix, index, label);
    Put(text, value);
}
// A text as its length (at most `limit`) and the characters, eight to a number (first character lowest).
void PutText(const char* label, const char* text, int limit) {
    int length = 0;
    while (length < limit && text[length] != 0) ++length;
    char name[40];
    std::snprintf(name, sizeof name, "%s.len", label);
    Put(name, length);
    for (int i = 0; i < length; i += 8) {
        uint64_t word = 0;
        for (int k = 0; k < 8 && i + k < length; ++k) word |= static_cast<uint64_t>(static_cast<uint8_t>(text[i + k])) << (8 * k);
        std::snprintf(name, sizeof name, "%s.w%d", label, i / 8);
        Put(name, static_cast<int64_t>(word));
    }
}

// The scalar globals by name (the test sets and reads them the same way on the PowerPC side).
struct Scalar {
    const char* name;
    void* address;
    int size;
};
Scalar* Scalars() {
    static Scalar table[] = {
        {"draw_enabled", &gESPDrawEnabled, 1},     {"third_person", &gESPThirdPerson, 4},   {"first_person", &gESPFirstPerson, 4},
        {"f20", &gESPF20, 4 /* the low word of the host pointer */}, {"f34", &gESPF34, 4},                    {"f88", &gESPF88, 4},
        {"map_width", &gESPMapWidth, 4},           {"map_height", &gESPMapHeight, 4},       {"cells_wide", &gESPCellsWide, 4},
        {"cells_high", &gESPCellsHigh, 4},         {"semitransparent", &gESPSemitransparent, 1},
        {"sprite_count", &gESPSpriteCount, 4},     {"total_sprites", &gESPTotalSprites, 4}, {"file_count", &gESPFileCount, 4},
        {"compressed", &gESPCompressed, 1},        {"hot_x", &gSpriteHotX, 4},              {"hot_y", &gSpriteHotY, 4},
        {nullptr, nullptr, 0},
    };
    return table;
}

void DumpSprite(const char* prefix, int index, const ESP_CSprite* sprite) {
    Put(prefix, index, "label", lab(sprite));
    if (lab(sprite) <= 0) return;   // none, or a table entry left pointing at a sprite that did not load and was deleted
    Put(prefix, index, "loaded", sprite->mLoaded);
    Put(prefix, index, "firstperson", sprite->mHasFirstPerson);
    Put(prefix, index, "f206", sprite->mF206);
    Put(prefix, index, "semi", sprite->mSemitransparent);
    Put(prefix, index, "records", sprite->mRecordCount);
    Put(prefix, index, "customcount", sprite->mCustomCount);
    for (int k = 0; k < 16; ++k) Put(prefix, index, "custom", sprite->mCustom[k]);
    Put(prefix, index, "actioncount", sprite->mActionCount);
    for (int k = 0; k < 16; ++k) {
        Put(prefix, index, "first", sprite->mActions[k].mFirstFrame);
        Put(prefix, index, "frames", sprite->mActions[k].mFrames);
        Put(prefix, index, "views", sprite->mActions[k].mViews);
    }
    char name[40];
    std::snprintf(name, sizeof name, "%s[%d].name", prefix, index);
    PutText(name, sprite->mName, 0x200);
    Put(prefix, index, "file", lab(sprite->mFile));
    if (sprite->mFile != nullptr) {
        Put(prefix, index, "io.count", static_cast<uint32_t>(sprite->mFile->mCount));
        Put(prefix, index, "io.records", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sprite->mFile->mRecords)));
        Put(prefix, index, "io.f08", static_cast<uint32_t>(sprite->mFile->mF08));
        Put(prefix, index, "io.f0c", static_cast<uint32_t>(sprite->mFile->mF0C));
    }
}

void DumpInfo(const char* prefix, int index, const CFileInfo* info) {
    Put(prefix, index, "label", lab(info));
    if (info == nullptr) return;
    Put(prefix, index, "f00", info->mF00);
    Put(prefix, index, "attributes", info->mAttributes);
    uint32_t words[6];
    std::memcpy(words, info->mF08, sizeof words);
    for (int k = 0; k < 6; ++k) Put(prefix, index, "word", words[k]);
    Put(prefix, index, "nameoffset", info->mName - info->mNameBuffer);
    char name[40];
    std::snprintf(name, sizeof name, "%s[%d].buffer", prefix, index);
    PutText(name, info->mNameBuffer, 0x104);
}

}  // namespace

extern "C" {

// Forgets the world: no listings, no files, the first theme, the options at their defaults, every global at its start value.
void tpw_esp_reset() {
    gListingCount = 0;
    gFileCount = 0;
    gCurrentListing = nullptr;
    gCursor = 0;
    gCurrentStorage = 0;
    gAccessUsed = 0;
    if (gResource == nullptr) {
        gResource = new (gResourceBytes) CTPSpriteResource(TbDynamicStringTemplate<char>("Sprites"),
                                                           TbDynamicStringTemplate<char>(""));
    }
    gGame.mResourceManager = reinterpret_cast<CTPResourceManager*>(gGameManager);
    gGame.mCurrentThemeName = "";
    gei_MapWho = reinterpret_cast<CMapWho*>(gMapWhoBytes);
    std::memset(gDisplayStorage, 0, sizeof gDisplayStorage);
    std::memset(gESPFiles, 0, sizeof gESPFiles);
    std::memset(gESPSprites, 0, sizeof gESPSprites);
    std::memset(gESPPathBuffer, 0, sizeof gESPPathBuffer);
    std::memset(gESPListInstances, 0, sizeof gESPListInstances);
    gESPGlobalInstances = nullptr;
    gESPFileCount = gESPSpriteCount = gESPTotalSprites = 0;
    for (int32_t i = 0; i < kESPGroups; ++i) gESPGroups[i].mFirst = gESPGroups[i].mCount = 0;
    static const char* const kKidFiles[4] = {"SPR_BI.ESP", "SPR_KI.ESP", "SPR_TA.ESP", "SPR_SU.ESP"};
    for (int i = 0; i < 4; ++i) gESPKidFiles[i] = kKidFiles[i];
    gESPSemitransparent = 0;
    gESPCompressed = 1;
    gESPFirstPerson = 0;
    gESPThirdPerson = 1;
    gESPDrawEnabled = 1;
    gESPCellsHigh = gESPCellsWide = gESPMapHeight = gESPMapWidth = gESPF34 = 0;
    gESPF20 = nullptr;
    gESPF88 = 0.0f;
    gSpriteHotX = gSpriteHotY = 0.0f;
    for (int32_t i = 0; i < 16; ++i) gHLSpriteLists[i].mCount = 0;
    gESPGlobalList.mCount = 0;
    gGame.mCurrentThemeName = "";
    for (CFileInfo& info : gInfos) {
        info.mF00 = 0;
        info.mAttributes = 0;
        std::memset(info.mF08, 0, sizeof info.mF08);
        std::memset(info.mNameBuffer, 0, sizeof info.mNameBuffer);
        info.mName = info.mNameBuffer;
    }
    gESPFileInfo.mF00 = 0;
    gESPFileInfo.mAttributes = 0;
    std::memset(gESPFileInfo.mF08, 0, sizeof gESPFileInfo.mF08);
    std::memset(gESPFileInfo.mNameBuffer, 0, sizeof gESPFileInfo.mNameBuffer);
    gESPFileInfo.mName = gESPFileInfo.mNameBuffer;
    for (CFileStorage& storage : gStorages) storage.mAccess = nullptr;
    for (int i = 0; i < kStorages; ++i) gStorages[i].mAccess = reinterpret_cast<bfl::FileSystem::_TbStorageAccess*>(static_cast<uintptr_t>(i));
    for (int i = 0; i < kSlots; ++i) gSlots[i] = nullptr;
    std::memset(gScratchBytes, 0, sizeof gScratchBytes);
    if (gHeightfieldHeader == nullptr) {
        gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
        sHField* field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
        gHeightfieldHeader->mHField = field;
    }
    gDumpCount = 0;
}

void tpw_esp_theme(const char* name) { gGame.mCurrentThemeName = name; }

// Starts a listing for the search pattern; the entries follow.
void tpw_esp_listing(const char* pattern) {
    if (gListingCount >= kListingsMax) return;
    Listing& listing = gListings[gListingCount++];
    std::snprintf(listing.pattern, sizeof listing.pattern, "%s", pattern);
    listing.count = 0;
}
void tpw_esp_entry(const char* buffer, int32_t offset, int32_t storage, const uint32_t* words) {
    if (gListingCount == 0) return;
    Listing& listing = gListings[gListingCount - 1];
    if (listing.count >= kEntriesMax) return;
    Entry& entry = listing.entries[listing.count++];
    std::snprintf(entry.buffer, sizeof entry.buffer, "%s", buffer);
    entry.offset = offset;
    entry.storage = storage;
    std::memcpy(entry.words, words, sizeof entry.words);
}
void tpw_esp_file(int32_t storage, const char* name, int32_t result, const uint8_t* bytes, int32_t size) {
    if (gFileCount >= kFilesMax) return;
    FileData& file = gFiles[gFileCount++];
    file.storage = storage;
    std::snprintf(file.name, sizeof file.name, "%s", name);
    file.result = result;
    file.size = size > static_cast<int32_t>(sizeof file.bytes) ? static_cast<int32_t>(sizeof file.bytes) : size;
    std::memcpy(file.bytes, bytes, static_cast<std::size_t>(file.size));
}
void tpw_esp_set_hfield(int32_t width, int32_t height) {
    sHField* field = gHeightfieldHeader->mHField;
    field->mF18[0] = static_cast<uint32_t>(width);
    field->mF18[1] = static_cast<uint32_t>(height);
}
void tpw_esp_set_display_flags(uint32_t flags) { reinterpret_cast<CDisplay*>(gDisplayStorage)->mFlags = flags; }
void tpw_esp_set_global(const char* name, int64_t value) {
    for (Scalar* s = Scalars(); s->name != nullptr; ++s) {
        if (std::strcmp(s->name, name) != 0) continue;
        if (s->size == 1) *static_cast<uint8_t*>(s->address) = static_cast<uint8_t>(value);
        else {
            const uint32_t word = static_cast<uint32_t>(value);
            std::memcpy(s->address, &word, 4);
        }
        return;
    }
    std::fprintf(stderr, "tpw_esp_set_global: no global %s\n", name);
    std::abort();
}
// Sets a group's first sprite and count (the table the sprites come from).
void tpw_esp_set_group(int32_t index, int32_t first, int32_t count) {
    gESPGroups[index].mFirst = first;
    gESPGroups[index].mCount = count;
}
// Replaces one of the four preferred kid files' names (the table ESP_FindESPFiles' first loop reads).
void tpw_esp_set_kid_file(int32_t index, const char* name) {
    static char names[4][64];
    std::snprintf(names[index], sizeof names[index], "%s", name);
    gESPKidFiles[index] = names[index];
}
void tpw_esp_set_path_buffer(const char* text) { std::snprintf(gESPPathBuffer, sizeof gESPPathBuffer, "%s", text); }

// Puts the labels of the fixed objects in the call log's registry (after tpw_cl_reset).
void tpw_esp_label_fixed() {
    tpw_cl_label(kLabelGame, &gGame);
    tpw_cl_label(kLabelMapWho, gei_MapWho);
    tpw_cl_label(kLabelManager, gGame.mResourceManager);
    tpw_cl_label(kLabelResource, gResource);
    for (int i = 0; i < kStorages; ++i) tpw_cl_label(kLabelStorage + i, &gStorages[i]);
    for (int i = 0; i < kInfos; ++i) tpw_cl_label(kLabelInfo + i, &gInfos[i]);
    // (A list and its link base are one address here: the base is the list's primary base. The PowerPC side gives both of
    // its addresses the same label.)
    for (int i = 0; i < 16; ++i) tpw_cl_label(kLabelLinkBase + i, static_cast<CMapLinkBase*>(&gHLSpriteLists[i]));
    tpw_cl_label(kLabelLinkBase + 16, static_cast<CMapLinkBase*>(&gESPGlobalList));
}

// Makes the entry `index` of the file list (a directory entry and a storage copy made here, with labels) from a listing
// entry of the case: the words, the buffer, where mName points, the storage's identity and whether it is loaded.
void tpw_esp_make_file(int32_t index, const char* buffer, int32_t offset, int32_t storage, const uint32_t* words, int32_t loaded) {
    const bool was = on();
    tpw_cl_on = 0;
    CFileInfo* info = new CFileInfo;
    std::strcpy(info->mNameBuffer, buffer);
    info->mName = info->mNameBuffer + offset;
    info->mF00 = words[0];
    info->mAttributes = words[1];
    std::memcpy(info->mF08, &words[2], sizeof info->mF08);
    CFileStorage* copy = new CFileStorage;
    copy->mAccess = reinterpret_cast<bfl::FileSystem::_TbStorageAccess*>(static_cast<uintptr_t>(storage));
    newlab(info);
    newlab(copy);
    gESPFiles[index].mInfo = info;
    gESPFiles[index].mStorage = copy;
    gESPFiles[index].mLoaded = static_cast<uint8_t>(loaded);
    tpw_cl_on = was ? 1 : 0;
}

// Fills a scratch directory entry (the arguments of a direct LoadFromESPFile).
void tpw_esp_set_info(int32_t index, const char* buffer, int32_t offset, const uint32_t* words) {
    CFileInfo& info = gInfos[index];
    std::strcpy(info.mNameBuffer, buffer);
    info.mName = info.mNameBuffer + offset;
    info.mF00 = words[0];
    info.mAttributes = words[1];
    std::memcpy(info.mF08, &words[2], sizeof info.mF08);
}

// Makes a sprite in the slot, or in the sprite table at `table` (>= 0) with a sprite file: the values are in the order of
// DumpSprite. `values` = loaded, firstperson, f206, semi, records, customcount, custom[16], actioncount, 16 x (first,
// frames, views), then the file: present, count, records pointer, f08, f0c.
void tpw_esp_install(int32_t slot, int32_t table, const char* name, const int64_t* v) {
    const bool was = on();
    tpw_cl_on = 0;
    ESP_CSprite* sprite;
    if (table >= 0) {
        sprite = new ESP_CSprite;
        gESPSprites[table] = sprite;
    } else {
        sprite = reinterpret_cast<ESP_CSprite*>(gSlotBytes[slot]);
        gSlots[slot] = sprite;
    }
    std::memset(static_cast<void*>(sprite), 0xa5, sizeof(ESP_CSprite));
    int k = 0;
    sprite->mLoaded = static_cast<uint8_t>(v[k++]);
    sprite->mHasFirstPerson = static_cast<uint8_t>(v[k++]);
    sprite->mF206 = static_cast<uint8_t>(v[k++]);
    sprite->mSemitransparent = static_cast<uint8_t>(v[k++]);
    sprite->mRecordCount = static_cast<uint32_t>(v[k++]);
    sprite->mCustomCount = static_cast<uint16_t>(v[k++]);
    for (int i = 0; i < 16; ++i) sprite->mCustom[i] = static_cast<uint8_t>(v[k++]);
    sprite->mActionCount = static_cast<uint16_t>(v[k++]);
    for (int i = 0; i < 16; ++i) {
        sprite->mActions[i].mFirstFrame = static_cast<uint16_t>(v[k++]);
        sprite->mActions[i].mFrames = static_cast<uint8_t>(v[k++]);
        sprite->mActions[i].mViews = static_cast<uint8_t>(v[k++]);
    }
    std::snprintf(sprite->mName, sizeof sprite->mName, "%s", name);
    sprite->mFile = nullptr;
    if (v[k++] != 0) {
        CSpriteFileIO* file = new CSpriteFileIO;
        file->mCount = static_cast<int32_t>(v[k++]);
        file->mRecords = reinterpret_cast<SSpriteEngineData*>(static_cast<uintptr_t>(v[k++]));
        file->mF08 = static_cast<int32_t>(v[k++]);
        file->mF0C = static_cast<int32_t>(v[k++]);
        sprite->mFile = file;
        newlab(file);
    }
    if (table >= 0) newlab(sprite);
    else tpw_cl_label(kLabelSlot + slot, sprite);
    tpw_cl_on = was ? 1 : 0;
}

// The operations. 0 ESP_InitialiseAndLoadSprites(a, b)  1 ESP_LoadSprites(a)  2 ESP_FindESPFiles(name)  3 ESP_SortFileList
// 4 constructor on slot a (filled with 0xa5 first)  5 LoadFromESPFile(slot a, info b, storage c)  6 GetActionFrames(slot a,
// action b)  7 GetCustomData(slot a, block b; outputs preset to 0x5a5a5a5a, read with tpw_esp_out)
// 8 ESP_GetParticleSprite(a, b, c), answered as the byte offset of the record from 0x40000000 (the test divides it by the record size)
int64_t tpw_esp_op(int32_t op, int64_t a, int64_t b, int64_t c, const char* name) {
    ev("op", {op, a, b, c}, {name != nullptr ? name : ""});
    switch (op) {
    case 0: ESP_InitialiseAndLoadSprites(a != 0, b != 0); return 0;
    case 1: return ESP_LoadSprites(a != 0);
    case 2: {
        const TbDynamicStringTemplate<char> folder(name);
        return ESP_FindESPFiles(*gResource, folder);
    }
    case 3: ESP_SortFileList(); return 0;
    case 4: {
        std::memset(static_cast<void*>(gSlotBytes[a]), 0xa5, sizeof(ESP_CSprite));
        gSlots[a] = new (gSlotBytes[a]) ESP_CSprite;
        tpw_cl_label(kLabelSlot + static_cast<int32_t>(a), gSlots[a]);
        return 0;
    }
    case 5: return gSlots[a]->LoadFromESPFile(gInfos[b], gStorages[c]);
    case 6: return gSlots[a]->GetActionFrames(static_cast<int32_t>(b));
    case 7: {
        int32_t first = 0x5a5a5a5a, third = 0x5a5a5a5a, fourth = 0x5a5a5a5a;
        ACTS second = static_cast<ACTS>(0x5a5a5a5a);
        gSlots[a]->GetCustomData(static_cast<int32_t>(b), &first, &second, &third, &fourth);
        gOut[0] = static_cast<uint32_t>(first);
        gOut[1] = static_cast<uint32_t>(second);
        gOut[2] = static_cast<uint32_t>(third);
        gOut[3] = static_cast<uint32_t>(fourth);
        return 0;
    }
    case 8: {
        PTCLTexture* record = ESP_GetParticleSprite(static_cast<int32_t>(a), static_cast<int32_t>(b), static_cast<int32_t>(c));
        return static_cast<int64_t>(reinterpret_cast<uintptr_t>(record)) - 0x40000000;
    }
    default: return -1;
    }
}
int64_t tpw_esp_out(int32_t index) { return gOut[index]; }
// The host's record size (SSpriteEngineData is 0x20 here, 0x18 on the Mac), for the test to turn ESP_GetParticleSprite's pointer
// into a record index.
int32_t tpw_esp_record_stride() { return static_cast<int32_t>(sizeof(SSpriteEngineData)); }

// Writes the observable state as records (see the top of the file); the PowerPC side lists the same numbers in this order.
int32_t tpw_esp_dump() {
    gDumpCount = 0;
    for (Scalar* s = Scalars(); s->name != nullptr; ++s) {
        if (s->size == 1) Put(s->name, *static_cast<uint8_t*>(s->address));
        else {
            uint32_t word;
            std::memcpy(&word, s->address, 4);
            Put(s->name, word);
        }
    }
    Put("display_flags", reinterpret_cast<CDisplay*>(gDisplayStorage)->mFlags);
    PutText("path", gESPPathBuffer, 0x200);
    for (int i = 0; i < kESPGroups; ++i) {
        Put("group", i, "first", gESPGroups[i].mFirst);
        Put("group", i, "count", gESPGroups[i].mCount);
    }
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) Put("instances", r * 4 + c, "ptr", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gESPListInstances[r][c])));
    }
    Put("instances", 16, "ptr", static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gESPGlobalInstances)));
    for (int i = 0; i < 16; ++i) Put("list", i, "count", static_cast<uint32_t>(gHLSpriteLists[i].mCount));
    Put("list", 16, "count", static_cast<uint32_t>(gESPGlobalList.mCount));
    for (int i = 0; i < kESPMaxFiles; ++i) {
        const ESPFileEntry& entry = gESPFiles[i];
        Put("file", i, "storage", entry.mInfo != nullptr ? lab(entry.mStorage) : -9);   // a freed storage's address can be reused
        Put("file", i, "loaded", entry.mLoaded);
        DumpInfo("fileinfo", i, entry.mInfo);
    }
    int32_t sprites = gESPSpriteCount;
    if (sprites < 0) sprites = 0;
    if (sprites > kESPMaxSprites) sprites = kESPMaxSprites;   // the Mac's table; a store past it lands on other globals there
    for (int i = 0; i < sprites; ++i) DumpSprite("sprite", i, gESPSprites[i]);
    for (int i = 0; i < kSlots; ++i) {
        if (gSlots[i] != nullptr) DumpSprite("slot", i, gSlots[i]);
    }
    for (int i = 0; i < kInfos; ++i) DumpInfo("info", i, &gInfos[i]);
    return gDumpCount;
}
const Record* tpw_esp_dump_data() { return gDump; }
int32_t tpw_esp_record_size() { return static_cast<int32_t>(sizeof(Record)); }

}  // extern "C"
