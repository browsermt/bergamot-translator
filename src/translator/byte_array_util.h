#include "marian.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

AlignedMemory loadFileToMemory(const std::string& path, size_t alignment);
AlignedMemory getModelMemoryFromConfig(marian::Ptr<marian::Options> options);
AlignedMemory getShortlistMemoryFromConfig(marian::Ptr<marian::Options> options);
void getVocabsMemoryFromConfig(marian::Ptr<marian::Options> options,
                               std::vector<AlignedMemory>& vocabsMemory,
                               std::vector<string_view>& vocabsMemorySV);

} // namespace bergamot
} // namespace marian
