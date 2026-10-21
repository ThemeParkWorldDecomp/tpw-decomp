# Unit v5114: ResetHLSprites and DrawHLSprites (src/game/HLSprite.cpp). The callees outside the unit are recording fakes in
# tests/unit_DrawHLSprites.cpp, which also defines the globals of other files.
set(TPW_UNIT_DrawHLSprites src/game/HLSprite.cpp platform/arena.cpp)
set(TPW_UNIT_DrawHLSprites_KEEP
  __Z13DrawHLSpritesv   # DrawHLSprites()
  __Z14ResetHLSpritesv   # ResetHLSprites()
  _gHLSpritesEnabled
)
