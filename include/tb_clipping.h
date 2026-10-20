// tb_clipping.h: the engine's polygon clipper (engine library; the file name is not known).
// CClipping's members are in src/tb_clipping.cpp (0x1003f4d4, 0x1003f6dc, 0x10040640..0x10040cf0),
// the edge clippers they call and the colour blend in src/tb_clipping_edges.cpp (0x1003f8b0..
// 0x10040424); Clipping_Init and Clipping_Close (0x1003f3bc, 0x1003f45c) are in tb_printf_opengl.cpp.
//
// CClipping was a two-word placeholder in tb_printf_opengl.h; that header now includes this
// one at the same place, so every includer sees the real class and the declarations the
// translated callers already use (the static CPP_TestData and the two ClipPoly overloads)
// are the ones below, unchanged. The object is allocated by Clipping_Init as eight bytes
// and has no constructor.
//
// Planes. Six planes in clip space (data at 0x10052008, four floats each, x y z w): a
// vertex is inside a plane when x*px + y*py + z*pz + w*pw >= 0. Plane 0 is y <= w, 1 is
// y >= -w, 2 is x <= w, 3 is x >= -w, 4 is z <= w and 5 is z >= 0. The "outside codes" of
// CPP_TestData and the masks ClipPoly takes use one bit per plane: bit 5 (0x20) is plane 0,
// bit 4 plane 1, bit 3 plane 2, bit 2 plane 3, bit 1 plane 4 and bit 0 (0x01) plane 5.
// ClipPoly clips against the planes whose bits are set, plane 5 first.
#pragma once
#include <cstdint>

struct SFVF_TLCTEX;
struct VertexClipList;
struct sAlignedBox;
class CMatrix;

struct CClipping {
    uint32_t mF00;   // 0x00  never written or read by the translated code
    float mMinZ;     // 0x04  CheckBoundingBox: the smallest z of the transformed box (0 if none is
                     //       below zero); CMesh::PreprocessTextures reads it

    // Outside codes of `count` vertices `stride` bytes apart, each ANDed with `mask`, one
    // byte each into `codes`; result[0] is the AND of them all and result[1] the OR. The
    // original is a member returning a four-byte structure through a hidden pointer (r3, with
    // `this` in r4, which it never reads); the other two bytes of that structure are stack
    // leftovers and are not written here. The Ghidra signature lost the last two arguments.
    // `clipping` is the CClipping, passed as a pointer.
    // A vertex is tested through its bit patterns (x, y, z, w at +0, +4, +8, +0xc), as signed
    // integers: with w not below zero the codes (bits 5..1) are y > w, y with its sign flipped
    // > w, x > w, x with its sign flipped > w and z > w, and bit 0 is the sign of z; with w below
    // zero they compare against |w| instead, and bit 1 tests the bits of z - 2w.
    static void CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices,
                             int32_t count, int32_t stride, uint32_t mask);   // 0x1003f6dc

    // Clips the polygon in `list` against the planes in `planes`, appending vertices at
    // vertices[count] (count grows). The list ends up holding the clipped polygon's vertex
    // numbers (it needs room for the growth). The original returns the new count minus two in
    // r3 (the triangles of a fan), which the declaration here, and the only caller, leave out.
    void ClipPoly(int32_t planes, SFVF_TLCTEX* vertices, VertexClipList* list, int32_t& count,
                  int32_t stride);   // 0x1004087c

    // Clips the polygon of `count` vertices at `in` against the planes in `planes`, using `out`
    // and `in` in turn as work space (so `in` needs room for the polygon's growth); the result
    // ends at `out` with `count` set. 0 when fewer than three vertices are left (the buffers
    // then hold the last pass), else 1.
    int32_t ClipPoly(int32_t planes, SFVF_TLCTEX* out, SFVF_TLCTEX* in, int32_t& count,
                     int32_t stride);   // 0x10040640

    // Clips the triangle whose three vertex numbers sit just before `indices` (indices[-3..-1])
    // through the global gei_Clipping (this is not read), appending new vertices at
    // vertices[count], and rewrites it in place as a fan of triangles: the first stays where it
    // was, the others follow it, written from the last backward. Returns the place after the
    // last triangle, `indices` moved by three entries per extra triangle, or back by the
    // vertices missing when fewer than three are left.
    uint16_t* ClipPoly(int32_t planes, uint16_t* indices, SFVF_TLCTEX* vertices, int32_t& count,
                       int32_t stride);   // 0x10040b24

    // Whether the box (grown by 5% on every side) may be seen through `matrix`: 0 when all its
    // eight corners are outside one plane, else the OR of the corners' outside codes with 0x40.
    // Also stores the smallest z of the transformed corners (or 0) in mMinZ.
    uint8_t CheckBoundingBox(const sAlignedBox& box, CMatrix& matrix);   // 0x1003f4d4
};
static_assert(sizeof(CClipping) == 8, "CClipping is eight bytes");

// The six edge clippers ClipPoly calls through a table (data at 0x10052068, one per plane in the
// order below; src/tb_clipping_edges.cpp). Each makes the vertex where the edge from `inside` to
// `outside` meets its plane and writes it at `out` (stride bytes; the stride after the 0x18 bytes
// of position, w, colour and specular colour must be a multiple of 8, or the original loops
// until it faults). Static functions of the original's file.
void VertexClip_Bottom(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);   // 0x1003f8b0
void VertexClip_Top(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);      // 0x1003fb68
void VertexClip_Right(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);    // 0x1003fd94
void VertexClip_Left(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);     // 0x1003ffc8
void VertexClip_Back(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);     // 0x100401f4
void VertexClip_Front(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride);    // 0x10040424

// Blends two colours (a, r, g, b bytes) by t/256 per channel pair: the colour of the edge clippers'
// new vertex, `from` at t = 0. 0x1003fae4
uint32_t InterpolateColour(uint32_t from, uint32_t to, uint32_t t);

// The working values the six edge clippers share (BSS; names invented).
extern int32_t gClipStep;               // 0x102b3618
extern int32_t gClipBytesLeft;          // 0x102b361c
extern double gClipFraction;            // 0x102b3620
extern double gClipInsideDistance;      // 0x102b3628
extern double gClipOutsideDistance;     // 0x102b3630
