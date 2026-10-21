# Unit v5580: RideStatusPanel::Condemned and ::InitStatusWindow (src/game/ui/ui_RideStatus.cpp). The callees
# outside the unit are recording fakes in tests/unit_RideStatusPanel_Condemned.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideStatusPanel_Condemned src/game/ui/ui_RideStatus.cpp platform/arena.cpp)
set(TPW_UNIT_RideStatusPanel_Condemned_KEEP
  __ZN15RideStatusPanel16InitStatusWindowEv   # RideStatusPanel::InitStatusWindow()
  __ZN15RideStatusPanel9CondemnedE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # RideStatusPanel::Condemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
)
