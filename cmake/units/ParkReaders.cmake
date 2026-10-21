# The frame readers of the things (unit w125, main app): CParkAnalyser::HowBusyIsPark, NumberOfVisitorsInPark, CountNeighboursOfType and the two text objects'
# Change (InterfaceNumberTextObject, InterfaceCashAndGTickTextObject). Every callee is a fake in the unit file (the asserts, the world accessor and its
# used-thing walk, CPeep::IsInsidePark, the analyser value's Get and Set, GetVisitorsInPark, the sprite's Change, swprintf and the string table's FormatString).
set(TPW_UNIT_ParkReaders src/game/park/analyser.cpp src/game/ui/InterfaceTextObjectSprite.cpp)
set(TPW_UNIT_ParkReaders_KEEP
  __ZN13CParkAnalyser21CountNeighboursOfTypeEN6CThing10ThingModelE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEii   # CParkAnalyser::CountNeighboursOfType(CThing::ThingModel, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int, int)
  __ZN25InterfaceNumberTextObject6ChangeEjP8DOChange   # InterfaceNumberTextObject::Change(unsigned int, DOChange*)
  __ZN31InterfaceCashAndGTickTextObject6ChangeEjP8DOChange   # InterfaceCashAndGTickTextObject::Change(unsigned int, DOChange*)
  __ZNK13CParkAnalyser13HowBusyIsParkEv   # CParkAnalyser::HowBusyIsPark() const
  __ZNK13CParkAnalyser22NumberOfVisitorsInParkEv   # CParkAnalyser::NumberOfVisitorsInPark() const
)
