# Unit v5947: CStaffHQ::StaffTypeHasAGrievance (src/game/buildings/StaffHQ.cpp). The callees
# outside the unit are recording fakes in tests/unit_CStaffHQ_StaffTypeHasAGrievance.cpp, which also defines gWorld and gThingArray.
set(TPW_UNIT_CStaffHQ_StaffTypeHasAGrievance src/game/buildings/StaffHQ.cpp platform/arena.cpp)
set(TPW_UNIT_CStaffHQ_StaffTypeHasAGrievance_KEEP
  __ZN8CStaffHQ22StaffTypeHasAGrievanceE9StaffType   # CStaffHQ::StaffTypeHasAGrievance(StaffType)
  _gStaffHQGrievanceEnd
)
