# Unit v5235: COnlineControl::SaveIgnore and LoadIgnore (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_SaveIgnore.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_SaveIgnore src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_SaveIgnore_KEEP
  __Z16OnlineSaveIgnoreP14COnlineControlPKc   # OnlineSaveIgnore(COnlineControl*, char const*)
  __ZN14COnlineControl10LoadIgnoreEPKc   # COnlineControl::LoadIgnore(char const*)
  __ZN14COnlineControl10SaveIgnoreEPKc   # COnlineControl::SaveIgnore(char const*)
)
