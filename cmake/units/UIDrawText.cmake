# The interface's text drawing and InterfaceFont's widths (unit w35b): the text formatter, the font render parameters, the Bullfrog
# drawing calls, the converter and the font table's owner are fakes in the unit file.
set(TPW_UNIT_UIDrawText src/game/ui/InterfaceFont.cpp src/game/ui/ui_main.cpp src/game/ui/ui_utils.cpp)
set(TPW_UNIT_UIDrawText_KEEP
  __Z11UI_DrawTextPKDsRK6TbRectRK8TbColouraaaiPS1_i   # UI_DrawText(char16_t const*, TbRect const&, TbColour const&, signed char, signed char, signed char, int, TbRect*, int)
  __Z14MakeRenderAreaPhP12TbRenderAreaii   # MakeRenderArea(unsigned char*, TbRenderArea*, int, int)
  __Z19UIMain_ConvertMultiPKh   # UIMain_ConvertMulti(unsigned char const*)
  __Z19UIMain_ConvertMultiPh   # UIMain_ConvertMulti(unsigned char*)
  __Z20UIMain_ConvertStringPDs   # UIMain_ConvertString(char16_t*)
  __Z20UIMain_ConvertStringPKDs   # UIMain_ConvertString(char16_t const*)
  __Z28AdjustJustificationForHebrewRi   # AdjustJustificationForHebrew(int&)
  __ZN13InterfaceFont10LastCharInEPKDsss   # InterfaceFont::LastCharIn(char16_t const*, short, short)
  __ZN13InterfaceFont12GetCharWidthEDs   # InterfaceFont::GetCharWidth(char16_t)
  __ZN13InterfaceFont8GetWidthEPKDsss   # InterfaceFont::GetWidth(char16_t const*, short, short)
  _gUIMainConvertBuffer
  _gUIMainMBBuffer
  _gUIRenderArea
  _gUIRenderAreaInUse
)
