# The frame readers of the things (unit w125, main app): CParkAnalyser::HowBusyIsPark, NumberOfVisitorsInPark, CountNeighboursOfType and the two text objects'
# Change (InterfaceNumberTextObject, InterfaceCashAndGTickTextObject). Every callee is a fake in the unit file (the asserts, the world accessor and its
# used-thing walk, CPeep::IsInsidePark, the analyser value's Get and Set, GetVisitorsInPark, the sprite's Change, swprintf and the string table's FormatString).
set(TPW_UNIT_ParkReaders src/ParkAnalyser_readers.cpp src/ui_TextObjects.cpp)
