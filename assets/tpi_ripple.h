// tpi_ripple.h: the lapping waves round the Theme Park Inc islands of the island screen (tpi_island.h), and the texture
// animation files they and the extended lobby sea (tpi_lobby_sea.h) play.
//
// How the original islands draw them (lobby.wad, data/lobby/terrain/): each <x>_isle.MD2 has a band of quads laid by hand
// along its water line, textured "Ripple.tga" (material 0x33, see-through; textures/Ripple.wct and stexture/Ripple.wct).
// The band's inner edge is on the island's foot, its outer edge a few units out, flat at the foot's height; each quad
// maps the texture once, u 0 to 1 along the shore and v 0 at the island to 1 at the outer edge. The island's animations
// <x>_isleM1.MD2 (100 frames) and <x>_isleM2.MD2 (200 frames), played one after the other at random by the island
// screen (src/game/online/lobby_island.cpp), are texture coordinate animations: every corner of the band goes from its
// (u, v) to (u - 1, v - 1) over 100 frames, so the wave bands drift towards the island at the same speed in both.
//
// The converter makes the same for each Theme Park Inc island: RippleRing lays the band round the slab's foot, with
// the originals' width and quad length measured from the player's own Lost Kingdom island (MeasureRipple), and
// WriteUvAnim writes the two animations with the originals' frame counts and drift. The texture is the player's own
// Ripple.wct, copied as it is. Nothing of the originals is in this file but the format.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "tpi_island_md2.h"

namespace tpw::assets::tpi_island {

// One quad of the band: corners inner a, inner b, outer b, outer a (a before b along the loop), with the originals'
// coordinates: (0,0), (1,0), (1,1), (0,1).
struct RippleQuad {
    Vec3 p[4];
    float u[4], v[4];
};

// The band round a closed outline (the points in order, either way round, the last joined to the first; y taken from
// the outline): each side is cut into pieces of about `quadLength`, the outer edge lies `width` out from the inner one
// along the outline's outward normals (mitred at the corners, so the outer edge stays parallel to each side). Empty when
// the outline has fewer than three points, no area, or width or quadLength are not positive.
std::vector<RippleQuad> RippleRing(const std::vector<Vec3>& outline, float width, float quadLength);

// The band as a mesh node named `name`, every face in texture slot `texture`, the faces facing up.
Md2Node RippleNode(const std::vector<RippleQuad>& quads, uint16_t texture, const std::string& name);

// ---- texture coordinate animations (the M1/M2 files of a mesh) --------------------------------------------------------

// One corner's keys: (frame, u, v), absolute coordinates.
struct UvKey {
    uint16_t frame = 0;
    float u = 0, v = 0;
};

// A node animation that moves texture coordinates only (tag_sNodeAnim with flag 0x10000 and a sUVAnim): the record as
// the file has it (its pointers and its own index are rewritten by the writer; the originals differ in words 8 and 0xc,
// so a new one is made from one of theirs) and one track per corner of the node, track k for corner k.
struct UvNodeAnim {
    std::array<uint8_t, 0x40> record{};
    std::vector<std::vector<UvKey>> tracks;
    uint16_t Node() const { return static_cast<uint16_t>(record[0x14] | (record[0x15] << 8)); }
    void SetNode(uint16_t fileIndex) {
        record[0x14] = static_cast<uint8_t>(fileIndex);
        record[0x15] = static_cast<uint8_t>(fileIndex >> 8);
    }
};

struct UvAnim {
    uint32_t f00 = 0x10, f04 = 0, frames = 0;   // tag_sAnimHeader's first words (0x10 to 0x14 in the originals, 0), the length
    std::vector<UvNodeAnim> nodes;
    // From the reader: whether the file holds anything but these (shapes, turns, scales, paths, morphs, animated
    // textures, enables or node animations without a texture track). WriteUvAnim never writes such things.
    bool other = false;
};

// Reads the texture coordinate tracks of an animation file (version 0xdd mesh header, tag_sAnimHeader at 0x98). Node
// animations without texture tracks are left out and set `other`. False with a message for a file that is not one.
bool ReadUvAnim(const std::vector<uint8_t>& bytes, UvAnim& anim, std::string& err);

// The animation file for the mesh `meshBytes` (a WriteMd2 or original mesh): the mesh's header words (counts and box),
// its name replaced by `name`, then the tracks laid out as the originals' are. The mesh's own tables are not repeated.
std::vector<uint8_t> WriteUvAnim(const std::vector<uint8_t>& meshBytes, const std::string& name, const UvAnim& anim);

// The index the engine gives model node `node` in a file WriteMd2 writes (full nodes first, then minis, each in the
// model's order); node animations name nodes by it. -1 for an index out of range.
int FileNodeIndex(const Md2Model& model, size_t node);

// The look of the originals' waves, measured from one original island and its two animations.
struct RippleStyle {
    float width = 0;         // mean distance across the band, square to its inner edge
    float quadLength = 0;    // mean length of a quad's inner edge
    uint16_t frames[2] = {0, 0};          // the two animations' lengths
    float drift[2][2] = {{0, 0}, {0, 0}};  // (du, dv) over each animation
    std::string texture;     // the band's texture name in the island ("Ripple.tga")
    // Each animation's header words and its band's node animation record, the pattern for new ones.
    UvAnim anim[2];
    std::array<uint8_t, 0x40> record[2]{};
};

// Finds the band (the root node's largest face set whose texture has material 0x33 and a name starting "ripple", case
// aside) in `isle` and measures it and the drift of its corners in `m1` and `m2`. False with a message when any of it is missing
// or the corners do not all drift alike.
bool MeasureRipple(const std::vector<uint8_t>& isle, const std::vector<uint8_t>& m1, const std::vector<uint8_t>& m2,
                   RippleStyle& style, std::string& err);

}  // namespace tpw::assets::tpi_island
