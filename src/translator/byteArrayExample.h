#include "marian.h"

namespace bergamot {

void * getBinaryFile(std::string path);
void * getBinaryModelFromConfig(marian::Ptr<marian::Options> options);
std::vector<char> getBinaryShortlistFromFile(const std::string& filename);
std::vector<char> getBinaryShortlistFromConfig(marian::Ptr<marian::Options> options);
} // namespace bergamot
