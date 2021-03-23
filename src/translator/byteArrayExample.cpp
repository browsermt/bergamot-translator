#include "byteArrayExample.h"
#include <stdlib.h>
#include <fstream>
#include <iostream>

namespace bergamot {

void * getBinaryFile(std::string path) {
    std::ifstream is (path, std::ifstream::binary);
    uint64_t length = 0; // Determine the length of file in bytes
    if (is) {
        is.seekg(0, is.end);
        length = is.tellg();
        is.seekg(0, is.beg);
    } else {
        std::cerr << "Failed opening file stream: " << path << std::endl;
        std::exit(1);
    }
    void *result;
    int fail = posix_memalign(&result, 256, length);
    if (fail) {
        std::cerr << "Failed to allocate aligned memory." << std::endl;
        std::exit(1);
    }
    is.read(static_cast<char *>(result), length);
    return result;
}

void * getBinaryModelFromConfig(marian::Ptr<marian::Options> options) {
    std::vector<std::string> models = options->get<std::vector<std::string>>("models");
    if (models.size() != 1) {
        std::cerr << "Loading multiple binary models is not supported for now as it is not necessary." << std::endl;
        std::exit(1);
        marian::filesystem::Path modelPath(models[0]);
        if (modelPath.extension() != marian::filesystem::Path(".bin")) {
            std::cerr << "Non binary models cannot be loaded as a byte array." << std::endl;
            std::exit(1);
        }
        return nullptr;
    } else {
        return getBinaryFile(models[0]);
    }
}

} // namespace bergamot
