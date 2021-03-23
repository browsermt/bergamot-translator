#include "marian.h"

namespace bergamot {

void * getBinaryFile(std::string path);
void * getBinaryModelFromConfig(marian::Ptr<marian::Options> options);

} // namespace bergamot
