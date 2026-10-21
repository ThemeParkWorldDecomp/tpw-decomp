# Unit v5027: ESP_SwitchToFirstPersonSprites, ESP_SwitchToThirdPersonSprites, ESP_AddHLSpriteFromScript (src/game/HLSprite.cpp).
# The callees outside the unit are recording fakes in tests/unit_ESP_SwitchToFirstPersonSprites.cpp, which also defines the globals of other files.
set(TPW_UNIT_ESP_SwitchToFirstPersonSprites src/game/HLSprite.cpp src/game/park/analyser.cpp platform/arena.cpp)
set(TPW_UNIT_ESP_SwitchToFirstPersonSprites_KEEP
  __Z17ESP_SwitchSpritesi   # ESP_SwitchSprites(int)
  __Z20ESP_ScriptHostSpritePK9tagSCRIPT   # ESP_ScriptHostSprite(tagSCRIPT const*)
  __Z22ESP_IsScriptHostSpritePK9tagSCRIPTPKv   # ESP_IsScriptHostSprite(tagSCRIPT const*, void const*)
  __Z25ESP_AddHLSpriteFromScriptP9tagSCRIPT   # ESP_AddHLSpriteFromScript(tagSCRIPT*)
  __Z30ESP_SwitchToFirstPersonSpritesv   # ESP_SwitchToFirstPersonSprites()
  __Z30ESP_SwitchToThirdPersonSpritesv   # ESP_SwitchToThirdPersonSprites()
  _gESPCellCounts
  _gESPGlobalCount
  _gESPMaxCellX
  _gESPMaxCellZ
  _gESPMinCellX
  _gESPMinCellZ
)
