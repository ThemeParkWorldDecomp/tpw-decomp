# Unit v5024: CResearchLab::ReceiveMessage and CResearchLab::Serialise (src/game/buildings/ResearchLab.cpp). The callees
# outside the unit are recording fakes in tests/unit_CResearchLab_ReceiveMessage.cpp, which also defines the globals of other files.
set(TPW_UNIT_CResearchLab_ReceiveMessage src/game/buildings/ResearchLab.cpp platform/arena.cpp)
set(TPW_UNIT_CResearchLab_ReceiveMessage_KEEP
  __ZN12CResearchLab14ReceiveMessageERK4CMsg   # CResearchLab::ReceiveMessage(CMsg const&)
  __ZN12CResearchLab9SerialiseEPviij   # CResearchLab::Serialise(void*, int, int, unsigned int)
)
