# The ESP sprite loader (unit w33): its file list, sorter, loader and the sprite lists' set-up; the lists' array is HLSprite.cpp's.
set(TPW_UNIT_ESPSprites src/ESP_sprites.cpp src/HLSprite.cpp src/TPGlobals.cpp src/TPGlobals_Dtor_TPGlobals.cpp platform/arena.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_ESPSprites_OWN_CALLLOG ON)
