# Unit v5728: StopTrainSounds and RestoreCoasterSounds (src/game/rides/CoasterSounds.cpp). The callees outside the unit are recording fakes in
# tests/unit_StopTrainSounds.cpp, which also defines the globals of other files.
set(TPW_UNIT_StopTrainSounds src/game/rides/CoasterSounds.cpp platform/arena.cpp)
set(TPW_UNIT_StopTrainSounds_KEEP
  __Z15StopTrainSoundsP20tag_sCoasterInstanceP14sTrainInstance   # StopTrainSounds(tag_sCoasterInstance*, sTrainInstance*)
  __Z20RestoreCoasterSoundsP20tag_sCoasterInstance   # RestoreCoasterSounds(tag_sCoasterInstance*)
)
