// Test library for CActionRecorder::ProcessAction (0x10145e38, 0x101465a8) and RecordAction (0x1014445c) (unit v5362;
// libtpw_unit_CActionRecorder_ProcessAction_2.dylib, tests/test_CActionRecorder_ProcessAction_2.py), main app:
//   src/ActionRecorder_..._ProcessAction_2.cpp   the three functions and the two chain-end halfwords
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "ActionRecorder.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// gUIInterface holds strings whose destructor lives in a file this library does not compile.
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}

// CFile's own members live in files this library does not compile: the fake file needs its vtable (never called except Write).
namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return static_cast<TbError>(0); }
int32_t CFile::GetMacFileReference() { return 0; }
}  // namespace bfl::FileSystem

// ---- globals of other files, defined here
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
CFSGlobalMasterSave* gMasterSave;
UIInterface gUIInterface;
CActionRecorder gActionRecorder;

namespace {

uintptr_t gRecBase;

uint32_t HashText(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint32_t>(static_cast<unsigned char>(*s));
    return h;
}

// Up to 5 characters of a NUL-ended UTF-16 string: the count, then five slots (0 padded).
void Text5(const char16_t* text, int64_t* out) {
    int64_t n = 0;
    if (text != nullptr)
        while (n < 5 && text[n] != 0) ++n;
    out[0] = n;
    for (int i = 0; i < 5; ++i) out[1 + i] = (i < n) ? text[i] : 0;
}

// ev with a run-time count (2, 4, 6, 7, 8 or 11 values are used).
void EvN(const char* name, const int64_t* v, int n) {
    switch (n) {
        case 2: ev(name, {v[0], v[1]}); break;
        case 4: ev(name, {v[0], v[1], v[2], v[3]}); break;
        case 6: ev(name, {v[0], v[1], v[2], v[3], v[4], v[5]}); break;
        case 7: ev(name, {v[0], v[1], v[2], v[3], v[4], v[5], v[6]}); break;
        case 8: ev(name, {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]}); break;
        case 11: ev(name, {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]}); break;
        case 13: ev(name, {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12]}); break;
        default: ev("BADCOUNT", {n}); break;
    }
}

int64_t Tag(const void* object) { return object == nullptr ? 0 : *reinterpret_cast<const uint16_t*>(object); }

class FakeFile : public bfl::FileSystem::CFile {
public:
    int32_t Write(const void* source, uint32_t size) override {
        const uint8_t* bytes = static_cast<const uint8_t*>(source);
        ev("FileWrite", {reinterpret_cast<uintptr_t>(this) == gRecBase + offsetof(CActionRecorder, mActionFile) ? 1 : 0,
                         static_cast<int64_t>(size), size > 0 ? bytes[0] : 0, size > 1 ? bytes[1] : 0});
        return 0;
    }
};

}  // namespace

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t v[12];
    int n = 0;
    v[n++] = level;
    v[n++] = HashText(format);
    va_list args;
    va_start(args, format);
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        if (p[1] == 'd') {
            v[n++] = static_cast<int32_t>(va_arg(args, uint32_t));
        } else if (p[1] == 'l' && p[2] == 's') {
            const char16_t* text = va_arg(args, const char16_t*);
            Text5(text, v + n);
            n += 6;
        }
    }
    va_end(args);
    EvN("CTRACE", v, n);
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition & 0xff, static_cast<int64_t>(HashText(format))}); }

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag}); }
void LLCC_SystemSetModeNR(int32_t mode, bool flag) { ev("LLCC_SystemSetModeNR", {mode, flag}); }
void LLCC_SystemSetRideTrack(int32_t track) { ev("LLCC_SystemSetRideTrack", {track}); }
void LLCC_SystemSetRideQueue(int32_t queue) { ev("LLCC_SystemSetRideQueue", {queue}); }
void LLCC_SetRideRotation(int32_t rotation, bool flag) { ev("LLCC_SetRideRotation", {rotation, flag}); }
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag}); }
void UIInterface::SetSelectedObject(CObject* object) { ev("SetSelectedObject", {lab(this), Tag(object)}); }
void LayoutStackPylonUp(uint16_t id) { ev("LayoutStackPylonUp", {id}); }
void LayoutStackPylonDown(uint16_t id) { ev("LayoutStackPylonDown", {id}); }
void LayoutSetCoasterEditOld(uint32_t id) { ev("LayoutSetCoasterEditOld", {id}); }
void LayoutSetCoasterEdit(uint32_t id) { ev("LayoutSetCoasterEdit", {id}); }
void LayoutRegenerateTrack(uint32_t id) { ev("LayoutRegenerateTrack", {id}); }

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("MasterSaveCtor", {newlab(this)}); }

bool SwearFilter::Filter(std::u16string& text, bool replace) {
    int64_t v[8];
    v[0] = lab(this);
    v[1] = replace;
    Text5(text.c_str(), v + 2);
    EvN("Filter", v, 8);
    if (ret("filter") != 0 && !text.empty()) text[0] = u'*';
    return true;
}

namespace {
void WriteSign(char16_t* buffer) {
    int64_t v = ret("sign");
    int n = 1 + static_cast<int>(v % 4);
    for (int i = 0; i < n; ++i) buffer[i] = static_cast<char16_t>(0x61 + ((v >> (3 * i)) & 7));
    buffer[n] = 0;
}
void LogText(const char* name, const void* object, const char16_t* text) {
    int64_t v[7];
    v[0] = Tag(object);
    Text5(text, v + 1);
    EvN(name, v, 7);
}
}  // namespace

void CObject::SetSignNameA(const char16_t* text) { LogText("SetSignNameA", this, text); }
void CObject::SetSignNameB(const char16_t* text) { LogText("SetSignNameB", this, text); }
void CObject::GetSignNameA(char16_t* buffer, uint32_t size) {
    ev("GetSignNameA", {Tag(this), size});
    WriteSign(buffer);
}
void CObject::GetSignNameB(char16_t* buffer, uint32_t size) {
    ev("GetSignNameB", {Tag(this), size});
    WriteSign(buffer);
}

void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB) {
    // an event holds 12 ints: the tag, then of each text its length and first four characters
    int64_t v[11], a[6], b[6];
    v[0] = Tag(object);
    Text5(signA, a);
    Text5(signB, b);
    for (int i = 0; i < 5; ++i) {
        v[1 + i] = a[i];
        v[6 + i] = b[i];
    }
    EvN("RegenerateRideSign", v, 11);
}

extern "C" {

uint64_t tpw_CActionRecorder_ProcessAction_2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMasterSave", &gMasterSave},
        {"gGameWorld", &gGameWorld},
        {"gThingArray", gThingArray},
        {"gUIInterface", &gUIInterface},
        {"gReplayChainEndA", &gReplayChainEndA},
        {"gReplayChainEndB", &gReplayChainEndB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_CActionRecorder_ProcessAction_2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_CActionRecorder_ProcessAction_2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mGatesThing));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellNext));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

// A recorder with the fake file in place; the state and recording words are the arguments.
uint64_t tpw_CActionRecorder_ProcessAction_2_recorder(int32_t state, int32_t recording) {
    CActionRecorder* recorder = static_cast<CActionRecorder*>(tpw::ArenaAllocZeroed(sizeof(CActionRecorder)));
    new (recorder->mActionFile) FakeFile();
    recorder->mState = state;
    recorder->mRecording = recording;
    gRecBase = reinterpret_cast<uintptr_t>(recorder);
    return static_cast<uint64_t>(gRecBase);
}

void tpw_CActionRecorder_ProcessAction_2_p1(int32_t action, uint32_t a) {
    gActionRecorder.ProcessAction(action, static_cast<uint16_t>(a));
}

void tpw_CActionRecorder_ProcessAction_2_p2(int32_t action, uint32_t a, uint32_t count, uint64_t text) {
    gActionRecorder.ProcessAction(action, static_cast<uint16_t>(a), static_cast<uint8_t>(count), reinterpret_cast<char16_t*>(text));
}

void tpw_CActionRecorder_ProcessAction_2_record(uint64_t recorder, int32_t action, uint32_t a, uint32_t b, uint32_t c) {
    reinterpret_cast<CActionRecorder*>(recorder)->RecordAction(action, static_cast<uint16_t>(a), static_cast<uint16_t>(b),
                                                              static_cast<uint16_t>(c));
}

}  // extern "C"
