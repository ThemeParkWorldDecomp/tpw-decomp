# Unit w182: CGuard (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, ProsecutePerp, IsAvailable, IsCarrying, UpdateTurn, ChooseNextAction,
# ModelState, SetState and the five Process states, src/game/people/guard.cpp). Every callee outside them (the CStaff, CPeep and CPerson members, CWorld::Register and
# Unregister, the map's effectors and entrance cells, the park analyser, the script and sound calls, the string table, the message centre, the serialise
# instances, the trace and the assertion) is a recording fake in tests/unit_Guard.cpp, which also defines the globals of other files.
set(TPW_UNIT_Guard src/game/people/guard.cpp platform/arena.cpp)
set(TPW_UNIT_Guard_KEEP
  __ZN6CGuard10ModelStateEv   # CGuard::ModelState()
  __ZN6CGuard10UpdateTurnEv   # CGuard::UpdateTurn()
  __ZN6CGuard11ProcessIdleEv   # CGuard::ProcessIdle()
  __ZN6CGuard12FinalDestroyEv   # CGuard::FinalDestroy()
  __ZN6CGuard13ProsecutePerpE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CGuard::ProsecutePerp(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN6CGuard14ReceiveMessageERK4CMsg   # CGuard::ReceiveMessage(CMsg const&)
  __ZN6CGuard16ChooseNextActionEv   # CGuard::ChooseNextAction()
  __ZN6CGuard17ProcessPatrollingEv   # CGuard::ProcessPatrolling()
  __ZN6CGuard18ProcessProsecutingEv   # CGuard::ProcessProsecuting()
  __ZN6CGuard24ProcessReturningToPatrolEv   # CGuard::ProcessReturningToPatrol()
  __ZN6CGuard24ProcessWalkingToEntranceEv   # CGuard::ProcessWalkingToEntrance()
  __ZN6CGuard4InitEv   # CGuard::Init()
  __ZN6CGuard8SetStateEN6CStaff10StaffStateE   # CGuard::SetState(CStaff::StaffState)
  __ZN6CGuard9SerialiseEPviij   # CGuard::Serialise(void*, int, int, unsigned int)
  __ZN6CGuardC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CGuard::CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN6CGuardC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEihi   # CGuard::CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, unsigned char, int)
  __ZNK6CGuard10IsCarryingEv   # CGuard::IsCarrying() const
  __ZNK6CGuard11IsAvailableEv   # CGuard::IsAvailable() const
)
