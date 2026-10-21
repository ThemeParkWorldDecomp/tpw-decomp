# Unit v5960: LOADVARSTRUCT<CThoughts::EventHistType>, CTagTable::GetElement, CTagSystem::ReceiveMessage
# (src/game/Thing.cpp, src/game/buildings/TrainingCentre.cpp). The callees outside the unit are recording fakes in
# tests/unit_LOADVARSTRUCT_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_LOADVARSTRUCT_3 src/game/Thing.cpp src/game/buildings/TrainingCentre.cpp platform/arena.cpp)
set(TPW_UNIT_LOADVARSTRUCT_3_KEEP
  __Z13LOADVARSTRUCTIN9CThoughts13EventHistTypeEEiPvPT_PKci   # int LOADVARSTRUCT<CThoughts::EventHistType>(void*, CThoughts::EventHistType*, char const*, int)
  __ZN10CTagSystem14ReceiveMessageERK4CMsg   # CTagSystem::ReceiveMessage(CMsg const&)
  __ZNK9CTagTable10GetElementEi   # CTagTable::GetElement(int) const
  _gTagTable
  _gTbErrorZero
)
