# Unit w188: CResearcher (both constructors, Init, Serialise, FinalDestroy, ReceiveMessage, UpdateTurn, ModelState, ChooseNextAction, the three Process states,
# DoSomeResearch and the static initialiser) and CRegionEffectCell's operator== and Serialise, src/game/people/Researcher.cpp. Every callee outside them (the CStaff and CPeep members,
# CWorld::Register and Unregister, the research lab, the script and sound calls, the string table, the serialise instances, memcmp, the trace and the assertion) is a
# recording fake in tests/unit_Researcher.cpp, which also defines the globals of other files.
set(TPW_UNIT_Researcher src/game/people/Researcher.cpp platform/arena.cpp)
set(TPW_UNIT_Researcher_KEEP
  __ZN11CResearcher10ModelStateEv   # CResearcher::ModelState()
  __ZN11CResearcher10UpdateTurnEv   # CResearcher::UpdateTurn()
  __ZN11CResearcher11ProcessIdleEv   # CResearcher::ProcessIdle()
  __ZN11CResearcher12FinalDestroyEv   # CResearcher::FinalDestroy()
  __ZN11CResearcher14DoSomeResearchEv   # CResearcher::DoSomeResearch()
  __ZN11CResearcher14ReceiveMessageERK4CMsg   # CResearcher::ReceiveMessage(CMsg const&)
  __ZN11CResearcher16ChooseNextActionEv   # CResearcher::ChooseNextAction()
  __ZN11CResearcher17ProcessPatrollingEv   # CResearcher::ProcessPatrolling()
  __ZN11CResearcher18ProcessResearchingEv   # CResearcher::ProcessResearching()
  __ZN11CResearcher4InitEh   # CResearcher::Init(unsigned char)
  __ZN11CResearcher9SerialiseEPviij   # CResearcher::Serialise(void*, int, int, unsigned int)
  __ZN11CResearcherC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CResearcher::CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN11CResearcherC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEihi   # CResearcher::CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, unsigned char, int)
  __ZN17CRegionEffectCell9SerialiseEPviij   # CRegionEffectCell::Serialise(void*, int, int, unsigned int)
  __ZN17CRegionEffectCelleqERKS_   # CRegionEffectCell::operator==(CRegionEffectCell const&)
  _gResearcherSinitHigh
  _gResearcherSinitLow
)
# The test fakes memcmp (operator== is memcmp of ten bytes on the Mac): keep the compiler from expanding the call inline.
set_source_files_properties(src/game/people/Researcher.cpp PROPERTIES COMPILE_OPTIONS -fno-builtin-memcmp)
