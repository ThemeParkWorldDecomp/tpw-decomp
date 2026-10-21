# The ltms text formatter and font renderer (unit w35a). The parameters' constructors and destructor are real (the formatter's render
# runs them); the fonts, the render methods, the parameters' other members, the vectors' other members and the line processor are fakes
# in the unit file.
set(TPW_UNIT_TextFormatter src/ltms/TbTextFormatter.cpp src/ltms/TbFontRender.cpp src/ltms/TbFontRenderParams.cpp)
set(TPW_UNIT_TextFormatter_KEEP
  __ZN11rank_callee13FormatterDtorEPvs   # rank_callee::FormatterDtor(void*, short)
  __ZN12TbFontRender10InitialiseERK18TbFontRenderParams   # TbFontRender::Initialise(TbFontRenderParams const&)
  __ZN12TbFontRender8DrawCharERK7TbPointtR6TbRect   # TbFontRender::DrawChar(TbPoint const&, unsigned short, TbRect&)
  __ZN12TbFontRenderC1Ev   # TbFontRender::TbFontRender()
  __ZN12TbFontRenderD1Ev   # TbFontRender::~TbFontRender()
  __ZN15TbTextFormatter11GetCharTypeEDs   # TbTextFormatter::GetCharType(char16_t)
  __ZN15TbTextFormatter11LayoutLinesEP14CCharacterInfoS1_   # TbTextFormatter::LayoutLines(CCharacterInfo*, CCharacterInfo*)
  __ZN15TbTextFormatter14CreateInfoListEPKhR18TbFontRenderParams   # TbTextFormatter::CreateInfoList(unsigned char const*, TbFontRenderParams&)
  __ZN15TbTextFormatter18EmptyStringProcessERK6TbRect   # TbTextFormatter::EmptyStringProcess(TbRect const&)
  __ZN15TbTextFormatter22GetCharIdxFromPositionE7TbPoint   # TbTextFormatter::GetCharIdxFromPosition(TbPoint)
  __ZN15TbTextFormatter24SetVerticalJustificationENS_14eJustificationE   # TbTextFormatter::SetVerticalJustification(TbTextFormatter::eJustification)
  __ZN15TbTextFormatter26SetHorizontalJustificationENS_14eJustificationE   # TbTextFormatter::SetHorizontalJustification(TbTextFormatter::eJustification)
  __ZN15TbTextFormatter4DrawERK6TbRectPKhR18TbFontRenderParamsPS0_   # TbTextFormatter::Draw(TbRect const&, unsigned char const*, TbFontRenderParams&, TbRect*)
  __ZN15TbTextFormatter6LayoutE6TbRect   # TbTextFormatter::Layout(TbRect)
  __ZN15TbTextFormatter7MeasureERK6TbRectPKhR18TbFontRenderParamsPS0_   # TbTextFormatter::Measure(TbRect const&, unsigned char const*, TbFontRenderParams&, TbRect*)
  __ZN15TbTextFormatter8DrawTextER18TbFontRenderParamsP6TbRectb   # TbTextFormatter::DrawText(TbFontRenderParams&, TbRect*, bool)
  __ZN15TbTextFormatterC1Ev   # TbTextFormatter::TbTextFormatter()
  __ZN15TbTextFormatterD1Ev   # TbTextFormatter::~TbTextFormatter()
  __ZN18TbFontRenderParams10UseTextureERK12TbRenderAreaRK7TbPoint   # TbFontRenderParams::UseTexture(TbRenderArea const&, TbPoint const&)
  __ZN18TbFontRenderParams11UseReColourERK8TbColour   # TbFontRenderParams::UseReColour(TbColour const&)
  __ZN18TbFontRenderParams14UseBlendedModeEv   # TbFontRenderParams::UseBlendedMode()
  __ZN18TbFontRenderParams14UsePreColouredEv   # TbFontRenderParams::UsePreColoured()
  __ZN18TbFontRenderParams17UseAlphaWriteModeEv   # TbFontRenderParams::UseAlphaWriteMode()
  __ZN18TbFontRenderParams7SetFontEP7TbIFont   # TbFontRenderParams::SetFont(TbIFont*)
  __ZN18TbFontRenderParamsC1ERKS_   # TbFontRenderParams::TbFontRenderParams(TbFontRenderParams const&)
  __ZN18TbFontRenderParamsC1Ev   # TbFontRenderParams::TbFontRenderParams()
  __ZN18TbFontRenderParamsD1Ev   # TbFontRenderParams::~TbFontRenderParams()
  __ZN18TbFontRenderParamsaSERKS_   # TbFontRenderParams::operator=(TbFontRenderParams const&)
  __ZN9MslVectorI14CCharacterInfoE9tear_downEv   # MslVector<CCharacterInfo>::tear_down()
  __ZN9MslVectorI7TbPointE9tear_downEv   # MslVector<TbPoint>::tear_down()
  __ZN9MslVectorI9CLineInfoE9tear_downEv   # MslVector<CLineInfo>::tear_down()
  __ZNK15TbTextFormatter20GetCharPositionArrayEv   # TbTextFormatter::GetCharPositionArray() const
  __ZNK15TbTextFormatter24GetCharPositionArraySizeEv   # TbTextFormatter::GetCharPositionArraySize() const
  __ZNK18TbFontRenderParams7GetFontEv   # TbFontRenderParams::GetFont() const
  __ZNK18TbFontRenderParams7GetTypeEv   # TbFontRenderParams::GetType() const
  __ZNK18TbFontRenderParams8GetStyleEv   # TbFontRenderParams::GetStyle() const
)
