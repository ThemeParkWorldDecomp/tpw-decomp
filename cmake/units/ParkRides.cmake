# The analyser's ride superlatives (unit w153, main app): CParkAnalyser::GetMostExcitingGokart, GetGokartWithMostCrossRoads, GetLongestTrackRide (both),
# GetCoasterWithMostLoops, GetHighestCoaster, GetAverageToiletQueueLenght, FindRideWithUnconnectedQueue, GetAverageExcitementLevelOfAllRides, GetScariestRide and
# GetRidesInPark. Every callee is a fake in the unit file (the asserts, the world accessor and its used-thing walk, the ride database, the CObject queries, the
# track and coaster statistics, the cell system's current object and the analyser value's Get and Set).
set(TPW_UNIT_ParkRides src/ParkAnalyser_rides.cpp)
