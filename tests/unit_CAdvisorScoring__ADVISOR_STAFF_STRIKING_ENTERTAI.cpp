// Test library for the CAdvisorScoring word getters (unit v51055), main app: the src file named in the cmake. No callees, no globals.
// The object is a zeroed arena block; the getters read raw words at Mac offsets from this.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "platform/arena.h"

extern "C" {

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_adv_call(int32_t idx, uint64_t object) {
    CAdvisorScoring* o = reinterpret_cast<CAdvisorScoring*>(static_cast<uintptr_t>(object));
    switch (idx) {
        case 0: return o->_ADVISOR_STAFF_STRIKING_ENTERTAINERS();
        case 1: return o->_ADVISOR_STAFF_STRIKING_GUARDS();
        case 2: return o->_ADVISOR_STAFF_STRIKING_RESEARCHERS();
        case 3: return o->_ADVISOR_STAFF_STRIKING_HANDYMEN();
        case 4: return o->_ADVISOR_STAFF_HAPPIER_MECHANICS();
        case 5: return o->_ADVISOR_STAFF_STRIKING_MECHANICS();
        case 6: return o->_ADVISOR_STAFF_HAPPIER_HANDYMEN();
        case 7: return o->_ADVISOR_STAFF_HAPPIER_GUARDS();
        case 8: return o->_ADVISOR_STAFF_VERY_UNHAPPY_RESEARCHERS();
        case 9: return o->_ADVISOR_STAFF_HAPPIER_ENTERTAINERS();
        case 10: return o->_ADVISOR_STAFF_VERY_UNHAPPY_ENTERTAINERS();
        case 11: return o->_ADVISOR_STAFF_HAPPIER_RESEARCHERS();
        case 12: return o->_ADVISOR_STAFF_VERY_UNHAPPY_GUARDS();
        case 13: return o->_ADVISOR_STAFF_STRIKE_END_BAD_MECHANICS();
        case 14: return o->_ADVISOR_STAFF_VERY_UNHAPPY_HANDYMEN();
        case 15: return o->_ADVISOR_STAFF_STRIKE_END_BAD_HANDYMEN();
        case 16: return o->_ADVISOR_STAFF_VERY_UNHAPPY_MECHANICS();
        case 17: return o->_ADVISOR_STAFF_STRIKE_END_BAD_GUARDS();
        case 18: return o->_ADVISOR_STAFF_UNHAPPY_RESEARCHERS();
        case 19: return o->_ADVISOR_STAFF_STRIKE_END_BAD_ENTERTAINERS();
        case 20: return o->_ADVISOR_STAFF_UNHAPPY_ENTERTAINERS();
        case 21: return o->_ADVISOR_STAFF_UNHAPPY_GUARDS();
        case 22: return o->_ADVISOR_STAFF_STRIKE_END_BAD_RESEARCHERS();
        case 23: return o->_ADVISOR_STAFF_UNHAPPY_HANDYMEN();
        case 24: return o->_ADVISOR_RIDES_LONG_QUEUES_BORED_VISITORS();
        case 25: return o->_ADVISOR_STAFF_UNHAPPY_MECHANICS();
        case 26: return o->_ADVISOR_RIDES_USE_UPGRADES();
        case 27: return o->_ADVISOR_RIDES_BREAKDOWN_IMMINENT();
        case 28: return o->_ADVISOR_RIDES_BREAKDOWN_NO_MECHANICS();
        case 29: return o->_ADVISOR_RIDES_BREAKDOWN_BUSY_MECHANICS();
    }
    return 0;
}

}  // extern "C"
