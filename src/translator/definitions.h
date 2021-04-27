#ifndef SRC_BERGAMOT_DEFINITIONS_H_
#define SRC_BERGAMOT_DEFINITIONS_H_

#include "data/types.h"
#include "data/vocab_base.h"
#include "aligned.h"
#include <vector>

namespace marian {
namespace bergamot {

typedef marian::Words Segment;
typedef std::vector<Segment> Segments;
typedef std::vector<marian::string_view> TokenRanges;
typedef std::vector<TokenRanges> SentenceTokenRanges;

/** @brief Creates unique_ptr any type, passes all arguments to any available
 *  * constructor */
template <class T, typename... Args> UPtr<T> UNew(Args &&... args) {
  return UPtr<T>(new T(std::forward<Args>(args)...));
}

template <class T> UPtr<T> UNew(UPtr<T> p) { return UPtr<T>(p); }

/// Shortcut to AlignedVector<char> for byte arrays
typedef AlignedVector<char> AlignedMemory;

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_DEFINITIONS_H_
