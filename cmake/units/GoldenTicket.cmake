# Unit w180: the golden ticket control (CGoldenTicketControl: HowManyTickets, SpendTicketOn, HasBeenBought, CanIAfford, CheckForLocal, CheckForGlobal,
# CheckForSecret, the three EarnedGoldenTicket and TellIfFirst; src/GoldenTicket.cpp) and CMsgAdvisor (constructor, GetType, destructor; src/MsgAdvisor.cpp).
# Every callee outside them (the master save, the ride database, the park analyser, the bank account, the park time, the request constructors, the
# message centre, the three TellTheWorld, CTRACE) is a recording fake in tests/unit_GoldenTicket.cpp, which also defines the globals of other files.
set(TPW_UNIT_GoldenTicket src/GoldenTicket.cpp src/MsgAdvisor.cpp platform/arena.cpp)
