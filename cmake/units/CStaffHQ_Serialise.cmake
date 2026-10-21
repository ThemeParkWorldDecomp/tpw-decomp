# Unit v5918: CStaffHQ::Serialise, ReceiveMessage, UpdateMonthly, FindNearestAvailableGuard (src/game/buildings/StaffHQ.cpp).
# The callees outside the unit are recording fakes in tests/unit_CStaffHQ_Serialise.cpp, which also defines the globals of other files.
set(TPW_UNIT_CStaffHQ_Serialise src/game/buildings/StaffHQ.cpp platform/arena.cpp)
set(TPW_UNIT_CStaffHQ_Serialise_KEEP
  __ZN8CStaffHQ13UpdateMonthlyEv   # CStaffHQ::UpdateMonthly()
  __ZN8CStaffHQ14ReceiveMessageERK4CMsg   # CStaffHQ::ReceiveMessage(CMsg const&)
  __ZN8CStaffHQ25FindNearestAvailableGuardE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CStaffHQ::FindNearestAvailableGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN8CStaffHQ9SerialiseEPviij   # CStaffHQ::Serialise(void*, int, int, unsigned int)
  _gStaffHQGuardListEnd
  _gStaffHQNoGuard
  _gStaffHQNoStaff
)
