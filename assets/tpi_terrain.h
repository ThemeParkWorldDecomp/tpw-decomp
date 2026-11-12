// tpi_terrain.h: the Theme Park Inc converter's (tpi_convert.h) repair of a world's terrain (the base.MD2 of
// terrain.wad), so that it draws in Theme Park World the way it did in Theme Park Inc.
//
// Theme Park Inc leaves two kinds of quad for its obstacles (levels/<world>/obstacles/*.wad: the lake, the bridges,
// the tunnels, the rocks; things the player pays to clear) to draw over, and Theme Park World draws neither:
//  - quads textured with a fully transparent texture (trans01.tga: alpha 0 everywhere) under the bridge obstacles,
//    which Theme Park Inc blended away so the river geometry below showed. Theme Park World draws height field textures
//    opaque (white), so these quads become hidden (bit 0 of the quad's flags, src/engine/tb_hfield.cpp kCellHidden),
//    as the river and its banks in base.MD2's own geometry already cover them (RepairTpiTerrain);
//  - hidden quads under the lake and tunnel obstacles, whose meshes draw the ground there. Theme Park World has no
//    obstacles, so their meshes become part of the terrain (MergeTpiObstacles), the way Theme Park World's own
//    terrains hold their fixed scenery (bridges, rocks, the castle): each obstacle as it is once the player has
//    cleared it, where its files tell that state. An obstacle whose Obstacle.ReplaceWithID names another wad's
//    Info.Id (the lake: the drained lake) is drawn as that one. One whose last animation (<name>m<N>.MD2, the
//    clearing's end) moves only a little of it (a tenth of its faces at most: the tunnels' and the building's
//    clearing crews, gates and boarding) is drawn without what moves; a bridge (its cleared shape leaves walkways, ','
//    or '@') is drawn posed as its animations leave it at their last frames: the deck raised, unfolded and turned
//    flat (compressed morphs, the place along a shape, rotation and scale keys); one whose animations move most of it
//    otherwise (the cliff, the rock, the swamp, the gates) is drawn as it is before clearing. The clearing crew, the
//    site props and the markers are left out. Its nodes are added to base.MD2 under its root. The cells nothing may be
//    built on are those of its .sam's Obstacle.AfterShape (the cleared state) when it is drawn cleared, else of its
//    Info.Shape, placed at Obstacle.ShapeOffsetX/Y; its closed cells that the mesh covers at ground level become
//    hidden quads, which the map takes as cells nothing goes on (src/game/park/TPHField.cpp HFieldCellStatus answers
//    0 for them, and src/game/park/MapCell.cpp CMapCell::Reset makes such a cell mode 7), as it does Theme Park
//    World's own scenery. Those quads are drawn again as geometry a little below the obstacle (Theme Park Inc drew the
//    ground under it), so the seams at the obstacle's edge show ground, not the clear colour. A bridge's walkway
//    cells over the river keep their hidden quads: the deck is drawn, but no path can be built across it.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "tpi_wad.h"

namespace tpw::assets {

struct TpiTerrainReport {
    int madeHidden = 0;                          // shown quads with a fully transparent texture, now hidden
    std::vector<std::string> clearTextures;      // the fully transparent textures the height field uses
};

// True for a 32-bit true colour TGA (image type 2, the only kind the game loads) whose alpha is 0 in every pixel.
bool TpiTgaFullyTransparent(const std::vector<uint8_t>& tga);

// Repairs the height field of a terrain mesh (an M3D2 with geometry and a height field) in place. `texture` gives the
// bytes of a texture by its file name as the mesh names it (case does not matter), or nullptr when there is none.
// A mesh without a height field is left as it is. False with a message for a mesh that cannot be read.
bool RepairTpiTerrain(std::vector<uint8_t>& md2,
                      const std::function<const std::vector<uint8_t>*(const std::string& name)>& texture,
                      TpiTerrainReport& report, std::string& err);

// One obstacle wad of a world, its members converted (TGA textures, tpi_convert.cpp ConvertMembers): <name>.sam,
// <name>.MD2, its animations (<name>m1.MD2 or <name>m.MD2), textures/ and stexture/.
struct TpiObstacle {
    std::string name;                     // the wad's name, for the report
    std::vector<TpiWadMember> members;
};

struct TpiObstacleResult {
    std::string name;
    std::string skipped;                  // why it is not merged ("not an obstacle", ...), empty when it is
    std::string state;                    // how it is drawn: "cleared", "cleared, posed", "replaced by <obstacle>",
                                          // "before clearing"
    int nodes = 0, faces = 0;             // the nodes and faces added to the terrain
    int blocked = 0;                      // blocked shape cells made hidden quads
    int alreadyHidden = 0;                // blocked shape cells whose quads were hidden already
    int open = 0;                         // blocked shape cells the mesh does not cover: left as they are
    std::vector<std::string> leftOut;     // the shown nodes left out (the crew, the props, the markers, moved nodes)
};

struct TpiObstacleReport {
    std::vector<TpiObstacleResult> obstacles;
    std::vector<std::string> addedTextures;     // the files added to terrain.wad ("textures/x.tga")
    std::vector<std::string> renamedTextures;   // "obstacle: old.tga -> new.tga", for names terrain.wad or the
                                                // shared textures hold with other pictures
    std::vector<std::string> missingTextures;   // names neither the obstacle, terrain.wad nor the shared textures hold
    std::vector<std::pair<int, int>> stillClear;   // hidden quads (x, y) that nothing covers after the merge
};

// Adds the obstacles' meshes to the terrain wad's base.MD2 (its members, converted) and their textures to its textures/
// and stexture/ folders (and their qickload.txt lists, as the mesh loader looks a texture up there first,
// src/game/MeshLoad.cpp OverrideTexturePaths), and hides the quads of their covered blocked cells. `shared` gives a
// shared texture's bytes by its path as terrain.wad would hold it ("textures/x.tga": the world's sharetex.wad,
// "stexture/x.tga": its ssharete.wad), nullptr when there is none. An obstacle wad without Info.IsObstacle 1, a mesh
// or a shape is skipped and reported. False with a message when the terrain or an obstacle's mesh cannot be read or the
// merged mesh would not fit the mesh format's 16-bit counts; the members are then left as they were.
bool MergeTpiObstacles(std::vector<TpiWadMember>& terrain, const std::vector<TpiObstacle>& obstacles,
                       const std::function<const std::vector<uint8_t>*(const std::string& path)>& shared,
                       TpiObstacleReport& report, std::string& err);

}  // namespace tpw::assets
