# The park analyser's object and ride type queries (unit w151, main app): 26 CParkAnalyser queries (src/game/park/analyser.cpp) and CObject::IsClosed and
# IsBrokenDown (src/game/Object.cpp). Every callee is a fake in the unit file (the world accessor and its used-thing walk, the asserts, the analyser value's
# Get and Set, the ride database's walk and GetRideInfo, the const object control lookup, the research lab, the history's GetValue and SetAtEnd).
set(TPW_UNIT_ParkObjects src/game/park/analyser.cpp src/game/Object.cpp)
set(TPW_UNIT_ParkObjects_KEEP
  __ZN13CParkAnalyser29SetLastOfObjectTypeResearchedE9UIObjType12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::SetLastOfObjectTypeResearched(UIObjType, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZNK13CParkAnalyser15FilthiestToiletERi   # CParkAnalyser::FilthiestToilet(int&) const
  __ZNK13CParkAnalyser20NumberOfObjectInParkE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser21NumberOfToiletsInParkEv   # CParkAnalyser::NumberOfToiletsInPark() const
  __ZNK13CParkAnalyser21TypesOfRidesAvailableEb   # CParkAnalyser::TypesOfRidesAvailable(bool) const
  __ZNK13CParkAnalyser21TypesOfShopsAvailableEb   # CParkAnalyser::TypesOfShopsAvailable(bool) const
  __ZNK13CParkAnalyser22AvgFilthLevelOfToiletsEv   # CParkAnalyser::AvgFilthLevelOfToilets() const
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
