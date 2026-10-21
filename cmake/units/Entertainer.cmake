# Unit w178: CEntertainer (both constructors, Init, ReceiveMessage, Serialise, FinalDestroy, UpdateTurn, ChooseNextAction, SetState, ModelState and the two
# statics, src/game/people/Entertainer.cpp), CStaff::~CStaff and the six CWorld::Unregister overloads (src/game/World.cpp). Every callee outside them (the CStaff and
# CPeep members, CWorld::Register, the map's effectors, the script and sound calls, the string table, the serialise instances) is a recording fake in
# tests/unit_Entertainer.cpp, which also defines the globals of other files.
set(TPW_UNIT_Entertainer src/game/people/Entertainer.cpp src/game/World.cpp platform/arena.cpp)
set(TPW_UNIT_Entertainer_KEEP
  __ZN12CEntertainer10ModelStateEv   # CEntertainer::ModelState()
  __ZN12CEntertainer10UpdateTurnEv   # CEntertainer::UpdateTurn()
  __ZN12CEntertainer12FinalDestroyEv   # CEntertainer::FinalDestroy()
  __ZN12CEntertainer14ReceiveMessageERK4CMsg   # CEntertainer::ReceiveMessage(CMsg const&)
  __ZN12CEntertainer16ChooseNextActionEv   # CEntertainer::ChooseNextAction()
  __ZN12CEntertainer4InitEh   # CEntertainer::Init(unsigned char)
  __ZN12CEntertainer8SetStateEN6CStaff10StaffStateE   # CEntertainer::SetState(CStaff::StaffState)
  __ZN12CEntertainer9SerialiseEPviij   # CEntertainer::Serialise(void*, int, int, unsigned int)
  __ZN12CEntertainerC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CEntertainer::CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN12CEntertainerC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEihi   # CEntertainer::CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, unsigned char, int)
  __ZN6CStaffD1Ev   # CStaff::~CStaff()
  __ZN6CWorld10UnregisterEP11CResearcher   # CWorld::Unregister(CResearcher*)
  __ZN6CWorld10UnregisterEP12CEntertainer   # CWorld::Unregister(CEntertainer*)
  __ZN6CWorld10UnregisterEP6CGuard   # CWorld::Unregister(CGuard*)
  __ZN6CWorld10UnregisterEP7CObject   # CWorld::Unregister(CObject*)
  __ZN6CWorld10UnregisterEP9CHandyman   # CWorld::Unregister(CHandyman*)
  __ZN6CWorld10UnregisterEP9CMechanic   # CWorld::Unregister(CMechanic*)
  _gEntertainerCostumeCounter
  _gEntertainerCostumeGuard
  _gEntertainerSinitHigh
  _gEntertainerSinitLow
)
