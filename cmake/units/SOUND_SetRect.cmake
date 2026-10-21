# Unit v5294: SOUND_SetRect (src/game/Sound.cpp). The audio object is a recording fake in
# tests/unit_SOUND_SetRect.cpp, which also defines the sound globals of other files.
set(TPW_UNIT_SOUND_SetRect src/game/Sound.cpp platform/arena.cpp)
set(TPW_UNIT_SOUND_SetRect_KEEP
  __Z13SOUND_SetRectiiiii   # SOUND_SetRect(int, int, int, int, int)
)
