# Unit w41b: the partial sprite draw and the sprite shadows (DrawPartial, CreateShadow, DrawRegularShadow, AddRegularShadow). The sprite
# classes' members that make a partial sprite, the texture entry, the shadow object, the sprite manager's frame members and the math
# unit are the real ones; the draw primitive, the texture cache's Prepare and Remove, the instance members of the regular draw and
# RotateVertex_Y are fakes in the unit file.
set(TPW_UNIT_SpritePartial src/tb_sprites_partial.cpp src/tb_sprites_shadow.cpp src/tb_spritefileio.cpp src/tb_spritelist_rest.cpp
    src/tb_rleload.cpp src/tb_textureentry.cpp src/tb_waveload.cpp src/tb_shadow.cpp src/tb_sprites.cpp src/MatrixPowerPC.cpp
    platform/arena.cpp)
