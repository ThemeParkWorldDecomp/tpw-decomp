// Test entry points for the OpenGL draw primitive's drawing members (library unit_OpenGLDraw:
// src/engine/DrawPrimitiveOpenGL.cpp). A test runs a script of operations on one draw primitive, three
// texture pages, a block of vertices and a block of indices; tests/test_OpenGLDraw.py runs
// the original on the same script and compares the result of every operation, the state of
// the draw primitive and the calls of everything outside the unit.
//
// Every OpenGL call is a fake that records its arguments (floats by their bits, a NaN as one
// value). The vertex arrays are recorded by contents: glLockArraysEXT dumps the vertices it
// locks (the colour words as the bytes GL reads, which must be the Mac's), glDrawElements the
// indices it draws, and the array pointers are recorded as offsets from the vertex pointer. The draw primitive's own setters, the math unit and the
// engine's error stack and profiler are fakes that record too.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"

using namespace calllog;

namespace {

constexpr int32_t kVertexCapacity = 64;   // vertices in the block
constexpr int32_t kIndexCapacity = 128;
constexpr int32_t kTextures = 3;

uint32_t gVertexWords[kVertexCapacity * 8];
uint16_t gIndexValues[kIndexCapacity];
alignas(4) uint8_t gPixel[8];                      // four bytes read by glReadPixels, four of guard

alignas(16) unsigned char gDrawStore[sizeof(COpenGLDrawPrimitive) + 16];
COpenGLDrawPrimitive* gDraw;
COpenGLTexture gPages[kTextures];

// The array pointers of the draw call in progress. glVertexPointer sets the base; when a call
// never sets it (a bad vertex type) the offsets are taken from the block itself.
const uint8_t* gBase;

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t canonical(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}

// A float argument as recorded.
uint32_t cbits(float value) { return canonical(fbits(value)); }

// An array pointer as its offset from the vertex pointer (or from the block when none was set).
int64_t offset_of(const void* pointer) {
    const uint8_t* base = gBase != nullptr ? gBase : reinterpret_cast<const uint8_t*>(gVertexWords);
    return static_cast<const uint8_t*>(pointer) - base;
}

// A colour word as GL reads it: its four bytes in memory order, the first as the high byte.
// The original's fake reads the same word from its big-endian memory, so the two sides agree
// when GL is given the Mac's bytes.
uint32_t memory_order(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) << 24 | static_cast<uint32_t>(bytes[1]) << 16 |
           static_cast<uint32_t>(bytes[2]) << 8 | bytes[3];
}

// The vertices [first, first + count) of the base, a NaN in a float place as one value. With
// `asRead`, the colour and specular words are the bytes GL reads (glLockArraysEXT); without,
// the words as the engine holds them (the math unit's Project).
void dump_vertices(const char* name, int32_t first, int32_t count, bool asRead = false) {
    if (gBase == nullptr || count <= 0 || count > 256) return;
    for (int32_t i = first; i < first + count; ++i) {
        uint32_t w[8];
        const uint8_t* vertex = gBase + 0x20 * i;
        std::memcpy(w, vertex, sizeof w);
        if (asRead) {
            w[4] = memory_order(vertex + 0x10);
            w[5] = memory_order(vertex + 0x14);
        }
        ev(name, {i, canonical(w[0]), canonical(w[1]), canonical(w[2]), canonical(w[3]), w[4], w[5], canonical(w[6]),
                  canonical(w[7])});
    }
}

// The indices a draw call reads, eight to an event.
void dump_indices(const uint16_t* indices, int32_t count) {
    if (indices == nullptr || count <= 0 || count > 1024) return;
    for (int32_t i = 0; i < count; i += 8) {
        int64_t v[8];
        for (int32_t k = 0; k < 8; ++k) v[k] = i + k < count ? indices[i + k] : -1;
        ev("Index", {i, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]});
    }
}

// The draw call's mode when it is the object's address (the primitive type was not 3 or 4).
int64_t mode_of(GLenum mode) {
    return mode == static_cast<GLenum>(reinterpret_cast<uintptr_t>(gDraw)) ? -1 : static_cast<int64_t>(mode);
}
}  // namespace

// ---- OpenGL ----------------------------------------------------------------------------------
extern "C" {
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    gBase = static_cast<const uint8_t*>(pointer);
    ev("glVertexPointer", {size, type, stride, lab(pointer)});
}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    ev("glTexCoordPointer", {size, type, stride, offset_of(pointer)});
}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer) {
    ev("glColorPointer", {size, type, stride, offset_of(pointer)});
}
void glLockArraysEXT(GLint first, GLsizei count) {
    ev("glLockArraysEXT", {first, count});
    dump_vertices("Vertex", first, count, true);
}
// A script may make the draw leave a DirectDraw error behind (queue DDResult, -1: no change).
void glUnlockArraysEXT() {
    ev("glUnlockArraysEXT");
    const int64_t result = ret("DDResult", -1);
    if (result >= 0) gDDResult = static_cast<int32_t>(result);
}
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    ev("glDrawElements", {mode_of(mode), count, type, lab(indices)});
    dump_indices(static_cast<const uint16_t*>(indices), count);
}
void glFinish() { ev("glFinish"); }
void glPushAttrib(GLbitfield mask) { ev("glPushAttrib", {mask}); }
void glPopAttrib() { ev("glPopAttrib"); }
void glEnable(GLenum cap) { ev("glEnable", {cap}); }
void glDisable(GLenum cap) { ev("glDisable", {cap}); }
void glDepthMask(uint8_t flag) { ev("glDepthMask", {flag}); }
void glBlendFunc(GLenum source, GLenum dest) { ev("glBlendFunc", {source, dest}); }
void glAlphaFunc(GLenum func, GLfloat ref) { ev("glAlphaFunc", {func, cbits(ref)}); }
void glFlush() { ev("glFlush"); }
GLenum glGetError() {
    ev("glGetError");
    return static_cast<GLenum>(ret("glGetError", 0));
}
void glDrawBuffer(GLenum mode) { ev("glDrawBuffer", {mode}); }
void glBegin(GLenum mode) { ev("glBegin", {mode}); }
void glEnd() { ev("glEnd"); }
void glColor4ubv(const uint8_t* components) {
    ev("glColor4ubv", {components[0], components[1], components[2], components[3]});
}
void glTexCoord2f(GLfloat s, GLfloat t) { ev("glTexCoord2f", {cbits(s), cbits(t)}); }
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { ev("glVertex4f", {cbits(x), cbits(y), cbits(z), cbits(w)}); }
void glPixelStorei(GLenum pname, GLint param) { ev("glPixelStorei", {pname, param}); }
void glReadBuffer(GLenum mode) { ev("glReadBuffer", {mode}); }
// Writes the four bytes of the queue's pixel, red first, where the pixel goes.
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {
    ev("glReadPixels", {x, y, width, height, format, type, lab(pixels)});
    const uint32_t value = static_cast<uint32_t>(ret("Pixel", 0));
    const uint8_t bytes[4] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                              static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    std::memcpy(pixels, bytes, 4);
}
}  // extern "C"

// ---- The engine and the platform around the unit ------------------------------------------------
void SamsUtilities::UDebug(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int argument = va_arg(arguments, int);
    va_end(arguments);
    ev("UDebug", {argument}, {format});
}

int32_t gDDResult;
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
CDisplay::CDisplay() {}
CDisplay gei_Display;
CProfiler gProfiler;
int32_t CProfiler::GetClockCycleCount10h() {
    ev("Clock");
    return static_cast<int32_t>(ret("Clock", 0));
}
uint32_t gOpenGLBlendModes[12];

// The math unit: records the corners it is asked to project and flips a bit in the
// position of each, so that what is drawn afterwards shows that it was called first.
CTransformBase::~CTransformBase() {}
namespace {
class FakeMathUnit : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void*, void*, int32_t, int32_t, int32_t, int32_t) override {}
    float Project(void* vertices, int32_t stride, int32_t count, int32_t type) override {
        ev("Project", {stride, count, type});
        const uint8_t* saved = gBase;
        gBase = static_cast<uint8_t*>(vertices);
        dump_vertices("ProjectVertex", 0, count);
        gBase = saved;
        for (int32_t i = 0; i < count; ++i) {
            uint32_t w[4];
            uint8_t* vertex = static_cast<uint8_t*>(vertices) + stride * i;
            std::memcpy(w, vertex, sizeof w);
            for (uint32_t& word : w) word ^= 0x00200000u;
            std::memcpy(vertex, w, sizeof w);
        }
        return 0.0f;
    }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMathUnit gMathUnit;
}  // namespace
CTransformBase* gei_MathUnit = &gMathUnit;

// ---- The draw primitive's other members and the texture page ---------------------------------------
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
COpenGLDrawPrimitive::COpenGLDrawPrimitive() {}
COpenGLDrawPrimitive::~COpenGLDrawPrimitive() {}
int32_t COpenGLDrawPrimitive::Init() { return 0; }
int32_t COpenGLDrawPrimitive::SetBlendingMode(int32_t mode) {
    ev("SetBlendingMode", {lab(this), mode});
    return static_cast<int32_t>(ret("SetBlendingMode", 0));
}
int32_t COpenGLDrawPrimitive::SetTexture(int32_t stage, void* texture) {
    ev("SetTexture", {lab(this), stage, lab(texture)});
    return static_cast<int32_t>(ret("SetTexture", 0));
}
int32_t COpenGLDrawPrimitive::ExecuteShadow(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::SetFogParameters(float, float, uint32_t) { return 0; }
int32_t COpenGLDrawPrimitive::BeginDraw() {
    ev("BeginDraw", {lab(this)});
    return static_cast<int32_t>(ret("BeginDraw", 0));
}
int32_t COpenGLDrawPrimitive::EndDraw() {
    ev("EndDraw", {lab(this)});
    return static_cast<int32_t>(ret("EndDraw", 0));
}
int32_t COpenGLDrawPrimitive::SupportsOnePassMultiTexture() { return 0; }

// Pages are only identified (dynamic_cast from the CTextureBase DrawFilledBox is given).
CTextureBase::~CTextureBase() {}
COpenGLTexture::COpenGLTexture() {}
COpenGLTexture::~COpenGLTexture() {}
double COpenGLTexture::TestUploadSpeed() { return 0.0; }
int32_t COpenGLTexture::Create(int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::Destroy() { return 0; }
int32_t COpenGLTexture::LockSurface() { return 0; }
int32_t COpenGLTexture::UnlockSurface() { return 0; }
int32_t COpenGLTexture::Download(CTextureEntry*, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::MipmapsUsed() { return 0; }
int32_t COpenGLTexture::GetPixelCount() { return 0; }
void COpenGLTexture::GetTexture() {}

namespace {
// An operation argument as unsigned bits, and as the float those bits hold.
uint32_t from_bits_u(int32_t value) { return static_cast<uint32_t>(value); }
float from_bits(int32_t value) {
    float result;
    const uint32_t bits = from_bits_u(value);
    std::memcpy(&result, &bits, 4);
    return result;
}

// The operations of tests/test_OpenGLDraw.py, with the arguments each reads from a[].
enum Op : int32_t {
    kExecute = 1, kDrawIndexed, kFilledBox, kPoly, kEdgeBox, kTestPoly, kDetectPoly, kGetPixel, kFillRate, kWire, kProfileBox,
    kSetField = 20, kSetResult, kSetDisplay,
};

// A float result as compared: its bits, a NaN as one value.
int64_t result_bits(float value) { return cbits(value); }
}  // namespace

extern "C" {

// Where the labelled objects are: 1 the draw primitive, 7 the vertices, 8 the indices, 10 the
// pixel bytes, 21.. the pages (as COpenGLTexture).
void* tpw_od_object(int32_t label) {
    switch (label) {
    case 1: return gDraw;
    case 7: return gVertexWords;
    case 8: return gIndexValues;
    case 10: return gPixel;
    default:
        if (label >= 21 && label < 21 + kTextures) return &gPages[label - 21];
        return nullptr;
    }
}

void tpw_od_reset() {
    if (gDraw != nullptr) gDraw->~COpenGLDrawPrimitive();
    std::memset(gDrawStore, 0xa5, sizeof gDrawStore);
    gDraw = new (gDrawStore) COpenGLDrawPrimitive;
    for (uint32_t& field : gDraw->mFields) field = 0xa5a5a5a5u;
    gDraw->mTexture = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
    gDraw->mPasses = 0;
    gDraw->mF10 = 0;
    gDraw->mF14 = 0;
    gDDResult = 0;
    gBase = nullptr;
    std::memset(gPixel, 0xa5, sizeof gPixel);
    for (uint32_t i = 0; i < 12; ++i) gOpenGLBlendModes[i] = 0x7000 + i;
    gei_Display.mTwoOverWidth = 0.0f;
    gei_Display.mTwoOverHeight = 0.0f;
    gei_Display.mWidth = 0;
    gei_Display.mHeight = 0;
}

// which 0: the vertex words (8 a vertex); 1: the indices (one per value).
void tpw_od_data(int32_t which, const uint32_t* values, int32_t count) {
    if (which == 0) {
        for (int32_t i = 0; i < count && i < kVertexCapacity * 8; ++i) gVertexWords[i] = values[i];
    } else {
        for (int32_t i = 0; i < count && i < kIndexCapacity; ++i) gIndexValues[i] = static_cast<uint16_t>(values[i]);
    }
}

// Runs one operation. The arguments a[] are described with the test.
int64_t tpw_od_op(int32_t op, const int32_t* a) {
    gBase = nullptr;
    switch (op) {
    case kExecute: {
        // a: mode, texture (-1 none, 0.. a page, 9 an unlabelled address), type, vertex type, vertices, indices
        SPrimitive primitive;
        std::memset(&primitive, 0xa5, sizeof primitive);
        primitive.mFlags = 0x21;
        primitive.mBlendingMode = from_bits_u(a[0]);
        primitive.mTexture = a[1] >= 0 && a[1] < kTextures ? static_cast<void*>(&gPages[a[1]])
                                                          : (a[1] == 9 ? reinterpret_cast<void*>(0x1234) : nullptr);
        primitive.mPrimitiveType = static_cast<int32_t>(from_bits_u(a[2]));
        primitive.mVertexFormat = from_bits_u(a[3]);
        primitive.mVertexCount = a[4];
        primitive.mIndexCount = a[5];
        primitive.mVertexData = reinterpret_cast<uint8_t*>(gVertexWords);
        primitive.mIndexData = gIndexValues;
        return gDraw->Execute(&primitive);
    }
    case kDrawIndexed:
        // a: type, vertex type, vertex count, index count, flags
        return gDraw->DrawIndexedPrimitive(a[0], from_bits_u(a[1]), gVertexWords, from_bits_u(a[2]), gIndexValues,
                                           from_bits_u(a[3]), from_bits_u(a[4]));
    case kFilledBox: {
        // a: x1, y1, x2, y2 (bits), colour, page (-1 none)
        CTextureBase* page = a[5] >= 0 && a[5] < kTextures ? static_cast<CTextureBase*>(&gPages[a[5]]) : nullptr;
        return gDraw->DrawFilledBox(from_bits(a[0]), from_bits(a[1]), from_bits(a[2]), from_bits(a[3]), a[4], page);
    }
    case kPoly:
        // a: count, colour, scale (bits)
        return gDraw->DrawPoly(reinterpret_cast<XZType*>(gVertexWords), a[0], a[1], from_bits(a[2]));
    case kEdgeBox: return gDraw->DrawEdgeBox();
    case kTestPoly: return gDraw->DrawTestPoly();
    case kDetectPoly:
        // a: size (bits), colour, unused, u0, v0, u1, v1 (bits)
        return gDraw->DrawDetectPoly(from_bits(a[0]), a[1], a[2], from_bits(a[3]), from_bits(a[4]), from_bits(a[5]),
                                     from_bits(a[6]));
    case kGetPixel:
        // a: x, y
        return gDraw->GetPixel(a[0], a[1], reinterpret_cast<uint32_t*>(gPixel));
    case kFillRate: return result_bits(gDraw->GetPixelFillRate());
    case kWire:
        // a: unused, faces
        return gDraw->DrawWiredframedBoundingBox(reinterpret_cast<SVertex*>(gVertexWords), a[0], a[1]);
    case kProfileBox: return gDraw->DrawProfileBox();
    case kSetField: gDraw->mFields[a[0] & 15] = from_bits_u(a[1]); return 0;
    case kSetResult: gDDResult = a[0]; return 0;
    case kSetDisplay:
        // a: 2 / width and 2 / height (bits), width, height
        gei_Display.mTwoOverWidth = from_bits(a[0]);
        gei_Display.mTwoOverHeight = from_bits(a[1]);
        gei_Display.mWidth = a[2];
        gei_Display.mHeight = a[3];
        return 0;
    }
    return -99;
}

// The state compared after every operation: the draw primitive's words, the DirectDraw
// result, the pixel bytes and a sum of each block.
int32_t tpw_od_state(int64_t* out, int32_t capacity) {
    int32_t count = 0;
    auto put = [&](int64_t value) {
        if (count < capacity) out[count] = value;
        ++count;
    };
    for (uint32_t field : gDraw->mFields) put(field);
    put(gDraw->mTexture == reinterpret_cast<void*>(static_cast<intptr_t>(-1)) ? -1 : (gDraw->mTexture == nullptr ? 0 : 1));
    put(gDraw->mPasses);
    put(gDraw->mF10);
    put(gDraw->mF14);
    put(gDDResult);
    for (int32_t i = 0; i < 4; ++i) put(gPixel[i]);
    uint32_t sum = 0;
    uint32_t mix = 0;
    for (uint32_t word : gVertexWords) {
        sum += word;
        mix = mix * 31u + word;
    }
    put(sum);
    put(mix);
    sum = 0;
    for (uint16_t value : gIndexValues) sum = sum * 31u + value;
    put(sum);
    return count;
}

}  // extern "C"
