// Test library for CActionRecorder::ActivateActionRecording and the RecordAction / ProcessAction overloads taking a RecordedAction
// (unit v51041; libtpw_unit_CActionRecorder_ActivateActionRecording.dylib, tests/test_CActionRecorder_ActivateActionRecording.py), main app:
//   src/game/ActionRecorder.cpp
//
// The callees outside the unit are fakes recording what the real ones read: CTRACE (a format id, the level and the %d arguments), the
// file's virtual Write (which file, the size and a hash of the bytes) and CMemFile::Open (which file and the size).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "bfl_FileSystem.h"

using namespace calllog;

namespace {

CActionRecorder gRec;

const char* const kFormats[] = {
    "AR - %d\n",
    "AR - %d (%d, %d)\n",
    "AR - %d (%d, %d, %d)\n",
    "AR - %d (%d)\n",
    "AR - %d (128 byte array)\n",
    "AR replay - %d (128 bytes of nothing)\n",
    "Erroneous stuff going down\n",
};

int FileIndex(const void* file) {
    if (file == gRec.mActionFile) return 0;
    if (file == gRec.mTimingFile) return 1;
    if (file == gRec.mCameraFile) return 2;
    return 9;
}

// Same virtual order as bfl::FileSystem::CFile (destructor, Close, Read, Write) and the same size (vtable pointer and mImplementation).
struct FakeFile {
    virtual ~FakeFile() {}
    virtual void Close() {}
    virtual int32_t Read(void*, uint32_t) { return 0; }
    virtual int32_t Write(const void* source, uint32_t size) {
        const uint8_t* p = static_cast<const uint8_t*>(source);
        uint32_t h = 0;
        for (uint32_t i = 0; i < size; ++i) h = h * 31u + p[i];
        ev("Write", {FileIndex(this), static_cast<int64_t>(size), static_cast<int64_t>(h)});
        return static_cast<int32_t>(size);
    }
    void* mImplementation = nullptr;
};
static_assert(sizeof(FakeFile) <= sizeof(gRec.mActionFile), "fake file fits the storage");

}  // namespace

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    int id = 99;
    for (int i = 0; i < 7; ++i)
        if (std::strcmp(kFormats[i], format) == 0) id = i;
    int64_t v[12];
    int n = 0;
    v[n++] = id;
    v[n++] = level;
    va_list ap;
    va_start(ap, format);
    for (const char* p = format; *p; ++p)
        if (p[0] == '%' && p[1] == 'd' && n < 12) v[n++] = va_arg(ap, uint32_t);
    va_end(ap);
    switch (n) {
        case 2: ev("CTRACE", {v[0], v[1], -1}); break;
        case 6: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5]}); break;
        case 3: ev("CTRACE", {v[0], v[1], v[2]}); break;
        case 4: ev("CTRACE", {v[0], v[1], v[2], v[3]}); break;
        case 5: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4]}); break;
        default: ev("CTRACE", {v[0], v[1], -1}); break;
    }
}

namespace bfl {
namespace FileSystem {
TbError CMemFile::Open(uint32_t size) {
    ev("Open", {FileIndex(this), static_cast<int64_t>(size)});
    return TbError(0);
}
}  // namespace FileSystem
}  // namespace bfl

extern "C" {

uint64_t tpw_ar_addr(const char*) { return 0; }

uint64_t tpw_ar_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ar_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(FakeFile));
    return n;
}

// The recorder object: state (0x00), mode (0x04), recording (0x24); the three files are fakes.
void tpw_ar_set(int32_t state, int32_t mode, int32_t recording) {
    std::memset(&gRec, 0, sizeof(gRec));
    new (gRec.mActionFile) FakeFile();
    new (gRec.mTimingFile) FakeFile();
    new (gRec.mCameraFile) FakeFile();
    gRec.mState = state;
    gRec.mMode = mode;
    gRec.mRecording = recording;
}

void tpw_ar_obs(uint32_t* out) {
    out[0] = static_cast<uint32_t>(gRec.mState);
    out[1] = static_cast<uint32_t>(gRec.mMode);
    out[2] = static_cast<uint32_t>(gRec.mRecording);
}

void tpw_ar_act(int32_t mode) { gRec.ActivateActionRecording(static_cast<CActionRecorder::RecordMode>(mode)); }
void tpw_ar_r0(int32_t action) { gRec.RecordAction(static_cast<RecordedAction>(action)); }
void tpw_ar_r2(int32_t action, uint8_t a, uint8_t b) { gRec.RecordAction(static_cast<RecordedAction>(action), a, b); }
void tpw_ar_r3(int32_t action, uint8_t a, uint8_t b, uint16_t c) { gRec.RecordAction(static_cast<RecordedAction>(action), a, b, c); }
void tpw_ar_r1(int32_t action, uint16_t v) { gRec.RecordAction(static_cast<RecordedAction>(action), v); }
void tpw_ar_r1b(int32_t action, uint16_t v, uint8_t e) { gRec.RecordAction(static_cast<RecordedAction>(action), v, e); }
void tpw_ar_rp(int32_t action, uint64_t data) {
    gRec.RecordAction(static_cast<RecordedAction>(action), reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data)));
}
void tpw_ar_pa(int32_t action, uint64_t data) {
    gRec.ProcessAction(static_cast<RecordedAction>(action), reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(data)));
}

}  // extern "C"
