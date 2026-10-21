# Unit v5276: the park-goto callbacks (src/game/online/online.cpp). The callees outside the unit are recording fakes in
# tests/unit_GotoPark_OK.cpp, which also defines the globals of other files.
set(TPW_UNIT_GotoPark_OK src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_GotoPark_OK_KEEP
  __Z11GotoPark_OKPv   # GotoPark_OK(void*)
  __Z14GotoCallbackFniPKv   # GotoCallbackFn(int, void const*)
)
