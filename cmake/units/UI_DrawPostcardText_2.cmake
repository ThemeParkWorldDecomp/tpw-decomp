# Unit v51012: UI_DrawPostcardText, __sinit_ui_utils_cpp, MakeRenderAreaFromRAW, InterfaceDrawEngineHGradient::CalcUVs and
# InterfaceVisitorTimeTextObject::Change (src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_utils.cpp). The callees outside the unit are recording
# fakes in tests/unit_UI_DrawPostcardText_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_UI_DrawPostcardText_2 src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceTextObjectSprite.cpp
    src/game/ui/ui_utils.cpp platform/arena.cpp)
set(TPW_UNIT_UI_DrawPostcardText_2_KEEP
  __Z19UI_DrawPostcardTextPDsPviiN13SamsUtilities10URectangle8LongRectEji   # UI_DrawPostcardText(char16_t*, void*, int, int, SamsUtilities::URectangle::LongRect, unsigned int, int)
  __Z20__sinit_ui_utils_cppv   # __sinit_ui_utils_cpp()
  __Z21MakeRenderAreaFromRAWPvii14RAWSurfaceTypeP12TbRenderArea   # MakeRenderAreaFromRAW(void*, int, int, RAWSurfaceType, TbRenderArea*)
  __ZN28InterfaceDrawEngineHGradient7CalcUVsEv   # InterfaceDrawEngineHGradient::CalcUVs()
  __ZN30InterfaceVisitorTimeTextObject6ChangeEjP8DOChange   # InterfaceVisitorTimeTextObject::Change(unsigned int, DOChange*)
  _gUIUtilsColour
)
