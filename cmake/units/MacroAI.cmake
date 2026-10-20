# The macro AI (unit w129): CMacroAI::Init and Update, CArrival::Init, Update and UpdateArrivalRate, CParkTime::Init, Update and
# GetFunnyTimeSinceSessionStart, CStaffDB::Init and Update, CGoldenTicketControl::CheckForTickets, CGame::Iterate, CControlManager::SetupInitialControls,
# CWorldSavvy::GetWorld, CParkAnalyser::GatesOpen and the nine companions (the constructor, GetType and destructor of CMsgEndOfDay, CMsgEndOfMonth and
# CMsgEndOfYear). Every callee outside them (the funny time, the time stamp operators, the message centre, the UI, the staff database's helpers, the
# arrivals' people, the world's dice and IsParkClosed, the game type, the golden ticket checks, the frame step and the control check) is a fake in the unit
# file; gMainBalance is storage defined there (its table methods are fakes).
set(TPW_UNIT_MacroAI src/MacroAI.cpp src/Arrival_update.cpp src/ParkTime_update.cpp src/StaffDB_update.cpp src/GoldenTicket_check.cpp src/Game_iterate.cpp src/ControlManager_setup.cpp src/WorldSavvy.cpp src/ParkAnalyser_gates.cpp src/DateMessages.cpp)
