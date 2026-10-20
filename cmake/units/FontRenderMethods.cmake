# The four 4-bit font render method objects, the method table and the one-colour glyph writers (unit w45b), linked with the render
# method's core (unit w45a) so that DrawChar can drive the writers end to end. The fakes are the textured classes' twelve writers,
# LbDraw_GetClipRect, TbRect::Intersection and the font's resource base.
set(TPW_UNIT_FontRenderMethods src/ltms_fontrender_methods.cpp src/ltms_fontrender_onecolour.cpp src/ltms_fontrender_onecolour_noblend.cpp
    src/ltms_fontrender_method.cpp src/ltms_fontrender.cpp src/tb_fontrenderparams.cpp src/TbIRLE4bitFont.cpp src/TbIRLE4bitFont_TbIRLE4bitFont_TbIRLE4bitFont.cpp)
