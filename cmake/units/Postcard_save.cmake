# Unit v5906: Postcard::save, SetWorldBoundingArea, SetHeightfieldTexture (src/game/MeshLoad.cpp, src/game/ui/Postcard.cpp). The callees outside the unit are
# recording fakes in tests/unit_Postcard_save.cpp, which also defines the globals of other files.
set(TPW_UNIT_Postcard_save src/game/MeshLoad.cpp src/game/ui/Postcard.cpp platform/arena.cpp)
set(TPW_UNIT_Postcard_save_KEEP
  __Z20SetWorldBoundingAreaP15tag_sM3D2HeaderS0_   # SetWorldBoundingArea(tag_sM3D2Header*, tag_sM3D2Header*)
  __Z21SetHeightfieldTextureP15tag_sM3D2HeaderP18sExtraTextureTableiiiii   # SetHeightfieldTexture(tag_sM3D2Header*, sExtraTextureTable*, int, int, int, int, int)
  __ZN8Postcard4saveEPc   # Postcard::save(char*)
)
