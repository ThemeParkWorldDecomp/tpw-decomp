# Unit w224: text drawing; external sprite draws and drawing globals are faked by the unit.
set(TPW_UNIT_LbDrawText src/bullfrog/LbDrawText.cpp src/bullfrog/LbRect.cpp src/game/ui/intrface.cpp platform/arena.cpp)
set(TPW_UNIT_LbDrawText_KEEP
  __Z11LbDraw_TextiiPKc8TbColour   # LbDraw_Text(int, int, char const*, TbColour)
  __Z12LbRect_WidthPK6TbRect   # LbRect_Width(TbRect const*)
  __Z13LbRect_HeightPK6TbRect   # LbRect_Height(TbRect const*)
  __Z16LbDraw_CheapTextiiPKc8TbColour   # LbDraw_CheapText(int, int, char const*, TbColour)
  __Z18LbDraw_GetTextSizePKcP6TbSize   # LbDraw_GetTextSize(char const*, TbSize*)
  __Z18LbDraw_SetTextFontPK12TbTextRenderPv   # LbDraw_SetTextFont(TbTextRender const*, void*)
  __Z18LbDraw_UnicodeTextiiPKDs8TbColour   # LbDraw_UnicodeText(int, int, char16_t const*, TbColour)
  __Z20LbDraw_FormattedTextPK6TbRectPKcj8TbColour   # LbDraw_FormattedText(TbRect const*, char const*, unsigned int, TbColour)
  __Z22SpriteRender_OnMeasurePvP6TbSizej   # SpriteRender_OnMeasure(void*, TbSize*, unsigned int)
  __Z23LbDraw_UnicodeCheapTextiiPKDs8TbColour   # LbDraw_UnicodeCheapText(int, int, char16_t const*, TbColour)
  __Z23SpriteRender_OnDrawCharPviij8TbColour   # SpriteRender_OnDrawChar(void*, int, int, unsigned int, TbColour)
  __Z25LbDraw_GetUnicodeTextSizePKDsP6TbSize   # LbDraw_GetUnicodeTextSize(char16_t const*, TbSize*)
  __Z27LbDraw_UnicodeFormattedTextPK6TbRectPKDsj8TbColour   # LbDraw_UnicodeFormattedText(TbRect const*, char16_t const*, unsigned int, TbColour)
  __Z28LbDraw_GetTextCharacterWidthj   # LbDraw_GetTextCharacterWidth(unsigned int)
  __Z29LbDraw_GetFormattedTextHeightjPKc   # LbDraw_GetFormattedTextHeight(unsigned int, char const*)
  __Z29LbDraw_GetTextCharacterHeightj   # LbDraw_GetTextCharacterHeight(unsigned int)
  __Z31OneColourSpriteRender_OnMeasurePvP6TbSizej   # OneColourSpriteRender_OnMeasure(void*, TbSize*, unsigned int)
  __Z32OneColourSpriteRender_OnDrawCharPviij8TbColour   # OneColourSpriteRender_OnDrawChar(void*, int, int, unsigned int, TbColour)
  __Z36LbDraw_GetUnicodeFormattedTextHeightjPKDs   # LbDraw_GetUnicodeFormattedTextHeight(unsigned int, char16_t const*)
  __ZN8TbColourC1Ev   # TbColour::TbColour()
  __lbOneColourSpriteRender
  __lbSpriteRender
  _gLbCheapTextColour
  _gLbCheapTextGuard
  _gLbCheapTextIndex
  _gLbCheapUnicodeTextColour
  _gLbCheapUnicodeTextGuard
  _gLbCheapUnicodeTextIndex
  _gLbTextFont
  _gLbTextRender
  _gLbTextSelectSense
  __Z10word_widthIDsEjPKT_Pj   # template uint32_t word_width<char16_t>(const char16_t*, uint32_t*) (explicit instantiation)
  __Z10word_widthIcEjPKT_Pj   # template uint32_t word_width<char>(const char*, uint32_t*) (explicit instantiation)
  __Z12Text_MeasureIDsEjPKT_P6TbSize   # template uint32_t Text_Measure<char16_t>(const char16_t*, TbSize*) (explicit instantiation)
  __Z12Text_MeasureIcEjPKT_P6TbSize   # template uint32_t Text_Measure<char>(const char*, TbSize*) (explicit instantiation)
  __Z14Draw_CheapTextIDsEviiPKT_8TbColour   # template void Draw_CheapText<char16_t>(int32_t, int32_t, const char16_t*, TbColour) (explicit instantiation)
  __Z14Draw_CheapTextIcEviiPKT_8TbColour   # template void Draw_CheapText<char>(int32_t, int32_t, const char*, TbColour) (explicit instantiation)
  __Z14draw_text_lineIDsEviiPKT_j8TbColourjijj   # template void draw_text_line<char16_t>(int32_t, int32_t, const char16_t*, uint32_t, TbColour, uint32_t, int32_t, uint32_t, uint32_t) (explicit instantiation)
  __Z14draw_text_lineIcEviiPKT_j8TbColourjijj   # template void draw_text_line<char>(int32_t, int32_t, const char*, uint32_t, TbColour, uint32_t, int32_t, uint32_t, uint32_t) (explicit instantiation)
  __Z15draw_text_charsIDsEviiPKT_j8TbColour   # template void draw_text_chars<char16_t>(int32_t, int32_t, const char16_t*, uint32_t, TbColour) (explicit instantiation)
  __Z15draw_text_charsIcEviiPKT_j8TbColour   # template void draw_text_chars<char>(int32_t, int32_t, const char*, uint32_t, TbColour) (explicit instantiation)
  __Z18Draw_FormattedTextIDsEvPK6TbRectPKT_j8TbColour   # template void Draw_FormattedText<char16_t>(const TbRect*, const char16_t*, uint32_t, TbColour) (explicit instantiation)
  __Z18Draw_FormattedTextIcEvPK6TbRectPKT_j8TbColour   # template void Draw_FormattedText<char>(const TbRect*, const char*, uint32_t, TbColour) (explicit instantiation)
  __Z27Text_MeasureFormattedHeightIDsEijPKT_   # template int32_t Text_MeasureFormattedHeight<char16_t>(uint32_t, const char16_t*) (explicit instantiation)
  __Z27Text_MeasureFormattedHeightIcEijPKT_   # template int32_t Text_MeasureFormattedHeight<char>(uint32_t, const char*) (explicit instantiation)
  __Z9Draw_TextIDsEviiPKT_8TbColour   # template void Draw_Text<char16_t>(int32_t, int32_t, const char16_t*, TbColour) (explicit instantiation)
  __Z9Draw_TextIcEviiPKT_8TbColour   # template void Draw_Text<char>(int32_t, int32_t, const char*, TbColour) (explicit instantiation)
  tmpl_01d67bd9126c@bullfrog/LbDrawText.cpp   # draw_text_chars (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_2231c19942e0@bullfrog/LbDrawText.cpp   # Draw_CheapText (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_284bf8171cab@bullfrog/LbDrawText.cpp   # Text_MeasureFormattedHeight (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_327db3ae750e@bullfrog/LbDrawText.cpp   # Draw_FormattedText (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_5191cfaf5c22@bullfrog/LbDrawText.cpp   # Text_Measure (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_b842a3d85fcc@bullfrog/LbDrawText.cpp   # draw_text_line (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_c9bef1a838c3@bullfrog/LbDrawText.cpp   # word_width (a template definition src/LbDraw_text.cpp had before the regroup)
  tmpl_de2669d15be2@bullfrog/LbDrawText.cpp   # Draw_Text (a template definition src/LbDraw_text.cpp had before the regroup)
)
