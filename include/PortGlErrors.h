// PortGlErrors.h: OpenGL errors left by earlier calls taken before a texture is bound, a switch for tpw-port #125
// (COpenGLTexture::Select). Select binds the texture and then reads glGetError, which answers the oldest error of any
// call since the last read, not only the bind's: one left by an earlier call turns texturing off (glDisable), and the
// draws after it, with that texture and the next ones until an untextured draw (COpenGLDrawPrimitive::SetTexture), come
// out in their bare vertex colours, on the park's ground the light's, near white. A driver that answers an error where the Mac's
// answers none would then draw bright patches that come and go, which fits tpw-port #125 (NVIDIA on Windows 10), but no
// Mac shows it, so the switch is off unless the player sets it (launcher.ini's gl_error_drain=1 or TPW_GL_ERROR_DRAIN=1,
// read once at the start); on, the pending errors are taken first and the first kLogged of them written to the launcher
// log, so the log tells whether a driver answers any; off, the Mac's. Header only and pure, so the host test
// (tests/host_gl_errors.cpp) sees what the game runs.
#pragma once

