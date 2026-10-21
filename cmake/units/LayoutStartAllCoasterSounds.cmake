# Unit v5205: StartTrainSounds and LayoutStartAllCoasterSounds (src/game/rides/CoasterSounds.cpp). The callees are recording fakes in
# tests/unit_LayoutStartAllCoasterSounds.cpp, which also defines the globals of other files.
set(TPW_UNIT_LayoutStartAllCoasterSounds src/game/rides/CoasterSounds.cpp platform/arena.cpp)
set(TPW_UNIT_LayoutStartAllCoasterSounds_KEEP
  __Z16StartTrainSoundsP20tag_sCoasterInstanceP14sTrainInstancei   # StartTrainSounds(tag_sCoasterInstance*, sTrainInstance*, int)
  __Z27LayoutStartAllCoasterSoundsv   # LayoutStartAllCoasterSounds()
)
