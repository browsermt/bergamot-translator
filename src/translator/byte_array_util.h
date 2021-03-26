#include "marian.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

MemoryGift loadFileToMemory(const std::string& filename, bool isModelFile);
MemoryGift getBinaryModelFromConfig(marian::Ptr<marian::Options> options);
MemoryGift getBinaryShortlistFromConfig(marian::Ptr<marian::Options> options);


} // namespace bergamot
} // namespace marian
