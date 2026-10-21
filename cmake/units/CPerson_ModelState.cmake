# Unit v5543: CPerson::ModelState, OnAdmittance, GetDebagged, MakeMajorDecision (src/CPerson_..._CPerson_ModelState.cpp). The callees outside the unit are
# recording fakes in tests/unit_CPerson_ModelState.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_ModelState src/game/people/Person.cpp src/game/people/PersonQueue.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_ModelState_KEEP
  __ZN7CPerson10ModelStateEv   # CPerson::ModelState()
  __ZN7CPerson11GetDebaggedE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CPerson::GetDebagged(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN7CPerson12OnAdmittanceEv   # CPerson::OnAdmittance()
  __ZN7CPerson17MakeMajorDecisionEv   # CPerson::MakeMajorDecision()
  _gMajorListEnd
  _gMajorNoQueue
  _gMajorNoRide
  _gModelListEnd
)
