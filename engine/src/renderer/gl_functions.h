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
using GLuint64Sage = std::uint64_t; // GLuint64 (not declared by the Windows gl.h)

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
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
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif
#ifndef GL_TEXTURE4
#define GL_TEXTURE4 0x84C4
#endif
#ifndef GL_TEXTURE5
#define GL_TEXTURE5 0x84C5
#endif
#ifndef GL_TEXTURE6
#define GL_TEXTURE6 0x84C6
#endif
#ifndef GL_TEXTURE7
#define GL_TEXTURE7 0x84C7
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER 0x8D41
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif
#ifndef GL_TEXTURE_COMPARE_MODE
#define GL_TEXTURE_COMPARE_MODE 0x884C
#endif
#ifndef GL_TEXTURE_COMPARE_FUNC
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#endif
#ifndef GL_COMPARE_REF_TO_TEXTURE
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT 0x8D00
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
// GPU timer queries (core since OpenGL 3.3 / ARB_timer_query)
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

namespace sage::gl {

using PFNGLCREATESHADERPROC = GLuint(APIENTRYP)(GLenum type);
using PFNGLGENQUERIESPROC = void(APIENTRYP)(GLsizei n, GLuint* ids);
using PFNGLDELETEQUERIESPROC = void(APIENTRYP)(GLsizei n, const GLuint* ids);
using PFNGLBEGINQUERYPROC = void(APIENTRYP)(GLenum target, GLuint id);
using PFNGLENDQUERYPROC = void(APIENTRYP)(GLenum target);
using PFNGLGETQUERYOBJECTIVPROC = void(APIENTRYP)(GLuint id, GLenum pname, GLint* params);
using PFNGLGETQUERYOBJECTUI64VPROC = void(APIENTRYP)(GLuint id, GLenum pname, GLuint64Sage* params);
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
using PFNGLUNIFORM4FVPROC = void(APIENTRYP)(GLint location, GLsizei count, const GLfloat* value);
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
using PFNGLVERTEXATTRIBDIVISORPROC = void(APIENTRYP)(GLuint index, GLuint divisor);
using PFNGLDRAWARRAYSINSTANCEDPROC = void(APIENTRYP)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
using PFNGLDRAWELEMENTSINSTANCEDPROC = void(APIENTRYP)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);

using PFNGLACTIVETEXTUREPROC = void(APIENTRYP)(GLenum texture);
using PFNGLGENERATEMIPMAPPROC = void(APIENTRYP)(GLenum target);

using PFNGLGENFRAMEBUFFERSPROC = void(APIENTRYP)(GLsizei n, GLuint* framebuffers);
using PFNGLBINDFRAMEBUFFERPROC = void(APIENTRYP)(GLenum target, GLuint framebuffer);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void(APIENTRYP)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = GLenum(APIENTRYP)(GLenum target);
using PFNGLDELETEFRAMEBUFFERSPROC = void(APIENTRYP)(GLsizei n, const GLuint* framebuffers);

using PFNGLGENRENDERBUFFERSPROC = void(APIENTRYP)(GLsizei n, GLuint* renderbuffers);
using PFNGLBINDRENDERBUFFERPROC = void(APIENTRYP)(GLenum target, GLuint renderbuffer);
using PFNGLRENDERBUFFERSTORAGEPROC = void(APIENTRYP)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
using PFNGLFRAMEBUFFERRENDERBUFFERPROC = void(APIENTRYP)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
using PFNGLDELETERENDERBUFFERSPROC = void(APIENTRYP)(GLsizei n, const GLuint* renderbuffers);

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
extern PFNGLUNIFORM4FVPROC uniform4fv;
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
extern PFNGLVERTEXATTRIBDIVISORPROC vertexAttribDivisor;
extern PFNGLDRAWARRAYSINSTANCEDPROC drawArraysInstanced;
extern PFNGLDRAWELEMENTSINSTANCEDPROC drawElementsInstanced;

extern PFNGLACTIVETEXTUREPROC activeTexture;
extern PFNGLGENERATEMIPMAPPROC generateMipmap;

extern PFNGLGENFRAMEBUFFERSPROC genFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC bindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers;

extern PFNGLGENRENDERBUFFERSPROC genRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC bindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC renderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers;

// Optional: GPU timer queries for per-pass profiling. Null if the driver lacks them;
// call hasTimerQueries() before use.
extern PFNGLGENQUERIESPROC genQueries;
extern PFNGLDELETEQUERIESPROC deleteQueries;
extern PFNGLBEGINQUERYPROC beginQuery;
extern PFNGLENDQUERYPROC endQuery;
extern PFNGLGETQUERYOBJECTIVPROC getQueryObjectiv;
extern PFNGLGETQUERYOBJECTUI64VPROC getQueryObjectui64v;
bool hasTimerQueries();

// Loads all required OpenGL 3.3 Core functions from the current GL context.
bool loadFunctions();

} // namespace sage::gl
