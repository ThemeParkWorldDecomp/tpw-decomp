# Unit v5209: FLY_Load, FLY_RestoreMeshes, OBJECT_Load (src/game/HLSprite.cpp, src/game/rides/FlyingRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_FLY_Load.cpp, which also defines the globals of other files.
set(TPW_UNIT_FLY_Load src/game/HLSprite.cpp src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_Load_KEEP
  __Z11OBJECT_LoadPc   # OBJECT_Load(char*)
  __Z17FLY_RestoreMeshesv   # FLY_RestoreMeshes()
  __Z8FLY_LoadPv   # FLY_Load(void*)
)
