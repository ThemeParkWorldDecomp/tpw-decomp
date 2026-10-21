# The analyser's ride superlatives (unit w153, main app): CParkAnalyser::GetMostExcitingGokart, GetGokartWithMostCrossRoads, GetLongestTrackRide (both),
# GetCoasterWithMostLoops, GetHighestCoaster, GetAverageToiletQueueLenght, FindRideWithUnconnectedQueue, GetAverageExcitementLevelOfAllRides, GetScariestRide and
# GetRidesInPark. Every callee is a fake in the unit file (the asserts, the world accessor and its used-thing walk, the ride database, the CObject queries, the
# track and coaster statistics, the cell system's current object and the analyser value's Get and Set).
set(TPW_UNIT_ParkRides src/game/park/analyser.cpp)
set(TPW_UNIT_ParkRides_KEEP
  __ZN13CParkAnalyser28FindRideWithUnconnectedQueueEb   # CParkAnalyser::FindRideWithUnconnectedQueue(bool)
  __ZNK13CParkAnalyser14GetRidesInParkEj   # CParkAnalyser::GetRidesInPark(unsigned int) const
  __ZNK13CParkAnalyser15GetScariestRideER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetScariestRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser17GetHighestCoasterER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::GetHighestCoaster(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser19GetLongestTrackRideER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::GetLongestTrackRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser19GetLongestTrackRideER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEi   # CParkAnalyser::GetLongestTrackRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&, int) const
  __ZNK13CParkAnalyser21GetMostExcitingGokartER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetMostExcitingGokart(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser23GetCoasterWithMostLoopsER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I25IDXTYPEUniqueNameObjectIdtE   # CParkAnalyser::GetCoasterWithMostLoops(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>) const
  __ZNK13CParkAnalyser27GetAverageToiletQueueLenghtEv   # CParkAnalyser::GetAverageToiletQueueLenght() const
  __ZNK13CParkAnalyser27GetGokartWithMostCrossRoadsER12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CParkAnalyser::GetGokartWithMostCrossRoads(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>&) const
  __ZNK13CParkAnalyser35GetAverageExcitementLevelOfAllRidesEj   # CParkAnalyser::GetAverageExcitementLevelOfAllRides(unsigned int) const
)
