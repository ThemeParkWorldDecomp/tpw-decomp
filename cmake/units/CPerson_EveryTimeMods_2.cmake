# Unit v51016: CPerson::EveryTimeMods, UpdateTemporaryHistory, UseObject, GetTimeInPark, LookForMinorDest
# (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_EveryTimeMods_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_EveryTimeMods_2 src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_EveryTimeMods_2_KEEP
  __ZN7CPerson13EveryTimeModsEP7CObject   # CPerson::EveryTimeMods(CObject*)
  __ZN7CPerson16LookForMinorDestEv   # CPerson::LookForMinorDest()
  __ZN7CPerson22UpdateTemporaryHistoryEv   # CPerson::UpdateTemporaryHistory()
  __ZN7CPerson9UseObjectEP7CObject   # CPerson::UseObject(CObject*)
  __ZNK7CPerson13GetTimeInParkEv   # CPerson::GetTimeInPark() const
  _gMinorListEnd
  _gMinorNoDest
  _gMinorNoDest2
  _gMinorNoQueue
  _gMinorNoRide
)
