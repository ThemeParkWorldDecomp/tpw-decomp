// Ps2Characters.h: the PS2 Guest Models mod (tpw-mods ps2-characters, TPW_MOD_HOOK ps2_characters). With the hook on,
// the park's children (the eight kids and the four theme world costume kids), staff and entertainers are drawn as the
// PlayStation 2 edition's 3D models instead of their sprites: the packs the launcher's converter wrote from the player's
// own PS2 disc (include/Ps2Pack.h, assets/ps2_convert.h), blended between two baked frames on the CPU, lit, and drawn through the
// engine's own 3D path (CDrawPrimitive) in the sprite pass, depth tested with the park. Draw only: nothing here calls the
// game's random numbers or writes the game's state; every choice a person's model makes (which idle, when) comes from a
// hash of its sprite record's address and the steady clock. The sprite stays for anything without a model, far away,
// or not captured this frame; its blob shadow and the pointer's pick are the sprite's, unchanged.
// The runtime is src/game/Ps2Characters.cpp; the pure parts below are tested by tests/host_ps2_characters.cpp.
#pragma once

