# Unit v5943: CalculateBraceCost, SetCoasterEdit (instance overload), CoasterAcceptLeftMouseClick (src/game/rides/CoasterEdit.cpp).
# The callees outside the unit are recording fakes in tests/unit_CalculateBraceCost.cpp, which also defines the globals of other files.
set(TPW_UNIT_CalculateBraceCost src/game/rides/CoasterEdit.cpp platform/arena.cpp)
set(TPW_UNIT_CalculateBraceCost_KEEP
  __Z14SetCoasterEditP20tag_sCoasterInstancei   # SetCoasterEdit(tag_sCoasterInstance*, int)
  __Z18CalculateBraceCostP20tag_sCoasterInstanceiP15tag_sTrackBracePfS3_S3_S3_   # CalculateBraceCost(tag_sCoasterInstance*, int, tag_sTrackBrace*, float*, float*, float*, float*)
  __Z27CoasterAcceptLeftMouseClicki   # CoasterAcceptLeftMouseClick(int)
)
