# The 4-bit font render method's core and the render parameters' last members (unit w45a): DrawChar, CalculateSourceRect, GetRotate,
# FourBit::FontData and the destructors; TbFontRender and the parameters; the real font (GetCharHeader and GetCharData). The pixel
# writers, LbDraw_GetClipRect, TbRect::Intersection and the font's resource base are fakes in the unit file.
set(TPW_UNIT_FontRenderMethod src/ltms_fontrender_method.cpp src/ltms_fontrender.cpp src/tb_fontrenderparams.cpp src/TbIRLE4bitFont.cpp src/TbIRLE4bitFont_TbIRLE4bitFont_TbIRLE4bitFont.cpp)
