# Unit v5067: SAVEVARSTRUCT<CThoughts::EventHistType> (src/game/buildings/TrainingCentre.cpp). The callees are recording fakes in
# tests/unit_SAVEVARSTRUCT.cpp, which also defines gSaveLoad.
set(TPW_UNIT_SAVEVARSTRUCT src/game/buildings/TrainingCentre.cpp platform/arena.cpp)
set(TPW_UNIT_SAVEVARSTRUCT_KEEP
  __Z13SAVEVARSTRUCTIN9CThoughts13EventHistTypeEEiPvPT_PKci   # int SAVEVARSTRUCT<CThoughts::EventHistType>(void*, CThoughts::EventHistType*, char const*, int)
)
