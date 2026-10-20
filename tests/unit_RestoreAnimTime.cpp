// Test library for RestoreAnimTime (unit v5561; libtpw_unit_RestoreAnimTime.dylib, tests/test_RestoreAnimTime.py), main app:
//   src/RestoreAnimTime.cpp
//
// IsValidAnimation and RestoreAnimTimeStats are fakes that record their arguments; the header's first word is a marker the fake logs.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b) {
    static_cast<void>(set);
    ev("IsValidAnimation", {static_cast<int64_t>(a), static_cast<int64_t>(b)});
    return static_cast<int32_t>(ret("IsValidAnimation"));
}

void RestoreAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation) {
    static_cast<void>(control);
    ev("RestoreAnimTimeStats", {static_cast<int64_t>(animation->mF00)});
}

extern "C" {

uint64_t tpw_rat_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// set size, groups offset, group size, entries offset in group, entry size, animation offset in entry, control size
int32_t tpw_rat_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mGroups));
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimGroup));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimGroup, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(sRideAnimEntry));
    out[n++] = static_cast<int64_t>(offsetof(sRideAnimEntry, mAnimation));
    out[n++] = static_cast<int64_t>(offsetof(sAnimTimeControl, mF30));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControlSave));
    return n;
}

uint32_t tpw_rat_call(uint64_t set, uint64_t control, uint64_t save) {
    return RestoreAnimTime(reinterpret_cast<tag_sRideAnimSet*>(set), reinterpret_cast<sAnimTimeControl*>(control),
                           reinterpret_cast<sAnimTimeControlSave*>(save)) ? 1u : 0u;
}

}  // extern "C"
