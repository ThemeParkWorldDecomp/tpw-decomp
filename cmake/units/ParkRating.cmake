# The park rating, the park value and the attraction points (unit w159, main app): CParkAnalyser::GetOverallParkRating, GetParkValue and
# GetTotalParkAttractionPoints (src/ParkAnalyser_rating.cpp), with the real readers (NumberOfVisitorsInPark), the object queries (NumberOfObjectTypeInPark and
# NumberOfObjectInPark) and Things2_small's NumberOfStaffType. Every other callee is a recording fake in tests/unit_ParkRating.cpp, which includes calllog.cpp
# itself (the cell walk and the long used-thing lists log many events); what the real sources need and the test never calls is a link-only fake there.
set(TPW_UNIT_ParkRating src/ParkAnalyser_rating.cpp src/ParkAnalyser_readers.cpp src/ParkAnalyser_objects.cpp src/Object_queries.cpp src/Things2_small.cpp)
set(TPW_UNIT_ParkRating_OWN_CALLLOG ON)
