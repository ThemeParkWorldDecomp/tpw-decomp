# Unit v5298: the CPeep destructor, HideMySprite, DrawMySpriteAt and FinalDestroy
# (src/game/people/Peep.cpp, src/game/people/Person.cpp). The callees outside the unit are recording fakes in
# tests/unit_CPeep_Dtor_CPeep.cpp.
set(TPW_UNIT_CPeep_Dtor_CPeep src/game/people/Peep.cpp src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPeep_Dtor_CPeep_KEEP
  __ZN5CPeep12FinalDestroyEv   # CPeep::FinalDestroy()
  __ZN5CPeep12HideMySpriteEv   # CPeep::HideMySprite()
  __ZN5CPeep14DrawMySpriteAtEfffiN14CDelayedScript9ScriptIdxE   # CPeep::DrawMySpriteAt(float, float, float, int, CDelayedScript::ScriptIdx)
  __ZN5CPeepD1Ev   # CPeep::~CPeep()
)
