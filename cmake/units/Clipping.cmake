set(TPW_UNIT_Clipping src/engine/Clipping.cpp)
set(TPW_UNIT_Clipping_KEEP
  __ZN9CClipping12CPP_TestDataEPhPvS0_PKviij   # CClipping::CPP_TestData(unsigned char*, void*, unsigned char*, void const*, int, int, unsigned int)
  __ZN9CClipping16CheckBoundingBoxERK11sAlignedBoxR7CMatrix   # CClipping::CheckBoundingBox(sAlignedBox const&, CMatrix&)
  __ZN9CClipping8ClipPolyEiP11SFVF_TLCTEXP14VertexClipListRii   # CClipping::ClipPoly(int, SFVF_TLCTEX*, VertexClipList*, int&, int)
  __ZN9CClipping8ClipPolyEiP11SFVF_TLCTEXS1_Rii   # CClipping::ClipPoly(int, SFVF_TLCTEX*, SFVF_TLCTEX*, int&, int)
  __ZN9CClipping8ClipPolyEiPtP11SFVF_TLCTEXRii   # CClipping::ClipPoly(int, unsigned short*, SFVF_TLCTEX*, int&, int)
)
