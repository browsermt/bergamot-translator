#include "byte_array_util.h"
#include "common/io.h"

#include <cstdlib>
#include <memory>

#include "data/shortlist.h"

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
}  // Anonymous namespace

bool validateBinaryModel(const AlignedMemory& model, uint64_t fileSize) {
  const void* current = model.begin();
  uint64_t memoryNeeded =
      sizeof(uint64_t) * 2;  // We keep track of how much memory we would need if we have a complete file
  uint64_t numHeaders;
  if (fileSize >= memoryNeeded) {  // We have enough filesize to fetch the headers.
    uint64_t binaryFileVersion = *get<uint64_t>(current);
    numHeaders = *get<uint64_t>(current);  // number of item headers that follow
  } else {
    return false;
  }
  memoryNeeded += numHeaders * sizeof(Header);
  const Header* headers;
  if (fileSize >= memoryNeeded) {
    headers = get<Header>(current, numHeaders);  // read that many headers
  } else {
    return false;
  }

  // Calculate how many bytes we are going to for reading just the names and the shape
  for (uint64_t i = 0; i < numHeaders; i++) {
    memoryNeeded += headers[i].nameLength + headers[i].shapeLength * sizeof(int);
    // Advance the pointers.
    get<char>(current, headers[i].nameLength);
    get<int>(current, headers[i].shapeLength);
  }

  // Before we start reading the data, there is a small padding to ensure alignment
  // Read that in, before calculating the actual tensor memory requirements.
  uint64_t aligned_offset;
  if (fileSize >= memoryNeeded) {
    aligned_offset = *get<uint64_t>(current);  // Offset to align memory to 256 size
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

AlignedMemory loadFileToMemory(const std::string& path, size_t alignment) {
  uint64_t fileSize = filesystem::fileSize(path);
  io::InputFileStream in(path);
  ABORT_IF(in.bad(), "Failed opening file stream: {}", path);
  AlignedMemory alignedMemory(fileSize, alignment);
  in.read(reinterpret_cast<char*>(alignedMemory.begin()), fileSize);
  ABORT_IF(alignedMemory.size() != fileSize, "Error reading file {}", path);
  return alignedMemory;
}

AlignedMemory getModelMemoryFromConfig(marian::Ptr<marian::Options> options) {
  auto models = options->get<std::vector<std::string>>("models");
  ABORT_IF(models.size() != 1, "Loading multiple binary models is not supported for now as it is not necessary.");
  if (marian::io::isBin(models[0])) {
    AlignedMemory alignedMemory = loadFileToMemory(models[0], 256);
    return alignedMemory;
  } else if (marian::io::isNpz(models[0])) {
    return AlignedMemory();
  } else {
    ABORT("Unknown extension for model: {}", models[0]);
  }
  return AlignedMemory();
}

AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options) {
  auto shortlist = options->get<std::vector<std::string>>("shortlist");
  if (!shortlist.empty()) {
    ABORT_IF(!marian::data::isBinaryShortlist(shortlist[0]),
             "Loading non-binary shortlist file into memory is not supported");
    return loadFileToMemory(shortlist[0], 64);
  }
  return AlignedMemory();
}

void getVocabsMemoryFromConfig(marian::Ptr<marian::Options> options,
                               std::vector<std::shared_ptr<AlignedMemory>>& vocabMemories) {
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  vocabMemories.resize(vfiles.size());
  std::unordered_map<std::string, std::shared_ptr<AlignedMemory>> vocabMap;
  for (size_t i = 0; i < vfiles.size(); ++i) {
    ABORT_IF(marian::filesystem::Path(vfiles[i]).extension() != marian::filesystem::Path(".spm"),
             "Loading non-SentencePiece vocab files into memory is not supported");
    auto m = vocabMap.emplace(std::make_pair(vfiles[i], std::shared_ptr<AlignedMemory>()));
    if (m.second) {
      m.first->second = std::make_shared<AlignedMemory>(loadFileToMemory(vfiles[i], 64));
    }
    vocabMemories[i] = m.first->second;
  }
}

AlignedMemory getQualityEstimatorModel(const marian::Ptr<marian::Options>& options) {
  const auto qualityEstimatorPath = options->get<std::string>("quality", "");
  if (qualityEstimatorPath.empty()) {
    return {};
  }
  return loadFileToMemory(qualityEstimatorPath, 64);
}

AlignedMemory getQualityEstimatorModel(MemoryBundle& memoryBundle, const marian::Ptr<marian::Options>& options) {
  if (memoryBundle.qualityEstimatorMemory.size() == 0) {
    return getQualityEstimatorModel(options);
  }

  return std::move(memoryBundle.qualityEstimatorMemory);
}

MemoryBundle getMemoryBundleFromConfig(marian::Ptr<marian::Options> options) {
  MemoryBundle memoryBundle;
  memoryBundle.model = getModelMemoryFromConfig(options);
  memoryBundle.shortlist = getShortlistMemoryFromConfig(options);
  getVocabsMemoryFromConfig(options, memoryBundle.vocabs);
  memoryBundle.ssplitPrefixFile = getSsplitPrefixFileMemoryFromConfig(options);
  memoryBundle.qualityEstimatorMemory = getQualityEstimatorModel(options);

  return memoryBundle;
}

AlignedMemory getSsplitPrefixFileMemoryFromConfig(marian::Ptr<marian::Options> options) {
  std::string fpath = options->get<std::string>("ssplit-prefix-file", "");
  if (!fpath.empty()) {
    return loadFileToMemory(fpath, 64);
  }
  // Return empty AlignedMemory
  return AlignedMemory();
}

}  // namespace bergamot
}  // namespace marian
