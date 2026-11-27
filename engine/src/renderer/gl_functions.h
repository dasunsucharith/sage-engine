#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <GL/gl.h>
#include <cstddef>
#include <cstdint>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif

// OpenGL 3.3 Core types & constants missing from Windows' legacy GL/gl.h (OpenGL 1.1)
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace sage::gl {

using PFNGLCREATESHADERPROC = GLuint(APIENTRYP)(GLenum type);
using PFNGLSHADERSOURCEPROC = void(APIENTRYP)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFNGLCOMPILESHADERPROC = void(APIENTRYP)(GLuint shader);
using PFNGLGETSHADERIVPROC = void(APIENTRYP)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void(APIENTRYP)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLDELETESHADERPROC = void(APIENTRYP)(GLuint shader);

using PFNGLCREATEPROGRAMPROC = GLuint(APIENTRYP)(void);
using PFNGLATTACHSHADERPROC = void(APIENTRYP)(GLuint program, GLuint shader);
using PFNGLLINKPROGRAMPROC = void(APIENTRYP)(GLuint program);
using PFNGLGETPROGRAMIVPROC = void(APIENTRYP)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETPROGRAMINFOLOGPROC = void(APIENTRYP)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLUSEPROGRAMPROC = void(APIENTRYP)(GLuint program);
using PFNGLDELETEPROGRAMPROC = void(APIENTRYP)(GLuint program);

using PFNGLGETUNIFORMLOCATIONPROC = GLint(APIENTRYP)(GLuint program, const GLchar* name);
using PFNGLUNIFORM1IPROC = void(APIENTRYP)(GLint location, GLint v0);
using PFNGLUNIFORM1FPROC = void(APIENTRYP)(GLint location, GLfloat v0);
using PFNGLUNIFORM2FPROC = void(APIENTRYP)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM3FPROC = void(APIENTRYP)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORMMATRIX4FVPROC = void(APIENTRYP)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

using PFNGLGENVERTEXARRAYSPROC = void(APIENTRYP)(GLsizei n, GLuint* arrays);
using PFNGLBINDVERTEXARRAYPROC = void(APIENTRYP)(GLuint array);
using PFNGLDELETEVERTEXARRAYSPROC = void(APIENTRYP)(GLsizei n, const GLuint* arrays);

using PFNGLGENBUFFERSPROC = void(APIENTRYP)(GLsizei n, GLuint* buffers);
using PFNGLBINDBUFFERPROC = void(APIENTRYP)(GLenum target, GLuint buffer);
using PFNGLBUFFERDATAPROC = void(APIENTRYP)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using PFNGLDELETEBUFFERSPROC = void(APIENTRYP)(GLsizei n, const GLuint* buffers);

using PFNGLVERTEXATTRIBPOINTERPROC = void(APIENTRYP)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(APIENTRYP)(GLuint index);

using PFNGLACTIVETEXTUREPROC = void(APIENTRYP)(GLenum texture);
using PFNGLGENERATEMIPMAPPROC = void(APIENTRYP)(GLenum target);

extern PFNGLCREATESHADERPROC createShader;
extern PFNGLSHADERSOURCEPROC shaderSource;
extern PFNGLCOMPILESHADERPROC compileShader;
extern PFNGLGETSHADERIVPROC getShaderiv;
extern PFNGLGETSHADERINFOLOGPROC getShaderInfoLog;
extern PFNGLDELETESHADERPROC deleteShader;

extern PFNGLCREATEPROGRAMPROC createProgram;
extern PFNGLATTACHSHADERPROC attachShader;
extern PFNGLLINKPROGRAMPROC linkProgram;
extern PFNGLGETPROGRAMIVPROC getProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog;
extern PFNGLUSEPROGRAMPROC useProgram;
extern PFNGLDELETEPROGRAMPROC deleteProgram;

extern PFNGLGETUNIFORMLOCATIONPROC getUniformLocation;
extern PFNGLUNIFORM1IPROC uniform1i;
extern PFNGLUNIFORM1FPROC uniform1f;
extern PFNGLUNIFORM2FPROC uniform2f;
extern PFNGLUNIFORM3FPROC uniform3f;
extern PFNGLUNIFORMMATRIX4FVPROC uniformMatrix4fv;

extern PFNGLGENVERTEXARRAYSPROC genVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC bindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays;

extern PFNGLGENBUFFERSPROC genBuffers;
extern PFNGLBINDBUFFERPROC bindBuffer;
extern PFNGLBUFFERDATAPROC bufferData;
extern PFNGLDELETEBUFFERSPROC deleteBuffers;

extern PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray;

extern PFNGLACTIVETEXTUREPROC activeTexture;
extern PFNGLGENERATEMIPMAPPROC generateMipmap;

// Loads all required OpenGL 3.3 Core functions from the current GL context.
bool loadFunctions();

} // namespace sage::gl
