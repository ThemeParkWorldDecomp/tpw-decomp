set(TPW_UNIT_LbDrawPrimitives src/bullfrog/LbDraw.cpp src/bullfrog/LbDrawText.cpp src/bullfrog/TbSystemInfo.cpp
    src/bullfrog/Draw.cpp src/bullfrog/LbRect.cpp)
set(TPW_UNIT_LbDrawPrimitives_KEEP
  __Z11LbDraw_Lineiiii8TbColour   # LbDraw_Line(int, int, int, int, TbColour)
  __Z12LbDraw_Pixelii8TbColour   # LbDraw_Pixel(int, int, TbColour)
  __Z13LbDraw_Circleiij8TbColour   # LbDraw_Circle(int, int, unsigned int, TbColour)
  __Z13LbDraw_SpriteiiPK8TbSprite   # LbDraw_Sprite(int, int, TbSprite const*)
  __Z14LbDraw_Paletteiijji   # LbDraw_Palette(int, int, unsigned int, unsigned int, int)
  __Z15LbDraw_PropTextiiPKc8TbColour   # LbDraw_PropText(int, int, char const*, TbColour)
  __Z15LbDraw_Triangleiiiiii8TbColour   # LbDraw_Triangle(int, int, int, int, int, int, TbColour)
  __Z16LbDraw_RectanglePK6TbRect8TbColour   # LbDraw_Rectangle(TbRect const*, TbColour)
  __Z16LbDraw_RectangleRK6TbRect8TbColour   # LbDraw_Rectangle(TbRect const&, TbColour)
  __Z17LbDraw_SetCurrentP9TbSurface   # LbDraw_SetCurrent(TbSurface*)
  __Z18LbDraw_GetViewPortv   # LbDraw_GetViewPort()
  __Z19LbDraw_CircleFillediij8TbColour   # LbDraw_CircleFilled(int, int, unsigned int, TbColour)
  __Z19LbDraw_ScaledSpriteiiPK8TbSpritejj   # LbDraw_ScaledSprite(int, int, TbSprite const*, unsigned int, unsigned int)
  __Z19LbDraw_VerticalLineiii8TbColour   # LbDraw_VerticalLine(int, int, int, TbColour)
  __Z19LbPalette_ToCompactPhPK9TbPalettej   # LbPalette_ToCompact(unsigned char*, TbPalette const*, unsigned int)
  __Z20LbDraw_CircleOutlineiij8TbColour   # LbDraw_CircleOutline(int, int, unsigned int, TbColour)
  __Z20LbDraw_ClearClipRect8TbColour   # LbDraw_ClearClipRect(TbColour)
  __Z21LbDraw_HorizontalLineiii8TbColour   # LbDraw_HorizontalLine(int, int, int, TbColour)
  __Z21LbDraw_RemappedSpriteiiPK8TbSpritePKh   # LbDraw_RemappedSprite(int, int, TbSprite const*, unsigned char const*)
  __Z22LbDraw_OneColourSpriteiiPK8TbSprite8TbColour   # LbDraw_OneColourSprite(int, int, TbSprite const*, TbColour)
  __Z22LbDraw_RectangleFilledPK6TbRect8TbColour   # LbDraw_RectangleFilled(TbRect const*, TbColour)
  __Z22LbDraw_RectangleFilledRK6TbRect8TbColour   # LbDraw_RectangleFilled(TbRect const&, TbColour)
  __Z22LbDraw_ReleaseClipRectv   # LbDraw_ReleaseClipRect()
  __Z22LbDraw_ReleaseViewPortv   # LbDraw_ReleaseViewPort()
  __Z22LbDraw_UnicodePropTextiiPKDs8TbColour   # LbDraw_UnicodePropText(int, int, char16_t const*, TbColour)
  __Z23LbDraw_RectangleOutlinePK6TbRect8TbColour   # LbDraw_RectangleOutline(TbRect const*, TbColour)
  __Z23LbDraw_RectangleOutlineRK6TbRect8TbColour   # LbDraw_RectangleOutline(TbRect const&, TbColour)
  __Z27LbDraw_SetSpriteScalingDataiijjjj   # LbDraw_SetSpriteScalingData(int, int, unsigned int, unsigned int, unsigned int, unsigned int)
  __ZN3bfl4draw13gDrawContext8E   # bfl::draw::gDrawContext8
  __ZN3bfl4draw14gDrawContext16E   # bfl::draw::gDrawContext16
  __ZN3bfl4draw14gDrawContext24E   # bfl::draw::gDrawContext24
  __ZN3bfl4draw14gDrawContext32E   # bfl::draw::gDrawContext32
  __ZN3bfl4draw5CDraw10SetCurrentEP12TbRenderArea   # bfl::draw::CDraw::SetCurrent(TbRenderArea*)
  __ZN3bfl4draw5CDraw10SetCurrentEP9TbSurface   # bfl::draw::CDraw::SetCurrent(TbSurface*)
  __ZN3bfl4draw5CDraw11SetClipRectERK6TbRect   # bfl::draw::CDraw::SetClipRect(TbRect const&)
  __ZN3bfl4draw5CDraw11SetViewPortERK6TbRect   # bfl::draw::CDraw::SetViewPort(TbRect const&)
  __ZN3bfl4draw5CDraw13ClearClipRectEv   # bfl::draw::CDraw::ClearClipRect()
  __ZN3bfl4draw5CDraw13ClearViewPortEv   # bfl::draw::CDraw::ClearViewPort()
  __ZN3bfl4draw5CDrawC1Ev   # bfl::draw::CDraw::CDraw()
  __ZN3bfl4draw5CDrawD1Ev   # bfl::draw::CDraw::~CDraw()
  __ZNK3bfl4draw5CDraw10GetCurrentEv   # bfl::draw::CDraw::GetCurrent() const
  __ZNK3bfl4draw5CDraw11GetClipRectEv   # bfl::draw::CDraw::GetClipRect() const
  __ZNK3bfl4draw5CDraw11GetViewPortEv   # bfl::draw::CDraw::GetViewPort() const
  __ZNK6TbRect12IntersectionERKS_   # TbRect::Intersection(TbRect const&) const
  __ZNK6TbRect8BoundingERKS_   # TbRect::Bounding(TbRect const&) const
)
