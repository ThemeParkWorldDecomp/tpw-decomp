# Unit w38: the sprite classes' remaining members (the RLE unpacking, the lists' Close and Prepare, Init(char*), the partial textures,
# Load(char*, int)). The texture entry and the wavelet loader are the real ones; the unit file labels every operator new itself.
set(TPW_UNIT_SpriteRest src/tb_spritefileio.cpp src/tb_rleload.cpp src/tb_spritelist_rest.cpp src/tb_textureentry.cpp src/tb_waveload.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_SpriteRest_OWN_CALLLOG ON)
