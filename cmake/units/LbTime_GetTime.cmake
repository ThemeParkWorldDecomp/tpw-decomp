# Unit v5309: LbTime_GetTime and LbTime_GetDate (src/bullfrog/LbTime.cpp). NS_MacDoze::GetLocalTime is a recording fake in
# tests/unit_LbTime_GetTime.cpp.
set(TPW_UNIT_LbTime_GetTime src/bullfrog/LbTime.cpp platform/arena.cpp)
set(TPW_UNIT_LbTime_GetTime_KEEP
  __Z14LbTime_GetDateP6TbDate   # LbTime_GetDate(TbDate*)
  __Z14LbTime_GetTimeP6TbTime   # LbTime_GetTime(TbTime*)
)
