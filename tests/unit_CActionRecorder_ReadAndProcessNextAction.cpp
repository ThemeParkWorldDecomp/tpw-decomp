// Test library for CActionRecorder::ReadAndProcessNextAction (unit v5077; libtpw_unit_CActionRecorder_ReadAndProcessNextAction.dylib,
// tests/test_CActionRecorder_ReadAndProcessNextAction.py), main app:
//   src/ActionRecorder_CActionRecorder_ReadAndProcessNextAction.cpp
//
// The callees outside the unit are recording fakes: the action file's Read (a stream of bytes the case supplies, a return value from
// the queue "Read"), the twelve ProcessAction overloads, CTRACE and SamsUtilities::_USwapBlock16 (which swaps the bytes of each word).
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

const uint8_t* gStream = nullptr;
uint32_t gStreamLen = 0;
uint32_t gCursor = 0;
const CActionRecorder* gObj = nullptr;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Hash(const void* p, uint32_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    uint32_t h = 17;
    for (uint32_t i = 0; i < n; i++) h = (h * 31 + b[i]) & 0x7fffffffu;
    return h;
}

int64_t Ok(const CActionRecorder* self) { return self == gObj ? 1 : 0; }

class FakeFile : public bfl::FileSystem::CMemFile {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {static_cast<int64_t>(size)});
        uint8_t* out = static_cast<uint8_t*>(dest);
        for (uint32_t i = 0; i < size; i++) out[i] = (gCursor + i < gStreamLen) ? gStream[gCursor + i] : 0;
        gCursor += size;
        const int32_t v = static_cast<int32_t>(ret("Read"));
        switch (v) {
            case 0: return static_cast<int32_t>(size);
            case 1: return static_cast<int32_t>(size) - 1;
            case 2: return static_cast<int32_t>(size) + 1;
            case 3: return 0;
            default: return -1;
        }
    }
};

}  // namespace

// ---- CFile members the fake file's base needs
namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace bfl::FileSystem

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

void SamsUtilities::_USwapBlock16(uint16_t* words, int32_t count) {
    ev("Swap16", {count});
    uint8_t* b = reinterpret_cast<uint8_t*>(words);
    for (int32_t i = 0; i < count; i++) {
        uint8_t t = b[2 * i];
        b[2 * i] = b[2 * i + 1];
        b[2 * i + 1] = t;
    }
}

void CActionRecorder::ProcessAction(int32_t action) { ev("PA", {Ok(this), action}); }
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b, uint16_t c) { ev("PA_bbh", {Ok(this), action, a, b, c}); }
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b) { ev("PA_bb", {Ok(this), action, a, b}); }
void CActionRecorder::ProcessAction(int32_t action, uint16_t a) { ev("PA_h", {Ok(this), action, a}); }
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint8_t b) { ev("PA_hb", {Ok(this), action, a, b}); }
void CActionRecorder::ProcessAction(int32_t action, float a, uint16_t b) { ev("PA_fh", {Ok(this), action, CanonBits(a), b}); }
void CActionRecorder::ProcessAction(int32_t action, float a, float b, uint16_t c) {
    ev("PA_ffh", {Ok(this), action, CanonBits(a), CanonBits(b), c});
}
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b) { ev("PA_hh", {Ok(this), action, a, b}); }
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b, uint16_t c) { ev("PA_hhh", {Ok(this), action, a, b, c}); }
void CActionRecorder::ProcessAction(int32_t action, uint8_t count, char16_t* text) {
    ev("PA_bw", {Ok(this), action, count, Hash(text, static_cast<uint32_t>(count) * 2)});
}
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint8_t count, char16_t* text) {
    ev("PA_hbw", {Ok(this), action, a, count, Hash(text, static_cast<uint32_t>(count) * 2)});
}
void CActionRecorder::ProcessAction(int32_t action, uint8_t* data) { ev("PA_P", {Ok(this), action, Hash(data, 128)}); }

extern "C" {

uint64_t tpw_rec_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rec_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mActionFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mActionKind));
    out[n++] = static_cast<int64_t>(sizeof(CActionRecorder));
    return n;
}

// Builds the fake file in the object's action file storage, sets the stream and calls.
int32_t tpw_rec_run(uint64_t object, uint64_t data, uint32_t length) {
    CActionRecorder* self = reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(object));
    new (self->mActionFile) FakeFile();
    gObj = self;
    gStream = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(data));
    gStreamLen = length;
    gCursor = 0;
    return self->ReadAndProcessNextAction();
}

}  // extern "C"
