# Unit v5281: CPeep::PlaySound, IsInsidePark, Teleport, UpdateTurn, IsStranded, SetStranded, SetDest x2 (src/game/people/Peep.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPeep_PlaySound.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPeep_PlaySound src/game/people/Peep.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_PlaySound_KEEP
  __ZN5CPeep10IsStrandedEv   # CPeep::IsStranded()
  __ZN5CPeep10UpdateTurnEv   # CPeep::UpdateTurn()
  __ZN5CPeep11SetStrandedEi   # CPeep::SetStranded(int)
  __ZN5CPeep7SetDestE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN5CPeep7SetDestE6MapPosS0_   # CPeep::SetDest(MapPos, MapPos)
  __ZN5CPeep8TeleportE6MapPosS0_   # CPeep::Teleport(MapPos, MapPos)
  __ZN5CPeep9PlaySoundEii   # CPeep::PlaySound(int, int)
  __ZNK5CPeep12IsInsideParkEv   # CPeep::IsInsidePark() const
  _gPeepNullCellId
)
