# Unit v5042: CalcSkirtTextureDirnFlags (src/game/park/SkirtDeform.cpp). The callees outside the unit are recording fakes in
# tests/unit_CalcSkirtTextureDirnFlags.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CalcSkirtTextureDirnFlags src/game/park/SkirtDeform.cpp platform/arena.cpp)
set(TPW_UNIT_CalcSkirtTextureDirnFlags_KEEP
  __Z25CalcSkirtTextureDirnFlagsP16tag_sRideAnimSetP9RideShapeiii   # CalcSkirtTextureDirnFlags(tag_sRideAnimSet*, RideShape*, int, int, int)
  _gSkirtCells
  _gSkirtCurrentCell
)
