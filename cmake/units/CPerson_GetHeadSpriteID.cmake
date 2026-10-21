# Unit v51034: CPerson::GetHeadSpriteID and six more (src/game/people/ParkTime.cpp, src/game/people/Peep.cpp, src/game/people/Person.cpp).
# The callees outside the unit are recording fakes in tests/unit_CPerson_GetHeadSpriteID.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_GetHeadSpriteID src/game/people/ParkTime.cpp src/game/people/Peep.cpp src/game/people/Person.cpp
    platform/arena.cpp)
set(TPW_UNIT_CPerson_GetHeadSpriteID_KEEP
  __Z7LOADVARI11ESP_SpritesEiPvPT_PKci   # int LOADVAR<ESP_Sprites>(void*, ESP_Sprites*, char const*, int)
  __ZN5PilotD1Ev   # Pilot::~Pilot()
  __ZN7CPerson11SetSpotAnimEN14CDelayedScript9ScriptIdxE   # CPerson::SetSpotAnim(CDelayedScript::ScriptIdx)
  __ZN7CPerson12FinalDestroyEv   # CPerson::FinalDestroy()
  __ZN7CPerson15GetHeadSpriteIDEv   # CPerson::GetHeadSpriteID()
  __ZN7CPerson23ScoreOnCloserInspectionEP7CObject   # CPerson::ScoreOnCloserInspection(CObject*)
  __ZNK9CParkTime25GetFunnyTimeSinceGameTurnEj   # CParkTime::GetFunnyTimeSinceGameTurn(unsigned int) const
)
