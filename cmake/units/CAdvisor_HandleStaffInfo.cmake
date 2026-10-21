# Unit v5223: CAdvisor::HandleStaffInfo and CAdvisor::Serialise (src/game/advisor/Advisor.cpp). The callees outside the unit are
# recording fakes in tests/unit_CAdvisor_HandleStaffInfo.cpp, which also defines the globals of other files.
set(TPW_UNIT_CAdvisor_HandleStaffInfo src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_CAdvisor_HandleStaffInfo_KEEP
  __ZN8CAdvisor15HandleStaffInfoERK13CMsgStaffInfo   # CAdvisor::HandleStaffInfo(CMsgStaffInfo const&)
  __ZN8CAdvisor9SerialiseEPviij   # CAdvisor::Serialise(void*, int, int, unsigned int)
)
