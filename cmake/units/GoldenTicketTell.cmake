# Unit w183: the three CGoldenTicketControl::TellTheWorld overloads (src/GoldenTicket_tell.cpp) with CMsgAdvisor (src/MsgAdvisor.cpp, real).
# Every callee outside them (TellIfFirst, CTRACE, the advisor request's two constructors, the message centre's PostMessage, the nothrow operator new
# and the subscription header constructor) is a recording fake in tests/unit_GoldenTicketTell.cpp.
set(TPW_UNIT_GoldenTicketTell src/GoldenTicket_tell.cpp src/MsgAdvisor.cpp platform/arena.cpp)
