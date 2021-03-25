#include "byte_array_util.h"
#include <stdlib.h>
#include <fstream>
#include <iostream>

namespace marian {
namespace bergamot {

MemoryGift loadFileToMemory(const std::string& path, bool isModelFile){
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
    if (isModelFile) {
        int fail = posix_memalign(&result, 256, length);
        if (fail) {
            std::cerr << "Failed to allocate aligned memory." << std::endl;
            std::exit(1);
        }
    }
    is.read(static_cast<char *>(result), length);
    return MemoryGift(result,length);
}

MemoryGift getBinaryModelFromConfig(marian::Ptr<marian::Options> options) {
    std::vector<std::string> models = options->get<std::vector<std::string>>("models");
    if (models.size() != 1) {
        std::cerr << "Loading multiple binary models is not supported for now as it is not necessary." << std::endl;
        std::exit(1);
    } else {
        marian::filesystem::Path modelPath(models[0]);
        if (modelPath.extension() != marian::filesystem::Path(".bin")) {
            std::cerr << "Non binary models cannot be loaded as a byte array." << std::endl;
            std::exit(1);
        }
        return loadFileToMemory(models[0], true);
    }
}

MemoryGift getBinaryShortlistFromConfig(marian::Ptr<marian::Options> options){
    std::vector<std::string> vals = options->get<std::vector<std::string>>("shortlist");
    if (vals.empty()){
        std::cerr << "No path to shortlist file given" << std::endl;
        std::exit(1);
    }
    std::string filename = vals[0];
    return loadFileToMemory(filename,false);
}

} // namespace bergamot
} // namespace marian