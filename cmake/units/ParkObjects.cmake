# The park analyser's object and ride type queries (unit w151, main app): 26 CParkAnalyser queries (src/ParkAnalyser_objects.cpp) and CObject::IsClosed and
# IsBrokenDown (src/Object_queries.cpp). Every callee is a fake in the unit file (the world accessor and its used-thing walk, the asserts, the analyser value's
# Get and Set, the ride database's walk and GetRideInfo, the const object control lookup, the research lab, the history's GetValue and SetAtEnd).
set(TPW_UNIT_ParkObjects src/ParkAnalyser_objects.cpp src/Object_queries.cpp)
