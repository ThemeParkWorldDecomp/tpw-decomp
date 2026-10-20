// Test library for CActionRecorder's constructor, destructor, Serialise and RecordAction (unit v5075;
// libtpw_unit_CActionRecorder_CActionRecorder.dylib, tests/test_CActionRecorder_CActionRecorder.py), main app:
//   src/ActionRecorder_CActionRecorder_CActionRecorder.cpp
//
// The callees outside the unit are fakes that record what the real ones read: CTRACE, SAVEVAR / LOADVAR of int and unsigned int,
// LbFile_Read / LbFile_Write, the bullfrog CFile (constructor, destructor, Close, Seek, GetSize; Read and Write of the FakeFile),
// ResetActionRecording, ActivateActionRecording and SaveRecordingToDisk. Text arguments are recorded as a hash of their bytes.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "ActionRecorder.h"
#include "Debug.h"
#include "LbFile.h"
#include "Serialise.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;
using bfl::FileSystem::CFile;
using bfl::FileSystem::CMemFile;

namespace {

int64_t U(uint32_t v) { return static_cast<int64_t>(v); }

int64_t Hash(const void* p, uint32_t n) {
    uint32_t h = 0;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (uint32_t i = 0; i < n; ++i) h = h * 31u + b[i];
    return h;
}

int64_t TextTag(const char* s) { return Hash(s, static_cast<uint32_t>(std::strlen(s))); }

void Pattern(void* dest, uint32_t n) {
    uint8_t* b = static_cast<uint8_t*>(dest);
    for (uint32_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(i * 7u + n);
}

// The recorder's files while a test runs: every virtual that the unit calls records an event.
struct FakeFile : CMemFile {
    void Close() override { ev("Close", {lab(this)}); }
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {lab(this), U(size)});
        Pattern(dest, size);
        return static_cast<int32_t>(ret("Read"));
    }
    int32_t Write(const void* source, uint32_t size) override {
        ev("Write", {lab(this), U(size), Hash(source, size)});
        return static_cast<int32_t>(ret("Write"));
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("Seek", {lab(this), U(offset), U(whence)});
        return static_cast<int32_t>(ret("Seek"));
    }
    int32_t GetSize() override {
        ev("GetSize", {lab(this)});
        return static_cast<int32_t>(ret("GetSize"));
    }
};

static_assert(sizeof(FakeFile) == 16, "a file slot");

FakeFile& Proto() {
    static FakeFile proto;
    return proto;
}

}  // namespace

// ---- bullfrog CFile: the members the vtable names
namespace bfl {
namespace FileSystem {
CFile::CFile() : mImplementation(nullptr) { ev("CFile::CFile", {newlab(this)}); }
CFile::~CFile() { ev("CFile::~CFile", {lab(this), 0}); }
void CFile::Close() { ev("Close", {lab(this)}); }
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace FileSystem
}  // namespace bfl

// ---- the other callees
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strcmp(format, "AR - %d (%d, %d)\n") == 0) {
        va_list ap;
        va_start(ap, format);
        uint32_t a = va_arg(ap, uint32_t);
        uint32_t b = va_arg(ap, uint32_t);
        uint32_t c = va_arg(ap, uint32_t);
        va_end(ap);
        ev("CTRACE", {U(level), TextTag(format), U(a), U(b), U(c)});
    } else {
        ev("CTRACE", {U(level), TextTag(format)});
    }
}

template<> int32_t SAVEVAR<int>(void* file, int* value, const char* name, int32_t noLog) {
    ev("SAVEVAR<int>", {lab(file), U(static_cast<uint32_t>(*value)), TextTag(name), U(static_cast<uint32_t>(noLog))});
    return static_cast<int32_t>(ret("SAVEVAR_i"));
}

template<> int32_t SAVEVAR<unsigned int>(void* file, unsigned int* value, const char* name, int32_t noLog) {
    ev("SAVEVAR<uint>", {lab(file), U(*value), TextTag(name), U(static_cast<uint32_t>(noLog))});
    return static_cast<int32_t>(ret("SAVEVAR_u"));
}

template<> int32_t LOADVAR<int>(void* file, int* value, const char* name, int32_t noLog) {
    ev("LOADVAR<int>", {lab(file), U(static_cast<uint32_t>(*value)), TextTag(name), U(static_cast<uint32_t>(noLog))});
    *value = static_cast<int>(ret("LOADVAR_i_v"));
    return static_cast<int32_t>(ret("LOADVAR_i"));
}

template<> int32_t LOADVAR<unsigned int>(void* file, unsigned int* value, const char* name, int32_t noLog) {
    ev("LOADVAR<uint>", {lab(file), U(*value), TextTag(name), U(static_cast<uint32_t>(noLog))});
    *value = static_cast<unsigned int>(ret("LOADVAR_u_v"));
    return static_cast<int32_t>(ret("LOADVAR_u"));
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t*) {
    ev("LbFile_Read", {lab(file), U(length)});
    Pattern(buffer, length);
    return TbError(static_cast<int32_t>(ret("LbFile_Read")));
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t*) {
    ev("LbFile_Write", {lab(file), U(length), Hash(buffer, length)});
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

void CActionRecorder::ResetActionRecording() {
    ev("ResetActionRecording", {lab(this)});
    mState = 0x5a5a;
}

void CActionRecorder::ActivateActionRecording(RecordMode mode) {
    ev("ActivateActionRecording", {lab(this), U(static_cast<uint32_t>(mode))});
    mF08 = 0x1234567;
}

void CActionRecorder::SaveRecordingToDisk() { ev("SaveRecordingToDisk", {lab(this)}); }

extern "C" {

// A zeroed block (32-bit addressable).
uint64_t tpw_ar_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets the module relies on (HOST_LAYOUT).
int32_t tpw_ar_layout(int64_t* out) {
    Proto();
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mState));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mMode));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mF08));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mActionFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mTimingFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mCameraFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mReplayType));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mTurnsToNext));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mActionKind));
    out[n++] = static_cast<int64_t>(sizeof(CActionRecorder));
    return n;
}

// Puts the recording fake into the three file slots (what the Mac's constructed object holds).
void tpw_ar_prep(uint64_t object) {
    CActionRecorder* o = reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object));
    std::memcpy(o->mActionFile, &Proto(), 16);
    std::memcpy(o->mTimingFile, &Proto(), 16);
    std::memcpy(o->mCameraFile, &Proto(), 16);
}

void tpw_ar_ctor(uint64_t object) { reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object))->Construct(); }

void tpw_ar_dtor(uint64_t object) { reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object))->Destruct(); }

int32_t tpw_ar_serialise(uint64_t object, uint64_t file, int32_t saving, int32_t version, uint32_t unused) {
    return reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object))
        ->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), saving, version, unused);
}

void tpw_ar_record(uint64_t object, int32_t action, uint16_t a, uint16_t b) {
    reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object))->RecordAction(static_cast<RecordedAction>(action), a, b);
}

}  // extern "C"
