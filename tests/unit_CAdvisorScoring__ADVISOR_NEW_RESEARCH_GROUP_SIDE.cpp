// Test library for the v51068 CAdvisorScoring scorers (libtpw_unit_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE.dylib), main app.
// The object is a raw arena block filled with Mac-offset words; no callees.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "platform/arena.h"
#include "AdvisorScoring.h"

extern "C" {

uint64_t tpw_advs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The words are the Mac object (word i at offset 4*i); word 5 + s is balance slot s. Copied in, called, copied back.
#define W(NAME) \
    uint32_t tpw_advs_##NAME(uint64_t block) { \
        uint32_t* words = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(block)); \
        alignas(16) static unsigned char raw[sizeof(CAdvisorScoring)]; \
        std::memset(raw, 0, sizeof(raw)); \
        CAdvisorScoring& obj = *reinterpret_cast<CAdvisorScoring*>(raw); \
        for (int32_t s = 0; s < kAdvisorBalanceSlots; ++s) obj.mBalance.mValues[s].mInteger = static_cast<int32_t>(words[5 + s]); \
        uint32_t answer = static_cast<uint32_t>(obj.NAME()); \
        for (int32_t s = 0; s < kAdvisorBalanceSlots; ++s) words[5 + s] = static_cast<uint32_t>(obj.mBalance.mValues[s].mInteger); \
        return answer; \
    }
W(_ADVISOR_PRANK_SBOMB)
W(_ADVISOR_PRANK_LITTER)
W(_ADVISOR_PRANK_BALLOON)
W(_ADVISOR_PARK_NOW_OPEN)
W(_ADVISOR_PARK_NOW_CLOSED)
W(_ADVISOR_UPGRADE_HIRE_MECHANIC)
W(_ADVISOR_UPGRADE_STIKING_MECHANICS)
W(_ADVISOR_NEW_UPGRADE_NO_MECHANIC)
W(_ADVISOR_NEW_RESEARCH_GROUP_RIDE)
W(_ADVISOR_NEW_RESEARCH_GROUP_SHOP)
W(_ADVISOR_NEW_RESEARCH_GROUP_SIDESHOW)
W(_ADVISOR_NEW_RESEARCH_GROUP_FEATURE)
W(_ADVISOR_BUS_ARRIVED)
W(_ADVISOR_PLANE_ARRIVED)
W(_ADVISOR_ONLINE_NEW_ITEM)
W(_ADVISOR_ONLINE_MESSAGE)
W(_ADVISOR_CONGRAT_BIG_COASTER)
W(_ADVISOR_CONGRAT_FUN_COASTER)
W(_ADVISOR_CONGRAT_FUN_GOKART)
W(_ADVISOR_CONGRAT_FUN_WATERRIDE)
W(_ADVISOR_CONGRAT_FAST_GOKART)
W(_ADVISOR_CONGRAT_GIFTS)
W(_ADVISOR_CONGRAT_GOLDEN_KEY_NONE)
W(_ADVISOR_CONGRAT_GOLDEN_KEY_JUNGLE)
W(_ADVISOR_CONGRAT_GOLDEN_KEY_HALLOWEEN)
W(_ADVISOR_CONGRAT_GOLDEN_KEY_SPACE)
W(_ADVISOR_CONGRAT_GOLDEN_KEY_FANTASY)
W(_ADVISOR_CONGRAT_KEY_AT_START)
W(_ADVISOR_CONGRAT_ALL_COMPLETE)
W(_ADVISOR_CONGRAT_ALL_COMPLETE_LEAVING)

}  // extern "C"
