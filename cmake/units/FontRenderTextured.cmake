# The twelve textured glyph writers (unit w45c), linked with the four method objects and the one-colour writers (unit w45b) and the
# render method's core (unit w45a) so that DrawChar can drive them end to end through the table. The fakes are LbDraw_GetClipRect,
# TbRect::Intersection and the font's resource base. TbPixFormat32 (the format every texture must have) is src/TbPixFmt.cpp.
set(TPW_UNIT_FontRenderTextured src/ltms_fontrender_textured.cpp src/ltms_fontrender_textured_noblend.cpp src/ltms_fontrender_methods.cpp
    src/ltms_fontrender_onecolour.cpp src/ltms_fontrender_onecolour_noblend.cpp src/ltms_fontrender_method.cpp src/ltms_fontrender.cpp
    src/tb_fontrenderparams.cpp src/TbIRLE4bitFont.cpp src/TbIRLE4bitFont_TbIRLE4bitFont_TbIRLE4bitFont.cpp src/TbPixFmt.cpp)
