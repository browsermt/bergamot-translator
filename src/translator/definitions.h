#ifndef SRC_BERGAMOT_DEFINITIONS_H_
#define SRC_BERGAMOT_DEFINITIONS_H_

#include "data/types.h"
#include "data/vocab_base.h"
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

class MemoryGift{
private:
  const void *memory_;
  const size_t memorySize_;
public:
  MemoryGift(const void *memory,const size_t memorySize)
  :memory_(memory),memorySize_(memorySize){}
  const void* data() const{
    return memory_;
  }
  const size_t length() const{
    return memorySize_;
  }
  void early_free(){
    free((char *)memory_);
  }
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_DEFINITIONS_H_
