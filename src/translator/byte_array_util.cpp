#include "byte_array_util.h"
#include <stdlib.h>
#include <fstream>
#include <iostream>

namespace marian {
namespace bergamot {

MemoryGift loadFileToMemory(const std::string& path, bool isModelFile){
  std::ifstream is (path, std::ifstream::binary);
  ABORT_IF(is.bad(), "Failed opening file stream: {}", path);
  uint64_t length = 0; // Determine the length of file in bytes
  is.seekg(0, is.end);
  length = is.tellg();
  is.seekg(0, is.beg);

  void *result = new char[length];  // To avoid the memory is destroyed after the function call
  if (isModelFile) {
    int fail = posix_memalign(&result, 256, length);
    ABORT_IF(fail, "Failed to allocate aligned memory.");
  }
  is.read(static_cast<char *>(result), length);
  return MemoryGift(result, length);
}

MemoryGift getBinaryModelFromConfig(marian::Ptr<marian::Options> options) {
    std::vector<std::string> models = options->get<std::vector<std::string>>("models");
    ABORT_IF(models.size() != 1, "Loading multiple binary models is not supported for now as it is not necessary.");
    marian::filesystem::Path modelPath(models[0]);
    ABORT_IF(modelPath.extension() != marian::filesystem::Path(".bin"), "Non binary models cannot be loaded as a byte array.");
    return loadFileToMemory(models[0], true);
}

MemoryGift getBinaryShortlistFromConfig(marian::Ptr<marian::Options> options){
  std::vector<std::string> shortlist = options->get<std::vector<std::string>>("shortlist");
  ABORT_IF(shortlist.empty(), "No path to shortlist file is given.");
  return loadFileToMemory(shortlist[0], false);
}

} // namespace bergamot
} // namespace marian
