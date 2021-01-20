#ifndef __BERGAMOT_UTILS_H
#define __BERGAMOT_UTILS_H

#include "common/options.h"
#include "common/types.h"  
#include "data/vocab.h"
#include "translator/history.h"

#include <string>
#include <vector>

namespace marian {
namespace bergamot {

std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options);

} // namespace bergamot
} // namespace marian

#endif // __BERGAMOT_UTILS_H
