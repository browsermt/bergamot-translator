#include "byte_array_util.h"
#include <stdlib.h>
#include <iostream>

namespace marian {
namespace bergamot {

AlignedMemory loadFileToMemory(const std::string& path, size_t alignment){
  uint64_t fileSize = filesystem::fileSize(path);
  io::InputFileStream in(path);
  ABORT_IF(in.bad(), "Failed opening file stream: {}", path);
  AlignedMemory alignedMemory(fileSize, alignment);
  in.read(reinterpret_cast<char *>(alignedMemory.begin()), fileSize);
  ABORT_IF(alignedMemory.size() != fileSize, "Error reading file {}", path);
  return alignedMemory;
}

AlignedMemory getModelMemoryFromConfig(marian::Ptr<marian::Options> options){
    auto models = options->get<std::vector<std::string>>("models");
    ABORT_IF(models.size() != 1, "Loading multiple binary models is not supported for now as it is not necessary.");
    marian::filesystem::Path modelPath(models[0]);
    ABORT_IF(modelPath.extension() != marian::filesystem::Path(".bin"), "The file of binary model should end with .bin");
    return loadFileToMemory(models[0], 256);
}

AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options){
  auto shortlist = options->get<std::vector<std::string>>("shortlist");
  ABORT_IF(shortlist.empty(), "No path to shortlist file is given.");
  return loadFileToMemory(shortlist[0], 64);
}

} // namespace bergamot
} // namespace marian
