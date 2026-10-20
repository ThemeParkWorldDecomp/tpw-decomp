// tb_meshdelay.h: the meshes whose preparation waits until the rest of the frame is prepared
// (engine library; CMeshDelay's members are at 0x1001ca64 and 0x1001ca98, in the
// inventory's tb_profiler.cpp range next to CMesh; the original file name is not in the
// binary). Who adds to the list is not translated yet (CMesh::Prepare, by the look of it).
#pragma once
#include <cstdint>

class CMesh;

class CMeshDelay {
public:
    // Forgets the meshes (CMapWho::RenderWindow, before anything is prepared).
    void Reset();        // 0x1001ca64
    // Sets mesh flag 0x10 on each mesh put off and prepares it; the count is read again
    // after every mesh, so meshes added meanwhile are prepared too. Returns 1.
    int32_t Prepare();   // 0x1001ca98

    CMesh* mMeshes[256];   // 0x000
    int32_t mCount;        // 0x400
};
// 0x404 bytes on the Mac (gRenderEntryFlags follows it).

extern CMeshDelay gMeshDelay;   // engine data 0x100b0100 (not exported; name invented)
