# Unit v5236: FLY_CreateBase and EmployEric (src/game/rides/FlyingRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_FLY_CreateBase.cpp, which also defines the globals of other files.
set(TPW_UNIT_FLY_CreateBase src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_CreateBase_KEEP
  __Z10EmployErici   # EmployEric(int)
  __Z14FLY_CreateBasejiiiiii   # FLY_CreateBase(unsigned int, int, int, int, int, int, int)
)
