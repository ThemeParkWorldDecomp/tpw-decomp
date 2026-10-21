# Unit w180: the golden ticket control (CGoldenTicketControl: HowManyTickets, SpendTicketOn, HasBeenBought, CanIAfford, CheckForLocal, CheckForGlobal,
# CheckForSecret, the three EarnedGoldenTicket and TellIfFirst; src/game/park/GoldenTickets.cpp) and CMsgAdvisor (constructor, GetType, destructor; src/game/park/GoldenTickets.cpp, src/game/ui/MessageCentre.cpp).
# Every callee outside them (the master save, the ride database, the park analyser, the bank account, the park time, the request constructors, the
# message centre, the three TellTheWorld, CTRACE) is a recording fake in tests/unit_GoldenTicket.cpp, which also defines the globals of other files.
set(TPW_UNIT_GoldenTicket src/game/park/GoldenTickets.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_GoldenTicket_KEEP
  __ZN11CMsgAdvisorC1ERK15CAdvisorRequest   # CMsgAdvisor::CMsgAdvisor(CAdvisorRequest const&)
  __ZN11CMsgAdvisorD1Ev   # CMsgAdvisor::~CMsgAdvisor()
  __ZN20CGoldenTicketControl11TellIfFirstEv   # CGoldenTicketControl::TellIfFirst()
  __ZN20CGoldenTicketControl13CheckForLocalEv   # CGoldenTicketControl::CheckForLocal()
  __ZN20CGoldenTicketControl13SpendTicketOnE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CGoldenTicketControl::SpendTicketOn(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN20CGoldenTicketControl14CheckForGlobalEv   # CGoldenTicketControl::CheckForGlobal()
  __ZN20CGoldenTicketControl14CheckForSecretEv   # CGoldenTicketControl::CheckForSecret()
  __ZN20CGoldenTicketControl18EarnedGoldenTicketE21GoldenTicketTypeLocal   # CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeLocal)
  __ZN20CGoldenTicketControl18EarnedGoldenTicketE22GoldenTicketTypeGlobali   # CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeGlobal, int)
  __ZN20CGoldenTicketControl18EarnedGoldenTicketE22GoldenTicketTypeSecret   # CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeSecret)
  __ZNK11CMsgAdvisor7GetTypeEv   # CMsgAdvisor::GetType() const
  __ZNK20CGoldenTicketControl10CanIAffordE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CGoldenTicketControl::CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK20CGoldenTicketControl13HasBeenBoughtE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK20CGoldenTicketControl14HowManyTicketsEv   # CGoldenTicketControl::HowManyTickets() const
)
