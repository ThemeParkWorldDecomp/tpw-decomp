# The text draw objects (unit w35c). The sprite draw object under them and the rectangles are real; the text formatter, the font parameters,
# the render area helpers, the engine's sprite surfaces, the map and the draw object base class are fakes in the unit file, which also uses
# the font table's real classes (src/InterfaceFont.cpp).
set(TPW_UNIT_UITextObjects src/ui_DrawObjectText.cpp src/ui_DrawObjectText_ctor.cpp src/ui_DrawObjectSprite.cpp src/UI_Base.cpp src/InterfaceFont.cpp)
