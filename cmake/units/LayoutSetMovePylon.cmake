# Unit v5261: the layout commands of the editor's coaster (src/game/rides/CoasterEdit.cpp). The callees are recording fakes in
# tests/unit_LayoutSetMovePylon.cpp, which also defines the globals of other files.
set(TPW_UNIT_LayoutSetMovePylon src/game/rides/CoasterEdit.cpp platform/arena.cpp
    tests/standins/LayoutSetMovePylon_callees.cpp)
set(TPW_UNIT_LayoutSetMovePylon_KEEP
  __Z18LayoutSetMovePylonttt   # LayoutSetMovePylon(unsigned short, unsigned short, unsigned short)
  __Z18LayoutStackPylonUpt   # LayoutStackPylonUp(unsigned short)
  __Z20LayoutSetCoasterEditj   # LayoutSetCoasterEdit(unsigned int)
  __Z20LayoutStackPylonDownt   # LayoutStackPylonDown(unsigned short)
  __Z21LayoutRegenerateTrackj   # LayoutRegenerateTrack(unsigned int)
  __Z21LayoutRegenerateTrackv   # LayoutRegenerateTrack()
  __Z23LayoutDeleteMultiPylonstt   # LayoutDeleteMultiPylons(unsigned short, unsigned short)
  __Z23LayoutSetCoasterEditOldj   # LayoutSetCoasterEditOld(unsigned int)
)
