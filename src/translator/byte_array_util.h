#include "marian.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

AlignedMemory loadFileToMemory(const std::string& path, size_t alignment);
AlignedMemory getModelMemoryFromConfig(marian::Ptr<marian::Options> options);
AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options);
void getVocabsMemoryFromConfig(marian::Ptr<marian::Options> options,
                               std::vector<marian::Ptr<AlignedMemory>>& vocabMemories);
bool validateBinaryModel(const AlignedMemory& model, uint64_t fileSize);
} // namespace bergamot
} // namespace marian
