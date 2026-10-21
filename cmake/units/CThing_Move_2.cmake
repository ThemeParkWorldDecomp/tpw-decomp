# Unit v5927: CThing::Move, FinalDestroy, Serialise (src/game/Thing.cpp). The callees are recording fakes in
# tests/unit_CThing_Move_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CThing_Move_2 src/game/Thing.cpp platform/arena.cpp)
set(TPW_UNIT_CThing_Move_2_KEEP
  __ZN6CThing12FinalDestroyEv   # CThing::FinalDestroy()
  __ZN6CThing4MoveE6MapPosS0_   # CThing::Move(MapPos, MapPos)
  __ZN6CThing9SerialiseEPviij   # CThing::Serialise(void*, int, int, unsigned int)
)
