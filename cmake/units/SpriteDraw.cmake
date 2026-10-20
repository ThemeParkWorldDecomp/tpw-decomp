# The regular sprite draw path (unit w41a): the new sources under the real sprite manager, DrawSprite and AddPoly_TLCTEX, the clipper and the
# texture entry. The unit file includes calllog.cpp itself (below) for its allocation observer.
set(TPW_UNIT_SpriteDraw src/tb_sprites_draw.cpp src/tb_rotatevertex.cpp src/tb_drawprimitive_addsprite.cpp src/tb_sprites.cpp
    src/tb_drawprimitive_sprite.cpp src/tb_clipping.cpp src/tb_clipping_edges.cpp src/tb_textureentry.cpp platform/arena.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_SpriteDraw_OWN_CALLLOG ON)
