# Unit v5121: COnlineControl's command senders and avatar-table accessors (src/COnlineControl_..._EarmuffsOff.cpp). The callees outside the unit
# are recording fakes in tests/unit_COnlineControl_EarmuffsOff.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_EarmuffsOff src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_EarmuffsOff_KEEP
  __Z26OnlineControlTeleportThing12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE6MapPosS2_   # OnlineControlTeleportThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, MapPos, MapPos)
  __ZN14COnlineControl10EarmuffsOnEv   # COnlineControl::EarmuffsOn()
  __ZN14COnlineControl11EarmuffsOffEv   # COnlineControl::EarmuffsOff()
  __ZN14COnlineControl14SetDestinationE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE6MapPosS3_   # COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, MapPos, MapPos)
  __ZN14COnlineControl14SetDestinationE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I22IDXTYPEUniqueNameMapIdtE   # COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN14COnlineControl15VoteForThisParkEv   # COnlineControl::VoteForThisPark()
  __ZN14COnlineControl8TeleportE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE6MapPosS3_   # COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, MapPos, MapPos)
  __ZN14COnlineControl8TeleportE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I22IDXTYPEUniqueNameMapIdtE   # COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN14COnlineControl9GetPersonERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::GetPerson(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  _gNullThingId
)
