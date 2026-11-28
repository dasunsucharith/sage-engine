#pragma once

// ShaderLibrary: loads GLSL sources from engine/shaders/*.vert|*.frag, compiles & links
// them with file-labelled error messages, and tracks file timestamps for hot reload.
//
// The source file is passed to OpenGL verbatim, so the line numbers in driver error
// messages (e.g. NVIDIA's "0(123) : error ...") are line numbers in that file.

#include "renderer/gl_functions.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace sage {

class ShaderLibrary {
public:
    // Finds the shader folder (<project>/engine/shaders, or ./engine/shaders). Logs the
    // candidates and returns false if it isn't found.
    bool findShaderDirectory(const std::filesystem::path& projectRoot);
    const std::filesystem::path& directory() const { return m_dir; }

    // Compiles + links one program from two files in the shader folder. Returns 0 (after
    // logging the file and the driver's error log) on failure.
    GLuint buildProgram(const char* label, const char* vertFile, const char* fragFile);

    // True if any shader file read so far changed on disk since it was last read.
    // `changedFile` names the first changed file. Re-arms itself (no repeated triggers).
    bool pollForChanges(std::string& changedFile);

private:
    bool readSource(const std::string& file, std::string& out);
    GLuint compile(GLenum type, const std::string& source, const std::string& file);

    std::filesystem::path m_dir;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimes;
};

} // namespace sage
