#include "AdvisorScoring.h"

// Scorers that answer one balance value (slot at the Mac offset in the comment) or a constant.
namespace {
constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }
}  // namespace

int32_t CAdvisorScoring::_ADVISOR_PRANK_SBOMB() { return mBalance.mValues[SlotOf(0x3d4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PRANK_LITTER() { return mBalance.mValues[SlotOf(0x3d8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PRANK_BALLOON() { return mBalance.mValues[SlotOf(0x3dc)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PARK_NOW_OPEN() { return mBalance.mValues[SlotOf(0x3e0)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PARK_NOW_CLOSED() { return mBalance.mValues[SlotOf(0x3e4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_UPGRADE_HIRE_MECHANIC() { return mBalance.mValues[SlotOf(0x3e8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_UPGRADE_STIKING_MECHANICS() { return mBalance.mValues[SlotOf(0x3ec)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_UPGRADE_NO_MECHANIC() { return mBalance.mValues[SlotOf(0x3f0)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_RIDE() { return mBalance.mValues[SlotOf(0x3f4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_SHOP() { return mBalance.mValues[SlotOf(0x3f8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_SIDESHOW() { return mBalance.mValues[SlotOf(0x3fc)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_FEATURE() { return mBalance.mValues[SlotOf(0x400)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_BUS_ARRIVED() { return mBalance.mValues[SlotOf(0x404)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PLANE_ARRIVED() { return mBalance.mValues[SlotOf(0x408)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_ONLINE_NEW_ITEM() { return mBalance.mValues[SlotOf(0x424)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_ONLINE_MESSAGE() { return mBalance.mValues[SlotOf(0x428)].mInteger; }

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_BIG_COASTER() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_COASTER() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_GOKART() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_WATERRIDE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FAST_GOKART() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GIFTS() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_NONE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_JUNGLE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_HALLOWEEN() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_SPACE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_FANTASY() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_KEY_AT_START() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ALL_COMPLETE() { return 0x2710; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ALL_COMPLETE_LEAVING() { return 0x270f; }
