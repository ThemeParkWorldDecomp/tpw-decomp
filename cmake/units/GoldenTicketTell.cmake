# Unit w183: the three CGoldenTicketControl::TellTheWorld overloads (src/game/park/GoldenTickets.cpp) with CMsgAdvisor (src/game/park/GoldenTickets.cpp, src/game/ui/MessageCentre.cpp, real).
# Every callee outside them (TellIfFirst, CTRACE, the advisor request's two constructors, the message centre's PostMessage, the nothrow operator new
# and the subscription header constructor) is a recording fake in tests/unit_GoldenTicketTell.cpp.
set(TPW_UNIT_GoldenTicketTell src/game/park/GoldenTickets.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_GoldenTicketTell_KEEP
  __ZN11CMsgAdvisorC1ERK15CAdvisorRequest   # CMsgAdvisor::CMsgAdvisor(CAdvisorRequest const&)
  __ZN11CMsgAdvisorD1Ev   # CMsgAdvisor::~CMsgAdvisor()
  __ZN20CGoldenTicketControl12TellTheWorldE21GoldenTicketTypeLocal19GoldenTicketTypeWon   # CGoldenTicketControl::TellTheWorld(GoldenTicketTypeLocal, GoldenTicketTypeWon)
  __ZN20CGoldenTicketControl12TellTheWorldE22GoldenTicketTypeGlobal19GoldenTicketTypeWon   # CGoldenTicketControl::TellTheWorld(GoldenTicketTypeGlobal, GoldenTicketTypeWon)
  __ZN20CGoldenTicketControl12TellTheWorldE22GoldenTicketTypeSecret19GoldenTicketTypeWon   # CGoldenTicketControl::TellTheWorld(GoldenTicketTypeSecret, GoldenTicketTypeWon)
  __ZNK11CMsgAdvisor7GetTypeEv   # CMsgAdvisor::GetType() const
)
