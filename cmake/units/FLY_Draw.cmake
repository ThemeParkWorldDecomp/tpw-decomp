# Unit v5007: FLY_Draw (src/game/rides/FlyingRide.cpp). The callees outside the unit are recording fakes in
# tests/unit_FLY_Draw.cpp, which also defines the globals of other files.
set(TPW_UNIT_FLY_Draw src/game/rides/FlyingRide.cpp platform/arena.cpp)
set(TPW_UNIT_FLY_Draw_KEEP
  __Z8FLY_Drawf   # FLY_Draw(float)
  _gFlyDrawAngA
  _gFlyDrawAngB
  _gFlyDrawAngC
  _gFlyDrawOrigin
  _gFlyDrawPosX
  _gFlyDrawPosY
  _gFlyDrawPosZ
)
