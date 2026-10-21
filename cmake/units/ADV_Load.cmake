# Unit v5685: ADV_Load and ADV_Save (src/game/advisor/Advisor.cpp). The callees outside the unit are recording fakes in
# tests/unit_ADV_Load.cpp, which also defines the globals of other files.
set(TPW_UNIT_ADV_Load src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_ADV_Load_KEEP
  __Z8ADV_LoadPv   # ADV_Load(void*)
  __Z8ADV_SavePv   # ADV_Save(void*)
)
