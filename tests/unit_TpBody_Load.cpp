// Test library for TpBody::Load/Save, five Sound Sprockets command constructors and UReferenceCounted::RemoveReference (unit v51014;
// libtpw_unit_TpBody_Load.dylib, tests/test_TpBody_Load.py), main app: src/sound/CSprocketsModule.cpp, src/sound/SprocketsChannel.cpp, src/sound/TpBody.cpp.
//
// The callees outside the unit are fakes that record what the real ones read: the file (Read, Write, Seek), the list (Load, Save),
// TbSoundPosition::Get and UCommand's constructor. The CFile members are defined here (the library's file class is an import).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbSoundPosition.h"
#include "CSprocketsModule.h"

using namespace calllog;

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

struct FakeFile : bfl::FileSystem::CFile {
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {lab(this), static_cast<int64_t>(size)});
        uint32_t v = static_cast<uint32_t>(ret("rd"));
        std::memcpy(dest, &v, 4);
        return 4;
    }
    int32_t Write(const void* source, uint32_t size) override {
        const uint8_t* b = static_cast<const uint8_t*>(source);
        ev("Write", {lab(this), static_cast<int64_t>(size), b[0], b[1], b[2], b[3]});
        return 4;
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("Seek", {lab(this), static_cast<int64_t>(offset), static_cast<int64_t>(whence)});
        return 0;
    }
};

struct FakeRef : SamsUtilities::UReferenceCounted {
    ~FakeRef() override { ev("Delete", {lab(this), 1}); }
    void Dispose() override { ev("Dispose", {lab(this), static_cast<int64_t>(mCount)}); }
};

FakeFile gFile;
alignas(16) uint8_t gList[16];

}  // namespace

TbError CAudioPlaceHolderList::Load(bfl::FileSystem::CFile* file, bool flag) {
    ev("ListLoad", {lab(this), lab(file), flag ? 1 : 0});
    return TbError(0);
}
TbError CAudioPlaceHolderList::Save(bfl::FileSystem::CFile* file) {
    ev("ListSave", {lab(this), lab(file)});
    return TbError(0);
}

void TbSoundPosition::Get(float& x, float& y, float& z) const {
    ev("Get", {lab(this)});
    uint32_t a = static_cast<uint32_t>(ret("getx")), b = static_cast<uint32_t>(ret("gety")), c = static_cast<uint32_t>(ret("getz"));
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    std::memcpy(&z, &c, 4);
}

namespace SamsUtilities {
void UCommand_Construct(UCommand* command) { ev("UCommand", {lab(command)}); }
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_tb_addr(const char* name) {
    if (std::strcmp(name, "file") == 0) return reinterpret_cast<uintptr_t>(static_cast<bfl::FileSystem::CFile*>(&gFile));
    if (std::strcmp(name, "list") == 0) return reinterpret_cast<uintptr_t>(gList);
    return 0;
}

uint64_t tpw_tb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(SamsUtilities::UCommand));
    return n;
}

// The body: a zeroed block of the host class; the list pointer is set by hand (the constructor is not part of the unit).
uint64_t tpw_tb_body(uint64_t list) {
    void* p = tpw::ArenaAllocZeroed(sizeof(TbAudioBody));
    TbAudioBody* body = reinterpret_cast<TbAudioBody*>(p);
    body->mPlaceHolders.mData = reinterpret_cast<CAudioPlaceHolderList*>(static_cast<uintptr_t>(list));
    return reinterpret_cast<uintptr_t>(p);
}
uint64_t tpw_tb_bodypos(uint64_t body) { return reinterpret_cast<uintptr_t>(&reinterpret_cast<TbAudioBody*>(body)->mPosition); }

int32_t tpw_tb_save(uint64_t body, uint64_t file) {
    return reinterpret_cast<TpBody*>(body)->Save(reinterpret_cast<void*>(file));
}
int32_t tpw_tb_load(uint64_t body, uint64_t file, uint32_t flag) {
    return reinterpret_cast<TpBody*>(body)->Load(reinterpret_cast<void*>(file), static_cast<uint8_t>(flag));
}

void tpw_tb_speaker(uint64_t p, int32_t type, int32_t flags) {
    new (reinterpret_cast<void*>(p)) SamsUtilities::UCommandSpeakerType(static_cast<SamsUtilities::SpeakerType>(type), flags);
}
void tpw_tb_volume(uint64_t p, double v) { new (reinterpret_cast<void*>(p)) SamsUtilities::UCommandVolume(v); }
void tpw_tb_scale(uint64_t p, double x, double y, double z) {
    new (reinterpret_cast<void*>(p)) SamsUtilities::UCommandListenerScale(x, y, z);
}
void tpw_tb_cpu(uint64_t p, double v) { new (reinterpret_cast<void*>(p)) SamsUtilities::UCommandCPULoad(v); }
void tpw_tb_stype(uint64_t p, int32_t t) { new (reinterpret_cast<void*>(p)) SamsUtilities::UCommandSoundType(static_cast<SamsUtilities::SoundType>(t)); }

uint64_t tpw_tb_newref(uint32_t count) {
    FakeRef* r = new FakeRef();
    r->mCount = count;
    return reinterpret_cast<uintptr_t>(static_cast<SamsUtilities::UReferenceCounted*>(r));
}
uint32_t tpw_tb_remove(uint64_t p) { return reinterpret_cast<SamsUtilities::UReferenceCounted*>(p)->RemoveReference(); }
uint32_t tpw_tb_refcount(uint64_t p) { return reinterpret_cast<SamsUtilities::UReferenceCounted*>(p)->mCount; }

}  // extern "C"
