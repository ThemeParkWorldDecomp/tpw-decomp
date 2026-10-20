# The park analyser's visitor counts and opinions (unit w154, main app): CParkAnalyser::OfferOpinionOn, IveMisbehaved, PersonLeftPark, GetHappinessLevel (and the free
# shim CParkAnalyser_GetHappinessLevel), GetVisitorsInPark, the NumberOf... counts, the two percentages, the opinion readers and GetAverageHappinessAroundPoint.
# Every callee is a fake in the unit file (the asserts, the world accessor and its used-thing walk, CPeep::IsInsidePark, GatesOpen, the analyser value, the
# histories, the map, GetTicketBoothPos and CParkTime::GetFunnyTimeForGameTurns).
set(TPW_UNIT_ParkVisitors src/ParkAnalyser_visitors.cpp)
