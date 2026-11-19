// Ps2Characters.cpp: the PS2 Guest Models mod's runtime (include/Ps2Characters.h, TPW_MOD_HOOK ps2_characters). Port
// code, no original behind it: the hooks in ESP_AddHLSpriteFromScript (src/game/HLSprite.cpp) and CSpriteManager
// (src/engine/SpriteManager.cpp) call it only with gModPs2Characters set.
//
// A frame: the script sprites are listed (Ps2CaptureSprite keeps, per listed record, the kid's sprite set, action, frame
// and place, and follows its walk for its heading); the sprite manager prepares the listed sprites (Ps2PrepareModels
// picks those drawn as models, near enough and captured with the record they show, and prepares their textures in the
// texture cache); the sprites are drawn with the models' left out, and the models after them (Ps2DrawModels): two of a
// clip's baked frames blended, lit by a fixed light, turned and scaled into the world and through the view's matrix, then
// added to one triangle list per texture page as the sprites are (clipped by AddPoly_TLCTEX, fogged, projected).
// A ride's riders' heads (the kidsheads and costumeheads sets): TickRideHeads gives each head's node matrix
// (Ps2NoteRideHead), and the head's model is the kid model's head (HeadPartOf) in the walk's first frame, turned by that
// matrix, as large as the head's picture and at its middle; a head with no matrix this frame stays a sprite.
// The staff's and entertainers' strike sign (SignTriangles) is left out of every clip but the strike's (ShowsSign).
// Debugging: TPW_PS2_DEBUG=1 prints what it loads and, once a second, the kids it draws; TPW_PS2_MIRROR=1 mirrors the
// models (the handedness check); TPW_PS2_BESIDE=1 keeps the sprites and draws each model beside its sprite (the facing
// and timing check); TPW_PS2_ACTION=N draws the staff and entertainers with action N's clip (the strike's, 5).
