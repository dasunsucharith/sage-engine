#include "renderer/gl_shaders.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace sage {

namespace fs = std::filesystem;

bool ShaderLibrary::findShaderDirectory(const fs::path& projectRoot) {
    const fs::path candidates[] = {
        projectRoot / "engine" / "shaders",
        fs::path("engine") / "shaders",
        fs::path("..") / "engine" / "shaders",
    };
    std::error_code ec;
    for (const fs::path& c : candidates) {
        if (!c.empty() && fs::exists(c / "lit.frag", ec)) {
            m_dir = fs::weakly_canonical(c, ec);
            std::cout << "[SAGE][Shader] Loading shaders from " << m_dir.string() << " (hot reload on save)\n";
            return true;
        }
    }
    std::cerr << "[SAGE][Shader] Shader folder not found. Looked for lit.frag in:\n";
    for (const fs::path& c : candidates) {
        std::cerr << "    " << fs::absolute(c, ec).string() << "\n";
    }
    std::cerr << "  Run sage.exe from the project folder (the one containing engine/shaders).\n";
    return false;
}

bool ShaderLibrary::readSource(const std::string& file, std::string& out) {
    const fs::path path = m_dir / file;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "[SAGE][Shader] Cannot open " << path.string() << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    std::error_code ec;
    m_fileTimes[file] = fs::last_write_time(path, ec);
    return true;
}

GLuint ShaderLibrary::compile(GLenum type, const std::string& source, const std::string& file) {
    const GLuint shader = gl::createShader(type);
    const char* src = source.c_str();
    gl::shaderSource(shader, 1, &src, nullptr);
    gl::compileShader(shader);

    GLint success = 0;
    gl::getShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        gl::getShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<std::size_t>(logLength > 1 ? logLength : 1));
        gl::getShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "[SAGE][Shader] Compile error in " << (m_dir / file).string()
                  << " (line numbers refer to this file):\n" << infoLog.data() << "\n";
        gl::deleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ShaderLibrary::buildProgram(const char* label, const char* vertFile, const char* fragFile) {
    std::string vertSrc;
    std::string fragSrc;
    if (!readSource(vertFile, vertSrc) || !readSource(fragFile, fragSrc)) {
        return 0;
    }
    const GLuint vs = compile(GL_VERTEX_SHADER, vertSrc, vertFile);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragSrc, fragFile);
    if (!vs || !fs) {
        if (vs) gl::deleteShader(vs);
        if (fs) gl::deleteShader(fs);
        return 0;
    }

    const GLuint program = gl::createProgram();
    gl::attachShader(program, vs);
    gl::attachShader(program, fs);
    gl::linkProgram(program);
    gl::deleteShader(vs);
    gl::deleteShader(fs);

    GLint linked = 0;
    gl::getProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        gl::getProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(static_cast<std::size_t>(logLength > 1 ? logLength : 1));
        gl::getProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "[SAGE][Shader] Link error in program " << label << " (" << vertFile << " + " << fragFile
                  << "):\n" << infoLog.data() << "\n";
        gl::deleteProgram(program);
        return 0;
    }
    return program;
}

bool ShaderLibrary::pollForChanges(std::string& changedFile) {
    bool changed = false;
    std::error_code ec;
    for (auto& [file, time] : m_fileTimes) {
        const auto now = fs::last_write_time(m_dir / file, ec);
        if (!ec && now != time) {
            if (!changed) changedFile = file;
            time = now; // Re-arm: a failed rebuild won't retrigger until the next save
            changed = true;
        }
    }
    return changed;
}

} // namespace sage
