# Unit v5652: CUI2DMapSprite::UpdateVisibilityAndColour. The callees outside the unit are recording fakes in
# tests/unit_CUI2DMapSprite_UpdateVisibilityAndColour.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMapSprite_UpdateVisibilityAndColour src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_CUI2DMapSprite_UpdateVisibilityAndColour_KEEP
  __ZN14CUI2DMapSprite25UpdateVisibilityAndColourEiii   # CUI2DMapSprite::UpdateVisibilityAndColour(int, int, int)
)
