// OpenGL 1.1 entry points the engine calls directly (OpenGLLibrary on the Mac; the
// engine imports 48 of them). Declarations as in gl.h, for the subset translated so far;
// the platform layer provides them (a real OpenGL compatibility context). They have C
// linkage, so in the native build they bind straight to the host's OpenGL library.
#pragma once
#include <cstdint>

using GLenum = uint32_t;
using GLbitfield = uint32_t;
using GLint = int32_t;
using GLsizei = int32_t;
using GLuint = uint32_t;
using GLfloat = float;

enum : GLenum {
    GL_ENABLE_BIT = 0x00002000,
    GL_FOG = 0x0b60,
    GL_DEPTH_TEST = 0x0b71,
    GL_ALPHA_TEST = 0x0bc0,
    GL_BLEND = 0x0be2,
    GL_UNPACK_ALIGNMENT = 0x0cf5,
    GL_TEXTURE_2D = 0x0de1,
    GL_GREATER = 0x0204,
    GL_GEQUAL = 0x0206,
    GL_CULL_FACE = 0x0b44,
    GL_DITHER = 0x0bd0,
    GL_PERSPECTIVE_CORRECTION_HINT = 0x0c50,
    GL_DONT_CARE = 0x1100,
    GL_NICEST = 0x1102,
    GL_SMOOTH = 0x1d01,
    GL_MODULATE = 0x2100,
    GL_TEXTURE_ENV_MODE = 0x2200,
    GL_TEXTURE_ENV = 0x2300,
    GL_NEAREST = 0x2600,
    GL_LINEAR = 0x2601,
    GL_LINEAR_MIPMAP_NEAREST = 0x2701,
    GL_LINEAR_MIPMAP_LINEAR = 0x2703,
    GL_TEXTURE_MAG_FILTER = 0x2800,
    GL_TEXTURE_MIN_FILTER = 0x2801,
    GL_TEXTURE_WRAP_S = 0x2802,
    GL_TEXTURE_WRAP_T = 0x2803,
    GL_CLAMP = 0x2900,
    GL_REPEAT = 0x2901,
    GL_RGBA4 = 0x8056,
    GL_UNSIGNED_BYTE = 0x1401,
    GL_RGBA = 0x1908,
    // The cursor overlay's (platform/cursor.cpp).
    GL_ALL_ATTRIB_BITS = 0xffffffff,
    GL_MODELVIEW = 0x1700,
    GL_PROJECTION = 0x1701,
    GL_QUADS = 0x0007,
    GL_REPLACE = 0x1e01,
    GL_SRC_ALPHA = 0x0302,
    GL_ONE_MINUS_SRC_ALPHA = 0x0303,
    GL_LIGHTING = 0x0b50,
};

extern "C" {
void glPushAttrib(GLbitfield mask);
void glPopAttrib();
void glDisable(GLenum cap);
void glPixelStorei(GLenum pname, GLint param);
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
void glPixelZoom(GLfloat xfactor, GLfloat yfactor);
void glDrawPixels(GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
GLenum glGetError();
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
void glClear(GLbitfield);
void glDepthMask(uint8_t);
void glEnable(GLenum);
void glDepthFunc(GLenum);
void glEnableClientState(GLenum);
void glViewport(GLint, GLint, GLsizei, GLsizei);
void glMatrixMode(GLenum);
void glLoadIdentity();
void glFrontFace(GLenum);
void glShadeModel(GLenum mode);
void glHint(GLenum target, GLenum mode);
void glAlphaFunc(GLenum func, GLfloat ref);
void glBlendFunc(GLenum source, GLenum dest);
void glFlush();
void glGenTextures(GLsizei n, GLuint* textures);
void glDeleteTextures(GLsizei n, const GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexEnvf(GLenum target, GLenum pname, GLfloat param);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const void* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, const void* pixels);
// The draw primitive's drawing members (src/engine/DrawPrimitiveOpenGL.cpp): vertex arrays, immediate
// mode and pixel reads. glLockArraysEXT and glUnlockArraysEXT are GL_EXT_compiled_vertex_array.
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
void glLockArraysEXT(GLint first, GLsizei count);
void glUnlockArraysEXT();
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void glFinish();
void glDrawBuffer(GLenum mode);
void glReadBuffer(GLenum mode);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
void glBegin(GLenum mode);
void glEnd();
void glColor4ubv(const uint8_t* components);
void glTexCoord2f(GLfloat s, GLfloat t);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
// The cursor overlay's (platform/cursor.cpp): matrices, an orthographic projection and a quad.
void glPushMatrix();
void glPopMatrix();
void glOrtho(double left, double right, double bottom, double top, double near, double far);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glVertex2f(GLfloat x, GLfloat y);
void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
// GLU, which the engine imports for mipmapped pages.
GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, const void* data);
}  // extern "C"
