// w70: isolated sound maps. Imports are functional strings and logged file/storage/refcount fakes.
#include "TbMapStreamer.h"
#include "TbSoundManager.h"
#include "calllog.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// The Mac allocator fakes return zeroed array slots, and untouched record bytes are 0xa5.
static bool gArrayContext;
static uint32_t gArrayKind;
static uint32_t gArrayFill;
static bool gSoundHeap;
static bool gSuppressHeap;
struct HeapScope {
    bool previous = gSuppressHeap;
    HeapScope() { gSuppressHeap = true; }
    ~HeapScope() { gSuppressHeap = previous; }
};
static void zero_alloc(size_t size, bool array, void* block) {
    std::memset(block, 0, size);
    if (array && gArrayContext && gArrayKind < 3) {
        size_t width = gArrayKind == 0 ? 4 : sizeof(void*);
        uint64_t value = gArrayFill;
        for (size_t i = 0; i + width <= size; i += width) std::memcpy(static_cast<char*>(block) + i, &value, width);
    }
    if (!calllog::on() || gSuppressHeap) return;
    if (array && gArrayContext) {
        uint32_t mac_size;
        if (gArrayKind < 3) mac_size = size / (gArrayKind == 0 ? 4 : sizeof(void*)) * 4;
        else {
            // Apple arm64's array cookie is two size_t words; CodeWarrior's is eight bytes.
#if defined(__APPLE__) && defined(__arm64__)
            constexpr size_t cookie = 2 * sizeof(size_t);
#else
            constexpr size_t cookie = sizeof(size_t);
#endif
            if (size < cookie || (size - cookie) % sizeof(TbDynamicStringTemplate<char>) != 0) return;
            mac_size = (size - cookie) / sizeof(TbDynamicStringTemplate<char>) * 20 + 8;
        }
        calllog::ev("new[]", {mac_size, calllog::newlab(block)});
    }
    if (!array && !gArrayContext && gSoundHeap) {
        uint32_t mac_size = size == sizeof(CCatagoryContainer) ? 40 : size == sizeof(CBankContainer) ? 44 :
                            size == sizeof(CCatagoryContainer::CElement) ? 8 : 0;
        if (mac_size == 0) return;
        if (mac_size == 8 && calllog::ret("ElementAlloc", 1) == 0) {
            calllog::ev("new", {8, 0});
            std::free(block);
            throw std::bad_alloc();
        }
        calllog::ev("new", {mac_size, calllog::newlab(block)});
    }
}
#define TPW_ALLOCATION_OBSERVER zero_alloc
#define TPW_CALLLOG_LABELS 4096
#include "calllog.cpp"

namespace {
TbMapStreamer* gStreamer;
// The real TbSoundManager (unit w79e) is not linked here: the manager is raw storage of its size and only its string member is
// made (the streamer reads GetSoundPath()); GetBank below is this unit's recording fake.
alignas(TbSoundManager) uint8_t gManagerBytes[sizeof(TbSoundManager)];
TbSoundManager& gManager = *reinterpret_cast<TbSoundManager*>(gManagerBytes);
void mark_parsed(void*, const std::vector<uint8_t>&, uint32_t);
struct FileData : bfl::FileSystem::CAccessItem {
    std::vector<uint8_t> bytes;
    uint32_t position = 0;
    bfl::FileSystem::CFile* owner = nullptr;
    int32_t Read(void* out, uint32_t count) override {
        HeapScope scope;
        calllog::ev("FileRead", {calllog::lab(owner), calllog::lab(out), count});
        mark_parsed(out, bytes, position);
        uint32_t n = std::min(count, static_cast<uint32_t>(bytes.size()) - std::min(position, static_cast<uint32_t>(bytes.size())));
        if (n != 0) std::memcpy(out, bytes.data() + position, n);
        position += n;
        return n;
    }
};
struct FileEntry {
    std::string path;
    std::string name;
    std::vector<uint8_t> bytes;
};
std::vector<FileEntry> gFiles;
std::vector<void*> gMapBlocks;
class FakeBank : public TbRefRelease {};
std::vector<FakeBank*> gFakeBanks;
std::map<const void*, uint32_t> gParsedCounts;
void mark_parsed(void* out, const std::vector<uint8_t>& bytes, uint32_t position) {
    if (position == 28 && bytes.size() >= 28 && bytes[0] == 0xe9 && bytes[1] == 0x61 && bytes[2] == 0x2c && bytes[3] == 0) {
        uint32_t count = 0;
        for (uint32_t i = 24; i < 28; ++i) count = (count << 8) | bytes[i];
        gParsedCounts[out] = count;
    }
}
FileData* data(bfl::FileSystem::CFile* file) { return reinterpret_cast<FileData*>(file->mImplementation); }
const char* path(const bfl::FileSystem::CFileStorage* storage) {
    return storage->mAccess == nullptr ? "" : reinterpret_cast<const char*>(storage->mAccess);
}
void set_path(bfl::FileSystem::CFileStorage* storage, const char* text) {
    std::free(storage->mAccess);
    storage->mAccess = reinterpret_cast<bfl::FileSystem::_TbStorageAccess*>(::strdup(text));
}
}

// File-image blocks must live in the Ptr32 arena. Record the Mac's allocation size.
namespace tpw {
void* map_alloc(size_t size) {
    HeapScope scope;
    void* p = ArenaAlloc(size);
    std::memset(p, 0xa5, size);
    calllog::ev("MapAlloc", {static_cast<int64_t>(size), calllog::newlab(p)});
    gMapBlocks.push_back(p);
    return p;
}
void map_free(void* p) {
    calllog::ev("MapFree", {calllog::lab(p)});
    calllog::forget(p);
    gParsedCounts.erase(p);
    auto found = std::find(gMapBlocks.begin(), gMapBlocks.end(), p);
    if (found != gMapBlocks.end()) gMapBlocks.erase(found);
    ArenaFree(p);
}
}
#define TPW_SWAP_FILE_WORDS 1
#define ArenaAlloc map_alloc
#define ArenaFree map_free
// src/snd_containers.cpp of before the regroup: the definitions it had, in sliced copies of the files that hold them (TPW_UNIT_MapStreamer_INCLUDED).
#include "src/sound/TbMapStreamer.cpp"
// src/TbMapStreamer.cpp of before the regroup: the definitions it had, in sliced copies of the files that hold them (TPW_UNIT_MapStreamer_INCLUDED).
#include "src/sound/TbAudioBody.cpp"
#include "src/sound/TbSoundManager.cpp"
#undef ArenaAlloc
#undef ArenaFree

// Reference-count imports run the host virtual destructor when its last reference drops.
TbRefRelease::TbRefRelease() : mRefCount(1) {
    int32_t label = calllog::lab(this);
    if (label < 0) label = calllog::newlab(this);
    calllog::ev("RefCtor", {label});
}
TbRefRelease::~TbRefRelease() { calllog::ev("RefDtor", {calllog::lab(this)}); }
int32_t TbRefRelease::AddRef() {
    calllog::ev("AddRef", {calllog::lab(this), mRefCount});
    return ++mRefCount;
}
int32_t TbRefRelease::Release() {
    calllog::ev("Release", {calllog::lab(this), mRefCount});
    if (--mRefCount == 0) {
        auto found = std::find(gFakeBanks.begin(), gFakeBanks.end(), this);
        if (found != gFakeBanks.end()) gFakeBanks.erase(found);
        delete this;
        return 0;
    }
    return mRefCount;
}
TbDynamicStringTemplate<char>& TbSoundManager::GetSoundPath() { return mSoundPath; }
TbBank* TbSoundManager::GetBank(uint32_t index) {
    calllog::ev("GetBank", {calllog::lab(this), index});
    if (calllog::ret("GetBank", 0) != 0) {
        HeapScope scope;
        auto* bank = new FakeBank;
        gFakeBanks.push_back(bank);
        gStreamer->mBanks[index]->mBank = reinterpret_cast<TbBank*>(bank);
        return reinterpret_cast<TbBank*>(bank);
    }
    return nullptr;
}

namespace bfl::FileSystem {
// The storage copy keeps the directory string, and FullPath with an empty name returns it.
CFileStorage::CFileStorage() : mAccess(nullptr) {
    calllog::ev("StorageCtor", {calllog::newlab(this)});
}
CFileStorage::CFileStorage(const CFileStorage& other) : CFileStorage() { *this = other; }
CFileStorage& CFileStorage::operator=(const CFileStorage& other) {
    calllog::ev("StorageCopy", {calllog::lab(this), calllog::lab(&other)}, {path(&other)});
    if (this != &other) set_path(this, path(&other));
    return *this;
}
CFileStorage::~CFileStorage() {
    calllog::ev("StorageDtor", {calllog::lab(this)});
    std::free(mAccess);
    calllog::forget(this);
}
void CFileStorage::Close() {
    calllog::ev("StorageClose", {calllog::lab(this)});
    set_path(this, "");
}
TbError CDiscFileStorage::Open(const char* name) {
    calllog::ev("StorageOpen", {calllog::lab(this)}, {name});
    int32_t result = calllog::ret("StorageOpen", 0);
    if (result >= 0) set_path(this, name);
    return TbError(result);
}
TbError CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* extra) {
    HeapScope scope;
    calllog::ev("FileOpen", {calllog::lab(this), calllog::lab(&file), mode, calllog::lab(extra)}, {path(this), name});
    const int32_t result = calllog::ret("FileOpen", 0);
    if (result < 0) return TbError(result);
    for (const auto& entry : gFiles) {
        if (entry.path == path(this) && entry.name == name) {
            auto* f = new FileData;
            f->bytes = entry.bytes;
            f->owner = &file;
            file.mImplementation = reinterpret_cast<CAccessItem*>(f);
            return TbError(result);
        }
    }
    return TbError(-1);
}
TbError CFileStorage::FullPath(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char>& name) {
    calllog::ev("FullPath", {calllog::lab(this)}, {name.mData});
    out = path(this);
    out += name;
    return TbError(0);
}
TbError CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FindNext(CFileInfo&) { return TbError(-1); }
TbError CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(-1); }
const char* CFileStorage::GetRootPath() { return path(this); }
TbError CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(-1); }
TbError CFileStorage::CreateDir(const char*) { return TbError(-1); }
TbError CFileStorage::Delete(const char*) { return TbError(-1); }
TbError CFileStorage::Move(const char*, const char*) { return TbError(-1); }

// CFile has exactly the four operations the readers use, over a supplied byte buffer.
CFile::CFile() : mImplementation(nullptr) { calllog::ev("FileCtor", {calllog::newlab(this)}); }
CFile::~CFile() {
    calllog::ev("FileDtor", {calllog::lab(this)});
    delete data(this);
    calllog::forget(this);
}
void CFile::Close() {
    calllog::ev("FileClose", {calllog::lab(this)});
    delete data(this);
    mImplementation = nullptr;
}
int32_t CFile::GetSize() {
    calllog::ev("FileSize", {calllog::lab(this)});
    return data(this) == nullptr ? -1 : static_cast<int32_t>(data(this)->bytes.size());
}
int32_t CFile::GetPos() {
    calllog::ev("FilePos", {calllog::lab(this)});
    return data(this) == nullptr ? -1 : static_cast<int32_t>(data(this)->position);
}
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    calllog::ev("FileSeek", {calllog::lab(this), offset, whence});
    auto* f = data(this);
    f->position = offset + (whence == 1 ? f->position : whence == 2 ? f->bytes.size() : 0);
    return f->position;
}
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}

// The existing header inlines CFile::Read through CAccessItem. Use one access fake for both interfaces.
namespace bfl::FileSystem {
CAccessItem::CAccessItem() : mNext(nullptr), mReferences(1) {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void* out, uint32_t count) {
    return -1;
}
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return -1; }
int32_t CAccessItem::GetPos() { return -1; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(0); }
int32_t CAccessItem::GetMacFileReference() { return 0; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }
}

namespace {
struct Event : sound::TbAudioEvent {
    uint32_t category;
    uint32_t type;
    uint32_t GetCategory() const override {
        calllog::ev("Event.Category", {calllog::lab(this)});
        return category;
    }
    uint32_t GetType() const override { return 0; }
    uint32_t GetEvent() const override {
        calllog::ev("Event.Event", {calllog::lab(this)});
        return type;
    }
};
}

extern "C" {
void tpw_ms_begin() {
    tpw_cl_on = 0;
    if (gStreamer == nullptr) gStreamer = TbMapStreamer::Instance();
    for (uint32_t i = 1; i < gStreamer->mCatagories.Size(); ++i) {
        if (gStreamer->mCatagories.mData[i] != nullptr) {
            gStreamer->mCatagories.mData[i]->mRefCount = 1;
            gStreamer->RemoveCatagory(i);
        }
    }
    for (uint32_t i = 0; i < gStreamer->mBanks.Size(); ++i) {
        delete gStreamer->mBanks.mData[i];
        gStreamer->mBanks.mData[i] = nullptr;
    }
    // Live dummy banks are owned by the test. Released ones are removed by the refcount fake.
    for (auto* bank : gFakeBanks) delete bank;
    gFakeBanks.clear();
    gStreamer->~TbMapStreamer();
    for (void* block : gMapBlocks) tpw::ArenaFree(block);
    gMapBlocks.clear();
    std::memset(static_cast<void*>(gStreamer), 0xa5, sizeof(*gStreamer));
    gParsedCounts.clear();
    gFiles.clear();
    tpw_cl_reset(100);
    tpw_cl_label(1, gStreamer);
    tpw_cl_label(2, &gManager);
    tpw_cl_on = 1;
    gSoundHeap = true;
    new (gStreamer) TbMapStreamer;
    static bool managerMade = false;
    if (!managerMade) {
        new (&gManager.mSoundPath) TbDynamicStringTemplate<char>();
        managerMade = true;
    }
    gManager.mSoundPath = "ROOT:";
    gStreamer->SetSoundManager(&gManager);
}
void tpw_ms_file(const char* path, const char* name, const void* bytes, uint32_t size) {
    tpw_cl_on = 0;
    const auto* p = static_cast<const uint8_t*>(bytes);
    gFiles.push_back({path, name, std::vector<uint8_t>(p, p + size)});
    tpw_cl_on = 1;
}
void* tpw_ms_object(uint32_t kind, uint32_t i) {
    if (kind == 0) return gStreamer;
    if (kind == 1) return &gManager;
    if (kind == 2) return i < gStreamer->mCatagories.Size() ? gStreamer->mCatagories.mData[i] : nullptr;
    if (kind == 3) return i < gStreamer->mBanks.Size() ? gStreamer->mBanks.mData[i] : nullptr;
    if (kind == 4) {
        auto* c = static_cast<CCatagoryContainer*>(tpw_ms_object(2, i));
        return c == nullptr ? nullptr : c->mData;
    }
    if (kind == 5) {
        auto* c = static_cast<CCatagoryContainer*>(tpw_ms_object(2, i));
        return c == nullptr ? nullptr : c->mElements;
    }
    if (kind == 6) return tpw::gArenaBase;
    return nullptr;
}
uint32_t tpw_ms_get(uint32_t kind, uint32_t i) {
    switch (kind) {
    case 0: return gStreamer->mCatagories.Size();
    case 1: return gStreamer->mBanks.Size();
    case 2: return gStreamer->mNames.Size();
    case 3: return gStreamer->mBankIndices.Size();
    case 4: return gStreamer->mBankIndexCount;
    case 5: return gStreamer->mCurrentCatagory;
    case 6: return gStreamer->mOldVersion;
    case 7: return gStreamer->mBankIndices.mData[i];
    case 8: return static_cast<CCatagoryContainer*>(tpw_ms_object(2, i))->mIndex;
    case 9: return static_cast<CCatagoryContainer*>(tpw_ms_object(2, i))->mRefCount;
    case 10: return static_cast<CBankContainer*>(tpw_ms_object(3, i))->mFlags;
    case 11: return static_cast<CBankContainer*>(tpw_ms_object(3, i))->mRefCount;
    case 12: return static_cast<CBankContainer*>(tpw_ms_object(3, i))->mBank != nullptr;
    }
    return 0;
}
const char* tpw_ms_text(uint32_t kind, uint32_t i) {
    if (kind == 0) return path(&gStreamer->mStorage);
    if (kind == 1) return gStreamer->mNames.mData[i].mData;
    if (kind == 2) return static_cast<CCatagoryContainer*>(tpw_ms_object(2, i))->mName.mData;
    if (kind == 3) return static_cast<CBankContainer*>(tpw_ms_object(3, i))->mName.mData;
    if (kind == 4) return path(&static_cast<CBankContainer*>(tpw_ms_object(3, i))->mStorage);
    return "";
}
int32_t tpw_ms_op(uint32_t op, uint32_t a, uint32_t* index, const char* first, const char* second) {
    switch (op) {
    case 0: return gStreamer->SetPath(first);
    case 1: return gStreamer->RegisterCatagory(first, second, *index);
    case 2: return gStreamer->RegisterCatagory(first, *index);
    case 3: return gStreamer->RemoveCatagory(a);
    case 4: return gStreamer->UnloadCatagory(static_cast<CCatagoryContainer*>(tpw_ms_object(2, a)));
    case 5: return gStreamer->FindCatagory(first);
    case 6: return gStreamer->RegisterBankDataFromStorage(first, second);
    case 7: return gStreamer->RegisterSFXDataFromStorage(first, *index);
    }
    return -1;
}
uint32_t tpw_ms_scalar(uint32_t size, uint32_t value) {
    if (size == 2) return sound::SamsUtilities::USwap16(static_cast<uint16_t>(value));
    return sound::SamsUtilities::USwap32(value);
}
void tpw_ms_swap(uint32_t kind, void* data, uint32_t count) {
    switch (kind) {
    case 0: static_cast<TbGUID*>(data)->Byteswap();
        break;
    case 1: static_cast<CFileHeader*>(data)->Byteswap();
        break;
    case 2: static_cast<CBankDataMap*>(data)->Byteswap();
        break;
    case 3: static_cast<CSFXDataMap*>(data)->Byteswap();
        break;
    case 4: static_cast<CCatagoryData*>(data)->Byteswap();
        break;
    case 5: sound::SamsUtilities::USwapBlockStruct(static_cast<CEventData*>(data), count);
        break;
    case 6: sound::SamsUtilities::USwapBlockStruct(static_cast<CSoundData*>(data), count);
        break;
    case 7: sound::SamsUtilities::USwapBlockStruct(static_cast<CSampleData*>(data), count);
        break;
    case 8: sound::SamsUtilities::USwapBlockStruct(static_cast<CSoundBranchData*>(data), count);
        break;
    case 9: sound::SamsUtilities::USwapInplace32(*static_cast<uint32_t*>(data));
        break;
    case 10: sound::SamsUtilities::USwapInplace16(*static_cast<uint16_t*>(data));
        break;
    }
}
}

// Formatting is not called by w70; the linked string instantiations still need their slots.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

namespace {
CResize<uint32_t> gUIntArray;
CResize<CCatagoryContainer*> gCatArray;
CResize<CBankContainer*> gBankArray;
CResize<TbDynamicStringTemplate<char>> gStringArray;

template<class T> uint32_t array_op(CResize<T>& array, uint32_t op, uint32_t index, uint32_t value) {
    if (op == 0) return array.Size();
    if (op == 1 || op == 2) {
        T& slot = array[index];
        if (op == 2) {
            if constexpr (std::is_pointer_v<T>) slot = reinterpret_cast<T>(static_cast<uintptr_t>(value));
            else slot = value;
        }
        if constexpr (std::is_pointer_v<T>) return reinterpret_cast<uintptr_t>(slot);
        else return slot;
    }
    if (op == 3) array.Clear();
    if (op == 4) {
        uint32_t i = 0;
        while (i < array.Size() && array.mData[i] != T{}) ++i;
        return i;
    }
    return 0;
}
}
extern "C" {
void tpw_ms_array_begin(uint32_t kind, uint32_t fill) {
    tpw_cl_on = 0;
    gArrayContext = false;
    gUIntArray.Clear();
    gCatArray.Clear();
    gBankArray.Clear();
    gStringArray.Clear();
    tpw_cl_reset(100);
    gArrayContext = true;
    gArrayKind = kind;
    gArrayFill = fill;
    tpw_cl_on = 1;
}
uint32_t tpw_ms_array_op(uint32_t kind, uint32_t op, uint32_t index, uint32_t value, const char* text, uint32_t* found) {
    if (kind == 0) return array_op(gUIntArray, op, index, value);
    if (kind == 1) return array_op(gCatArray, op, index, value);
    if (kind == 2) return array_op(gBankArray, op, index, value);
    if (op == 0) return gStringArray.Size();
    if (op == 1) {
        gStringArray[index];
        return index;
    }
    if (op == 2) {
        gStringArray[index] = text;
        return index;
    }
    if (op == 3) gStringArray.Clear();
    if (op == 5) {
        TbDynamicStringTemplate<char> wanted(text);
        return gStringArray.Find(wanted, *found);
    }
    return 0;
}
uint32_t tpw_ms_array_get(uint32_t kind, uint32_t index) {
    if (kind == 0) return gUIntArray.mData[index];
    if (kind == 1) return reinterpret_cast<uintptr_t>(gCatArray.mData[index]);
    if (kind == 2) return reinterpret_cast<uintptr_t>(gBankArray.mData[index]);
    return 0;
}
const char* tpw_ms_array_text(uint32_t index) { return gStringArray.mData[index].mData; }
uint32_t tpw_ms_array_size(uint32_t kind) {
    if (kind == 0) return gUIntArray.Size();
    if (kind == 1) return gCatArray.Size();
    if (kind == 2) return gBankArray.Size();
    return gStringArray.Size();
}
float tpw_ms_float(float value) { return sound::SamsUtilities::USwapFloat(value); }
uint32_t tpw_ms_remove_path(char* text, uint32_t length) {
    return gStreamer->RemovePath(text, length) - text;
}
int32_t tpw_ms_header(void* data, uint32_t* old) {
    bool result = gStreamer->IsSFXHeaderValid(*static_cast<CFileHeader*>(data));
    *old = gStreamer->mOldVersion;
    return result;
}
int32_t tpw_ms_guid(void* first, void* second) {
    return *static_cast<TbGUID*>(first) == *static_cast<TbGUID*>(second);
}
void tpw_ms_header_ctor(void* data) { new (data) CFileHeader; }
void tpw_ms_fixup_ctor(void* data) { new (data) TbMapStreamer::CCatFixupTable; }
uint32_t tpw_ms_modify(void* event, void* sound) {
    auto* e = static_cast<CEventData*>(event);
    auto* s = static_cast<CSoundData*>(sound);
    e->mSounds = s;
    gStreamer->ModifyEventForSense(e);
    return e->mFlags;
}
int32_t tpw_ms_map(void* data, uint32_t category, uint32_t type, uint32_t null_category) {
    auto* c = static_cast<CCatagoryData*>(data);
    Event event;
    event.category = category;
    event.type = type;
    tpw_cl_label(3, &event);
    auto* answer = TbEventMapper::MapEvent(null_category ? nullptr : c, event);
    return answer == nullptr ? -1 : (answer - c->mEvents.get());
}
}

extern "C" {
void* tpw_ms_block(const void* data, uint32_t size) {
    void* block = tpw::ArenaAlloc(size);
    if (size) std::memcpy(block, data, size);
    return block;
}
void tpw_ms_block_free(void* block) { tpw::ArenaFree(block); }
void tpw_ms_block32(void* data, int32_t count) {
    auto* words = static_cast<uint32_t*>(data);
    for (int32_t i = 0; i < count; ++i) sound::SamsUtilities::USwapInplace32(words[i]);
}
}

namespace {
CCatagoryContainer* gContainer;

template<class T> void auto_test(uint32_t count, uint32_t no_object, uint32_t no_data, int32_t flag, uint32_t* out) {
    T* data = no_data ? nullptr : new T[count];
    auto* object = no_object ? nullptr : new CAutoArrayPtr<T>(data);
    tpw_cl_label(1, object);
    out[0] = object != nullptr;
    out[1] = object != nullptr && object->mData != nullptr;
    out[2] = object != nullptr && static_cast<T*>(*object) == data;
    if (object != nullptr) {
        if (static_cast<int16_t>(flag) > 0) delete object;
        else {
            object->~CAutoArrayPtr<T>();
            tpw_cl_on = 0;
            ::operator delete(object);
            tpw_cl_on = 1;
            // The wrapper's cleanup is outside the tested deleting-destructor path.
        }
    } else if (data != nullptr) {
        tpw_cl_on = 0;
        delete[] data;
        tpw_cl_on = 1;
    }
}
}
extern "C" {
void tpw_ms_auto(uint32_t kind, uint32_t count, uint32_t no_object, uint32_t no_data, int32_t flag, uint32_t* out) {
    tpw_cl_on = 0;
    tpw_cl_reset(100);
    tpw_cl_on = 1;
    if (kind == 0) auto_test<uint32_t>(count, no_object, no_data, flag, out);
    if (kind == 1) auto_test<CCatagoryContainer*>(count, no_object, no_data, flag, out);
    if (kind == 2) auto_test<CBankContainer*>(count, no_object, no_data, flag, out);
    if (kind == 3) auto_test<TbDynamicStringTemplate<char>>(count, no_object, no_data, flag, out);
}
void tpw_ms_container_begin(uint32_t index, uint32_t attach) {
    HeapScope scope;
    gContainer = new CCatagoryContainer(index);
    if (attach) gStreamer->mCatagories[index] = gContainer;
}
uint32_t tpw_ms_container_op(uint32_t op, uint32_t value) {
    if (op == 0) {
        gContainer->Add(value);
        if (gContainer->mElements != nullptr && calllog::lab(gContainer->mElements) < 0) calllog::newlab(gContainer->mElements);
        return 0;
    }
    if (op == 1) return gContainer->Find(value);
    if (op == 2) {
        if (gContainer->mElements != nullptr) gContainer->mElements->Release();
        gContainer->mElements = nullptr;
    }
    if (op == 3) {
        gContainer->~CCatagoryContainer();
        if (static_cast<int16_t>(value) > 0) ::operator delete(gContainer);
        else {
            tpw_cl_on = 0;
            ::operator delete(gContainer);
            tpw_cl_on = 1;
        }
        gContainer = nullptr;
    }
    return 0;
}
void* tpw_ms_container() { return gContainer; }
void* tpw_ms_elements() { return gContainer->mElements; }
uint32_t tpw_ms_catcount(uint32_t index) {
    auto* c = static_cast<CCatagoryContainer*>(tpw_ms_object(2, index));
    if (c == nullptr || c->mData == nullptr) return 0;
    auto found = gParsedCounts.find(c->mData);
    return found == gParsedCounts.end() ? 1 : found->second;
}
void tpw_ms_set_catcount(uint32_t index, uint32_t count) {
    auto* c = static_cast<CCatagoryContainer*>(tpw_ms_object(2, index));
    if (c != nullptr && c->mData != nullptr) gParsedCounts[c->mData] = count;
}
}

namespace {
template<class T> void array_dtor(CResize<T>& original, int32_t flag, uint32_t* out) {
    auto* object = new CResize<T>;
    object->mData = original.mData;
    object->mSize = original.mSize;
    original.mData = nullptr;
    original.mSize = 0;
    tpw_cl_label(1, object);
    object->~CResize<T>();
    out[0] = object->mData != nullptr;
    out[1] = object->mSize;
    if (static_cast<int16_t>(flag) <= 0) tpw_cl_on = 0;
    ::operator delete(object);
    tpw_cl_on = 1;
}
}
extern "C" void tpw_ms_array_dtor(uint32_t kind, int32_t flag, uint32_t* out) {
    if (kind == 0) array_dtor(gUIntArray, flag, out);
    if (kind == 1) array_dtor(gCatArray, flag, out);
    if (kind == 2) array_dtor(gBankArray, flag, out);
    if (kind == 3) array_dtor(gStringArray, flag, out);
    gArrayContext = false;
}

extern "C" {
void tpw_ms_destroy(uint32_t* arrays) {
    gStreamer->~TbMapStreamer();
    arrays[0] = gStreamer->mNames.mData != nullptr;
    arrays[1] = gStreamer->mCatagories.mData != nullptr;
    arrays[2] = gStreamer->mBanks.mData != nullptr;
    arrays[3] = gStreamer->mBankIndices.mData != nullptr;
    tpw_cl_on = 0;
    std::memset(static_cast<void*>(gStreamer), 0xa5, sizeof(*gStreamer));
    new (gStreamer) TbMapStreamer;
    tpw_cl_on = 1;
}
uint32_t tpw_ms_access(uint32_t kind) {
    if (kind == 0) return TbMapStreamer::Instance() == gStreamer;
    if (kind == 1) return &gStreamer->GetCatagoryDeque() == &gStreamer->mCatagories;
    if (kind == 2) return &gStreamer->GetBankDeque() == &gStreamer->mBanks;
    if (kind == 3) return &gManager.GetSoundPath() == &gManager.mSoundPath;
    if (kind == 4) {
        gStreamer->SetSoundManager(nullptr);
        const bool cleared = TbMapStreamer::mpSoundManager == nullptr;
        gStreamer->SetSoundManager(&gManager);
        return cleared && TbMapStreamer::mpSoundManager == &gManager;
    }
    return 0;
}
void tpw_ms_storage(int32_t flag, uint32_t no_object, uint32_t* out) {
    tpw_cl_reset(100);
    tpw_cl_on = 1;
    auto* s = no_object ? nullptr : new CStorageContainer;
    tpw_cl_label(1, s);
    out[0] = s == nullptr ? 0 : s->mValid;
    if (s != nullptr) {
        s->~CStorageContainer();
        if (static_cast<int16_t>(flag) <= 0) tpw_cl_on = 0;
        ::operator delete(s);
        tpw_cl_on = 1;
    }
}
}

extern "C" uint32_t tpw_ms_container_field(uint32_t kind) {
    if (kind == 0) return gContainer->mRefCount;
    if (kind == 1) return gContainer->mData != nullptr;
    if (kind == 2) return gContainer->mIndex;
    return 0;
}
extern "C" const char* tpw_ms_container_text() { return gContainer->mName.mData; }

extern "C" int32_t tpw_ms_map_cat(uint32_t index, uint32_t record, uint32_t event_id) {
    auto* object = static_cast<CCatagoryContainer*>(tpw_ms_object(2, index));
    return tpw_ms_map(object == nullptr || object->mData == nullptr ? nullptr : object->mData + record,
                      index, event_id, object == nullptr || object->mData == nullptr);
}

extern "C" void tpw_ms_array_empty_count(uint32_t kind, uint32_t count) {
    if (kind == 0) gUIntArray.mSize = count;
    if (kind == 1) gCatArray.mSize = count;
    if (kind == 2) gBankArray.mSize = count;
    if (kind == 3) gStringArray.mSize = count;
}
extern "C" uint32_t tpw_ms_array_present(uint32_t kind) {
    if (kind == 0) return gUIntArray.mData != nullptr;
    if (kind == 1) return gCatArray.mData != nullptr;
    if (kind == 2) return gBankArray.mData != nullptr;
    return gStringArray.mData != nullptr;
}

// The nothrow overload routes game element allocations through the same queued allocator fake.
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
void operator delete(void* pointer, const std::nothrow_t&) noexcept { ::operator delete(pointer); }

extern "C" void tpw_ms_reader_state(uint32_t old_version, const uint32_t* indices, uint32_t count) {
    gStreamer->mCurrentCatagory = 0;
    gStreamer->mOldVersion = old_version;
    for (uint32_t i = 0; i < count; ++i) gStreamer->mBankIndices[i] = indices[i];
}
extern "C" uint32_t tpw_ms_reader(uint32_t kind, void* block, uint32_t count, uint32_t* output) {
    char* base = static_cast<char*>(block);
    char* end;
    if (kind == 0) end = gStreamer->ReadSounds(base + 86, reinterpret_cast<CSoundData*>(base + 44));
    else if (kind == 1) end = gStreamer->ReadEvents(base + 44, reinterpret_cast<CEventData*>(base + 24));
    else if (kind == 2) end = gStreamer->ReadCatagory(base + 24, reinterpret_cast<CCatagoryData*>(base));
    else {
        CCatagoryData* cat;
        end = reinterpret_cast<char*>(gStreamer->ReadSampleData(reinterpret_cast<uint8_t*>(base), &cat, count));
        *output = reinterpret_cast<char*>(cat) - base;
    }
    return end - base;
}
