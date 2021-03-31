#include "byte_array_util.h"
#include <stdlib.h>
#include <iostream>

namespace marian {
namespace bergamot {

void loadFileToMemory(const std::string& path, AlignedMemory& alignedMemory){
  uint64_t fileSize = filesystem::fileSize(path);
  io::InputFileStream in(path);
  ABORT_IF(in.bad(), "Failed opening file stream: {}", path);
  in.read(reinterpret_cast<char *>(alignedMemory.begin()), fileSize);
  ABORT_IF(alignedMemory.size() < fileSize, "Error reading file {}", path);  // Model memory size could be bigger than file size
}

std::string getModelFileFromConfig(marian::Ptr<marian::Options> options) {
    auto models = options->get<std::vector<std::string>>("models");
    ABORT_IF(models.size() != 1, "Loading multiple binary models is not supported for now as it is not necessary.");
    marian::filesystem::Path modelPath(models[0]);
    ABORT_IF(modelPath.extension() != marian::filesystem::Path(".bin"), "The file of binary model should end with .bin");
    return models[0];
}
std::string getShortlistFileFromConfig(marian::Ptr<marian::Options> options){
  auto shortlist = options->get<std::vector<std::string>>("shortlist");
  ABORT_IF(shortlist.empty(), "No path to shortlist file is given.");
  return shortlist[0];
}

size_t getMemorySizeFromFile(const std::string& filename, bool isModelFile){
  uint64_t fileSize = filesystem::fileSize(filename);
  if (isModelFile){  // As model memory need to be 256-byte aligned (AlignedMemory only promises 64-byte aligned memory)
    fileSize = fileSize%256==0? fileSize : (fileSize/256+1)*256;
  }
  return fileSize;
}

} // namespace bergamot
} // namespace marian
