# Unit v5037: CStaffHQ::DropStaff and ForceStrike (src/game/buildings/StaffHQ.cpp). The callees are recording fakes in
# tests/unit_CStaffHQ_DropStaff.cpp, which also defines gThingArray.
set(TPW_UNIT_CStaffHQ_DropStaff src/game/buildings/StaffHQ.cpp platform/arena.cpp)
set(TPW_UNIT_CStaffHQ_DropStaff_KEEP
  __ZN8CStaffHQ11ForceStrikeE9StaffType   # CStaffHQ::ForceStrike(StaffType)
  __ZN8CStaffHQ9DropStaffE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CStaffHQ::DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
)
