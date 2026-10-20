// Test library for CSaveLoad::Save (unit v51020; libtpw_unit_CSaveLoad_Save.dylib, tests/test_CSaveLoad_Save.py), main app:
//   src/SaveLoad_CSaveLoad_SaveHeader_2_CSaveLoad_Save.cpp   CSaveLoad::Save
//
// Every callee outside the unit is a recording fake (the file system classes, the LbFile functions, the temporary allocator, zlib,
// SaveHeader, SaveModules, PreSaveOperations, GAME_ASSERTMSG, CTRACE); answers come from the test's queues. Buffers get an order
// number (BufId) so the log names them the same way on both sides. gMemoryTemporary is defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "SaveLoad.h"
#include "Debug.h"
#include "LbFile.h"
#include "TbError.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "platform/samsutils.h"

using namespace calllog;

SamsUtilities::UAllocatorTemporary* SamsUtilities::gMemoryTemporary = nullptr;

namespace {
void* gBufs[32];
int gBufCount = 0;
bool gSwallow = false;   // the base destructor that follows ~CDiscFile is not a call of the original
bool gQuiet = false;
SamsUtilities::UAllocatorTemporary* gExisting = nullptr;

int64_t BufId(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < gBufCount; i++)
        if (gBufs[i] == p) return i + 1;
    return -1;
}

int64_t KindOf(const bfl::FileSystem::CFile* f) {
    if (dynamic_cast<const bfl::FileSystem::CDiscFile*>(f) != nullptr) return 2;
    if (dynamic_cast<const bfl::FileSystem::CMemFile*>(f) != nullptr) return 1;
    return 0;
}
}  // namespace

namespace bfl::FileSystem {
// The access objects the files get (ids 1 and 2).
class FakeItem : public CAccessItem {
public:
    explicit FakeItem(int id) : mId(id) {}
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {int64_t(mId), BufId(dest), int64_t(size)});
        for (uint32_t i = 0; i < size; i++) static_cast<uint8_t*>(dest)[i] = static_cast<uint8_t>(i % 251);
        return static_cast<int32_t>(size);
    }
    int32_t Write(const void* source, uint32_t size) override {
        ev("Write", {int64_t(mId), BufId(source), int64_t(size)});
        return static_cast<int32_t>(ret("AccWrite"));
    }
    int mId;
};
FakeItem gMemItem(1);
FakeItem gDiscItem(2);

CAccessItem::CAccessItem() {}
CAccessItem::~CAccessItem() {}
int32_t CAccessItem::Read(void*, uint32_t) { return -1; }
int32_t CAccessItem::Write(const void*, uint32_t) { return -1; }
int32_t CAccessItem::Seek(int32_t, uint32_t) { return -1; }
int32_t CAccessItem::GetSize() { return 0; }
int32_t CAccessItem::GetPos() { return 0; }
const uint8_t* CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void CAccessItem::UnmapFile(const uint8_t*) {}
TbError CAccessItem::Flush() { return TbError(0); }
int32_t CAccessItem::GetMacFileReference() { return 0; }
void CAccessItem::Close() {}
void CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* CAccessItem::GetSemaphore() { return nullptr; }

CFile::CFile() { mImplementation = nullptr; }
CFile::~CFile() {
    if (gSwallow) {
        gSwallow = false;
        return;
    }
    ev("FileDtor", {});
}
void CFile::Close() { ev("FileClose", {KindOf(this)}); }
int32_t CFile::Seek(uint32_t offset, uint32_t whence) {
    ev("Seek", {KindOf(this), int64_t(static_cast<int32_t>(offset)), int64_t(whence)});
    return 0;
}
int32_t CFile::GetSize() {
    ev("GetSize", {KindOf(this)});
    return static_cast<int32_t>(ret("GetSize"));
}
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }

CDiscFile::CDiscFile() { ev("DiscCtor", {}); }
CDiscFile::~CDiscFile() {
    ev("DiscDtor", {});
    gSwallow = true;
}
TbError CDiscFile::Open(const char* name, uint32_t mode) {
    ev("DiscOpen", {int64_t(mode)}, {name});
    const int32_t result = static_cast<int32_t>(ret("DiscOpen"));
    if (result >= 0 && ret("discImpl") != 0) mImplementation = &gDiscItem;
    return TbError(result);
}

TbError CMemFile::Open(uint32_t size) {
    ev("MemOpen", {int64_t(size)});
    const int32_t result = static_cast<int32_t>(ret("MemOpen"));
    if (result >= 0 && ret("memImpl") != 0) mImplementation = &gMemItem;
    return TbError(result);
}
}  // namespace bfl::FileSystem

namespace SamsUtilities {
UAllocatorTemporary::UAllocatorTemporary() {
    if (!gQuiet) ev("AllocCtor", {});
}
UAllocatorTemporary::~UAllocatorTemporary() {}
uint8_t* UAllocatorTemporary::Allocate(uint32_t size) {
    ev("Alloc", {int64_t(size)});
    if (ret("Alloc") == 0) return nullptr;
    uint8_t* p = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size != 0 ? size : 1));
    if (gBufCount < 32) gBufs[gBufCount++] = p;
    return p;
}
void UAllocatorTemporary::Free(uint8_t*& block) {
    ev("Free", {BufId(block)});
    block = nullptr;
}
}  // namespace SamsUtilities

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    if (std::strstr(format, "%s") != nullptr) {
        va_list ap;
        va_start(ap, format);
        const char* a = va_arg(ap, const char*);
        va_end(ap);
        ev("assert", {int64_t(condition)}, {format, a});
    } else {
        ev("assert", {int64_t(condition)}, {format});
    }
}

void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%s") != nullptr) {
        va_list ap;
        va_start(ap, format);
        const char* a = va_arg(ap, const char*);
        va_end(ap);
        ev("trace", {int64_t(level)}, {format, a});
    } else {
        ev("trace", {int64_t(level)}, {format});
    }
}

TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    ev("LbFile_Open", {int64_t(mode)}, {name});
    const int32_t result = static_cast<int32_t>(ret("LbOpen"));
    file = reinterpret_cast<void*>(static_cast<uintptr_t>(ret("LbHandle")));
    return TbError(result);
}
TbError LbFile_Write(void* file, const void*, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {int64_t(reinterpret_cast<uintptr_t>(file)), int64_t(length)});
    const int32_t result = static_cast<int32_t>(ret("LbWrite"));
    if (transferred != nullptr) *transferred = length;
    return TbError(result);
}
TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {int64_t(reinterpret_cast<uintptr_t>(file))});
    return TbError(0);
}
TbError LbFile_Delete(const char* name) {
    ev("LbFile_Delete", {}, {name});
    return TbError(0);
}
uint32_t LbZLib_MemCompress(void* source, void* dest, uint32_t length, void* workspace) {
    ev("Compress", {BufId(source), BufId(dest), int64_t(length), int64_t(reinterpret_cast<uintptr_t>(workspace))});
    return static_cast<uint32_t>(ret("Compress"));
}

int32_t CSaveLoad::SaveHeader(void* file, CSaveLoadHeader* header) {
    ev("SaveHeader", {int64_t(reinterpret_cast<uintptr_t>(file)), int64_t(header != nullptr)});
    return static_cast<int32_t>(ret("SaveHeader"));
}
void CSaveLoad::PreSaveOperations() { ev("PreSave", {}); }
int32_t CSaveLoad::SaveModules(void* file, LoadType type) {
    ev("SaveModules", {KindOf(static_cast<bfl::FileSystem::CFile*>(file)), int64_t(type)});
    return static_cast<int32_t>(ret("SaveModules"));
}

extern "C" {

uint64_t tpw_sl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMemTemp", &SamsUtilities::gMemoryTemporary},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Starts a case: forgets the buffers; exists = 1 gives the global an allocator already, 0 leaves it null.
void tpw_sl_reset(int32_t exists) {
    gBufCount = 0;
    gSwallow = false;
    if (gExisting == nullptr) {
        gQuiet = true;
        gExisting = new SamsUtilities::UAllocatorTemporary;
        gQuiet = false;
    }
    SamsUtilities::gMemoryTemporary = exists != 0 ? gExisting : nullptr;
}

int32_t tpw_sl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSaveLoad, mError));
    return n;
}

uint32_t tpw_sl_Save(uint64_t self, uint64_t path, uint64_t header, int32_t type) {
    return static_cast<uint32_t>(reinterpret_cast<CSaveLoad*>(self)->Save(
        reinterpret_cast<const char*>(path), reinterpret_cast<CSaveLoadHeader*>(header), static_cast<CSaveLoad::LoadType>(type)));
}

int32_t tpw_sl_has_allocator() { return SamsUtilities::gMemoryTemporary != nullptr; }

}  // extern "C"
