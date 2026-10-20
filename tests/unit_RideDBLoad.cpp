// Unit w119b: all eleven loader functions are real. External callees record their complete arguments.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "TPEngine.h"
#include "TPCoaster.h"
#include "CoasterBalance.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSound.h"
#include "TPString.h"
#include "rsse.h"
#include "calllog.h"

// Fill every game allocation so constructor omissions remain observable.
static void ObserveAllocation(std::size_t size, bool, void* object) {
    std::memset(object, 0xa5, size);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_CALLLOG_LABELS 4096
#include "calllog.cpp"
#define TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
#include "fake_strings.h"
#define TPW_RECORDED_ACCESS_IO
#include "fake_file_slots.h"
using namespace calllog;
using namespace bfl::FileSystem;

TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CExitError gExitError;
#if defined(__APPLE__)
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
#else
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("gGame");
#endif
alignas(CGame) unsigned char gGameStorage[sizeof(CGame)];
const char16_t gRideMissingName[] = u"MISSING STRING";
tag_sRideAnimSet* gRideTypes[kMaxRideTypes];
uint32_t gExtrudeFaceCountA;
uint32_t gExtrudeFaceCountB;
CGameType::CGameType() : mType(kGameTypeNormal) {}

namespace {
CRideDatabase gDB;
RideInfo* gSubject;
RideInfo* gLatest;
RideShape gShape;
tag_sCoasterType gCoaster;
sDTextureSet gTextures;
tag_sRideAnimSet gSet;
CCoasterBalance gBalance;
CTPResourceManager gManagers[2];
char gRoot[512];
char16_t gFileText[2048];
int32_t gTextBytes;
char16_t gOwned[4][128];
struct Entry { char name[300]; int32_t dir; int32_t wad; };
Entry gEntries[3][16];
int32_t gEntryCount[3];
int32_t gNext[3];
uint32_t gSlotValues[160];
char gSubNames[8][128];
int32_t gStorageIndex;
int32_t gLoadCalls;
int32_t gEntryIndex;
int32_t gManagerCalls;
int32_t gKind;
CFile* gCurrentFile;
CAccessItem gAccess;
alignas(CTPItemListResource) unsigned char gListBytes[sizeof(CTPItemListResource)];
alignas(CTPItemResource) unsigned char gItemBytes[sizeof(CTPItemResource)];
CTPItemListResource& List() { return *reinterpret_cast<CTPItemListResource*>(gListBytes); }
CTPItemResource& Item() { return *reinterpret_cast<CTPItemResource*>(gItemBytes); }
CGame& Game() { return *reinterpret_cast<CGame*>(gGameStorage); }

class Storage : public CFileStorage {
public:
    int32_t index;
};
Storage gStorages[3];
TbPtrList<CFileStorage> gStorageList;

// FileInfo's name follows a prefix in the buffer, rather than starting at its beginning.
void FillInfo(CFileInfo& info, int32_t storage, int32_t entry) {
    const Entry& source = gEntries[storage][entry];
    std::snprintf(info.mNameBuffer, sizeof info.mNameBuffer, "pre/%s", source.name);
    info.mName = info.mNameBuffer + 4;
    info.mAttributes = source.dir;
    info.mF00 = source.wad;
}
void FillBalance(CBalanceTable* table) {
    if (table->GetBalanceDescription(0) == gCoasterBalanceDescriptions) return;
    BalanceValue* values = table->GetBalanceValuesStart();
    for (int32_t i = 0; i < 160; ++i) values[i].mInteger = static_cast<int32_t>(gSlotValues[i]);
    values[kRideSlot_Shape].mShape = &gShape;
    values[kRideSlot_Hoarding].mShape = nullptr;
    for (int32_t i = 0; i < 8; ++i) values[kRideSlot_SupplementalMeshes_FileName + i].mString = gSubNames[i];
}
} // namespace

// Base storage slots: directory entries and root path are recorded, unused slots are inert.
CFileStorage::CFileStorage() : mAccess(nullptr) {}
CFileStorage::~CFileStorage() {}
void CFileStorage::Close() {}
TbError CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    const int32_t index = this == &Item().mStorage ? 0 : static_cast<Storage*>(this)->index;
    ev("FindFirst", {lab(this)}, {pattern});
    gNext[index] = 1;
    gStorageIndex = index;
    gEntryIndex = 0;
    const int32_t result = static_cast<int32_t>(ret("FindFirst", gEntryCount[index] > 0 ? 0 : -1));
    if (result >= 0) FillInfo(info, index, 0);
    return TbError(result);
}
TbError CFileStorage::FindNext(CFileInfo& info) {
    const int32_t index = this == &Item().mStorage ? 0 : static_cast<Storage*>(this)->index;
    ev("FindNext", {lab(this)});
    const int32_t next = gNext[index]++;
    gStorageIndex = index;
    gEntryIndex = next;
    const int32_t result = next < gEntryCount[index] ? static_cast<int32_t>(ret("FindNext", 0)) : -1;
    if (result >= 0) FillInfo(info, index, next);
    return TbError(result);
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    ev("OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
    return TbError(static_cast<int32_t>(ret("OpenFile")));
}
const char* CFileStorage::GetRootPath() {
    ev("GetRootPath", {lab(this)});
    return ret("null root") ? nullptr : gRoot;
}
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) { return TbError(0); }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError CFileStorage::Delete(const char*) { return TbError(0); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(0); }

CFile::CFile() : mImplementation(&gAccess) {
    gCurrentFile = this;
    newlab(this);
}
CFile::~CFile() {}
void CFile::Close() { ev("Close", {lab(this)}); }
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void* dest, uint32_t size) {
    ev("Read", {lab(gCurrentFile), lab(dest), size});
    if (dest) std::memcpy(dest, gFileText, std::min<uint32_t>(size, sizeof gFileText));
    return static_cast<int32_t>(ret("Read", gTextBytes));
}
int32_t CAccessItem::Write(const void*, uint32_t) { return 0; }
int32_t CFile::GetSize() {
    ev("GetSize", {lab(this)});
    return static_cast<int32_t>(ret("GetSize", gTextBytes));
}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }

// Resource objects expose the exact strings passed to their original nonvirtual members.
CTPItemResource::CTPItemResource(const CFileInfo& info, const TbDynamicStringTemplate<char>& path) {
    newlab(this);
    ev("ItemCtor", {lab(this)}, {info.mName, path.mData});
    mName = info.mName;
    mPath = path;
    forget(&mStorage);
    tpw_cl_label(lab(this) + 1000, &mStorage);
}
CTPItemResource::~CTPItemResource() { ev("ItemDtor", {lab(this), 1}); }
#define ITEM_FILE(method) \
int32_t CTPItemResource::method(CFile& file) { \
    ev(#method, {lab(this), lab(&file)}); \
    return static_cast<int32_t>(ret(#method)); \
}
ITEM_FILE(GetSamFile)
ITEM_FILE(GetEasySamFile)
ITEM_FILE(GetOnlineSamFile)
ITEM_FILE(GetCoasterSamFile)
ITEM_FILE(GetTextFile)
int32_t CTPItemResource::HasSounds() {
    ev("HasSounds", {lab(this)});
    return static_cast<int32_t>(ret("HasSounds"));
}
int32_t CTPItemResource::HasParticles() {
    ev("HasParticles", {lab(this)});
    return static_cast<int32_t>(ret("HasParticles"));
}
int32_t CTPIntermediate::GetSamFile(CFileStorage& storage, CFile& file) const {
    ev("ListSam", {lab(this), lab(&storage), lab(&file)});
    return static_cast<int32_t>(ret("ListSam"));
}
int32_t CTPIntermediate::GetOnlineSamFile(CFileStorage& storage, CFile& file) const {
    ev("ListOnline", {lab(this), lab(&storage), lab(&file)});
    return static_cast<int32_t>(ret("ListOnline"));
}
int32_t CTPItemListResource::FindItem(const TbDynamicStringTemplate<char>& name) const {
    ev("FindItem", {lab(this)}, {name.mData});
    return static_cast<int32_t>(ret("FindItem"));
}
int32_t CTPItemListResource::RegisterItem(CTPItemResource& item) {
    ev("RegisterItem", {lab(this), lab(&item)});
    item.mList = this;
    return static_cast<int32_t>(ret("RegisterItem", 1));
}
int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** output) {
    ev("FindResource", {lab(this)}, {name.mData});
    const int32_t result = static_cast<int32_t>(ret("FindResource"));
    if (result == 1) *output = &List();
    if (ret("manager hook")) Game().mResourceManager = &gManagers[(++gManagerCalls) & 1];
    return result;
}
CTPResourceManager::CTPResourceManager() {}
CTPResourceManager::~CTPResourceManager() {}
bool FS_IsSubDirectory(const CFileInfo& info) {
    ev("IsDir", {}, {info.mName});
    return info.mAttributes != 0;
}
bool FS_IsWadFile(const CFileInfo& info) {
    ev("IsWad", {}, {info.mName});
    return info.mF00 != 0;
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("AddSeparator", {}, {path.mData});
    path += ":";
}
bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    ev("StrEq", {}, {a.mData, b.mData});
    return ret("StrEq", std::strcmp(a.mData, b.mData) == 0) != 0;
}
// Extension removal is functional; the final-character indexing remains in the real loader.
TbDynamicStringTemplate<char>& RemoveExtension(const TbDynamicStringTemplate<char>& extension,
                                               TbDynamicStringTemplate<char>& name) {
    ev("RemoveExtension", {}, {extension.mData, name.mData});
    const int32_t n = name.mLength;
    const int32_t suffix = extension.mLength;
    if (n >= suffix && strcasecmp(name.mData + n - suffix, extension.mData) == 0) {
        name = name.Left(n - suffix);
    }
    return name;
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("Assert", {condition}, {format}); }
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) { ev("SetError", {lab(this), type, value}, {text}); }
void CBalanceTable::Init() {
    const bool coaster = GetBalanceDescription(0) == gCoasterBalanceDescriptions;
    if (coaster) tpw_cl_label(50, this);
    else {
        gLatest = reinterpret_cast<RideInfo*>(reinterpret_cast<char*>(this) - offsetof(RideInfo, mBalance));
        newlab(gLatest);
        tpw_cl_label(lab(gLatest) + 1000, this);
    }
    ev("Init", {lab(this)});
    mRows = nullptr;
    if (!coaster) FillBalance(this);
}
void CBalanceTable::UnInit() { ev("UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* name, CFile& file, LoadMode mode) {
    ev("Load", {lab(this), lab(&file), mode}, {name});
    ++gLoadCalls;
    return static_cast<int32_t>(ret("Load", 1));
}
bool CRideNameDB::GetName(int32_t id, char16_t* buffer, int32_t size) {
    ev("GetName", {lab(this), id, size});
    const char16_t text[] = u"built name";
    std::copy(std::begin(text), std::end(text), buffer);
    return true;
}
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t count,
                     const char16_t* signA, const char16_t* signB, int32_t width, int32_t height) {
    ev("LoadRideType", {lab(a), lab(b), flags, id, count, lab(signA), lab(signB), width, height}, {folder, name});
    const uint32_t result = static_cast<uint32_t>(ret("LoadRideType", 5));
    if (gLatest && flags == 0xc0 && ret("sub hook", -1) >= 0) gLatest->mSubRideCount = static_cast<int32_t>(ret("sub hook", -1));
    gRideTypes[result % kMaxRideTypes] = &gSet;
    return result;
}
void CRideDatabase::DeleteCustomEffects() { ev("DeleteEffects", {lab(this)}); }
int32_t SOUND_RegisterCatagory(const char* path, const char* name) {
    ev("RegisterSound", {}, {path, name});
    return static_cast<int32_t>(ret("RegisterSound"));
}
void SOUND_FreeCatagory(int32_t category) { ev("FreeSound", {category}); }
int32_t PAR_LoadLibEmitter(CFile* file) {
    ev("LoadEmitter", {lab(file)});
    return static_cast<int32_t>(ret("LoadEmitter"));
}
void PAR_KillLibEmitter(uint32_t emitter) { ev("KillEmitter", {emitter}); }
void* LbMem_Alloc(uint32_t size) {
    ev("Alloc", {size});
    if (ret("Alloc", 1) == 0) return nullptr;
    void* result = std::calloc(1, std::min<uint32_t>(size, 8192) + 16);
    newlab(result);
    return result;
}
void LbMem_Free(void* block) { ev("Free", {lab(block)}); }
void SamsUtilities::_USwapBlock16(uint16_t* block, int32_t count) { ev("Swap16", {lab(block), count}); }
int32_t find_string_id(const char16_t* text) {
    ev("FindString", {}, {narrow(text).c_str()});
    return static_cast<int32_t>(ret("FindString", -1));
}
const char16_t* find_line(const char16_t* text) {
    ev("FindLine", {}, {narrow(text).c_str()});
    return text + ret("FindLine", 1);
}
void copy_line(const char16_t** output, const char16_t* text) {
    ev("CopyLine", {lab(output)}, {narrow(text).c_str()});
    *output = gOwned[ret("CopyLine") & 3];
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    ev("Trace", {level, va_arg(args, int32_t)}, {format});
    va_end(args);
}
tag_sCoasterType* LoadCoasterTypeDataFile(uint32_t type, char* folder, char* texturePath, CCoasterBalance& balance) {
    ev("CoasterData", {type, lab(&balance)}, {folder, texturePath});
    return &gCoaster;
}
void CreateTrackSectionMesh(tag_sCoasterType* coaster) { ev("TrackMesh", {lab(coaster)}); }
void SetupTrainType(tag_sCoasterType* coaster, char* folder) { ev("Train", {lab(coaster)}, {folder}); }
void LoadCoasterPylons(tag_sCoasterType* coaster, sPylonControl* a, sPylonControl* b, char* folder) {
    ev("Pylons", {lab(coaster), lab(a), lab(b)}, {folder});
}

extern "C" {
// All setup happens with recording off. Raw resource storage avoids unrelated constructors.
void* tpw_rd_addr(int32_t which) {
    if (which == 0) return &gDB;
    if (which == 1) return gSubject;
    if (which == 2) return &List();
    if (which == 3) return &Item();
    if (which >= 4 && which <= 6) return &gStorages[which - 4];
    if (which == 7) return &Item().mStorage;
    if (which == 8) return &gDB.mNames;
    if (which == 9) return &gExitError;
    if (which == 10) return &gManagers[0];
    if (which == 11) return &gManagers[1];
    if (which == 12) return &gCoaster;
    if (which == 13) return &gCoaster.mPylonsA;
    if (which == 14) return &gCoaster.mPylonsB;
    if (which == 15) return &gBalance;
    if (which == 16) return &gShape;
    if (which >= 20 && which < 24) return gOwned[which - 20];
    if (which == 24) return const_cast<char16_t*>(gRideMissingName);
    if (which == 25) return &gSubject->mBalance;
    if (which >= 30 && which < 360) return gDB.mRides[which - 30];
    if (which >= 1000 && which < 1004) return &gSubject->mStrings[which - 1000];
    if (which >= 2000 && which < 2330) return &gDB.mRides[which - 2000]->mBalance;
    return nullptr;
}
void tpw_rd_reset(int32_t kind, int32_t type, const char* name, const char* path, const char* root) {
    tpw_cl_on = 0;
    gKind = kind;
    gStorageList.RemoveAll();
    std::memset(&gDB, 0xa5, sizeof gDB);
    std::fill(std::begin(gDB.mRides), std::end(gDB.mRides), nullptr);
    gDB.mRideCount = 0;
    gDB.mCachedId = 0;
    gDB.mCachedRide = nullptr;
    gLatest = nullptr;
    gSubject = new RideInfo;
    std::memset(gSubject->mBalance.mValues, 0xa5, sizeof gSubject->mBalance.mValues);
    gSubject->mBalance.mRows = nullptr;
    List().mStorages = &gStorageList;
    new (&List().mName) TbDynamicStringTemplate<char>("Rides");
    new (&List().mPath) TbDynamicStringTemplate<char>(path);
    new (gItemBytes) CTPItemResource(CFileInfo(), TbDynamicStringTemplate<char>(path));
    Item().mName = name;
    Item().mPath = path;
    std::snprintf(gRoot, sizeof gRoot, "%s", root);
    TheGameType().mType = static_cast<GameType>(type);
    Game().mResourceManager = &gManagers[0];
    gManagerCalls = 0;
    gLoadCalls = 0;
    for (int32_t i = 0; i < 3; ++i) {
        gStorages[i].index = i;
        gEntryCount[i] = 0;
        gNext[i] = 0;
    }
    std::memset(&gShape, 0, sizeof gShape);
    std::memset(&gCoaster, 0xa5, sizeof gCoaster);
    gCoaster.mTextures = &gTextures;
    gTextures.mCount = 0;
    gCoaster.mF3C = 0;
    gCoaster.mF2E8 = 0;
    gCoaster.mF2F8 = 0;
    gExtrudeFaceCountA = 0;
    gExtrudeFaceCountB = 0;
    gSet.mFlags = 0;
    for (auto& rideType : gRideTypes) rideType = &gSet;
    std::fill(std::begin(gSlotValues), std::end(gSlotValues), 0xa5a5a5a5);
    for (int32_t i = 0; i < 8; ++i) std::snprintf(gSubNames[i], sizeof gSubNames[i], "mesh%d", i);
    gTextBytes = 0;
    std::memset(gFileText, 0, sizeof gFileText);
}
void tpw_rd_global(int32_t index, const char* text) {
    TbDynamicStringTemplate<char>* strings[] = {&gTPGlobals.mWadFolderPattern, &gTPGlobals.mOnlinePrefix,
        &gTPGlobals.mF427C, &gTPGlobals.mF42A4, &gTPGlobals.mAllFiles, &gTPGlobals.mF45B0, &gTPGlobals.mF43A8,
        &gTPGlobals.mF4650, &gTPGlobals.mF4740, &gTPGlobals.mF4704, &gTPGlobals.mF4718, &gTPGlobals.mF472C};
    *strings[index] = text;
}
void tpw_rd_slot(int32_t slot, uint32_t value) {
    gSlotValues[slot] = value;
    gSubject->mBalance.mValues[slot].mInteger = static_cast<int32_t>(value);
}
void tpw_rd_shape(int32_t width, int32_t height, const uint8_t* bytes) {
    gShape.mWidth = width;
    gShape.mHeight = height;
    std::memcpy(gShape.mCells, bytes, sizeof gShape.mCells);
    gSubject->Value(kRideSlot_Shape).mShape = &gShape;
}
void tpw_rd_storage(int32_t count) {
    if (count < 0) List().mStorages = nullptr;
    else for (int32_t i = 0; i < count; ++i) gStorageList.AddTail(&gStorages[i]);
}
void tpw_rd_entry(int32_t storage, const char* name, int32_t dir, int32_t wad) {
    Entry& entry = gEntries[storage][gEntryCount[storage]++];
    std::snprintf(entry.name, sizeof entry.name, "%s", name);
    entry.dir = dir;
    entry.wad = wad;
}
void tpw_rd_text(const uint16_t* text, int32_t size) {
    gTextBytes = size;
    std::memcpy(gFileText, text, std::min<int32_t>(std::max(size, 0), sizeof gFileText));
}
void tpw_rd_fields(const uint32_t* words, const int32_t* strings) {
    gSubject->mFlags = words[0];
    for (int32_t i = 0; i < 6; ++i) gSubject->mEmitterWords[i] = words[1 + i];
    for (int32_t i = 0; i < 4; ++i) {
        gSubject->mStrings[i] = strings[i] == -1 ? gRideMissingName : strings[i] == 0 ? nullptr : gOwned[strings[i] - 1];
    }
}
void tpw_rd_database(int32_t count, int32_t cached, int32_t cacheIndex, const uint32_t* ids, const uint16_t* name) {
    for (int32_t i = 0; i < std::max(0, count); ++i) {
        RideInfo* ride = new RideInfo;
        ride->mId = ids[i] & 0xffff;
        std::memset(ride->mEmitterWords, 0, sizeof ride->mEmitterWords);
        std::fill(std::begin(ride->mStrings), std::end(ride->mStrings), nullptr);
        ride->mName = ids[i] & 0x10000 ? gRideMissingName : ids[i] & 0x20000 ? nullptr : reinterpret_cast<const char16_t*>(name);
        gDB.mRides[i] = ride;
    }
    gDB.mRideCount = count;
    gDB.mCachedId = cached;
    gDB.mCachedRide = cacheIndex < 0 ? nullptr : gDB.mRides[cacheIndex];
}
void tpw_rd_coaster(uint32_t flags, uint32_t a, uint32_t b, int32_t faces, uint16_t textures, int32_t x, int32_t y) {
    gSet.mFlags = flags;
    gExtrudeFaceCountA = a;
    gExtrudeFaceCountB = b;
    gCoaster.mF3C = faces;
    gTextures.mCount = textures;
    gCoaster.mF2E8 = x;
    gCoaster.mF2F8 = y;
}
int32_t tpw_rd_call(int32_t kind, int32_t argument) {
    if (kind == 0) return gDB.LoadItem(List(), gStorages[0], Item());
    if (kind == 1) return gDB.LoadTheme(reinterpret_cast<const char*>(static_cast<uintptr_t>(argument)));
    if (kind == 2) return gDB.ScanDirectory(List());
    if (kind == 3) gDB.ProcessShape(&gSubject);
    if (kind == 4) { CFile file; gSubject->LoadStrings(file); }
    if (kind == 5) gSubject->LoadSounds(gStorages[0]);
    if (kind == 6) gSubject->LoadParticles(gStorages[0]);
    if (kind == 7) gSubject->SetIsCustomRide();
    if (kind == 8) gSubject->DeallocStrings();
    if (kind == 9) {
        if (static_cast<int16_t>(argument) > 0) delete gSubject;
        else gSubject->~RideInfo();
    }
    if (kind == 10) LoadCoasterType(argument, Item().mPath.mData, gBalance);
    return 0;
}
// Canonical state uses Mac word order; pointers are labels and narrow/wide buffers are full arrays.
int32_t tpw_rd_dump(uint32_t* output, uint8_t* text) {
    RideInfo* ride = gKind <= 2 ? gLatest : gSubject;
    int32_t n = 0;
    output[n++] = gDB.mRideCount;
    output[n++] = gDB.mCachedId;
    output[n++] = lab(gDB.mCachedRide);
    output[n++] = gDB.mF72C;
    output[n++] = gSet.mFlags;
    output[n++] = gExtrudeFaceCountA;
    output[n++] = gExtrudeFaceCountB;
    output[n++] = gCoaster.mF318;
    output[n++] = ride != nullptr && gKind != 9;
    if (ride && gKind != 9) {
        output[n++] = ride->mId;
        for (int32_t i = 0; i < 160; ++i) {
            const bool pointer = i == kRideSlot_Shape || i == kRideSlot_Hoarding || (i >= 142 && i < 150);
            output[n++] = pointer ? lab(ride->mBalance.mValues[i].mShape) : static_cast<uint32_t>(ride->mBalance.mValues[i].mInteger);
        }
        output[n++] = ride->mRideTypeNumber;
        output[n++] = ride->mEntryX;
        output[n++] = ride->mEntryY;
        std::memcpy(output + n++, &ride->mEntryFlag, 4);
        output[n++] = ride->mExitX;
        output[n++] = ride->mExitY;
        std::memcpy(output + n++, &ride->mExitFlag, 4);
        output[n++] = ride->mUIType;
        output[n++] = ride->mMapOffsetX;
        output[n++] = ride->mMapOffsetY;
        output[n++] = ride->mSubRideCount;
        for (uint32_t value : ride->mSubRideTypes) output[n++] = value;
        for (uint32_t value : ride->mEmitterWords) output[n++] = value;
        for (const auto* value : ride->mStrings) {
            const int32_t label = lab(value);
            output[n++] = label < 0 ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)) : label;
        }
        output[n++] = ride->mFlags;
        output[n++] = ride->mLoadFailed;
        std::memcpy(text, ride->mFolder, 1024);
        std::memcpy(text + 1024, ride->mItemName, 128);
        std::memcpy(text + 1152, ride->mTypeName, 128);
    }
    return n;
}
void tpw_rd_slots(int32_t* output) {
    for (int32_t i = 0; i < 330; ++i) output[i] = lab(gDB.mRides[i]);
}
const BalanceDescription* tpw_rd_description(int32_t which, int32_t row) {
    CRideBalance ride;
    CCoasterBalance coaster;
    return which ? coaster.GetBalanceDescription(row) : ride.GetBalanceDescription(row);
}
int32_t tpw_rd_values(int32_t which) {
    CRideBalance ride;
    CCoasterBalance coaster;
    return which ? coaster.GetBalanceValuesStart() == coaster.mValues : ride.GetBalanceValuesStart() == ride.mValues;
}
}
