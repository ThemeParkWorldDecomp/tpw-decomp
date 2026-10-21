# The park analyser's visitor counts and opinions (unit w154, main app): CParkAnalyser::OfferOpinionOn, IveMisbehaved, PersonLeftPark, GetHappinessLevel (and the free
# shim CParkAnalyser_GetHappinessLevel), GetVisitorsInPark, the NumberOf... counts, the two percentages, the opinion readers and GetAverageHappinessAroundPoint.
# Every callee is a fake in the unit file (the asserts, the world accessor and its used-thing walk, CPeep::IsInsidePark, GatesOpen, the analyser value, the
# histories, the map, GetTicketBoothPos and CParkTime::GetFunnyTimeForGameTurns).
set(TPW_UNIT_ParkVisitors src/game/park/analyser.cpp)
set(TPW_UNIT_ParkVisitors_KEEP
  __Z31CParkAnalyser_GetHappinessLevelP13CParkAnalyseri   # CParkAnalyser_GetHappinessLevel(CParkAnalyser*, int)
  __ZN13CParkAnalyser13IveMisbehavedEi   # CParkAnalyser::IveMisbehaved(int)
  __ZN13CParkAnalyser14OfferOpinionOnEih   # CParkAnalyser::OfferOpinionOn(int, unsigned char)
  __ZN13CParkAnalyser14PersonLeftParkEi   # CParkAnalyser::PersonLeftPark(int)
  __ZNK13CParkAnalyser17GetHappinessLevelEj   # CParkAnalyser::GetHappinessLevel(unsigned int) const
  __ZNK13CParkAnalyser17GetVisitorsInParkEj   # CParkAnalyser::GetVisitorsInPark(unsigned int) const
  __ZNK13CParkAnalyser19GetAverageOpinionOnEi   # CParkAnalyser::GetAverageOpinionOn(int) const
  __ZNK13CParkAnalyser22NumberOfHungryVisitorsEh   # CParkAnalyser::NumberOfHungryVisitors(unsigned char) const
  __ZNK13CParkAnalyser23NumberOfThirstyVisitorsEh   # CParkAnalyser::NumberOfThirstyVisitors(unsigned char) const
  __ZNK13CParkAnalyser25HaveCompleteSetOfOpinionsEi   # CParkAnalyser::HaveCompleteSetOfOpinions(int) const
  __ZNK13CParkAnalyser27NumberOfVisitorsOutsideParkEv   # CParkAnalyser::NumberOfVisitorsOutsidePark() const
  __ZNK13CParkAnalyser28MisbehavingVisitorsLastMonthEb   # CParkAnalyser::MisbehavingVisitorsLastMonth(bool) const
  __ZNK13CParkAnalyser28NumberOfVisitorsInLastMonthsEi   # CParkAnalyser::NumberOfVisitorsInLastMonths(int) const
  __ZNK13CParkAnalyser28NumberOfVisitorsWithBalloonsEv   # CParkAnalyser::NumberOfVisitorsWithBalloons() const
  __ZNK13CParkAnalyser28NumberOfVisitorsWithCostumesEv   # CParkAnalyser::NumberOfVisitorsWithCostumes() const
  __ZNK13CParkAnalyser29NumberOfVisitorsNeedingToiletEh   # CParkAnalyser::NumberOfVisitorsNeedingToilet(unsigned char) const
  __ZNK13CParkAnalyser30GetAverageHappinessAroundPointE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEj   # CParkAnalyser::GetAverageHappinessAroundPoint(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, unsigned int) const
  __ZNK13CParkAnalyser30NumberOfVisitorsSinceBeginningEv   # CParkAnalyser::NumberOfVisitorsSinceBeginning() const
  __ZNK13CParkAnalyser37PercentageOfVisitorsWithHappinessOverEi   # CParkAnalyser::PercentageOfVisitorsWithHappinessOver(int) const
  __ZNK13CParkAnalyser38PercentageOfVisitorsWithHappinessUnderEi   # CParkAnalyser::PercentageOfVisitorsWithHappinessUnder(int) const
)
