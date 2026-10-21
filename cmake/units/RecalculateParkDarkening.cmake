# Unit v5902: RecalculateParkDarkening, UpdateHFieldDarkeningNormals, SetupMeshForPolySearchSimple (src/game/park/TPHField.cpp).
# The callees outside the unit are recording fakes in tests/unit_RecalculateParkDarkening.cpp, which also defines the globals of other files.
set(TPW_UNIT_RecalculateParkDarkening src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_RecalculateParkDarkening_KEEP
  __Z24RecalculateParkDarkeningv   # RecalculateParkDarkening()
  __Z28SetupMeshForPolySearchSimpleP9tag_sMeshi   # SetupMeshForPolySearchSimple(tag_sMesh*, int)
  __Z28UpdateHFieldDarkeningNormalsP9tag_sNode7sMatrix   # UpdateHFieldDarkeningNormals(tag_sNode*, sMatrix)
  _gParkDarkeningFlag
)
