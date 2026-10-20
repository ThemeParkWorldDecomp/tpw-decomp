# Unit w201: the park's hoarding (the walk, the lock, the redraw, the image and the legality checks; src/LLCellSystem_hoardimage.cpp) with the cell flag and
# kind tests (src/TrackCell_flags.cpp, src/MapCell_flags.cpp). The cell bases and the track cell's neighbouring cell (src/CellBase.cpp, src/TrackCell.cpp) and the
# eight compass deltas (src/MapDelta.cpp) run for real; every other callee is a recording fake in tests/unit_HoardingImage.cpp, which also defines the globals of
# other files.
set(TPW_UNIT_HoardingImage src/LLCellSystem_hoardimage.cpp src/TrackCell_flags.cpp src/MapCell_flags.cpp src/CellBase.cpp src/TrackCell.cpp src/MapDelta.cpp src/Serialise_basic.cpp src/LLCellClicks.cpp)
