// HLSprite.cpp (main app 0x1007c790): the end of the people sprites. ESP_InitialiseAndLoadSprites (src/ESP_sprites.cpp) made them.
#include "ESP_sprites.h"
#include "HLSprite.h"

// Gives back every sprite set the loader made: the sprite file's data (CSpriteFileIO::Free) and the file object, for the sets
// that loaded, then the set itself; then the sixteen sprite lists (4 x 4) and the global list are closed. gESPSpriteCount and the
// pointer array are left as they are. The sets are not tested for NULL, and the first Free runs on the file pointer whatever it
// holds (the Mac tests only the second); the file pointer is read again after the Free, as the Mac does.
void ESP_RemoveSprites() {
    for (int32_t i = 0; i < gESPSpriteCount; ++i) {
        ESP_CSprite* sprite = gESPSprites[i];
        if (sprite->mLoaded != 0) {
            sprite->mFile->Free();
            CSpriteFileIO* file = sprite->mFile;
            if (file != nullptr) {
                file->Free();
                delete file;
            }
            sprite->mFile = nullptr;
        }
        ESP_CSprite* again = gESPSprites[i];
        if (again != nullptr) delete again;
    }
    for (int32_t row = 0; row < 4; ++row)
        for (int32_t column = 0; column < 4; ++column) gHLSpriteLists[row * 4 + column].Close();
    gESPGlobalList.Close();
}
