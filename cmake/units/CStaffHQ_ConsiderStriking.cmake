# Unit v5681: CStaffHQ::ConsiderStriking (src/game/buildings/StaffHQ.cpp). The callees outside the unit are
# recording fakes in tests/unit_CStaffHQ_ConsiderStriking.cpp, which also defines the globals of other files.
set(TPW_UNIT_CStaffHQ_ConsiderStriking src/game/buildings/StaffHQ.cpp platform/arena.cpp)
set(TPW_UNIT_CStaffHQ_ConsiderStriking_KEEP
  __ZN14staffhq_callee11OperatorNewEj   # staffhq_callee::OperatorNew(unsigned int)
  __ZN14staffhq_callee22ConstructSubscriptionsEPv   # staffhq_callee::ConstructSubscriptions(void*)
  __ZN8CStaffHQ16ConsiderStrikingE9StaffType   # CStaffHQ::ConsiderStriking(StaffType)
)
