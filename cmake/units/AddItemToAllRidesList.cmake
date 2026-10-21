# Unit v5219: the all-kids panel's creation and rows, the rides and sideshows list rows (src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp). The callees outside the unit are
# recording fakes in tests/unit_AddItemToAllRidesList.cpp, which also defines the globals of other files.
set(TPW_UNIT_AddItemToAllRidesList src/game/ui/ui_allItemsPanel.cpp src/game/ui/ui_allKidsPanel.cpp platform/arena.cpp)
set(TPW_UNIT_AddItemToAllRidesList_KEEP
  __Z18CreateAllKidsPanelP6CThing   # CreateAllKidsPanel(CThing*)
  __Z21AddItemToAllRidesListPK6CThingP21InterfaceListControl2P7CObject   # AddItemToAllRidesList(CThing const*, InterfaceListControl2*, CObject*)
  __Z21AllKidsPanel_KidAddedP6CThing   # AllKidsPanel_KidAdded(CThing*)
  __Z21UpdateRideListEntriesP21InterfaceListControl2   # UpdateRideListEntries(InterfaceListControl2*)
  __Z28FillAllSideshowsListCallbackP6CThing   # FillAllSideshowsListCallback(CThing*)
  _gAllKidsColumnFont
  _gAllKidsCounter
  _gAllKidsScript
  _gAllRidesNoParent
)
