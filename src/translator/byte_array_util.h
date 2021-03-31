#include "marian.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

void loadFileToMemory(const std::string& filename, AlignedMemory& alignedMemory);
std::string getModelFileFromConfig(marian::Ptr<marian::Options> options);
std::string getShortlistFileFromConfig(marian::Ptr<marian::Options> options);
size_t getMemorySizeFromFile(const std::string& filename, bool isModelFile);
} // namespace bergamot
} // namespace marian
