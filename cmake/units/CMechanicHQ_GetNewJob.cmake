# Unit v5889: the mechanic HQ (CMechanicHQ::GetNewJob, Serialise, ReassignJobsAfterDrop, ReceiveMessage; src/game/buildings/MechanicHQ.cpp). The callees
# outside the unit are recording fakes in tests/unit_CMechanicHQ_GetNewJob.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMechanicHQ_GetNewJob src/game/buildings/MechanicHQ.cpp platform/arena.cpp)
set(TPW_UNIT_CMechanicHQ_GetNewJob_KEEP
  __ZN11CMechanicHQ14ReceiveMessageERK4CMsg   # CMechanicHQ::ReceiveMessage(CMsg const&)
  __ZN11CMechanicHQ21ReassignJobsAfterDropER9CMechanic12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMechanicHQ::ReassignJobsAfterDrop(CMechanic&, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN11CMechanicHQ9GetNewJobE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMechanicHQ::GetNewJob(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN11CMechanicHQ9SerialiseEPviij   # CMechanicHQ::Serialise(void*, int, int, unsigned int)
  _gMechHQId102a0b9a
  _gMechHQId102a0b9c
  _gMechHQId102a0b9e
  _gMechHQId102a0ba0
  _gMechHQId102a0ba2
  _gMechHQId102a0ba4
  _gMechHQId102a0ba6
  _gMechHQId102a0ba8
  _gMechHQId102a0baa
  _gMechHQId102a0bac
  _gMechHQId102a0bae
)
