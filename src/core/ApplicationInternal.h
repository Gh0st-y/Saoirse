#pragma once

// Shared internal helpers used by multiple Application_*.cpp translation units.
// Not part of the public Application interface - do not include from outside
// the Application_*.cpp split files.

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

// Reads a binary file (e.g. compiled SPIR-V shader) into a byte buffer.
// Used by: Application_Pipeline.cpp, Application_ShadowsCascade.cpp, Application_ShadowsPoint.cpp
inline std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
