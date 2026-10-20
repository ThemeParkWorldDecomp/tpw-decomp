// Test library for TbMapStreamer::CCatFixupTable::Load (unit v5234; libtpw_unit_TbMapStreamer_CCatFixupTable_Load.dylib), main app:
//   src/TbMapStreamer_TbMapStreamer_CCatFixupTable_Load.cpp
//
// The callees outside the unit are fakes that record what the real ones read: Unload, Instance, FindCatagory, the file's Read and
// operator new[].
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "TbMapStreamer.h"

using namespace calllog;

namespace {

struct FakeFile : bfl::FileSystem::CFile {
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {lab(this), static_cast<int64_t>(size)});
        uint8_t* out = static_cast<uint8_t*>(dest);
        for (uint32_t i = 0; i < size; ++i) out[i] = pos + i < bytes.size() ? bytes[pos + i] : 0;
        pos += size;
        return static_cast<int32_t>(size);
    }
};

FakeFile gFile;
TbMapStreamer::CCatFixupTable gTable;
alignas(16) uint8_t gInstance[16];

}  // namespace

// ---- CFile's out-of-line virtuals, which the fake file inherits (never called)
namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }
}  // namespace bfl::FileSystem

// ---- operator new[] is the original's allocator call (size 0 gives 4 bytes, zeroed like the emulator's memory)
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    ev("new", {static_cast<int64_t>(size)});
    return std::calloc(size < 4 ? 4 : size, 1);
}

// ---- the fakes
void TbMapStreamer::CCatFixupTable::Unload() { ev("Unload", {lab(this)}); }

TbMapStreamer* TbMapStreamer::Instance() {
    ev("Instance", {});
    return reinterpret_cast<TbMapStreamer*>(gInstance);
}

uint32_t TbMapStreamer::FindCatagory(const char* name) {
    ev("FindCatagory", {lab(this)}, {name});
    return static_cast<uint32_t>(ret("FindCatagory"));
}

extern "C" {

uint64_t tpw_cft_addr(const char* name) {
    if (std::strcmp(name, "inst") == 0) return reinterpret_cast<uintptr_t>(gInstance);
    if (std::strcmp(name, "table") == 0) return reinterpret_cast<uintptr_t>(&gTable);
    if (std::strcmp(name, "file") == 0) return reinterpret_cast<uintptr_t>(static_cast<bfl::FileSystem::CFile*>(&gFile));
    return 0;
}

uint64_t tpw_cft_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cft_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbMapStreamer::CCatFixupTable, mData));
    out[n++] = static_cast<int64_t>(offsetof(TbMapStreamer::CCatFixupTable, mCount));
    return n;
}

void tpw_cft_setfile(const uint8_t* data, uint32_t len) {
    gFile.bytes.assign(data, data + len);
    gFile.pos = 0;
    gTable.mData = nullptr;
    gTable.mCount = 0xdeadbeef;
}

// out[0] = count, out[1] = the entry array pointer
void tpw_cft_table(uint64_t* out) {
    out[0] = gTable.mCount;
    out[1] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTable.mData));
}

int32_t tpw_cft_load() { return static_cast<int32_t>(gTable.Load(gFile)); }

}  // extern "C"
