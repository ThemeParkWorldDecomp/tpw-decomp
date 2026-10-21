# The macro AI (unit w129): CMacroAI::Init and Update, CArrival::Init, Update and UpdateArrivalRate, CParkTime::Init, Update and
# GetFunnyTimeSinceSessionStart, CStaffDB::Init and Update, CGoldenTicketControl::CheckForTickets, CGame::Iterate, CControlManager::SetupInitialControls,
# CWorldSavvy::GetWorld, CParkAnalyser::GatesOpen and the nine companions (the constructor, GetType and destructor of CMsgEndOfDay, CMsgEndOfMonth and
# CMsgEndOfYear). Every callee outside them (the funny time, the time stamp operators, the message centre, the UI, the staff database's helpers, the
# arrivals' people, the world's dice and IsParkClosed, the game type, the golden ticket checks, the frame step and the control check) is a fake in the unit
# file; gMainBalance is storage defined there (its table methods are fakes).
set(TPW_UNIT_MacroAI src/game/park/Map.cpp src/game/people/Arrival.cpp src/game/people/ParkTime.cpp
    src/game/people/StaffDB.cpp src/game/park/GoldenTickets.cpp src/game/Game.cpp src/game/ControlManager.cpp
    src/game/ui/ui_visitorStatus.cpp src/game/park/analyser.cpp src/game/people/Peep.cpp src/game/ui/MessageCentre.cpp)
set(TPW_UNIT_MacroAI_KEEP
  __ZN12CMsgEndOfDayC1Ev   # CMsgEndOfDay::CMsgEndOfDay()
  __ZN12CMsgEndOfDayD1Ev   # CMsgEndOfDay::~CMsgEndOfDay()
  __ZN13CMsgEndOfYearC1Ev   # CMsgEndOfYear::CMsgEndOfYear()
  __ZN13CMsgEndOfYearD1Ev   # CMsgEndOfYear::~CMsgEndOfYear()
  __ZN14CMsgEndOfMonthC1Ev   # CMsgEndOfMonth::CMsgEndOfMonth()
  __ZN14CMsgEndOfMonthD1Ev   # CMsgEndOfMonth::~CMsgEndOfMonth()
  __ZN15CControlManager20SetupInitialControlsEv   # CControlManager::SetupInitialControls()
  __ZN20CGoldenTicketControl15CheckForTicketsEv   # CGoldenTicketControl::CheckForTickets()
  __ZN5CGame7IterateEv   # CGame::Iterate()
  __ZN8CArrival17UpdateArrivalRateEv   # CArrival::UpdateArrivalRate()
  __ZN8CArrival4InitEv   # CArrival::Init()
  __ZN8CArrival6UpdateEv   # CArrival::Update()
  __ZN8CMacroAI4InitEv   # CMacroAI::Init()
  __ZN8CMacroAI6UpdateEv   # CMacroAI::Update()
  __ZN8CStaffDB4InitEv   # CStaffDB::Init()
  __ZN8CStaffDB6UpdateEv   # CStaffDB::Update()
  __ZN9CParkTime4InitEv   # CParkTime::Init()
  __ZN9CParkTime6UpdateEv   # CParkTime::Update()
  __ZNK11CWorldSavvy8GetWorldEv   # CWorldSavvy::GetWorld() const
  __ZNK12CMsgEndOfDay7GetTypeEv   # CMsgEndOfDay::GetType() const
  __ZNK13CMsgEndOfYear7GetTypeEv   # CMsgEndOfYear::GetType() const
  __ZNK13CParkAnalyser9GatesOpenEv   # CParkAnalyser::GatesOpen() const
  __ZNK14CMsgEndOfMonth7GetTypeEv   # CMsgEndOfMonth::GetType() const
  __ZNK9CParkTime29GetFunnyTimeSinceSessionStartEv   # CParkTime::GetFunnyTimeSinceSessionStart() const
  _gGameIterateFlag
  _gParkDate
)
