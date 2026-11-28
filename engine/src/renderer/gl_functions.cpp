#include "renderer/gl_functions.h"

#include "sage/platform/window.h"

#include <iostream>
#include <type_traits>

namespace sage::gl {

PFNGLCREATESHADERPROC createShader = nullptr;
PFNGLSHADERSOURCEPROC shaderSource = nullptr;
PFNGLCOMPILESHADERPROC compileShader = nullptr;
PFNGLGETSHADERIVPROC getShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC getShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC deleteShader = nullptr;

PFNGLCREATEPROGRAMPROC createProgram = nullptr;
PFNGLATTACHSHADERPROC attachShader = nullptr;
PFNGLLINKPROGRAMPROC linkProgram = nullptr;
PFNGLGETPROGRAMIVPROC getProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC getProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC useProgram = nullptr;
PFNGLDELETEPROGRAMPROC deleteProgram = nullptr;

PFNGLGETUNIFORMLOCATIONPROC getUniformLocation = nullptr;
PFNGLUNIFORM1IPROC uniform1i = nullptr;
PFNGLUNIFORM1FPROC uniform1f = nullptr;
PFNGLUNIFORM2FPROC uniform2f = nullptr;
PFNGLUNIFORM3FPROC uniform3f = nullptr;
PFNGLUNIFORM4FVPROC uniform4fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC uniformMatrix4fv = nullptr;

PFNGLGENVERTEXARRAYSPROC genVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC bindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays = nullptr;

PFNGLGENBUFFERSPROC genBuffers = nullptr;
PFNGLBINDBUFFERPROC bindBuffer = nullptr;
PFNGLBUFFERDATAPROC bufferData = nullptr;
PFNGLDELETEBUFFERSPROC deleteBuffers = nullptr;

PFNGLVERTEXATTRIBPOINTERPROC vertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC enableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBDIVISORPROC vertexAttribDivisor = nullptr;
PFNGLDRAWARRAYSINSTANCEDPROC drawArraysInstanced = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC drawElementsInstanced = nullptr;

PFNGLACTIVETEXTUREPROC activeTexture = nullptr;
PFNGLGENERATEMIPMAPPROC generateMipmap = nullptr;

PFNGLGENFRAMEBUFFERSPROC genFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC bindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers = nullptr;

PFNGLGENRENDERBUFFERSPROC genRenderbuffers = nullptr;
PFNGLBINDRENDERBUFFERPROC bindRenderbuffer = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC renderbufferStorage = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer = nullptr;
PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers = nullptr;

PFNGLGENQUERIESPROC genQueries = nullptr;
PFNGLDELETEQUERIESPROC deleteQueries = nullptr;
PFNGLBEGINQUERYPROC beginQuery = nullptr;
PFNGLENDQUERYPROC endQuery = nullptr;
PFNGLGETQUERYOBJECTIVPROC getQueryObjectiv = nullptr;
PFNGLGETQUERYOBJECTUI64VPROC getQueryObjectui64v = nullptr;

bool hasTimerQueries() {
    return genQueries && deleteQueries && beginQuery && endQuery && getQueryObjectiv && getQueryObjectui64v;
}

namespace {

template <typename FnPtr>
bool loadOne(FnPtr& target, const char* symbolName) {
    void* rawPtr = Window::getGLProcAddress(symbolName);
    if (!rawPtr) {
        std::cerr << "[SAGE][OpenGL] Failed to load required OpenGL 3.3 function: "
                  << symbolName << "\n";
        return false;
    }
    target = reinterpret_cast<FnPtr>(rawPtr);
    return true;
}

} // namespace

bool loadFunctions() {
    bool ok = true;

    ok &= loadOne(createShader, "glCreateShader");
    ok &= loadOne(shaderSource, "glShaderSource");
    ok &= loadOne(compileShader, "glCompileShader");
    ok &= loadOne(getShaderiv, "glGetShaderiv");
    ok &= loadOne(getShaderInfoLog, "glGetShaderInfoLog");
    ok &= loadOne(deleteShader, "glDeleteShader");

    ok &= loadOne(createProgram, "glCreateProgram");
    ok &= loadOne(attachShader, "glAttachShader");
    ok &= loadOne(linkProgram, "glLinkProgram");
    ok &= loadOne(getProgramiv, "glGetProgramiv");
    ok &= loadOne(getProgramInfoLog, "glGetProgramInfoLog");
    ok &= loadOne(useProgram, "glUseProgram");
    ok &= loadOne(deleteProgram, "glDeleteProgram");

    ok &= loadOne(getUniformLocation, "glGetUniformLocation");
    ok &= loadOne(uniform1i, "glUniform1i");
    ok &= loadOne(uniform1f, "glUniform1f");
    ok &= loadOne(uniform2f, "glUniform2f");
    ok &= loadOne(uniform3f, "glUniform3f");
    ok &= loadOne(uniform4fv, "glUniform4fv");
    ok &= loadOne(uniformMatrix4fv, "glUniformMatrix4fv");

    ok &= loadOne(genVertexArrays, "glGenVertexArrays");
    ok &= loadOne(bindVertexArray, "glBindVertexArray");
    ok &= loadOne(deleteVertexArrays, "glDeleteVertexArrays");

    ok &= loadOne(genBuffers, "glGenBuffers");
    ok &= loadOne(bindBuffer, "glBindBuffer");
    ok &= loadOne(bufferData, "glBufferData");
    ok &= loadOne(deleteBuffers, "glDeleteBuffers");

    ok &= loadOne(vertexAttribPointer, "glVertexAttribPointer");
    ok &= loadOne(enableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= loadOne(vertexAttribDivisor, "glVertexAttribDivisor");
    ok &= loadOne(drawArraysInstanced, "glDrawArraysInstanced");
    ok &= loadOne(drawElementsInstanced, "glDrawElementsInstanced");

    ok &= loadOne(activeTexture, "glActiveTexture");
    ok &= loadOne(generateMipmap, "glGenerateMipmap");

    ok &= loadOne(genFramebuffers, "glGenFramebuffers");
    ok &= loadOne(bindFramebuffer, "glBindFramebuffer");
    ok &= loadOne(framebufferTexture2D, "glFramebufferTexture2D");
    ok &= loadOne(checkFramebufferStatus, "glCheckFramebufferStatus");
    ok &= loadOne(deleteFramebuffers, "glDeleteFramebuffers");

    ok &= loadOne(genRenderbuffers, "glGenRenderbuffers");
    ok &= loadOne(bindRenderbuffer, "glBindRenderbuffer");
    ok &= loadOne(renderbufferStorage, "glRenderbufferStorage");
    ok &= loadOne(framebufferRenderbuffer, "glFramebufferRenderbuffer");
    ok &= loadOne(deleteRenderbuffers, "glDeleteRenderbuffers");

    // Optional (profiling only): a missing timer query never prevents the engine from starting.
    const auto loadOptional = [](auto& target, const char* symbolName) {
        target = reinterpret_cast<std::remove_reference_t<decltype(target)>>(Window::getGLProcAddress(symbolName));
    };
    loadOptional(genQueries, "glGenQueries");
    loadOptional(deleteQueries, "glDeleteQueries");
    loadOptional(beginQuery, "glBeginQuery");
    loadOptional(endQuery, "glEndQuery");
    loadOptional(getQueryObjectiv, "glGetQueryObjectiv");
    loadOptional(getQueryObjectui64v, "glGetQueryObjectui64v");
    if (!hasTimerQueries()) {
        std::cout << "[SAGE][OpenGL] GPU timer queries unavailable: per-pass GPU timings disabled.\n";
    }

    return ok;
}

} // namespace sage::gl
