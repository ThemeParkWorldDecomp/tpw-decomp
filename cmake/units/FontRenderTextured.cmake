# The twelve textured glyph writers (unit w45c), linked with the four method objects and the one-colour writers (unit w45b) and the
# render method's core (unit w45a) so that DrawChar can drive them end to end through the table. The fakes are LbDraw_GetClipRect,
# TbRect::Intersection and the font's resource base. TbPixFormat32 (the format every texture must have) is src/bullfrog/TbPixFmt.cpp.
set(TPW_UNIT_FontRenderTextured src/ltms/TbTextured4BitFontRenderMethod.cpp
    src/ltms/TbTexturedNoBlend4BitFontRenderMethod.cpp src/ltms/TbOneColour4BitFontRenderMethod.cpp
    src/ltms/TbOneColourNoBlend4BitFontRenderMethod.cpp src/ltms/StaticUnicodeBuffer.cpp src/ltms/TbFontRender.cpp
    src/ltms/TbFontRenderParams.cpp src/ltms/TbIRLE4bitFont.cpp src/bullfrog/TbPixFmt.cpp)
set(TPW_UNIT_FontRenderTextured_KEEP
  _TbPixFormat16
  _TbPixFormat24
  _TbPixFormat32
  _TbPixFormat8
  __Z12SetWriteZerob   # SetWriteZero(bool)
  __ZN10TbMaskInfo3SetEjj   # TbMaskInfo::Set(unsigned int, unsigned int)
  __ZN11TbPixelCaps3SetERK11TbPixFormat   # TbPixelCaps::Set(TbPixFormat const&)
  __ZN12TbFontRender10InitialiseERK18TbFontRenderParams   # TbFontRender::Initialise(TbFontRenderParams const&)
  __ZN12TbFontRender8DrawCharERK7TbPointtR6TbRect   # TbFontRender::DrawChar(TbPoint const&, unsigned short, TbRect&)
  __ZN12TbFontRenderC1Ev   # TbFontRender::TbFontRender()
  __ZN12TbFontRenderD1Ev   # TbFontRender::~TbFontRender()
  __ZN14TbIRLE4bitFont11GetCharSizeEt   # TbIRLE4bitFont::GetCharSize(unsigned short)
  __ZN14TbIRLE4bitFont12GetCharCountEv   # TbIRLE4bitFont::GetCharCount()
  __ZN14TbIRLE4bitFont12GetCharIndexERPKh   # TbIRLE4bitFont::GetCharIndex(unsigned char const*&)
  __ZN14TbIRLE4bitFont14GetMBToUnicodeEv   # TbIRLE4bitFont::GetMBToUnicode()
  __ZN14TbIRLE4bitFont18GetVerticalSpacingEv   # TbIRLE4bitFont::GetVerticalSpacing()
  __ZN14TbIRLE4bitFont7GetCapsEv   # TbIRLE4bitFont::GetCaps()
  __ZN14TbIRLE4bitFont7GetTypeEv   # TbIRLE4bitFont::GetType()
  __ZN14TbIRLE4bitFontC1EPhP16TbIResourceIndex   # TbIRLE4bitFont::TbIRLE4bitFont(unsigned char*, TbIResourceIndex*)
  __ZN14TbIRLE4bitFontC1Ev   # TbIRLE4bitFont::TbIRLE4bitFont()
  __ZN14TbIRLE4bitFontD1Ev   # TbIRLE4bitFont::~TbIRLE4bitFont()
  __ZN18TbFontRenderMethodD1Ev   # TbFontRenderMethod::~TbFontRenderMethod()
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
  __ZN22Tb4BitFontRenderMethod19CalculateSourceRectERK6TbRectRK7TbPointRK12TbRenderArea   # Tb4BitFontRenderMethod::CalculateSourceRect(TbRect const&, TbPoint const&, TbRenderArea const&)
  __ZN22Tb4BitFontRenderMethod8DrawCharERK7TbPointtR6TbRectR18TbFontRenderParams   # Tb4BitFontRenderMethod::DrawChar(TbPoint const&, unsigned short, TbRect&, TbFontRenderParams&)
  __ZN22Tb4BitFontRenderMethod9GetRotateEj   # Tb4BitFontRenderMethod::GetRotate(unsigned int)
  __ZN22Tb4BitFontRenderMethodD1Ev   # Tb4BitFontRenderMethod::~Tb4BitFontRenderMethod()
  __ZN30TbTextured4BitFontRenderMethod13DrawCharTo555ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharTo555(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethod13DrawCharTo565ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharTo565(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethod13DrawCharToAnyER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethod14DrawCharTo4444ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharTo4444(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethod14DrawCharTo8888ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharTo8888(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethod14DrawCharToBGRAER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTextured4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN30TbTextured4BitFontRenderMethodC1Ev   # TbTextured4BitFontRenderMethod::TbTextured4BitFontRenderMethod()
  __ZN30TbTextured4BitFontRenderMethodD1Ev   # TbTextured4BitFontRenderMethod::~TbTextured4BitFontRenderMethod()
  __ZN31TbOneColour4BitFontRenderMethod13DrawCharTo555ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharTo555(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethod13DrawCharTo565ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharTo565(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethod13DrawCharToAnyER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethod14DrawCharTo4444ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharTo4444(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethod14DrawCharTo8888ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharTo8888(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethod14DrawCharToBGRAER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColour4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN31TbOneColour4BitFontRenderMethodC1Ev   # TbOneColour4BitFontRenderMethod::TbOneColour4BitFontRenderMethod()
  __ZN31TbOneColour4BitFontRenderMethodD1Ev   # TbOneColour4BitFontRenderMethod::~TbOneColour4BitFontRenderMethod()
  __ZN37TbTexturedNoBlend4BitFontRenderMethod13DrawCharTo555ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo555(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethod13DrawCharTo565ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo565(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethod13DrawCharToAnyER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethod14DrawCharTo4444ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo4444(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethod14DrawCharTo8888ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharTo8888(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethod14DrawCharToBGRAER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbTexturedNoBlend4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN37TbTexturedNoBlend4BitFontRenderMethodC1Ev   # TbTexturedNoBlend4BitFontRenderMethod::TbTexturedNoBlend4BitFontRenderMethod()
  __ZN37TbTexturedNoBlend4BitFontRenderMethodD1Ev   # TbTexturedNoBlend4BitFontRenderMethod::~TbTexturedNoBlend4BitFontRenderMethod()
  __ZN38TbOneColourNoBlend4BitFontRenderMethod13DrawCharTo555ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo555(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethod13DrawCharTo565ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo565(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethod13DrawCharToAnyER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharToAny(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethod14DrawCharTo4444ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo4444(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethod14DrawCharTo8888ER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharTo8888(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethod14DrawCharToBGRAER12TbRenderAreaRK31C4BitFontRenderMethodParameters   # TbOneColourNoBlend4BitFontRenderMethod::DrawCharToBGRA(TbRenderArea&, C4BitFontRenderMethodParameters const&)
  __ZN38TbOneColourNoBlend4BitFontRenderMethodC1Ev   # TbOneColourNoBlend4BitFontRenderMethod::TbOneColourNoBlend4BitFontRenderMethod()
  __ZN38TbOneColourNoBlend4BitFontRenderMethodD1Ev   # TbOneColourNoBlend4BitFontRenderMethod::~TbOneColourNoBlend4BitFontRenderMethod()
  __ZN7FourBit8FontData1IEv   # FourBit::FontData::I()
  __ZN7FourBit8FontData5sMadeE   # FourBit::FontData::sMade
  __ZN7FourBit8FontDataD1Ev   # FourBit::FontData::~FontData()
  __ZN7TbIFontD1Ev   # TbIFont::~TbIFont()
  __ZNK14TbIRLE4bitFont11GetCharDataEt   # TbIRLE4bitFont::GetCharData(unsigned short) const
  __ZNK14TbIRLE4bitFont13GetCharHeaderEt   # TbIRLE4bitFont::GetCharHeader(unsigned short) const
  __ZNK18TbFontRenderParams7GetFontEv   # TbFontRenderParams::GetFont() const
  __ZNK18TbFontRenderParams7GetTypeEv   # TbFontRenderParams::GetType() const
  __ZNK18TbFontRenderParams8GetStyleEv   # TbFontRenderParams::GetStyle() const
  _gFontRenderMethods
  _gOneColour4BitFontRenderMethod
  _gOneColourNoBlend4BitFontRenderMethod
  _gTextured4BitFontRenderMethod
  _gTexturedNoBlend4BitFontRenderMethod
  _gWriteZero
  __ZN12_GLOBAL__N_112gScaledGlyphE   # gScaledGlyph (static object)
  __ZN12_GLOBAL__N_19gFontDataE   # gFontData (static object)
)
