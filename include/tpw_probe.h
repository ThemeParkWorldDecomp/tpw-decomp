// Frame probe: a few counters the translated frame code bumps and the launcher prints once
// a pass with --probe, to see which stages of a native frame run (nothing prints
// otherwise; the cost is an increment). Host-only; the Mac has no such thing.
#pragma once

#include <cstdint>

namespace tpw_probe {

enum Counter {
    kHFieldExecute,         // CHField::Execute ran its body
    kHFieldExecuteSkipped,  // CHField::Execute returned at once (no view or light change)
    kExecuteUnrendered,     // CRenderMemory::ExecuteUnrenderedPrimitives calls
    kRecordsListed,         // records it handed to ExternExecute
    kExternExecute,         // CDrawPrimitive::ExternExecute calls
    kGLDrawIndexed,         // COpenGLDrawPrimitive::DrawIndexedPrimitive calls
    kGLVertices,            // vertices those calls were given
    kPackTextures,          // CTextureCache::PackTextures calls
    kUploadToVRAM,          // CTextureCache::UploadToVRAM calls
    kHFieldTextures,        // pages GetTexturesUsed listed for CHField::Execute
    kHFieldCells,           // QuadSaveInfo records CHField::Execute wrote (cells drawn)
    kHFieldPrimitiveNull,   // CHField::Execute got no primitive from the buffer
    kTexQueued,             // entries CTextureCache::Prepare queued for packing
    kTexExecute,            // CTextureCache::Execute calls with a non-empty queue
    kPrepareCells,          // cells CHField::Prepare visited
    kPrepareHidden,         // of them, hidden (flag 1)
    kPrepareNoTexture,      // of them, with no texture
    kDacEarlyOut,           // CHField::DivideAndConquer found the camera's box wholly off the map
    // Values (Set, not counted): the last DivideAndConquer's camera box and the rows it left.
    kDacX0, kDacX1, kDacZ0, kDacZ1, kDacWidth, kDacDepth, kRowsFirst, kRowsEnd,
    kMeshPrepare,           // CMesh::Prepare calls
    kCalcRenderEntry,       // SRenderListEntry::CalcRenderEntry calls
    kLoadRideType,          // LoadRideType calls (lobby set-up)
    kInstanceRide,          // InstanceRide calls
    kInstanceRideFailed,    // of them, answered 0
    kMapLinkAdd,            // CMapWho::Link_Add calls
    kMapLinkNoNode,         // of them, refused for want of a free node
    kMapLinkDuplicate,      // of them, already linked
    kPrepareListLinks,      // links PrepareList prepared
    kGlobalList6,           // links in global list 6 when PrepareWorldspace starts
    kTexPrepare,            // CTextureCache::Prepare calls
    kTexPrepareNull,        // of them, with no entry
    kTexPrepareIgnored,     // of them, entries with the no-prepare flag
    kTexPrepareHeld,        // of them, entries already queued or on a page
    kMeshTexNull,           // CMesh::PreprocessTextures records with no texture or no entry
    kCount
};

inline uint32_t gCounters[kCount] = {};

// --probe also makes the texture loader say what it loads (one line a file, at set-up):
// the launcher installs the printer, the translated code only formats the line.
inline void (*gLog)(const char* text) = nullptr;

inline void Hit(Counter counter, uint32_t n = 1) { gCounters[counter] += n; }
inline void Set(Counter counter, int32_t value) { gCounters[counter] = static_cast<uint32_t>(value); }

}  // namespace tpw_probe
