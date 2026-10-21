# Unit w187: CMechanic (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, SetState, ModelState, UpdateTurn, ChooseNextAction, the four
# Process states and the static initialiser, src/game/people/mechanic.cpp). Every callee outside them (the CStaff, CPeep and CObject members, CWorld::Register and
# Unregister, GetRand, GetMechanicHQ and GetAdvisor, CMechanicHQ::GetNewJob, the advisor request and its queue, the script and sound calls, the string table,
# the message centre, the serialise instances, the trace and the assertion) is a recording fake in tests/unit_Mechanic.cpp, which also defines the globals
# of other files.
set(TPW_UNIT_Mechanic src/game/people/mechanic.cpp platform/arena.cpp)
set(TPW_UNIT_Mechanic_KEEP
  __ZN9CMechanic10ModelStateEv   # CMechanic::ModelState()
  __ZN9CMechanic10UpdateTurnEv   # CMechanic::UpdateTurn()
  __ZN9CMechanic11ProcessIdleEv   # CMechanic::ProcessIdle()
  __ZN9CMechanic12FinalDestroyEv   # CMechanic::FinalDestroy()
  __ZN9CMechanic14ReceiveMessageERK4CMsg   # CMechanic::ReceiveMessage(CMsg const&)
  __ZN9CMechanic16ChooseNextActionEv   # CMechanic::ChooseNextAction()
  __ZN9CMechanic16ProcessRepairingEv   # CMechanic::ProcessRepairing()
  __ZN9CMechanic17ProcessPatrollingEv   # CMechanic::ProcessPatrolling()
  __ZN9CMechanic25ProcessWalkingToBreakdownEv   # CMechanic::ProcessWalkingToBreakdown()
  __ZN9CMechanic4InitEh   # CMechanic::Init(unsigned char)
  __ZN9CMechanic8SetStateEN6CStaff10StaffStateE   # CMechanic::SetState(CStaff::StaffState)
  __ZN9CMechanic9SerialiseEPviij   # CMechanic::Serialise(void*, int, int, unsigned int)
  __ZN9CMechanicC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMechanic::CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN9CMechanicC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEihi   # CMechanic::CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, unsigned char, int)
  _gMechanicSinitHigh
  _gMechanicSinitLow
  _gMechanicSprite
  _gMechanicSpriteGuard
)
