# The park rating, the park value and the attraction points (unit w159, main app): CParkAnalyser::GetOverallParkRating, GetParkValue and
# GetTotalParkAttractionPoints (src/game/park/analyser.cpp), with the real readers (NumberOfVisitorsInPark), the object queries (NumberOfObjectTypeInPark and
# NumberOfObjectInPark) and Things2_small's NumberOfStaffType. Every other callee is a recording fake in tests/unit_ParkRating.cpp, which includes calllog.cpp
# itself (the cell walk and the long used-thing lists log many events); what the real sources need and the test never calls is a link-only fake there.
set(TPW_UNIT_ParkRating src/game/park/analyser.cpp src/game/Object.cpp src/game/advisor/Advisor.cpp
    src/game/park/BankAccount.cpp src/game/park/Weather.cpp)
set(TPW_UNIT_ParkRating_KEEP
  __ZN12CBankAccount14MakeWithdrawalEj   # CBankAccount::MakeWithdrawal(unsigned int)
  __ZN13CParkAnalyser14BuiltSomethingEv   # CParkAnalyser::BuiltSomething()
  __ZN13CParkAnalyser21CountNeighboursOfTypeEN6CThing10ThingModelE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEii   # CParkAnalyser::CountNeighboursOfType(CThing::ThingModel, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, int)
  __ZN13CParkAnalyser28GetTotalParkAttractionPointsEv   # CParkAnalyser::GetTotalParkAttractionPoints()
  __ZN13CParkAnalyser29SetLastOfObjectTypeResearchedE9UIObjType12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::SetLastOfObjectTypeResearched(UIObjType, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN8CAdvisor29FindHighestScoringSlotInQueueEv   # CAdvisor::FindHighestScoringSlotInQueue()
  __ZN8CAdvisor9IsPlayingEv   # CAdvisor::IsPlaying()
  __ZN8CWeather20SetPrecipitationTypeE17PrecipitationType   # CWeather::SetPrecipitationType(PrecipitationType)
  __ZNK13CParkAnalyser12GetParkValueEj   # CParkAnalyser::GetParkValue(unsigned int) const
  __ZNK13CParkAnalyser13HowBusyIsParkEv   # CParkAnalyser::HowBusyIsPark() const
  __ZNK13CParkAnalyser15FilthiestToiletERi   # CParkAnalyser::FilthiestToilet(int&) const
  __ZNK13CParkAnalyser17NumberOfStaffTypeEN6CThing10ThingModelE   # CParkAnalyser::NumberOfStaffType(CThing::ThingModel) const
  __ZNK13CParkAnalyser20GetOverallParkRatingEv   # CParkAnalyser::GetOverallParkRating() const
  __ZNK13CParkAnalyser20NumberOfObjectInParkE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser21NumberOfToiletsInParkEv   # CParkAnalyser::NumberOfToiletsInPark() const
  __ZNK13CParkAnalyser21TypesOfRidesAvailableEb   # CParkAnalyser::TypesOfRidesAvailable(bool) const
  __ZNK13CParkAnalyser21TypesOfShopsAvailableEb   # CParkAnalyser::TypesOfShopsAvailable(bool) const
  __ZNK13CParkAnalyser22AvgFilthLevelOfToiletsEv   # CParkAnalyser::AvgFilthLevelOfToilets() const
  __ZNK13CParkAnalyser22NumberOfVisitorsInParkEv   # CParkAnalyser::NumberOfVisitorsInPark() const
  __ZNK13CParkAnalyser23NumberOfWornRidesInParkEi   # CParkAnalyser::NumberOfWornRidesInPark(int) const
  __ZNK13CParkAnalyser24NumberOfObjectTypeInParkE9UIObjTypei   # CParkAnalyser::NumberOfObjectTypeInPark(UIObjType, int) const
  __ZNK13CParkAnalyser24NumberOfStaffRoomsInParkEv   # CParkAnalyser::NumberOfStaffRoomsInPark() const
  __ZNK13CParkAnalyser24TypesOfFeaturesAvailableEb   # CParkAnalyser::TypesOfFeaturesAvailable(bool) const
  __ZNK13CParkAnalyser24TypesOfUpgradesAvailableEb   # CParkAnalyser::TypesOfUpgradesAvailable(bool) const
  __ZNK13CParkAnalyser25TypesOfSideshowsAvailableEb   # CParkAnalyser::TypesOfSideshowsAvailable(bool) const
  __ZNK13CParkAnalyser26LastOfObjectTypeResearchedE9UIObjTypei   # CParkAnalyser::LastOfObjectTypeResearched(UIObjType, int) const
  __ZNK13CParkAnalyser26TypesOfRidesLeftToResearchEv   # CParkAnalyser::TypesOfRidesLeftToResearch() const
  __ZNK13CParkAnalyser26TypesOfShopsLeftToResearchEv   # CParkAnalyser::TypesOfShopsLeftToResearch() const
  __ZNK13CParkAnalyser27NumberOfClosedToiletsInParkEv   # CParkAnalyser::NumberOfClosedToiletsInPark() const
  __ZNK13CParkAnalyser29NumberOfBrokenDownRidesInParkEv   # CParkAnalyser::NumberOfBrokenDownRidesInPark() const
  __ZNK13CParkAnalyser29TypesOfFeaturesLeftToResearchEv   # CParkAnalyser::TypesOfFeaturesLeftToResearch() const
  __ZNK13CParkAnalyser30AllObjectsResearchedAndPresentEv   # CParkAnalyser::AllObjectsResearchedAndPresent() const
  __ZNK13CParkAnalyser30NumberOfClosedObjectTypeInParkE9UIObjType   # CParkAnalyser::NumberOfClosedObjectTypeInPark(UIObjType) const
  __ZNK13CParkAnalyser30NumberOfClosedStaffRoomsInParkEv   # CParkAnalyser::NumberOfClosedStaffRoomsInPark() const
  __ZNK13CParkAnalyser30PercentageOfTotalObjectsInParkEv   # CParkAnalyser::PercentageOfTotalObjectsInPark() const
  __ZNK13CParkAnalyser30TypesOfSideshowsLeftToResearchEv   # CParkAnalyser::TypesOfSideshowsLeftToResearch() const
  __ZNK13CParkAnalyser31GetHighestUpgradeLevelForObjectE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::GetHighestUpgradeLevelForObject(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser33TypesOfParticularObjectTypeInParkE9UIObjTypeb   # CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType, bool) const
  __ZNK7CObject12IsBrokenDownEv   # CObject::IsBrokenDown() const
  __ZNK7CObject8IsClosedEv   # CObject::IsClosed() const
)
set(TPW_UNIT_ParkRating_OWN_CALLLOG ON)
