// Test library for TbMapStreamer::ReShuffleSoundForDroppable and SaveCatgoryFixupTable (unit v5047;
// libtpw_unit_TbMapStreamer_ReShuffleSoundForDroppable.dylib), main app:
//   src/sound/TbMapStreamer.cpp
//
// The callees outside the unit are recording fakes: TbSoundManager::IsBankDroppable, the file's Write / Seek / GetPos (a CFile
// subclass) and the string helpers of TbMapStreamerFixup. mpSoundManager is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbError.h"
#include "TbMapStreamer.h"
#include "TbSoundManager.h"

using namespace calllog;

TbSoundManager* TbMapStreamer::mpSoundManager = nullptr;

namespace {

int64_t Fnv(const uint8_t* data, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ data[i]) * 16777619u;
    return h;
}

}  // namespace

bool TbSoundManager::IsBankDroppable(uint32_t index) {
    ev("IsBankDroppable", {lab(this), static_cast<int64_t>(index)});
    return ret("IsBankDroppable") != 0;
}

namespace bfl {
namespace FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace FileSystem
}  // namespace bfl

namespace {

class FakeFile : public bfl::FileSystem::CFile {
public:
    int32_t Write(const void* source, uint32_t size) override {
        const uint8_t* bytes = static_cast<const uint8_t*>(source);
        uint32_t first = 0;
        std::memcpy(&first, bytes, size < 4 ? size : 4);
        ev("Write", {lab(this), static_cast<int64_t>(size), static_cast<int64_t>(first), Fnv(bytes, size)});
        return static_cast<int32_t>(size);
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("Seek", {lab(this), static_cast<int64_t>(offset), static_cast<int64_t>(whence)});
        return 0;
    }
    int32_t GetPos() override {
        ev("GetPos", {lab(this)});
        return static_cast<int32_t>(ret("GetPos"));
    }
};

FakeFile gFile;

}  // namespace

namespace TbMapStreamerFixup {
uint32_t NameLength(const TbDynamicStringTemplate<char>& name) {
    const char* const* slot = reinterpret_cast<const char* const*>(&name);
    const uint32_t tag = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(&name) + 8);
    ev("NameLength", {static_cast<int64_t>(tag)});
    return static_cast<uint32_t>(std::strlen(*slot));
}
const char* NameText(const TbDynamicStringTemplate<char>& name) {
    const char* const* slot = reinterpret_cast<const char* const*>(&name);
    const uint32_t tag = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(&name) + 8);
    ev("NameText", {static_cast<int64_t>(tag), Fnv(reinterpret_cast<const uint8_t*>(*slot), std::strlen(*slot))});
    return *slot;
}
void StringInit(void* ustring, const char* text) {
    ev("StringInit", {Fnv(reinterpret_cast<const uint8_t*>(text), std::strlen(text))});
    std::memcpy(ustring, &text, sizeof(text));
}
void StringReplace(void*, int32_t from, int32_t to, int32_t flag) { ev("StringReplace", {from, to, flag}); }
char* StringText(void* ustring) {
    ev("StringText", {});
    char* text;
    std::memcpy(&text, ustring, sizeof(text));
    return text;
}
void StringDestroy(void*, int32_t flag) { ev("StringDestroy", {flag}); }
}  // namespace TbMapStreamerFixup

extern "C" {

uint64_t tpw_msf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"mpSoundManager", &TbMapStreamer::mpSoundManager},
        {"file", &gFile},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_msf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_msf_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_msf_layout(int64_t* out) {
    int32_t n = 0;
    auto* streamer = static_cast<TbMapStreamer*>(tpw::ArenaAllocZeroed(sizeof(TbMapStreamer) + 64));
    auto* category = static_cast<CCatagoryContainer*>(tpw::ArenaAllocZeroed(sizeof(CCatagoryContainer) + 64));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&streamer->mCatagories) - reinterpret_cast<char*>(streamer));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&category->mName) - reinterpret_cast<char*>(category));
    out[n++] = static_cast<int64_t>(sizeof(CSoundData));
    out[n++] = static_cast<int64_t>(sizeof(CSampleData));
    return n;
}

uint32_t tpw_msf_reshuffle(uint64_t streamer, uint64_t sound) {
    reinterpret_cast<TbMapStreamer*>(static_cast<uintptr_t>(streamer))
        ->ReShuffleSoundForDroppable(reinterpret_cast<CSoundData*>(static_cast<uintptr_t>(sound)));
    return 0;
}

int32_t tpw_msf_save(uint64_t streamer, uint64_t file) {
    const TbError result = reinterpret_cast<TbMapStreamer*>(static_cast<uintptr_t>(streamer))
                               ->SaveCatgoryFixupTable(reinterpret_cast<bfl::FileSystem::CFile*>(static_cast<uintptr_t>(file)));
    return static_cast<int32_t>(result);
}

}  // extern "C"
