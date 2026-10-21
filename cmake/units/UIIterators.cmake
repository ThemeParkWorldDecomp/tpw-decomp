# Unit w227: UI thing-list walks, dispatchers and wrapping searches. All external callees are recording fakes.
set(TPW_UNIT_UIIterators src/game/ui/ui_iterators.cpp)
set(TPW_UNIT_UIIterators_KEEP
  __Z11NextUpgradeP6CThing   # NextUpgrade(CThing*)
  __Z11PreviousBogP6CThing   # PreviousBog(CThing*)
  __Z13NextStaffRoomP6CThing   # NextStaffRoom(CThing*)
  __Z15PreviousUpgradeP6CThing   # PreviousUpgrade(CThing*)
  __Z16NextObjectOfType9UIObjTypeP6CThing   # NextObjectOfType(UIObjType, CThing*)
  __Z17PreviousStaffRoomP6CThing   # PreviousStaffRoom(CThing*)
  __Z19NextNonTrackUpgradeP6CThing   # NextNonTrackUpgrade(CThing*)
  __Z19UI_IterateThingList15UIIteratorTypesPFiP6CThingE   # UI_IterateThingList(UIIteratorTypes, int (*)(CThing*))
  __Z19UI_IterateThingNext15UIIteratorTypesP6CThing   # UI_IterateThingNext(UIIteratorTypes, CThing*)
  __Z20PreviousObjectOfType9UIObjTypeP6CThing   # PreviousObjectOfType(UIObjType, CThing*)
  __Z23PreviousNonTrackUpgradeP6CThing   # PreviousNonTrackUpgrade(CThing*)
  __Z23UI_IterateThingPrevious15UIIteratorTypesP6CThing   # UI_IterateThingPrevious(UIIteratorTypes, CThing*)
  __Z23UI_WrapIterateThingNext15UIIteratorTypesP6CThing   # UI_WrapIterateThingNext(UIIteratorTypes, CThing*)
  __Z27UI_WrapIterateThingPrevious15UIIteratorTypesP6CThing   # UI_WrapIterateThingPrevious(UIIteratorTypes, CThing*)
  __Z28NextFeatureNotBogOrStaffRoomP6CThing   # NextFeatureNotBogOrStaffRoom(CThing*)
  __Z32PreviousFeatureNotBogOrStaffRoomP6CThing   # PreviousFeatureNotBogOrStaffRoom(CThing*)
  __Z7NextBogP6CThing   # NextBog(CThing*)
  _gUIIterFileWord10396A4C
  _gUIIterFileWord10396A50
)
