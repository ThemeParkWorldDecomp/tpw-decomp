# Unit w184: CHandyman (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, ChooseNextAction, SetState, ModelState, the six Process
# states, FindNearestDirtyToilet and the static initialiser, src/game/people/Handyman.cpp). Every callee outside them (the CStaff, CPeep and CObject members, CWorld::Register and
# Unregister, the park analyser, the map cell calls, the script and sound calls, the string table, the message centre, the serialise instances, the trace and
# the assertion) is a recording fake in tests/unit_Handyman.cpp, which also defines the globals of other files.
set(TPW_UNIT_Handyman src/game/people/Handyman.cpp platform/arena.cpp)
set(TPW_UNIT_Handyman_KEEP
  __ZN9CHandyman10ModelStateEv   # CHandyman::ModelState()
  __ZN9CHandyman10UpdateTurnEv   # CHandyman::UpdateTurn()
  __ZN9CHandyman11ProcessIdleEv   # CHandyman::ProcessIdle()
  __ZN9CHandyman12FinalDestroyEv   # CHandyman::FinalDestroy()
  __ZN9CHandyman14ReceiveMessageERK4CMsg   # CHandyman::ReceiveMessage(CMsg const&)
  __ZN9CHandyman16ChooseNextActionEv   # CHandyman::ChooseNextAction()
  __ZN9CHandyman17ProcessPatrollingEv   # CHandyman::ProcessPatrolling()
  __ZN9CHandyman21ProcessCleaningLitterEv   # CHandyman::ProcessCleaningLitter()
  __ZN9CHandyman21ProcessCleaningToiletEv   # CHandyman::ProcessCleaningToilet()
  __ZN9CHandyman22FindNearestDirtyToiletEv   # CHandyman::FindNearestDirtyToilet()
  __ZN9CHandyman22ProcessWalkingToLitterEv   # CHandyman::ProcessWalkingToLitter()
  __ZN9CHandyman27ProcessWalkingToCleanToiletEv   # CHandyman::ProcessWalkingToCleanToilet()
  __ZN9CHandyman4InitEh   # CHandyman::Init(unsigned char)
  __ZN9CHandyman8SetStateEN6CStaff10StaffStateE   # CHandyman::SetState(CStaff::StaffState)
  __ZN9CHandyman9SerialiseEPviij   # CHandyman::Serialise(void*, int, int, unsigned int)
  __ZN9CHandymanC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CHandyman::CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN9CHandymanC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEihi   # CHandyman::CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, unsigned char, int)
  _gHandymanSinitHigh
  _gHandymanSinitLow
  _gHandymanSprite
  _gHandymanSpriteGuard
)
