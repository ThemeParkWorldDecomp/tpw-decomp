# Unit v5026: dragging a pylon of the track editor (StartMovePylon, DoMovePylon; src/game/rides/CoasterEdit.cpp). The callees outside the unit are
# recording fakes in tests/unit_DoMovePylon.cpp, which also defines the globals of other files.
set(TPW_UNIT_DoMovePylon src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_DoMovePylon_KEEP
  __Z11DoMovePylonjj   # DoMovePylon(unsigned int, unsigned int)
  __Z14StartMovePylonP15tag_sTrackBrace   # StartMovePylon(tag_sTrackBrace*)
)
