# Unit v5293: CPerson balloon, happiness and catch helpers (src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_GetBagged.cpp.
set(TPW_UNIT_CPerson_GetBagged src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_GetBagged_KEEP
  __ZN7CPerson10LosingModsEP7CObject   # CPerson::LosingMods(CObject*)
  __ZN7CPerson11DrawBalloonEfffi   # CPerson::DrawBalloon(float, float, float, int)
  __ZN7CPerson14LowerHappinessENS_6AmountE   # CPerson::LowerHappiness(CPerson::Amount)
  __ZN7CPerson14MakeBalloonPopEv   # CPerson::MakeBalloonPop()
  __ZN7CPerson14RaiseHappinessENS_6AmountE   # CPerson::RaiseHappiness(CPerson::Amount)
  __ZN7CPerson9GetBaggedEv   # CPerson::GetBagged()
  __ZNK7CPerson11IsCatchableEv   # CPerson::IsCatchable() const
  __ZNK7CPerson18GetHappinessAmountENS_6AmountE   # CPerson::GetHappinessAmount(CPerson::Amount) const
  _gBalloonPopTarget
)
