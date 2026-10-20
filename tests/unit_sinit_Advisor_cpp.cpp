// Test library for Advisor.cpp's static initialiser and save templates plus small leaves of the bump rides, camera and coasters
// (unit v51069; libtpw_unit_sinit_Advisor_cpp.dylib, tests/test_sinit_Advisor_cpp.py), main app:
//   src/AdvisorScoring_sinit_Advisor_cpp.cpp
//
// The callees outside the unit (the string constructor, __register_global_object, WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Read,
// LbFile_Write, MReleaseFreeF, __destroy_arr) are fakes that record the arguments the real ones read and answer from the test's queues.
// The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "BMP_InsideArea.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"
#include "keytable_actions.h"
#include "ui_main.h"
#include "TPAdvisor.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tabCOLLIST gColList;
uint32_t gFirstPersonFlags;
tag_sRideAnimSet* gFirstPersonRideSet;
uint32_t gFirstPersonRideInstance;
uint32_t gCameraModeA;
tag_sCoasterInstance* gCoasterInstances;
void* gObjectTable[0x100];
tag_sRideAnimSet* gRideAnimSets[0x800];
CSaveLoad gSaveLoad;
// gAdvisorStore is declared with a type whose constructor is not in this library: raw bytes under its name.
unsigned char gAdvisorStoreBytes[0x400] __asm__("_gAdvisorStore");

void UIMain_DestroyWString(void*) {}

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("TbDynamicString", {lab(this), lab(const_cast<char16_t*>(text))});
}

// The vtable the constructor above makes needs it (never called).
using WString = TbDynamicStringTemplate<char16_t>;
template <> void WString::AllocBuffer(uint32_t) {}
template <> void WString::Format(const char16_t*, ...) {}
template <> void WString::ResizeTo(uint32_t) {}
template <> WString::~TbDynamicStringTemplate() {}
template <> WString& WString::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> WString& WString::operator=(const WString&) { return *this; }
template <> WString& WString::operator=(const char16_t*) { return *this; }
template <> WString& WString::operator=(const char16_t&) { return *this; }
template <> WString& WString::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> WString& WString::operator+=(const char16_t&) { return *this; }
template <> WString& WString::operator+=(const char16_t*) { return *this; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    uint32_t word = static_cast<uint32_t>(ret("ReadWord"));
    *transferred = static_cast<uint32_t>(ret("ReadCount"));
    int32_t result = static_cast<int32_t>(ret("ReadResult"));
    if (result == 0) std::memcpy(buffer, &word, length < 4 ? length : 4);   // a failed read leaves the buffer alone
    return TbError(result);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t word = 0;
    std::memcpy(&word, buffer, length < 4 ? length : 4);
    ev("LbFile_Write", {lab(file), length, word});
    *transferred = static_cast<uint32_t>(ret("WriteCount"));
    return TbError(static_cast<int32_t>(ret("WriteResult")));
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}

extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count) {
    ev("__destroy_arr", {lab(array), lab(destructor), size, count});
}

extern "C" {

uint64_t tpw_sinitadv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"colList", &gColList},
        {"fpFlags", &gFirstPersonFlags},
        {"fpSet", &gFirstPersonRideSet},
        {"fpInst", &gFirstPersonRideInstance},
        {"camA", &gCameraModeA},
        {"coasters", &gCoasterInstances},
        {"objTable", gObjectTable},
        {"rideSets", gRideAnimSets},
        {"w7314", &gTPCoasterWord7314},
        {"w7318", &gTPCoasterWord7318},
        {"store", gAdvisorStoreBytes},
        {"dtorVec", gAdvisorStoreDtorVector},
        {"sinitStr", gAdvisorMsgStringStorage},
        {"sinitNode", gAdvisorMsgStringNode},
        {"sinitText", gAdvisorMsgText},
        {"sinitDtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"saveLoad", &gSaveLoad},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinitadv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sinitadv_base() { return static_cast<uint64_t>((uintptr_t)tpw::gArenaBase); }

// The first five entries are the Mac's own layout (the module checks them); the rest are the host's offsets.
int32_t tpw_sinitadv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabBUOY));
    out[n++] = static_cast<int64_t>(offsetof(tabBUOY, index));
    out[n++] = static_cast<int64_t>(offsetof(tabBUOY, next));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLOBJECT));
    out[n++] = static_cast<int64_t>(offsetof(tabCOLLIST, count));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(tagDataNodeTable, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(tagDataNodeEntry));
    out[n++] = static_cast<int64_t>(offsetof(tagDataNodeEntry, mMatrix));
    out[n++] = static_cast<int64_t>(offsetof(sWraparoundQueue, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mCObject));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mLastBrace));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mScriptID));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mObjectInstanceID));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mLength));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mRatingA));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mRatingB));
    out[n++] = static_cast<int64_t>(offsetof(tag_sTrackBrace, mF90));
    out[n++] = static_cast<int64_t>(offsetof(sBraceSpline, mControlsA));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    return n;
}

// ---- the functions
void tpw_sinitadv_sinit_adv() { __sinit_Advisor_cpp(); }
int32_t tpw_sinitadv_findbuoy(uint64_t ride, int32_t x, int32_t y) {
    return FindBuoy(reinterpret_cast<tagBUMPRIDE*>(ride), x, y);
}
void tpw_sinitadv_freecol(uint64_t ride) { FreeColObjectsForRide(reinterpret_cast<tagBUMPRIDE*>(ride)); }
uint64_t tpw_sinitadv_finish(uint64_t ride) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetFinishLine(reinterpret_cast<tagBUMPRIDE*>(ride))));
}
void tpw_sinitadv_setseq(uint64_t ride) { SetBuoySequence(reinterpret_cast<tagBUMPRIDE*>(ride)); }
int32_t tpw_sinitadv_bmpread(uint64_t file, uint64_t buffer, uint32_t size, uint64_t transferred) {
    TbError result = BMPFile_Read(reinterpret_cast<void*>(file), reinterpret_cast<void*>(buffer), size,
                                  reinterpret_cast<uint32_t*>(transferred));
    return result.mValue;
}
uint64_t tpw_sinitadv_datanode(uint64_t meshCell, uint64_t indexCell) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        GetDataNodeWorldMatrix(*reinterpret_cast<tag_sMesh**>(meshCell), *reinterpret_cast<uint32_t*>(indexCell))));
}
uint64_t tpw_sinitadv_ridefrom(uint64_t idCell) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetRideFromID(*reinterpret_cast<uint32_t*>(idCell))));
}
void tpw_sinitadv_arraydtor() { __arraydtor_3112(); }
uint32_t tpw_sinitadv_fpid() { return GetFirstPersonRideID(); }
int32_t tpw_sinitadv_resetcam() { return ResetCameraPosition(); }
uint64_t tpw_sinitadv_nth(uint32_t index, uint64_t control) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(GetNthCoasterTrackStart(index, reinterpret_cast<void**>(control))));
}
void tpw_sinitadv_sinit_coaster() { __sinit_TPCoaster_cpp(); }
int32_t tpw_sinitadv_coasterid(int32_t handle) { return GetCoasterInstanceID(handle); }
void tpw_sinitadv_destroyqueue(uint64_t queue) { DestroyQueue(reinterpret_cast<sWraparoundQueue*>(queue)); }
int32_t tpw_sinitadv_eos(uint64_t instance, float position) {
    return IsEndOfSampleBuffer(reinterpret_cast<tag_sCoasterInstance*>(instance), position);
}
void tpw_sinitadv_dopf(uint64_t instance) { DoImmediatePhysicsFeedback(reinterpret_cast<tag_sCoasterInstance*>(instance)); }
void tpw_sinitadv_peep(uint32_t id, uint64_t a, uint64_t b) {
    GetCoasterPeepStats(id, reinterpret_cast<float*>(a), reinterpret_cast<float*>(b));
}

}  // extern "C"
