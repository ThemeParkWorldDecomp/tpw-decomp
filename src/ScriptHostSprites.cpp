// The script sprites' host records (host only, no Mac code; unit v5L05, 2026-10-20).
//
// A script block (tagSCRIPT, 0x118 bytes) keeps the sprite it draws as an SSpriteGameData embedded at +0xd0 in the Mac's
// layout (0x44 bytes, 32-bit pointers): the block is the save file's record, so it keeps that layout. ESP_AddHLSpriteFromScript
// fills those bytes as the Mac does, then lists the script's host record from here instead of the embedded one: the sprite
// engine reads the host's SSpriteGameData, whose pointers are 8 bytes (mSprite at 0x28..0x2f, mF2C at 0x30), so the embedded
// bytes read as one would make mSprite of the Mac's mSprite and mF2C together. SCR_WhichScriptHasThisSprite matches the
// host record too (the hot sprite the peep picker asks about is the one that was drawn).
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#include "HLSprite.h"

namespace {

// Keyed by the block's address; a block freed and allocated again at the same address gets the same record, which the adder
// fills again before listing it. The records are never freed (one for each block address the game has used).
std::unordered_map<const tagSCRIPT*, SSpriteGameData*>& Table() {
    static std::unordered_map<const tagSCRIPT*, SSpriteGameData*> table;
    return table;
}

}  // namespace

SSpriteGameData* ESP_ScriptHostSprite(const tagSCRIPT* script) {
    SSpriteGameData*& sprite = Table()[script];
    if (sprite == nullptr) sprite = static_cast<SSpriteGameData*>(std::calloc(1, sizeof(SSpriteGameData)));
    return sprite;
}

bool ESP_IsScriptHostSprite(const tagSCRIPT* script, const void* sprite) {
    const auto& table = Table();
    const auto found = table.find(script);
    return found != table.end() && found->second == sprite;
}
