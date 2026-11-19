// PortGroundPool.h: the sizes of the far ground's procedural textures (the port's fix groundpool, src/engine/tb_init.cpp and
// CHField::Prepare in src/engine/tb_hfield.cpp). When more of the procedural window is in view than the handler's ring of
// tiles holds, the nearest cells keep the ring's full size tiles and every other cell in view gets a tile of the pool, made
// at full size and shrunk to the size the cell covers on the screen: full, a half, a quarter or an eighth across, the
// smallest that still has a texel for each pixel. When the cells in view would need more than the pool's budget, all of them
// step down together. Header only and pure, so the host test (tests/host_ground_pool.cpp) sees what the game does.
#pragma once

