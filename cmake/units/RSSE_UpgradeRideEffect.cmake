# Unit v5020: RSSE_UpgradeRideEffect (src/game/rides/rsse.cpp). The callees outside the unit are recording fakes in
# tests/unit_RSSE_UpgradeRideEffect.cpp, which also defines the globals of other files.
set(TPW_UNIT_RSSE_UpgradeRideEffect src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_RSSE_UpgradeRideEffect_KEEP
  __Z22RSSE_UpgradeRideEffecti   # RSSE_UpgradeRideEffect(int)
  _gRSSEUpgradeSkipThing
)
