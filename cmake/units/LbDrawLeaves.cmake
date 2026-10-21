# The Bullfrog library's rectangle, viewport, clip and palette leaves (unit w35b): the only fake is TbPixelCaps::Set.
set(TPW_UNIT_LbDrawLeaves src/bullfrog/LbDraw.cpp src/bullfrog/LbDrawText.cpp src/bullfrog/LbRect.cpp)
set(TPW_UNIT_LbDrawLeaves_KEEP
  __Z12LbRect_EmptyP6TbRect   # LbRect_Empty(TbRect*)
  __Z16LbRect_NormaliseP6TbRect   # LbRect_Normalise(TbRect*)
  __Z17LbDraw_SetCurrentP12TbRenderArea   # LbDraw_SetCurrent(TbRenderArea*)
  __Z18LbDraw_GetClipRectv   # LbDraw_GetClipRect()
  __Z18LbDraw_SetClipRectPK6TbRect   # LbDraw_SetClipRect(TbRect const*)
  __Z18LbDraw_SetViewPortPK6TbRect   # LbDraw_SetViewPort(TbRect const*)
  __Z19LbRect_IntersectionP6TbRectPKS_   # LbRect_Intersection(TbRect*, TbRect const*)
  __Z20LbPalette_FindColourPK9TbPalettehhh   # LbPalette_FindColour(TbPalette const*, unsigned char, unsigned char, unsigned char)
  __lbCurrentClipRect
  __lbCurrentOriginPtr
  __lbCurrentPixelCaps
  __lbCurrentViewPort
  __lbRenderContext16
  __lbRenderContext24
  __lbRenderContext32
  __lbRenderContext8
  __lbpCurrentRA
  __lbpRenderBase
)
