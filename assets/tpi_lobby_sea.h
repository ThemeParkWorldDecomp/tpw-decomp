// tpi_lobby_sea.h: the island screen's sea, made larger for the TPI Worlds and Rides mod (tpw-mods tpi-worlds;
// TPW_MOD_HOOK tpi_islands, include/TpiWorlds.h).
//
// The original sea (lobby.wad, data/lobby/terrain/Base.MD2) is a disc about 528 in radius round (550.5, 553.1): a
// see-through surface "Sea" (texture jri_lak3, material 0x33) flat at y 0 over an opaque bowl "Sea_Under" (jri_lak2)
// whose rim comes up to just under it. Both are mapped by a planar projection (u and v are affine in x and z), and
// Basem1.MD2 slides every corner's (u, v) by (-1, -1) over 200 frames. The island screen's camera wanders over
// x and z 100 to 900 (lobby_island.cpp) and draws to 1000 ahead (LBLobbyView.cpp), so from the box's corners, and from
// the second ring of islands the mod adds at z 930, the disc's edge (and the sky past it) can come into view.
//
// The converter writes a copy of both files with each disc carried on outwards in rings, flat at the height of its
// edge, out to LobbySeaRadius: every new corner gets the same planar mapping (fitted from the node's own corners,
// so the texture runs on without a seam or a stretch) and the same slide in the animation, and the rings are as
// fine as the original's (about 117 apart, sides no longer than about 140), so the per-vertex fog falls the same
// way. Everything else in the files (the height field, the textures, the nodes' order and the embedded record) is
// kept. The game loads the copy only with the mod on (TPHField.cpp, LoadLevelHeightfield).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tpi_island_md2.h"

namespace tpw::assets::tpi_island {

// Where the island screen's camera can be, in x and z: the wandering camera's box (lobby_island.cpp, mRoamCentre +-
// mRoamExtent, 100 to 900), which holds the four original islands and the circle the browsing camera flies round each
// (70 out, lobby.txt's spin radius), and, with the mod on, the circles round the second ring's islands (z 930,
// include/tpi_island_ring.h), up to z 1000. The camera only glides in straight lines between such places, so it stays
// in this box. And how far it sees: the projection's far plane (LBLobbyView.cpp).
constexpr float kLobbyReachMinX = 100.0f, kLobbyReachMaxX = 900.0f, kLobbyReachMinZ = 100.0f, kLobbyReachMaxZ = 1000.0f;
constexpr float kLobbyFarPlane = 1000.0f;
// How far from the camera a point at the far plane's depth can be: the view's corners lie off its axis, at most this
// many times the depth. The island screen's field of view is lobby.txt's ISLANDFOV (100) or LBLobbyView.cpp's 115 (the
// same tangent both ways, tb_matrix.cpp Projection), the frame 4:3 (a wider window letterboxes it, platform/fit.h): at
// 115 degrees a corner's direction is (2.09, 1.57, 1) over the depth, 2.8 times as long.
constexpr float kLobbyViewSlant = 3.0f;
// How far past the reach the sea goes.
constexpr float kLobbySeaMargin = 150.0f;
// The rings: this far apart (the original disc's rings are 117.3 apart), each side at most this long (the original's
// outer sides are 137 long).
constexpr float kLobbySeaStep = 117.3f, kLobbySeaMaxSide = 140.0f;

// The radius round the sea's centre (x, z) that keeps the sea in every view: the farthest corner of the camera's
// box (kLobbyReach*), plus the farthest the view reaches (the far plane times kLobbyViewSlant), plus the margin.
float LobbySeaRadius(float centreX, float centreZ);

// One ring of the extension: its radius and how many corners it has.
struct SeaRing {
    float radius = 0;
    int sides = 0;
};
// The rings from an edge of `edgeSides` corners at `edgeRadius` out to `outerRadius` (the last ring is at least that
// far): `step` apart, the number of corners doubled whenever a side would be longer than `maxSide`.
std::vector<SeaRing> SeaRingPlan(float edgeRadius, int edgeSides, float outerRadius, float step, float maxSide);

// u = a[0] + a[1] x + a[2] z (and the same for v), in the node's frame.
struct UvPlane {
    double u[3] = {0, 0, 0}, v[3] = {0, 0, 0};
    float U(float x, float z) const { return static_cast<float>(u[0] + u[1] * x + u[2] * z); }
    float V(float x, float z) const { return static_cast<float>(v[0] + v[1] * x + v[2] * z); }
};

// Carries a node's disc on outwards to `outerRadius` round its frame's origin (x 0, z 0): the corners on the edge
// (the edges only one face uses) must form one loop round the origin, all at the same height; the node's texture
// coordinates must be planar (within 0.01). The new corners and faces go after the old ones, in the node's last
// face set, wound and lit (corner normals straight up) as the old faces are. False with a message, the node unchanged,
// otherwise. `plane` gets the fitted mapping, `firstNew` the first new corner.
bool ExtendSeaNode(Md2Node& node, float outerRadius, UvPlane& plane, size_t& firstNew, std::string& err);

// The larger sea: `base` and `anim` are the original Base.MD2 and Basem1.MD2. Every geometry node (Sea_Under and Sea)
// is extended to LobbySeaRadius of the disc's centre in the lobby's world; the animation gets a track for every new
// corner, its keys those of the node's other corners (they must all slide alike). False with a message otherwise.
bool ExtendLobbySea(const std::vector<uint8_t>& base, const std::vector<uint8_t>& anim, std::vector<uint8_t>& outBase,
                    std::vector<uint8_t>& outAnim, std::string& err);

// The embedded records of a mesh file (the 0x14-byte records the header's words 0x48 and 0x7c count and point to),
// as bytes; empty when there are none or they hold pointers (WriteMd2 drops them; only records without pointers are
// carried over).
bool ReadEmbeddedRecords(const std::vector<uint8_t>& mesh, std::vector<uint8_t>& records, uint16_t& count, std::string& err);
// Appends the records to a WriteMd2 file and points the header at them.
void AppendEmbeddedRecords(std::vector<uint8_t>& mesh, const std::vector<uint8_t>& records, uint16_t count);

}  // namespace tpw::assets::tpi_island
