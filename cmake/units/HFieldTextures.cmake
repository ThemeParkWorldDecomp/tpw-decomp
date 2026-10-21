set(TPW_UNIT_HFieldTextures src/game/MeshLoad.cpp src/game/TextureTable.cpp src/game/park/TPHField.cpp
    platform/arena.cpp)
set(TPW_UNIT_HFieldTextures_KEEP
  __Z10IsMiscCharP10sFileParse   # IsMiscChar(sFileParse*)
  __Z12IsWhiteSpaceP10sFileParse   # IsWhiteSpace(sFileParse*)
  __Z13ParseLevelTCTPcP18sExtraTextureTable   # ParseLevelTCT(char*, sExtraTextureTable*)
  __Z14SkipWhitespaceP10sFileParse   # SkipWhitespace(sFileParse*)
  __Z15ReplaceTexturesP15tag_sM3D2HeaderPiP8sTextureij   # ReplaceTextures(tag_sM3D2Header*, int*, sTexture*, int, unsigned int)
  __Z22FreeHFieldTextureTablev   # FreeHFieldTextureTable()
  __Z23BuildHFieldTextureTableP15tag_sM3D2Header   # BuildHFieldTextureTable(tag_sM3D2Header*)
  __Z23DetermineHFieldFlatnessP15tag_sM3D2Header   # DetermineHFieldFlatness(tag_sM3D2Header*)
  __Z30InterceptAndInsertM3D2TexturesP15tag_sM3D2HeaderP18sExtraTextureTablePc   # InterceptAndInsertM3D2Textures(tag_sM3D2Header*, sExtraTextureTable*, char*)
  __Z8IsLetterP10sFileParse   # IsLetter(sFileParse*)
  __Z8IsNumberP10sFileParse   # IsNumber(sFileParse*)
  __Z9GetStringP10sFileParsePciiiii   # GetString(sFileParse*, char*, int, int, int, int, int)
  __Z9IsCommentP10sFileParse   # IsComment(sFileParse*)
  _gExtraTextureEntries
  _gExtraTextureTable
)
