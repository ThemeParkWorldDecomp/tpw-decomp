# Unit v5133: COnlinePerson's UpdateTurn, ModelState, IsThisMyName, SetDest x2, Teleport, ProcessWalkingToDest (src/game/online/OnlinePerson.cpp). The callees
# outside the unit are recording fakes in tests/unit_COnlinePerson_SetDest.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlinePerson_SetDest src/game/online/OnlinePerson.cpp platform/arena.cpp)
set(TPW_UNIT_COnlinePerson_SetDest_KEEP
  __ZN13COnlinePerson10ModelStateEv   # COnlinePerson::ModelState()
  __ZN13COnlinePerson10UpdateTurnEv   # COnlinePerson::UpdateTurn()
  __ZN13COnlinePerson20ProcessWalkingToDestEv   # COnlinePerson::ProcessWalkingToDest()
  __ZN13COnlinePerson7SetDestE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # COnlinePerson::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN13COnlinePerson7SetDestE6MapPosS0_   # COnlinePerson::SetDest(MapPos, MapPos)
  __ZN13COnlinePerson8TeleportE6MapPosS0_   # COnlinePerson::Teleport(MapPos, MapPos)
  __ZNK13COnlinePerson12IsThisMyNameERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlinePerson::IsThisMyName(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&) const
)
