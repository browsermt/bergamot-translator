#include "byte_array_util.h"
#include <stdlib.h>
#include <iostream>

namespace marian {
namespace bergamot {

namespace {

// This is a basic validator that checks if the file has not been truncated
// it basically loads up the header and checks

// This struct and the getter are copied from the marian source, because it's located
// inside src/common/binary.cpp:15 and we can't include it.
struct Header {
  uint64_t nameLength;
  uint64_t type;
  uint64_t shapeLength;
  uint64_t dataLength;
};

// cast current void pointer to T pointer and move forward by num elements
template <typename T>
const T* get(const void*& current, uint64_t num = 1) {
  const T* ptr = (const T*)current;
  current = (const T*)current + num;
  return ptr;
}

bool validateBinaryModel(AlignedMemory& model, uint64_t fileSize) {
  const void * current = &model[0];
  uint64_t memoryNeeded = sizeof(uint64_t)*2; // We keep track of how much memory we would need if we have a complete file
  uint64_t numHeaders;
  if (fileSize >= memoryNeeded) { // We have enough filesize to fetch the headers.
    uint64_t binaryFileVersion = *get<uint64_t>(current);
    numHeaders = *get<uint64_t>(current); // number of item headers that follow
  } else {
    return false;
  }
  memoryNeeded += numHeaders*sizeof(Header);
  const Header* headers;
  if (fileSize >= memoryNeeded) {
    headers = get<Header>(current, numHeaders); // read that many headers
  } else {
    return false;
  }

  // Calculate how many bytes we are going to for reading just the names and the shape
  for (uint64_t i = 0; i < numHeaders; i++) {
    memoryNeeded += headers[i].nameLength + headers[i].shapeLength*sizeof(int);
    // Advance the pointers.
    get<char>(current, headers[i].nameLength);
    get<int>(current, headers[i].shapeLength);
  }

  // Before we start reading the data, there is a small padding to ensure alignment
  // Read that in, before calculating the actual tensor memory requirements.
  uint64_t aligned_offset;
  if (fileSize >= memoryNeeded) {
    aligned_offset = *get<uint64_t>(current); // Offset to align memory to 256 size
    memoryNeeded += aligned_offset + sizeof(uint64_t);
  } else {
    return false;
  }

  // Finally the tensor size:
  for (uint64_t i = 0; i < numHeaders; i++) {
    memoryNeeded += headers[i].dataLength;
  }

  // If this final check passes, the file is at least big enough to contain the model
  if (fileSize >= memoryNeeded) {
    return true;
  } else {
    return false;
  }
}

} // Anonymous namespace

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
    AlignedMemory alignedMemory = loadFileToMemory(models[0], 256);
    ABORT_IF(!validateBinaryModel(alignedMemory, alignedMemory.size()), "The binary file is invalid. Incomplete or corrupted download?");
    return alignedMemory;
}

AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options){
  auto shortlist = options->get<std::vector<std::string>>("shortlist");
  ABORT_IF(shortlist.empty(), "No path to shortlist file is given.");
  return loadFileToMemory(shortlist[0], 64);
}

} // namespace bergamot
} // namespace marian
