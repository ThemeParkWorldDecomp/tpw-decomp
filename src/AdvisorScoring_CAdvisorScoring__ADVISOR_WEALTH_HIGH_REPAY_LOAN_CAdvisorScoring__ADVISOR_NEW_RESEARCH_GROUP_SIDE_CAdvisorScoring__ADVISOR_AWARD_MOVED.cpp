// Unit v51116: the 8 byte scorers (a balance value read, or 0), two small destructors and the sound library's inline byte swaps.
#include "AdvisorScoring.h"

#include <cstring>

#include "platform/samsutils.h"

namespace {
// The Mac offset of a balance value in the scoring object: the table is at 0x0c, its values from +8, so slot i is at 0x14 + 4 * i.
inline int32_t Bal(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}
}  // namespace

int32_t CAdvisorScoring::_ADVISOR_NO_SCORE() { return 0; }                          // 0x10011358
int32_t CAdvisorScoring::_ADVISOR_WELCOME() { return Bal(this, 0xa0); }             // 0x1001139c
int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_COVERAGE() { return 0; }              // 0x10012d2c
int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_COVERAGE_LOST_WORLD() { return 0; }   // 0x10012d7c
int32_t CAdvisorScoring::_ADVISOR_RIDES_BREAKDOWN_MECHANIC_ON_IT() { return Bal(this, 0x254); }   // 0x10014500
int32_t CAdvisorScoring::_ADVISOR_FEATURES_FAR_TO_TOILETS() { return Bal(this, 0x2c4); }          // 0x10015514
int32_t CAdvisorScoring::_ADVISOR_FEATURES_BUILD_REST_AREA() { return 0; }          // 0x10015bc0
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_RIDE_RESEARCHED() { return Bal(this, 0x300); }         // 0x10015dc0
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_ADDON_RESEARCHED() { return Bal(this, 0x304); }        // 0x10015e14
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_SHOP_RESEARCHED() { return Bal(this, 0x308); }         // 0x10015e68
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_SIDESHOW_RESEARCHED() { return Bal(this, 0x30c); }     // 0x10015ebc
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_FEATURE_RESEARCHED() { return Bal(this, 0x310); }      // 0x10015f14
int32_t CAdvisorScoring::_ADVISOR_GENERAL_WON_GOLDEN_TICKET() { return Bal(this, 0x31c); }        // 0x10016038
int32_t CAdvisorScoring::_ADVISOR_AWARD_GIVEN() { return Bal(this, 0x320); }                      // 0x1001608c
int32_t CAdvisorScoring::_ADVISOR_AWARD_MOVED() { return Bal(this, 0x324); }                      // 0x100160d0

// 0x1001010c: the vptr store (Mac +4) has no host equivalent.
CAdvisorBalance* CAdvisorBalanceDestruct(CAdvisorBalance* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->UnInit();
        if (deleteFlag > 0) v51116_callee::OperatorDelete(self);
    }
    return self;
}

// 0x1000e2d0
CMsgTag* CMsgTagDestruct(CMsgTag* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtable = 0x1027de84;
        if (deleteFlag > 0) v51116_callee::OperatorDelete(self);
    }
    return self;
}

int32_t CMsgTag::GetType() const { return 0x18; }   // 0x1000e90c

namespace v51116_swap {
namespace {
inline uint32_t Swap(uint32_t v) { return __builtin_bswap32(v); }
template<class T> inline T* SwapPtr(T* p) { return reinterpret_cast<T*>(static_cast<uintptr_t>(Swap(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))))); }
inline void Inplace(uint32_t& word) { word = __builtin_bswap32(word); }
}  // namespace

tagBUMPRIDE* USwap32_tagBUMPRIDE(tagBUMPRIDE* p) { return SwapPtr(p); }   // 0x10021bf8
tabCOLOBJECT* USwap32_tabCOLOBJECT(tabCOLOBJECT* p) { return SwapPtr(p); }   // 0x10021c6c
tagBUMP* USwap32_tagBUMP(tagBUMP* p) { return SwapPtr(p); }   // 0x10021ce0
tagPEEPOFF* USwap32_tagPEEPOFF(tagPEEPOFF* p) { return SwapPtr(p); }   // 0x10021d48
int32_t* USwap32_long_ptr(int32_t* p) { return SwapPtr(p); }   // 0x10009ad0

void USwapInplace32_long_10021db8(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_ulong_10021dfc(uint32_t& v) { Inplace(v); }
void USwapInplace32_long_10009c74(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_int_10009c30(int32_t& v) { Inplace(reinterpret_cast<uint32_t&>(v)); }
void USwapInplace32_ulong_10009b2c(uint32_t& v) { Inplace(v); }

void USwapBlock32_ulong(uint32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(words, count); }   // 0x10009bd0
void USwapBlock32_long(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }   // 0x10009b74
}  // namespace v51116_swap
