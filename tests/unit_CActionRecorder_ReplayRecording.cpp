// Test library for CActionRecorder::ReplayRecording, EndReplay and three RecordAction overloads (unit v5224;
// libtpw_unit_CActionRecorder_ReplayRecording.dylib, tests/test_CActionRecorder_ReplayRecording.py), main app:
//   src/game/ActionRecorder.cpp
//
// The three files of the recorder are fakes with CFile's vtable order (Write and Seek record an event); the callees outside the unit
// (GetBankAccount, GetNext*FileEntry, DoTick, RSSE_Tick, LayoutStartAllCoasterSounds, CTRACE, the swear filter, the master save
// constructor) record the same events as the PowerPC stubs. gGameWorld and gMasterSave are defined here, zero at start.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "TPCoaster.h"
#include "ThingReceivers.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

CWorld* gGameWorld = nullptr;
CFSGlobalMasterSave* gMasterSave = nullptr;

namespace {

CBankAccount* gBank = nullptr;

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

// FNV-1a over bytes, the same in the Python module.
uint64_t Fnv(const uint8_t* p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

uint64_t FnvUnits(const char16_t* p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++) {
        h ^= static_cast<uint8_t>(p[i] & 0xff);
        h *= 0x100000001b3ull;
        h ^= static_cast<uint8_t>(p[i] >> 8);
        h *= 0x100000001b3ull;
    }
    return h;
}

// Same layout and virtual order as CFile (the destructor takes two slots).
struct FakeFile {
    virtual ~FakeFile() {}
    virtual void Close() {}
    virtual int32_t Read(void*, uint32_t) { return 0; }
    virtual int32_t Write(const void* source, uint32_t size) {
        uint64_t h = Fnv(static_cast<const uint8_t*>(source), size);
        ev("Write", {I(lab(this)), I(size), I(h & 0xffffffffu), I(h >> 32)});
        return 0;
    }
    virtual int32_t Seek(uint32_t offset, uint32_t whence) {
        ev("Seek", {I(lab(this)), I(offset), I(whence)});
        return 0;
    }
    virtual int32_t GetSize() { return 0; }
    virtual int32_t GetPos() { return 0; }
    virtual void* MapFile() { return nullptr; }
    void* mImplementation = nullptr;
};
static_assert(sizeof(FakeFile) == 16, "file storage");

void InitFiles(CActionRecorder* r) {
    new (r->mActionFile) FakeFile();
    new (r->mTimingFile) FakeFile();
    new (r->mCameraFile) FakeFile();
}

}  // namespace

// ---- the fakes
// platform/sams_core.cpp (for _USwapBlock16) references it; never run.
namespace tpw {
void WindowOrigin(int& x, int& y) {
    x = 0;
    y = 0;
}
void Log(const char*, ...) {}
}  // namespace tpw

// Referenced by the master save fake constructor (unwinding); never run.
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {I(lab(this))});
    return gBank;
}

void CActionRecorder::GetNextTimingFileEntry() { ev("GetNextTimingFileEntry", {I(lab(this))}); }
void CActionRecorder::GetNextCameraFileEntry() { ev("GetNextCameraFileEntry", {I(lab(this))}); }
void CActionRecorder::DoTick() { ev("DoTick", {I(lab(this))}); }

int32_t RSSE_Tick() {
    ev("RSSE_Tick", {});
    return 0;
}

void LayoutStartAllCoasterSounds() { ev("LayoutStartAllCoasterSounds", {}); }

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("MasterSaveCtor", {I(newlab(this))}); }

bool SwearFilter::Filter(std::u16string& text, bool replace) {
    int64_t cut = ret("FilterCut", 100);
    if (cut >= 0 && static_cast<size_t>(cut) < text.size()) text.resize(static_cast<size_t>(cut));
    uint64_t h = FnvUnits(text.data(), text.size());
    ev("Filter", {I(lab(this)), I(text.size()), I(h & 0xffffffffu), I(h >> 32), replace ? 1 : 0});
    return true;
}

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t v[12];
    int n = 0;
    v[n++] = level;
    uint64_t fh = Fnv(reinterpret_cast<const uint8_t*>(format), std::strlen(format));
    v[n++] = I(fh & 0xffffffffu);
    v[n++] = I(fh >> 32);
    va_list ap;
    va_start(ap, format);
    for (const char* p = format; *p; p++) {
        if (*p != '%') continue;
        p++;
        if (*p == 'd') {
            v[n++] = static_cast<uint32_t>(va_arg(ap, int));
        } else if (*p == 'g') {
            double d = va_arg(ap, double);
            uint64_t b;
            std::memcpy(&b, &d, 8);
            if (d != d) b = 0x7ff8000000000000ull;
            v[n++] = I(b & 0xffffffffu);
            v[n++] = I(b >> 32);
        } else if (*p == 'l') {
            p++;
            const char16_t* s = va_arg(ap, const char16_t*);
            size_t len = std::char_traits<char16_t>::length(s);
            uint64_t h = FnvUnits(s, len);
            v[n++] = I(len);
            v[n++] = I(h & 0xffffffffu);
            v[n++] = I(h >> 32);
        }
    }
    va_end(ap);
    // The events carry at most 12 ints.
    switch (n) {
        case 5: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4]}); break;
        case 6: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5]}); break;
        case 7: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5], v[6]}); break;
        case 8: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]}); break;
        case 9: ev("CTRACE", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]}); break;
        default: ev("CTRACE", {v[0], v[1], v[2]}); break;
    }
}

extern "C" {

uint64_t tpw_rec_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameWorld", &gGameWorld},
        {"gMasterSave", &gMasterSave},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rec_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_rec_setbank(uint64_t bank) { gBank = reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(bank)); }

int32_t tpw_rec_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mState));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mMode));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mF08));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mActionFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mTimingFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mCameraFile));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mRecording));
    out[n++] = static_cast<int64_t>(offsetof(CActionRecorder, mReplayType));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mWithdrawalsEnabled));
    out[n++] = static_cast<int64_t>(sizeof(CActionRecorder));
    out[n++] = static_cast<int64_t>(sizeof(CBankAccount));
    return n;
}

static CActionRecorder* Rec(uint64_t obj) {
    CActionRecorder* r = reinterpret_cast<CActionRecorder*>(static_cast<uintptr_t>(obj));
    InitFiles(r);
    return r;
}

void tpw_rec_replay(uint64_t obj, int32_t type) { Rec(obj)->ReplayRecording(static_cast<ReplayType>(type)); }
void tpw_rec_end(uint64_t obj) { Rec(obj)->EndReplay(); }
void tpw_rec_ra_f(uint64_t obj, int32_t action, float value, uint32_t id) {
    Rec(obj)->RecordAction(static_cast<RecordedAction>(action), value, static_cast<uint16_t>(id));
}
void tpw_rec_ra_ff(uint64_t obj, int32_t action, float a, float b, uint32_t id) {
    Rec(obj)->RecordAction(static_cast<RecordedAction>(action), a, b, static_cast<uint16_t>(id));
}
void tpw_rec_ra_w(uint64_t obj, int32_t action, uint32_t a, uint32_t b, uint64_t text) {
    Rec(obj)->RecordAction(static_cast<RecordedAction>(action), static_cast<uint16_t>(a), static_cast<uint8_t>(b),
                           reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text)));
}

}  // extern "C"
