set(TPW_UNIT_CTrainingCentre_Serialise src/game/buildings/TrainingCentre.cpp platform/arena.cpp)
set(TPW_UNIT_CTrainingCentre_Serialise_KEEP
  __ZN15CTrainingCentre13UpdateMonthlyEv   # CTrainingCentre::UpdateMonthly()
  __ZN15CTrainingCentre9SerialiseEPviij   # CTrainingCentre::Serialise(void*, int, int, unsigned int)
  _gTrainingCounts
  _gTrainingEnd1
  _gTrainingEnd2
)
