#include "marian.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

AlignedMemory loadFileToMemory(const std::string& path, size_t alignment);
AlignedMemory getModelMemoryFromConfig(marian::Ptr<marian::Options> options);
AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options);
void getVocabsMemoryFromConfig(marian::Ptr<marian::Options> options,
                               std::vector<std::shared_ptr<AlignedMemory>>& vocabMemories);
bool validateBinaryModel(const AlignedMemory& model, uint64_t fileSize);
void getMemoryBundleFromConfig(marian::Ptr<marian::Options> options, MemoryBundle& memoryBundle);
} // namespace bergamot
} // namespace marian
