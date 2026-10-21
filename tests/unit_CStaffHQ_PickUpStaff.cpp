// Test library for the small functions of unit v51052 (libtpw_unit_CStaffHQ_PickUpStaff.dylib, tests/test_CStaffHQ_PickUpStaff.py), main app:
//   src/game/Thing.cpp, src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/buildings/TrainingCentre.cpp, src/game/people/Thoughts.cpp, src/game/ui/vector2d.cpp
//
// The callees outside the unit are recording fakes: CThing::FinalDestroy, CThing::Serialise, SCR_KillScript, operator delete, the wide string
// constructor and __register_global_object. One dispatcher (tpw_v51052_call) reaches every function of the unit by number.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "analyser.h"

using namespace calllog;

namespace {
template <class T> auto CStr(const T& t, int) -> decltype(t.c_str()) { return t.c_str(); }
template <class T> const char* CStr(const T& t, long) { return t; }
}  // namespace

// ---- the fakes
void CThing::FinalDestroy() { ev("CThingFinalDestroy", {lab(this)}); }

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThingSerialise", {lab(this), lab(file), save, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("CThingSerialise"));
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

namespace v51052_callee {
void OperatorDelete(void* block) { ev("OperatorDelete", {lab(block)}); }
}  // namespace v51052_callee

void UIMain_DestroyWString(void*) {}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), lab(destructor), lab(node)});
}

// The wide string constructor and the virtuals its vtable needs (the real ones are other units').
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    auto n = narrow(text);
    ev("StrCtor", {lab(this)}, {CStr(n, 0)});
}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

extern "C" {

uint64_t tpw_v51052_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"staffH", &gStaffHQSinitHigh},
        {"staffL", &gStaffHQSinitLow},
        {"thingH", &gThingSinitHigh},
        {"thingL", &gThingSinitLow},
        {"trainH", &gTrainingCentreSinitHigh},
        {"trainL", &gTrainingCentreSinitLow},
        {"vecH", &gVector2dSinitHigh},
        {"vecL", &gVector2dSinitLow},
        {"rlstr", gResearchLabString},
        {"rlnode", gResearchLabStringNode},
        {"glue", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51052_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51052_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mStrikes));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF60));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF64));
    out[n++] = static_cast<int64_t>(sizeof(CStaffHQ));
    return n;
}

// fn numbers are the order of FNS in the test module.
uint64_t tpw_v51052_call(int32_t fn, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    auto P = [](uint64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); };
    CStaffHQ* staff = static_cast<CStaffHQ*>(P(a));
    switch (fn) {
    case 0: {
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
        id.mIndex = static_cast<uint16_t>(b);
        staff->PickUpStaff(id);
        return 0;
    }
    case 1: staff->StopStrikeInProgress(static_cast<StaffType>(static_cast<int32_t>(b))); return 0;
    case 2: return static_cast<uint32_t>(staff->IsStriking(static_cast<StaffType>(static_cast<int32_t>(b))));
    case 3: staff->FinalDestroy(); return 0;
    case 4: __sinit_StaffHQ_cpp(); return 0;
    case 5: static_cast<CTagSystem*>(P(a))->FinalDestroy(); return 0;
    case 6:
        return static_cast<uint32_t>(static_cast<CTagSystem*>(P(a))->Serialise(P(b), static_cast<int32_t>(c), static_cast<int32_t>(d),
                                                                               static_cast<uint32_t>(e)));
    case 7: __sinit_Thing_cpp(); return 0;
    case 8: EventHistType_Ctor(static_cast<CThoughts::EventHistType*>(P(a))); return 0;
    case 9: static_cast<CThoughts*>(P(a))->FinalDestroy(); return 0;
    case 10: __sinit_TrainingCentre_cpp(); return 0;
    case 11: static_cast<CUIMsgReceiver*>(P(a))->FinalDestroy(); return 0;
    case 12:
        return static_cast<uint32_t>(static_cast<CUIMsgReceiver*>(P(a))->Serialise(P(b), static_cast<int32_t>(c), static_cast<int32_t>(d),
                                                                                   static_cast<uint32_t>(e)));
    case 13: return reinterpret_cast<uintptr_t>(Vector2d_Dtor(static_cast<Vector2d*>(P(a)), static_cast<int16_t>(b)));
    case 14: __sinit_vector2d_cpp(); return 0;
    case 15: __sinit_ResearchLab_cpp(); return 0;
    case 16: return static_cast<uint32_t>(CMsgResearchCategoryUpdated_GetType(static_cast<CMsgResearchCategoryUpdated*>(P(a))));
    case 17:
        return reinterpret_cast<uintptr_t>(CMsgResearchCategoryUpdated_Dtor(static_cast<CMsgResearchCategoryUpdated*>(P(a)), static_cast<int16_t>(b)));
    case 18: return reinterpret_cast<uintptr_t>(Follow_Path_Dtor(static_cast<Follow_Path*>(P(a)), static_cast<int16_t>(b)));
    case 19: return reinterpret_cast<uintptr_t>(Avoid_Walls_Dtor(static_cast<Avoid_Walls*>(P(a)), static_cast<int16_t>(b)));
    case 20: return reinterpret_cast<uintptr_t>(Repel_Obstacle_Dtor(static_cast<Repel_Obstacle*>(P(a)), static_cast<int16_t>(b)));
    case 21: return reinterpret_cast<uintptr_t>(Avoid_Obstacle_Dtor(static_cast<Avoid_Obstacle*>(P(a)), static_cast<int16_t>(b)));
    case 22: return reinterpret_cast<uintptr_t>(Separation_Dtor(static_cast<Separation*>(P(a)), static_cast<int16_t>(b)));
    }
    return 0;
}

}  // extern "C"
